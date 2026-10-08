#include "bulba/core/objects3d/audio3d.h"
#include "bulba/core/objects3d/cube.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/utils/config.h"

#include "volume_off_256dp.h"
#include "volume_up_256dp.h"

#include <stdlib.h>
#include <string.h>

static ma_uint32 BLB_AudioFlags(const BLB_Audio3D *audio) {
  ma_uint32 flags = 0;

  if (audio->stream)
    flags |= MA_SOUND_FLAG_STREAM;

  return flags;
}

static void BLB_ConfigureAudio(BLB_Audio3D *audio) {
  if (!audio)
    return;

  ma_sound_set_volume(audio->sound, audio->volume);
  ma_sound_set_pitch(audio->sound, audio->pitch);
  ma_sound_set_looping(audio->sound, audio->loop);
  ma_sound_set_position(audio->sound, audio->obj->position.x, audio->obj->position.y, audio->obj->position.z);
}

BLB_Audio3D *BLB_CreateAudio3D(BLB_IO_Audio *io_audio, bool loop, bool stream, float volume, float attenuation, float pitch, bool play,
                               bool surround) {
  (void)io_audio;
  (void)loop;
  (void)stream;
  (void)volume;
  (void)attenuation;
  (void)pitch;
  (void)play;
  (void)surround;

  return NULL;
}

BLB_Audio3D *BLB_LoadAudio3D(BLB_IO_Audio *io_audio, const char *path, bool play, bool surround, bool loop, HMM_Vec3 position) {
  if (!io_audio || !path)
    return NULL;

  BLB_Audio3D *audio = calloc(1, sizeof(BLB_Audio3D));
  if (!audio)
    return NULL;

  audio->sound = malloc(sizeof(ma_sound));
  if (!audio->sound) {
    free(audio);
    return NULL;
  }

  audio->result = malloc(sizeof(ma_result));
  if (!audio->result) {
    free(audio);
    return NULL;
  }

  audio->io_audio = malloc(sizeof(BLB_IO_Audio));
  if (!io_audio) {
    free(audio);
    return NULL;
  }
  audio->io_audio = io_audio;

  audio->path = strdup(path);
  if (!audio->path) {
    free(audio);
    return NULL;
  }

  audio->volume = 1.0f;
  audio->attenuation = 1.0f;
  audio->pitch = 1.0f;
  audio->loop = loop;
  audio->stream = false;
  audio->surround = surround;
  audio->play = play;

  ma_result result = ma_sound_init_from_file(&io_audio->engine, audio->path, BLB_AudioFlags(audio), NULL, NULL, audio->sound);
  *audio->result = result;

  if (result != MA_SUCCESS) {
    free(audio->path);
    free(audio);
    return NULL;
  }

  audio->obj = BLB_CreateCube3D(HMM_V3(1.0f, 1.0f, 0.001f), position, NULL, BLB_TEXMAP_STRETCH);
  if (!audio->obj) {
    free(audio->path);
    free(audio);
    return NULL;
  }

  audio->obj->visible = false;

  if (BLB_DEBUG) {
    audio->audio_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_volume_up_256dp_width, BLB_TEXTURE_volume_up_256dp_height,
                                                      BLB_TEXTURE_volume_up_256dp_pixels, BLB_TEXTURE_volume_up_256dp_pixel_size);

    audio->audio_off_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_volume_off_256dp_width, BLB_TEXTURE_volume_off_256dp_height,
                                                          BLB_TEXTURE_volume_off_256dp_pixels, BLB_TEXTURE_volume_off_256dp_pixel_size);

    BLB_Object3D_SetTexture(audio->obj, play ? audio->audio_debug_texture : audio->audio_off_debug_texture);
    audio->obj->visible = true;
  }

  BLB_ConfigureAudio(audio);
  return audio;
}

void BLB_DestroyAudio3D(BLB_IO_Audio *io_audio, BLB_Audio3D *audio) {
  (void)io_audio;

  if (!audio)
    return;

  ma_sound_uninit(audio->sound);

  free(audio->path);
  free(audio);
}

void BLB_ReleaseAudio3D(BLB_IO_Audio *io_audio, BLB_Audio3D *audio) { BLB_DestroyAudio3D(io_audio, audio); }

void BLB_SetVolume_Audio3D(BLB_Audio3D *audio, float volume) {
  if (!audio)
    return;

  audio->volume = volume;
  ma_sound_set_volume(audio->sound, volume);
}

void BLB_PlayAudio3D(BLB_Audio3D *audio) {
  if (!audio)
    return;

  if (ma_sound_start(audio->sound) == MA_SUCCESS)
    audio->play = true;
}

void BLB_StopAudio3D(BLB_Audio3D *audio) {
  if (!audio)
    return;

  if (ma_sound_stop(audio->sound) == MA_SUCCESS)
    audio->play = false;
}

void BLB_SetPosition_Audio3D(BLB_Audio3D *audio, HMM_Vec3 position) {
  if (!audio)
    return;

  audio->obj->position = position;

  ma_sound_set_position(audio->sound, position.x, position.y, position.z);
}

void BLB_SetVelocity_Audio3D(BLB_Audio3D *audio, HMM_Vec3 velocity) {
  if (!audio)
    return;

  ma_sound_set_velocity(audio->sound, velocity.x, velocity.y, velocity.z);
}

void BLB_SetRotation_Audio3D(BLB_Audio3D *audio, HMM_Vec3 rotation) {
  if (!audio)
    return;

  ma_sound_set_direction(audio->sound, rotation.x, rotation.y, rotation.z);
  BLB_Object3D_Rotate(audio->obj, rotation);
}

void BLB_SetAttenuation_Audio3D(BLB_Audio3D *audio, float attenuation) {
  if (!audio)
    return;

  audio->attenuation = attenuation;

  ma_sound_set_attenuation_model(audio->sound, ma_attenuation_model_inverse);
  ma_sound_set_rolloff(audio->sound, attenuation);
}

void BLB_SetLoop_Audio3D(BLB_Audio3D *audio, bool loop) {
  if (!audio)
    return;

  audio->loop = loop;
  ma_sound_set_looping(audio->sound, loop);
}

void BLB_SetPitch_Audio3D(BLB_Audio3D *audio, float pitch) {
  if (!audio)
    return;

  audio->pitch = pitch;
  ma_sound_set_pitch(audio->sound, pitch);
}

void BLB_SetAudio3DSeek(BLB_Audio3D *audio, float seconds, int frame) {
  if (!audio || (seconds == 0 && frame == 0) || (seconds != 0 && frame != 0))
    return;

  if (seconds != 0)
    ma_sound_seek_to_second(audio->sound, seconds);
  else if (frame != 0)
    ma_sound_seek_to_pcm_frame(audio->sound, frame);
}
