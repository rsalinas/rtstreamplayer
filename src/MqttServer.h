#pragma once

#include <string>
#include "Mosquitto.h"

class MqttServer : public Mosquitto::Listener {
public:
    class Listener {
    public:
        virtual std::string runCommand(const std::string& clientId, const std::string& cmdline) = 0;
    };

    MqttServer();
    ~MqttServer();
    void start(Listener& listener);
    void pleaseFinish() {
        running_ = false;

    }

    void parameterChanged(const std::string& param, const std::string& value);
    void setServerStatus(const std::string& str);
    void onMessage(const std::string& topic, const std::string& value) override;

private:
    Mosquitto mosquitto_;
    Listener* listener_ = nullptr;
    volatile bool running_ = true;
};
