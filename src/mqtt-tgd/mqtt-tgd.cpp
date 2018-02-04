#include "MqttClient.h"
#include "TelegramBot.h"

#include <iostream>
#include <thread>

using namespace std;

struct Main : public MqttClient::Listener, TelegramBot::Listener {
    const Properties props_;
    MqttClient mc;
    TelegramBot bot;

    void runCommand(int64_t clientId, const std::string& cmdline) override {
        mc.runCommand(clientId, cmdline);
    }
    void messageForUser(const std::string& user, const std::string& message) override {
        bot.sendMessageToUser(user, message);
    }

    void statusChanged(const std::string& value) override {
        clog << "Status changed!" << value << endl;
        bot.sendMessageToSubscribed("Status: " + value);
    }

    void setCommands(const std::vector<std::string>& cmds) override {
        clog << "Commands: " << endl;
        for (const auto cmd : cmds) {
            clog << "  " << cmd << endl;
        }
        bot.setCommands(cmds);
        clog << endl;
    }

    Main()
        : props_("config.props")
        , mc("rtsp", *this)
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
        return EXIT_SUCCESS;
    }

private:
    std::thread mqttThread_;
    std::thread botThread_;
};

int main() {
    return Main{}.run();
}
