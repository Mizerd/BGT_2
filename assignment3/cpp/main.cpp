#include "merkle.hpp"
#include "models.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
  try {
    bool compare = false;
    std::vector<std::string> txids;
    for (int index = 1; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--compare-custom") {
        compare = true;
      } else if (argument == "--file") {
        if (++index == argc) {
          throw std::invalid_argument("--file needs a txid text file");
        }
        std::ifstream input(argv[index]);
        if (!input) {
          throw std::invalid_argument("cannot open txid file");
        }
        std::string txid;
        while (input >> txid) {
          txids.push_back(txid);
        }
      } else {
        txids.push_back(argument);
      }
    }
    const auto root = bitcoin_lab::merkle_root(txids);
    if (!compare) {
      std::cout << root << '\n';
      return 0;
    }
    std::vector<blockchain::Transaction> transactions;
    for (const auto& txid : txids) {
      transactions.push_back({"", "", 0, 0, txid});
    }
    const auto educational = blockchain::merkle_root(transactions);
    std::cout << "bitcoin_sha256d=" << root << "\neducational_custom=" << educational
              << "\ndifferent=" << (root != educational ? "true" : "false") << '\n';
    return root != educational ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}
