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
  check(genesis.block_hash == "e23688e389a5fa4d8f3814b9c43baba828e908796135c28e048275903549cf1b",
        "v0.1 genesis remains unchanged");
}

void test_mining() {
  const auto genesis = make_genesis_block();
  const auto transactions = std::vector<Transaction>{make_transaction("a", "b", 5, 1)};
  Block block{{genesis.block_hash, genesis_timestamp + 1, "0.2", merkle_root(transactions), 0, 3},
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
  check(candidate.deferred.size() == 1, "overspending entry explained");
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
  Block block{{chain.blocks().back().block_hash, genesis_timestamp + 1, "0.2",
               merkle_root(transactions), 0, 0}, transactions, ""};
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
               "0.2", merkle_root(transactions), 0, difficulty}, transactions, ""};
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
  changed.header.version = "0.3";
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
  tampered.transaction_id = std::string(64, '0');
  Chain chain(users, {valid, valid, tampered, make_transaction("a", "b", 1000, 3)}, 0);
  std::mt19937_64 random(42);
  const auto result = chain.process_next(random);
  check(result.block_accepted && result.removed == 1, "only winner entries removed");
  check(chain.pending().size() == 1 && chain.rejected().size() == 2, "unfunded transfer deferred");
  const auto stopped = chain.process_next(random);
  check(!stopped.block_accepted && stopped.removed == 1, "unfunded remainder rejected when no progress possible");
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

dihash::Digest256 reference_parent(const dihash::Digest256& left, const dihash::Digest256& right) {
  dihash::Hasher hasher;
  hasher.update(left);
  hasher.update(right);
  return hasher.finish();
}

void test_merkle_roots() {
  const Transaction a{"", "", 0, 0, std::string(64, '0')};
  const Transaction b{"", "", 0, 0, std::string(64, '1')};
  const Transaction c{"", "", 0, 0, std::string(64, '2')};
  const Transaction d{"", "", 0, 0, std::string(64, '3')};
  const Transaction e{"", "", 0, 0, std::string(64, '4')};
  dihash::Digest256 a_bytes{}, b_bytes{}, c_bytes{}, d_bytes{}, e_bytes{};
  b_bytes.fill(0x11);
  c_bytes.fill(0x22);
  d_bytes.fill(0x33);
  e_bytes.fill(0x44);
  const auto ab = reference_parent(a_bytes, b_bytes);
  const auto cc = reference_parent(c_bytes, c_bytes);
  const auto cd = reference_parent(c_bytes, d_bytes);
  const auto ee = reference_parent(e_bytes, e_bytes);
  check(merkle_root({}) == hash_text(""), "empty Merkle root preserves genesis convention");
  check(merkle_root({a}) == a.transaction_id, "one leaf is already the root");
  check(merkle_root({a, b}) == dihash::to_hex(ab), "two leaves hash raw bytes");
  check(merkle_root({a, b}) != hash_text(a.transaction_id + b.transaction_id), "hex text is not hashed");
  check(merkle_root({a, b, c}) == dihash::to_hex(reference_parent(ab, cc)), "three leaves");
  const auto expected_five = reference_parent(reference_parent(ab, cd), reference_parent(ee, ee));
  check(merkle_root({a, b, c, d, e}) == dihash::to_hex(expected_five), "five leaves duplicate at two levels");
  check(merkle_root({a, b, c}) == merkle_root({a, b, c, c}), "last odd leaf duplicated");
}

void test_merkle_changes() {
  const auto first = make_transaction("a", "b", 10, 1);
  const auto second = make_transaction("b", "a", 20, 2);
  const auto modified = make_transaction("a", "b", 11, 1);
  check(first.transaction_id != modified.transaction_id, "changed content gets a new ID");
  check(merkle_root({first, second}) != merkle_root({modified, second}), "changed ID changes root");
  check(merkle_root({first, second}) != merkle_root({second, first}), "different order changes root");
  const std::string binary("\0A\0", 3);
  const std::array<std::uint8_t, 3> bytes{0, 'A', 0};
  check(hash_text(binary) == dihash::to_hex(dihash::custom_hash(bytes)), "text adapter is binary safe");
  check(hash_text(binary) != hash_text(""), "zero byte does not terminate input");
}

