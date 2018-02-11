/*
 * Real Time Stream Player
 *
 * By Raúl Salinas-Monteagudo <rausalinas@gmail.com> 2017-06-15
 */

#include "rtstreamplayer.h"
#include <functional>
#include <map>
#include <iostream>
#include <sstream>
#include "popen.h"
#include "string_utils.h"

using namespace std;

struct Param  {
    std::string name;
    std::string defvalue;
    std::string desc;
};

static const std::vector<Param> params = {
    {"sync_time", "1", "Time to wait initially for synchronization"},
    {"min_buffer_time", "3", "Minimum buffer size. Play does not start until this amount is filled up"},
    {"margin_time", "2", "Extra delay that can be accumulated. Beyond it, sound is discarded until min_buffer_time"},
    {"backup_wait", "5", "Time to wait after the main source fails until the backup is started"},
    {"source", "./source.sh", "Command providing the RIFF WAV input"},
    {"backup_start", "./backup-start.sh", "Command that starts the backup streaming"},
    {"backup_stop", "./backup-stop.sh","Command  that stops the backup streaming"},
};

std::string RtStreamPlayer::currentStatus() {
    stringstream ss;
    ss << "Sample rate : " << samplerate ;
    ss << " Channels    : "  << channels;
    ss << " Format: " <<  sndfile->format();
    return ss.str();
}

static std::map<std::string, MqttServer::CommandMeta> adapt(const std::map<std::string, RtStreamPlayer::CommandSpec>& cmds) {
    std::map<std::string, MqttServer::CommandMeta> ret;
    for (const auto& kv: cmds) {
        ret[kv.first] = kv.second.meta;
    }
    return ret;
}
std::string RtStreamPlayer::adaptParams() {
    string ret;
    for (const auto& param : params) {
        ret += param.name + "|" + param.desc + "|" + paramValues_[param.name] +"\n";
    }
    return ret;
}

RtStreamPlayer::RtStreamPlayer(const Properties& props) : props_(props), mqttServer("rtsp"), mqttServerThread_(mqttServer, *this) {
    mqttServer.setServerStatus("Initializing");
    mqttServer.setCommandList(adapt(commands_));
    mqttServer.setParamList(adaptParams());

    startBackupSource();
    inputProcess = openProcess();
    sndfile.reset(new SndfileHandle(inputProcess->getFd(), false));
    if (!sndfile->samplerate()) {
        clog << "Bad format" << endl;
        LOG_WARN() << "Bad format";
        throw RtStreamPlayerException{"Invalid format"};
    }
    samplerate = sndfile->samplerate();
    channels = sndfile->channels();

    LOG_INFO()  << "    Sample rate : " << samplerate;
    LOG_INFO()  <<  "    Channels    : " <<  channels;
    LOG_INFO()  <<  "    Format:       " <<  sndfile->format();

    if (!(sndfile->format() & SF_FORMAT_PCM_16)) {
        throw std::runtime_error(
                    std::string("Invalid file format, PCM16 expected"));
    }
    //~ readThread = std::thread{&RtStreamPlayer::readInput, this};

    SDL_AudioSpec wanted, obtained;

    wanted.freq = sndfile->samplerate();
    wanted.format = AUDIO_S16;
    wanted.channels = sndfile->channels();
    wanted.samples = 32768; // sndfile->samplerate()*sndfile->channels()/2/2;
    //    int wantedSamples = wanted.samples;
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

RtStreamPlayer::~RtStreamPlayer() {
    LOG_INFO() << "Closing";
    mqttServer.setServerStatus("Closing");
    SDL_CloseAudio();
    for (auto buffer : readyBuffers) {
        delete buffer;
    }
    for (auto buffer : freeBuffers) {
        delete buffer;
    }
}



void RtStreamPlayer::readInput() {
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
        auto now = steady_clock::now();

        //LOG_INFO()  << "read block: " << buf->usedSamples << " samples" << " " << freeBuffers.size() << " free buffers, " << readyBuffers.size() << " ready buffers" ;
        if (mustReopen_ || buf->usedSamples == 0) {
            bool wasIntentional = false;
            swap(mustReopen_, wasIntentional);
            LOG_INFO() << "EOF in input stream";
            sndfile.reset();
            inputProcess.reset();
            std::ifstream t("cmd.out");
            std::string cmdOut((std::istreambuf_iterator<char>(t)),
                               std::istreambuf_iterator<char>());
            clog << "cmd out: " << cmdOut << endl;
            LOG_INFO() << "Reopening process";
            auto timeSinceLastData = duration_cast<duration<double>>(now - lastInput).count();
            LOG_DEBUG() << "timeSinceLastData " << timeSinceLastData;
            if (validPackets) {
                mqttServer.setServerStatus("Reopen input process. Output was:\n\n" + cmdOut);
                validPackets = false;
            } else {
                sleep(3);
            }
            inputProcess = openProcess();
            sndfile.reset(new SndfileHandle(inputProcess->getFd(), false));
        } else {
            lastInput = now;
            validPackets = true;
        }

        std::unique_lock<std::mutex> lock{mutex};
        readyBuffers.push_back(buf);
        if (state == WaitingForInput) {
            state = Buffering;
            LOG_INFO() << "WaitingForInput -> Buffering";
            startTime = now;
            mqttServer.setServerStatus("Buffering");
        }
    }
}

