#include "MqttServer.h"

#include <iostream>
#include <map>

using namespace std;

MqttServer::MqttServer(const std::string& serverPrefix)
    : serverPrefix_(serverPrefix)
    , mosquitto_(__FUNCTION__, "localhost", 1883, &running_) {
    mosquitto_.setListener(*this);
    mosquitto_.subscribe(serverPrefix_ + "/cmd/#", [this](std::string topic, std::string value) {
        clog << __FUNCTION__ << endl;
        auto ss = splitString(topic, '/');
        if (ss.size() != 3) {
            clog << "Bad topic: " << topic << endl;
            return;
        }
        auto newtopic = serverPrefix_ + "/response/" + ss[2];
        auto result = listener_->runCommand(ss[2], value);
        if (result.size()) {
            mosquitto_.sendMessage(newtopic, result);
        }
    });
    clog << "MqttServer connected" << endl;
}

MqttServer::~MqttServer() {
}

void MqttServer::start(Listener& listener) {
    listener_ = &listener;
    while (running_) {
        clog << __FUNCTION__ << " Still running" << endl;
        mosquitto_.run(1000);
    }
}

void MqttServer::setServerStatus(const std::string& str) {
    mosquitto_.sendMessage(serverPrefix_ + "/state", str);
}

void MqttServer::setCommandList(const std::map<std::string, CommandMeta>& cmds) {
    clog << __FUNCTION__ << endl;
    std::string keys;
    for (const auto& kv : cmds) {
        if (keys.size())
            keys.append(",");
        keys.append(kv.first);
        mosquitto_.sendMessage(serverPrefix_ + "/command/desc/" + kv.first, kv.second.desc, true);
    }
    clog << "Commands: " << keys << endl;
    mosquitto_.sendMessage(serverPrefix_ + "/commands", keys, true);
}

void MqttServer::onMessage(const std::string& topic, const std::string& value) {
    clog << __FUNCTION__  << " " << topic << " : " << value << endl;
}