void test_invalid_merkle_hashes() {
  for (const auto& id : {std::string(63, '0'), std::string(65, '0'), std::string(64, 'g'),
                         std::string(64, 'A'), std::string(64, '\0')}) {
    bool rejected = false;
    try {
      merkle_root({{"a", "b", 1, 1, id}});
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    check(rejected, "invalid hash encoding rejected even for one leaf");
  }
}

void test_merkle_block_validation() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  const std::vector<Transaction> transactions{make_transaction("a", "b", 10, 1),
                                               make_transaction("a", "c", 20, 2)};
  Chain chain(users, transactions, 0);
  const auto valid = make_test_block(chain, transactions);
  check(chain.validate_block(valid).empty(), "v0.2 Merkle root validates");
  auto changed = valid;
  changed.header.tx_root = transaction_root(transactions);
  mine_block(changed);
  check_rejected_block(chain, changed, "incorrect transaction root");
  changed = valid;
  changed.transactions[0] = make_transaction("a", "b", 11, 1);
  check_rejected_block(chain, changed, "incorrect transaction root");
  changed = valid;
  std::swap(changed.transactions[0], changed.transactions[1]);
  check_rejected_block(chain, changed, "incorrect transaction root");
  changed = valid;
  changed.transactions[0].transaction_id = "bad";
  check_rejected_block(chain, changed, "invalid transaction ID format");
  changed = valid;
  changed.header.version = "0.1";
  check_rejected_block(chain, changed, "unsupported block version");
  check(chain.accept_block(valid).empty(), "untampered Merkle block accepted");
  chain.check_invariants();
}

void test_transaction_structure() {
  Balances balances{{"a", 0}, {"b", 0}};
  std::set<std::string> ids;
  std::set<std::uint64_t> sequences;
  const auto transaction = make_transaction("a", "b", 100, 1);
  check(validate_transaction(transaction, balances, ids, sequences).empty(),
        "structural validation does not reserve funds");
  check(execute_transaction(transaction, balances, ids, sequences) == "insufficient funds",
        "execution checks available funds");
  for (const auto& invalid : {std::string(63, '0'), std::string(64, 'z'), std::string(64, 'A')}) {
    auto changed = transaction;
    changed.transaction_id = invalid;
    check(validate_transaction(changed, balances, ids, sequences) == "invalid transaction ID format",
          "ID must have canonical encoding");
    check(execute_transaction(changed, balances, ids, sequences) == "invalid transaction ID format",
          "execution checks ID format too");
  }
  check(balances.at("a") == 0 && balances.at("b") == 0 && ids.empty() && sequences.empty(),
        "validation changes nothing");
}

void test_pool_admission() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  Chain chain(users, {}, 0);
  const auto balances = chain.balances();
  const auto first = make_transaction("a", "b", 70, 1);
  const auto second = make_transaction("a", "c", 60, 2);
  check(chain.add_transaction(first).empty() && chain.add_transaction(second).empty(),
        "admission reserves no balance");
  check(chain.balances() == balances && chain.pending().size() == 2, "pool admission is not execution");
  check(chain.add_transaction(first) == "duplicate transaction", "duplicate pool ID rejected");
  check(chain.add_transaction(make_transaction("a", "c", 1, 1)) == "duplicate sequence",
        "duplicate pool sequence rejected");
  check(chain.add_transaction(make_transaction("missing", "b", 1, 3)) == "unknown sender",
        "unknown sender rejected on entry");
  check(chain.add_transaction(make_transaction("a", "missing", 1, 4)) == "unknown receiver",
        "unknown receiver rejected on entry");
  check(chain.add_transaction(make_transaction("a", "b", 0, 5)) == "amount must be a positive integer",
        "zero amount rejected on entry");
  auto malformed = make_transaction("a", "b", 1, 6);
  malformed.transaction_id = "bad";
  check(chain.add_transaction(malformed) == "invalid transaction ID format", "malformed ID rejected on entry");
  check(chain.rejected().size() == 6 && chain.pending().size() == 2, "admission rejections accounted for");
  check(chain.accept_block(make_test_block(chain, {first})).empty(), "valid pool transaction accepted");
  check(chain.add_transaction(first) == "duplicate transaction", "confirmed transaction cannot reenter");
  check(chain.balances().at("a") == 30 && chain.pending().size() == 1, "reentry has no state effect");
  chain.check_invariants();
}

