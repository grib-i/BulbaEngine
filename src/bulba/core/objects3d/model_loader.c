#include "bulba/core/objects3d/model_loader.h"

#include <stdlib.h>
#include <string.h>

int BLB_LoadModelOBJ(const char *path, Mesh *m) {
  (void)path;
  if (!m)
    return -1;
  memset(m, 0, sizeof(*m));
  return -1;
}

void BLB_DestroyMesh(Mesh *m) {
  if (!m)
    return;
  free(m->vertices);
  free(m->normals);
  free(m->indices);
  memset(m, 0, sizeof(*m));
}
