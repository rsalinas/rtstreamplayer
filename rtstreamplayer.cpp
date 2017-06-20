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
#include <memory>
#include <mutex>
#include <pthread.h>
#include <SDL/SDL_audio.h>
#include <SDL/SDL.h>
#include <sndfile.hh>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::duration_cast;


class Popen
{
    FILE * f = nullptr;
public:
    Popen(const char *cmd, const char *mode) : f(popen(cmd, mode)) {

    }

    FILE * getFile() const {
        return f;
    }

    ~Popen() {
        pclose(f);
    }
};
static Popen shellProcess{"/bin/sh -x", "w"};
class RtStreamPlayer;

static std::unique_ptr<RtStreamPlayer> instance;

std::string nowStr( const char* format = "%c" )
{
    std::time_t t = std::time(0) ;
    char cstr[128] ;
    std::strftime( cstr, sizeof(cstr), format, std::localtime(&t) ) ;
    return cstr ;
}

static void logInfo(const std::string &message) {
    std::clog << nowStr() << ": " << message << std::endl;
}

static void logDebug(const std::string &message) {
    std::clog << nowStr() << ": " << message << std::endl;
}

void runCommand(const std::string & cmd) {
    fputs((cmd+"\n").c_str(), shellProcess.getFile());
    fflush(shellProcess.getFile());
}

class RtStreamPlayer {
    //const SDL_AudioSpec sdlAudioSpec;

    std::unique_ptr<Popen> inputProcess;
    enum State { WaitingForInput, Buffering, Playing };
    State state = WaitingForInput;
    decltype(steady_clock::now()) startTime = steady_clock::now(), backupStarted;
    const float SYNC_TIME = 1;
    const float MIN_BUFFER_TIME = 1;
    const float MARGIN_TIME = 1;
    size_t minBufferCount = 0;
    int samplerate = 0, channels = 0;

    std::mutex mutex;
    std::condition_variable readyCondVar;
    std::condition_variable freeCondVar;
    class AudioBuffer {
    public:
        AudioBuffer(size_t length) : buffer(new Uint8[length]), length(length)
        {
        }

        Uint8 *buffer;
        size_t length;
        float avg;
        sf_count_t usedSamples;
    };
    std::deque<AudioBuffer *> readyBuffers, freeBuffers;
    std::unique_ptr<SndfileHandle> sndfile;
    bool mustExit = false;
    size_t bufferSize;

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
                //     std::clog << "read input iterating" << std::endl;
                std::unique_lock<std::mutex> lock{mutex};

                if (freeBuffers.empty() /*&& state != Playing*/) {
                    if (state == Playing) {
                        size_t swallowed = 0;
                        while (readyBuffers.size() > secondsToBuffers(MIN_BUFFER_TIME)) {
                            buf = readyBuffers.back();
                            readyBuffers.pop_back();
                            freeBuffers.push_back(buf);
                            swallowed++;
                        }
                        logInfo("Drop excess packets: " + std::to_string(swallowed));

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
            
            //std::clog << "read block: " << buf->usedSamples << " samples" << " " << freeBuffers.size() << " free buffers, " << readyBuffers.size() << " ready buffers" <<  std::endl;
            if (buf->usedSamples == 0) {
                logInfo("EOF, must reload");
                sndfile.reset();
                inputProcess.reset();
                inputProcess = openProcess();
                sndfile.reset(new SndfileHandle(fileno(inputProcess->getFile()), false));
            }

            std::unique_lock<std::mutex> lock{mutex};
            readyBuffers.push_back(buf);
            readyCondVar.notify_one();
            if (state == WaitingForInput) {
                state = Buffering;
                std::clog << "WaitingForInput -> Buffering" << std::endl;
                startTime = steady_clock::now();
            }
        }
    }

    void print(const std::string & message, const SDL_AudioSpec obtained) {
        std::clog << message << ": " << obtained.freq << " " << int(obtained.channels) << " chan, " << obtained.samples << " samples" << std::endl;
    }