void test_balance_limits() {
  const auto maximum = std::numeric_limits<Amount>::max();
  Balances balances{{"a", maximum}, {"b", 0}};
  std::set<std::string> ids;
  std::set<std::uint64_t> sequences;
  const auto transfer = make_transaction("a", "b", maximum, 1);
  check(execute_transaction(transfer, balances, ids, sequences).empty(), "maximum balance transfers safely");
  check(balances.at("a") == 0 && balances.at("b") == maximum, "no signed overflow at maximum");
  check(total_balance(balances) == maximum, "maximum total preserved");
  const auto before = balances;
  check(execute_transaction(make_transaction("a", "b", 1, 2), balances, ids, sequences) ==
            "insufficient funds", "sender cannot underflow");
  check(balances == before && ids.size() == 1, "underflow rejection is atomic");
  for (auto invalid : {Balances{{"a", -1}, {"b", 0}}, Balances{{"a", 1}, {"b", -1}}}) {
    std::set<std::string> empty_ids;
    std::set<std::uint64_t> empty_sequences;
    const auto original = invalid;
    check(execute_transaction(make_transaction("a", "b", 1, 1), invalid, empty_ids, empty_sequences) ==
              "negative account balance", "negative input state rejected");
    check(invalid == original && empty_ids.empty(), "negative input state remains unchanged");
  }
  const std::vector<User> users{{"A", "a", maximum}, {"B", "b", 0}};
  const auto extra = make_transaction("a", "b", 1, 2);
  Chain chain(users, {transfer, extra}, 0);
  check(chain.pending().size() == 2, "pool admission does not reserve maximum funds");
  check_rejected_block(chain, make_test_block(chain, {transfer, extra}), "insufficient funds");
  check(chain.accept_block(make_test_block(chain, {transfer})).empty(), "maximum transfer block accepted");
  chain.check_invariants();
}

void test_pool_invalid_fields() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}};
  auto incorrect_id = make_transaction("a", "b", 1, 4);
  incorrect_id.amount = 2;
  const std::vector<Transaction> invalid{make_transaction("a", "b", -1, 1),
                                       make_transaction("a", "a", 1, 2),
                                       make_transaction("a", "b", std::numeric_limits<Amount>::min(), 3),
                                       incorrect_id};
  Chain chain(users, invalid, 3);
  check(chain.pending().empty() && chain.rejected().size() == invalid.size(), "invalid fields rejected at entry");
  check(chain.balances() == chain.initial_balances(), "admission rejections preserve balances");
  check(chain.rejected().back().reason == "incorrect transaction ID", "valid hex with incorrect ID distinguished");
  chain.check_invariants();
}

