#include "http/huffman.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/types/span.h"
#include "io/buffer.h"

namespace tsdb2 {
namespace http {
namespace hpack {

namespace {

using ::tsdb2::io::Buffer;

// A node of the Huffman tree.
//
// We store nodes in a single contiguous array (see `nodes` below).
//
// `left` and `right` refer to the left and right child. A value greater than 0 indicates that the
// respective child is another internal node whose index in the `nodes` array corresponds to that
// value. A negative value indicates that the respective child is a leaf labeled with `-value`. A
// value of zero also indicates that the edge points to a leaf, and in this case the leaf label is 0
// (i.e. the NUL terminator character). We're using the value 0 as a leaf rather than an internal
// node because node #0 is the root of the tree and no nodes point to the root.
//
// Examples:
//
//   * if `left` is 42 the left child of the node is `nodes[42]`;
//   * if `right` is -65 the right child of the node is the leaf corresponding to character 65,
//     that is `A`.
//
struct ABSL_ATTRIBUTE_PACKED Node {
  int16_t left;
  int16_t right;
};

// Describes how to encode a character.
struct ABSL_ATTRIBUTE_PACKED Encoding {
  // The number of bits in the encoding. It's always greater than 0 and less than 32.
  uint32_t length;

  // The LBS-aligned bits of the encoding.
  //
  // The `length` least significant bits of this 32-bit word must be appended to the output to
  // encode the corresponding character.
  uint32_t bits;
};

// Number of leaves in this Huffman tree, as per the specs.
inline size_t constexpr kNumLeaves = 257;

// The nodes of the Huffman tree, used for decoding.
//
// The Huffman code in the HTTPWG specification has exactly 257 leaves. Since Huffman trees don't
// have any nodes with only one child, the number of internal nodes must be 257-1 = 256 and the
// total number of nodes must be 2*257-1 = 513.
//
// In this array we only store the internal nodes because leaf nodes are implicitly stored inside
// internal nodes as negative edge values (see the `Node` struct above).
//
// The root of the tree is the node at slot #0.
//
// NOTE: one of the nodes has its left edge set to 0. That's meant as a label (i.e. character 0, the
// NUL terminator), not as a node index. No nodes point to node #0 because that's the root of the
// tree.
std::array<Node, kNumLeaves - 1> constexpr nodes{
    Node{.left = 66, .right = 1},      Node{.left = 93, .right = 2},
    Node{.left = 104, .right = 3},     Node{.left = 119, .right = 4},
    Node{.left = 144, .right = 5},     Node{.left = 75, .right = 6},
    Node{.left = 123, .right = 7},     Node{.left = 71, .right = 8},
    Node{.left = 77, .right = 9},      Node{.left = 73, .right = 10},
    Node{.left = 11, .right = 13},     Node{.left = 12, .right = 102},
    Node{.left = 0, .right = -36},     Node{.left = 127, .right = 14},
    Node{.left = 128, .right = 15},    Node{.left = 98, .right = 16},
    Node{.left = -123, .right = 17},   Node{.left = 124, .right = 18},
    Node{.left = 150, .right = 19},    Node{.left = 20, .right = 25},
    Node{.left = 199, .right = 21},    Node{.left = 216, .right = 22},
    Node{.left = 23, .right = 162},    Node{.left = 24, .right = 161},
    Node{.left = -1, .right = -135},   Node{.left = 167, .right = 26},
    Node{.left = 41, .right = 27},     Node{.left = 191, .right = 28},
    Node{.left = 211, .right = 29},    Node{.left = 229, .right = 30},
    Node{.left = 31, .right = 45},     Node{.left = 32, .right = 38},
    Node{.left = 33, .right = 35},     Node{.left = -254, .right = 34},
    Node{.left = -2, .right = -3},     Node{.left = 36, .right = 37},
    Node{.left = -4, .right = -5},     Node{.left = -6, .right = -7},
    Node{.left = 39, .right = 52},     Node{.left = 40, .right = 51},
    Node{.left = -8, .right = -11},    Node{.left = 208, .right = 42},
    Node{.left = 43, .right = 165},    Node{.left = -239, .right = 44},
    Node{.left = -9, .right = -142},   Node{.left = 55, .right = 46},
    Node{.left = 63, .right = 47},     Node{.left = 147, .right = 48},
    Node{.left = -249, .right = 49},   Node{.left = 50, .right = 59},
    Node{.left = -10, .right = -13},   Node{.left = -12, .right = -14},
    Node{.left = 53, .right = 54},     Node{.left = -15, .right = -16},
    Node{.left = -17, .right = -18},   Node{.left = 56, .right = 60},
    Node{.left = 57, .right = 58},     Node{.left = -19, .right = -20},
    Node{.left = -21, .right = -23},   Node{.left = -22, .right = -256},
    Node{.left = 61, .right = 62},     Node{.left = -24, .right = -25},
    Node{.left = -26, .right = -27},   Node{.left = 64, .right = 65},
    Node{.left = -28, .right = -29},   Node{.left = -30, .right = -31},
    Node{.left = 85, .right = 67},     Node{.left = 68, .right = 82},
    Node{.left = 143, .right = 69},    Node{.left = 70, .right = 81},
    Node{.left = -32, .right = -37},   Node{.left = 72, .right = 79},
    Node{.left = -33, .right = -34},   Node{.left = -124, .right = 74},
    Node{.left = -35, .right = -62},   Node{.left = 76, .right = 80},
    Node{.left = -38, .right = -42},   Node{.left = -63, .right = 78},
    Node{.left = -39, .right = -43},   Node{.left = -40, .right = -41},
    Node{.left = -44, .right = -59},   Node{.left = -45, .right = -46},
    Node{.left = 83, .right = 90},     Node{.left = 84, .right = 89},
    Node{.left = -47, .right = -51},   Node{.left = 86, .right = 130},
    Node{.left = 87, .right = 88},     Node{.left = -48, .right = -49},
    Node{.left = -50, .right = -97},   Node{.left = -52, .right = -53},
    Node{.left = 91, .right = 92},     Node{.left = -54, .right = -55},
    Node{.left = -56, .right = -57},   Node{.left = 99, .right = 94},
    Node{.left = 138, .right = 95},    Node{.left = 142, .right = 96},
    Node{.left = 97, .right = 103},    Node{.left = -58, .right = -66},
    Node{.left = -60, .right = -96},   Node{.left = 100, .right = 132},
    Node{.left = 101, .right = 129},   Node{.left = -61, .right = -65},
    Node{.left = -64, .right = -91},   Node{.left = -67, .right = -68},
    Node{.left = 105, .right = 112},   Node{.left = 106, .right = 109},
    Node{.left = 107, .right = 108},   Node{.left = -69, .right = -70},
    Node{.left = -71, .right = -72},   Node{.left = 110, .right = 111},
    Node{.left = -73, .right = -74},   Node{.left = -75, .right = -76},
    Node{.left = 113, .right = 116},   Node{.left = 114, .right = 115},
    Node{.left = -77, .right = -78},   Node{.left = -79, .right = -80},
    Node{.left = 117, .right = 118},   Node{.left = -81, .right = -82},
    Node{.left = -83, .right = -84},   Node{.left = 120, .right = 136},
    Node{.left = 121, .right = 122},   Node{.left = -85, .right = -86},
    Node{.left = -87, .right = -89},   Node{.left = -88, .right = -90},
    Node{.left = 125, .right = 155},   Node{.left = 126, .right = 148},
    Node{.left = -92, .right = -195},  Node{.left = -93, .right = -126},
    Node{.left = -94, .right = -125},  Node{.left = -95, .right = -98},
    Node{.left = 131, .right = 135},   Node{.left = -99, .right = -101},
    Node{.left = 133, .right = 134},   Node{.left = -100, .right = -102},
    Node{.left = -103, .right = -104}, Node{.left = -105, .right = -111},
    Node{.left = 137, .right = 141},   Node{.left = -106, .right = -107},
    Node{.left = 139, .right = 140},   Node{.left = -108, .right = -109},
    Node{.left = -110, .right = -112}, Node{.left = -113, .right = -118},
    Node{.left = -114, .right = -117}, Node{.left = -115, .right = -116},
    Node{.left = 145, .right = 146},   Node{.left = -119, .right = -120},
    Node{.left = -121, .right = -122}, Node{.left = -127, .right = -220},
    Node{.left = -208, .right = 149},  Node{.left = -128, .right = -130},
    Node{.left = 196, .right = 151},   Node{.left = 152, .right = 178},
    Node{.left = 153, .right = 158},   Node{.left = -230, .right = 154},
    Node{.left = -129, .right = -132}, Node{.left = 156, .right = 175},
    Node{.left = 157, .right = 204},   Node{.left = -131, .right = -162},
    Node{.left = 159, .right = 160},   Node{.left = -133, .right = -134},
    Node{.left = -136, .right = -146}, Node{.left = -137, .right = -138},
    Node{.left = 163, .right = 164},   Node{.left = -139, .right = -140},
    Node{.left = -141, .right = -143}, Node{.left = 166, .right = 171},
    Node{.left = -144, .right = -145}, Node{.left = 168, .right = 185},
    Node{.left = 169, .right = 173},   Node{.left = 170, .right = 172},
    Node{.left = -147, .right = -149}, Node{.left = -148, .right = -159},
    Node{.left = -150, .right = -151}, Node{.left = 174, .right = 181},
    Node{.left = -152, .right = -155}, Node{.left = 241, .right = 176},
    Node{.left = 177, .right = 188},   Node{.left = -153, .right = -161},
    Node{.left = 179, .right = 183},   Node{.left = 180, .right = 182},
    Node{.left = -154, .right = -156}, Node{.left = -157, .right = -158},
    Node{.left = -160, .right = -163}, Node{.left = 184, .right = 190},
    Node{.left = -164, .right = -169}, Node{.left = 186, .right = 194},
    Node{.left = 187, .right = 189},   Node{.left = -165, .right = -166},
    Node{.left = -167, .right = -172}, Node{.left = -168, .right = -174},
    Node{.left = -170, .right = -173}, Node{.left = 192, .right = 218},
    Node{.left = 193, .right = 234},   Node{.left = -171, .right = -206},
    Node{.left = 195, .right = 203},   Node{.left = -175, .right = -180},
    Node{.left = 197, .right = 235},   Node{.left = 198, .right = 202},
    Node{.left = -176, .right = -177}, Node{.left = 200, .right = 206},
    Node{.left = 201, .right = 205},   Node{.left = -178, .right = -181},
    Node{.left = -179, .right = -209}, Node{.left = -182, .right = -183},
    Node{.left = -184, .right = -194}, Node{.left = -185, .right = -186},
    Node{.left = 207, .right = 210},   Node{.left = -187, .right = -189},
    Node{.left = 209, .right = 215},   Node{.left = -188, .right = -191},
    Node{.left = -190, .right = -196}, Node{.left = 212, .right = 224},
    Node{.left = 213, .right = 222},   Node{.left = 214, .right = 221},
    Node{.left = -192, .right = -193}, Node{.left = -197, .right = -231},
    Node{.left = 217, .right = 243},   Node{.left = -198, .right = -228},
    Node{.left = 245, .right = 219},   Node{.left = 220, .right = 244},
    Node{.left = -199, .right = -207}, Node{.left = -200, .right = -201},
    Node{.left = 223, .right = 228},   Node{.left = -202, .right = -205},
    Node{.left = 237, .right = 225},   Node{.left = 248, .right = 226},
    Node{.left = -255, .right = 227},  Node{.left = -203, .right = -204},
    Node{.left = -210, .right = -213}, Node{.left = 230, .right = 249},
    Node{.left = 231, .right = 239},   Node{.left = 232, .right = 233},
    Node{.left = -211, .right = -212}, Node{.left = -214, .right = -221},
    Node{.left = -215, .right = -225}, Node{.left = 236, .right = 242},
    Node{.left = -216, .right = -217}, Node{.left = 238, .right = 246},
    Node{.left = -218, .right = -219}, Node{.left = 240, .right = 247},
    Node{.left = -222, .right = -223}, Node{.left = -224, .right = -226},
    Node{.left = -227, .right = -229}, Node{.left = -232, .right = -233},
    Node{.left = -234, .right = -235}, Node{.left = -236, .right = -237},
    Node{.left = -238, .right = -240}, Node{.left = -241, .right = -244},
    Node{.left = -242, .right = -243}, Node{.left = 250, .right = 253},
    Node{.left = 251, .right = 252},   Node{.left = -245, .right = -246},
    Node{.left = -247, .right = -248}, Node{.left = 254, .right = 255},
    Node{.left = -250, .right = -251}, Node{.left = -252, .right = -253},
};

// The encodings of each character.
//
// NOTE: the HPACK specs describe encodings for 257 symbols due to the extra EOS symbol. Since the
// encoding for EOS is all 1's, we simply pad our final output with 1's rather than running the
// algorithm to encode EOS. For this reason we don't need to know the encoding for EOS and we only
// keep 256 elements in this array.
std::array<Encoding, kNumLeaves - 1> constexpr codes{
    Encoding{.length = 13, .bits = 0x1FF8},     Encoding{.length = 23, .bits = 0x7FFFD8},
    Encoding{.length = 28, .bits = 0xFFFFFE2},  Encoding{.length = 28, .bits = 0xFFFFFE3},
    Encoding{.length = 28, .bits = 0xFFFFFE4},  Encoding{.length = 28, .bits = 0xFFFFFE5},
    Encoding{.length = 28, .bits = 0xFFFFFE6},  Encoding{.length = 28, .bits = 0xFFFFFE7},
    Encoding{.length = 28, .bits = 0xFFFFFE8},  Encoding{.length = 24, .bits = 0xFFFFEA},
    Encoding{.length = 30, .bits = 0x3FFFFFFC}, Encoding{.length = 28, .bits = 0xFFFFFE9},
    Encoding{.length = 28, .bits = 0xFFFFFEA},  Encoding{.length = 30, .bits = 0x3FFFFFFD},
    Encoding{.length = 28, .bits = 0xFFFFFEB},  Encoding{.length = 28, .bits = 0xFFFFFEC},
    Encoding{.length = 28, .bits = 0xFFFFFED},  Encoding{.length = 28, .bits = 0xFFFFFEE},
    Encoding{.length = 28, .bits = 0xFFFFFEF},  Encoding{.length = 28, .bits = 0xFFFFFF0},
    Encoding{.length = 28, .bits = 0xFFFFFF1},  Encoding{.length = 28, .bits = 0xFFFFFF2},
    Encoding{.length = 30, .bits = 0x3FFFFFFE}, Encoding{.length = 28, .bits = 0xFFFFFF3},
    Encoding{.length = 28, .bits = 0xFFFFFF4},  Encoding{.length = 28, .bits = 0xFFFFFF5},
    Encoding{.length = 28, .bits = 0xFFFFFF6},  Encoding{.length = 28, .bits = 0xFFFFFF7},
    Encoding{.length = 28, .bits = 0xFFFFFF8},  Encoding{.length = 28, .bits = 0xFFFFFF9},
    Encoding{.length = 28, .bits = 0xFFFFFFA},  Encoding{.length = 28, .bits = 0xFFFFFFB},
    Encoding{.length = 6, .bits = 0x14},        Encoding{.length = 10, .bits = 0x3F8},
    Encoding{.length = 10, .bits = 0x3F9},      Encoding{.length = 12, .bits = 0xFFA},
    Encoding{.length = 13, .bits = 0x1FF9},     Encoding{.length = 6, .bits = 0x15},
    Encoding{.length = 8, .bits = 0xF8},        Encoding{.length = 11, .bits = 0x7FA},
    Encoding{.length = 10, .bits = 0x3FA},      Encoding{.length = 10, .bits = 0x3FB},
    Encoding{.length = 8, .bits = 0xF9},        Encoding{.length = 11, .bits = 0x7FB},
    Encoding{.length = 8, .bits = 0xFA},        Encoding{.length = 6, .bits = 0x16},
    Encoding{.length = 6, .bits = 0x17},        Encoding{.length = 6, .bits = 0x18},
    Encoding{.length = 5, .bits = 0x0},         Encoding{.length = 5, .bits = 0x1},
    Encoding{.length = 5, .bits = 0x2},         Encoding{.length = 6, .bits = 0x19},
    Encoding{.length = 6, .bits = 0x1A},        Encoding{.length = 6, .bits = 0x1B},
    Encoding{.length = 6, .bits = 0x1C},        Encoding{.length = 6, .bits = 0x1D},
    Encoding{.length = 6, .bits = 0x1E},        Encoding{.length = 6, .bits = 0x1F},
    Encoding{.length = 7, .bits = 0x5C},        Encoding{.length = 8, .bits = 0xFB},
    Encoding{.length = 15, .bits = 0x7FFC},     Encoding{.length = 6, .bits = 0x20},
    Encoding{.length = 12, .bits = 0xFFB},      Encoding{.length = 10, .bits = 0x3FC},
    Encoding{.length = 13, .bits = 0x1FFA},     Encoding{.length = 6, .bits = 0x21},
    Encoding{.length = 7, .bits = 0x5D},        Encoding{.length = 7, .bits = 0x5E},
    Encoding{.length = 7, .bits = 0x5F},        Encoding{.length = 7, .bits = 0x60},
    Encoding{.length = 7, .bits = 0x61},        Encoding{.length = 7, .bits = 0x62},
    Encoding{.length = 7, .bits = 0x63},        Encoding{.length = 7, .bits = 0x64},
    Encoding{.length = 7, .bits = 0x65},        Encoding{.length = 7, .bits = 0x66},
    Encoding{.length = 7, .bits = 0x67},        Encoding{.length = 7, .bits = 0x68},
    Encoding{.length = 7, .bits = 0x69},        Encoding{.length = 7, .bits = 0x6A},
    Encoding{.length = 7, .bits = 0x6B},        Encoding{.length = 7, .bits = 0x6C},
    Encoding{.length = 7, .bits = 0x6D},        Encoding{.length = 7, .bits = 0x6E},
    Encoding{.length = 7, .bits = 0x6F},        Encoding{.length = 7, .bits = 0x70},
    Encoding{.length = 7, .bits = 0x71},        Encoding{.length = 7, .bits = 0x72},
    Encoding{.length = 8, .bits = 0xFC},        Encoding{.length = 7, .bits = 0x73},
    Encoding{.length = 8, .bits = 0xFD},        Encoding{.length = 13, .bits = 0x1FFB},
    Encoding{.length = 19, .bits = 0x7FFF0},    Encoding{.length = 13, .bits = 0x1FFC},
    Encoding{.length = 14, .bits = 0x3FFC},     Encoding{.length = 6, .bits = 0x22},
    Encoding{.length = 15, .bits = 0x7FFD},     Encoding{.length = 5, .bits = 0x3},
    Encoding{.length = 6, .bits = 0x23},        Encoding{.length = 5, .bits = 0x4},
    Encoding{.length = 6, .bits = 0x24},        Encoding{.length = 5, .bits = 0x5},
    Encoding{.length = 6, .bits = 0x25},        Encoding{.length = 6, .bits = 0x26},
    Encoding{.length = 6, .bits = 0x27},        Encoding{.length = 5, .bits = 0x6},
    Encoding{.length = 7, .bits = 0x74},        Encoding{.length = 7, .bits = 0x75},
    Encoding{.length = 6, .bits = 0x28},        Encoding{.length = 6, .bits = 0x29},
    Encoding{.length = 6, .bits = 0x2A},        Encoding{.length = 5, .bits = 0x7},
    Encoding{.length = 6, .bits = 0x2B},        Encoding{.length = 7, .bits = 0x76},
    Encoding{.length = 6, .bits = 0x2C},        Encoding{.length = 5, .bits = 0x8},
    Encoding{.length = 5, .bits = 0x9},         Encoding{.length = 6, .bits = 0x2D},
    Encoding{.length = 7, .bits = 0x77},        Encoding{.length = 7, .bits = 0x78},
    Encoding{.length = 7, .bits = 0x79},        Encoding{.length = 7, .bits = 0x7A},
    Encoding{.length = 7, .bits = 0x7B},        Encoding{.length = 15, .bits = 0x7FFE},
    Encoding{.length = 11, .bits = 0x7FC},      Encoding{.length = 14, .bits = 0x3FFD},
    Encoding{.length = 13, .bits = 0x1FFD},     Encoding{.length = 28, .bits = 0xFFFFFFC},
    Encoding{.length = 20, .bits = 0xFFFE6},    Encoding{.length = 22, .bits = 0x3FFFD2},
    Encoding{.length = 20, .bits = 0xFFFE7},    Encoding{.length = 20, .bits = 0xFFFE8},
    Encoding{.length = 22, .bits = 0x3FFFD3},   Encoding{.length = 22, .bits = 0x3FFFD4},
    Encoding{.length = 22, .bits = 0x3FFFD5},   Encoding{.length = 23, .bits = 0x7FFFD9},
    Encoding{.length = 22, .bits = 0x3FFFD6},   Encoding{.length = 23, .bits = 0x7FFFDA},
    Encoding{.length = 23, .bits = 0x7FFFDB},   Encoding{.length = 23, .bits = 0x7FFFDC},
    Encoding{.length = 23, .bits = 0x7FFFDD},   Encoding{.length = 23, .bits = 0x7FFFDE},
    Encoding{.length = 24, .bits = 0xFFFFEB},   Encoding{.length = 23, .bits = 0x7FFFDF},
    Encoding{.length = 24, .bits = 0xFFFFEC},   Encoding{.length = 24, .bits = 0xFFFFED},
    Encoding{.length = 22, .bits = 0x3FFFD7},   Encoding{.length = 23, .bits = 0x7FFFE0},
    Encoding{.length = 24, .bits = 0xFFFFEE},   Encoding{.length = 23, .bits = 0x7FFFE1},
    Encoding{.length = 23, .bits = 0x7FFFE2},   Encoding{.length = 23, .bits = 0x7FFFE3},
    Encoding{.length = 23, .bits = 0x7FFFE4},   Encoding{.length = 21, .bits = 0x1FFFDC},
    Encoding{.length = 22, .bits = 0x3FFFD8},   Encoding{.length = 23, .bits = 0x7FFFE5},
    Encoding{.length = 22, .bits = 0x3FFFD9},   Encoding{.length = 23, .bits = 0x7FFFE6},
    Encoding{.length = 23, .bits = 0x7FFFE7},   Encoding{.length = 24, .bits = 0xFFFFEF},
    Encoding{.length = 22, .bits = 0x3FFFDA},   Encoding{.length = 21, .bits = 0x1FFFDD},
    Encoding{.length = 20, .bits = 0xFFFE9},    Encoding{.length = 22, .bits = 0x3FFFDB},
    Encoding{.length = 22, .bits = 0x3FFFDC},   Encoding{.length = 23, .bits = 0x7FFFE8},
    Encoding{.length = 23, .bits = 0x7FFFE9},   Encoding{.length = 21, .bits = 0x1FFFDE},
    Encoding{.length = 23, .bits = 0x7FFFEA},   Encoding{.length = 22, .bits = 0x3FFFDD},
    Encoding{.length = 22, .bits = 0x3FFFDE},   Encoding{.length = 24, .bits = 0xFFFFF0},
    Encoding{.length = 21, .bits = 0x1FFFDF},   Encoding{.length = 22, .bits = 0x3FFFDF},
    Encoding{.length = 23, .bits = 0x7FFFEB},   Encoding{.length = 23, .bits = 0x7FFFEC},
    Encoding{.length = 21, .bits = 0x1FFFE0},   Encoding{.length = 21, .bits = 0x1FFFE1},
    Encoding{.length = 22, .bits = 0x3FFFE0},   Encoding{.length = 21, .bits = 0x1FFFE2},
    Encoding{.length = 23, .bits = 0x7FFFED},   Encoding{.length = 22, .bits = 0x3FFFE1},
    Encoding{.length = 23, .bits = 0x7FFFEE},   Encoding{.length = 23, .bits = 0x7FFFEF},
    Encoding{.length = 20, .bits = 0xFFFEA},    Encoding{.length = 22, .bits = 0x3FFFE2},
    Encoding{.length = 22, .bits = 0x3FFFE3},   Encoding{.length = 22, .bits = 0x3FFFE4},
    Encoding{.length = 23, .bits = 0x7FFFF0},   Encoding{.length = 22, .bits = 0x3FFFE5},
    Encoding{.length = 22, .bits = 0x3FFFE6},   Encoding{.length = 23, .bits = 0x7FFFF1},
    Encoding{.length = 26, .bits = 0x3FFFFE0},  Encoding{.length = 26, .bits = 0x3FFFFE1},
    Encoding{.length = 20, .bits = 0xFFFEB},    Encoding{.length = 19, .bits = 0x7FFF1},
    Encoding{.length = 22, .bits = 0x3FFFE7},   Encoding{.length = 23, .bits = 0x7FFFF2},
    Encoding{.length = 22, .bits = 0x3FFFE8},   Encoding{.length = 25, .bits = 0x1FFFFEC},
    Encoding{.length = 26, .bits = 0x3FFFFE2},  Encoding{.length = 26, .bits = 0x3FFFFE3},
    Encoding{.length = 26, .bits = 0x3FFFFE4},  Encoding{.length = 27, .bits = 0x7FFFFDE},
    Encoding{.length = 27, .bits = 0x7FFFFDF},  Encoding{.length = 26, .bits = 0x3FFFFE5},
    Encoding{.length = 24, .bits = 0xFFFFF1},   Encoding{.length = 25, .bits = 0x1FFFFED},
    Encoding{.length = 19, .bits = 0x7FFF2},    Encoding{.length = 21, .bits = 0x1FFFE3},
    Encoding{.length = 26, .bits = 0x3FFFFE6},  Encoding{.length = 27, .bits = 0x7FFFFE0},
    Encoding{.length = 27, .bits = 0x7FFFFE1},  Encoding{.length = 26, .bits = 0x3FFFFE7},
    Encoding{.length = 27, .bits = 0x7FFFFE2},  Encoding{.length = 24, .bits = 0xFFFFF2},
    Encoding{.length = 21, .bits = 0x1FFFE4},   Encoding{.length = 21, .bits = 0x1FFFE5},
    Encoding{.length = 26, .bits = 0x3FFFFE8},  Encoding{.length = 26, .bits = 0x3FFFFE9},
    Encoding{.length = 28, .bits = 0xFFFFFFD},  Encoding{.length = 27, .bits = 0x7FFFFE3},
    Encoding{.length = 27, .bits = 0x7FFFFE4},  Encoding{.length = 27, .bits = 0x7FFFFE5},
    Encoding{.length = 20, .bits = 0xFFFEC},    Encoding{.length = 24, .bits = 0xFFFFF3},
    Encoding{.length = 20, .bits = 0xFFFED},    Encoding{.length = 21, .bits = 0x1FFFE6},
    Encoding{.length = 22, .bits = 0x3FFFE9},   Encoding{.length = 21, .bits = 0x1FFFE7},
    Encoding{.length = 21, .bits = 0x1FFFE8},   Encoding{.length = 23, .bits = 0x7FFFF3},
    Encoding{.length = 22, .bits = 0x3FFFEA},   Encoding{.length = 22, .bits = 0x3FFFEB},
    Encoding{.length = 25, .bits = 0x1FFFFEE},  Encoding{.length = 25, .bits = 0x1FFFFEF},
    Encoding{.length = 24, .bits = 0xFFFFF4},   Encoding{.length = 24, .bits = 0xFFFFF5},
    Encoding{.length = 26, .bits = 0x3FFFFEA},  Encoding{.length = 23, .bits = 0x7FFFF4},
    Encoding{.length = 26, .bits = 0x3FFFFEB},  Encoding{.length = 27, .bits = 0x7FFFFE6},
    Encoding{.length = 26, .bits = 0x3FFFFEC},  Encoding{.length = 26, .bits = 0x3FFFFED},
    Encoding{.length = 27, .bits = 0x7FFFFE7},  Encoding{.length = 27, .bits = 0x7FFFFE8},
    Encoding{.length = 27, .bits = 0x7FFFFE9},  Encoding{.length = 27, .bits = 0x7FFFFEA},
    Encoding{.length = 27, .bits = 0x7FFFFEB},  Encoding{.length = 28, .bits = 0xFFFFFFE},
    Encoding{.length = 27, .bits = 0x7FFFFEC},  Encoding{.length = 27, .bits = 0x7FFFFED},
    Encoding{.length = 27, .bits = 0x7FFFFEE},  Encoding{.length = 27, .bits = 0x7FFFFEF},
    Encoding{.length = 27, .bits = 0x7FFFFF0},  Encoding{.length = 26, .bits = 0x3FFFFEE},
};

}  // namespace

std::string HuffmanCode::Decode(absl::Span<uint8_t const> const data) {
  std::string result;
  int16_t node = 0;
  for (auto const byte : data) {
    for (int j = 0; j < 8; ++j) {
      if (((byte >> (7 - j)) & 1) != 0) {
        node = nodes[node].right;
      } else {
        node = nodes[node].left;
      }
      if (node < 1) {
        int16_t const label = -node;
        if (label < 256) {
          result += static_cast<char>(label);
          node = 0;
        } else {
          return result;
        }
      }
    }
  }
  return result;
}

size_t HuffmanCode::GetEncodedLength(std::string_view const text) {
  size_t num_bits = 0;
  for (char const ch : text) {
    num_bits += codes[ch].length;
  }
  return (num_bits + 7) >> 3;
}

Buffer HuffmanCode::Encode(std::string_view const text) {
  std::vector<uint8_t> bytes;
  bytes.reserve(text.size());
  size_t num_bits = 0;
  for (uint8_t const ch : text) {
    auto const& encoding = codes[ch];
    size_t const length = encoding.length;
    size_t const tail = num_bits & 7;
    uint64_t const bits = static_cast<uint64_t>(encoding.bits) << (40 - length - tail);
    if (tail != 0) {
      bytes.back() |= static_cast<uint8_t>(bits >> 32);
    } else {
      bytes.push_back(bits >> 32);
    }
    if (length > 8 - tail) {
      bytes.push_back((bits >> 24) & 0xFF);
    }
    if (length > 16 - tail) {
      bytes.push_back((bits >> 16) & 0xFF);
    }
    if (length > 24 - tail) {
      bytes.push_back((bits >> 8) & 0xFF);
    }
    if (length > 32 - tail) {
      bytes.push_back(bits & 0xFF);
    }
    num_bits += length;
  }
  size_t const tail = num_bits & 7;
  if (tail != 0) {
    bytes.back() |= (uint8_t{1} << (8 - tail)) - 1;
  }
  Buffer buffer{bytes.size()};
  buffer.MemCpy(bytes.data(), bytes.size());
  return buffer;
}

}  // namespace hpack
}  // namespace http
}  // namespace tsdb2
