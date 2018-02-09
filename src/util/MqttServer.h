#pragma once

#include <string>
#include "Mosquitto.h"
#include <map>
#include <thread>

class MqttServer : public Mosquitto::Listener {
public:
    class Listener {
    public:
        virtual std::string runCommand(const std::string& clientId, const std::string& cmdline) = 0;
    };
    struct CommandMeta {
        bool privileged;
        std::string desc;
    };


    MqttServer(const std::string& serverPrefix);
    ~MqttServer();
    void run(Listener& listener);
    void pleaseFinish() {
        running_ = false;
    }

    void parameterChanged(const std::string& param, const std::string& value);
    void setServerStatus(const std::string& str);
    void setCommandList(const std::map<std::string, CommandMeta>& list);
    void setParamList(const std::string& list);
    void onMessage(const std::string& topic, const std::string& value) override;

private:
    std::string serverPrefix_;
    Mosquitto mosquitto_;
    Listener* listener_ = nullptr;
    volatile bool running_ = true;
};

class AutoJoiningThread : public std::thread {
public:
    AutoJoiningThread(MqttServer &mqttServer, MqttServer::Listener& listener) : std::thread([&] () {
        mqttServer.run(listener);
    }), mqttServer(mqttServer) {
    }
    ~AutoJoiningThread() {
        mqttServer.pleaseFinish();
        join();
    }

    MqttServer& mqttServer;
};
