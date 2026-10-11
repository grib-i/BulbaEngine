#include "bulba/core/objects2d/audio2d.h"
#include "bulba/core/objects2d/square.h"
#include "bulba/core/utils/audio.h"
#include "bulba/core/utils/config.h"

#include "volume_off_256dp.h"
#include "volume_up_256dp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static ma_uint32 BLB_AudioFlags(const BLB_Audio2D *audio) {
  ma_uint32 flags = 0;

  if (audio->stream)
    flags |= MA_SOUND_FLAG_STREAM;

  return flags;
}

static void BLB_ConfigureAudio(BLB_Audio2D *audio) {
  if (!audio)
    return;

  ma_sound_set_volume(audio->sound, audio->volume);
  ma_sound_set_pitch(audio->sound, audio->pitch);
  ma_sound_set_looping(audio->sound, audio->loop);
  ma_sound_set_position(audio->sound, audio->obj->position.x, audio->obj->position.y, 0);
}

BLB_Audio2D *BLB_CreateAudio2D(BLB_IO_Audio *io_audio, bool loop, bool stream, float volume, float attenuation, float pitch, bool play,
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

BLB_Audio2D *BLB_LoadAudio2D(BLB_IO_Audio *io_audio, const char *path, bool play, bool surround, bool loop, HMM_Vec2 position) {
  if (!io_audio || !path)
    return NULL;

  BLB_Audio2D *audio = calloc(1, sizeof(BLB_Audio2D));
  if (!audio)
    return NULL;

  audio->sound = malloc(sizeof(ma_sound));
  if (!audio->sound) {
    free(audio);
    return NULL;
  }

  audio->result = malloc(sizeof(*audio->result));
  if (!audio->result) {
    free(audio->sound);
    free(audio);
    return NULL;
  }

  // The audio engine is borrowed; this object must not allocate or free a copy.
  audio->io_audio = io_audio;

  audio->path = strdup(path);
  if (!audio->path) {
    free(audio->result);
    free(audio->sound);
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
    free(audio->result);
    free(audio->sound);
    free(audio);
    return NULL;
  }

  audio->obj = BLB_CreateSquare2D(HMM_V2(1.0f, 1.0f), position, NULL, false);
  if (!audio->obj) {
    ma_sound_uninit(audio->sound);
    free(audio->path);
    free(audio->result);
    free(audio->sound);
    free(audio);
    return NULL;
  }

  audio->obj->visible = false;

  BLB_ConfigureAudio(audio);
  audio->play = play && ma_sound_start(audio->sound) == MA_SUCCESS;

  if (BLB_DEBUG) {
    audio->audio_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_volume_up_256dp_width, BLB_TEXTURE_volume_up_256dp_height,
                                                      BLB_TEXTURE_volume_up_256dp_pixels, BLB_TEXTURE_volume_up_256dp_pixel_size);

    audio->audio_off_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_volume_off_256dp_width, BLB_TEXTURE_volume_off_256dp_height,
                                                          BLB_TEXTURE_volume_off_256dp_pixels, BLB_TEXTURE_volume_off_256dp_pixel_size);

    BLB_Object2D_SetTexture(audio->obj, audio->play ? audio->audio_debug_texture : audio->audio_off_debug_texture);
    audio->obj->visible = true;
  }

  return audio;
}

void BLB_DestroyAudio2D(BLB_IO_Audio *io_audio, BLB_Audio2D *audio) {
  (void)io_audio;

  if (!audio)
    return;

  if (audio->sound) {
    ma_sound_uninit(audio->sound);
    free(audio->sound);
  }
  if (audio->obj)
    BLB_DestroySquare2D(audio->obj);
  if (audio->audio_debug_texture)
    BLB_Texture_Release(audio->audio_debug_texture);
  if (audio->audio_off_debug_texture)
    BLB_Texture_Release(audio->audio_off_debug_texture);

  free(audio->result);
  free(audio->path);
  free(audio);
}

void BLB_ReleaseAudio2D(BLB_IO_Audio *io_audio, BLB_Audio2D *audio) { BLB_DestroyAudio2D(io_audio, audio); }

void BLB_SetVolume_Audio2D(BLB_Audio2D *audio, float volume) {
  if (!audio)
    return;

  audio->volume = volume;
  ma_sound_set_volume(audio->sound, volume);
}

void BLB_PlayAudio2D(BLB_Audio2D *audio) {
  if (!audio)
    return;

  if (ma_sound_start(audio->sound) == MA_SUCCESS)
    audio->play = true;
}

void BLB_StopAudio2D(BLB_Audio2D *audio) {
  if (!audio)
    return;

  if (ma_sound_stop(audio->sound) == MA_SUCCESS)
    audio->play = false;
}

void BLB_SetPosition_Audio2D(BLB_Audio2D *audio, HMM_Vec2 position) {
  if (!audio)
    return;

  BLB_Object2D_SetPosition(audio->obj, position);

  ma_sound_set_position(audio->sound, position.x, position.y, 0.0f);
}

void BLB_SetVelocity_Audio2D(BLB_Audio2D *audio, HMM_Vec2 velocity) {
  if (!audio)
    return;

  ma_sound_set_velocity(audio->sound, velocity.x, velocity.y, 0.0f);
}

void BLB_SetRotation_Audio2D(BLB_Audio2D *audio, HMM_Vec2 rotation) {
  if (!audio)
    return;

  ma_sound_set_direction(audio->sound, rotation.x, rotation.y, 0.0f);

  // The direction vector determines an absolute facing angle; this is a
  // setter, not angular velocity, so do not apply delta_time or accumulate it.
  if (HMM_LenV2(rotation) > 1e-7f)
    BLB_Object2D_SetRotation(audio->obj, atan2f(rotation.y, rotation.x) * (180.0f / HMM_PI));
}

void BLB_SetAttenuation_Audio2D(BLB_Audio2D *audio, float attenuation) {
  if (!audio)
    return;

  audio->attenuation = attenuation;

  ma_sound_set_attenuation_model(audio->sound, ma_attenuation_model_inverse);
  ma_sound_set_rolloff(audio->sound, attenuation);
}

void BLB_SetLoop_Audio2D(BLB_Audio2D *audio, bool loop) {
  if (!audio)
    return;

  audio->loop = loop;
  ma_sound_set_looping(audio->sound, loop);
}

void BLB_SetPitch_Audio2D(BLB_Audio2D *audio, float pitch) {
  if (!audio)
    return;

  audio->pitch = pitch;
  ma_sound_set_pitch(audio->sound, pitch);
}

void BLB_SetAudio2DSeek(BLB_Audio2D *audio, float seconds, int frame) {
  if (!audio || (seconds == 0 && frame == 0) || (seconds != 0 && frame != 0))
    return;

  if (seconds != 0)
    ma_sound_seek_to_second(audio->sound, seconds);
  else if (frame != 0)
    ma_sound_seek_to_pcm_frame(audio->sound, frame);
}
