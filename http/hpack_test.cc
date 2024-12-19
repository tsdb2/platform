#include "http/hpack.h"

#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/buffer_testing.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::AnyOf;
using ::testing::ElementsAreArray;
using ::testing::Pair;
using ::testing::UnorderedElementsAreArray;
using ::tsdb2::http::hpack::Decoder;
using ::tsdb2::http::hpack::DynamicHeaderTable;
using ::tsdb2::http::hpack::Encoder;
using ::tsdb2::http::hpack::Header;
using ::tsdb2::http::hpack::HeaderSet;
using ::tsdb2::testing::io::BufferAsBytes;

TEST(DynamicHeaderTableTest, InitialState) {
  DynamicHeaderTable table{123};
  EXPECT_EQ(table.max_size(), 123);
  EXPECT_EQ(table.size(), 0);
  EXPECT_EQ(table.num_headers(), 0);
}

TEST(DynamicHeaderTableTest, Add) {
  DynamicHeaderTable table{123};
  EXPECT_TRUE(table.Add(Header("lorem", "ipsum")));
  EXPECT_EQ(table.max_size(), 123);
  EXPECT_EQ(table.size(), 42);
  EXPECT_EQ(table.num_headers(), 1);
  EXPECT_THAT(table[0], Pair("lorem", "ipsum"));
}

TEST(DynamicHeaderTableTest, AddTwo) {
  DynamicHeaderTable table{123};
  EXPECT_TRUE(table.Add(Header("lorem", "ipsum")));
  EXPECT_TRUE(table.Add(Header("consectetur", "adipisci")));
  EXPECT_EQ(table.max_size(), 123);
  EXPECT_EQ(table.size(), 93);
  EXPECT_EQ(table.num_headers(), 2);
  EXPECT_THAT(table[0], Pair("consectetur", "adipisci"));
  EXPECT_THAT(table[1], Pair("lorem", "ipsum"));
}

TEST(DynamicHeaderTableTest, AddTooBig) {
  DynamicHeaderTable table{10};
  EXPECT_FALSE(table.Add(Header("lorem", "ipsum")));
  EXPECT_EQ(table.max_size(), 10);
  EXPECT_EQ(table.size(), 0);
  EXPECT_EQ(table.num_headers(), 0);
}

TEST(DynamicHeaderTableTest, OldestEvicted) {
  DynamicHeaderTable table{130};
  EXPECT_TRUE(table.Add(Header("sator", "arepo")));
  EXPECT_TRUE(table.Add(Header("arepo", "tenet")));
  EXPECT_TRUE(table.Add(Header("tenet", "opera")));
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  EXPECT_EQ(table.max_size(), 130);
  EXPECT_EQ(table.size(), 126);
  EXPECT_EQ(table.num_headers(), 3);
  EXPECT_THAT(table[0], Pair("opera", "rotas"));
  EXPECT_THAT(table[1], Pair("tenet", "opera"));
  EXPECT_THAT(table[2], Pair("arepo", "tenet"));
}

TEST(DynamicHeaderTableTest, AllowMore) {
  DynamicHeaderTable table{130};
  EXPECT_TRUE(table.Add(Header("sator", "arepo")));
  EXPECT_TRUE(table.Add(Header("arepo", "tenet")));
  EXPECT_TRUE(table.Add(Header("tenet", "opera")));
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  table.SetMaxSize(200);
  EXPECT_EQ(table.max_size(), 200);
  EXPECT_EQ(table.size(), 126);
  EXPECT_EQ(table.num_headers(), 3);
  EXPECT_THAT(table[0], Pair("opera", "rotas"));
  EXPECT_THAT(table[1], Pair("tenet", "opera"));
  EXPECT_THAT(table[2], Pair("arepo", "tenet"));
}

TEST(DynamicHeaderTableTest, Reinsert) {
  DynamicHeaderTable table{130};
  EXPECT_TRUE(table.Add(Header("sator", "arepo")));
  EXPECT_TRUE(table.Add(Header("arepo", "tenet")));
  EXPECT_TRUE(table.Add(Header("tenet", "opera")));
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  table.SetMaxSize(200);
  ASSERT_EQ(table.max_size(), 200);
  ASSERT_EQ(table.size(), 126);
  ASSERT_EQ(table.num_headers(), 3);
  EXPECT_TRUE(table.Add(Header("sator", "arepo")));
  EXPECT_EQ(table.size(), 168);
  EXPECT_EQ(table.num_headers(), 4);
  EXPECT_THAT(table[0], Pair("sator", "arepo"));
  EXPECT_THAT(table[1], Pair("opera", "rotas"));
  EXPECT_THAT(table[2], Pair("tenet", "opera"));
  EXPECT_THAT(table[3], Pair("arepo", "tenet"));
}

