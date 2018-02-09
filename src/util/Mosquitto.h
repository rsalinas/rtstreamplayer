#pragma once

#include <mosquitto.h>

#include <string>
#include <vector>
#include <functional>
#include <boost/regex.hpp>
#include <vector>

class  Mosquitto {
public:
    struct Listener {
        virtual void onMessage(const std::string& topic, const std::string& value) = 0;
    };

    Mosquitto(const char* name, const char* mqtt_host, int mqtt_port = 1883, volatile bool* keepGoing = nullptr);
    ~Mosquitto();
    bool sendMessage(const char * topic, const std::string& value, bool retained = false);
    bool sendMessage(const std::string& topic, const std::string& value, bool retained = false);
    bool subscribe(const std::string& topic, std::function<void(std::string,std::string)> f);
    void setListener(Listener& listener) {
        listener_ = &listener;
    }

    bool run(int timeoutMs);

private:
    struct mosquitto *mosq = nullptr;
    volatile bool* run_;
    Listener* listener_ = nullptr;
    std::vector<std::pair<boost::regex, std::function<void(std::string,std::string)>>> handlers_;
};
