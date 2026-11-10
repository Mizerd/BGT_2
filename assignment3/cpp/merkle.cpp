#include "merkle.hpp"

#include <bitcoin/bitcoin.hpp>
#include <algorithm>
#include <stdexcept>

namespace bitcoin_lab {

std::string merkle_root(const std::vector<std::string>& txids) {
  if (txids.empty()) {
    throw std::invalid_argument("Bitcoin Merkle tree requires at least one txid");
  }
  std::vector<bc::data_chunk> level;
  for (const auto& txid : txids) {
    bc::data_chunk bytes;
    if (txid.size() != 64 || !bc::decode_base16(bytes, txid) || bytes.size() != 32) {
      throw std::invalid_argument("txid must encode exactly 32 bytes as hex");
    }
    std::reverse(bytes.begin(), bytes.end());
    level.push_back(bytes);
  }
  while (level.size() > 1) {
    if (level.size() % 2 != 0) {
      level.push_back(level.back());
    }
    std::vector<bc::data_chunk> next;
    for (std::size_t index = 0; index < level.size(); index += 2) {
      auto pair = level[index];
      pair.insert(pair.end(), level[index + 1].begin(), level[index + 1].end());
      if (pair.size() != 64) {
        throw std::logic_error("Merkle parent input must be 64 bytes");
      }
      const auto hash = bc::bitcoin_hash(pair);
      next.emplace_back(hash.begin(), hash.end());
    }
    level = std::move(next);
  }
  std::reverse(level.front().begin(), level.front().end());
  return bc::encode_base16(level.front());
}

}
