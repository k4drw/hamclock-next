#include "NetLoggerStore.h"

std::shared_ptr<const NetLoggerData> NetLoggerStore::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

void NetLoggerStore::update(const NetLoggerData& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = std::make_shared<NetLoggerData>(data);
}

void NetLoggerStore::setSelectedNet(const std::string& server, const std::string& net) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto newData = std::make_shared<NetLoggerData>(*data_);
    newData->selectedServerName = server;
    newData->selectedNetName = net;
    newData->hasCheckins = false;
    newData->checkins.clear();
    data_ = newData;
}

void NetLoggerStore::getSelectedNet(std::string& server, std::string& net) const {
    std::lock_guard<std::mutex> lock(mutex_);
    server = data_->selectedServerName;
    net = data_->selectedNetName;
}