void RtStreamPlayer::fill_audio(Uint8 *stream, int len)
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
            stopBackupSource();

            auto audioDowntime = duration_cast<duration<double>>(now - underrunStartTime).count();
            downTime += audioDowntime;
            LOG_INFO() << "player: Buffering -> Playing. ellapsed == " << fillingBufferInterval.count();
            LOG_INFO() << "Downtime: " << downTime << " uptime: " << uptime;
        }
        if (state != Playing) {
            //~ LOG_INFO()  << "player: syncing"  ;
            fillWithSilence();
            if (startTime != backupStarted && fillingBufferInterval.count() > BACKUP_WAIT) {
                startBackupSource();
                backupStarted = startTime;
            }
            return;
        }
        if (readyBuffers.empty()) {
            underrunCount++;
            LOG_INFO() << "Buffer underrun. Count == " << underrunCount;
            mqttServer.setServerStatus("Buffer underrun. Count is " + to_string(underrunCount));
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

    if (muted_ || buf->usedSamples * 2 != len) {
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

std::unique_ptr<Popen> RtStreamPlayer::openProcess() {
    return std::unique_ptr<Popen>(new Popen{"exec ./source.sh 2> cmd.out", "r"});
}

int RtStreamPlayer::run() {
    LOG_INFO() << "Starting playback...";
    SDL_PauseAudio(0);
    readInput();
    return true;
}

bool RtStreamPlayer::startBackupSource() {
    if (!backupRunning) {
        LOG_INFO() << "Starting backup source";
        mqttServer.setServerStatus("Backup");
        shellExecutor.runCommand("./failed.sh");
        backupRunning = true;
    }
    return true;
}

bool RtStreamPlayer::stopBackupSource() {
    if (backupRunning) {
        LOG_INFO() << "Stopping backup source";
        shellExecutor.runCommand("./restored.sh");
        backupRunning = false;
        mqttServer.setServerStatus("Streaming");
    }
    return true;
}

const std::map<std::string, RtStreamPlayer::CommandSpec> RtStreamPlayer::commands_ = {
    {"cpuinfo", CommandSpec{MqttServer::CommandMeta{false,"Get CPU info"},  [](std::string, RtStreamPlayer* self)  {
                                std::ifstream t("/proc/cpuinfo");
                                std::string str((std::istreambuf_iterator<char>(t)),
                                std::istreambuf_iterator<char>());
                                return str;
                            } }},
    {"quit", CommandSpec{MqttServer::CommandMeta{true, "Stop rtsp server"}, [](std::string, RtStreamPlayer* self)  {
                             self->pleaseFinish();
                             return "Quitting!";
                         }}},
    {"temp", CommandSpec{MqttServer::CommandMeta{false, "Get CPU temperature"}, [](std::string, RtStreamPlayer* self)  {
                             std::ifstream t("/sys/class/thermal/thermal_zone0/temp");
                             std::string str((std::istreambuf_iterator<char>(t)),
                             std::istreambuf_iterator<char>());
                             return std::to_string(atof(str.c_str()) / 1000) + std::string{" ºC"};

                         }}},
    {"status", CommandSpec{MqttServer::CommandMeta{false, "Get current state"}, [](std::string, RtStreamPlayer* self)  {
                               return self->currentStatus();
                           }}},
    {"uptime", CommandSpec{MqttServer::CommandMeta{false, "Get uptime of host and services"}, [](std::string, RtStreamPlayer* self)  {
                               std::ifstream t("/proc/uptime");
                               std::string str((std::istreambuf_iterator<char>(t)),
                               std::istreambuf_iterator<char>());
                               return str;
                           }}},
    {"mute", CommandSpec{MqttServer::CommandMeta{true, "Mute output. Flow is not interrupted"}, [](std::string, RtStreamPlayer* self)  {
                             self->muted_ = true;
                             return "Muted. Resume with /unmute";
                         }}},
    {"unmute", CommandSpec{MqttServer::CommandMeta{true, "Unmute output, normal operation."}, [](std::string, RtStreamPlayer* self)  {
                               self->muted_ = false;
                               return "Unmuted. Mute with /mute";
                           }}},
    {"reopen", CommandSpec{MqttServer::CommandMeta{true, "Reopen source."}, [](std::string, RtStreamPlayer* self)  {
                               self->reopen();
                               return "Will reopen";
                           }}},


    {"player_version", CommandSpec{MqttServer::CommandMeta{true, "Show rtstreamplayer version."}, [](std::string, RtStreamPlayer* self)  {
                                       return execCmd(string{"stat -L /proc/"}+to_string(getpid())+"/exe  --format %y");
                                   }}},

};

std::string RtStreamPlayer::runCommand(const std::string& clientId, const std::string& cmdline) {
    LOG_INFO() << __FUNCTION__ << " ..."  << cmdline << " for " << clientId;
    auto ss = splitString(cmdline, ' ');
    if (ss.size() < 1) {
        return "ERROR";
    }
    auto kv = commands_.find(ss[0]);
    if (kv != commands_.end()) {
        return  kv->second.func(cmdline, this);
    }
    return "Error: Unknown command";
}
