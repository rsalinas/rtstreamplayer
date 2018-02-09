#pragma once

#include <vector>
#include <string>
#include <map>

class SubscriptionManager {
public:
      SubscriptionManager(const std::string& filename = "subscriptors.txt");
      ~SubscriptionManager();
      void add(const std::string& id);
      void remove(const std::string& id);
      bool contains(const std::string& id);
      std::vector<std::string> getSubscriptors() const;
      bool persist();

private:
      std::map<std::string, std::string> map_;
      std::string filename_;
      bool changed_ = false;
};
