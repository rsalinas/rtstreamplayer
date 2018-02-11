#include "SubscriptionManager.h"
#include <fstream>
#include "logging.h"
using namespace std;

SubscriptionManager::SubscriptionManager(const std::string& filename) : filename_(filename) {
    LOG_DEBUG() << __FUNCTION__;
    ifstream f{filename};
    if (!f) {
        LOG_INFO() << "Could not load subscribers from " << filename;
        return;
    }
    LOG_INFO() << "Loading subscribers from " << filename;
    char buffer[1024];
    while (f.getline(buffer, sizeof buffer)) {
        map_[string{buffer}] = "";
        LOG_DEBUG()  << "Loaded subscriber " << buffer;
    }
    LOG_DEBUG() << ".";
}
bool SubscriptionManager::persist() {
    if (changed_) {
        changed_ = false;
        ofstream f{filename_};
        if (!f) {
            LOG_ERROR() << "Could NOT write subscribers to " << filename_ << ": " << strerror(errno);
        }
        for (const auto& kv : map_) {
            f << kv.first << endl;
            LOG_DEBUG() << "Written subscriber " << kv.first;
        }
        f.flush();
    }
    return true; //FIXME
}
SubscriptionManager::~SubscriptionManager() {
    try {
        persist();
    } catch (...) {
        LOG_ERROR()  << "Could not persist";
    }
}

void SubscriptionManager::add(const std::string& id) {
    map_[id] = "";
    changed_ = true;
}

void SubscriptionManager::remove(const std::string& id) {
    map_.erase(id);
    changed_ = true;
}

bool SubscriptionManager::contains(const std::string& id) {
    return map_.find(id) != map_.end();
}

std::vector<std::string> SubscriptionManager::getSubscriptors() const {
    std::vector<std::string> ret;
    for (const auto& kv : map_) {
        ret.push_back(kv.first);
    }
    return ret;
}