TEST(DynamicHeaderTableTest, AllowLess) {
  DynamicHeaderTable table{130};
  EXPECT_TRUE(table.Add(Header("sator", "arepo")));
  EXPECT_TRUE(table.Add(Header("arepo", "tenet")));
  EXPECT_TRUE(table.Add(Header("tenet", "opera")));
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  table.SetMaxSize(100);
  EXPECT_EQ(table.max_size(), 100);
  EXPECT_EQ(table.size(), 84);
  EXPECT_EQ(table.num_headers(), 2);
  EXPECT_THAT(table[0], Pair("opera", "rotas"));
  EXPECT_THAT(table[1], Pair("tenet", "opera"));
}

TEST(DynamicHeaderTableTest, FindInEmpty) {
  DynamicHeaderTable table{130};
  ASSERT_EQ(table.size(), 0);
  ASSERT_EQ(table.num_headers(), 0);
  EXPECT_LT(table.FindHeader(Header("opera", "rotas")), 0);
  EXPECT_LT(table.FindHeader(Header("tenet", "opera")), 0);
  EXPECT_LT(table.FindHeader(Header("arepo", "tenet")), 0);
  EXPECT_LT(table.FindHeader(Header("sator", "arepo")), 0);
  EXPECT_LT(table.FindHeader(Header("lorem", "ipsum")), 0);
}

TEST(DynamicHeaderTableTest, Find) {
  DynamicHeaderTable table{130};
  EXPECT_TRUE(table.Add(Header("sator", "arepo")));
  EXPECT_TRUE(table.Add(Header("arepo", "tenet")));
  EXPECT_TRUE(table.Add(Header("tenet", "opera")));
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  ASSERT_EQ(table.max_size(), 130);
  ASSERT_EQ(table.size(), 126);
  ASSERT_EQ(table.num_headers(), 3);
  EXPECT_EQ(table.FindHeader(Header("opera", "rotas")), 0);
  EXPECT_EQ(table.FindHeader(Header("tenet", "opera")), 1);
  EXPECT_EQ(table.FindHeader(Header("arepo", "tenet")), 2);
  EXPECT_LT(table.FindHeader(Header("sator", "arepo")), 0);
  EXPECT_LT(table.FindHeader(Header("lorem", "ipsum")), 0);
}

TEST(DynamicHeaderTableTest, FindNameInEmpty) {
  DynamicHeaderTable table{130};
  ASSERT_EQ(table.size(), 0);
  ASSERT_EQ(table.num_headers(), 0);
  EXPECT_LT(table.FindHeaderName("opera"), 0);
  EXPECT_LT(table.FindHeaderName("tenet"), 0);
  EXPECT_LT(table.FindHeaderName("arepo"), 0);
  EXPECT_LT(table.FindHeaderName("sator"), 0);
  EXPECT_LT(table.FindHeaderName("lorem"), 0);
}

TEST(DynamicHeaderTableTest, FindName) {
  DynamicHeaderTable table{130};
  EXPECT_TRUE(table.Add(Header("sator", "arepo")));
  EXPECT_TRUE(table.Add(Header("arepo", "tenet")));
  EXPECT_TRUE(table.Add(Header("tenet", "opera")));
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  ASSERT_EQ(table.max_size(), 130);
  ASSERT_EQ(table.size(), 126);
  ASSERT_EQ(table.num_headers(), 3);
  EXPECT_EQ(table.FindHeaderName("opera"), 0);
  EXPECT_EQ(table.FindHeaderName("tenet"), 1);
  EXPECT_EQ(table.FindHeaderName("arepo"), 2);
  EXPECT_LT(table.FindHeaderName("sator"), 0);
  EXPECT_LT(table.FindHeaderName("lorem"), 0);
}

