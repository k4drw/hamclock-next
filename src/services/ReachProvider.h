#pragma once

#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include "../core/LiveSpotData.h"
#include "../core/ReachData.h"
#include "../core/HamClockState.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <memory>
#include <vector>

namespace HamClock {

class ReachProvider : public ::ProviderBase, public std::enable_shared_from_this<ReachProvider> {
public:
    using Callback = std::function<void(const ReachData&)>;

    ReachProvider(NetworkManager &net, std::shared_ptr<HamClockState> state,
                  std::shared_ptr<LiveSpotDataStore> spotStore = nullptr);

    void fetch(const std::string& band, const std::string& mode);
    void setCallback(Callback cb) { callback_ = std::move(cb); }
    ReachData getData() const;

private:
    struct SpotEntry {
        double lat, lon;
        std::string callsign;
    };

    void fetchPSK(const std::string& band, const std::string& mode);
    void fetchWSPR(const std::string& band, const std::string& mode);
    void processPSK(const std::string& body, const std::string& band, const std::string& mode);
    void processWSPR(const std::string& body, const std::string& band, const std::string& mode);
    void onSourceComplete(const std::string& band, const std::string& mode);
    void applyHeatmap(const std::string& band, const std::string& mode);

    NetworkManager &net_;
    std::shared_ptr<HamClockState> state_;
    ReachData data_;
    mutable std::mutex mutex_;
    Callback callback_;

    // Multi-source merge state
    std::atomic<int> pendingFetches_{0};
    std::vector<SpotEntry> pendingSpots_;  // guarded by mutex_

    std::shared_ptr<LiveSpotDataStore> spotStore_; // optional — reuse live spot cache
};

} // namespace HamClock
