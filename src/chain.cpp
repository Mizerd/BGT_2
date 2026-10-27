#include "chain.hpp"
#include "hashing.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace blockchain {

bool satisfies_difficulty(const std::string& hash, int difficulty) {
  return difficulty >= 0 && difficulty <= static_cast<int>(hash_length) &&
         hash.size() == hash_length &&
         hash.find_first_not_of("0123456789abcdef") == std::string::npos &&
         hash.starts_with(std::string(static_cast<std::size_t>(difficulty), '0'));
}

MiningStats mine_block(Block& block) {
  if (block.header.difficulty < 0 || block.header.difficulty > static_cast<int>(hash_length)) {
    throw std::invalid_argument("difficulty must be between 0 and 64");
  }
  const auto start = std::chrono::steady_clock::now();
  block.header.nonce = 0;
  std::uint64_t attempts = 0;
  while (attempts < std::numeric_limits<std::uint64_t>::max()) {
    ++attempts;
    block.block_hash = calculate_block_hash(block.header);
    if (satisfies_difficulty(block.block_hash, block.header.difficulty)) {
      const double duration = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      return {attempts, duration};
    }
    ++block.header.nonce;
  }
  throw std::runtime_error("nonce search exhausted");
}

std::string execute_transaction(const Transaction& transaction, Balances& balances,
                                std::set<std::string>& accepted_ids,
                                std::set<std::uint64_t>& accepted_sequences) {
  const auto sender = balances.find(transaction.sender);
  const auto receiver = balances.find(transaction.receiver);
  if (sender == balances.end()) {
    return "unknown sender";
  }
  if (receiver == balances.end()) {
    return "unknown receiver";
  }
  if (transaction.sender == transaction.receiver) {
    return "sender equals receiver";
  }
  if (transaction.sender.find('|') != std::string::npos ||
      transaction.receiver.find('|') != std::string::npos) {
    return "identifier contains delimiter";
  }
  if (transaction.amount <= 0) {
    return "amount must be a positive integer";
  }
  if (transaction.transaction_id != hash_text(serialize_transaction(transaction))) {
    return "incorrect transaction ID";
  }
  if (accepted_ids.contains(transaction.transaction_id)) {
    return "duplicate transaction";
  }
  if (accepted_sequences.contains(transaction.sequence)) {
    return "duplicate sequence";
  }
  if (sender->second < transaction.amount) {
    return "insufficient funds";
  }
  if (receiver->second > std::numeric_limits<Amount>::max() - transaction.amount) {
    return "receiver balance overflow";
  }
  sender->second -= transaction.amount;
  receiver->second += transaction.amount;
  accepted_ids.insert(transaction.transaction_id);
  accepted_sequences.insert(transaction.sequence);
  return "";
}

namespace {

std::string validate_block_state(const Block& block, const Block& previous, int difficulty,
                                 Balances& balances, std::set<std::string>& ids,
                                 std::set<std::uint64_t>& sequences) {
  if (block.header.prev_hash != previous.block_hash) {
    return "incorrect previous hash";
  }
  if (block.header.version != "0.1") {
    return "unsupported block version";
  }
  if (block.header.timestamp <= previous.header.timestamp) {
    return "timestamp must increase";
  }
  if (block.header.difficulty != difficulty) {
    return "incorrect difficulty";
  }
  if (block.transactions.empty() || block.transactions.size() > 100) {
    return "block must contain 1 to 100 transactions";
  }
  if (block.header.tx_root != transaction_root(block.transactions)) {
    return "incorrect transaction root";
  }
  if (block.block_hash != calculate_block_hash(block.header)) {
    return "incorrect block hash";
  }
  if (!satisfies_difficulty(block.block_hash, difficulty)) {
    return "proof of work does not meet difficulty";
  }
  for (const auto& transaction : block.transactions) {
    const auto reason = execute_transaction(transaction, balances, ids, sequences);
    if (!reason.empty()) {
      return "transaction " + std::to_string(transaction.sequence) + ": " + reason;
    }
  }
  return "";
}

}  // namespace

