#pragma once

#include "custom_hash.hpp"

#include <string>
#include <string_view>

namespace blockchain {

constexpr std::size_t hash_length = 64;

inline std::string hash_text(std::string_view text) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
  return dihash::to_hex(dihash::custom_hash({bytes, text.size()}));
}

}  // namespace blockchain
