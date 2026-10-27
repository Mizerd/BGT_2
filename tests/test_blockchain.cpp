#include "hashing.hpp"
#include "models.hpp"
#include "chain.hpp"

#include <functional>
#include <iostream>
#include <limits>
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

void test_transfers() {
  Balances balances{{"a", 100}, {"b", 0}, {"c", 0}};
  std::set<std::string> ids;
  std::set<std::uint64_t> sequences;
  check(execute_transaction(make_transaction("a", "b", 70, 1), balances, ids, sequences).empty(),
        "valid transfer accepted");
  check(balances.at("a") == 30 && balances.at("b") == 70, "transfer balances");
  const auto after_first = balances;
  check(execute_transaction(make_transaction("a", "c", 60, 2), balances, ids, sequences) ==
            "insufficient funds", "sequential overspending rejected");
  check(balances == after_first && ids.size() == 1 && sequences.size() == 1,
        "failed transfer changes no state");
  check(execute_transaction(make_transaction("b", "c", 70, 3), balances, ids, sequences).empty(),
        "received funds can be spent");
  check(balances.at("b") == 0 && balances.at("c") == 70, "exact balance transfer");
}

void test_invalid_transactions() {
  const Balances initial{{"a", 100}, {"b", 10}, {"c", 0}};
  auto wrong_id = make_transaction("a", "b", 10, 1);
  wrong_id.amount = 11;
  const std::vector<std::pair<Transaction, std::string>> invalid = {
      {make_transaction("a", "b", 101, 1), "insufficient funds"},
      {make_transaction("a", "b", 0, 1), "amount must be a positive integer"},
      {make_transaction("a", "b", -1, 1), "amount must be a positive integer"},
      {make_transaction("missing", "b", 1, 1), "unknown sender"},
      {make_transaction("a", "missing", 1, 1), "unknown receiver"},
      {make_transaction("a", "a", 1, 1), "sender equals receiver"},
      {wrong_id, "incorrect transaction ID"},
  };
  for (const auto& [transaction, reason] : invalid) {
    auto balances = initial;
    std::set<std::string> ids;
    std::set<std::uint64_t> sequences;
    check(execute_transaction(transaction, balances, ids, sequences) == reason, reason);
    check(balances == initial && ids.empty() && sequences.empty(), "rejection has no effect");
  }
}

void test_duplicate_transactions() {
  Balances balances{{"a", 100}, {"b", 10}};
  std::set<std::string> ids;
  std::set<std::uint64_t> sequences;
  const auto transaction = make_transaction("a", "b", 10, 1);
  check(execute_transaction(transaction, balances, ids, sequences).empty(), "first execution");
  const auto confirmed = balances;
  check(execute_transaction(transaction, balances, ids, sequences) == "duplicate transaction",
        "cannot execute twice");
  check(execute_transaction(make_transaction("b", "a", 1, 1), balances, ids, sequences) ==
            "duplicate sequence", "sequence cannot be reused");
  check(balances == confirmed && ids.size() == 1, "duplicates have no effect");
}

void test_headers_and_roots() {
  const BlockHeader header{"abc", 1700000001, "0.1", "def", 42, 3};
  check(serialize_header(header) == "abc|1700000001|0.1|def|42|3", "canonical header");
  check(calculate_block_hash(header) == hash_text("abc|1700000001|0.1|def|42|3"),
        "fixed header hashing");
  check(calculate_block_hash(header) ==
            "a3843bd939695e2754504278ceb9b7cbe5b944da3854550a79dc2eea3cdf03db",
        "header matches Assignment 1 Python reference");
  check(calculate_block_hash(header) == calculate_block_hash(header), "repeatable header hash");
  const auto first = make_transaction("a", "b", 1, 1);
  const auto second = make_transaction("b", "a", 2, 2);
  check(transaction_root({first, second}) == hash_text(first.transaction_id + second.transaction_id),
        "v0.1 concatenates transaction IDs");
  check(transaction_root({first, second}) != transaction_root({second, first}), "root order matters");
  check(transaction_root({}) == hash_text(""), "empty transaction root");
}

void test_genesis() {
  const auto genesis = make_genesis_block();
  check(genesis.transactions.empty(), "genesis has no transactions");
  check(genesis.header.prev_hash == std::string(hash_length, '0'), "genesis predecessor");
  check(genesis.header.timestamp == genesis_timestamp, "fixed genesis timestamp");
  check(genesis.header.tx_root == hash_text(""), "genesis root");
  check(genesis.header.nonce == 0 && genesis.header.difficulty == 0, "genesis PoW exemption");
  check(genesis.block_hash == make_genesis_block().block_hash, "reproducible genesis");
}

