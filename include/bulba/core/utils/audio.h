#ifndef BULBA_CORE_UTILS_AUDIO_H
#define BULBA_CORE_UTILS_AUDIO_H

#include "bulba/core/utils/miniaudio.h"

typedef struct {
  ma_engine engine;
} BLB_IO_Audio;

BLB_IO_Audio *BLB_InitAudio(void);
void BLB_ShutdownAudio(BLB_IO_Audio *io_audio);

#endif
