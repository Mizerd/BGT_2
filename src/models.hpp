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

std::vector<User> generate_users(std::size_t count, std::uint64_t seed);

}  // namespace blockchain