TEST(DynamicHeaderTableTest, FindNameAmongDuplicates) {
  DynamicHeaderTable table{130};
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  EXPECT_TRUE(table.Add(Header("tenet", "opera")));
  EXPECT_TRUE(table.Add(Header("opera", "rotas")));
  ASSERT_EQ(table.max_size(), 130);
  ASSERT_EQ(table.size(), 126);
  ASSERT_EQ(table.num_headers(), 3);
  EXPECT_THAT(table.FindHeaderName("opera"), AnyOf(0, 2));
  EXPECT_EQ(table.FindHeaderName("tenet"), 1);
  EXPECT_LT(table.FindHeaderName("sator"), 0);
}

class DecoderTest : public ::testing::Test {
 protected:
  Decoder decoder_;
};

TEST_F(DecoderTest, RequestsWithoutHuffman) {
  EXPECT_THAT(decoder_.Decode({
                  0x82, 0x86, 0x84, 0x41, 0x0F, 0x77, 0x77, 0x77, 0x2E, 0x65,
                  0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D,
              }),
              IsOkAndHolds(UnorderedElementsAreArray({
                  Pair(":method", "GET"),
                  Pair(":scheme", "http"),
                  Pair(":path", "/"),
                  Pair(":authority", "www.example.com"),
              })));
  EXPECT_THAT(decoder_.Decode({
                  0x82,
                  0x86,
                  0x84,
                  0xBE,
                  0x58,
                  0x08,
                  0x6E,
                  0x6F,
                  0x2D,
                  0x63,
                  0x61,
                  0x63,
                  0x68,
                  0x65,
              }),
              IsOkAndHolds(UnorderedElementsAreArray({
                  Pair(":method", "GET"),
                  Pair(":scheme", "http"),
                  Pair(":path", "/"),
                  Pair(":authority", "www.example.com"),
                  Pair("cache-control", "no-cache"),
              })));
  EXPECT_THAT(
      decoder_.Decode({
          0x82, 0x87, 0x85, 0xBF, 0x40, 0x0A, 0x63, 0x75, 0x73, 0x74, 0x6F, 0x6D, 0x2D, 0x6B, 0x65,
          0x79, 0x0C, 0x63, 0x75, 0x73, 0x74, 0x6F, 0x6D, 0x2D, 0x76, 0x61, 0x6C, 0x75, 0x65,
      }),
      IsOkAndHolds(UnorderedElementsAreArray({
          Pair(":method", "GET"),
          Pair(":scheme", "https"),
          Pair(":path", "/index.html"),
          Pair(":authority", "www.example.com"),
          Pair("custom-key", "custom-value"),
      })));
}

TEST_F(DecoderTest, RequestsWithHuffman) {
  EXPECT_THAT(decoder_.Decode({
                  0x82,
                  0x86,
                  0x84,
                  0x41,
                  0x8C,
                  0xF1,
                  0xE3,
                  0xC2,
                  0xE5,
                  0xF2,
                  0x3A,
                  0x6B,
                  0xA0,
                  0xAB,
                  0x90,
                  0xF4,
                  0xFF,
              }),
              IsOkAndHolds(UnorderedElementsAreArray({
                  Pair(":method", "GET"),
                  Pair(":scheme", "http"),
                  Pair(":path", "/"),
                  Pair(":authority", "www.example.com"),
              })));
  EXPECT_THAT(decoder_.Decode({
                  0x82,
                  0x86,
                  0x84,
                  0xBE,
                  0x58,
                  0x86,
                  0xA8,
                  0xEB,
                  0x10,
                  0x64,
                  0x9C,
                  0xBF,
              }),
              IsOkAndHolds(UnorderedElementsAreArray({
                  Pair(":method", "GET"),
                  Pair(":scheme", "http"),
                  Pair(":path", "/"),
                  Pair(":authority", "www.example.com"),
                  Pair("cache-control", "no-cache"),
              })));
  EXPECT_THAT(decoder_.Decode({
                  0x82, 0x87, 0x85, 0xBF, 0x40, 0x88, 0x25, 0xA8, 0x49, 0xE9, 0x5B, 0xA9,
                  0x7D, 0x7F, 0x89, 0x25, 0xA8, 0x49, 0xE9, 0x5B, 0xB8, 0xE8, 0xB4, 0xBF,
              }),
              IsOkAndHolds(UnorderedElementsAreArray({
                  Pair(":method", "GET"),
                  Pair(":scheme", "https"),
                  Pair(":path", "/index.html"),
                  Pair(":authority", "www.example.com"),
                  Pair("custom-key", "custom-value"),
              })));
}

