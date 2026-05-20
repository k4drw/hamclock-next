#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace StringUtils {

// Extract an XML attribute value from a tag string.
// Finds attr="value" pattern and returns the value.
// Returns empty string if attribute not found.
// Example: extractAttr("<tag foo=\"bar\" />", "foo") returns "bar"
std::string extractAttr(const std::string &tag, const char *attr);

// Lightweight CSV helper: splits a line into fields, handling quotes.
// Returns a vector of views into the original string.
std::vector<std::string_view> splitCSVLineSV(std::string_view line);

// Version that takes a callback to avoid vector allocation entirely.
void splitCSVLineSV(std::string_view line, std::function<void(std::string_view)> callback);

// Safely convert a string to a double, returning 0.0 on failure.
double safe_stod(const std::string &s);
double safe_stod(std::string_view s);

// Safely convert a string to a float, returning 0.0f on failure.
float safe_stof(const std::string &s);
float safe_stof(std::string_view s);

// Safate convert a string to an int, returning 0 on failure.
int safe_stoi(const std::string &s);
int safe_stoi(std::string_view s);

// Safely convert a string to a long long, returning 0 on failure.
long long safe_stoll(const std::string &s);

// Convert a string to lowercase.
std::string toLower(const std::string &s);

// Trim whitespace from both ends of a string.
std::string trim(const std::string &s);
std::string_view trimSV(std::string_view s);

// Unescape common HTML entities (&amp;, &lt;, etc.)
std::string unescapeHtml(const std::string &s);

} // namespace StringUtils