    std::unique_ptr<Popen> openProcess() {
        return std::unique_ptr<Popen>(new Popen{"./source.sh", "r"});
    }

public:
    RtStreamPlayer() : inputProcess(openProcess()), sndfile(new SndfileHandle(fileno(inputProcess->getFile()), false)) {
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

        /* Set the audio format */
        wanted.freq = sndfile->samplerate();
        wanted.format = AUDIO_S16;
        wanted.channels = sndfile->channels();
        wanted.samples = sndfile->samplerate()*sndfile->channels()/2/2;
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

        size_t nBufs = secondsToBuffers(MIN_BUFFER_TIME+ MARGIN_TIME) + 1;
        std::clog << "secondsToBuffers : " << nBufs << " "
                  << buffersToSeconds(nBufs) << std::endl;

        for (size_t i = 0; i < nBufs; i++) {
            auto audioBuffer = new AudioBuffer(obtained.size);
            freeBuffers.push_back(audioBuffer);
        }
        std::clog << "Buffer count: " << nBufs << std::endl;

        logInfo("Starting playback...");
        SDL_PauseAudio(0);
    }

    int run() {
        readInput();
    }

    void fill_audio(Uint8 *stream, int len)
    {
        auto fillWithSilence = [stream, len] {
            memset(stream, 0, len);
        };
        AudioBuffer *buf = nullptr;
        {
            std::unique_lock<std::mutex> lock{mutex};
            //~ std::clog << "fill buffer = " << readyBuffers.size() <<"  len=="<<
            // len<< std::endl;

            auto now = steady_clock::now();
            std::chrono::duration<double> time_span =
                    duration_cast<duration<double>>(now - startTime);

            if (state == Buffering && time_span.count() > SYNC_TIME &&
                    readyBuffers.size() >= secondsToBuffers(MIN_BUFFER_TIME)) {
                state = Playing;
                raise(SIGUSR2);
                logInfo("player: Buffering -> Playing. ellapsed == " + std::to_string(time_span.count()));
            }
            if (state != Playing) {
                //~ std::clog << "player: syncing"  << std::endl;
                fillWithSilence();
                if (startTime != backupStarted && time_span.count() > 5) {
                    raise(SIGUSR1);
                    backupStarted = startTime;

                }
                return;
            }
            if (readyBuffers.empty()) {
                std::clog << "buffer underrun" << std::endl;
                startTime = steady_clock::now();
                state = WaitingForInput;
                fillWithSilence();
                return;
            }
            buf = readyBuffers.front();
            readyBuffers.pop_front();
        }

        if (buf->usedSamples == 0) {
            logInfo("player found 0");
//            mustExit = true;
        }

        if (buf->usedSamples * 2 != len) {
            std::clog << "buffer size mismatch!" << buf->usedSamples * 2 << " vs "
                      << len << std::endl;
            fillWithSilence();
        } else {
            memcpy(stream, buf->buffer, buf->usedSamples * 2);
        }
        //~ std::clog << "usedSamples: " << buf->usedSamples << " bufsize== " << len
        //<< std::endl;

        std::unique_lock<std::mutex> lock{mutex};
        freeBuffers.push_back(buf);
        freeCondVar.notify_one();
    }

    ~RtStreamPlayer() {
        logInfo("Closing");
        //~ readThread.join();
        SDL_CloseAudio();
    }
    void pleaseFinish() {
        mustExit = true;
    }
};


void signalHandler(int sig) {
    std::clog << __FUNCTION__ << " " << strsignal(sig) << std::endl;
    switch (sig) {
    case SIGUSR1:
        logInfo("Starting backup source");
        runCommand("daemon -n player -- mplayer /home/rsalinas/Downloads/lcwo-001.mp3");
        break;
    case SIGUSR2:
        logInfo("Stopping backup source");
        runCommand("daemon -n player --stop");
        break;
    case SIGINT:
    case SIGTERM:
        instance->pleaseFinish();
        break;
    }
}


int main(int argc, char **argv)
{
    try {
        runCommand("date");
        instance.reset(new RtStreamPlayer);
        signal(SIGUSR1, signalHandler);
        signal(SIGUSR2, signalHandler);
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        instance->run();
    } catch (const std::exception &e) {
        std::clog << "Exception: " << e.what() << std::endl;
    }
}