TEST_F(DecoderTest, ResponsesWithoutHuffman) {
  EXPECT_THAT(
      decoder_.Decode({
          0x48, 0x03, 0x33, 0x30, 0x32, 0x58, 0x07, 0x70, 0x72, 0x69, 0x76, 0x61, 0x74, 0x65,
          0x61, 0x1D, 0x4D, 0x6F, 0x6E, 0x2C, 0x20, 0x32, 0x31, 0x20, 0x4F, 0x63, 0x74, 0x20,
          0x32, 0x30, 0x31, 0x33, 0x20, 0x32, 0x30, 0x3A, 0x31, 0x33, 0x3A, 0x32, 0x31, 0x20,
          0x47, 0x4D, 0x54, 0x6E, 0x17, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3A, 0x2F, 0x2F, 0x77,
          0x77, 0x77, 0x2E, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D,
      }),
      IsOkAndHolds(UnorderedElementsAreArray({
          Pair(":status", "302"),
          Pair("cache-control", "private"),
          Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
          Pair("location", "https://www.example.com"),
      })));
  EXPECT_THAT(decoder_.Decode({
                  0x48,
                  0x03,
                  0x33,
                  0x30,
                  0x37,
                  0xC1,
                  0xC0,
                  0xBF,
              }),
              IsOkAndHolds(UnorderedElementsAreArray({
                  Pair(":status", "307"),
                  Pair("cache-control", "private"),
                  Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
                  Pair("location", "https://www.example.com"),
              })));
  EXPECT_THAT(
      decoder_.Decode({
          0x88, 0xC1, 0x61, 0x1D, 0x4D, 0x6F, 0x6E, 0x2C, 0x20, 0x32, 0x31, 0x20, 0x4F, 0x63,
          0x74, 0x20, 0x32, 0x30, 0x31, 0x33, 0x20, 0x32, 0x30, 0x3A, 0x31, 0x33, 0x3A, 0x32,
          0x32, 0x20, 0x47, 0x4D, 0x54, 0xC0, 0x5A, 0x04, 0x67, 0x7A, 0x69, 0x70, 0x77, 0x38,
          0x66, 0x6F, 0x6F, 0x3D, 0x41, 0x53, 0x44, 0x4A, 0x4B, 0x48, 0x51, 0x4B, 0x42, 0x5A,
          0x58, 0x4F, 0x51, 0x57, 0x45, 0x4F, 0x50, 0x49, 0x55, 0x41, 0x58, 0x51, 0x57, 0x45,
          0x4F, 0x49, 0x55, 0x3B, 0x20, 0x6D, 0x61, 0x78, 0x2D, 0x61, 0x67, 0x65, 0x3D, 0x33,
          0x36, 0x30, 0x30, 0x3B, 0x20, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x3D, 0x31,
      }),
      IsOkAndHolds(UnorderedElementsAreArray({
          Pair(":status", "200"),
          Pair("cache-control", "private"),
          Pair("date", "Mon, 21 Oct 2013 20:13:22 GMT"),
          Pair("location", "https://www.example.com"),
          Pair("content-encoding", "gzip"),
          Pair("set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"),
      })));
}

