#include "chain.hpp"
#include "hashing.hpp"

#include <limits>

namespace blockchain {

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
