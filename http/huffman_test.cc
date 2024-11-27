#include "http/huffman.h"

#include <string_view>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/buffer_testing.h"

namespace {

using ::testing::ElementsAreArray;
using ::tsdb2::http::hpack::HuffmanCode;
using ::tsdb2::testing::io::BufferAsBytes;

std::string_view constexpr kText1 = "custom-value";
uint8_t const kBytes1[] = {0x25, 0xA8, 0x49, 0xE9, 0x5B, 0xB8, 0xE8, 0xB4, 0xBF};

std::string_view constexpr kText2 = "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1";

uint8_t const kBytes2[] = {
    0x94, 0xE7, 0x82, 0x1D, 0xD7, 0xF2, 0xE6, 0xC7, 0xB3, 0x35, 0xDF, 0xDF, 0xCD, 0x5B, 0x39,
    0x60, 0xD5, 0xAF, 0x27, 0x08, 0x7F, 0x36, 0x72, 0xC1, 0xAB, 0x27, 0x0F, 0xB5, 0x29, 0x1F,
    0x95, 0x87, 0x31, 0x60, 0x65, 0xC0, 0x03, 0xED, 0x4E, 0xE5, 0xB1, 0x06, 0x3D, 0x50, 0x07,
};

std::string_view constexpr kText3 = R"lipsum(
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Maecenas finibus nulla et ante vulputate,
ac congue velit euismod. Integer ut leo id nisl consequat lacinia pulvinar vel nibh. Nulla finibus
turpis non orci consequat, id consequat massa consequat. Integer consectetur mollis enim. Duis
sagittis odio accumsan iaculis pellentesque. Phasellus semper tristique sem, at tempor odio varius
id. Vestibulum et ex leo. Nunc vitae nunc leo. Fusce egestas ac erat ut sollicitudin.
)lipsum";

