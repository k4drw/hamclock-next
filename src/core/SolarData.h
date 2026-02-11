#pragma once

#include <chrono>
#include <mutex>

struct SolarData {
    int sfi = 0;
    int k_index = 0;
    int a_index = 0;
    int sunspot_number = 0;
    std::chrono::system_clock::time_point last_updated{};
    bool valid = false;
};

class SolarDataStore {
public:
    SolarData get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }

    void set(const SolarData& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_ = data;
    }

private:
    mutable std::mutex mutex_;
    SolarData data_;
};
