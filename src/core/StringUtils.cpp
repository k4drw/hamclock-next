#include "StringUtils.h"
#include <charconv>
#include <string_view>

namespace StringUtils {

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
  double value = 0.0;
  std::string_view sv(s);
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
  if (ec == std::errc()) {
    return value;
  }
  return 0.0;
}

float safe_stof(const std::string &s) {
  if (s.empty()) {
    return 0.0f;
  }
  float value = 0.0f;
  std::string_view sv(s);
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
  if (ec == std::errc()) {
    return value;
  }
  return 0.0f;
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

} // namespace StringUtils
