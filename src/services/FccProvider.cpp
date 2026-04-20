#include "FccProvider.h"
#include "../core/Logger.h"
#include "../core/TimeUtils.h"

#if !defined(__EMSCRIPTEN__)
#include <curl/curl.h>
#include <filesystem>
#endif

#include <regex>
#include <thread>

// ─── internal helpers ────────────────────────────────────────────────────────

#if !defined(__EMSCRIPTEN__)
static size_t fccWriteCallback(void *contents, size_t size, size_t nmemb,
                               void *userp) {
  static_cast<std::string *>(userp)->append(static_cast<char *>(contents),
                                            size * nmemb);
  return size * nmemb;
}

static std::string stripTags(const std::string &html) {
  std::string out;
  out.reserve(html.size());
  bool inTag = false;
  for (char c : html) {
    if (c == '<')
      inTag = true;
    else if (c == '>')
      inTag = false;
    else if (!inTag)
      out += c;
  }
  // Collapse whitespace
  std::string result;
  result.reserve(out.size());
  bool lastSpace = false;
  for (char c : out) {
    if (c == '\r' || c == '\n' || c == '\t')
      c = ' ';
    if (c == ' ') {
      if (!lastSpace)
        result += c;
      lastSpace = true;
    } else {
      result += c;
      lastSpace = false;
    }
  }
  // Trim
  auto f = result.find_first_not_of(' ');
  if (f == std::string::npos)
    return "";
  auto l = result.find_last_not_of(' ');
  return result.substr(f, l - f + 1);
}

/// Parse MM/DD/YYYY → YYYY-MM-DD (or leave as-is if parsing fails).
static std::string normalizeDate(const std::string &s) {
  int m = 0, d = 0, y = 0;
  if (std::sscanf(s.c_str(), "%d/%d/%d", &m, &d, &y) == 3) {
    return TimeUtils::dateISO(y, m, d);
  }
  return s;
}

static std::vector<FccLicense> parseUlsHtml(const std::string &html) {
  std::vector<FccLicense> results;

  // Match each data row (align=left valign=top)
  std::regex rowRe("<tr align=\"left\" valign=\"top\">[\\s\\S]*?</tr>",
                   std::regex::icase);
  std::regex cellRe("<td[^>]*>([\\s\\S]*?)</td>", std::regex::icase);

  auto rowBegin = std::sregex_iterator(html.begin(), html.end(), rowRe);
  auto rowEnd = std::sregex_iterator();

  for (auto it = rowBegin; it != rowEnd; ++it) {
    std::string rowHtml = (*it)[0].str();

    std::vector<std::string> cells;
    auto cellBegin =
        std::sregex_iterator(rowHtml.begin(), rowHtml.end(), cellRe);
    for (auto jt = cellBegin; jt != rowEnd; ++jt) {
      cells.push_back(stripTags((*jt)[1].str()));
    }

    // FCC ULS results table: col 1=callsign, col 4=service, col 5=status,
    // col 6=expiry (0-indexed after the row-type checkbox cell)
    if (cells.size() >= 7) {
      FccLicense lic;
      lic.callsign = cells[1];
      lic.service = cells[4];
      lic.status = cells[5];
      lic.expiry = normalizeDate(cells[6]);
      if (!lic.callsign.empty())
        results.push_back(lic);
    }
  }

  return results;
}

/// Perform the 2-step GET+POST FCC ULS FRN search synchronously.
/// Returns the parsed license list.  Must be called from a background thread.
static std::vector<FccLicense> fetchFrnSync(const std::string &frn) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    LOG_E("FccProvider", "curl_easy_init failed");
    return {};
  }

  std::string htmlBuffer;

  // Full Chrome-130 browser fingerprint — FCC ULS WAF checks all of these
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(
      headers, "Accept: text/html,application/xhtml+xml,application/xml;"
               "q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,"
               "application/signed-exchange;v=b3;q=0.7");
  headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate, br");
  headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
  headers = curl_slist_append(headers, "Connection: keep-alive");
  headers = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");
  headers = curl_slist_append(headers, "Sec-Fetch-Dest: document");
  headers = curl_slist_append(headers, "Sec-Fetch-Mode: navigate");
  headers = curl_slist_append(headers, "Sec-Fetch-Site: none");
  headers = curl_slist_append(headers, "Sec-Fetch-User: ?1");

  curl_easy_setopt(curl, CURLOPT_USERAGENT,
                   "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                   "(KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fccWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &htmlBuffer);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
  // Accept-Encoding: br/gzip sent above — let libcurl decompress automatically
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  // In-memory cookie jar (empty string enables it without writing a file)
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

#if defined(__linux__)
  if (std::filesystem::exists("/etc/ssl/certs/ca-certificates.crt"))
    curl_easy_setopt(curl, CURLOPT_CAINFO,
                     "/etc/ssl/certs/ca-certificates.crt");
  else if (std::filesystem::exists("/etc/pki/tls/certs/ca-bundle.crt"))
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/pki/tls/certs/ca-bundle.crt");
#endif

  // ── Step 1: GET to establish session + cookie ────────────────────────────
  curl_easy_setopt(
      curl, CURLOPT_URL,
      "https://wireless2.fcc.gov/UlsApp/UlsSearch/searchLicense.jsp");
  CURLcode res = curl_easy_perform(curl);
  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

  if (res != CURLE_OK || httpCode != 200) {
    LOG_W("FccProvider", "Step 1 (session establish) failed: {} {}", httpCode,
          curl_easy_strerror(res));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return {};
  }

  // ── Step 2: POST the FRN query ───────────────────────────────────────────
  htmlBuffer.clear();

  // Add POST-specific headers
  headers = curl_slist_append(headers, "Origin: https://wireless2.fcc.gov");
  headers = curl_slist_append(
      headers,
      "Referer: https://wireless2.fcc.gov/UlsApp/UlsSearch/searchLicense.jsp");
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  std::string postFields = "fiUlsSearchByType=uls_l_frn"
                           "&fiUlsSearchByValue=" +
                           frn +
                           "&x=33&y=6&hiddenForm=hiddenForm&jsValidated=true";

  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://wireless2.fcc.gov/UlsApp/UlsSearch/results.jsp");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());

  res = curl_easy_perform(curl);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK || httpCode != 200 || htmlBuffer.size() < 2000) {
    LOG_W("FccProvider", "Step 2 (FRN POST) failed: {} size={}", httpCode,
          htmlBuffer.size());
    return {};
  }

  LOG_D("FccProvider", "FRN {} returned {} bytes", frn, htmlBuffer.size());
  return parseUlsHtml(htmlBuffer);
}
#endif // !__EMSCRIPTEN__

// ─── public API ──────────────────────────────────────────────────────────────

void FccProvider::lookupFrn(const std::string &frn, ResultCb onDone) {
  if (frn.empty()) {
    onDone({});
    return;
  }

#if defined(__EMSCRIPTEN__)
  // FCC ULS requires cookies + POST — not feasible from a WASM/CORS context.
  LOG_W("FccProvider", "FCC ULS lookup not supported on WASM");
  onDone({});
#else
  std::thread([this, frn, onDone = std::move(onDone), alive = alive_]() {
    {
      std::lock_guard<std::mutex> lk(inflightMutex_);
      if (!alive->load(std::memory_order_relaxed)) return;
      ++inflight_;
    }
    auto result = fetchFrnSync(frn);
    onDone(std::move(result));
    {
      std::lock_guard<std::mutex> lk(inflightMutex_);
      if (--inflight_ == 0) inflightCv_.notify_all();
    }
  }).detach();
#endif
}
