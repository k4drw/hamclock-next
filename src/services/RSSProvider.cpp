#include "RSSProvider.h"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace {

struct FeedInfo {
    const char* url;
    const char* name;
};

static constexpr FeedInfo kFeeds[] = {
    // 1. HamWeekly (Atom format)
    {"https://daily.hamweekly.com/atom.xml", "HamWeekly"},
    // 2. ARNewsLine (RSS format)
    {"https://www.arnewsline.org/?format=rss", "ARNewsLine"},
    // 3. NG3K (HTML table of DX expeditions)
    {"https://www.ng3k.com/Misc/adxo.html", "NG3K"},
};
static constexpr int kNumFeeds = sizeof(kFeeds) / sizeof(kFeeds[0]);

// Strip <![CDATA[...]]> wrapper if present.
std::string stripCDATA(const std::string& s)
{
    if (s.size() > 12 && s.substr(0, 9) == "<![CDATA[") {
        return s.substr(9, s.size() - 12);
    }
    return s;
}

// Remove HTML/XML tags from a string.
std::string stripTags(const std::string& s)
{
    std::string result;
    bool inTag = false;
    for (char c : s) {
        if (c == '<') { inTag = true; continue; }
        if (c == '>') { inTag = false; continue; }
        if (!inTag) result += c;
    }
    return result;
}

// Collapse runs of whitespace (including newlines) to single spaces, trim ends.
std::string collapse(const std::string& s)
{
    std::string result;
    bool lastSpace = true; // trims leading whitespace
    for (char c : s) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ') {
            if (!lastSpace) result += ' ';
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

// Extract <title> text from blocks delimited by startTag/endTag.
// Works for both RSS (<item>) and Atom (<entry>) feeds.
std::vector<std::string> extractTitles(const std::string& body,
                                       const char* startTag,
                                       const char* endTag)
{
    std::vector<std::string> titles;
    std::string::size_type pos = 0;
    const auto startLen = std::char_traits<char>::length(startTag);
    const auto endLen   = std::char_traits<char>::length(endTag);

    while (pos < body.size()) {
        auto blockStart = body.find(startTag, pos);
        if (blockStart == std::string::npos) break;
        auto blockEnd = body.find(endTag, blockStart);
        if (blockEnd == std::string::npos) break;

        auto titleStart = body.find("<title>", blockStart);
        if (titleStart != std::string::npos && titleStart < blockEnd) {
            titleStart += 7;
            auto titleEnd = body.find("</title>", titleStart);
            if (titleEnd != std::string::npos && titleEnd <= blockEnd) {
                std::string t = body.substr(titleStart, titleEnd - titleStart);
                t = stripCDATA(t);
                t = stripTags(t);
                t = collapse(t);
                if (!t.empty()) titles.push_back(t);
            }
        }
        pos = blockEnd + endLen;
        (void)startLen; // used only for clarity
    }
    return titles;
}

// Parse Atom feed: <entry>...<title>...</title>...</entry>
std::vector<std::string> parseAtom(const std::string& body)
{
    return extractTitles(body, "<entry>", "</entry>");
}

// Parse RSS feed: <item>...<title>...</title>...</item>
std::vector<std::string> parseRSS(const std::string& body)
{
    return extractTitles(body, "<item>", "</item>");
}

// Parse NG3K HTML page: extract DX expedition rows from table.
std::vector<std::string> parseNG3K(const std::string& body)
{
    std::vector<std::string> headlines;
    std::string::size_type pos = 0;

    while (pos < body.size() && headlines.size() < 15) {
        auto rowStart = body.find("<tr", pos);
        if (rowStart == std::string::npos) break;
        auto tagEnd = body.find(">", rowStart);
        if (tagEnd == std::string::npos) break;
        auto rowEnd = body.find("</tr>", tagEnd);
        if (rowEnd == std::string::npos) break;

        std::string rowContent = body.substr(tagEnd + 1, rowEnd - tagEnd - 1);
        std::string text = stripTags(rowContent);
        text = collapse(text);

        // Keep rows with enough substance (skip headers / empty rows)
        if (text.size() > 15) {
            headlines.push_back(text);
        }

        pos = rowEnd + 5;
    }
    return headlines;
}

// Shared state that accumulates headlines from all feeds.
// Each callback updates its own slot; the combined list is pushed to the store.
struct FeedAggregator {
    std::mutex mutex;
    std::vector<std::string> perFeed[kNumFeeds];
    std::shared_ptr<RSSDataStore> store;

    explicit FeedAggregator(std::shared_ptr<RSSDataStore> s) : store(std::move(s)) {}

    void update(int idx, std::vector<std::string> headlines) {
        std::lock_guard<std::mutex> lock(mutex);
        perFeed[idx] = std::move(headlines);

        RSSData data;
        for (int i = 0; i < kNumFeeds; ++i)
            for (const auto& h : perFeed[i])
                data.headlines.push_back(h);

        if (data.headlines.empty()) {
            data.headlines = {
                "HamClock-Next: A modern amateur radio dashboard",
                "Welcome to HamClock -- real-time propagation and space weather",
            };
        }

        data.lastUpdated = std::chrono::system_clock::now();
        data.valid = true;
        store->set(data);
    }
};

} // namespace

RSSProvider::RSSProvider(NetworkManager& net, std::shared_ptr<RSSDataStore> store)
    : net_(net)
    , store_(std::move(store))
{
}

void RSSProvider::fetch()
{
    auto agg = std::make_shared<FeedAggregator>(store_);

    // Feed 0: HamWeekly (Atom)
    net_.fetchAsync(kFeeds[0].url, [agg](std::string body) {
        auto headlines = parseAtom(body);
        std::fprintf(stderr, "RSSProvider: %s -> %zu headlines\n",
                     kFeeds[0].name, headlines.size());
        agg->update(0, std::move(headlines));
    });

    // Feed 1: ARNewsLine (RSS)
    net_.fetchAsync(kFeeds[1].url, [agg](std::string body) {
        auto headlines = parseRSS(body);
        std::fprintf(stderr, "RSSProvider: %s -> %zu headlines\n",
                     kFeeds[1].name, headlines.size());
        agg->update(1, std::move(headlines));
    });

    // Feed 2: NG3K (HTML)
    net_.fetchAsync(kFeeds[2].url, [agg](std::string body) {
        auto headlines = parseNG3K(body);
        std::fprintf(stderr, "RSSProvider: %s -> %zu headlines\n",
                     kFeeds[2].name, headlines.size());
        agg->update(2, std::move(headlines));
    });
}