Chain::Chain(const std::vector<User>& users, std::vector<Transaction> pending, int difficulty)
    : pending_(std::move(pending)), difficulty_(difficulty) {
  if (difficulty < 0 || difficulty > static_cast<int>(hash_length)) {
    throw std::invalid_argument("difficulty must be between 0 and 64");
  }
  Amount total = 0;
  for (const auto& user : users) {
    if (user.public_key.empty() || user.public_key.find('|') != std::string::npos) {
      throw std::invalid_argument("user key must be nonempty and cannot contain '|'");
    }
    if (user.balance < 0 || user.balance > std::numeric_limits<Amount>::max() - total) {
      throw std::invalid_argument("invalid initial balance or total balance overflow");
    }
    if (!initial_balances_.emplace(user.public_key, user.balance).second) {
      throw std::invalid_argument("duplicate user key");
    }
    total += user.balance;
  }
  balances_ = initial_balances_;
  blocks_.push_back(make_genesis_block());
}

Candidate Chain::build_candidate(std::mt19937_64& random) const {
  auto balances = balances_;
  auto ids = accepted_ids_;
  auto sequences = accepted_sequences_;
  std::vector<std::size_t> selected(pending_.size());
  std::iota(selected.begin(), selected.end(), 0);
  std::shuffle(selected.begin(), selected.end(), random);
  selected.resize(std::min<std::size_t>(100, selected.size()));

  Candidate candidate;
  for (const auto index : selected) {
    const auto& transaction = pending_[index];
    const auto reason = execute_transaction(transaction, balances, ids, sequences);
    if (reason.empty()) {
      candidate.block.transactions.push_back(transaction);
    } else {
      candidate.rejected.push_back({transaction, reason});
    }
  }
  candidate.block.header = {blocks_.back().block_hash,
                            genesis_timestamp + static_cast<std::int64_t>(blocks_.size()),
                            "0.1", transaction_root(candidate.block.transactions), 0, difficulty_};
  return candidate;
}

std::string Chain::validate_block(const Block& block) const {
  auto balances = balances_;
  auto ids = accepted_ids_;
  auto sequences = accepted_sequences_;
  return validate_block_state(block, blocks_.back(), difficulty_, balances, ids, sequences);
}

std::string Chain::accept_block(const Block& block) {
  auto balances = balances_;
  auto ids = accepted_ids_;
  auto sequences = accepted_sequences_;
  const auto reason = validate_block_state(block, blocks_.back(), difficulty_, balances, ids, sequences);
  if (!reason.empty()) {
    return reason;
  }
  auto pending = pending_;
  for (const auto& transaction : block.transactions) {
    const auto found = std::find(pending.begin(), pending.end(), transaction);
    if (found == pending.end()) {
      return "transaction is not in the pending pool";
    }
    pending.erase(found);
  }
  blocks_.push_back(block);
  balances_ = std::move(balances);
  accepted_ids_ = std::move(ids);
  accepted_sequences_ = std::move(sequences);
  pending_ = std::move(pending);
  return "";
}

StepResult Chain::process_next(std::mt19937_64& random) {
  auto candidate = build_candidate(random);
  StepResult result;
  if (!candidate.block.transactions.empty()) {
    result.mining = mine_block(candidate.block);
    result.error = accept_block(candidate.block);
    if (!result.error.empty()) {
      return result;
    }
    result.block_accepted = true;
    result.removed = candidate.block.transactions.size();
  }
  for (const auto& rejected : candidate.rejected) {
    const auto found = std::find(pending_.begin(), pending_.end(), rejected.transaction);
    if (found == pending_.end()) {
      throw std::logic_error("rejected transaction missing from pool");
    }
    pending_.erase(found);
    rejected_.push_back(rejected);
    ++result.removed;
  }
  return result;
}

}  // namespace blockchain