TEST_F(DecoderTest, ResponsesWithHuffman) {
  EXPECT_THAT(
      decoder_.Decode({
          0x48, 0x82, 0x64, 0x02, 0x58, 0x85, 0xAE, 0xC3, 0x77, 0x1A, 0x4B, 0x61, 0x96, 0xD0,
          0x7A, 0xBE, 0x94, 0x10, 0x54, 0xD4, 0x44, 0xA8, 0x20, 0x05, 0x95, 0x04, 0x0B, 0x81,
          0x66, 0xE0, 0x82, 0xA6, 0x2D, 0x1B, 0xFF, 0x6E, 0x91, 0x9D, 0x29, 0xAD, 0x17, 0x18,
          0x63, 0xC7, 0x8F, 0x0B, 0x97, 0xC8, 0xE9, 0xAE, 0x82, 0xAE, 0x43, 0xD3,
      }),
      IsOkAndHolds(UnorderedElementsAreArray({
          Pair(":status", "302"),
          Pair("cache-control", "private"),
          Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
          Pair("location", "https://www.example.com"),
      })));
  EXPECT_THAT(decoder_.Decode({
                  0x48,
                  0x83,
                  0x64,
                  0x0E,
                  0xFF,
                  0xC1,
                  0xC0,
                  0xBF,
              }),
              IsOkAndHolds(UnorderedElementsAreArray({
                  Pair(":status", "307"),
                  Pair("cache-control", "private"),
                  Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
                  Pair("location", "https://www.example.com"),
              })));
  EXPECT_THAT(
      decoder_.Decode({
          0x88, 0xC1, 0x61, 0x96, 0xD0, 0x7A, 0xBE, 0x94, 0x10, 0x54, 0xD4, 0x44, 0xA8, 0x20,
          0x05, 0x95, 0x04, 0x0B, 0x81, 0x66, 0xE0, 0x84, 0xA6, 0x2D, 0x1B, 0xFF, 0xC0, 0x5A,
          0x83, 0x9B, 0xD9, 0xAB, 0x77, 0xAD, 0x94, 0xE7, 0x82, 0x1D, 0xD7, 0xF2, 0xE6, 0xC7,
          0xB3, 0x35, 0xDF, 0xDF, 0xCD, 0x5B, 0x39, 0x60, 0xD5, 0xAF, 0x27, 0x08, 0x7F, 0x36,
          0x72, 0xC1, 0xAB, 0x27, 0x0F, 0xB5, 0x29, 0x1F, 0x95, 0x87, 0x31, 0x60, 0x65, 0xC0,
          0x03, 0xED, 0x4E, 0xE5, 0xB1, 0x06, 0x3D, 0x50, 0x07,
      }),
      IsOkAndHolds(UnorderedElementsAreArray({
          Pair(":status", "200"),
          Pair("cache-control", "private"),
          Pair("date", "Mon, 21 Oct 2013 20:13:22 GMT"),
          Pair("location", "https://www.example.com"),
          Pair("content-encoding", "gzip"),
          Pair("set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"),
      })));
}

class EncoderTest : public ::testing::Test {
 protected:
  Encoder encoder_;
};

TEST_F(EncoderTest, Requests) {
  EXPECT_THAT(encoder_.Encode({
                  {":method", "GET"},
                  {":scheme", "http"},
                  {":path", "/"},
                  {":authority", "www.example.com"},
              }),
              BufferAsBytes(ElementsAreArray({
                  0x82,
                  0x86,
                  0x84,
                  0x41,
                  0x8C,
                  0xF1,
                  0xE3,
                  0xC2,
                  0xE5,
                  0xF2,
                  0x3A,
                  0x6B,
                  0xA0,
                  0xAB,
                  0x90,
                  0xF4,
                  0xFF,
              })));
  EXPECT_THAT(encoder_.Encode({
                  {":method", "GET"},
                  {":scheme", "http"},
                  {":path", "/"},
                  {":authority", "www.example.com"},
                  {"cache-control", "no-cache"},
              }),
              BufferAsBytes(ElementsAreArray({
                  0x82,
                  0x86,
                  0x84,
                  0xBE,
                  0x58,
                  0x86,
                  0xA8,
                  0xEB,
                  0x10,
                  0x64,
                  0x9C,
                  0xBF,
              })));
  EXPECT_THAT(encoder_.Encode({
                  {":method", "GET"},
                  {":scheme", "https"},
                  {":path", "/index.html"},
                  {":authority", "www.example.com"},
                  {"custom-key", "custom-value"},
              }),
              BufferAsBytes(ElementsAreArray({
                  0x82, 0x87, 0x85, 0xBF, 0x40, 0x88, 0x25, 0xA8, 0x49, 0xE9, 0x5B, 0xA9,
                  0x7D, 0x7F, 0x89, 0x25, 0xA8, 0x49, 0xE9, 0x5B, 0xB8, 0xE8, 0xB4, 0xBF,
              })));
}

