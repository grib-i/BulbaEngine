#include "audio.h"

int Demo_Audio(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "Audio: looping MP3 from assets/tests", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Text(&demo, "Playing space-transmission.mp3 at 20% volume (requires audio device)", 100, 350, 22);
  demo.audio = BLB_InitAudio();
  if (!demo.audio) {
    demo.failed = true;
    return Demo_Finish(&demo, NULL);
  }
  BLB_Audio2D *sound = BLB_LoadAudio2D(demo.audio, "assets/tests/sounds/space-transmission.mp3", true, false, true, BLB_V2(0, 0));

  if (!sound || BLB_AddAudio2D(demo.scene, sound) != 0) {
    BLB_DestroyAudio2D(demo.audio, sound);
    demo.failed = true;
  } else {
    BLB_SetVolume_Audio2D(sound, 0.2f);
    BLB_PlayAudio_Audio2D(sound);
  }
  return Demo_Finish(&demo, NULL);
}