void test_five_independent_candidates() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  const std::vector<Transaction> transactions{make_transaction("a", "b", 70, 1),
                                               make_transaction("a", "c", 60, 2)};
  Chain chain(users, transactions, 0);
  std::mt19937_64 random(42);
  const auto candidates = chain.build_candidates(random);
  check(candidates.size() == 5, "exactly five candidates");
  for (const auto& candidate : candidates) {
    check(candidate.block.header.prev_hash == chain.blocks().back().block_hash, "same confirmed parent");
    check(candidate.block.header.timestamp == genesis_timestamp + 1, "same next height timestamp");
    check(candidate.block.header.difficulty == 0 && candidate.block.header.version == "0.2",
          "same required difficulty and version");
    check(candidate.block.transactions.size() == 1 && candidate.deferred.size() == 1,
          "each candidate starts with its own full balance");
    check(candidate.block.header.tx_root == candidate.block.transactions[0].transaction_id,
          "single transaction Merkle root");
  }
  check(chain.balances() == chain.initial_balances() && chain.pending() == transactions &&
            chain.blocks().size() == 1 && chain.accepted_transactions() == 0,
        "building five candidates changes no confirmed state or pool");
  chain.check_invariants();
}

void test_candidate_alternatives() {
  auto users = generate_users(4, 42);
  for (auto& user : users) {
    user.balance = 1000000;
  }
  const auto transactions = generate_transactions(users, 130, 42);
  Chain chain(users, transactions, 0);
  std::mt19937_64 random(42);
  const auto candidates = chain.build_candidates(random);
  std::set<std::vector<std::uint64_t>> orders;
  for (const auto& candidate : candidates) {
    check(candidate.block.transactions.size() == 100, "candidate capacity");
    std::set<std::string> ids;
    std::vector<std::uint64_t> order;
    for (const auto& transaction : candidate.block.transactions) {
      check(ids.insert(transaction.transaction_id).second, "no duplicate within candidate");
      order.push_back(transaction.sequence);
    }
    orders.insert(order);
  }
  check(orders.size() == 5, "five distinct deterministic selections or orderings");
  check(chain.pending() == transactions, "overlapping alternatives leave pool unchanged");
}

void test_resumable_mining() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}};
  Chain chain(users, {make_transaction("a", "b", 5, 1)}, 3);
  std::mt19937_64 random(42);
  auto candidate = chain.build_candidate(random);
  auto expected = candidate.block;
  const auto expected_stats = mine_block(expected);
  check(expected.header.nonce > 1, "fixture requires continued mining");
  const auto header = candidate.block.header;
  check(!mine_candidate(candidate, 1), "first limited search fails");
  check(candidate.mining.attempts == 1 && candidate.block.header.nonce == 1, "next nonce preserved");
  check(!mine_candidate(candidate, 1), "second limited search fails");
  check(candidate.mining.attempts == 2 && candidate.block.header.nonce == 2, "unused nonce tested next");
  check(mine_candidate(candidate, expected.header.nonce), "continued search finds original solution");
  check(candidate.block.block_hash == expected.block_hash && candidate.mining.attempts == expected_stats.attempts,
        "no attempts repeated or skipped");
  auto unchanged = candidate.block.header;
  unchanged.nonce = header.nonce;
  check(serialize_header(unchanged) == serialize_header(header), "other header fields remain fixed");
  const auto attempts = candidate.mining.attempts;
  check(mine_candidate(candidate, 10) && candidate.mining.attempts == attempts, "solved candidate is not remined");
  check(candidate.mining.duration_seconds >= 0, "cumulative mining time measured");
}

