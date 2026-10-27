#pragma once

#include "models.hpp"

#include <map>
#include <random>
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

struct RejectedTransaction {
  Transaction transaction;
  std::string reason;
};

struct Candidate {
  Block block;
  std::vector<RejectedTransaction> rejected;
};

struct StepResult {
  bool block_accepted = false;
  std::size_t removed = 0;
  MiningStats mining{};
  std::string error;
};

class Chain {
 public:
  Chain(const std::vector<User>& users, std::vector<Transaction> pending, int difficulty);

  Candidate build_candidate(std::mt19937_64& random) const;
  std::string validate_block(const Block& block) const;
  std::string accept_block(const Block& block);
  StepResult process_next(std::mt19937_64& random);

  const Balances& initial_balances() const { return initial_balances_; }
  const Balances& balances() const { return balances_; }
  const std::vector<Block>& blocks() const { return blocks_; }
  const std::vector<Transaction>& pending() const { return pending_; }
  const std::vector<RejectedTransaction>& rejected() const { return rejected_; }
  std::size_t accepted_transactions() const { return accepted_ids_.size(); }

 private:
  Balances initial_balances_;
  Balances balances_;
  std::vector<Block> blocks_;
  std::vector<Transaction> pending_;
  std::vector<RejectedTransaction> rejected_;
  std::set<std::string> accepted_ids_;
  std::set<std::uint64_t> accepted_sequences_;
  int difficulty_;
};

}  // namespace blockchain