uint8_t const kBytes3[] = {
    0xFF, 0xFF, 0xFF, 0xF3, 0x39, 0xEC, 0x2D, 0x2A, 0x1A, 0xB4, 0x5B, 0x4A, 0x92, 0x1E, 0x83, 0xD8,
    0xA2, 0x0C, 0x95, 0x07, 0x49, 0x53, 0xF4, 0xA1, 0x0F, 0x52, 0x0A, 0x44, 0x95, 0x36, 0xD8, 0xA0,
    0xE4, 0x35, 0x66, 0x41, 0x0D, 0x54, 0xCA, 0x16, 0x83, 0x25, 0x75, 0x34, 0x0C, 0xA4, 0x2D, 0x43,
    0x42, 0x92, 0x9A, 0xA3, 0x47, 0x6A, 0x14, 0xAA, 0xDA, 0x28, 0x1A, 0x85, 0x4A, 0x83, 0xA9, 0x25,
    0x53, 0xBD, 0xB4, 0x57, 0x6A, 0x46, 0x92, 0xFD, 0x7F, 0xFF, 0xFF, 0xF8, 0x32, 0x28, 0x43, 0xD5,
    0x35, 0xA5, 0x53, 0xB9, 0x68, 0x32, 0x54, 0x2D, 0xA6, 0x45, 0x27, 0x91, 0x75, 0x32, 0x54, 0x92,
    0xCC, 0x5B, 0x14, 0xB5, 0x2A, 0x50, 0x53, 0xA8, 0x69, 0x14, 0xA8, 0xC8, 0xA1, 0x42, 0x1E, 0xA4,
    0x17, 0xB5, 0xA3, 0x4A, 0x94, 0x0C, 0x86, 0xA8, 0xC3, 0x52, 0xBB, 0x68, 0xEE, 0x6A, 0x87, 0x62,
    0x9D, 0xCB, 0x42, 0x95, 0x1A, 0x39, 0xD7, 0x53, 0x4D, 0xB4, 0x50, 0x35, 0x25, 0x35, 0x46, 0x8E,
    0xD4, 0x7F, 0xFF, 0xFF, 0xF8, 0x9B, 0x6C, 0xAC, 0xC8, 0x52, 0xA3, 0xD4, 0xA1, 0xEC, 0x21, 0x94,
    0x21, 0xEA, 0x41, 0x7B, 0x5A, 0x34, 0xFD, 0x28, 0x69, 0x14, 0x21, 0xEA, 0x41, 0x7B, 0x5A, 0x34,
    0xA9, 0x48, 0xD0, 0x81, 0xA8, 0x43, 0xD4, 0x82, 0xF6, 0xB4, 0x69, 0x5D, 0x4C, 0x95, 0x24, 0xB3,
    0x16, 0xC5, 0x08, 0x7A, 0x90, 0x52, 0x24, 0xA9, 0xB6, 0xC5, 0x29, 0x3D, 0x14, 0x19, 0x0A, 0x16,
    0xA3, 0x52, 0xBA, 0x97, 0xED, 0x32, 0x3F, 0xFF, 0xFF, 0xFC, 0x40, 0xE6, 0x32, 0x52, 0x64, 0x28,
    0x79, 0x0C, 0x75, 0x06, 0x42, 0x5B, 0x4A, 0x07, 0x52, 0x86, 0x19, 0x2D, 0xA0, 0xC8, 0x52, 0xB2,
    0xD1, 0x41, 0x6A, 0x49, 0x51, 0xDA, 0xD2, 0xAE, 0xA6, 0xB9, 0xC6, 0x82, 0xD1, 0x45, 0xA8, 0x51,
    0x05, 0xA6, 0xB2, 0xD8, 0xA2, 0x6C, 0x32, 0x12, 0x6E, 0xD6, 0x95, 0x44, 0x16, 0x9F, 0xA5, 0x06,
    0x95, 0x12, 0x5A, 0x6B, 0x3D, 0x8A, 0x1E, 0x43, 0x1D, 0x4E, 0xE3, 0xB0, 0xD6, 0xA3, 0xFF, 0xFF,
    0xFF, 0xC3, 0x48, 0xBA, 0x9C, 0x4A, 0x84, 0x9A, 0x3B, 0x68, 0xB6, 0x95, 0x0A, 0x95, 0x0B, 0xE5,
    0x4A, 0x0A, 0x75, 0xD4, 0xD3, 0x6D, 0x44, 0x53, 0xB9, 0x92, 0x32, 0xA9, 0x55, 0xB5, 0x11, 0x4A,
    0x0A, 0x75, 0xD4, 0xC3, 0x6A, 0x08, 0x55, 0x0B, 0x31, 0x50, 0x91, 0xA1, 0x41, 0x91, 0x42, 0xD8,
    0x34, 0xA9, 0x6A, 0x54, 0x41, 0xE8, 0xA0, 0xC4, 0x32, 0x6D, 0x90, 0xD5, 0x2F, 0xFF, 0xFF, 0xFF,
    0xE7,
};

TEST(HuffmanTest, Decode1) { EXPECT_EQ(HuffmanCode::Decode(kBytes1), kText1); }
TEST(HuffmanTest, Encode1) {
  EXPECT_THAT(HuffmanCode::Encode(kText1), BufferAsBytes(ElementsAreArray(kBytes1)));
}

TEST(HuffmanTest, Decode2) { EXPECT_EQ(HuffmanCode::Decode(kBytes2), kText2); }
TEST(HuffmanTest, Encode2) {
  EXPECT_THAT(HuffmanCode::Encode(kText2), BufferAsBytes(ElementsAreArray(kBytes2)));
}

TEST(HuffmanTest, Decode3) { EXPECT_EQ(HuffmanCode::Decode(kBytes3), kText3); }
TEST(HuffmanTest, Encode3) {
  EXPECT_THAT(HuffmanCode::Encode(kText3), BufferAsBytes(ElementsAreArray(kBytes3)));
}

}  // namespace
