#pragma once

#include "popen.h"
#include <memory>
#include "logging.h"
#include "process.h"

#include <condition_variable>
#include <deque>
#include <list>

#include <mutex>
#include <SDL/SDL_audio.h>
#include <SDL/SDL.h>
#include <sndfile.hh>
#include "rtstreamplayer.h"
#include "MqttServer.h"
#include "buffer.h"
#include "TelegramBot.h"
#include <thread>
#include "Properties.h"

using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::duration_cast;

class RtStreamPlayerException : public std::exception {
public:
    RtStreamPlayerException(const std::string& msg) : message_(msg) {
    }
    const char* what() const noexcept  override  {
        return message_.c_str();
    }

private:
    std::string message_;
};

class AutoJoiningThread : public std::thread {
public:
    AutoJoiningThread(MqttServer &mqttServer, MqttServer::Listener& listener) : std::thread([&] () {
        mqttServer.start(listener);
    }), mqttServer(mqttServer) {

    }
    ~AutoJoiningThread() {
        mqttServer.pleaseFinish();
        join();
    }

    MqttServer& mqttServer;
};


class RtStreamPlayer : public MqttServer::Listener {
public:
    RtStreamPlayer(const Properties& props);
    int run();
    ~RtStreamPlayer();

    void pleaseFinish() {
        mustExit = true;
    }
    void sigUsr(int sig) {
        switch (sig) {
        case SIGUSR1:
            std::unique_lock<std::mutex> lock{mutex};
            auto fbs = freeBuffers.size();
            auto rbs = readyBuffers.size();
            LOG_INFO() << "Buffer state: " << rbs << " free: " << fbs << " total: " << (fbs+rbs);
            break;
        }
    }
    struct CommandSpec {
        MqttServer::CommandMeta meta;
        std::function<std::string(std::string, RtStreamPlayer*)> func;
    };

private:
    void fill_audio(Uint8 *stream, int len);
    //const SDL_AudioSpec sdlAudioSpec;
    SimpleCommandExecutor shellExecutor;

    std::unique_ptr<Popen> inputProcess;
    std::unique_ptr<SndfileHandle> sndfile;
    enum State { WaitingForInput, Buffering, Playing };
    State state = WaitingForInput;
    steady_clock::time_point appStartTime = steady_clock::now();
    float downTime = 0;
    steady_clock::time_point startTime = steady_clock::now(), backupStarted;
    steady_clock::time_point underrunStartTime = steady_clock::now();
    steady_clock::time_point lastInput{};
    const float SYNC_TIME = 1;
    const float MIN_BUFFER_TIME = 3;
    const float MARGIN_TIME = 2;
    const float BACKUP_WAIT = 5;
    size_t minBufferCount = 0;
    int samplerate = 0, channels = 0;
    size_t underrunCount = 0;

    std::mutex mutex;
    std::condition_variable freeCondVar;
    std::deque<AudioBuffer *> readyBuffers;
    std::list<AudioBuffer *> freeBuffers;
    bool mustExit = false;
    size_t bufferSize;
    bool backupRunning = false;
    bool validPackets = false;
    bool muted_ = false;

    size_t secondsToBuffers(float seconds)
    {
        //        LOG_INFO()  << __FUNCTION__ << " " << samplerate << " " << channels ;
        return size_t( ceil(seconds * samplerate * channels * 2 /
                            bufferSize));
    }
    float buffersToSeconds(size_t buffers)
    {
        return float(buffers) * bufferSize /
                (float(sndfile->samplerate()) * sndfile->channels() * 2);
    }

    void readInput();

    std::unique_ptr<Popen> openProcess();
    std::string  runCommand(const std::string& clientId, const std::string& cmdline);

    const Properties& props_;
    bool startBackupSource();
    bool stopBackupSource();
    std::string currentStatus();
    std::string status_;
    MqttServer mqttServer;
    AutoJoiningThread mqttServerThread_;

    static const std::map<std::string, CommandSpec> commands_;
};
