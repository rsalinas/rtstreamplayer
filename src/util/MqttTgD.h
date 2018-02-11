#include "MqttClient.h"
#include "TelegramBot.h"
#include "logging.h"
#include <iostream>
#include <thread>
#include "SignalHandler.h"

using namespace std;

struct MqttTgD : public MqttClient::Listener, TelegramBot::Listener {
    const Properties props_;
    MqttClient mc;
    TelegramBot bot;

    void runCommand(int64_t clientId, const std::string& cmdline) override;

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

    void setParams(const std::string& paramEncoded) override {
        clog << "Params are: " << paramEncoded;
        bot.setParams(paramEncoded);
    }

    MqttTgD(const char* serverPrefix, const char* configFile);
    ~MqttTgD();

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
