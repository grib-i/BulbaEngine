#include "bulba/core/utils/audio.h"

#define MINIAUDIO_IMPLEMENTATION
#include "bulba/core/math3v/miniaudio.h"

#include <stdlib.h>

BLB_IO_Audio *BLB_InitAudio(void) {
  BLB_IO_Audio *io_audio = malloc(sizeof(BLB_IO_Audio));
  if (!io_audio)
    return NULL;

  if (ma_engine_init(NULL, &io_audio->engine) != MA_SUCCESS) {
    free(io_audio);
    return NULL;
  }

  return io_audio;
}

void BLB_ShutdownAudio(BLB_IO_Audio *io_audio) {
  if (!io_audio)
    return;

  ma_engine_uninit(&io_audio->engine);
  free(io_audio);
}
