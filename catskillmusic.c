#include "catskillmusic.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "gme/gme.h"
#include "miniaudio.h"

ma_result music_result;
ma_engine music_engine;
ma_pcm_rb rb;
ma_sound music;
Music_Emu *emu;
ma_uint32 frameCount;
enum MusicState
{
    musicNotReady,
    musicReady,
    musicPlaying,
    musicStopped,
    musicPaused
};
volatile int musicState = musicNotReady;
const int MUSIC_FORMAT = ma_format_s16;

void musicInit()
{
    if (musicState != musicNotReady)
    {
        return;
    }

    music_result = ma_engine_init(NULL, &music_engine);
    if (music_result != MA_SUCCESS)
    {
        printf("Failed to initialize audio engine\n");
        return;
    }
    music_result = ma_pcm_rb_init(MUSIC_FORMAT, MUSIC_CHANNELS, MUSIC_BUFFER_SIZE_IN_FRAMES, NULL, NULL, &rb);
    if (music_result != MA_SUCCESS)
    {
        printf("failed to initialize rotating buffer\n");
    }
    musicState = musicReady;
}

void musicGetFrame()
{
    if (musicState != musicPlaying)
    {
        return;
    }
    ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(MUSIC_FORMAT, MUSIC_CHANNELS);
    frameCount = 1024; // frames requested
    void *pWriteBuffer;
    music_result = ma_pcm_rb_acquire_write(&rb, &frameCount, &pWriteBuffer);
    if (music_result != MA_SUCCESS)
    {
        printf("Failed to acquire write\n");
        return;
    }
    ma_uint32 buff_size = frameCount * MUSIC_CHANNELS;
    int16_t buf[buff_size];
    gme_play(emu, buff_size, buf);
    memcpy(pWriteBuffer, &buf, bytesPerFrame * frameCount);
    music_result = ma_pcm_rb_commit_write(&rb, frameCount);
    if (music_result != MA_SUCCESS)
    {
        printf("Failed to commit write.\n");
        return;
    }
}

void musicPlayFrame()
{

    if (musicState != musicPlaying)
    {
        return;
    }
    void *pReadBuffer;
    ma_result music_result =
        ma_pcm_rb_acquire_read(&rb, &frameCount, &pReadBuffer);
    if (music_result != MA_SUCCESS)
    {
        printf("Failed to acquire read\n");
        return;
    }
    ma_sound_uninit(&music);
    music_result = ma_sound_init_from_data_source(&music_engine, &rb, 0, NULL, &music);
    if (music_result != MA_SUCCESS)
    {
        printf("Failed to initialize sound\n");
        return;
    }
    ma_sound_start(&music);
    music_result = ma_pcm_rb_commit_read(&rb, frameCount);
}

void musicStop()
{
    musicState = musicReady;
}

void musicPause()
{
    if (musicState == musicPlaying)
    {
        musicState = musicPaused;
    }
    else if (musicState == musicPaused)
    {
        musicResume();
    }
}

void musicResume()
{
    if (musicState != musicNotReady)
    {
        musicState = musicPlaying;
    }
}

void musicPlay(const char *path, int track)
{
    if (musicState == musicNotReady)
    {
        return;
    }
    gme_open_file(path, &emu, MUSIC_SAMPLERATE);
    gme_start_track(emu, track);
    musicState = musicPlaying;
    musicGetFrame();
    musicPlayFrame();
}

void serviceMusic()
{
    if (musicState != musicPlaying)
    {
        return;
    }

    if (gme_track_ended(emu))
    {
        musicStop();
        return;
    }

    musicGetFrame();
    if (ma_sound_at_end(&music))
    {
        musicPlayFrame();
    }
}
