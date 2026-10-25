#include "test.h"

static void offline_audio_controls(void) {
  /* Real decoding and sound state, no audio device and no audible output. */
  BLB_IO_Audio *io = calloc(1, sizeof(*io));

  TEST_REQUIRE(io);
  ma_engine_config config = ma_engine_config_init();
  config.noDevice = MA_TRUE;
  config.channels = 2;
  config.sampleRate = 48000;
  ma_result initialized = ma_engine_init(&config, &io->engine);
  TEST_CHECK(initialized == MA_SUCCESS);
  if (initialized != MA_SUCCESS) {
    free(io);
    return;
  }

  BLB_Audio2D *a = BLB_LoadAudio2D(io, "assets/tests/sounds/space-transmission.mp3", false, false, false, BLB_V2(0, 0));
  BLB_Audio3D *b = BLB_LoadAudio3D(io, "assets/tests/sounds/space-transmission.mp3", false, true, false, BLB_V3(0, 0, -2));

  TEST_CHECK(a && b);
  if (a && b) {
    BLB_SetVolume_Audio2D(a, 0.2f);
    BLB_SetPitch_Audio2D(a, 1.1f);
    BLB_SetLoop_Audio2D(a, true);
    BLB_SetAttenuation_Audio2D(a, 0.8f);
    BLB_SetPosition_Audio2D(a, BLB_V2(2, 3));
    BLB_SetVelocity_Audio2D(a, BLB_V2(1, 0));
    BLB_SetRotation_Audio2D(a, BLB_V2(0, 1));
    TEST_NEAR(a->volume, 0.2, 1e-5);
    TEST_NEAR(a->pitch, 1.1, 1e-5);
    TEST_CHECK(a->loop);
    BLB_PlayAudio_Audio2D(a);
    TEST_CHECK(a->play);
    BLB_SetAudio2DSeek(a, 0.25f, 0);
    BLB_StopAudio_Audio2D(a);
    TEST_CHECK(!a->play);

    BLB_SetVolume_Audio3D(b, 0.3f);
    BLB_SetPitch_Audio3D(b, 0.9f);
    BLB_SetLoop_Audio3D(b, true);
    BLB_SetAttenuation_Audio3D(b, 0.6f);
    BLB_SetPosition_Audio3D(b, BLB_V3(1, 2, -3));
    BLB_SetVelocity_Audio3D(b, BLB_V3(0, 0, 1));
    BLB_SetRotation_Audio3D(b, BLB_V3(0, 0, -1));
    TEST_NEAR(b->volume, 0.3, 1e-5);
    TEST_NEAR(b->pitch, 0.9, 1e-5);
    TEST_CHECK(b->loop);
    BLB_PlayAudio3D(b);
    TEST_CHECK(b->play);
    BLB_SetAudio3DSeek(b, 0.25f, 0);
    float pcm[512 * 2];

    ma_uint64 read = 0;
    TEST_CHECK(ma_engine_read_pcm_frames(&io->engine, pcm, 512, &read) == MA_SUCCESS);
    TEST_CHECK(read == 512);
    BLB_StopAudio3D(b);
    TEST_CHECK(!b->play);
  }

  BLB_DestroyAudio2D(io, a);
  BLB_DestroyAudio3D(io, b);
  BLB_ShutdownAudio(io);
}

static const Test_Case cases[] = {TEST_CASE(offline_audio_controls)};
const Test_Suite test_audio = {"audio", cases, sizeof(cases) / sizeof(cases[0])};
