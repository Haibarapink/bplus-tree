#pragma once

#include <string>
#include <vector>

inline std::string to_string(const std::vector<char> &v) {
  return std::string{v.begin(), v.end()};
}
