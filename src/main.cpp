#include "chain.hpp"
#include "hashing.hpp"

#include <charconv>
#include <array>
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
  std::uint64_t attempt_budget = 1000;
  std::uint64_t max_attempts = 1000000;
};

void print_usage() {
  std::cout << "Usage: blockchain [--users N] [--transactions N] [--seed N] [--difficulty N]\n"
            << "                  [--attempt-budget N] [--max-attempts N]\n"
            << "Defaults: users=1000 transactions=10000 seed=20261029 difficulty=3\n"
            << "Mining: attempt-budget=1000 per round, max-attempts=1000000 per candidate per height\n"
            << "Users >= 2; transactions and seed >= 0; difficulty 0..64; mining budgets >= 1.\n";
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
        option != "--difficulty" && option != "--attempt-budget" && option != "--max-attempts") {
      throw std::invalid_argument("unknown option: " + std::string(option));
    }
    if (++index == argc) {
      throw std::invalid_argument("missing value for " + std::string(option));
    }
    const auto value = parse_number(argv[index]);
    if (option == "--seed") {
      options.seed = value;
    } else if (option == "--attempt-budget") {
      options.attempt_budget = value;
    } else if (option == "--max-attempts") {
      options.max_attempts = value;
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
  if (options.attempt_budget == 0 || options.max_attempts == 0) {
    throw std::invalid_argument("mining budgets must be positive");
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

void print_competition(std::size_t height, const StepResult& result) {
  std::cout << "Competition height=" << height << " | rounds=" << result.competition.rounds << " | winner=";
  if (result.competition.winner >= 0) {
    std::cout << result.competition.winner + 1;
  } else {
    std::cout << "none";
  }
  std::cout << " | accepted_height=";
  if (result.block_accepted) {
    std::cout << height;
  } else {
    std::cout << "none";
  }
  std::cout << '\n';
  for (std::size_t index = 0; index < result.candidates.size(); ++index) {
    const auto& candidate = result.candidates[index];
    std::cout << "  Candidate " << index + 1 << " | transactions=" << candidate.block.transactions.size()
              << " | attempts=" << candidate.mining.attempts
              << " | mining=" << candidate.mining.duration_seconds << "s"
              << " | found=" << (candidate.found ? "yes" : "no") << '\n';
  }
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
    std::size_t competitions = 0;
    std::size_t rounds = 0;
    MiningStats total_mining{};
    std::array<std::size_t, 5> wins{};
    std::cout << std::fixed << std::setprecision(6);
    while (!chain.pending().empty()) {
      const auto height = chain.blocks().size();
      const auto result = chain.process_next(random, options.attempt_budget, options.max_attempts);
      ++competitions;
      rounds += result.competition.rounds;
      for (const auto& candidate : result.candidates) {
        total_mining.attempts += candidate.mining.attempts;
        total_mining.duration_seconds += candidate.mining.duration_seconds;
      }
      print_competition(height, result);
      if (result.block_accepted) {
        ++wins[static_cast<std::size_t>(result.competition.winner)];
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
              << "\nCompetitions: " << competitions
              << "\nMining rounds: " << rounds
              << "\nTotal mining attempts: " << total_mining.attempts
              << "\nTotal mining duration: " << total_mining.duration_seconds << "s"
              << "\nInitial total balance: " << total_balance(chain.initial_balances())
              << "\nFinal total balance: " << total_balance(chain.balances())
              << "\nTotal execution time: " << duration << "s\n";
    std::cout << "Candidate wins:";
    for (std::size_t index = 0; index < wins.size(); ++index) {
      std::cout << ' ' << index + 1 << '=' << wins[index];
    }
    std::cout << '\n';
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
