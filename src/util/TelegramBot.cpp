#include "TelegramBot.h"

#include <iostream>
#include "logging.h"
#include "popen.h"
#include "string_utils.h"

using namespace std;

static std::string myVersion() {
    return execCmd(string{"stat -L /proc/"}+to_string(getpid())+"/exe  --format %y");
}

void TelegramBot::registerCommand(const std::string& name, const std::string& desc, const TgBot::EventBroadcaster::MessageListener& listener) {
    myCommands_ += "/" + name;
    if (desc.size()) {
        myCommands_.append(" - ").append(desc);
    }
    myCommands_.append("\n");
    bot.getEvents().onCommand(name, listener);
}
TelegramBot::~TelegramBot() {
    LOG_INFO() << __FUNCTION__;
}

TelegramBot::TelegramBot(const Properties& props, Listener& listener)
    : props_(props), listener_(listener)
    , bot(props_.getString("tgbot.token", "")) {
    registerCommand("start", "Start command", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Welcome!");
        bot.getApi().sendMessage(message->chat->id, getHelp());
    });
    bot.getEvents().onUnknownCommand([this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Bad command: " + message->text);
    });

    registerCommand("help", "Show this help", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, getHelp());
    });


    registerCommand("ip", "Show server's IP", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, execCmd("curl https://api.ipify.org"));
    });
    registerCommand("bot_version", "Show this software's version", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, myVersion());
    });
    registerCommand("quitbot", "Stop the Telegram bot", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Ignored");
        //        running_ = false;
    });
    registerCommand("set", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "set param value", false, message->messageId);
    });
    registerCommand("params", "Get parameters", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, paramHelp_);
    });
    registerCommand("get", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, std::string{"get param -> "}  + message->text );
    });

    registerCommand("subscribe", "Subscribe to events", [this](TgBot::Message::Ptr message) {
        subscribe(message->chat->id);
        bot.getApi().sendMessage(message->chat->id, string{"Subscribed. Total subscriptors: "} + to_string(subscriptionManager_.getSubscriptors().size()));
        subscriptionManager_.persist();
    });

    registerCommand("unsubscribe", "Subscribe to events", [this](TgBot::Message::Ptr message) {
        unsubscribe(message->chat->id);
        subscriptionManager_.persist();
        bot.getApi().sendMessage(message->chat->id, string{"Unsubscribed. Total subscriptors: "} + to_string(subscriptionManager_.getSubscriptors().size()));
    });

    bot.getEvents().onNonCommandMessage([this](TgBot::Message::Ptr message) {
        if (message->audio) {
            clog << "It is an audio!"<<endl;
            bot.getApi().sendMessage(message->chat->id, "Thanks for your audio, doing nothing");

        }
        printf("User wrote %s\n", message->text.c_str());
        if (StringTools::startsWith(message->text, "/start")) {
            return;
        }
        bot.getApi().sendMessage(message->chat->id, "Your message is: " + message->text);
    });
    try {
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        sendMessageToSubscribed("Telegram bot started. Version: " + myVersion());

    } catch (TgBot::TgException& e) {
        printf("error: %s\n", e.what());
    }

}

bool TelegramBot::subscribe(int64_t id) {
    subscriptionManager_.add(std::to_string(id));
    return true;
}

bool TelegramBot::unsubscribe(int64_t id) {
    subscriptionManager_.remove(std::to_string(id));
    return true;
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

void TelegramBot::setParams(const std::string& paramEncoded) {
    paramHelp_.clear();
    for (auto line : splitString(paramEncoded, '\n')) {
        auto ss = splitString(line, '|');
        if (ss.size() <2) {
            clog << "skipping bad line: " << line << " " << ss.size() << endl;
            continue;
        }
        paramHelp_ += ss[0] + " - " +  ss[1];
        if (ss.size() == 3) {
            paramHelp_ += " " + paren(ss[2]);
        }
        paramHelp_ += "\n";
    }
    clog << "END" <<endl <<endl;

}

void TelegramBot::run() {
    TgBot::TgLongPoll longPoll(bot);
    while (running_) {
        LOG_DEBUG() << "Long poll started";
        try {
            longPoll.start();
        } catch (const TgBot::TgException& e) {
            clog << "Exception in TelegramBot::run(): " << e.what() << endl;
            throw;
        } catch (...) {
            LOG_WARN() << "Exception";
            throw;
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
    std::vector<uint64_t> ids;
    for (auto str : subscriptionManager_.getSubscriptors()) {
        ids.push_back(std::stoul(str));
    }
    for (auto id :  ids) { //FIXME
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
    help.append(myCommands_).append("\n");
    for (const auto& cmd : commandNames_) {
        help.append("/").append(cmd).append("\n");
    }
    return help;
}


