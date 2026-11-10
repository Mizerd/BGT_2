#include "merkle.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

int checks = 0;

void check(bool condition, const std::string& name) {
  if (!condition) {
    throw std::runtime_error(name);
  }
  ++checks;
  std::cout << "PASS " << name << '\n';
}

bool rejects(const std::vector<std::string>& txids) {
  try {
    bitcoin_lab::merkle_root(txids);
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

std::vector<std::string> read(const char* path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("cannot open fixture");
  }
  std::vector<std::string> values;
  std::string value;
  while (file >> value) values.push_back(value);
  return values;
}

int main(int argc, char* argv[]) {
  try {
    if (argc != 4) throw std::invalid_argument("need historical IDs, regtest IDs and regtest root files");
    const std::string a = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    const std::string b = "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f";
    const std::string c = "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f";
    const std::string d = "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f";
    const std::string e = "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f";
    using bitcoin_lab::merkle_root;
    // Expected roots were calculated with explicit pairings using Python hashlib.
    check(merkle_root({a}) == a, "one leaf and leading zeros");
    check(merkle_root({a,b}) == "acbeb35a6101ca45c44d286224eab1ab9948f0199eb12eba0623d8c30cf90610", "two leaves");
    check(merkle_root({a,b,c}) == "2c44a66b6fa155d3a05a241215e9ff6fa815e0cbb21dc7234c8b1deec1f6484a", "three leaves");
    check(merkle_root({a,b,c,d,e}) == "c46185334a045bbe00826029a9516d52ba99ffa5442ea5b972d79df510934c46", "five leaves");
    check(merkle_root({a,b,c}) == merkle_root({a,b,c,c}), "odd last leaf duplicated");
    check(merkle_root({a,b,c,d,e}) == merkle_root({a,b,c,d,e,e}), "odd internal level duplicated");
    check(rejects({}), "empty input rejected");
    check(rejects({std::string(63, '0')}), "invalid hex length");
    check(rejects({std::string(64, 'g')}), "invalid hex characters");
    auto whitespace = a;
    whitespace[3] = ' ';
    check(rejects({whitespace}), "whitespace is not a hash byte");
    auto changed = a;
    changed[0] = '1';
    check(merkle_root({a,b}) != merkle_root({changed,b}), "modified leaf changes root");
    check(merkle_root({a,b}) != merkle_root({b,a}), "transaction order matters");
    check(merkle_root({a,b}).size() == 64, "32 byte result");
    const auto historical = read(argv[1]);
    const std::string expected = "f3e94742aca4b5ef85488dc37c06c3282295ffec960994b2c0d5ac2a25a95766";
    check(merkle_root(historical) == expected, "mainnet block 100000");
    auto wrong_order = historical;
    for (auto& id : wrong_order) {
      std::string reversed;
      for (std::size_t end = id.size(); end > 0; end -= 2) reversed += id.substr(end - 2, 2);
      id = reversed;
    }
    check(merkle_root(wrong_order) != expected, "wrong input byte order is detected");
    const auto regtest = read(argv[2]);
    const auto expected_regtest = read(argv[3]);
    check(regtest.size() >= 2, "regtest includes coinbase and payment");
    check(expected_regtest.size() == 1 && merkle_root(regtest) == expected_regtest[0], "regtest root matches Core");
    std::cout << checks << " checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
