#include "MqttClient.h"
#include "../TelegramBot.h"

#include <iostream>
#include <thread>


using namespace std;

struct Main : public MqttClient::Listener, TelegramBot::Listener {
    Properties props_;
    MqttClient mc;
    TelegramBot bot;

    void cmdQuit(int64_t clientId) override {
        mc.cmdQuit(clientId);
    }
    void runCommand(int64_t clientId, const std::string& cmdline) override {
        mc.runCommand(clientId, cmdline);
    }
    void messageForUser(const std::string& user, const std::string& message) {
        bot.sendMessageToUser(user, message);
    }

    void statusChanged(const std::string& value) override {
        clog << "Status changed!" << value << endl;
        bot.sendMessageToSubscribed("Status: " + value);
    }

    Main()
        : props_("config.props")
        , mc(*this)
        , bot(props_,*this) {
        mqttThread_ = std::thread{[this]() {
            mc.run();
        }};
    }
    ~Main() {
        mqttThread_.join();
    }

    void parameterChanged(const std::string& key, const std::string& value) override {
        clog << "Param changed: " << key << " -> " << value << endl;
    }

    int run() {
        bot.run();
        return 0;
    }
    std::thread mqttThread_;
    std::thread botThread_;
};

int main() {

    return Main{}.run();
}
