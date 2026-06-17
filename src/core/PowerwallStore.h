#pragma once

#include <mutex>
#include <memory>

struct PowerwallData {
    bool valid = false;
    double gridPower = 0.0;
    double solarPower = 0.0;
    double homePower = 0.0;
    double batteryPower = 0.0;
    double batteryLevel = 100.0;
};

class PowerwallStore {
public:
    std::shared_ptr<const PowerwallData> get() const;
    void update(const PowerwallData& data);

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const PowerwallData> data_ = std::make_shared<PowerwallData>();
};
