#include "chain.hpp"
#include "hashing.hpp"

#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace blockchain;

void check(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

dihash::Digest256 parent(const dihash::Digest256& left, const dihash::Digest256& right) {
  dihash::Hasher hasher;
  hasher.update(left);
  hasher.update(right);
  return hasher.finish();
}

void test_six_merkle_leaves() {
  std::vector<Transaction> leaves;
  std::array<dihash::Digest256, 6> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index].fill(static_cast<std::uint8_t>(index * 0x11));
    leaves.push_back({"", "", 0, index, std::string(64, static_cast<char>('0' + index))});
  }
  const auto first = parent(bytes[0], bytes[1]);
  const auto second = parent(bytes[2], bytes[3]);
  const auto third = parent(bytes[4], bytes[5]);
  const auto expected = parent(parent(first, second), parent(third, third));
  check(merkle_root(leaves) == dihash::to_hex(expected), "duplicate the internal node, not a leaf");
}

void test_attempt_counter_limit() {
  Candidate candidate;
  candidate.block = make_genesis_block();
  candidate.block.header.difficulty = 64;
  candidate.mining.attempts = std::numeric_limits<std::uint64_t>::max();
  const auto header = serialize_header(candidate.block.header);
  const auto hash = candidate.block.block_hash;
  bool rejected = false;
  try {
    mine_candidate(candidate, 1);
  } catch (const std::overflow_error&) {
    rejected = true;
  }
  check(rejected, "attempt counter overflow rejected");
  check(candidate.mining.attempts == std::numeric_limits<std::uint64_t>::max(), "counter cannot wrap");
  check(serialize_header(candidate.block.header) == header && candidate.block.block_hash == hash,
        "counter rejection does not test another nonce");
}

void test_empty_competition() {
  std::vector<Candidate> candidates(5);
  for (auto& candidate : candidates) {
    candidate.block = make_genesis_block();
  }
  const auto result = mine_candidates(candidates, 1, 3);
  check(result.winner == -1 && result.rounds == 0, "empty candidates are not mined");
  for (const auto& candidate : candidates) {
    check(candidate.mining.attempts == 0 && !candidate.found, "empty competition changes nothing");
  }
}

void test_stale_solved_candidate() {
  Chain chain({{"A", "a", 100}, {"B", "b", 0}}, {make_transaction("a", "b", 5, 1)}, 0);
  std::mt19937_64 random(42);
  auto solved = chain.build_candidate(random);
  check(mine_candidate(solved, 1), "valid fixture is solved");
  std::vector<Candidate> invalid(3, solved);
  invalid[0].block.block_hash = std::string(64, '0');
  ++invalid[1].block.header.timestamp;
  invalid[2].block.header.difficulty = 64;
  invalid[2].block.block_hash = calculate_block_hash(invalid[2].block.header);
  check(!satisfies_difficulty(invalid[2].block.block_hash, 64), "fixture has no valid proof");
  for (auto& candidate : invalid) {
    const auto header = serialize_header(candidate.block.header);
    const auto hash = candidate.block.block_hash;
    const auto attempts = candidate.mining.attempts;
    bool rejected = false;
    try {
      mine_candidate(candidate, 1);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    check(rejected, "found flag cannot replace hash and PoW verification");
    check(candidate.mining.attempts == attempts && serialize_header(candidate.block.header) == header &&
              candidate.block.block_hash == hash, "invalid cached solution is not mined or changed");
  }
  chain.check_invariants();
}

void check_unchanged(const std::vector<Candidate>& candidates, const std::vector<Candidate>& before) {
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const auto& candidate = candidates[index];
    const auto& original = before[index];
    check(serialize_header(candidate.block.header) == serialize_header(original.block.header) &&
              candidate.block.block_hash == original.block.block_hash &&
              candidate.block.transactions == original.block.transactions && candidate.found == original.found &&
              candidate.mining.attempts == original.mining.attempts &&
              candidate.mining.duration_seconds == original.mining.duration_seconds,
          "invalid competition must be rejected before any candidate is mined");
  }
}

