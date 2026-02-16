#include "StringUtils.h"

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

} // namespace StringUtils
