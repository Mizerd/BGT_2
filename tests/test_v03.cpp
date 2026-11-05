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

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"six Merkle leaves", test_six_merkle_leaves},
      {"attempt counter limit", test_attempt_counter_limit},
      {"empty competition", test_empty_competition},
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