void test_mining() {
  const auto genesis = make_genesis_block();
  const auto transactions = std::vector<Transaction>{make_transaction("a", "b", 5, 1)};
  Block block{{genesis.block_hash, genesis_timestamp + 1, "0.1", transaction_root(transactions), 0, 3},
              transactions, ""};
  const auto original_header = block.header;
  const auto stats = mine_block(block);
  check(block.block_hash.starts_with("000"), "real difficulty three");
  check(block.block_hash == calculate_block_hash(block.header), "recomputed PoW hash");
  check(stats.attempts == block.header.nonce + 1, "all attempts counted from zero");
  check(stats.duration_seconds >= 0, "measured duration");
  auto unchanged = block.header;
  unchanged.nonce = original_header.nonce;
  check(serialize_header(unchanged) == serialize_header(original_header), "other mining fields fixed");
  auto repeated = block;
  const auto repeated_stats = mine_block(repeated);
  check(repeated.block_hash == block.block_hash && repeated_stats.attempts == stats.attempts,
        "repeatable mining");
  check(!satisfies_difficulty("000", 3), "short fake hash rejected");
  check(!satisfies_difficulty(std::string(64, 'z'), 0), "non-hex rejected");
  check(!satisfies_difficulty(block.block_hash, -1), "negative difficulty rejected");
}

void test_candidate_and_acceptance() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  const std::vector<Transaction> transactions{make_transaction("a", "b", 70, 1),
                                               make_transaction("a", "c", 60, 2)};
  Chain chain(users, transactions, 0);
  std::mt19937_64 random(42);
  const auto before = chain.balances();
  auto candidate = chain.build_candidate(random);
  check(candidate.block.transactions.size() == 1, "candidate prevents combined overspending");
  check(candidate.rejected.size() == 1, "overspending entry explained");
  check(chain.balances() == before && chain.pending() == transactions && chain.blocks().size() == 1,
        "candidate building changes no confirmed state");
  mine_block(candidate.block);
  check(chain.validate_block(candidate.block).empty(), "complete block validates");
  check(chain.balances() == before, "validation changes no confirmed state");
  check(chain.accept_block(candidate.block).empty(), "block appended");
  check(chain.blocks().size() == 2 && chain.accepted_transactions() == 1, "accepted counts");
  check(chain.pending().size() == 1, "only accepted entry removed");
  check(chain.initial_balances() == before && chain.balances() != before, "original balances saved");
}

void test_failed_block_is_atomic() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  const std::vector<Transaction> transactions{make_transaction("a", "b", 70, 1),
                                               make_transaction("a", "c", 60, 2)};
  Chain chain(users, transactions, 0);
  Block block{{chain.blocks().back().block_hash, genesis_timestamp + 1, "0.1",
               transaction_root(transactions), 0, 0}, transactions, ""};
  mine_block(block);
  const auto before = chain.balances();
  check(chain.accept_block(block).find("insufficient funds") != std::string::npos,
        "block fails after first transfer was checked");
  check(chain.balances() == before && chain.blocks().size() == 1, "failure rolls back all transfers");
  check(chain.pending() == transactions && chain.accepted_transactions() == 0 && chain.rejected().empty(),
        "failed block preserves pool and accounting");
}

void test_pool_processing() {
  auto users = generate_users(5, 42);
  for (auto& user : users) {
    user.balance = 1000000;
  }
  const auto transactions = generate_transactions(users, 205, 42);
  Chain chain(users, transactions, 0);
  std::mt19937_64 random(42);
  while (!chain.pending().empty()) {
    const auto result = chain.process_next(random);
    check(result.error.empty() && result.removed > 0, "pool makes progress");
    chain.check_invariants();
  }
  check(chain.blocks().size() == 4 && chain.accepted_transactions() == 205, "all entries accepted");
  check(chain.blocks().back().transactions.size() == 5, "partial final block");
  check(chain.rejected().empty(), "no spurious rejections");
  std::set<std::string> ids;
  for (const auto& block : chain.blocks()) {
    for (const auto& transaction : block.transactions) {
      check(ids.insert(transaction.transaction_id).second, "accepted only once");
    }
  }
  Amount initial = 0;
  Amount final = 0;
  for (const auto& [key, balance] : chain.balances()) {
    check(balance >= 0, "nonnegative final balances");
    initial += chain.initial_balances().at(key);
    final += balance;
  }
  check(initial == final, "funds conserved");
}

Block make_test_block(const Chain& chain, const std::vector<Transaction>& transactions,
                      int difficulty = 0) {
  Block block{{chain.blocks().back().block_hash, chain.blocks().back().header.timestamp + 1,
               "0.1", transaction_root(transactions), 0, difficulty}, transactions, ""};
  mine_block(block);
  return block;
}

