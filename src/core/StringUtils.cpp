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

std::vector<std::string_view> splitCSVLineSV(std::string_view line) {
  std::vector<std::string_view> fields;
  size_t start = 0;
  bool inQuotes = false;
  for (size_t i = 0; i < line.length(); ++i) {
    char c = line[i];
    if (c == '"') {
      inQuotes = !inQuotes;
    } else if (c == ',' && !inQuotes) {
      fields.push_back(line.substr(start, i - start));
      start = i + 1;
    }
  }
  fields.push_back(line.substr(start));
  
  // Clean up quotes from fields
  for (auto &f : fields) {
    if (f.size() >= 2 && f.front() == '"' && f.back() == '"') {
      f.remove_prefix(1);
      f.remove_suffix(1);
    }
  }
  return fields;
}

double safe_stod(const std::string &s) {
  return safe_stod(std::string_view(s));
}

double safe_stod(std::string_view s) {
  if (s.empty()) {
    return 0.0;
  }
  // Fallback for environments where std::from_chars(double) is missing (GCC < 11, MinGW, WASM)
  // strtod is exception-free and stable, but requires null-termination.
  std::string tmp(s);
  char *endptr = nullptr;
  return std::strtod(tmp.c_str(), &endptr);
}

float safe_stof(const std::string &s) {
  return safe_stof(std::string_view(s));
}

float safe_stof(std::string_view s) {
  if (s.empty()) {
    return 0.0f;
  }
  std::string tmp(s);
  char *endptr = nullptr;
  return std::strtof(tmp.c_str(), &endptr);
}

int safe_stoi(const std::string &s) {
  return safe_stoi(std::string_view(s));
}

int safe_stoi(std::string_view s) {
  if (s.empty()) {
    return 0;
  }
  int value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec == std::errc()) {
    return value;
  }
  return 0;
}

long long safe_stoll(const std::string &s) {
  if (s.empty()) {
    return 0;
  }
  long long value = 0;
  std::string_view sv(s);
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
  if (ec == std::errc()) {
    return value;
  }
  return 0;
}

std::string trim(const std::string &s) {
  return std::string(trimSV(s));
}

std::string_view trimSV(std::string_view s) {
  auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string_view::npos)
    return "";
  auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

std::string unescapeHtml(const std::string &s) {
  std::string res = s;
  static const std::pair<std::string, std::string> entities[] = {
      {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
  for (const auto &p : entities) {
    size_t pos = 0;
    while ((pos = res.find(p.first, pos)) != std::string::npos) {
      res.replace(pos, p.first.length(), p.second);
      pos += p.second.length();
    }
  }
  return res;
}

} // namespace StringUtils
