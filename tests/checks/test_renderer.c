#include "bulba/graphics/vulkan/renderer.h"
#include "test.h"

typedef struct {
  int begun;
  int ended;
  int result;
  int destroyed;
  unsigned draws;
  double pose_checksum;
} Mock_Backend;

static int begin(void *backend) {
  Mock_Backend *mock = backend;
  ++mock->begun;
  return mock->result;
}

static int end(void *backend) {
  ++((Mock_Backend *)backend)->ended;
  return 0;
}

static void destroy(void *backend) {
  ++((Mock_Backend *)backend)->destroyed;
}

static void viewport(const void *backend, uint32_t *width, uint32_t *height) {
  (void)backend;
  *width = 800;
  *height = 600;
}

static void draw_pose(void *backend, const BLB_Polygon3D *polygon, const float *mvp, const float *model_rows, float r, float g, float b, float a,
                      const BLB_RenderMaterial *material, BLB_Texture *texture) {
  (void)mvp;
  (void)model_rows;
  (void)r;
  (void)g;
  (void)b;
  (void)a;
  (void)material;
  (void)texture;

  Mock_Backend *mock = backend;

  ++mock->draws;
  for (size_t i = 0; i < polygon->vertex_count; ++i) {
    BLB_Vec3 v = polygon->vertices[i];

    mock->pose_checksum += (v.x + 3 * v.y + 7 * v.z) * (i + 1);
  }
}

static void frame_state_and_backend_ownership(void) {
  const BLB_RenderBackendOps ops = {.name = "test mock", .begin_frame = begin, .end_frame = end, .destroy = destroy, .get_viewport = viewport};

  Mock_Backend mock = {0};
  BLB_Renderer renderer;

  TEST_CHECK(BLB_Renderer_Attach(&renderer, BLB_RENDER_API_NONE, &mock, &ops) == 0);
  TEST_CHECK(!strcmp(BLB_Renderer_GetAPIName(&renderer), "test mock"));
  TEST_CHECK(BLB_RendererBeginFrame(&renderer) == 0);
  TEST_CHECK(BLB_RendererIsFrameReady(&renderer));
  TEST_CHECK(BLB_RendererBeginFrame(&renderer) == -1);
  TEST_CHECK(BLB_RendererEndFrame(&renderer) == 0);
  TEST_CHECK(!BLB_RendererIsFrameReady(&renderer));
  TEST_CHECK(BLB_RendererEndFrame(&renderer) == -1);
  TEST_CHECK(mock.begun == 1 && mock.ended == 1);
  uint32_t width, height;

  BLB_RendererGetViewport(&renderer, &width, &height);
  TEST_CHECK(width == 800 && height == 600);
  mock.result = -2;
  TEST_CHECK(BLB_RendererBeginFrame(&renderer) == 1);
  TEST_CHECK(renderer.swapchain_pending);
  BLB_Renderer_Destroy(&renderer);
  TEST_CHECK(mock.destroyed == 0);
  TEST_CHECK(!renderer.backend && !renderer.ops);
  TEST_CHECK(BLB_RendererBeginFrame(NULL) == -1);
}