void check_rejected_block(Chain& chain, const Block& block, const std::string& reason) {
  const auto balances = chain.balances();
  const auto pending = chain.pending();
  const auto height = chain.blocks().size();
  const auto accepted = chain.accepted_transactions();
  const auto rejected = chain.rejected().size();
  check(chain.validate_block(block).find(reason) != std::string::npos, "validation: " + reason);
  check(chain.accept_block(block).find(reason) != std::string::npos, "acceptance: " + reason);
  check(chain.balances() == balances && chain.pending() == pending && chain.blocks().size() == height &&
            chain.accepted_transactions() == accepted && chain.rejected().size() == rejected,
        "rejected block preserves all confirmed state and pool");
  chain.check_invariants();
}

void test_invalid_block_headers() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}};
  const std::vector<Transaction> transactions{make_transaction("a", "b", 10, 1)};
  Chain chain(users, transactions, 0);
  const auto valid = make_test_block(chain, transactions);
  auto changed = valid;
  changed.header.prev_hash[0] = 'x';
  check_rejected_block(chain, changed, "incorrect previous hash");
  changed = valid;
  changed.header.tx_root[0] = 'x';
  check_rejected_block(chain, changed, "incorrect transaction root");
  changed = valid;
  changed.block_hash[0] = 'x';
  check_rejected_block(chain, changed, "incorrect block hash");
  changed = valid;
  changed.header.version = "0.2";
  check_rejected_block(chain, changed, "unsupported block version");
  changed = valid;
  changed.header.timestamp = genesis_timestamp;
  check_rejected_block(chain, changed, "timestamp must increase");
  check_rejected_block(chain, make_test_block(chain, {}), "block must contain 1 to 100");
  check_rejected_block(chain, make_test_block(chain, std::vector<Transaction>(101, transactions[0])),
                       "block must contain 1 to 100");
  check(chain.accept_block(valid).empty(), "valid block remains usable after failures");
  chain.check_invariants();
}

void test_invalid_block_transactions() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  const auto first = make_transaction("a", "b", 10, 1);
  const auto second = make_transaction("a", "c", 10, 2);
  Chain chain(users, {first, second}, 0);
  check_rejected_block(chain, make_test_block(chain, {first, first}), "duplicate transaction");
  check_rejected_block(chain, make_test_block(chain, {first, make_transaction("a", "c", 10, 1)}),
                       "duplicate sequence");
  for (const auto amount : {Amount{0}, Amount{-1}, Amount{101}}) {
    check_rejected_block(chain, make_test_block(chain, {make_transaction("a", "b", amount, 1)}),
                         amount <= 0 ? "positive integer" : "insufficient funds");
  }
  check_rejected_block(chain, make_test_block(chain, {make_transaction("missing", "b", 1, 1)}),
                       "unknown sender");
  check_rejected_block(chain, make_test_block(chain, {make_transaction("a", "missing", 1, 1)}),
                       "unknown receiver");
  auto tampered = first;
  tampered.amount = 11;
  check_rejected_block(chain, make_test_block(chain, {tampered}), "incorrect transaction ID");
  const auto not_pending = make_test_block(chain, {make_transaction("a", "b", 1, 3)});
  const auto before = chain.balances();
  check(chain.validate_block(not_pending).empty(), "transfer is structurally valid");
  check(chain.accept_block(not_pending) == "transaction is not in the pending pool", "local pool required");
  check(chain.balances() == before && chain.pending().size() == 2, "missing pool entry has no effect");
  check(chain.accept_block(make_test_block(chain, {first})).empty(), "first accepted block");
  check_rejected_block(chain, make_test_block(chain, {first}), "duplicate transaction");
  chain.check_invariants();
}

void test_block_difficulty_validation() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}};
  const std::vector<Transaction> transactions{make_transaction("a", "b", 10, 1)};
  Chain chain(users, transactions, 3);
  check_rejected_block(chain, make_test_block(chain, transactions, 0), "incorrect difficulty");
  auto invalid = make_test_block(chain, transactions, 3);
  do {
    ++invalid.header.nonce;
    invalid.block_hash = calculate_block_hash(invalid.header);
  } while (satisfies_difficulty(invalid.block_hash, 3));
  check_rejected_block(chain, invalid, "proof of work does not meet difficulty");
  check(chain.accept_block(make_test_block(chain, transactions, 3)).empty(), "difficulty three accepted");
  chain.check_invariants();
}

