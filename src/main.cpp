#include "chain.hpp"
#include "hashing.hpp"

#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace blockchain;

struct Options {
  std::size_t users = 1000;
  std::size_t transactions = 10000;
  std::uint64_t seed = 20261029;
  int difficulty = 3;
};

void print_usage() {
  std::cout << "Usage: blockchain [--users N] [--transactions N] [--seed N] [--difficulty N]\n"
            << "Defaults: users=1000 transactions=10000 seed=20261029 difficulty=3\n"
            << "Users must be >= 2; transactions and seed >= 0; difficulty 0..64.\n";
}

std::uint64_t parse_number(std::string_view text) {
  std::uint64_t number = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
  if (result.ec != std::errc() || result.ptr != text.data() + text.size()) {
    throw std::invalid_argument("expected a nonnegative decimal integer");
  }
  return number;
}

Options parse_options(int argc, char* argv[]) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view option = argv[index];
    if (option != "--users" && option != "--transactions" && option != "--seed" &&
        option != "--difficulty") {
      throw std::invalid_argument("unknown option: " + std::string(option));
    }
    if (++index == argc) {
      throw std::invalid_argument("missing value for " + std::string(option));
    }
    const auto value = parse_number(argv[index]);
    if (option == "--seed") {
      options.seed = value;
    } else if (option == "--difficulty") {
      if (value > hash_length) {
        throw std::invalid_argument("difficulty must be between 0 and 64");
      }
      options.difficulty = static_cast<int>(value);
    } else {
      if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("dataset size is too large");
      }
      if (option == "--users") {
        options.users = static_cast<std::size_t>(value);
      } else {
        options.transactions = static_cast<std::size_t>(value);
      }
    }
  }
  if (options.users < 2) {
    throw std::invalid_argument("at least two users are required");
  }
  return options;
}

void print_block(std::size_t height, const Block& block, const MiningStats& mining) {
  std::cout << "Block " << height << " | transactions=" << block.transactions.size()
            << " | difficulty=" << block.header.difficulty << " | nonce=" << block.header.nonce
            << " | attempts=" << mining.attempts << " | mining=" << mining.duration_seconds << "s\n"
            << "  prev_hash=" << block.header.prev_hash << '\n'
            << "  tx_root=" << block.header.tx_root << '\n'
            << "  block_hash=" << block.block_hash << '\n';
}

int main(int argc, char* argv[]) {
  const auto start = std::chrono::steady_clock::now();
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
      print_usage();
      return 0;
    }
    const auto options = parse_options(argc, argv);
    const auto users = generate_users(options.users, options.seed);
    auto transactions = generate_transactions(users, options.transactions, options.seed);
    Chain chain(users, std::move(transactions), options.difficulty);
    std::mt19937_64 random(options.seed);
    std::string stopped_reason;
    std::cout << std::fixed << std::setprecision(6);
    while (!chain.pending().empty()) {
      const auto result = chain.process_next(random);
      if (result.block_accepted) {
        print_block(chain.blocks().size() - 1, chain.blocks().back(), result.mining);
      }
      if (!result.error.empty() || result.removed == 0) {
        stopped_reason = result.error.empty() ? "no further progress possible" : result.error;
        break;
      }
    }
    chain.check_invariants();
    const double duration = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << "Users: " << users.size()
              << "\nGenerated transactions: " << options.transactions
              << "\nAccepted blocks (excluding genesis): " << chain.blocks().size() - 1
              << "\nAccepted transactions: " << chain.accepted_transactions()
              << "\nRejected transactions: " << chain.rejected().size()
              << "\nRemaining transactions: " << chain.pending().size()
              << "\nInitial total balance: " << total_balance(chain.initial_balances())
              << "\nFinal total balance: " << total_balance(chain.balances())
              << "\nTotal execution time: " << duration << "s\n";
    std::map<std::string, std::size_t> reasons;
    for (const auto& rejected : chain.rejected()) {
      ++reasons[rejected.reason];
    }
    std::cout << "Rejected reasons:" << (reasons.empty() ? " none\n" : "\n");
    for (const auto& [reason, count] : reasons) {
      std::cout << "  " << reason << ": " << count << '\n';
    }
    if (chain.pending().empty()) {
      std::cout << "Remaining reasons: none\n";
    } else {
      std::cout << "Remaining reasons: " << chain.pending().size() << " entries: " << stopped_reason << '\n';
    }
    std::cout << "Invariants: passed (chain replay, unique transactions, nonnegative balances, conserved funds)\n";
    return stopped_reason.empty() ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}
