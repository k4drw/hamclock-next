#include "PowerwallStore.h"

std::shared_ptr<const PowerwallData> PowerwallStore::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

void PowerwallStore::update(const PowerwallData& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = std::make_shared<PowerwallData>(data);
}
