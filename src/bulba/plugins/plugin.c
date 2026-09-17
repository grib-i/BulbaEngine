#include "bulba/plugins/plugin.h"

#include <stdlib.h>
#include <string.h>

#include "bulba/core/platform.h"

struct BLB_Plugin {
  BLB_PlatformLibrary library;
  char *path;
  char *name;
  bool loaded;
  BLB_PluginUnloadFn unload;
  BLB_PluginAPI api;
};

static void default_free(void *memory, void *user_data) {
  (void)user_data;
  free(memory);
}

static void *default_alloc(size_t size, void *user_data) {
  (void)user_data;
  return malloc(size);
}

static void default_log(int level, const char *message, void *user_data) {
  (void)level;
  (void)message;
  (void)user_data;
}

BLB_Plugin *BLB_Plugin_Load(const char *path, const BLB_PluginAPI *api) {
  if (!path)
    return NULL;

  BLB_PlatformLibrary library = BLB_Platform_LoadLibrary(path);
  if (!library)
    return NULL;

  BLB_PluginGetABIFn get_abi = (BLB_PluginGetABIFn)BLB_Platform_LoadSymbol(library, "BLB_PluginGetABI");
  BLB_PluginLoadFn load = (BLB_PluginLoadFn)BLB_Platform_LoadSymbol(library, "BLB_PluginLoad");
  BLB_PluginUnloadFn unload = (BLB_PluginUnloadFn)BLB_Platform_LoadSymbol(library, "BLB_PluginUnload");
  BLB_PluginGetInfoFn get_info = (BLB_PluginGetInfoFn)BLB_Platform_LoadSymbol(library, "BLB_PluginGetInfo");

  if (!get_abi || !load || !unload || get_abi() != BLB_PLUGIN_API_VERSION) {
    BLB_Platform_UnloadLibrary(library);
    return NULL;
  }

  BLB_Plugin *plugin = calloc(1, sizeof(*plugin));
  if (!plugin) {
    BLB_Platform_UnloadLibrary(library);
    return NULL;
  }

  plugin->library = library;
  plugin->path = BLB_Platform_DuplicateString(path);
  plugin->unload = unload;
  if (!plugin->path) {
    BLB_Platform_UnloadLibrary(library);
    free(plugin);
    return NULL;
  }

  plugin->api.api_version = BLB_PLUGIN_API_VERSION;
  plugin->api.alloc = default_alloc;
  plugin->api.free = default_free;
  plugin->api.log = default_log;
  if (api) {
    plugin->api = *api;
    plugin->api.api_version = BLB_PLUGIN_API_VERSION;
  }

  if (load(&plugin->api) != 0) {
    BLB_Platform_UnloadLibrary(library);
    free(plugin->path);
    free(plugin);
    return NULL;
  }

  if (get_info) {
    const BLB_PluginInfo *info = get_info();
    if (info && info->name)
      plugin->name = BLB_Platform_DuplicateString(info->name);
  }

  plugin->loaded = true;
  return plugin;
}

void BLB_Plugin_Unload(BLB_Plugin *plugin) {
  if (!plugin)
    return;
  if (plugin->loaded && plugin->unload)
    plugin->unload();
  BLB_Platform_UnloadLibrary(plugin->library);
  free(plugin->name);
  free(plugin->path);
  free(plugin);
}

const char *BLB_Plugin_GetName(const BLB_Plugin *plugin) {
  return plugin ? plugin->name : NULL;
}

const char *BLB_Plugin_GetPath(const BLB_Plugin *plugin) {
  return plugin ? plugin->path : NULL;
}

bool BLB_Plugin_IsLoaded(const BLB_Plugin *plugin) {
  return plugin && plugin->loaded;
}
