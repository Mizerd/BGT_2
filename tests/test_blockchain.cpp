#include "hashing.hpp"
#include "models.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace blockchain;

void check(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void test_hash() {
  check(hash_text("") == "f83958be8ca002bca110726b8b5c768ebc1a8cc5a2f2e183538c783707d97286",
        "empty input matches Assignment 1");
  check(hash_text("abc") == "a881978cbef8990b0868ec299003e871d8523ccd3f7cfb8f9bd667b568f476bf",
        "abc matches Assignment 1");
  const auto first = hash_text("A");
  check(first != hash_text("B"), "different test inputs differ");
  check(first == hash_text("A"), "hash calls are independent");
  for (const auto& input : {std::string(), std::string("hello"), std::string(1000, 'x')}) {
    const auto hash = hash_text(input);
    check(hash.size() == hash_length, "fixed hash length");
    check(hash.find_first_not_of("0123456789abcdef") == std::string::npos, "hex output");
  }
  dihash::Digest256 digest{};
  digest.back() = 1;
  check(dihash::to_hex(digest) == std::string(63, '0') + "1", "leading zeros preserved");
}

void test_users() {
  const auto users = generate_users(1000, 20261029);
  check(users.size() == 1000, "requested user count");
  check(users == generate_users(1000, 20261029), "repeatable users and balances");
  check(users != generate_users(1000, 1), "seed affects balances");
  std::set<std::string> keys;
  for (const auto& user : users) {
    check(keys.insert(user.public_key).second, "unique public keys");
    check(user.public_key.find_first_not_of("0123456789abcdef") == std::string::npos,
          "simulated hexadecimal keys");
    check(user.balance >= 100 && user.balance <= 1000000, "initial balance bounds");
  }
  check(users.front().name == "User0001", "readable names");
}

void test_transaction_ids() {
  const auto transaction = make_transaction("a12f", "b34e", 100, 5);
  check(serialize_transaction(transaction) == "a12f|b34e|100|5", "canonical serialization");
  check(transaction.transaction_id == hash_text("a12f|b34e|100|5"), "hash canonical bytes");
  check(transaction == make_transaction("a12f", "b34e", 100, 5), "identical transaction IDs");
  for (const auto& changed : {make_transaction("c", "b34e", 100, 5),
                             make_transaction("a12f", "c", 100, 5),
                             make_transaction("a12f", "b34e", 101, 5),
                             make_transaction("a12f", "b34e", 100, 6)}) {
    check(transaction.transaction_id != changed.transaction_id, "changed data changes ID");
  }
  auto changed_id = transaction;
  changed_id.transaction_id = "ignored";
  check(serialize_transaction(changed_id) == serialize_transaction(transaction), "ID not serialized");
  bool rejected = false;
  try {
    make_transaction("a|b", "c", 1, 1);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  check(rejected, "delimiter rejected");
}

void test_transaction_generation() {
  const auto users = generate_users(20, 42);
  const auto original = users;
  const auto transactions = generate_transactions(users, 200, 42);
  check(transactions == generate_transactions(users, 200, 42), "repeatable transaction data");
  check(transactions != generate_transactions(users, 200, 43), "transaction seed matters");
  check(users == original, "generation leaves balances unchanged");
  std::set<std::uint64_t> sequences;
  std::set<std::string> identifiers;
  for (const auto& transaction : transactions) {
    check(transaction.sender != transaction.receiver, "different parties");
    check(transaction.amount > 0 && transaction.amount <= 10000, "sensible positive payments");
    check(sequences.insert(transaction.sequence).second, "unique sequences");
    check(identifiers.insert(transaction.transaction_id).second, "unique generated IDs");
  }
}

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"custom hash", test_hash},
      {"users and balances", test_users},
      {"transaction IDs", test_transaction_ids},
      {"transaction generation", test_transaction_generation},
  };
  int failures = 0;
  for (const auto& [name, test] : tests) {
    try {
      test();
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
  }
  std::cout << tests.size() - failures << '/' << tests.size() << " tests passed\n";
  return failures == 0 ? 0 : 1;
}
