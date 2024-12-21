#ifndef __TSDB2_HTTP_CHANNEL_H__
#define __TSDB2_HTTP_CHANNEL_H__

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "common/reffed_ptr.h"
#include "http/handlers.h"
#include "http/http.h"
#include "http/processor.h"
#include "net/base_sockets.h"
#include "net/epoll_server.h"
#include "net/sockets.h"
#include "net/ssl_sockets.h"

namespace tsdb2 {
namespace http {

// Abstract interface of all channels.
class BaseChannel {
 public:
  explicit BaseChannel() = default;
  virtual ~BaseChannel() = default;

  // `Ref` and `Unref` make channels suitable for `reffed_ptr`.

  virtual void Ref() = 0;
  virtual bool Unref() = 0;

  // Starts a server endpoint by reading the HTTP/2 client preface and starting to exchange frames.
  //
  // Don't call this method explicitly, it's called automatically by the `ChannelManager` for
  // server-side channels.
  virtual void StartServer() = 0;

 private:
  BaseChannel(BaseChannel const&) = delete;
  BaseChannel& operator=(BaseChannel const&) = delete;
  BaseChannel(BaseChannel&&) = delete;
  BaseChannel& operator=(BaseChannel&&) = delete;
};

class ChannelManager {
 public:
  explicit ChannelManager() = default;
  virtual ~ChannelManager() = default;

  virtual void RemoveChannel(BaseChannel* channel) = 0;

  virtual absl::StatusOr<Handler*> GetHandler(std::string_view path) = 0;

 private:
  ChannelManager(ChannelManager const&) = delete;
  ChannelManager& operator=(ChannelManager const&) = delete;
  ChannelManager(ChannelManager&&) = delete;
  ChannelManager& operator=(ChannelManager&&) = delete;
};

namespace internal {

// Internal interface used by `ChannelProcessor` to perform I/O on the channel.
class ChannelInterface {
 public:
  using ContinuationFrameCallback =
      absl::AnyInvocable<void(FrameHeader const&, tsdb2::net::Buffer) &&>;

  explicit ChannelInterface() = default;
  virtual ~ChannelInterface() = default;

  virtual tsdb2::net::BaseSocket* socket() = 0;
  virtual tsdb2::net::BaseSocket const* socket() const = 0;

  virtual absl::StatusOr<Handler*> GetHandler(std::string_view path) = 0;

  // Waits for the next frame, reads it, and passes it on to the processor.
  virtual void Continue() = 0;

  virtual void ReadContinuationFrame(uint32_t stream_id, ContinuationFrameCallback callback) = 0;

  virtual void CloseConnection() = 0;

 private:
  ChannelInterface(ChannelInterface const&) = delete;
  ChannelInterface& operator=(ChannelInterface const&) = delete;
  ChannelInterface(ChannelInterface&&) = delete;
  ChannelInterface& operator=(ChannelInterface&&) = delete;
};

}  // namespace internal

// Manages a single HTTP/2 connection (with multiplexed streams).
template <typename Socket,
          std::enable_if_t<std::is_base_of_v<tsdb2::net::BaseSocket, Socket>, bool> = true>
class Channel final : private Socket, public BaseChannel, private internal::ChannelInterface {
 public:
  // Creates a client-side channel connected to the specified address.
  //
  // Example with SSL sockets:
  //
  //   auto status_or_channel = Channel<tsdb2::net::SSLSocket>::Create(
  //       "www.example.com", 443, SocketOptions(),
  //       [](reffed_ptr<Channel<tsdb2::net::SSLSocket>> channel, absl::Status connect_status) {
  //         if (connect_status.ok()) {
  //           // The channel is now connected.
  //         } else {
  //           // Connection to the provided address/port failed.
  //         }
  //       });
  //   if (!status_or_channel.ok()) {
  //     // An error occurred.
  //   }
  //
  //
  // Example with Unix domain sockets:
  //
  //   auto status_or_channel = Channel<tsdb2::net::Socket>::Create(
  //       kUnixDomainSocketTag, "/tmp/foo.sock",
  //       [](reffed_ptr<Channel<tsdb2::net::Socket>> channel, absl::Status connect_status) {
  //         if (connect_status.ok()) {
  //           // The channel is now connected.
  //         } else {
  //           // Connection to the provided address/port failed.
  //         }
  //       });
  //   if (!status_or_channel.ok()) {
  //     // An error occurred.
  //   }
  //
  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<Channel>> Create(Args&&... args) {
    return Socket::template Create<Channel>(std::forward<Args>(args)...);
  }