void test_rejected_pool_accounting() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}};
  const auto valid = make_transaction("a", "b", 10, 1);
  auto tampered = make_transaction("a", "b", 5, 2);
  tampered.transaction_id = "invalid";
  Chain chain(users, {valid, valid, tampered, make_transaction("a", "b", 1000, 3)}, 0);
  std::mt19937_64 random(42);
  const auto result = chain.process_next(random);
  check(result.block_accepted && result.removed == 4, "mixed pool processed");
  check(chain.accepted_transactions() == 1 && chain.rejected().size() == 3 && chain.pending().empty(),
        "each submitted occurrence accounted for");
  check(chain.balances().at("a") == 90 && chain.balances().at("b") == 10, "rejections have no effect");
  std::set<std::string> reasons;
  for (const auto& rejected : chain.rejected()) {
    reasons.insert(rejected.reason);
  }
  check(reasons == std::set<std::string>{"duplicate transaction", "incorrect transaction ID",
                                        "insufficient funds"}, "meaningful rejection reasons");
  chain.check_invariants();
}

void test_no_executable_transactions() {
  const std::vector<User> users{{"A", "a", 0}, {"B", "b", 0}};
  Chain chain(users, {make_transaction("a", "b", 1, 1), make_transaction("b", "a", 1, 2)}, 3);
  std::mt19937_64 random(42);
  const auto result = chain.process_next(random);
  check(!result.block_accepted && result.removed == 2 && result.mining.attempts == 0,
        "unexecutable pool rejected without mining");
  check(chain.pending().empty() && chain.rejected().size() == 2 && chain.blocks().size() == 1,
        "no infinite loop or empty block");
  chain.check_invariants();
  Chain empty(users, {}, 3);
  check(empty.process_next(random).removed == 0, "empty dataset is a no-op");
  empty.check_invariants();
}

void test_reproducible_chain() {
  const auto users = generate_users(10, 123);
  const auto transactions = generate_transactions(users, 130, 123);
  Chain first(users, transactions, 0);
  Chain second(users, transactions, 0);
  std::mt19937_64 first_random(123);
  std::mt19937_64 second_random(123);
  while (!first.pending().empty()) {
    first.process_next(first_random);
    second.process_next(second_random);
    first.check_invariants();
    second.check_invariants();
  }
  check(first.balances() == second.balances(), "repeatable final balances");
  check(first.blocks().size() == second.blocks().size(), "repeatable block count");
  for (std::size_t index = 0; index < first.blocks().size(); ++index) {
    check(first.blocks()[index].block_hash == second.blocks()[index].block_hash &&
              first.blocks()[index].transactions == second.blocks()[index].transactions,
          "repeatable block hashes and ordered data");
  }
}

void test_invalid_initial_state() {
  const std::vector<std::vector<User>> invalid = {
      {{"A", "a", -1}}, {{"A", "a", 10}, {"B", "a", 10}}, {{"A", "a|b", 10}},
      {{"A", "", 10}}, {{"A", "a", std::numeric_limits<Amount>::max()}, {"B", "b", 1}},
  };
  for (const auto& users : invalid) {
    bool rejected = false;
    try {
      Chain chain(users, {}, 0);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    check(rejected, "invalid initial state rejected");
  }
  Balances balances{{"a", 1}, {"b", std::numeric_limits<Amount>::max()}};
  std::set<std::string> ids;
  std::set<std::uint64_t> sequences;
  check(execute_transaction(make_transaction("a", "b", 1, 1), balances, ids, sequences) ==
            "receiver balance overflow", "integer overflow guarded");
  check(balances.at("a") == 1 && ids.empty(), "overflow changes no state");
}

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"custom hash", test_hash},
      {"users and balances", test_users},
      {"transaction IDs", test_transaction_ids},
      {"transaction generation", test_transaction_generation},
      {"valid sequential transfers", test_transfers},
      {"invalid transactions", test_invalid_transactions},
      {"duplicate transactions", test_duplicate_transactions},
      {"headers and transaction roots", test_headers_and_roots},
      {"genesis block", test_genesis},
      {"proof of work difficulty three", test_mining},
      {"candidate selection and acceptance", test_candidate_and_acceptance},
      {"failed block atomicity", test_failed_block_is_atomic},
      {"pool processing and balances", test_pool_processing},
      {"invalid block headers", test_invalid_block_headers},
      {"invalid block transactions", test_invalid_block_transactions},
      {"block difficulty enforcement", test_block_difficulty_validation},
      {"rejected pool accounting", test_rejected_pool_accounting},
      {"no executable transactions", test_no_executable_transactions},
      {"reproducible chain", test_reproducible_chain},
      {"invalid initial state", test_invalid_initial_state},
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
