#include "chain.hpp"
#include "hashing.hpp"

#include <chrono>
#include <limits>
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

}  // namespace blockchain
