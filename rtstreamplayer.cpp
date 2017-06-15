#include <iostream>
#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>
#include <sndfile.hh>
#include <stdexcept>


class RtStreamPlayer {
  Uint8 *audio_chunk;
  Uint32 audio_len;
  Uint8 *audio_pos;

  SndfileHandle file;
  public:
  RtStreamPlayer(const char * filename) : file(filename) {
  if (!file.samplerate()) {
    throw std::runtime_error("Invalid format");
  }


	printf ("Opened file '%s'\n", filename) ;
	printf ("    Sample rate : %d\n", file.samplerate ()) ;
	printf ("    Channels    : %d\n", file.channels ()) ;
	printf ("    Format:       %d\n", file.format ()) ;



    SDL_AudioSpec wanted;

    /* Set the audio format */
    wanted.freq = file.samplerate();
    wanted.format = AUDIO_S16;
    wanted.channels = file.channels();    /* 1 = mono, 2 = stereo */
    wanted.samples = 1024;  
    wanted.callback = fill_audio_adaptor;
    wanted.userdata = this;

    /* Open the audio device, forcing the desired format */
    if ( SDL_OpenAudio(&wanted, NULL) < 0 ) 
      throw std::runtime_error(std::string("Couldn't open audio: ") + SDL_GetError());

    /* Load the audio data ... */

    ;;;;;

//    audio_pos = audio_chunk;

    /* Let the callback function play the audio chunk */
    SDL_PauseAudio(0);

    /* Do some processing */

    ;;;;;

}

int  run() {
  
    /* Wait for sound to complete */
    while ( audio_len > 0 ) {
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
  
  std::clog << "fill" << std::endl;
	std::clog << "read: " << file.read (reinterpret_cast<short*>(stream), len/2) << std::endl; 

    //~ /* Only play if we have data left */
    //~ if ( audio_len == 0 )
        //~ return;

    //~ /* Mix as much data as possible */
    //~ len = ( len > audio_len ? audio_len : len );
    //~ SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    //~ audio_pos += len;
    //~ audio_len -= len;
    
}

~RtStreamPlayer() {
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
