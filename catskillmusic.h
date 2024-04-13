#ifndef _CATSKILLMUSIC_H
#define _CATSKILLMUSIC_H
#define MUSIC_SAMPLES 8192
#define MUSIC_CHANNELS 2
#define MUSIC_BUFFER_SIZE_IN_FRAMES MUSIC_SAMPLES / MUSIC_CHANNELS
#define MUSIC_SAMPLERATE 44100
void musicInit();
void musicGetFrame();
void musicPlayFrame();
void musicStop();
void musicPause();
void musicResume();
void musicPlay(const char *path, int track);
void serviceMusic();
#endif