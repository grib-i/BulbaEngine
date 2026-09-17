#ifndef BULBA_PLUGINS_BPL_H
#define BULBA_PLUGINS_BPL_H

#include <stdint.h>

#define BLB_BPL_VERSION 1u
#define BLB_PLUGIN_API_VERSION 1u
#define BLB_BPL_MAGIC 0x314C5042u

typedef enum {
  BLB_BPL_PLATFORM_LINUX = 1,
  BLB_BPL_PLATFORM_WINDOWS = 2,
  BLB_BPL_PLATFORM_MACOS = 3
} BLB_BPLPlatform;

typedef enum {
  BLB_BPL_ARCH_X86_64 = 1,
  BLB_BPL_ARCH_ARM64 = 2
} BLB_BPLArchitecture;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t header_size;
  uint32_t platform;
  uint32_t architecture;
  uint32_t api_version;
  uint32_t manifest_offset;
  uint32_t manifest_size;
  uint32_t payload_offset;
  uint32_t payload_size;
} BLB_BPLHeader;

#endif
