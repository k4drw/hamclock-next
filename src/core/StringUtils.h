#pragma once

#include <string>

namespace StringUtils {

// Extract an XML attribute value from a tag string.
// Finds attr="value" pattern and returns the value.
// Returns empty string if attribute not found.
// Example: extractAttr("<tag foo=\"bar\" />", "foo") returns "bar"
std::string extractAttr(const std::string &tag, const char *attr);

// Safely convert a string to a double, returning 0.0 on failure.
double safe_stod(const std::string &s);

// Safely convert a string to a float, returning 0.0f on failure.
float safe_stof(const std::string &s);

// Safely convert a string to an int, returning 0 on failure.
int safe_stoi(const std::string &s);

} // namespace StringUtils