TEST_F(EncoderTest, Responses) {
  EXPECT_THAT(
      encoder_.Encode({
          {":status", "302"},
          {"cache-control", "private"},
          {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
          {"location", "https://www.example.com"},
      }),
      BufferAsBytes(ElementsAreArray({
          0x48, 0x82, 0x64, 0x02, 0x58, 0x85, 0xAE, 0xC3, 0x77, 0x1A, 0x4B, 0x61, 0x96, 0xD0,
          0x7A, 0xBE, 0x94, 0x10, 0x54, 0xD4, 0x44, 0xA8, 0x20, 0x05, 0x95, 0x04, 0x0B, 0x81,
          0x66, 0xE0, 0x82, 0xA6, 0x2D, 0x1B, 0xFF, 0x6E, 0x91, 0x9D, 0x29, 0xAD, 0x17, 0x18,
          0x63, 0xC7, 0x8F, 0x0B, 0x97, 0xC8, 0xE9, 0xAE, 0x82, 0xAE, 0x43, 0xD3,
      })));
  EXPECT_THAT(encoder_.Encode({
                  {":status", "307"},
                  {"cache-control", "private"},
                  {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
                  {"location", "https://www.example.com"},
              }),
              BufferAsBytes(ElementsAreArray({
                  0x48,
                  0x03,
                  0x33,
                  0x30,
                  0x37,
                  0xC1,
                  0xC0,
                  0xBF,
              })));
  EXPECT_THAT(
      encoder_.Encode({
          {":status", "200"},
          {"cache-control", "private"},
          {"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
          {"location", "https://www.example.com"},
          {"content-encoding", "gzip"},
          {"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
      }),
      BufferAsBytes(ElementsAreArray({
          0x88, 0xC1, 0x61, 0x96, 0xD0, 0x7A, 0xBE, 0x94, 0x10, 0x54, 0xD4, 0x44, 0xA8, 0x20,
          0x05, 0x95, 0x04, 0x0B, 0x81, 0x66, 0xE0, 0x84, 0xA6, 0x2D, 0x1B, 0xFF, 0xC0, 0x5A,
          0x83, 0x9B, 0xD9, 0xAB, 0x77, 0xAD, 0x94, 0xE7, 0x82, 0x1D, 0xD7, 0xF2, 0xE6, 0xC7,
          0xB3, 0x35, 0xDF, 0xDF, 0xCD, 0x5B, 0x39, 0x60, 0xD5, 0xAF, 0x27, 0x08, 0x7F, 0x36,
          0x72, 0xC1, 0xAB, 0x27, 0x0F, 0xB5, 0x29, 0x1F, 0x95, 0x87, 0x31, 0x60, 0x65, 0xC0,
          0x03, 0xED, 0x4E, 0xE5, 0xB1, 0x06, 0x3D, 0x50, 0x07,
      })));
}

class PairTest : public ::testing::Test {
 protected:
  absl::StatusOr<HeaderSet> Transcode(HeaderSet const& headers);

  Encoder encoder_;
  Decoder decoder_;
};

absl::StatusOr<HeaderSet> PairTest::Transcode(HeaderSet const& headers) {
  auto const buffer = encoder_.Encode(headers);
  return decoder_.Decode(buffer.span());
}

TEST_F(PairTest, Requests) {
  HeaderSet const headers1{
      {":method", "GET"},
      {":scheme", "http"},
      {":path", "/"},
      {":authority", "www.example.com"},
  };
  EXPECT_THAT(Transcode(headers1), IsOkAndHolds(headers1));

  HeaderSet const headers2{
      {":method", "GET"},
      {":scheme", "http"},
      {":path", "/"},
      {":authority", "www.example.com"},
      {"cache-control", "no-cache"},
  };
  EXPECT_THAT(Transcode(headers2), IsOkAndHolds(headers2));

  HeaderSet const headers3{
      {":method", "GET"},
      {":scheme", "https"},
      {":path", "/index.html"},
      {":authority", "www.example.com"},
      {"custom-key", "custom-value"},
  };
  EXPECT_THAT(Transcode(headers3), IsOkAndHolds(headers3));
}

TEST_F(PairTest, Responses) {
  HeaderSet const headers1{
      {":status", "302"},
      {"cache-control", "private"},
      {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
      {"location", "https://www.example.com"},
  };
  EXPECT_THAT(Transcode(headers1), IsOkAndHolds(headers1));

  HeaderSet const headers2{
      {":status", "307"},
      {"cache-control", "private"},
      {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
      {"location", "https://www.example.com"},
  };
  EXPECT_THAT(Transcode(headers2), IsOkAndHolds(headers2));

  HeaderSet const headers3{
      {":status", "200"},
      {"cache-control", "private"},
      {"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
      {"location", "https://www.example.com"},
      {"content-encoding", "gzip"},
      {"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
  };
  EXPECT_THAT(Transcode(headers3), IsOkAndHolds(headers3));
}

}  // namespace
