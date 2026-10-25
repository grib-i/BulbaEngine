#include "test.h"

static void texture_lifetime(void) {
  unsigned char pixels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
  BLB_Texture *texture = BLB_Texture_Create2D(2, 2, pixels, sizeof(pixels));

  TEST_REQUIRE(texture);
  TEST_CHECK(BLB_Texture_IsLoaded(texture));
  TEST_CHECK(BLB_Texture_GetWidth(texture) == 2 && BLB_Texture_GetHeight(texture) == 2);
  TEST_CHECK(BLB_Texture_GetType(texture) == BLB_TEXTURE_2D);
  TEST_CHECK(BLB_Texture_GetFormat(texture) == BLB_TEXTURE_FORMAT_RGBA8);
  TEST_CHECK(!memcmp(texture->pixels, pixels, sizeof(pixels)));
  BLB_Texture_Retain(texture);
  TEST_CHECK(texture->ref_count == 2);
  BLB_Texture_Release(texture);
  TEST_CHECK(texture->ref_count == 1);

  BLB_Object2D *object = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 0), texture, false);

  TEST_CHECK(object);
  if (object) {
    TEST_CHECK(texture->ref_count >= 2);
    TEST_CHECK(BLB_Object2D_GetTexture(object) == texture);
    BLB_Object2D_SetTexture(object, NULL);
    TEST_CHECK(BLB_Object2D_GetTexture(object) == NULL);
    Test_Destroy2D(object);
  }
  BLB_Texture_Release(texture);
  BLB_Texture_Release(NULL);
  TEST_CHECK(BLB_Texture_Load2D("assets/tests/does-not-exist.png") == NULL);
}

static void material_properties(void) {
  BLB_Material *material = BLB_Material_Create3D();

  TEST_REQUIRE(material);
  BLB_Material_SetName(material, "test material");
  BLB_Material_SetBaseColor(material, 0.2f, 0.4f, 0.6f, 1);
  BLB_Material_SetPBR(material, 0.8f, 0.3f);
  BLB_Material_SetEmission(material, 1, 0.5f, 0, 1, 2);
  BLB_Material_SetAlphaCutoff(material, 0.25f);
  BLB_Material_SetNormal(material, 0.7f);
  BLB_Material_SetOcclusion(material, 0.6f);
  BLB_Material_SetSpecular(material, 0.5f, 1, 0.9f, 0.8f);
  BLB_Material_SetIOR(material, 1.5f);
  BLB_Material_SetTransmission(material, 0.4f);
  BLB_Material_SetVolume(material, 0.2f, 4, 0.8f, 0.7f, 0.6f);
  BLB_Material_SetClearcoat(material, 0.5f, 0.2f, 1);
  BLB_Material_SetSheen(material, 0.2f, 0.3f, 0.4f, 0.5f);
  BLB_Material_SetIridescence(material, 0.4f, 1.3f, 100, 400);
  BLB_Material_SetAnisotropy(material, 0.6f, 0.8f);
  BLB_Material_SetDispersion(material, 0.1f);
  BLB_Material_SetGlow(material, 1, 2, 1.5f);
  BLB_Material_SetTemperature(material, 6500);
  BLB_Material_SetRenderMode(material, BLB_RENDER_TRANSPARENT);
  BLB_Material_SetAlphaMode(material, BLB_ALPHA_BLEND);
  BLB_Material_SetLighting(material, false);
  BLB_Material_SetDepth(material, true, false);
  BLB_Material_SetDoubleSided(material, true);
  BLB_Material_SetUnlit(material, true);

  TEST_CHECK(!strcmp(material->name, "test material"));
  TEST_NEAR(material->base_color[1], 0.4, 1e-6);
  TEST_NEAR(material->metallic, 0.8, 1e-6);
  TEST_NEAR(material->roughness, 0.3, 1e-6);
  TEST_NEAR(material->emission_strength, 2, 1e-6);
  TEST_NEAR(material->alpha_cutoff, 0.25, 1e-6);
  TEST_NEAR(material->normal_scale, 0.7, 1e-6);
  TEST_NEAR(material->occlusion_strength, 0.6, 1e-6);
  TEST_NEAR(material->specular_factor, 0.5, 1e-6);
  TEST_NEAR(material->ior, 1.5, 1e-6);
  TEST_NEAR(material->transmission, 0.4, 1e-6);
  TEST_NEAR(material->clearcoat_factor, 0.5, 1e-6);
  TEST_NEAR(material->sheen_roughness, 0.5, 1e-6);
  TEST_NEAR(material->iridescence_factor, 0.4, 1e-6);
  TEST_NEAR(material->anisotropy_strength, 0.6, 1e-6);
  TEST_NEAR(material->dispersion, 0.1, 1e-6);
  TEST_CHECK(BLB_Material_IsGlowing(material));
  TEST_CHECK(material->unlit && material->double_sided && !material->depth_write);

  BLB_Material *copy = BLB_Material_Clone(material);

  TEST_CHECK(copy && copy != material);
  if (copy) {
    TEST_NEAR(copy->metallic, material->metallic, 1e-6);
    BLB_Material_SetPBR(copy, 0, 1);
    TEST_NEAR(material->metallic, 0.8, 1e-6);
    BLB_Material_Release(copy);
  }
  BLB_Material_Retain(material);
  TEST_CHECK(material->ref_count == 2);
  BLB_Material_Release(material);
  BLB_Material_Release(material);
}

