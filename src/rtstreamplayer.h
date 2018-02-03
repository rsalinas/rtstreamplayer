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

    void readInput() {
        LOG_DEBUG() << __FUNCTION__;

        while (!mustExit) {
            AudioBuffer *buf = nullptr;
            {
                std::unique_lock<std::mutex> lock{mutex};
                if (freeBuffers.empty() /*&& state != Playing*/) {
                    if (state == Playing) {
                        size_t dropped = 0;
                        while (readyBuffers.size() > secondsToBuffers(MIN_BUFFER_TIME)) {
                            buf = readyBuffers.back();
                            readyBuffers.pop_back();
                            freeBuffers.push_back(buf);
                            dropped++;
                        }
                        LOG_INFO() << "Drop excess packets: " << dropped;
                    }
                    buf = readyBuffers.front();
                    readyBuffers.pop_front();
                } else {
                    buf = freeBuffers.back();
                    freeBuffers.pop_back();
                }
            }
            buf->usedSamples = sndfile->read(reinterpret_cast<short *>(buf->buffer),
                                             buf->length / 2);
            buf->calculateAverage();

            //LOG_INFO()  << "read block: " << buf->usedSamples << " samples" << " " << freeBuffers.size() << " free buffers, " << readyBuffers.size() << " ready buffers" ;
            if (buf->usedSamples == 0) {
                LOG_INFO() << "EOF in input stream";
                sndfile.reset();
                inputProcess.reset();
                LOG_INFO() << "Reopening process";
                inputProcess = openProcess();
                sndfile.reset(new SndfileHandle(inputProcess->getFd(), false));
            }

            std::unique_lock<std::mutex> lock{mutex};
            readyBuffers.push_back(buf);
            if (state == WaitingForInput) {
                state = Buffering;
                LOG_INFO() << "WaitingForInput -> Buffering";
                startTime = steady_clock::now();
            }
        }
    }

    std::unique_ptr<Popen> openProcess() {
        return std::unique_ptr<Popen>(new Popen{"exec ./source.sh", "r"});
    }