/* Exercise the public frame path, not the animation helper in isolation. */
static void scene_advances_skeletal_animation(void) {
  const BLB_RenderBackendOps ops = {
      .name = "animation frame mock", .begin_frame = begin, .end_frame = end, .get_viewport = viewport, .draw_polygon3d = draw_pose};
  const char *paths[] = {"assets/tests/models/fox/Fox.fbx", "assets/tests/models/fox/Fox.gltf"};

  for (size_t format = 0; format < 2; ++format) {
    Mock_Backend mock = {0};
    BLB_Renderer renderer;
    BLB_Scene *scene = BLB_CreateScene("skeletal frame regression");
    BLB_Object3D *object = format == 0 ? BLB_LoadModelFBX(paths[format], BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV)
                                       : BLB_LoadModelGLTF(paths[format], BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);

    TEST_REQUIRE(scene && object);
    scene->camera = BLB_CreateCamera(BLB_V3(0, 35, 200));
    TEST_REQUIRE(scene->camera);
    TEST_CHECK(BLB_Renderer_Attach(&renderer, BLB_RENDER_API_NONE, &mock, &ops) == 0);
    TEST_CHECK(BLB_AddObject3D(scene, object) == 0);
    TEST_NEAR(BLB_Model_GetAnimationDuration(object), 0, 1e-6);
    TEST_CHECK(BLB_Model_PlayAnimation(object, "Run", true) == 0);
    float duration = BLB_Model_GetAnimationDuration(object);

    TEST_CHECK(duration > 0.3f && duration < 2);

    size_t bytes = object->polygon->vertex_count * sizeof(BLB_Vec3);
    BLB_Vec3 *before = malloc(bytes);

    TEST_REQUIRE(before);
    memcpy(before, object->polygon->vertices, bytes);
    BLB_SetSceneDeltaTime(scene, 0.1f);
    TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.1f, 1e-5);
    TEST_CHECK(memcmp(before, object->polygon->vertices, bytes) != 0);

    TEST_CHECK(mock.draws > 0);
    double first_pose = mock.pose_checksum;

    mock.draws = 0;
    mock.pose_checksum = 0;
    memcpy(before, object->polygon->vertices, bytes);
    BLB_SetSceneDeltaTime(scene, 0);
    TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.1f, 1e-5);
    TEST_CHECK(memcmp(before, object->polygon->vertices, bytes) == 0);

    TEST_CHECK(mock.draws > 0);
    TEST_NEAR(mock.pose_checksum, first_pose, 1e-5);
    mock.draws = 0;
    mock.pose_checksum = 0;

    BLB_Model_SetAnimationSpeed(object, 2);
    BLB_SetSceneDeltaTime(scene, 0.1f);
    TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.3f, 1e-5);
    TEST_CHECK(memcmp(before, object->polygon->vertices, bytes) != 0);

    TEST_CHECK(mock.draws > 0);
    TEST_CHECK(fabs(mock.pose_checksum - first_pose) > 1);

    mock.result = -2;
    BLB_SetSceneDeltaTime(scene, 0.1f);
    TEST_CHECK(BLB_DrawScene(scene, &renderer) == 1);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.3f, 1e-5);
    TEST_CHECK(mock.begun == 4 && mock.ended == 3);

    mock.result = 0;
    scene->enabled = false;
    BLB_SetSceneDeltaTime(scene, 0.1f);
    TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.3f, 1e-5);

    scene->enabled = true;
    BLB_Model_SetAnimationSpeed(object, 1);
    TEST_CHECK(BLB_Model_SetAnimationTime(object, duration - 0.05f) == 0);
    BLB_SetSceneDeltaTime(scene, 0.1f);
    TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.05f, 1e-4);

    TEST_CHECK(BLB_Model_PlayAnimation(object, "Run", false) == 0);
    TEST_CHECK(BLB_Model_SetAnimationTime(object, duration - 0.05f) == 0);
    BLB_SetSceneDeltaTime(scene, 0.1f);
    TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), duration, 1e-4);
    BLB_Model_StopAnimation(object);
    TEST_NEAR(BLB_Model_GetAnimationDuration(object), 0, 1e-6);

    free(before);
    BLB_RemoveObject3D(scene, object);
    Test_Destroy3D(object);
    BLB_DestroyCamera(scene->camera);
    scene->camera = NULL;
    BLB_DestroyScene(scene);
    BLB_Renderer_Destroy(&renderer);
  }
}

static void draw_lit_sprite(void *backend, const BLB_Polygon2D *polygon, float width, float height, const BLB_RenderMaterial *material,
                            BLB_Texture *texture, const BLB_RenderInstanceData *instances, size_t count, bool screen) {
  (void)polygon;
  (void)width;
  (void)height;
  (void)texture;
  (void)instances;
  Mock_Backend *mock = backend;

  TEST_CHECK(count == 1);
  TEST_CHECK(material->lighting_enabled == (mock->destroyed == 0));
  TEST_CHECK(material->unlit == (mock->destroyed != 0));
  if (screen) {
    TEST_CHECK(!material->depth_enabled);
    TEST_CHECK(!material->depth_write);
  }
  ++mock->draws;
}

static int ensure_instances(void *backend, size_t count) {
  (void)backend;
  (void)count;
  return 0;
}

