#pragma once

#include "../network/NetworkManager.h"
#include "../core/ReachData.h"
#include "../core/HamClockState.h"
#include <functional>
#include <mutex>
#include <string>
#include <memory>

namespace HamClock {

class ReachProvider {
public:
    using Callback = std::function<void(const ReachData&)>;

    ReachProvider(NetworkManager &net, std::shared_ptr<HamClockState> state);

    void fetch(const std::string& band, const std::string& mode);
    void setCallback(Callback cb) { callback_ = std::move(cb); }
    ReachData getData() const;

private:
    void fetchPSK(const std::string& band, const std::string& mode);
    void processPSK(const std::string& body, const std::string& band, const std::string& mode);
    void applyHeatmap(const std::vector<std::pair<double, double>>& spots, const std::string& band, const std::string& mode);

    NetworkManager &net_;
    std::shared_ptr<HamClockState> state_;
    ReachData data_;
    mutable std::mutex mutex_;
    Callback callback_;
};

} // namespace HamClock
