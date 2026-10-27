#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace blockchain {

using Amount = std::int64_t;

struct User {
  std::string name;
  std::string public_key;
  Amount balance;

  bool operator==(const User&) const = default;
};

struct Transaction {
  std::string sender;
  std::string receiver;
  Amount amount;
  std::uint64_t sequence;
  std::string transaction_id;

  bool operator==(const Transaction&) const = default;
};

std::vector<User> generate_users(std::size_t count, std::uint64_t seed);
std::string serialize_transaction(const Transaction& transaction);
Transaction make_transaction(const std::string& sender, const std::string& receiver,
                             Amount amount, std::uint64_t sequence);
std::vector<Transaction> generate_transactions(const std::vector<User>& users,
                                               std::size_t count, std::uint64_t seed);

}  // namespace blockchain
