#pragma once

#include "popen.h"
#include <memory>
#include "logging.h"
#include "process.h"
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <iostream>
#include <list>

#include <mutex>
#include <SDL/SDL_audio.h>
#include <SDL/SDL.h>
#include <sndfile.hh>
#include <stdexcept>
#include <unistd.h>
#include "rtstreamplayer.h"

#include "buffer.h"


using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::duration_cast;



class RtStreamPlayer {
    //const SDL_AudioSpec sdlAudioSpec;
    SimpleCommandExecutor shellExecutor;

    std::unique_ptr<Popen> inputProcess;
    std::unique_ptr<SndfileHandle> sndfile;
    enum State { WaitingForInput, Buffering, Playing };
    State state = WaitingForInput;
    decltype(steady_clock::now()) appStartTime = steady_clock::now();
    float downTime = 0;
    decltype(steady_clock::now()) startTime = steady_clock::now(), backupStarted;
    decltype(steady_clock::now()) underrunStartTime = steady_clock::now();
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
    bool backupRunning = true;

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

public:
    RtStreamPlayer();
    int run();

    void fill_audio(Uint8 *stream, int len);

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
};
