#include "NetLoggerStore.h"

std::shared_ptr<NetLoggerStore> NetLoggerStore::instance() {
    static auto inst = std::make_shared<NetLoggerStore>();
    return inst;
}

NetLoggerData NetLoggerStore::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

void NetLoggerStore::update(const NetLoggerData& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
}

void NetLoggerStore::setSelectedNet(const std::string& server, const std::string& net) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.selectedServerName = server;
    data_.selectedNetName = net;
    data_.hasCheckins = false;
    data_.checkins.clear();
}

void NetLoggerStore::getSelectedNet(std::string& server, std::string& net) const {
    std::lock_guard<std::mutex> lock(mutex_);
    server = data_.selectedServerName;
    net = data_.selectedNetName;
}
