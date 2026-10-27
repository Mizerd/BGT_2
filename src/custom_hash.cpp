#include "custom_hash.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>

namespace dihash {
namespace {

// Constants: decimal digits of n^n (n = 2, 3, ...) cut into 20-digit chunks,
// each taken mod 2^64 with the low bit set; only chunks with 26..38 one bits
// are kept. The rotations come from the next chunks, mapped to 9..55.
constexpr std::array<std::uint64_t, 8> kLaneInit = {
    0x50EFEF418AACDDB3ull, 0x06837D22C9EC9F1Bull, 0x93ED2CF209AF0CA9ull,
    0x1511760347A610CBull, 0xE0613E1645D94F07ull, 0x7AA3E75445026145ull,
    0x0976B9398CFCD1FFull, 0x2B84EC77CA013781ull,
};

// Odd, so multiplying by them can't lose information.
constexpr std::uint64_t kMulForward = 0x2AD26360F20D2A3Dull;
constexpr std::uint64_t kMulBackward = 0x65D3D764F8ED45F5ull;

constexpr std::uint64_t kTagStep = 0xC5E512181F592E6Bull;  // times the block number
constexpr std::uint64_t kTagTail = 0x9C019A9E6A275255ull;  // times (leftover bytes + 1)
constexpr std::uint64_t kTagEnd = 0x0C2605DFE6C8B025ull;   // length and drain steps

constexpr int kRotForward = 41;
constexpr int kRotBackward = 53;
constexpr int kRotFold = 40;

constexpr std::size_t kLanes = 8;  // 8 x 64 = 512-bit state
constexpr std::size_t kBlockBytes = 32;
constexpr int kRounds = 2;  // with one, block words can be solved to reach any state (tests/attack_v012.py)
constexpr int kDrainSteps = 3;

using State = std::array<std::uint64_t, kLanes>;

// Big-endian by hand, so the digest does not depend on the machine.
std::uint64_t load_be64(const std::uint8_t* bytes) {
  std::uint64_t word = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    word = (word << 8) | static_cast<std::uint64_t>(bytes[i]);
  }
  return word;
}

void store_be64(std::uint64_t word, std::uint8_t* bytes) {
  for (std::size_t i = 0; i < 8; ++i) {
    bytes[i] = static_cast<std::uint8_t>(word >> (56 - 8 * i));
  }
}

// One round. The upward sweep carries every change to lane 7 and the downward
// sweep brings it back to lane 0, so afterwards each lane depends on all others.
// Multiplication only moves bits upwards; the rotations bring them back down.
void mix(State& s) {
  for (std::size_t i = 1; i < kLanes; ++i) {
    s[i] = (s[i] ^ std::rotl(s[i - 1], kRotForward)) * kMulForward;
  }
  for (std::size_t i = kLanes - 1; i-- > 0;) {
    s[i] = (s[i] + std::rotl(s[i + 1], kRotBackward)) * kMulBackward;
  }
}

// Absorbs one block: the words go into every other lane (+ and ^ alternate),
// then two rounds, then the old state is XORed back in (feed-forward), so the
// step can no longer be undone round by round from its output.
void step(State& s, std::uint64_t word0, std::uint64_t word1, std::uint64_t word2,
          std::uint64_t word3, std::uint64_t tag) {
  const State before = s;
  s[0] += word0 ^ tag;
  s[2] ^= word1;
  s[4] += word2;
  s[6] ^= word3;
  for (int round = 0; round < kRounds; ++round) {
    mix(s);
  }
  for (std::size_t i = 0; i < kLanes; ++i) {
    s[i] ^= before[i];
  }
}

// `first` is the 1-based number of the first block. Tagging blocks by position
// makes the same block count differently at different offsets.
void absorb_blocks(State& state, const std::uint8_t* data, std::size_t count, std::uint64_t first) {
  State s = state;  // local copy stays in registers; the input bytes may alias `state`
  for (std::size_t i = 0; i < count; ++i) {
    const std::uint8_t* block = data + i * kBlockBytes;
    step(s, load_be64(block), load_be64(block + 8), load_be64(block + 16),
         load_be64(block + 24), (first + i) * kTagStep);
  }
  state = s;
}

// Tail, length, drain steps and the 512 -> 256 bit fold.
// inline: short inputs would otherwise pay for a call and a state copy.
inline Digest256 finalize(State state, const std::uint8_t* rest, std::size_t rest_size, std::uint64_t blocks) {
  const std::uint64_t length = blocks * kBlockBytes + rest_size;

  // Runs even with 0 leftover bytes. There is no padding byte; the leftover
  // count goes into the tag instead, which is what keeps "ab" and "ab\0" apart.
  std::uint8_t tail[kBlockBytes] = {};
  std::copy_n(rest, rest_size, tail);
  const std::uint64_t tail_tag = (blocks + 1) * kTagStep +
                                 (static_cast<std::uint64_t>(rest_size) + 1) * kTagTail;
  step(state, load_be64(tail), load_be64(tail + 8), load_be64(tail + 16),
       load_be64(tail + 24), tail_tag);

  step(state, length, std::rotl(length, 32), 0, 0, kTagEnd);

  // Extra steps without input, so the last block is mixed as well as the first.
  for (int i = 1; i <= kDrainSteps; ++i) {
    step(state, 0, 0, 0, 0, kTagEnd + static_cast<std::uint64_t>(i) * kTagStep);
  }

  // Every output word combines two lanes, so no lane is ever output directly.
  Digest256 digest{};
  for (std::size_t i = 0; i < 4; ++i) {
    const std::uint64_t folded = state[i] ^ std::rotl(state[i + 4], kRotFold);
    store_be64(folded, digest.data() + i * 8);
  }
  return digest;
}

}  // namespace

Digest256 custom_hash(std::span<const std::uint8_t> input) {
  State state = kLaneInit;
  const std::size_t whole_blocks = input.size() / kBlockBytes;
  absorb_blocks(state, input.data(), whole_blocks, 1);
  return finalize(state, input.data() + whole_blocks * kBlockBytes, input.size() % kBlockBytes, whole_blocks);
}

Hasher::Hasher() : state_(kLaneInit) {}

void Hasher::update(std::span<const std::uint8_t> bytes) {
  if (pending_size_ > 0) {  // top up the block left over from the previous call
    const std::size_t take = std::min(kBlockBytes - pending_size_, bytes.size());
    std::copy_n(bytes.begin(), take, pending_.begin() + pending_size_);
    pending_size_ += take;
    bytes = bytes.subspan(take);
    if (pending_size_ < kBlockBytes) {
      return;
    }
    absorb_blocks(state_, pending_.data(), 1, ++blocks_);
    pending_size_ = 0;
  }
  const std::size_t count = bytes.size() / kBlockBytes;
  absorb_blocks(state_, bytes.data(), count, blocks_ + 1);
  blocks_ += count;
  const std::span<const std::uint8_t> rest = bytes.subspan(count * kBlockBytes);
  std::copy(rest.begin(), rest.end(), pending_.begin());
  pending_size_ = rest.size();
}

Digest256 Hasher::finish() const { return finalize(state_, pending_.data(), pending_size_, blocks_); }

std::string to_hex(const Digest256& digest) {
  static constexpr char kHexDigits[] = "0123456789abcdef";
  std::string text;
  text.reserve(digest.size() * 2);
  for (const std::uint8_t byte : digest) {
    text.push_back(kHexDigits[byte >> 4]);
    text.push_back(kHexDigits[byte & 0x0F]);
  }
  return text;
}

}  // namespace dihash