static void overlay_preserves_explicit_2d_lighting(void) {
  const BLB_RenderBackendOps ops = {.name = "2D lighting mock",
                                    .begin_frame = begin,
                                    .end_frame = end,
                                    .get_viewport = viewport,
                                    .draw_instanced_polygon2d = draw_lit_sprite,
                                    .ensure_instance_capacity = ensure_instances};
  Mock_Backend mock = {0};
  BLB_Renderer renderer;
  BLB_Scene *scene = BLB_CreateScene("2D lighting regression");
  BLB_Object2D *object = BLB_CreateSquare2D(BLB_V2(80, 80), BLB_V2(100, 100), NULL, true);
  BLB_Material *material = BLB_Material_Create2D();

  TEST_REQUIRE(scene && object && material);
  scene->camera = BLB_CreateCamera(BLB_V3(0, 0, 10));
  TEST_REQUIRE(scene->camera);
  BLB_Material_SetLighting(material, true);
  BLB_Material_SetUnlit(material, false);
  BLB_Object2D_SetMaterial(object, material);
  TEST_CHECK(BLB_AddObject2D(scene, object) == 0);
  TEST_CHECK(BLB_Renderer_Attach(&renderer, BLB_RENDER_API_NONE, &mock, &ops) == 0);
  TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
  TEST_CHECK(mock.draws == 1);

  /* The expected material is independent of the begin-frame result. */
  mock.destroyed = 1;
  BLB_Material_SetUnlit(material, true);
  TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
  TEST_CHECK(mock.draws == 2);
  mock.destroyed = 0;
  BLB_Material_SetUnlit(material, false);
  BLB_Object2D_SetPosition(object, BLB_V2(0, 0));
  BLB_Object2D_SetCameraAttached(object, false);
  TEST_CHECK(BLB_DrawScene(scene, &renderer) == 0);
  TEST_CHECK(mock.draws == 3);

  BLB_RemoveObject2D(scene, object);
  Test_Destroy2D(object);
  BLB_Material_Release(material);
  BLB_DestroyCamera(scene->camera);
  scene->camera = NULL;
  BLB_DestroyScene(scene);
  BLB_Renderer_Destroy(&renderer);
}

static void gpu_2d_light_space_and_types(void) {
  VULKAN *backend = calloc(1, sizeof(*backend));
  VulkanLightBufferData buffer = {0};
  BLB_Light2D *point = BLB_CreateLight2D(BLB_LIGHT_POINT, BLB_V2(200, 250), 0);
  BLB_Light2D *spot = BLB_CreateLight2D(BLB_LIGHT_SPOT, BLB_V2(3, 4), 90);
  BLB_Light2D *directional = BLB_CreateLight2D(BLB_LIGHT_DIRECTIONAL, BLB_V2(0, 0), 0);

  TEST_REQUIRE(backend && point && spot && directional);
  BLB_Object2D_SetCameraAttached(point->object, true);
  BLB_Object2D_SetCameraAttached(spot->object, false);
  BLB_Object2D_SetCameraAttached(directional->object, false);
  backend->light_buffers_2d[0].mapped = &buffer;
  backend->lights2d[0] = point;
  backend->lights2d[1] = spot;
  backend->lights2d[2] = directional;
  backend->light2d_count = 3;
  backend->light_buffer_dirty_2d = true;
  point->range = 700;
  spot->range = 8;
  update_light_buffer_2d(backend);

  TEST_CHECK(buffer.light_count == 3);
  TEST_NEAR(buffer.lights[0].position[3], 1, 0);
  TEST_NEAR(buffer.lights[1].position[3], 0, 0);
  TEST_NEAR(buffer.lights[2].position[3], 0, 0);
  TEST_NEAR(buffer.lights[0].cone[0], 700, 0);
  TEST_NEAR(buffer.lights[1].cone[0], 8, 0);
  TEST_NEAR(buffer.lights[0].cone[3], BLB_LIGHT_POINT, 0);
  TEST_NEAR(buffer.lights[1].cone[3], BLB_LIGHT_SPOT, 0);
  TEST_NEAR(buffer.lights[2].cone[3], BLB_LIGHT_DIRECTIONAL, 0);
  TEST_NEAR(buffer.lights[1].direction[0], 0, 1e-5);
  TEST_NEAR(buffer.lights[1].direction[1], 1, 1e-5);

  spot->enabled = false;
  backend->light_buffer_dirty_2d = true;
  update_light_buffer_2d(backend);
  TEST_CHECK(buffer.light_count == 2);
  BLB_DestroyLight2D(point);
  BLB_DestroyLight2D(spot);
  BLB_DestroyLight2D(directional);
  free(backend);
}

static const Test_Case cases[] = {TEST_CASE(frame_state_and_backend_ownership), TEST_CASE(scene_advances_skeletal_animation),
                                  TEST_CASE(overlay_preserves_explicit_2d_lighting), TEST_CASE(gpu_2d_light_space_and_types)};
const Test_Suite test_renderer = {"renderer", cases, sizeof(cases) / sizeof(cases[0])};
