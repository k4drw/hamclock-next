#pragma once

#include <functional>
#include <string>
#include <vector>

/// A single FCC ULS license entry parsed from the search results page.
struct FccLicense {
  std::string callsign;
  std::string service;
  std::string status;
  std::string expiry; // YYYY-MM-DD format
};

/// Provides FCC ULS license lookups by FRN (FCC Registration Number).
/// Uses the same 2-step GET/POST session approach as the FCC ULS web UI.
/// Results are delivered via callback on a background thread.
class FccProvider {
public:
  using ResultCb = std::function<void(std::vector<FccLicense>)>;

  FccProvider() = default;

  /// Look up all licenses under the given FRN.
  /// @param frn   FRN string, e.g. "0032974487"
  /// @param onDone  Called on completion (possibly on a background thread)
  ///               with the list of parsed licenses.  Empty list on failure.
  void lookupFrn(const std::string &frn, ResultCb onDone);
};
