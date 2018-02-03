#include "Mosquitto.h"

#include "mosquitto.h"
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <sstream>

using namespace std;

Mosquitto::Mosquitto(const char* name, const char* mqtt_host, int mqtt_port, volatile bool* keepGoing)
    : run_(keepGoing) {
    static auto libInit = mosquitto_lib_init();
    mosq = mosquitto_new(name, this);
    if (!mosq)    {
        clog << "mosquitto_new failed" << endl;
        return;
    }

    mosquitto_connect_callback_set(mosq, [](void *obj, int result) {
        auto self = static_cast<Mosquitto*>(obj);
        clog << "Connected: " << result << endl;
    });
    mosquitto_message_callback_set(mosq, [](void *obj, const struct mosquitto_message *message) {
        auto self = static_cast<Mosquitto*>(obj);
        std::string msg{(char*)message->payload, message->payloadlen - 1};
        printf("got a message '%.*s' for topic '%s'\n", (int) msg.size(), msg.c_str(), message->topic);
        fflush(stdout);
        if (self->listener_) {
            self->listener_->onMessage(message->topic,msg);
        }
    });

    int rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60, true);
    clog << "mosquitto_connect: " << rc << endl;
    if (rc != MOSQ_ERR_SUCCESS)  {
        clog << "COULT NOT CONNECT!" << endl;
        mosquitto_destroy((mosq));
        mosq = nullptr;
    }

}

bool Mosquitto::subscribe(const std::string& topic) {
    if (!mosq) {
        clog << "null mosq in " << __FUNCTION__ << endl;
        return false;
    }
    uint16_t messageId;
    int rc= mosquitto_subscribe(mosq, &messageId, topic.c_str(), 0);
    clog << "subscribed " << rc << " mid == " << messageId << endl;
}


bool Mosquitto::run(int timeoutMs) {
    if (!mosq) {
        clog << "null mosq in " << __FUNCTION__  << endl;
        return false;
    }
    while (run_ == nullptr || *run_) {
        int rc = mosquitto_loop(mosq, timeoutMs);
        clog << "loop..." << rc << endl;

        if((run_==nullptr || *run_) && rc != MOSQ_ERR_SUCCESS){
            const char* errstr = strerror(errno);
            clog << "Reconnection error " << rc << " " << errstr << endl;
            sleep(10);
            mosquitto_reconnect(mosq);
        }
    }
    return 0; //FIXME
}

Mosquitto::~Mosquitto() {
    if (mosq)
        mosquitto_destroy(mosq);
    //    mosquitto_lib_cleanup();

}

bool Mosquitto::sendMessage(const char * topic, const std::string& value, bool retained) {
    uint16_t messageId;
    auto payload = (const uint8_t*) value.c_str();
    int rc = mosquitto_publish(mosq, &messageId, topic, value.size() + 1, payload, 0, retained);
    clog << "messageId " << messageId << endl;
}

bool Mosquitto::sendMessage(const std::string& topic, const std::string& value, bool retained) {
    return sendMessage(topic.c_str(), value);
}

vector<string> splitString(const string& s, char delim) {
    vector<string> result;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

bool startsWith(const std::string& hay, const std::string& needle) {
    return  hay.compare(0, needle.length(), needle) == 0;
}
