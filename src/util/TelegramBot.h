#pragma once

#include <tgbot/tgbot.h>
#include <tgbot/EventBroadcaster.h>
#include "Properties.h"
#include "SubscriptionManager.h"

class TelegramBot {
public:
    class Listener {
    public:
        virtual void runCommand(int64_t clientId, const std::string& cmdline) = 0;
    };

    TelegramBot(const std::string& appname, const Properties& props, Listener& listener);
    ~TelegramBot();

    bool sendMessageToSubscribed(const std::string& msg);
    bool sendMessageToUser(const std::string& user, const std::string& message);
    void setCommands(const std::vector<std::string>& cmds);
    void setParams(const std::string& string);
    void run();
    void setServerStatus(const std::string& str);
    void pleaseFinish() {
        running_ = false;
    }
    std::string getHelp();
    void registerCommand(const std::string& name, const std::string& desc, const TgBot::EventBroadcaster::MessageListener& listener);
    void registerCommand(const std::string& name, const TgBot::EventBroadcaster::MessageListener& listener) {
        registerCommand(name, "", listener);
    }
    bool subscribe(int64_t id);
    bool unsubscribe(int64_t id);

private:
    const Properties& props_;
    Listener& listener_;
    TgBot::Bot bot;
    volatile bool running_ = true;
    std::vector<std::string> commandNames_;
    std::string paramHelp_;
    std::string myCommands_;
    SubscriptionManager subscriptionManager_;
};