void test_competition_context() {
  Chain chain({{"A", "a", 100}, {"B", "b", 0}}, {make_transaction("a", "b", 5, 1)}, 0);
  std::mt19937_64 random(42);
  for (int field = 0; field < 4; ++field) {
    auto candidates = chain.build_candidates(random);
    auto& header = candidates.back().block.header;
    if (field == 0) header.prev_hash = std::string(64, '0');
    if (field == 1) ++header.timestamp;
    if (field == 2) header.version = "0.3";
    if (field == 3) header.difficulty = 1;
    const auto before = candidates;
    bool rejected = false;
    try {
      mine_candidates(candidates, 1, 1);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    check(rejected, "competition must contain candidates for the same height and rules");
    check_unchanged(candidates, before);
  }
}

void test_competition_cached_proof() {
  Chain chain({{"A", "a", 100}, {"B", "b", 0}}, {make_transaction("a", "b", 5, 1)}, 0);
  std::mt19937_64 random(42);
  for (int invalid = 0; invalid < 2; ++invalid) {
    auto candidates = chain.build_candidates(random);
    if (invalid == 0) {
      candidates.back().found = true;
      candidates.back().block.block_hash = "bad";
    } else {
      for (auto& candidate : candidates) {
        candidate.block = make_genesis_block();
      }
      candidates[0].found = true;
    }
    const auto before = candidates;
    bool rejected = false;
    try {
      mine_candidates(candidates, 1, 1);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    check(rejected, "forged or empty cached winner rejected");
    check_unchanged(candidates, before);
  }
}

void test_competition_existing_budget() {
  Chain chain({{"A", "a", 100}, {"B", "b", 0}}, {make_transaction("a", "b", 5, 1)}, 0);
  std::mt19937_64 random(42);
  auto candidates = chain.build_candidates(random);
  candidates.back().mining.attempts = 2;
  const auto before = candidates;
  bool rejected = false;
  try {
    mine_candidates(candidates, 1, 1);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  check(rejected, "new limit cannot be smaller than work already recorded");
  check_unchanged(candidates, before);
  candidates = chain.build_candidates(random);
  const auto solved = mine_candidates(candidates, 1, 1);
  check(solved.winner == 0, "fixture solved within limit");
  const auto cached = candidates;
  const auto repeated = mine_candidates(candidates, 1, 1);
  check(repeated.winner == 0 && repeated.rounds == 0, "valid cached solutions stay usable");
  check_unchanged(candidates, cached);
}

Block block_for(const Chain& chain, const std::vector<Transaction>& transactions) {
  Block block{{chain.blocks().back().block_hash, chain.blocks().back().header.timestamp + 1,
               "0.2", merkle_root(transactions), 0, 0}, transactions, ""};
  mine_block(block);
  return block;
}

void test_late_pool_rejection() {
  const std::vector<User> users{{"A", "a", 100}, {"B", "b", 0}, {"C", "c", 0}};
  const auto first = make_transaction("a", "b", 10, 1);
  const auto second = make_transaction("a", "c", 20, 2);
  const auto outside = make_transaction("b", "c", 5, 3);
  Chain chain(users, {first, second}, 0);
  const auto balances = chain.balances();
  const auto pending = chain.pending();
  const auto invalid = block_for(chain, {first, outside});
  check(chain.validate_block(invalid).empty(), "both transfers are valid in order");
  check(chain.accept_block(invalid) == "transaction is not in the pending pool", "last entry is outside pool");
  check(chain.balances() == balances && chain.pending() == pending && chain.blocks().size() == 1 &&
            chain.accepted_transactions() == 0 && chain.rejected().empty(),
        "late pool rejection rolls back the earlier pool transaction too");
  chain.check_invariants();
  check(chain.accept_block(block_for(chain, {first, second})).empty(), "valid block still accepts after rejection");
  check(chain.balances().at("a") == 70 && chain.balances().at("b") == 10 && chain.balances().at("c") == 20,
        "only accepted transfers change balances");
  chain.check_invariants();
}

void test_pool_removal_order() {
  std::vector<Transaction> transactions;
  for (std::uint64_t sequence = 1; sequence <= 5; ++sequence) {
    transactions.push_back(make_transaction("a", "b", static_cast<Amount>(sequence), sequence));
  }
  Chain chain({{"A", "a", 100}, {"B", "b", 0}}, transactions, 0);
  check(chain.accept_block(block_for(chain, {transactions[3], transactions[1]})).empty(), "unordered subset accepts");
  check(chain.pending() == std::vector<Transaction>{transactions[0], transactions[2], transactions[4]},
        "unconfirmed entries retain their original order");
  check(chain.balances().at("a") == 94 && chain.balances().at("b") == 6, "subset counted exactly once");
  check(chain.accept_block(block_for(chain, {transactions[4], transactions[0], transactions[2]})).empty(),
        "remaining entries accept in another order");
  check(chain.pending().empty() && chain.accepted_transactions() == 5 && chain.balances().at("a") == 85 &&
            chain.balances().at("b") == 15, "no missing or repeated pool entry");
  chain.check_invariants();
}

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"six Merkle leaves", test_six_merkle_leaves},
      {"attempt counter limit", test_attempt_counter_limit},
      {"empty competition", test_empty_competition},
      {"stale solved candidate", test_stale_solved_candidate},
      {"competition context", test_competition_context},
      {"competition cached proof", test_competition_cached_proof},
      {"competition existing budget", test_competition_existing_budget},
      {"late pool rejection", test_late_pool_rejection},
      {"pool removal order", test_pool_removal_order},
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
