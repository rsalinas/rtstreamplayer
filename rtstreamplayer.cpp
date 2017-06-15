#include <iostream>
#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>
#include <sndfile.hh>
#include <stdexcept>
#include <pthread.h>
#include <deque>
#include <thread>
#include <unistd.h>
#include <mutex>
#include <condition_variable>

class RtStreamPlayer {
  std::mutex mutex;
  std::condition_variable readyCondVar;
  std::condition_variable freeCondVar;
  class AudioBuffer {
  public:
    static const size_t bufferSize = 4096;
    Uint8 buffer[bufferSize];
    float avg;
    sf_count_t usedSamples;
  };
  std::deque<AudioBuffer*> readyBuffers, freeBuffers;

  bool eofReached = false;
  SndfileHandle file;
  bool mustExit = false;

  void readInput() {
    std::clog << __FUNCTION__ << std::endl;


    while (!eofReached) {

      AudioBuffer * buf=nullptr;
      {
	//	   std::clog << "read input iterating" << std::endl;
	std::unique_lock<std::mutex> lock{mutex};
	while (freeBuffers.empty()) {
	  //			std::clog << "no free buffers"  << std::endl;
	  freeCondVar.wait(lock);
	}
	buf = freeBuffers.back();
	freeBuffers.pop_back();
      }
      buf->usedSamples = file.read (reinterpret_cast<short*>(buf->buffer), sizeof(buf->buffer) / 2) ;
      std::clog << "read block: " << buf->usedSamples << " samples" << std::endl;
      if (buf->usedSamples == 0) {
	std::clog << "EOF, must exit!" << std::endl;

	eofReached = true;
      }
      std::unique_lock<std::mutex> lock{mutex};
      readyBuffers.push_back(buf);
      readyCondVar.notify_one();

    }
  }

  std::thread readThread;
public:



  RtStreamPlayer(const char * filename) : file(filename) {
    if (!file.samplerate()) {
      throw std::runtime_error("Invalid format");
    }

    size_t nBufs = 5*file.samplerate()*file.channels()*2 / AudioBuffer::bufferSize;
    for (size_t i = 0 ; i < nBufs; i++) {
      auto audioBuffer = new AudioBuffer;
      freeBuffers.push_back(audioBuffer);
    }
    std::clog << "Buffer count: " << nBufs << std::endl;


    printf ("Opened file '%s'\n", filename) ;
    printf ("    Sample rate : %d\n", file.samplerate ()) ;
    printf ("    Channels    : %d\n", file.channels ()) ;
    printf ("    Format:       %d\n", file.format ()) ;
    readThread = std::thread {&RtStreamPlayer::readInput, this};



    SDL_AudioSpec wanted;

    /* Set the audio format */
    wanted.freq = file.samplerate();
    wanted.format = AUDIO_S16;
    wanted.channels = file.channels();    /* 1 = mono, 2 = stereo */
    wanted.samples = AudioBuffer::bufferSize;
    wanted.callback = fill_audio_adaptor;
    wanted.userdata = this;

    /* Open the audio device, forcing the desired format */
    if ( SDL_OpenAudio(&wanted, NULL) < 0 )
      throw std::runtime_error(std::string("Couldn't open audio: ") + SDL_GetError());

    /* Load the audio data ... */

    {
      std::unique_lock<std::mutex> lock{mutex};
      while (!eofReached && readyBuffers.size()< 100) {
	std::clog << "prebuffering"  << std::endl;
	readyCondVar.wait(lock);
      }
    }


    ;;;;;

    //    audio_pos = audio_chunk;

    /* Let the callback function play the audio chunk */
    std::clog << "unpausing"  << std::endl;
    SDL_PauseAudio(0);

    /* Do some processing */

    ;;;;;

  }

  int  run() {

    /* Wait for sound to complete */
    while ( !mustExit )  { // audio_len > 0 ) {
      SDL_Delay(100);         /* Sleep 1/10 second */
    }
    SDL_CloseAudio();

  }

  /* The audio function callback takes the following parameters:
     stream:  A pointer to the audio buffer to be filled
     len:     The length (in bytes) of the audio buffer
  */
  static void fill_audio_adaptor(void *udata, Uint8 *stream, int len)
  {
    static_cast<RtStreamPlayer*>(udata)->fill_audio(udata, stream, len);
  }

  void fill_audio(void *udata, Uint8 *stream, int len)
  {

    std::clog << "fill"  << len<< std::endl;

    AudioBuffer * buf=nullptr;
    {
      std::unique_lock<std::mutex> lock{mutex};
      if (readyBuffers.empty()) {
	std::clog << "buffer underrun"  << std::endl;
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
    std::unique_lock<std::mutex> lock{mutex};
    freeBuffers.push_back(buf);
    freeCondVar.notify_one();

	if (buf->usedSamples*2 != len) {
		std::clog << "buffer size mismatch!" << buf->usedSamples*2 << " vs " << len  << std::endl;
		return;
	}

    memcpy(stream, buf->buffer, buf->usedSamples * 2);
    std::clog << "usedSamples: " << buf->usedSamples << " bufsize== " << len << std::endl;
  }

  ~RtStreamPlayer() {
		readThread.join();
  }
};

int main(int argc, char **argv) {

  if (argc != 2) {
    fprintf(stderr, "bad args. app <file.ogg>\n");
    exit(EXIT_FAILURE);
  }

  try {
    (RtStreamPlayer(argv[1])).run();
  } catch (const std::exception &e) {
    std::clog << "Exception: "<< e.what() << std::endl;
  }


}
