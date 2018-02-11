#pragma once

#include <string>
#include "Mosquitto.h"

class MqttClient : public Mosquitto::Listener {
public:
    class Listener {
    public:
        virtual void parameterChanged(const std::string& key, const std::string& value) = 0;
        virtual void runCommand(int64_t clientId, const std::string& cmdline) = 0;
        virtual void statusChanged(const std::string& value) = 0;
        virtual void messageForUser(const std::string& user, const std::string& message) = 0;
        virtual void setCommands(const std::vector<std::string>& cmds) = 0;
        virtual void setParams(const std::string& paramEncoded) = 0;
    };

    void onMessage(const std::string& topic, const std::string& value) override;

    MqttClient(const std::string& serverPrefix, Listener& listener);
    ~MqttClient();

    void run();
    void cmdQuit(int64_t clientId);
    void runCommand(int64_t clientId, const std::string& cmdline);

private:
    const std::string serverPrefix_;
    Listener& listener_;
    Mosquitto mosquitto_;
    volatile bool running_ = true;
};
