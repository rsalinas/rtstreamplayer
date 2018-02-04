#include "TelegramBot.h"

#include <iostream>

using namespace std;

TelegramBot::TelegramBot(const Properties& props, Listener& listener)
    : props_(props), listener_(listener)
    , bot(props_.getString("tgbot.token", "")) {
    bot.getEvents().onCommand("start", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Welcome!");
        bot.getApi().sendMessage(message->chat->id, getHelp());
    });
    bot.getEvents().onUnknownCommand([this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Bad command: " + message->text);
    });

    bot.getEvents().onCommand("help", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, getHelp());
    });
    bot.getEvents().onCommand("set", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "set param value", false, message->messageId);
    });
    bot.getEvents().onCommand("get", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, std::string{"get param -> "}  + message->text );
    });

    bot.getEvents().onNonCommandMessage([this](TgBot::Message::Ptr message) {
        printf("User wrote %s\n", message->text.c_str());
        if (StringTools::startsWith(message->text, "/start")) {
            return;
        }
        bot.getApi().sendMessage(message->chat->id, "Your message is: " + message->text);
    });
    try {
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        //        bot.getApi().sendMessage(7654160, "Server up");
    } catch (TgBot::TgException& e) {
        printf("error: %s\n", e.what());
    }

}

void TelegramBot::setCommands(const std::vector<std::string>& cmds) {
    commandNames_ = cmds;
    for (const auto& cmd : cmds) {
        clog << "Registering command " << cmd << endl;
        bot.getEvents().onCommand(cmd, [this, cmd](TgBot::Message::Ptr message) {
            listener_.runCommand(message->chat->id, cmd);
        });
    }
}

void TelegramBot::run() {
    TgBot::TgLongPoll longPoll(bot);
    while (running_) {
        printf("Long poll started\n");
        longPoll.start();
    }

}

void TelegramBot::setServerStatus(const std::string& str) {
    try {
        sendMessageToSubscribed("State changed: " + str);
    } catch (...) {
        clog << "Exception sending to Telegram"<< endl;
    }
}

bool TelegramBot::sendMessageToSubscribed(const std::string& msg) {
    for (auto id :  { 7654160L }) { //FIXME
        auto m =  bot.getApi().sendMessage(id, msg);
    }
    return true;
}


bool TelegramBot::sendMessageToUser(const std::string& user, const std::string& message) {
    auto id = strtoll(user.c_str(), NULL, 10);
    clog << __FUNCTION__ << " tg id: " << id << " MSG: " << message << endl;
    auto m =  bot.getApi().sendMessage(id, message);
}

std::string TelegramBot::getHelp() {
    std::string help{"bot Help:\n"};
    for (const auto& cmd : commandNames_) {
        help.append("/").append(cmd).append("\n");
    }
    return help;
}

