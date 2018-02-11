#include "MqttClient.h"

#include <iostream>
#include <string>
#include "logging.h"
#include "string_utils.h"

using namespace std;

MqttClient::MqttClient(const std::string& serverPrefix, Listener& listener, const char* host, int port)
    : serverPrefix_(serverPrefix)
    , listener_(listener)
    , mosquitto_((serverPrefix+"_client").c_str(), host, port, &running_) {
    mosquitto_.setListener(*this);
    mosquitto_.subscribe(serverPrefix_ + "/response/#", [this](std::string topic, std::string value) {
        try {
            auto ss = splitString(topic, '/');
            if (startsWith(topic, serverPrefix_+ "/response/") && ss.size() == 3) {
                listener_.messageForUser(ss[2], value);
                return;
            }
        } catch (...) {
            LOG_WARN() << "Exception writing to telegram";
        }
    });
    mosquitto_.subscribe(serverPrefix_ + "/state", [this](std::string topic, std::string value) {
        listener_.statusChanged(value);

    });
    mosquitto_.subscribe(serverPrefix_ + "/commands",  [this](std::string topic, std::string value) {
        listener_.setCommands(splitString(value, ','));
    });
    mosquitto_.subscribe(serverPrefix_ + "/params",  [this](std::string topic, std::string value) {
        LOG_DEBUG() << "PARAMETROS: " << topic << ":" << value ;
        listener_.setParams(value);
    });

}

MqttClient::~MqttClient() {
    running_= false;
}

void MqttClient::run() {
    while (running_) {
        mosquitto_.run(1000);
    }
}

void MqttClient::runCommand(int64_t clientId, const std::string& cmdline) {
    LOG_DEBUG() << __FUNCTION__ << clientId << ": " << cmdline ;
    mosquitto_.sendMessage((serverPrefix_ + "/cmd/" +to_string(clientId)).c_str(), cmdline);
}

void MqttClient::onMessage(const std::string& topic, const std::string& value) {
    clog << __FUNCTION__ << ": " << topic << " -> " << value << endl;
}
