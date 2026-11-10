#pragma once

#include <string>
#include <vector>

namespace bitcoin_lab {

std::string merkle_root(const std::vector<std::string>& txids);

}
