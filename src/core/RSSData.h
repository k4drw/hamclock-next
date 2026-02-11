#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct RSSData {
    std::vector<std::string> headlines;
    std::chrono::system_clock::time_point lastUpdated{};
    bool valid = false;
};

class RSSDataStore {
public:
    RSSData get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }

    void set(const RSSData& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_ = data;
    }

private:
    mutable std::mutex mutex_;
    RSSData data_;
};