public:
    RtStreamPlayer() : inputProcess(openProcess()), sndfile(new SndfileHandle(inputProcess->getFd(), false)) {
        if (!sndfile->samplerate()) {
            throw std::runtime_error("Invalid format");
        }
        samplerate = sndfile->samplerate();
        channels = sndfile->channels();

        LOG_INFO()  << "    Sample rate : %d" << samplerate;
        LOG_INFO()  <<  "    Channels    : %d" <<  channels;
        LOG_INFO()  <<  "    Format:       %0x" <<  sndfile->format();
        if (!sndfile->format() & SF_FORMAT_PCM_16) {
            throw std::runtime_error(
                        std::string("Invalid file format, PCM16 expected"));
        }
        //~ readThread = std::thread{&RtStreamPlayer::readInput, this};

        SDL_AudioSpec wanted, obtained;

        wanted.freq = sndfile->samplerate();
        wanted.format = AUDIO_S16;
        wanted.channels = sndfile->channels();
        wanted.samples = 32768; // sndfile->samplerate()*sndfile->channels()/2/2;
        int wantedSamples = wanted.samples;
        wanted.callback = [](void *udata, Uint8 *stream, int len) {
            static_cast<RtStreamPlayer *>(udata)->fill_audio(stream, len);
        };
        wanted.userdata = this;

        if (SDL_OpenAudio(&wanted, &obtained) < 0)
            throw std::runtime_error(std::string("Couldn't open audio: ") +
                                     SDL_GetError());
        LOG_INFO() << "Wanted: " << wanted << ", obtained: " << obtained;
        bufferSize = obtained.size;
        LOG_INFO()  << "Audio in a buffer: " << buffersToSeconds(1) ;

        size_t nBufs = secondsToBuffers(MIN_BUFFER_TIME + MARGIN_TIME);
        LOG_INFO()  << "secondsToBuffers : " << nBufs << " "
                    << buffersToSeconds(nBufs) ;

        for (size_t i = 0; i < nBufs; i++) {
            auto audioBuffer = new AudioBuffer(obtained.size);
            freeBuffers.push_back(audioBuffer);
        }
        LOG_INFO()  << "Buffer count: " << nBufs ;
    }

    int run() {
        LOG_INFO() << "Starting playback...";
        SDL_PauseAudio(0);
        readInput();
    }

    void fill_audio(Uint8 *stream, int len)
    {
        bool lastWasSilence = false;
        auto fillWithSilence = [stream, len, &lastWasSilence] {
            if (!lastWasSilence) {
                memset(stream, 0, len);
                lastWasSilence = true;
            }
        };
        AudioBuffer *buf = nullptr;
        auto now = steady_clock::now();
        auto fillingBufferInterval =
                duration_cast<duration<double>>(now - startTime);
        auto uptime = duration_cast<duration<double>>(now - appStartTime).count();
        {
            std::unique_lock<std::mutex> lock{mutex};
            //~ LOG_INFO()  << "fill buffer = " << readyBuffers.size() <<"  len=="<<
            // len;

            if (state == Buffering && fillingBufferInterval.count() > SYNC_TIME &&
                    readyBuffers.size() >= secondsToBuffers(MIN_BUFFER_TIME)) {
                state = Playing;
                if (backupRunning) {
                    LOG_INFO() << "Stopping backup source";
                    shellExecutor.runCommand("./restored.sh");

                    backupRunning = false;
                }


                auto audioDowntime = duration_cast<duration<double>>(now - underrunStartTime).count();
                downTime += audioDowntime;
                LOG_INFO() << "player: Buffering -> Playing. ellapsed == " << fillingBufferInterval.count();
                LOG_INFO() << "Downtime: " << downTime << " uptime: " << uptime;
            }
            if (state != Playing) {
                //~ LOG_INFO()  << "player: syncing"  ;
                fillWithSilence();
                if (startTime != backupStarted && fillingBufferInterval.count() > BACKUP_WAIT) {
                    LOG_INFO() << "Starting backup source";
                    shellExecutor.runCommand("./failed.sh");

                    backupRunning = true;
                    backupStarted = startTime;
                }
                return;
            }
            if (readyBuffers.empty()) {
                underrunCount++;
                LOG_INFO() << "Buffer underrun. Count == " << underrunCount;
                startTime = now;
                underrunStartTime = now;
                state = WaitingForInput;
                fillWithSilence();
                return;
            }
            buf = readyBuffers.front();
            readyBuffers.pop_front();
        }

        if (buf->usedSamples == 0) {
            LOG_INFO() << "player found 0";
        }

        if (buf->usedSamples * 2 != len) {
            LOG_WARN() << "buffer size mismatch: " <<  buf->usedSamples * 2 << " vs "
                        << len ;
            fillWithSilence();
        } else {
            lastWasSilence = false;
            memcpy(stream, buf->buffer, buf->usedSamples * 2);
            //            buf->printAvg();
        }
        //~ LOG_INFO()  << "usedSamples: " << buf->usedSamples << " bufsize== " << len
        //;

        std::unique_lock<std::mutex> lock{mutex};
        freeBuffers.push_back(buf);
        freeCondVar.notify_one();
        //        auto fbs = freeBuffers.size();
        //        auto rbs = readyBuffers.size();
        //logInfo("Buffer state: " + std::to_string(rbs)+ " free: " + std::to_string(fbs)+ " total: " + std::to_string(fbs+rbs));
    }

    ~RtStreamPlayer() {
        LOG_INFO() << "Closing";
        //~ readThread.join();
        SDL_CloseAudio();
        for (auto buffer : readyBuffers) {
            delete buffer;
        }
        for (auto buffer : freeBuffers) {
            delete buffer;
        }
    }
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
