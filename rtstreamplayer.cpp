/*
 * Real Time Stream Player
 *
 * By Raúl Salinas-Monteagudo <rausalinas@gmail.com> 2017-06-15
 */

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <SDL/SDL_audio.h>
#include <SDL/SDL.h>
#include <sndfile.hh>
#include <stdexcept>
#include <unistd.h>

#include "buffer.h"
#include "logging.h"
#include "popen.h"
#include "process.h"

using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::duration_cast;


class RtStreamPlayer;

static std::unique_ptr<RtStreamPlayer> instance;

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
        //        std::clog << __FUNCTION__ << " " << samplerate << " " << channels << std::endl;
        return size_t( ceil(seconds * samplerate * channels * 2 /
                            bufferSize));
    }
    float buffersToSeconds(size_t buffers)
    {
        return float(buffers) * bufferSize /
                (float(sndfile->samplerate()) * sndfile->channels() * 2);
    }

    void readInput() {
        logDebug(__FUNCTION__);

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
                        logInfo("Drop excess packets: " + std::to_string(dropped));
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
            
            //std::clog << "read block: " << buf->usedSamples << " samples" << " " << freeBuffers.size() << " free buffers, " << readyBuffers.size() << " ready buffers" <<  std::endl;
            if (buf->usedSamples == 0) {
                logInfo("EOF in input file");
                sndfile.reset();
                inputProcess.reset();
                logInfo("Reopening process");
                inputProcess = openProcess();
                sndfile.reset(new SndfileHandle(inputProcess->getFd(), false));
            }

            std::unique_lock<std::mutex> lock{mutex};
            readyBuffers.push_back(buf);
            if (state == WaitingForInput) {
                state = Buffering;
                logInfo("WaitingForInput -> Buffering");
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

        fprintf(stderr, "    Sample rate : %d\n", samplerate);
        fprintf(stderr, "    Channels    : %d\n", channels);
        fprintf(stderr, "    Format:       %0x\n", sndfile->format());
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
        std::clog << "Wanted: " << wantedSamples << " got " << wanted.samples<< std::endl;
        print("wanted " , wanted);
        print("got " , obtained);
        bufferSize = obtained.size;
        std::clog << "Audio in a buffer: " << buffersToSeconds(1) << std::endl;

        size_t nBufs = secondsToBuffers(MIN_BUFFER_TIME + MARGIN_TIME);
        std::clog << "secondsToBuffers : " << nBufs << " "
                  << buffersToSeconds(nBufs) << std::endl;

        for (size_t i = 0; i < nBufs; i++) {
            auto audioBuffer = new AudioBuffer(obtained.size);
            freeBuffers.push_back(audioBuffer);
        }
        std::clog << "Buffer count: " << nBufs << std::endl;
    }

    int run() {
        logInfo("Starting playback...");
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
            //~ std::clog << "fill buffer = " << readyBuffers.size() <<"  len=="<<
            // len<< std::endl;

            if (state == Buffering && fillingBufferInterval.count() > SYNC_TIME &&
                    readyBuffers.size() >= secondsToBuffers(MIN_BUFFER_TIME)) {
                state = Playing;
                if (backupRunning) {
                    logInfo("Stopping backup source");
                    shellExecutor.runCommand("./restored.sh");

                    backupRunning = false;
                }


                auto audioDowntime = duration_cast<duration<double>>(now - underrunStartTime).count();
                downTime += audioDowntime;
                logInfo("player: Buffering -> Playing. ellapsed == " + std::to_string(fillingBufferInterval.count()));
                logInfo("Downtime: " + std::to_string(downTime) +  " uptime: " + std::to_string(uptime));
            }
            if (state != Playing) {
                //~ std::clog << "player: syncing"  << std::endl;
                fillWithSilence();
                if (startTime != backupStarted && fillingBufferInterval.count() > BACKUP_WAIT) {
                    logInfo("Starting backup source");
                    shellExecutor.runCommand("./failed.sh");

                    backupRunning = true;
                    backupStarted = startTime;
                }
                return;
            }
            if (readyBuffers.empty()) {
                underrunCount++;
                logInfo("Buffer underrun. Count == " + std::to_string(underrunCount));
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
            logInfo("player found 0");
        }

        if (buf->usedSamples * 2 != len) {
            logWarn("buffer size mismatch!");
            std::clog << buf->usedSamples * 2 << " vs "
                      << len << std::endl;
            fillWithSilence();
        } else {
            lastWasSilence = false;
            memcpy(stream, buf->buffer, buf->usedSamples * 2);
            //            buf->printAvg();
        }
        //~ std::clog << "usedSamples: " << buf->usedSamples << " bufsize== " << len
        //<< std::endl;

        std::unique_lock<std::mutex> lock{mutex};
        freeBuffers.push_back(buf);
        freeCondVar.notify_one();
        //        auto fbs = freeBuffers.size();
        //        auto rbs = readyBuffers.size();
        //logInfo("Buffer state: " + std::to_string(rbs)+ " free: " + std::to_string(fbs)+ " total: " + std::to_string(fbs+rbs));
    }

    ~RtStreamPlayer() {
        logInfo("Closing");
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
            logInfo("Buffer state: " + std::to_string(rbs)+ " free: " + std::to_string(fbs)+ " total: " + std::to_string(fbs+rbs));
            break;
        }
    }
};


void signalHandler(int sig) {
    std::clog << __FUNCTION__ << " " << strsignal(sig) << std::endl;
    switch (sig) {
    case SIGUSR1:
        break;
    case SIGUSR2:
        break;
    case SIGINT:
    case SIGTERM:
        instance->pleaseFinish();
        break;
    }
}

class SignalHandler {
    const int sig;
public:
    SignalHandler(int sig, sighandler_t handler) : sig(sig) {
        std::clog << __FUNCTION__ << " " << strsignal(sig) << std::endl;
        signal(sig, handler);
    }
    ~SignalHandler() {
        std::clog << __FUNCTION__ << " " << strsignal(sig) << std::endl;
        signal(sig, SIG_DFL);

    }
};

int main(int argc, char **argv)
{

    LOG_INFO() << "hola" << 5;
    return 0;
    try {
        instance.reset(new RtStreamPlayer);
        auto sigUsrHandler =  [](int sig) {
            std::clog << __FUNCTION__ << " " << strsignal(sig) << std::endl;
            //instance->sigUSR1();
        };

        SignalHandler usr1Handler{SIGUSR1, sigUsrHandler};
        SignalHandler usr2Handler{SIGUSR2, sigUsrHandler};

        auto finishHandler = [](int sig) {
            std::clog << __FUNCTION__ << " " << strsignal(sig) << std::endl;
            instance->pleaseFinish();
        };

        SignalHandler intHandler{SIGINT, finishHandler};
        SignalHandler termHandler{SIGTERM, finishHandler};

        instance->run();
    } catch (const std::exception &e) {
        std::clog << "Exception: " << e.what() << std::endl;
    }
}
