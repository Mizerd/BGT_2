#include "models.hpp"
#include "hashing.hpp"

#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace blockchain {

std::vector<User> generate_users(std::size_t count, std::uint64_t seed) {
  if (count < 2) {
    throw std::invalid_argument("at least two users are required");
  }
  std::mt19937_64 random(seed);
  std::uniform_int_distribution<Amount> balance(100, 1000000);
  std::vector<User> users;
  for (std::size_t index = 0; index < count; ++index) {
    std::ostringstream name;
    name << "User" << std::setw(4) << std::setfill('0') << index + 1;
    std::ostringstream key;
    key << std::hex << std::setw(16) << std::setfill('0') << index + 1;
    users.push_back({name.str(), key.str(), balance(random)});
  }
  return users;
}

std::string serialize_transaction(const Transaction& transaction) {
  if (transaction.sender.find('|') != std::string::npos ||
      transaction.receiver.find('|') != std::string::npos) {
    throw std::invalid_argument("identifiers cannot contain '|'");
  }
  return transaction.sender + "|" + transaction.receiver + "|" +
         std::to_string(transaction.amount) + "|" + std::to_string(transaction.sequence);
}

Transaction make_transaction(const std::string& sender, const std::string& receiver,
                             Amount amount, std::uint64_t sequence) {
  Transaction transaction{sender, receiver, amount, sequence, ""};
  transaction.transaction_id = hash_text(serialize_transaction(transaction));
  return transaction;
}

std::vector<Transaction> generate_transactions(const std::vector<User>& users,
                                               std::size_t count, std::uint64_t seed) {
  if (users.size() < 2) {
    throw std::invalid_argument("transactions require at least two users");
  }
  std::mt19937_64 random(seed);
  std::uniform_int_distribution<std::size_t> sender_index(0, users.size() - 1);
  std::uniform_int_distribution<std::size_t> receiver_index(0, users.size() - 2);
  std::uniform_int_distribution<Amount> payment(1, 10000);
  std::vector<Transaction> transactions;
  for (std::size_t index = 0; index < count; ++index) {
    const auto sender = sender_index(random);
    auto receiver = receiver_index(random);
    // Skip the sender while keeping every other receiver equally likely.
    if (receiver >= sender) {
      ++receiver;
    }
    transactions.push_back(make_transaction(users[sender].public_key, users[receiver].public_key,
                                            payment(random), index + 1));
  }
  return transactions;
}

std::string transaction_root(const std::vector<Transaction>& transactions) {
  std::string identifiers;
  for (const auto& transaction : transactions) {
    identifiers += transaction.transaction_id;
  }
  return hash_text(identifiers);
}

std::string serialize_header(const BlockHeader& header) {
  return header.prev_hash + "|" + std::to_string(header.timestamp) + "|" + header.version +
         "|" + header.tx_root + "|" + std::to_string(header.nonce) + "|" +
         std::to_string(header.difficulty);
}

std::string calculate_block_hash(const BlockHeader& header) {
  return hash_text(serialize_header(header));
}

Block make_genesis_block() {
  Block block{{std::string(hash_length, '0'), genesis_timestamp, "0.1", hash_text(""), 0, 0}, {}, ""};
  block.block_hash = calculate_block_hash(block.header);
  return block;
}

}  // namespace blockchain
