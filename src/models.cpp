#include "models.hpp"

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

}  // namespace blockchain
