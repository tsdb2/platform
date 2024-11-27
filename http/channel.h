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
#include "common/reffed_ptr.h"
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
  virtual ~ChannelInterface() = default;
  virtual tsdb2::net::BaseSocket* socket() = 0;
  virtual tsdb2::net::BaseSocket const* socket() const = 0;
  virtual void ReadContinuationFrame(uint32_t stream_id) = 0;
  virtual void ReadNextFrame() = 0;
  virtual void CloseConnection() = 0;
};

}  // namespace internal

// Manages a single HTTP/2 connection (with multiplexed streams).
template <typename Socket,
          std::enable_if_t<std::is_base_of_v<tsdb2::net::BaseSocket, Socket>, bool> = true>
class Channel final : private Socket, public BaseChannel, private internal::ChannelInterface {
 public:
  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<Channel>> Create(Args&&... args) {
    return Socket::template Create<Channel>(std::forward<Args>(args)...);
  }

  // Creates a channel connected to a socket. Used in test scenarios to check the frames written by
  // the channel.
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<Channel>, tsdb2::common::reffed_ptr<Socket>>>
  CreatePairWithRawPeerForTesting() {
    return CreatePairWithRawPeerHelper<Socket>();
  }

  // Creates a pair of channels connected to each other.
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<Channel>, tsdb2::common::reffed_ptr<Channel>>>
  CreatePairForTesting() {
    return CreatePairHelper<Socket>();
  }

  using Socket::Close;
  using Socket::is_open;
  using Socket::kIsListener;

  void Ref() override { Socket::Ref(); }
  bool Unref() override { return Socket::Unref(); }

  ~Channel() { Close(); }

  // Starts an HTTP/2 server by reading the client preface and then starting exchanging frames.
  void StartServer() override {
    ReadWithTimeout(kClientPreface.size(), [this](tsdb2::net::Buffer const data) {
      std::string_view const preface{data.as_char_array(), data.size()};
      if (preface != kClientPreface) {
        LOG(ERROR) << "HTTP/2 client preface error: \"" << absl::CEscape(preface) << "\"";
        Close();
      } else {
        processor_.SendSettings();
        ReadNextFrame();
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
  CreatePairWithRawPeerHelper();

  template <>
  absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>,
                           tsdb2::common::reffed_ptr<tsdb2::net::Socket>>>
  CreatePairWithRawPeerHelper<tsdb2::net::Socket>() {
    return tsdb2::net::EpollServer::GetInstance()
        ->CreateHeterogeneousSocketPair<tsdb2::net::Socket, Channel<tsdb2::net::Socket>,
                                        tsdb2::net::Socket>();
  }

  template <>
  absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>,
                           tsdb2::common::reffed_ptr<tsdb2::net::SSLSocket>>>
  CreatePairWithRawPeerHelper<tsdb2::net::SSLSocket>() {
    return tsdb2::net::SSLSocket::CreateHeterogeneousPairForTesting<Channel<tsdb2::net::SSLSocket>,
                                                                    tsdb2::net::SSLSocket>();
  }

  template <typename SocketType>
  static absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<SocketType>>,
                                  tsdb2::common::reffed_ptr<Channel<SocketType>>>>
  CreatePairHelper();

  template <>
  absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>,
                           tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>>>
  CreatePairHelper<tsdb2::net::Socket>() {
    return tsdb2::net::EpollServer::GetInstance()->CreateSocketPair<Channel<tsdb2::net::Socket>>();
  }

  template <>
  absl::StatusOr<std::pair<tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>,
                           tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>>>
  CreatePairHelper<tsdb2::net::SSLSocket>() {
    return tsdb2::net::SSLSocket::CreateHeterogeneousPairForTesting<
        Channel<tsdb2::net::SSLSocket>, Channel<tsdb2::net::SSLSocket>>();
  }

  template <typename... Args>
  explicit Channel(Args&&... args) : Socket(std::forward<Args>(args)...), processor_{this} {}

  Channel(Channel const&) = delete;
  Channel& operator=(Channel const&) = delete;
  Channel(Channel&&) = delete;
  Channel& operator=(Channel&&) = delete;

  tsdb2::net::BaseSocket* socket() override { return this; }
  tsdb2::net::BaseSocket const* socket() const override { return this; }

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

  void ReadContinuationFrame(uint32_t const stream_id) override {
    Read(sizeof(FrameHeader), [this, stream_id](Buffer const buffer) {
      auto const& header = buffer.as<FrameHeader>();
      bool going_away = false;
      // TODO: even if we're expecting a CONTINUATION frame we need to keep accepting high-priority
      // frames like PING and GOAWAY.
      if (header.frame_type() != FrameType::kContinuation) {
        going_away = true;
        processor_.GoAway(ConnectionError::kProtocolError);
      } else {
        auto const header_validation_error =
            processor_.ValidateContinuationHeader(stream_id, header);
        going_away = header_validation_error != ConnectionError::kNoError;
      }
      auto const length = header.length();
      if (going_away) {
        if (length > 0) {
          Skip(length, [this] { ReadNextFrame(); });
        } else {
          ReadNextFrame();
        }
      } else {
        if (length > 0) {
          ReadWithTimeout(length, absl::bind_front(&ChannelProcessor::ProcessContinuationFrame,
                                                   &processor_, stream_id, header));
        } else {
          processor_.ProcessContinuationFrame(stream_id, header, Buffer());
        }
      }
    });
  }

  void ReadNextFrame() override {
    Read(sizeof(FrameHeader), [this](Buffer const buffer) {
      auto const& header = buffer.as<FrameHeader>();
      auto const header_validation_error = processor_.ValidateFrameHeader(header);
      auto const length = header.length();
      if (header_validation_error != ConnectionError::kNoError &&
          header.frame_type() != FrameType::kGoAway) {
        if (length > 0) {
          Skip(length, [this] { ReadNextFrame(); });
        } else {
          ReadNextFrame();
        }
      } else {
        if (length > 0) {
          ReadWithTimeout(length,
                          absl::bind_front(&ChannelProcessor::ProcessFrame, &processor_, header));
        } else {
          processor_.ProcessFrame(header, Buffer());
        }
      }
    });
  }

  void CloseConnection() override { Close(); }

  ChannelProcessor processor_;
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_CHANNEL_H__
