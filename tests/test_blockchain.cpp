#include "hashing.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
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

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"custom hash", test_hash},
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