  // Creates a channel connected to a socket. Used in test scenarios to check the frames written by
  // the channel.
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<Channel>, tsdb2::common::reffed_ptr<Socket>>>
  CreatePairWithRawPeerForTesting(ChannelManager* const manager) {
    return CreatePairWithRawPeerHelper<Socket>(manager);
  }

  // Creates a pair of channels connected to each other.
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<Channel>, tsdb2::common::reffed_ptr<Channel>>>
  CreatePairForTesting(ChannelManager* const manager) {
    return CreatePairHelper<Socket>(manager);
  }

  using Socket::Close;
  using Socket::is_open;
  using Socket::kIsListener;

  ~Channel() override { Close(); }

  void Ref() override { Socket::Ref(); }
  bool Unref() override { return Socket::Unref(); }

  // Indicates whether this is a client-side channel.
  bool is_client() const { return manager_ == nullptr; }

  // Indicates whether this is a server-side channel.
  bool is_server() const { return manager_ != nullptr; }

  void StartServer() override {
    ReadWithTimeout(kClientPreface.size(), [this](tsdb2::net::Buffer const data) {
      std::string_view const preface{data.as_char_array(), data.size()};
      if (preface != kClientPreface) {
        LOG(ERROR) << "HTTP/2 client preface error: \"" << absl::CEscape(preface) << "\"";
        Close();
      } else {
        processor_.SendSettings();
        Continue();
      }
    });
  }

 private:
  using Buffer = tsdb2::net::Buffer;
  using ReadCallback = absl::AnyInvocable<void(Buffer)>;
  using SkipCallback = absl::AnyInvocable<void()>;

  friend class tsdb2::net::EpollServer;
  friend class tsdb2::net::Socket;
  friend class tsdb2::net::SSLSocket;

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<Channel>> CreateInternal(
      tsdb2::net::EpollServer* const parent, Args&&... args) {
    return Socket::template CreateClass<Channel>(parent, std::forward<Args>(args)...);
  }

  template <typename FirstSocket, typename SecondSocket, typename... Args,
            std::enable_if_t<std::conjunction_v<std::is_base_of<Channel, FirstSocket>,
                                                std::is_base_of<Socket, SecondSocket>>,
                             bool> = true>
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
  CreatePairInternal(tsdb2::net::EpollServer* const parent, Args&&... args) {
    return Socket::template CreateClassPair<FirstSocket, SecondSocket>(parent,
                                                                       std::forward<Args>(args)...);
  }

  template <typename SocketType>
  static absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<SocketType>>,
                                  tsdb2::common::reffed_ptr<SocketType>>>
  CreatePairWithRawPeerHelper(ChannelManager* manager);

  template <>
  absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>,
                           tsdb2::common::reffed_ptr<tsdb2::net::Socket>>>
  CreatePairWithRawPeerHelper<tsdb2::net::Socket>(ChannelManager* const manager) {
    return tsdb2::net::EpollServer::GetInstance()
        ->CreateHeterogeneousSocketPair<tsdb2::net::Socket, Channel<tsdb2::net::Socket>,
                                        tsdb2::net::Socket>(manager);
  }

  template <>
  absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>,
                           tsdb2::common::reffed_ptr<tsdb2::net::SSLSocket>>>
  CreatePairWithRawPeerHelper<tsdb2::net::SSLSocket>(ChannelManager* const manager) {
    return tsdb2::net::SSLSocket::CreateHeterogeneousPairForTesting<Channel<tsdb2::net::SSLSocket>,
                                                                    tsdb2::net::SSLSocket>(manager);
  }

  template <typename SocketType>
  static absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<SocketType>>,
                                  tsdb2::common::reffed_ptr<Channel<SocketType>>>>
  CreatePairHelper(ChannelManager* manager);

  template <>
  absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>,
                           tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>>>
  CreatePairHelper<tsdb2::net::Socket>(ChannelManager* const manager) {
    return tsdb2::net::EpollServer::GetInstance()->CreateSocketPair<Channel<tsdb2::net::Socket>>(
        manager);
  }

  template <>
  absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>,
                           tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>>>
  CreatePairHelper<tsdb2::net::SSLSocket>(ChannelManager* const manager) {
    return tsdb2::net::SSLSocket::CreateHeterogeneousPairForTesting<Channel<tsdb2::net::SSLSocket>,
                                                                    Channel<tsdb2::net::SSLSocket>>(
        manager);
  }

