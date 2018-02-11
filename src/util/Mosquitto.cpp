#include "Mosquitto.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sstream>
#include "logging.h"
#include <iostream>

using namespace std;

Mosquitto::Mosquitto(const char* name, const char* mqtt_host, int mqtt_port, volatile bool* keepGoing)
    : run_(keepGoing) {
    clog << "Connecting to " << mqtt_host << ":" << mqtt_port << " with name " << name << endl;
    static auto libInit = mosquitto_lib_init();
    (void) libInit;
    mosq = mosquitto_new(name, false/* TODO */, this);
    if (!mosq)    {
        LOG_ERROR() << "mosquitto_new failed";
        return;
    }
    mosquitto_threaded_set(mosq, true);
    mosquitto_connect_callback_set(mosq, [](struct mosquitto*, void *obj, int result) {
        //        auto self = static_cast<Mosquitto*>(obj);
        LOG_INFO() << "Connected: " << result;
    });
    mosquitto_message_callback_set(mosq, [](struct mosquitto*, void *obj, const struct mosquitto_message *message) {
        auto self = static_cast<Mosquitto*>(obj);
        std::string msg{(char*)message->payload, (size_t) message->payloadlen};
        printf("got a message '%.*s' for topic '%s'\n", (int) msg.size(), msg.c_str(), message->topic);
        fflush(stdout);

        for (const auto& pair : self->handlers_) {
            if (regex_match(message->topic, pair.first)) {
                pair.second(message->topic, msg);
                return;
            }
        }
        if (self->listener_) {
            self->listener_->onMessage(message->topic,msg);
        }

    });

    int rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);
    LOG_INFO() << "mosquitto_connect: " << rc;
    if (rc != MOSQ_ERR_SUCCESS)  {
        LOG_ERROR() << "COULD NOT CONNECT!";
        mosquitto_destroy((mosq));
        mosq = nullptr;
    }

}

bool Mosquitto::subscribe(const std::string& topic, std::function<void(std::string,std::string)> f) {
    if (!mosq) {
        LOG_ERROR() << "null mosq in " << __FUNCTION__;
        return false;
    }
    int messageId;

    int rc= mosquitto_subscribe(mosq, &messageId, topic.c_str(), 0);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR() << "Error subscribing";
        return false;
    }
    LOG_DEBUG() << "subscribed " << rc << " topic: " << topic << " mid == " << messageId;
    static auto sharpSign = boost::regex{"#"};
    static auto plusSign = boost::regex{"\\+"};
    auto topic2 = regex_replace(topic, sharpSign, std::string{".*"}) ;
    topic2 = regex_replace(topic2, plusSign, std::string{"[^/]*"});

    LOG_DEBUG() << "TOPIC2: " << topic2 << " from " << topic;
    handlers_.push_back(make_pair(boost::regex{"^" + topic2 + "$"}, f));
    return true;
}

bool Mosquitto::run(int timeoutMs) {
    if (!mosq) {
        LOG_ERROR() << "null mosq in " << __FUNCTION__;
        return false;
    }
    while (run_ == nullptr || *run_) {
        int rc = mosquitto_loop(mosq, timeoutMs, 5 /* TODO MAX PACKETS */);
        //        clog << "loop..." << rc << endl;

        if((run_==nullptr || *run_) && rc != MOSQ_ERR_SUCCESS){
            const char* errstr = strerror(errno);
            LOG_WARN() << "Reconnection error " << rc << " " << errstr;
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
    LOG_DEBUG() << __FUNCTION__ << " " << topic << ": " << value;
    int messageId;
    auto payload = (const uint8_t*) value.c_str();
    int rc = mosquitto_publish(mosq, &messageId, topic, value.size(), payload, 0, retained);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR() << "Error sending message";
        return false;
    }
    LOG_INFO() << "messageId " << messageId;
    return true;
}

bool Mosquitto::sendMessage(const std::string& topic, const std::string& value, bool retained) {
    return sendMessage(topic.c_str(), value, retained);
}


