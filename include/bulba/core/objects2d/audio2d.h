#ifndef BULBA_CORE_OBJECTS2D_AUDIO2D_H
#define BULBA_CORE_OBJECTS2D_AUDIO2D_H

#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/utils/audio.h"
#include "bulba/core/utils/miniaudio.h"

#include <stdbool.h>

typedef struct {
  BLB_Object2D *obj;
  BLB_Texture *audio_debug_texture;
  BLB_Texture *audio_off_debug_texture;

  BLB_IO_Audio *io_audio;

  ma_sound *sound;
  ma_result *result;

  float volume;
  float attenuation;
  float pitch;

  bool loop;
  bool stream;
  bool surround;
  char *path;

  bool play;
} BLB_Audio2D;

BLB_Audio2D *BLB_CreateAudio2D(BLB_IO_Audio *io_audio, bool loop, bool stream, float volume, float attenuation, float pitch, bool play,
                               bool surround);

BLB_Audio2D *BLB_LoadAudio2D(BLB_IO_Audio *io_audio, const char *path, bool play, bool surround, bool loop, BLB_Vec2 position);

void BLB_DestroyAudio2D(BLB_IO_Audio *io_audio, BLB_Audio2D *audio);
void BLB_ReleaseAudio2D(BLB_IO_Audio *io_audio, BLB_Audio2D *audio);

void BLB_SetVolume_Audio2D(BLB_Audio2D *audio, float volume);

void BLB_PlayAudio_Audio2D(BLB_Audio2D *audio);
void BLB_StopAudio_Audio2D(BLB_Audio2D *audio);

void BLB_SetPosition_Audio2D(BLB_Audio2D *audio, BLB_Vec2 position);
void BLB_SetVelocity_Audio2D(BLB_Audio2D *audio, BLB_Vec2 velocity);
void BLB_SetRotation_Audio2D(BLB_Audio2D *audio, BLB_Vec2 rotation);

void BLB_SetAttenuation_Audio2D(BLB_Audio2D *audio, float attenuation);
void BLB_SetLoop_Audio2D(BLB_Audio2D *audio, bool loop);
void BLB_SetPitch_Audio2D(BLB_Audio2D *audio, float pitch);

void BLB_SetAudio2DSeek(BLB_Audio2D *audio, float seconds, int frame);

void BLB_Audio2DSeek(BLB_Audio2D *audio, float seconds, int frame);

#endif
