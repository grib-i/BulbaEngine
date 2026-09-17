#ifndef BULBA_PLUGINS_PLUGIN_H
#define BULBA_PLUGINS_PLUGIN_H

#include "bulba/plugins/bpl.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define BLB_PLUGIN_EXPORT __declspec(dllexport)
#else
#define BLB_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

typedef struct BLB_Plugin BLB_Plugin;
typedef void (*BLB_PluginTypeRegisterFn)(const char *name, uint32_t type_id, void *user_data);
typedef void *(*BLB_PluginAllocFn)(size_t size, void *user_data);
typedef void (*BLB_PluginFreeFn)(void *memory, void *user_data);
typedef void (*BLB_PluginLogFn)(int level, const char *message, void *user_data);

typedef struct {
  uint32_t api_version;
  uint32_t engine_version_major;
  uint32_t engine_version_minor;
  BLB_PluginAllocFn alloc;
  BLB_PluginFreeFn free;
  BLB_PluginLogFn log;
  BLB_PluginTypeRegisterFn register_type;
  void *user_data;
} BLB_PluginAPI;

typedef uint32_t (*BLB_PluginGetABIFn)(void);
typedef int (*BLB_PluginLoadFn)(const BLB_PluginAPI *api);
typedef void (*BLB_PluginUnloadFn)(void);

typedef struct {
  const char *name;
  const char *version;
  const char *author;
} BLB_PluginInfo;

typedef const BLB_PluginInfo *(*BLB_PluginGetInfoFn)(void);

BLB_Plugin *BLB_Plugin_Load(const char *path, const BLB_PluginAPI *api);
void BLB_Plugin_Unload(BLB_Plugin *plugin);
const char *BLB_Plugin_GetName(const BLB_Plugin *plugin);
const char *BLB_Plugin_GetPath(const BLB_Plugin *plugin);
bool BLB_Plugin_IsLoaded(const BLB_Plugin *plugin);

#endif
