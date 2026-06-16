#pragma once

#include "NetLoggerData.h"
#include <memory>
#include <mutex>

class NetLoggerStore {
public:
    std::shared_ptr<const NetLoggerData> get() const;
    void update(const NetLoggerData& data);
    
    void setSelectedNet(const std::string& server, const std::string& net);
    void getSelectedNet(std::string& server, std::string& net) const;

    NetLoggerStore() = default;

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const NetLoggerData> data_ = std::make_shared<NetLoggerData>();
};
