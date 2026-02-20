#include "RSSProvider.h"
#include "../core/Constants.h"
#include "../core/Logger.h"
#include "../core/WorkerService.h"
#include "SDL_events.h"

#include <string>
#include <vector>

namespace {

// ... (All the helper functions: stripCDATA, stripTags, decodeEntities,
// collapse, extractTitles, parseAtom, parseRSS, parseNG3K remain unchanged)

struct FeedInfo {
  const char *url;
  const char *name;
  std::function<std::vector<std::string>(const std::string &)> parser;
};

// Strip <![CDATA[...]]> wrapper if present.
std::string stripCDATA(const std::string &s) {
  if (s.size() > 12 && s.substr(0, 9) == "<![CDATA[") {
    return s.substr(9, s.size() - 12);
  }
  return s;
}

// Remove HTML/XML tags from a string.
std::string stripTags(const std::string &s) {
  std::string result;
  bool inTag = false;
  bool needSpace = false;
  for (char c : s) {
    if (c == '<') {
      if (!inTag && !result.empty() && result.back() != ' ')
        needSpace = true;
      inTag = true;
      continue;
    }
    if (c == '>') {
      inTag = false;
      if (needSpace) {
        result += ' ';
        needSpace = false;
      }
      continue;
    }
    if (!inTag) {
      result += c;
      needSpace = false;
    }
  }
  return result;
}

// Decode common HTML entities
std::string decodeEntities(const std::string &s) {
  std::string result;
  result.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '&' && i + 1 < s.size()) {
      size_t end = s.find(';', i + 1);
      if (end != std::string::npos && end - i <= 10) {
        std::string entity = s.substr(i + 1, end - i - 1);
        if (entity == "amp")
          result += '&';
        else if (entity == "lt")
          result += '<';
        else if (entity == "gt")
          result += '>';
        else if (entity == "quot")
          result += '"';
        else if (entity == "apos" || entity == "#39")
          result += '\'';
        else if (entity == "nbsp" || entity == "#160")
          result += ' ';
        else
          result += s.substr(i, end - i + 1);
        i = end;
      } else {
        result += s[i];
      }
    } else {
      result += s[i];
    }
  }
  return result;
}

// Collapse runs of whitespace to single spaces, trim ends.
std::string collapse(const std::string &s) {
  std::string result;
  bool lastSpace = true;
  for (char c : s) {
    if (c == '\n' || c == '\r' || c == '\t')
      c = ' ';
    if (c == ' ') {
      if (!lastSpace)
        result += ' ';
      lastSpace = true;
    } else {
      result += c;
      lastSpace = false;
    }
  }
  if (!result.empty() && result.back() == ' ')
    result.pop_back();
  return result;
}

// Extract <title> text from blocks
std::vector<std::string> extractTitles(const std::string &body,
                                       const char *startTag,
                                       const char *endTag) {
  std::vector<std::string> titles;
  std::string::size_type pos = 0;
  while (pos < body.size()) {
    auto blockStart = body.find(startTag, pos);
    if (blockStart == std::string::npos)
      break;
    auto blockEnd = body.find(endTag, blockStart);
    if (blockEnd == std::string::npos)
      break;
    auto titleStart = body.find("<title>", blockStart);
    if (titleStart != std::string::npos && titleStart < blockEnd) {
      titleStart += 7;
      auto titleEnd = body.find("</title>", titleStart);
      if (titleEnd != std::string::npos && titleEnd <= blockEnd) {
        std::string t = body.substr(titleStart, titleEnd - titleStart);
        t = stripCDATA(t);
        t = stripTags(t);
        t = decodeEntities(t);
        t = collapse(t);
        if (!t.empty())
          titles.push_back(t);
      }
    }
    pos = blockEnd + std::char_traits<char>::length(endTag);
  }
  return titles;
}

std::vector<std::string> parseAtom(const std::string &body) {
  return extractTitles(body, "<entry>", "</entry>");
}
std::vector<std::string> parseRSS(const std::string &body) {
  return extractTitles(body, "<item>", "</item>");
}
std::vector<std::string> parseNG3K(const std::string &body) {
  std::vector<std::string> headlines;
  std::string::size_type pos = 0;
  while (pos < body.size() && headlines.size() < 15) {
    auto rowStart = body.find("<tr", pos);
    if (rowStart == std::string::npos)
      break;
    auto tagEnd = body.find(">", rowStart);
    if (tagEnd == std::string::npos)
      break;
    auto rowEnd = body.find("</tr>", tagEnd);
    if (rowEnd == std::string::npos)
      break;
    std::string rowContent = body.substr(tagEnd + 1, rowEnd - tagEnd - 1);
    std::string text = stripTags(rowContent);
    text = decodeEntities(text);
    text = collapse(text);
    if (text.size() > 15) {
      headlines.push_back(text);
    }
    pos = rowEnd + 5;
  }
  return headlines;
}

static const FeedInfo kFeeds[] = {
    {"https://daily.hamweekly.com/atom.xml", "HamWeekly", parseAtom},
    {"https://www.arnewsline.org/?format=rss", "ARNewsLine", parseRSS},
    {"https://www.ng3k.com/Misc/adxo.html", "NG3K", parseNG3K},
};
static constexpr int kNumFeeds = sizeof(kFeeds) / sizeof(kFeeds[0]);

} // namespace

RSSProvider::RSSProvider(NetworkManager &net,
                         std::shared_ptr<RSSDataStore> store)
    : net_(net), store_(std::move(store)) {}

void RSSProvider::fetch() {
  if (!enabled_)
    return;

  for (int i = 0; i < kNumFeeds; ++i) {
    const auto &feed = kFeeds[i];
    net_.fetchAsync(feed.url, [feed_index = i, feed_name = feed.name,
                               parser = feed.parser](std::string body) {
      if (body.empty()) {
        LOG_W("RSSProvider", "Fetch failed for {}", feed_name);
        return;
      }

      // Offload the parsing to a worker thread
      WorkerService::getInstance().submitTask(
          [body, feed_index, feed_name, parser]() {
            LOG_D("RSSProvider", "Parsing {} on worker thread.", feed_name);
            auto *headlines = new std::vector<std::string>(parser(body));
            LOG_I("RSSProvider", "{} -> {} headlines", feed_name,
                  headlines->size());

            SDL_Event event;
            SDL_zero(event);
            event.type = HamClock::AE_BASE_EVENT + HamClock::AE_RSS_DATA_READY;
            event.user.code = feed_index;
            event.user.data1 = headlines;
            SDL_PushEvent(&event);
          });
    });
  }
}
