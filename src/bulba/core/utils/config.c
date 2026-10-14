#include "bulba/core/utils/config.h"

#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

/*
 * Debug mode on viseble ligt, camera and audio
 */
bool BLB_DEBUG = false;

/*
 * 3d object aggregation and smart path selection. Static compatible objects
 * are baked into nearby clusters, while dynamic objects keep an instance
 * fallback so movement does not force a full scene rebuild every frame.
 */
bool BLB_OBJECT_AGGREGATION = true;
int BLB_OBJECT_AGGREGATION_MIN_OBJECTS = 100;
float BLB_OBJECT_AGGREGATION_MAX_DISTANCE = 15.0f;
bool BLB_OBJECT_AGGREGATION_REBUILD_ON_TRANSFORM_CHANGE = false;
bool BLB_OBJECT_AGGREGATION_STATIC_ONLY = true;
int BLB_OBJECT_AGGREGATION_MAX_CLUSTER_TRIANGLES = 20000;

/*
 * Smart optimization keeps an instance fallback ready for dynamic objects.
 * A transition between static and dynamic state is allowed to rebuild the
 * optimization map, but stable frames never rebuild aggregation.
 */
bool BLB_OBJECT_SMART_OPTIMIZATION = true;
int BLB_OBJECT_OPTIMIZATION_STATIC_RECOVER_FRAMES = 30;
float BLB_OBJECT_OPTIMIZATION_MOTION_POSITION_EPSILON = 0.0005f;
float BLB_OBJECT_OPTIMIZATION_MOTION_ROTATION_EPSILON = 0.05f;

/*
 * Instancing keeps independent transforms on the GPU. Smart optimization
 * uses it as the dynamic path when objects cannot safely stay aggregated.
 */
bool BLB_OBJECT_INSTANCING = true;
int BLB_OBJECT_INSTANCING_MIN_OBJECTS = 100;
int BLB_OBJECT_INSTANCING_MAX_INSTANCES = 80192;
bool BLB_OBJECT_2D_INSTANCING = true;
int BLB_OBJECT_2D_INSTANCING_MIN_OBJECTS = 100;
bool BLB_PARTICLE_INSTANCING = true;
int BLB_PARTICLE_INSTANCING_MAX_INSTANCES = 4096;
bool BLB_PARTICLE_CULLING = true;
int BLB_RENDER_ASYNC_WORKERS = 0;

/*
 * LOD first applies the global detail level, then optionally adds distance
 * based reductions. Generated levels are cached and reused by matching mesh
 * data instead of being rebuilt for every object.
 */
bool BLB_OBJECT_LOD = true;
int BLB_OBJECT_LOD_LEVELS = 3;
float BLB_OBJECT_LOD_ERROR = 0.01f;
float BLB_OBJECT_LOD_REDUCTION = 0.5f;
float BLB_OBJECT_LOD_FALLBACK_ERROR = 0.08f;
int BLB_OBJECT_LOD_DEFAULT_LEVEL = 0;
bool BLB_OBJECT_LOD_DISTANCE_OPTIMIZATION = true;
float BLB_OBJECT_LOD_DISTANCE_START = 100.0f;
float BLB_OBJECT_LOD_DISTANCE_STEP = 90.0f;
int BLB_OBJECT_LOD_MIN_LEVEL = 0;
int BLB_OBJECT_LOD_MIN_TRIANGLES = 16;
int BLB_OBJECT_LOD_SHADOW_MAX_LEVEL = 2;
bool BLB_OBJECT_LOD_CACHE = true;
int BLB_OBJECT_LOD_CACHE_MAX_ENTRIES = 4096;
size_t BLB_OBJECT_LOD_CACHE_MAX_BYTES = 128u * 1024u * 1024u;
bool BLB_OBJECT_LOD_PRESERVE_BOUNDARIES = true;
bool BLB_OBJECT_LOD_PRESERVE_NORMALS = true;

/*
 * Visibility cache is intentionally disabled by default. Camera movement
 * invalidates its entries; enable it after profiling mostly-static-camera scenes.
 */
bool BLB_OBJECT_VISIBILITY_CACHE = false;
int BLB_OBJECT_VISIBILITY_CACHE_MAX_ENTRIES = 65536;
int BLB_OBJECT_VISIBILITY_CACHE_MAX_AGE = 1;
float BLB_OBJECT_VISIBILITY_CACHE_FAR_DISTANCE = 100.0f;
int BLB_OBJECT_VISIBILITY_CACHE_FAR_MAX_AGE = 3;

bool BLB_OBJECT_CULLING = true;
bool BLB_OBJECT_FRUSTUM_CULLING = true;
bool BLB_OBJECT_DISTANCE_CULLING = true;
float BLB_OBJECT_MAX_RENDER_DISTANCE = 1000000.0f;
float BLB_OBJECT_CULLING_MARGIN = 0.05f;
bool BLB_OBJECT_SHADOW_CULL_BY_CAMERA = true;
float BLB_OBJECT_MAX_SHADOW_RENDER_DISTANCE = 180.0f;

/* HSA currently does not build cluster LODs in object_aggregation.c;
 * leave disabled until that path is implemented and benchmarked.
 */
bool BLB_HSA = false;
int BLB_HSA_LEVELS = 1;
float BLB_HSA_ERROR = 0.01f;
float BLB_HSA_REDUCTION_FACTOR = 0.5f;
int BLB_HSA_MAX_TRIANGLES = 40096;

/*
 * defoult cache path
 * linux home/user/.local/share/bulba/cache/
 * windows C:\Users\user\AppData\Local/cache
 */
char BLB_DEFAULT_CACHE_PATH[BLB_PATH_MAX];

static void blb_make_directory(const char *path) {
  if (!path || !path[0])
    return;

#ifdef _WIN32
  if (_mkdir(path) != 0 && errno != EEXIST)
    return;
#else
  if (mkdir(path, 0755) != 0 && errno != EEXIST)
    return;
#endif
}

void BLB_InitConfig(void) {
#ifdef _WIN32
  const char *local = getenv("LOCALAPPDATA");
  if (!local || !local[0])
    local = ".";

  char root[BLB_PATH_MAX];
  char cache[BLB_PATH_MAX];

  snprintf(root, sizeof(root), "%s/bulba", local);
  snprintf(cache, sizeof(cache), "%s/cache", root);

  blb_make_directory(root);
  blb_make_directory(cache);

  snprintf(BLB_DEFAULT_CACHE_PATH, sizeof(BLB_DEFAULT_CACHE_PATH), "%s", cache);
#else
  const char *home = getenv("HOME");
  if (!home || !home[0])
    home = ".";

  char local_dir[BLB_PATH_MAX];
  char share_dir[BLB_PATH_MAX];
  char root[BLB_PATH_MAX];
  char cache[BLB_PATH_MAX];

  if (snprintf(local_dir, sizeof(local_dir), "%s/.local", home) >= (int)sizeof(local_dir))
    return;
  if (snprintf(share_dir, sizeof(share_dir), "%s/share", local_dir) >= (int)sizeof(share_dir))
    return;
  if (snprintf(root, sizeof(root), "%s/bulba", share_dir) >= (int)sizeof(root))
    return;
  if (snprintf(cache, sizeof(cache), "%s/cache", root) >= (int)sizeof(cache))
    return;

  blb_make_directory(local_dir);
  blb_make_directory(share_dir);
  blb_make_directory(root);
  blb_make_directory(cache);

  snprintf(BLB_DEFAULT_CACHE_PATH, sizeof(BLB_DEFAULT_CACHE_PATH), "%s", cache);
#endif
}
