#pragma once

#include "models.hpp"

#include <map>
#include <set>

namespace blockchain {

using Balances = std::map<std::string, Amount>;

struct MiningStats {
  std::uint64_t attempts;
  double duration_seconds;
};

bool satisfies_difficulty(const std::string& hash, int difficulty);
MiningStats mine_block(Block& block);

// The caller supplies temporary state until a whole block has passed validation.
std::string execute_transaction(const Transaction& transaction, Balances& balances,
                                std::set<std::string>& accepted_ids,
                                std::set<std::uint64_t>& accepted_sequences);

}  // namespace blockchain
