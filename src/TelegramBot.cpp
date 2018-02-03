#include "TelegramBot.h"
#include <stdio.h>
#include <iostream>

#include <signal.h>

using namespace std;

TelegramBot::TelegramBot(const Properties& props, Listener& listener)
    : props_(props), listener_(listener)
    , bot(props_.getString("tgbot.token", "")) {
    bot.getEvents().onCommand("start", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Hi!");
    });
    bot.getEvents().onUnknownCommand([this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Bad command");
    });

    bot.getEvents().onCommand("help", [this](TgBot::Message::Ptr message) {


        bot.getApi().sendMessage(message->chat->id, "Help:\n/get param\n/set param value");
    });
    bot.getEvents().onCommand("quit", [this](TgBot::Message::Ptr message) {
        listener_.cmdQuit(message->chat->id);
    });


    bot.getEvents().onCommand("cpuinfo", [this](TgBot::Message::Ptr message) {
        listener_.runCommand(message->chat->id, "cpuinfo");
    });
    bot.getEvents().onCommand("temp", [this](TgBot::Message::Ptr message) {
        listener_.runCommand(message->chat->id, "temp");
    });
    bot.getEvents().onCommand("status", [this](TgBot::Message::Ptr message) {
        listener_.runCommand(message->chat->id, "status");
    });
    bot.getEvents().onCommand("uptime", [this](TgBot::Message::Ptr message) {
        listener_.runCommand(message->chat->id, "uptime");
    });
    bot.getEvents().onCommand("mute", [this](TgBot::Message::Ptr message) {
        listener_.runCommand(message->chat->id, "mute");
    });
    bot.getEvents().onCommand("unmute", [this](TgBot::Message::Ptr message) {
        listener_.runCommand(message->chat->id, "unmute");
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
        //        bot.getApi().sendMessage(7654160, "RtStreamPlayer up");
    } catch (TgBot::TgException& e) {
        printf("error: %s\n", e.what());
    }

}
TelegramBot::~TelegramBot() {
    clog << __FUNCTION__ << endl;
    //    bot.getApi().sendMessage(7654160, "RtStreamPlayer shutting down");

}

void TelegramBot::run() {
    TgBot::TgLongPoll longPoll(bot);
    while (running_) {
        printf("Long poll started\n");
        longPoll.start();
    }

}


void TelegramBot::setServerStatus(const std::string& str) {
    clog << "sending message: " << str << endl;
    try {
        sendMessageToSubscribed("State changed: " + str);
        //    } catch (const std::exception& e) {
        //        clog << "Exception sending to Telegram: " << e.what() << endl;
    } catch (...) {
        clog << "Exception sending to Telegram"<< endl;
    }
}

bool TelegramBot::sendMessageToSubscribed(const std::string& msg) {
    for (auto id :  { 7654160L }) {
        auto m =  bot.getApi().sendMessage(id, msg);
    }
    return true;
}


bool TelegramBot::sendMessageToUser(const std::string& user, const std::string& message) {
    auto id = strtoll(user.c_str(), NULL, 10);
    clog << __FUNCTION__ << " tg id: " << id << " MSG: " << message << endl;
    auto m =  bot.getApi().sendMessage(id, message);

}