static void material_texture_transform(void) {
  BLB_MaterialTexture slot;

  BLB_MaterialTexture_Init(&slot);
  TEST_CHECK(!BLB_MaterialTexture_Valid(&slot));
  BLB_MaterialTexture_SetTransform(&slot, 0.1f, 0.2f, 2, 3, 0.5f);
  BLB_MaterialTexture_SetSampler(&slot, BLB_TEXTURE_WRAP_CLAMP_TO_EDGE, BLB_TEXTURE_WRAP_REPEAT, BLB_TEXTURE_FILTER_NEAREST,
                                 BLB_TEXTURE_FILTER_LINEAR);
  BLB_MaterialTexture_SetProjection(&slot, BLB_TEXMAP_UV);
  TEST_NEAR(slot.offset[0], 0.1, 1e-6);
  TEST_NEAR(slot.scale[1], 3, 1e-6);
  TEST_NEAR(slot.rotation, 0.5, 1e-6);
  TEST_CHECK(slot.wrap_u == BLB_TEXTURE_WRAP_CLAMP_TO_EDGE);
  TEST_CHECK(slot.mag_filter == BLB_TEXTURE_FILTER_LINEAR && slot.projection == BLB_TEXMAP_UV);
}

static void sprite_animation(void) {
  unsigned char red[] = {255, 0, 0, 255}, blue[] = {0, 0, 255, 255};
  BLB_Texture *frames[] = {BLB_Texture_Create2D(1, 1, red, 4), BLB_Texture_Create2D(1, 1, blue, 4)};
  BLB_Object2D *object = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 0), NULL, false);

  TEST_CHECK(frames[0] && frames[1] && object);
  if (frames[0] && frames[1] && object) {
    BLB_SetAnimation(object, frames, 0.1f, 2);
    BLB_StartAnimation(object);
    TEST_CHECK(object->animation->enable);
    TEST_CHECK(BLB_Object2D_GetTexture(object) == frames[0]);
    BLB_Object2D_UpdateAnimation(object, 0.11f);
    TEST_CHECK(BLB_Object2D_GetTexture(object) == frames[1]);
    BLB_Object2D_UpdateAnimation(object, 0.1f);
    TEST_CHECK(BLB_Object2D_GetTexture(object) == frames[0]);
    BLB_StopAnimation(object);
    BLB_Object2D_UpdateAnimation(object, 0.11f);
    TEST_CHECK(BLB_Object2D_GetTexture(object) == frames[0]);
    BLB_ClearAnimation(object);
    TEST_CHECK(object->animation->textures_count == 0);
  }
  Test_Destroy2D(object);
  BLB_Texture_Release(frames[0]);
  BLB_Texture_Release(frames[1]);
}

static void text_buffer_and_attachment(void) {
  BLB_Text2D *text = BLB_CreateText2D("FPS: 0", Test_Font(), BLB_V2(10, 20), 20, true);

  TEST_REQUIRE(text);
  TEST_CHECK(!strcmp(text->text, "FPS: 0"));
  BLB_SetText2D(text, "A longer text forces the buffer to grow safely: 0123456789");
  TEST_CHECK(text->text_capacity > strlen(text->text));
  BLB_SetText2D(text, "short");
  TEST_CHECK(!strcmp(text->text, "short"));
  BLB_SetText2D(text, "");
  TEST_CHECK(text->text[0] == '\0');
  BLB_Text2D_SetCameraAttached(text, false);
  TEST_CHECK(!BLB_Text2D_IsCameraAttached(text));
  BLB_DestroyText2D(text);
}

static const Test_Case resource_cases[] = {TEST_CASE(texture_lifetime), TEST_CASE(material_properties), TEST_CASE(material_texture_transform),
                                           TEST_CASE(sprite_animation), TEST_CASE(text_buffer_and_attachment)};
const Test_Suite test_resources = {"resources", resource_cases, sizeof(resource_cases) / sizeof(resource_cases[0])};
