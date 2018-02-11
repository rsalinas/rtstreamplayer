#include "MqttTgD.h"

MqttTgD::MqttTgD(const char* serverPrefix, const char* configFile)
    : props_(configFile)
    , mc(serverPrefix, *this, props_.getString("mosquitto.host", "localhost").c_str(), props_.getInt("mosquitto.port", 1883))
    , bot(serverPrefix, props_, *this) {
    mqttThread_ = std::thread{[this]() {
        mc.run();
    }};
}

MqttTgD::~MqttTgD() {
    mqttThread_.join();
}

void MqttTgD::runCommand(int64_t clientId, const std::string& cmdline) {
    mc.runCommand(clientId, cmdline);
}

