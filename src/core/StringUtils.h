#pragma once

#include <string>

namespace StringUtils {

// Extract an XML attribute value from a tag string.
// Finds attr="value" pattern and returns the value.
// Returns empty string if attribute not found.
// Example: extractAttr("<tag foo=\"bar\" />", "foo") returns "bar"
std::string extractAttr(const std::string &tag, const char *attr);

} // namespace StringUtils