void test_mining_budget_edges() {
  Candidate candidate;
  candidate.block = make_genesis_block();
  candidate.block.header.difficulty = 64;
  bool rejected = false;
  try {
    mine_candidate(candidate, 0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  check(rejected && candidate.mining.attempts == 0, "zero budget rejected without mining");
  check(!mine_candidate(candidate, 3), "small hard search fails within budget");
  check(candidate.mining.attempts == 3 && candidate.block.header.nonce == 3, "budget exactly respected");
  candidate.block.header.nonce = std::numeric_limits<std::uint64_t>::max();
  rejected = false;
  try {
    mine_candidate(candidate, 1);
  } catch (const std::overflow_error&) {
    rejected = true;
  }
  check(rejected && candidate.block.header.nonce == std::numeric_limits<std::uint64_t>::max(),
        "nonce cannot wrap around");
}

void test_competition_rounds() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}};
  const std::vector<Transaction> transactions{make_transaction("a", "b", 5, 1)};
  Chain chain(users, transactions, 0);
  std::mt19937_64 random(42);
  auto candidates = chain.build_candidates(random);
  const auto result = mine_candidates(candidates, 1, 1);
  check(result.winner == 0 && result.rounds == 1, "first candidate wins ties after complete round");
  for (const auto& candidate : candidates) {
    check(candidate.found && candidate.mining.attempts == 1, "all five receive their round budget");
    check(chain.validate_block(candidate.block).empty(), "each found solution is a valid block");
  }
  check(chain.pending() == transactions && chain.balances() == chain.initial_balances(),
        "competition alone does not accept a winner");
}

void test_competition_limit() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}};
  Chain chain(users, {make_transaction("a", "b", 5, 1)}, 64);
  std::mt19937_64 random(42);
  auto candidates = chain.build_candidates(random);
  const auto result = mine_candidates(candidates, 2, 7);
  check(result.winner == -1 && result.rounds == 3, "budgets grow two, four, then one remaining attempt");
  for (const auto& candidate : candidates) {
    check(!candidate.found && candidate.mining.attempts == 7 && candidate.block.header.nonce == 7,
          "each candidate stops at cap without restarting nonces");
    auto last_header = candidate.block.header;
    last_header.nonce = 6;
    check(candidate.block.block_hash == calculate_block_hash(last_header), "last tested nonce was six");
    check(candidate.block.header.difficulty == 64, "difficulty never lowered");
  }
  candidates = chain.build_candidates(random);
  const auto capped = mine_candidates(candidates, 100, 3);
  check(capped.rounds == 1 && candidates[0].mining.attempts == 3, "initial budget is capped too");
  bool rejected = false;
  try {
    mine_candidates(candidates, 0, 3);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  check(rejected, "zero competition budget rejected");
}

void test_failed_competition_state() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}};
  const std::vector<Transaction> transactions{make_transaction("a", "b", 5, 1)};
  Chain chain(users, transactions, 64);
  const auto before = chain.balances();
  std::mt19937_64 random(42);
  const auto result = chain.process_next(random, 2, 7);
  check(!result.block_accepted && result.competition.winner == -1 && result.removed == 0 &&
            result.error == "mining attempt limit reached", "failed competition stops with a reason");
  check(chain.balances() == before && chain.blocks().size() == 1 && chain.pending() == transactions &&
            chain.rejected().empty() && chain.accepted_transactions() == 0,
        "failed searches change no confirmed state or pool entries");
  for (const auto& candidate : result.candidates) {
    check(candidate.mining.attempts == 7, "competition cap applies to each candidate");
  }
  chain.check_invariants();
}

void test_only_winner_changes_state() {
  auto users = generate_users(4, 42);
  for (auto& user : users) {
    user.balance = 1000000;
  }
  const auto transactions = generate_transactions(users, 130, 42);
  Chain chain(users, transactions, 0);
  auto expected = chain.balances();
  std::mt19937_64 random(42);
  const auto result = chain.process_next(random, 1, 1);
  check(result.block_accepted && result.candidates.size() == 5 && result.competition.winner == 0,
        "exactly one deterministic winner");
  const auto& winner = result.candidates[0].block;
  std::set<std::string> confirmed;
  for (const auto& transaction : winner.transactions) {
    expected.at(transaction.sender) -= transaction.amount;
    expected.at(transaction.receiver) += transaction.amount;
    confirmed.insert(transaction.transaction_id);
  }
  check(chain.balances() == expected && chain.blocks().size() == 2 && chain.accepted_transactions() == 100,
        "only winner transfers affect balances and chain");
  check(chain.pending().size() == 30 && chain.rejected().empty(), "only winner transactions removed");
  for (const auto& transaction : transactions) {
    bool remains = false;
    for (const auto& pending : chain.pending()) {
      if (pending == transaction) {
        remains = true;
      }
    }
    check(remains == !confirmed.contains(transaction.transaction_id), "exact pool membership after competition");
  }
  bool overlap = false;
  bool losing_only = false;
  for (std::size_t index = 1; index < result.candidates.size(); ++index) {
    const auto& loser = result.candidates[index];
    check(loser.found, "fixture has multiple valid solutions");
    for (const auto& transaction : loser.block.transactions) {
      if (confirmed.contains(transaction.transaction_id)) {
        overlap = true;
      } else {
        losing_only = true;
      }
    }
    check_rejected_block(chain, loser.block, "incorrect previous hash");
  }
  check(overlap && losing_only, "both overlapping and losing-only transactions exercised");
  chain.check_invariants();
}

