// 256-bit hash over raw bytes. Text encoding and file reading are the caller's job.

#ifndef CUSTOM_HASH_HPP
#define CUSTOM_HASH_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace dihash {

using Digest256 = std::array<std::uint8_t, 32>;

Digest256 custom_hash(std::span<const std::uint8_t> input);

// Same hash for input that arrives in pieces (e.g. a file read in chunks):
// any split of the bytes gives the same digest as custom_hash.
class Hasher {
 public:
  Hasher();

  void update(std::span<const std::uint8_t> bytes);

  Digest256 finish() const;  // does not change the hasher, more input may follow

 private:
  std::array<std::uint64_t, 8> state_;
  std::array<std::uint8_t, 32> pending_{};  // bytes of a block not yet full
  std::size_t pending_size_ = 0;
  std::uint64_t blocks_ = 0;
};

// 64 lowercase hex digits, leading zeros kept.
std::string to_hex(const Digest256& digest);

}  // namespace dihash

#endif  // CUSTOM_HASH_HPP
