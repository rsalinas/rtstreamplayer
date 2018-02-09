#include "TelegramBot.h"

#include <iostream>
#include "logging.h"
#include <execinfo.h>

using namespace std;

static std::string execCmd(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}
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


    bot.getEvents().onCommand("ip", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, execCmd("curl https://api.ipify.org"));
    });
    bot.getEvents().onCommand("set", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "set param value", false, message->messageId);
    });
    bot.getEvents().onCommand("get", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, std::string{"get param -> "}  + message->text );
    });

    bot.getEvents().onNonCommandMessage([this](TgBot::Message::Ptr message) {
        if (message->audio) {
            clog << "It is an audio!"<<endl;
        }
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
        try {
        longPoll.start();
        } catch (const TgBot::TgException& e) {
            clog << "Ex: " << e.what() << endl;
        } catch (...) {
            LOG_WARN() << "Exception";
        }
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
    auto m = bot.getApi().sendMessage(id, message.substr(0, 4096));
    if (!m) {
        clog << "Error sending message" << endl;
        return false;
    }
    return true;
}

std::string TelegramBot::getHelp() {
    std::string help{"bot Help:\n"};
    for (const auto& cmd : commandNames_) {
        help.append("/").append(cmd).append("\n");
    }
    return help;
}

