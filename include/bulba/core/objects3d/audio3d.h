#ifndef BULBA_CORE_OBJECTS3D_AUDIO3D_H
#define BULBA_CORE_OBJECTS3D_AUDIO3D_H

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/math3v/miniaudio.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/utils/audio.h"

#include <stdbool.h>

typedef struct {
  BLB_Object3D *obj;
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
} BLB_Audio3D;

BLB_Audio3D *BLB_CreateAudio3D(BLB_IO_Audio *io_audio, bool loop, bool stream, float volume, float attenuation, float pitch, bool play,
                               bool surround);

BLB_Audio3D *BLB_LoadAudio3D(BLB_IO_Audio *io_audio, const char *path, bool play, bool surround, bool loop, HMM_Vec3 position);

void BLB_DestroyAudio3D(BLB_IO_Audio *io_audio, BLB_Audio3D *audio);
void BLB_ReleaseAudio3D(BLB_IO_Audio *io_audio, BLB_Audio3D *audio);

void BLB_SetVolume_Audio3D(BLB_Audio3D *audio, float volume);

void BLB_PlayAudio3D(BLB_Audio3D *audio);
void BLB_StopAudio3D(BLB_Audio3D *audio);

void BLB_SetPosition_Audio3D(BLB_Audio3D *audio, HMM_Vec3 position);
void BLB_SetVelocity_Audio3D(BLB_Audio3D *audio, HMM_Vec3 velocity);
void BLB_SetRotation_Audio3D(BLB_Audio3D *audio, HMM_Vec3 rotation);

void BLB_SetAttenuation_Audio3D(BLB_Audio3D *audio, float attenuation);
void BLB_SetLoop_Audio3D(BLB_Audio3D *audio, bool loop);
void BLB_SetPitch_Audio3D(BLB_Audio3D *audio, float pitch);

void BLB_SetAudio3DSeek(BLB_Audio3D *audio, float seconds, int frame);

void BLB_Audio3DSeek(BLB_Audio3D *audio, float seconds, int frame);

#endif
