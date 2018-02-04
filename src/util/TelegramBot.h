#pragma once

#include <tgbot/tgbot.h>
#include "Properties.h"

class TelegramBot {
public:
    class Listener {
    public:
        virtual void runCommand(int64_t clientId, const std::string& cmdline) = 0;
    };

    TelegramBot(const Properties& props, Listener& listener);

    bool sendMessageToSubscribed(const std::string& msg);
    bool sendMessageToUser(const std::string& user, const std::string& message);
    void setCommands(const std::vector<std::string>& cmds);
    void run();
    void setServerStatus(const std::string& str);
    void pleaseFinish() {
        running_ = false;
    }
    std::string getHelp();

private:
    const Properties& props_;
    Listener& listener_;
    TgBot::Bot bot;
    volatile bool running_ = true;
    std::vector<std::string> commandNames_;
};
