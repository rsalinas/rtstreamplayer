/*
 * Real Time Stream Player
 *
 * By Raúl Salinas-Monteagudo <rausalinas@gmail.com> 2017-06-15
 */

#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <SDL/SDL_audio.h>
#include <SDL/SDL.h>
#include <sndfile.hh>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <memory>
#include <cmath>
#include <csignal>

using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::duration_cast;

static FILE * shellProcess = popen("/bin/sh -x", "w");

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

void signalHandler(int sig) {
    std::clog << __FUNCTION__ << " " << strsignal(sig) << std::endl;
    switch (sig) {
    case SIGUSR1:
        fputs("daemon -n player -- mplayer /home/rsalinas/Downloads/lcwo-001.mp3", shellProcess);
        fflush(shellProcess);
        break;
    case SIGUSR2:
        fputs("daemon -n player --stop", shellProcess);
        fflush(shellProcess);
        break;
    }
}

class RtStreamPlayer {
    //const SDL_AudioSpec sdlAudioSpec;

    FILE * inputProcess = nullptr;
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
        AudioBuffer(size_t length) : buffer(new Uint8[length]), length(length) {

        }

        Uint8 *buffer;
        size_t length;
        float avg;
        sf_count_t usedSamples;
    };
    std::deque<AudioBuffer *> readyBuffers, freeBuffers;
    size_t skipped = 0;

    bool eofReached = false;
    std::unique_ptr<SndfileHandle> sndfile;
    bool mustExit = false;
    size_t bufferSize;
    size_t secondsToBuffers(float seconds) {
//        std::clog << __FUNCTION__ << " " << samplerate << " " << channels << std::endl;
        return size_t( ceil(seconds * samplerate * channels * 2 /
                            bufferSize));
    }
    float buffersToSeconds(size_t buffers) {
        return float(buffers) * bufferSize /
                (float(sndfile->samplerate()) * sndfile->channels() * 2);
    }

    void readInput() {
        logDebug(__FUNCTION__);

        while (!eofReached) {

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
                pclose(inputProcess);
                sndfile.reset();
                inputProcess = openProcess();
                sndfile.reset(new SndfileHandle(fileno(inputProcess), false));
                //eofReached = true;
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

    std::thread readThread;
    void print(const std::string & message, const SDL_AudioSpec obtained) {
        std::clog << message << ": " << obtained.freq << " " << int(obtained.channels) << " chan, " << obtained.samples << " samples" << std::endl;

    }
    FILE * openProcess() {
        return popen("./source.sh", "r");
    }

public:
    RtStreamPlayer(const char *filename) : inputProcess(openProcess()), sndfile(new SndfileHandle(fileno(inputProcess), false)) {
        if (!sndfile->samplerate()) {
            throw std::runtime_error("Invalid format");
        }
        signal(SIGUSR1, signalHandler);
        signal(SIGUSR2, signalHandler);
        samplerate = sndfile->samplerate();
        channels = sndfile->channels();

        fprintf(stderr, "Opened file '%s'\n", filename);

        fprintf(stderr, "    Sample rate : %d\n", samplerate);
        fprintf(stderr, "    Channels    : %d\n", channels);
        fprintf(stderr, "    Format:       %0x\n", sndfile->format());
        if (!sndfile->format() & SF_FORMAT_PCM_16) {
            throw std::runtime_error(
                        std::string("Invalid file format, PCM16 expected"));
        }
        //    if (file.format() &
        //~ readThread = std::thread{&RtStreamPlayer::readInput, this};

        SDL_AudioSpec wanted, obtained;

        /* Set the audio format */
        wanted.freq = sndfile->samplerate();
        wanted.format = AUDIO_S16;
        wanted.channels = sndfile->channels();
        wanted.samples = sndfile->samplerate()*sndfile->channels()/4;
        int wantedSamples = wanted.samples;
        wanted.callback = fill_audio_adaptor;
        wanted.userdata = this;

        /* Open the audio device, forcing the desired format */
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

        std::clog << "Starting playback..." << std::endl;
        SDL_PauseAudio(0);
    }

    int run() {
        //~ while (!mustExit) {
        //~ SDL_Delay(500);
        //~ }
        readInput();

        SDL_CloseAudio();
    }

    static void fill_audio_adaptor(void *udata, Uint8 *stream, int len) {
        static_cast<RtStreamPlayer *>(udata)->fill_audio(stream, len);
    }

    void fill_audio(Uint8 *stream, int len) {

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
                std::clog << "player: Buffering -> Playing. ellapsed == "
                          << time_span.count() << std::endl;
            }
            if (state != Playing) {
                //~ std::clog << "player: syncing"  << std::endl;
                memset(stream, 0, len);
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
                memset(stream, 0, len);
                return;
            }
            buf = readyBuffers.front();
            readyBuffers.pop_front();
        }

        if (buf->usedSamples == 0) {
            std::clog << "player found 0, must exit!" << std::endl;
            mustExit = true;
        }

        if (buf->usedSamples * 2 != len) {
            std::clog << "buffer size mismatch!" << buf->usedSamples * 2 << " vs "
                      << len << std::endl;
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
        //~ readThread.join();
    }
};

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "bad args. app <soundfile>\n");
        exit(EXIT_FAILURE);
    }

    fputs("date\n", shellProcess);
    fflush(shellProcess);
    try {
        (RtStreamPlayer(argv[1])).run();
    } catch (const std::exception &e) {
        std::clog << "Exception: " << e.what() << std::endl;
    }
}