void test_deferred_payment_becomes_funded() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  const auto payment = make_transaction("b", "c", 70, 1);
  const auto funding = make_transaction("a", "b", 70, 2);
  Chain chain(users, {payment, funding}, 0);
  std::mt19937_64 random(42);
  const auto first = chain.process_next(random, 1, 1);
  check(first.block_accepted && chain.accepted_transactions() == 1, "funding winner accepted first");
  check(chain.pending() == std::vector<Transaction>{payment} && chain.rejected().empty(),
        "deferred transaction remains eligible even if another candidate included it");
  const auto second = chain.process_next(random, 1, 1);
  check(second.block_accepted && chain.pending().empty() && chain.accepted_transactions() == 2,
        "payment reconsidered after balances change");
  check(chain.balances().at("a") == 30 && chain.balances().at("b") == 0 && chain.balances().at("c") == 70,
        "both blocks apply exactly once");
  chain.check_invariants();
}

void test_progress_with_large_unfunded_pool() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  std::vector<Transaction> transactions;
  for (std::uint64_t sequence = 1; sequence <= 200; ++sequence) {
    transactions.push_back(make_transaction("b", "c", 1, sequence));
  }
  transactions.push_back(make_transaction("a", "b", 100, 201));
  Chain chain(users, transactions, 0);
  std::mt19937_64 random(42);
  std::size_t steps = 0;
  while (!chain.pending().empty()) {
    const auto result = chain.process_next(random, 1, 1);
    check(result.error.empty() && result.removed > 0, "full pool scan makes progress");
    check(++steps <= 3, "bounded termination");
    chain.check_invariants();
  }
  check(chain.accepted_transactions() == 101 && chain.rejected().size() == 100,
        "funded payments accepted and unfunded remainder accounted for");
  check(chain.balances().at("c") == 100 && total_balance(chain.balances()) == 100, "funds conserved");
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
      {"Merkle roots and odd levels", test_merkle_roots},
      {"Merkle content and order", test_merkle_changes},
      {"invalid Merkle encoding", test_invalid_merkle_hashes},
      {"Merkle block validation", test_merkle_block_validation},
      {"transaction structure and ID format", test_transaction_structure},
      {"transaction pool admission", test_pool_admission},
      {"balance limits and rollback", test_balance_limits},
      {"invalid pool fields", test_pool_invalid_fields},
      {"five independent candidates", test_five_independent_candidates},
      {"candidate alternatives", test_candidate_alternatives},
      {"resumable candidate mining", test_resumable_mining},
      {"mining budget boundaries", test_mining_budget_edges},
      {"competition rounds and ties", test_competition_rounds},
      {"competition budget limit", test_competition_limit},
      {"failed competition preserves state", test_failed_competition_state},
      {"only winner changes state", test_only_winner_changes_state},
      {"deferred payment becomes funded", test_deferred_payment_becomes_funded},
      {"large unfunded pool terminates", test_progress_with_large_unfunded_pool},
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