  // Constructs a server-side channel.
  template <typename... Args>
  explicit Channel(tsdb2::net::EpollServer* const parent, ChannelManager* const manager,
                   Args&&... args)
      : Socket(parent, std::forward<Args>(args)...), manager_(manager), processor_{this} {}

  // Constructs a client-side channel.
  template <typename... Args>
  explicit Channel(tsdb2::net::EpollServer* const parent, Args&&... args)
      : Socket(parent, std::forward<Args>(args)...), manager_(nullptr), processor_{this} {}

  Channel(Channel const&) = delete;
  Channel& operator=(Channel const&) = delete;
  Channel(Channel&&) = delete;
  Channel& operator=(Channel&&) = delete;

  tsdb2::net::BaseSocket* socket() override { return this; }
  tsdb2::net::BaseSocket const* socket() const override { return this; }

  absl::StatusOr<Handler*> GetHandler(std::string_view path) override {
    if (manager_ != nullptr) {
      return manager_->GetHandler(path);
    } else {
      return absl::FailedPreconditionError(absl::StrCat(
          "Cannot handle requests for \"", absl::CEscape(path), "\" in a client-side channel."));
    }
  }

  void Read(size_t const length, ReadCallback callback) {
    auto const status = Socket::Read(length, [this, callback = std::move(callback)](
                                                 absl::StatusOr<Buffer> status_or_buffer) mutable {
      if (status_or_buffer.ok()) {
        callback(std::move(status_or_buffer).value());
      } else {
        callback = nullptr;
        Close();
      }
    });
    if (!status.ok()) {
      Close();
    }
  }

  void ReadWithTimeout(size_t const length, ReadCallback callback) {
    auto const status = Socket::ReadWithTimeout(
        length,
        [this, callback = std::move(callback)](absl::StatusOr<Buffer> status_or_buffer) mutable {
          if (status_or_buffer.ok()) {
            callback(std::move(status_or_buffer).value());
          } else {
            callback = nullptr;
            Close();
          }
        },
        absl::GetFlag(FLAGS_http2_io_timeout));
    if (!status.ok()) {
      Close();
    }
  }

  void Skip(size_t const length, SkipCallback callback) {
    auto const status = Socket::SkipWithTimeout(
        length,
        [this, callback = std::move(callback)](absl::Status const status) mutable {
          if (status.ok()) {
            callback();
          } else {
            callback = nullptr;
            Close();
          }
        },
        absl::GetFlag(FLAGS_http2_io_timeout));
    if (!status.ok()) {
      Close();
    }
  }

  void SkipAndContinue(size_t const length) {
    if (length > 0) {
      Skip(length, [this] { Continue(); });
    } else {
      Continue();
    }
  }

  void Continue() override {
    Read(sizeof(FrameHeader), [this](Buffer const buffer) {
      auto const& header = buffer.as<FrameHeader>();
      auto const header_validation_error = processor_.ValidateFrameHeader(header);
      auto const length = header.length();
      if (!header_validation_error.ok()) {
        if (header_validation_error.type() != ErrorType::kConnectionError) {
          SkipAndContinue(length);
        }
        return;
      }
      if (length > 0) {
        ReadWithTimeout(length,
                        absl::bind_front(&ChannelProcessor::ProcessFrame, &processor_, header));
      } else {
        processor_.ProcessFrame(header, Buffer());
      }
    });
  }

  void ReadContinuationFrame(uint32_t const stream_id,
                             ContinuationFrameCallback callback) override {
    Read(sizeof(FrameHeader), [this, stream_id,
                               callback = std::move(callback)](Buffer const buffer) mutable {
      auto const& header = buffer.as<FrameHeader>();
      auto const header_validation_error =
          ChannelProcessor::ValidateContinuationHeader(stream_id, header);
      auto const length = header.length();
      if (!header_validation_error.ok()) {
        if (header_validation_error.type() != ErrorType::kConnectionError) {
          SkipAndContinue(length);
        }
        return;
      }
      if (length > 0) {
        ReadWithTimeout(length, [header, callback = std::move(callback)](Buffer payload) mutable {
          std::move(callback)(header, std::move(payload));
        });
      } else {
        processor_.ProcessFrame(header, Buffer());
      }
    });
  }

  void CloseConnection() override { Close(); }

  // Not owned. Points to the parent `Server` for server-side channels, while it's nullptr for
  // client-side ones.
  ChannelManager* const manager_;

  ChannelProcessor processor_;
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_CHANNEL_H__
