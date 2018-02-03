#include "MqttServer.h"

#include <iostream>

using namespace std;


MqttServer::MqttServer() : mosquitto_(__FUNCTION__, "localhost", 1883, &running_) {
    mosquitto_.setListener(*this);
    mosquitto_.subscribe("#");
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
    mosquitto_.sendMessage("rtsp/state", str);
}

void MqttServer::onMessage(const std::string& topic, const std::string& value) {
    clog << __FUNCTION__  << " " << topic << " : " << value << endl;
    auto ss = splitString(topic, '/');
    //    for (size_t i = 0; i < ss.size(); ++i) {
    //        clog << "chunk: " << ss[i] << endl;
    //    }
    //    clog << endl;
    if (ss.size() != 3) {
        return;
    }
    if (ss[0] == "rtsp" && ss[1] == "cmd") {
        auto topic = std::string{"rtsp/response/"} + ss[2];
        clog << "Answering to " << ss[2] << endl;
        auto result = listener_->runCommand(ss[2], value);
        mosquitto_.sendMessage(topic, result);
    }
}
