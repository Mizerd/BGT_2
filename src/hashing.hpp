#pragma once

#include "custom_hash.hpp"

#include <string>
#include <string_view>
#include <stdexcept>

namespace blockchain {

constexpr std::size_t hash_length = 64;

inline bool is_hash(std::string_view text) {
  return text.size() == hash_length &&
         text.find_first_not_of("0123456789abcdef") == std::string_view::npos;
}

inline dihash::Digest256 decode_hash(std::string_view text) {
  if (!is_hash(text)) {
    throw std::invalid_argument("hash must contain 64 lowercase hex digits");
  }
  const std::string_view digits = "0123456789abcdef";
  dihash::Digest256 bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<std::uint8_t>(digits.find(text[2 * index]) * 16 +
                                            digits.find(text[2 * index + 1]));
  }
  return bytes;
}

inline std::string hash_text(std::string_view text) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
  return dihash::to_hex(dihash::custom_hash({bytes, text.size()}));
}

}  // namespace blockchain
