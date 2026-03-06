#include "StringUtils.h"
#include <charconv>
#include <cstdlib>
#include <string_view>

#include <algorithm>
#include <cctype>

namespace StringUtils {

std::string toLower(const std::string &s) {
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

std::string extractAttr(const std::string &tag, const char *attr) {
  std::string needle = std::string(attr) + "=\"";
  auto pos = tag.find(needle);
  if (pos == std::string::npos)
    return {};
  pos += needle.size();
  auto end = tag.find('"', pos);
  if (end == std::string::npos)
    return {};
  return tag.substr(pos, end - pos);
}

double safe_stod(const std::string &s) {
  if (s.empty()) {
    return 0.0;
  }
  // Fallback for environments where std::from_chars(double) is missing (MinGW,
  // WASM) strtod is exception-free and stable.
  char *endptr = nullptr;
  double val = std::strtod(s.c_str(), &endptr);
  return val;
}

float safe_stof(const std::string &s) {
  if (s.empty()) {
    return 0.0f;
  }
  char *endptr = nullptr;
  float val = std::strtof(s.c_str(), &endptr);
  return val;
}

int safe_stoi(const std::string &s) {
  if (s.empty()) {
    return 0;
  }
  int value = 0;
  std::string_view sv(s);
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
  if (ec == std::errc()) {
    return value;
  }
  return 0;
}

std::string trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

} // namespace StringUtils
