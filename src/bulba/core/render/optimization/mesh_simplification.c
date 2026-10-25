#include "mesh_simplification.h"
#include "bulba/core/utils/config.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BLB_EDGE_EMPTY 0u
#define BLB_EDGE_TOMBSTONE SIZE_MAX
#define BLB_QEM_EPSILON 1.0e-12
#define BLB_AREA_EPSILON 1.0e-10f
#define BLB_NORMAL_DOT_MIN 0.25f

typedef struct {
  double m[10];
} BLB_QEMQuadric;

typedef struct {
  unsigned int a;
  unsigned int b;
  unsigned int face_count;
  uint64_t version;
  uint64_t dirty_stamp;
  bool active;
} BLB_QEMEdge;

typedef struct {
  size_t edge_id;
  uint64_t edge_version;
  unsigned int a;
  unsigned int b;
  BLB_Vec3 position;
  double error;
} BLB_QEMCandidate;

typedef struct {
  unsigned int *items;
  size_t count;
  size_t capacity;
} BLB_U32Vector;

typedef struct {
  BLB_QEMCandidate *items;
  size_t count;
  size_t capacity;
} BLB_QEMHeap;

typedef struct {
  BLB_Vec3 *vertices;
  BLB_Vec2 *uvs;
  size_t vertex_count;
  unsigned int *indices;
  size_t triangle_count;
  bool *triangle_active;
  size_t active_triangles;
  BLB_QEMQuadric *quadrics;
  bool *removed;
  BLB_U32Vector *incident_triangles;
  uint32_t *triangle_stamp;
  uint64_t triangle_visit_token;
  uint32_t *neighbor_stamp;
  uint32_t *common_stamp;
  uint32_t neighbor_token;
  BLB_QEMEdge *edges;
  size_t edge_count;
  size_t edge_capacity;
  size_t *edge_slots;
  size_t edge_slot_count;
  size_t edge_used_slots;
  BLB_QEMHeap heap;
  size_t *dirty_edges;
  size_t dirty_count;
  size_t dirty_capacity;
  uint64_t dirty_token;

  unsigned int ia;
  unsigned int ib;
  unsigned int ic;
} BLB_QEMWorkMesh;

typedef struct {
  uint32_t px;
  uint32_t py;
  uint32_t pz;
  uint32_t ux;
  uint32_t uy;
  size_t index;
} BLB_WeldKey;

typedef struct {
  unsigned int a;
  unsigned int b;
} BLB_TopologyEdge;

typedef struct {
  size_t boundary_edges;
  size_t nonmanifold_edges;
  size_t degenerate_triangles;
  bool invalid_indices;
  bool nonfinite_vertices;
} BLB_TopologyStats;

static Mesh *clone_mesh(const Mesh *source);

static int topology_edge_compare(const void *left, const void *right) {
  const BLB_TopologyEdge *a = left;
  const BLB_TopologyEdge *b = right;

  if (a->a != b->a)
    return a->a < b->a ? -1 : 1;

  if (a->b != b->b)
    return a->b < b->b ? -1 : 1;

  return 0;
}

static void topology_edge_normalize(BLB_TopologyEdge *edge) {
  if (edge && edge->a > edge->b) {
    unsigned int value = edge->a;
    edge->a = edge->b;
    edge->b = value;
  }
}

static BLB_TopologyStats mesh_topology_stats(const Mesh *mesh) {
  BLB_TopologyStats stats = {0};

  if (!mesh || !mesh->vertices || !mesh->indices || mesh->index_count < 3 || mesh->index_count % 3u != 0) {
    stats.invalid_indices = true;

    return stats;
  }

  for (size_t i = 0; i < mesh->vertex_count; ++i) {
    BLB_Vec3 v = mesh->vertices[i];

    if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z)) {
      stats.nonfinite_vertices = true;
      break;
    }
  }

  size_t triangle_count = mesh->index_count / 3u;

  if (triangle_count > SIZE_MAX / 3u) {
    stats.invalid_indices = true;

    return stats;
  }
  size_t edge_count = triangle_count * 3u;
  BLB_TopologyEdge *edges = malloc(edge_count * sizeof(*edges));

  if (!edges) {
    stats.invalid_indices = true;

    return stats;
  }

  size_t edge_index = 0;

  for (size_t i = 0; i < triangle_count; ++i) {
    unsigned int ia = mesh->indices[i * 3u + 0u];
    unsigned int ib = mesh->indices[i * 3u + 1u];
    unsigned int ic = mesh->indices[i * 3u + 2u];

    if (ia >= mesh->vertex_count || ib >= mesh->vertex_count || ic >= mesh->vertex_count) {
      stats.invalid_indices = true;
      break;
    }

    if (ia == ib || ib == ic || ic == ia) {
      ++stats.degenerate_triangles;
    } else {
      BLB_Vec3 ab = BLB_SubV3(mesh->vertices[ib], mesh->vertices[ia]);
      BLB_Vec3 ac = BLB_SubV3(mesh->vertices[ic], mesh->vertices[ia]);
      BLB_Vec3 cross = BLB_Cross(ab, ac);

      if (!isfinite(cross.x) || !isfinite(cross.y) || !isfinite(cross.z) || BLB_LenV3(cross) <= BLB_AREA_EPSILON)
        ++stats.degenerate_triangles;
    }

    edges[edge_index++] = (BLB_TopologyEdge){ia, ib};
    topology_edge_normalize(&edges[edge_index - 1u]);
    edges[edge_index++] = (BLB_TopologyEdge){ib, ic};
    topology_edge_normalize(&edges[edge_index - 1u]);
    edges[edge_index++] = (BLB_TopologyEdge){ic, ia};
    topology_edge_normalize(&edges[edge_index - 1u]);
  }

  if (!stats.invalid_indices) {
    qsort(edges, edge_index, sizeof(*edges), topology_edge_compare);

    for (size_t i = 0; i < edge_index;) {
      size_t j = i + 1u;

      while (j < edge_index && edges[j].a == edges[i].a && edges[j].b == edges[i].b)
        ++j;
      size_t count = j - i;

      if (count == 1u)
        ++stats.boundary_edges;
      else if (count > 2u)
        ++stats.nonmanifold_edges;
      i = j;
    }
  }

  free(edges);

  return stats;
}

static bool mesh_topology_safe(const Mesh *source, const Mesh *candidate) {
  BLB_TopologyStats source_stats = mesh_topology_stats(source);
  BLB_TopologyStats candidate_stats = mesh_topology_stats(candidate);

  if (candidate_stats.invalid_indices || candidate_stats.nonfinite_vertices || candidate_stats.degenerate_triangles > 0)
    return false;

  if (candidate_stats.nonmanifold_edges > source_stats.nonmanifold_edges)
    return false;

  if (candidate_stats.boundary_edges > source_stats.boundary_edges)
    return false;

  return true;
}

static bool grow_array(void **ptr, size_t *capacity, size_t count, size_t item_size) {
  if (!ptr || !capacity || item_size == 0)
    return false;

  if (count < *capacity)
    return true;
  size_t new_capacity = *capacity ? *capacity * 2u : 16u;

  if (new_capacity < count)
    new_capacity = count;

  if (new_capacity > SIZE_MAX / item_size)
    return false;
  void *new_ptr = realloc(*ptr, new_capacity * item_size);

  if (!new_ptr)
    return false;
  *ptr = new_ptr;
  *capacity = new_capacity;
  return true;
}

static bool u32_vector_push(BLB_U32Vector *vector, unsigned int value) {
  if (!vector)
    return false;

  if (!grow_array((void **)&vector->items, &vector->capacity, vector->count + 1u, sizeof(*vector->items)))
    return false;
  vector->items[vector->count++] = value;

  return true;
}

static void u32_vectors_destroy(BLB_U32Vector *vectors, size_t count) {
  if (!vectors)
    return;

  for (size_t i = 0; i < count; ++i)
    free(vectors[i].items);
  free(vectors);
}

static uint64_t hash_u64(uint64_t value) {
  value ^= value >> 30;
  value *= UINT64_C(0xbf58476d1ce4e5b9);
  value ^= value >> 27;
  value *= UINT64_C(0x94d049bb133111eb);
  value ^= value >> 31;

  return value;
}

static uint64_t edge_key(unsigned int a, unsigned int b) {
  if (a > b) {
    unsigned int t = a;
    a = b;
    b = t;
  }

  return ((uint64_t)a << 32u) | (uint64_t)b;
}

static size_t next_power_of_two(size_t value) {
  size_t result = 16u;

  while (result < value) {
    if (result > SIZE_MAX / 2u)
      return 0;
    result <<= 1u;
  }

  return result;
}

static void heap_swap(BLB_QEMCandidate *a, BLB_QEMCandidate *b) {
  BLB_QEMCandidate t = *a;
  *a = *b;
  *b = t;
}

static bool candidate_less(const BLB_QEMCandidate *a, const BLB_QEMCandidate *b) {
  if (a->error != b->error)
    return a->error < b->error;

  if (a->edge_id != b->edge_id)
    return a->edge_id < b->edge_id;

  return a->edge_version < b->edge_version;
}

static bool heap_push(BLB_QEMHeap *heap, BLB_QEMCandidate candidate) {
  if (!heap)
    return false;

  if (!grow_array((void **)&heap->items, &heap->capacity, heap->count + 1u, sizeof(*heap->items)))
    return false;
  size_t index = heap->count++;
  heap->items[index] = candidate;

  while (index > 0) {
    size_t parent = (index - 1u) / 2u;

    if (!candidate_less(&heap->items[index], &heap->items[parent]))
      break;
    heap_swap(&heap->items[index], &heap->items[parent]);
    index = parent;
  }

  return true;
}

static bool heap_pop(BLB_QEMHeap *heap, BLB_QEMCandidate *out) {
  if (!heap || heap->count == 0 || !out)
    return false;
  *out = heap->items[0];
  --heap->count;

  if (heap->count == 0)
    return true;
  heap->items[0] = heap->items[heap->count];
  size_t index = 0;

  for (;;) {
    size_t left = index * 2u + 1u;
    size_t right = left + 1u;
    size_t best = index;

    if (left < heap->count && candidate_less(&heap->items[left], &heap->items[best]))
      best = left;

    if (right < heap->count && candidate_less(&heap->items[right], &heap->items[best]))
      best = right;

    if (best == index)
      break;
    heap_swap(&heap->items[index], &heap->items[best]);
    index = best;
  }

  return true;
}

static void heap_destroy(BLB_QEMHeap *heap) {
  if (!heap)
    return;
  free(heap->items);
  memset(heap, 0, sizeof(*heap));
}

static BLB_QEMQuadric quadric_add(BLB_QEMQuadric a, BLB_QEMQuadric b) {
  BLB_QEMQuadric result;

  for (int i = 0; i < 10; ++i)
    result.m[i] = a.m[i] + b.m[i];

  return result;
}

static BLB_QEMQuadric quadric_plane(double a, double b, double c, double d) {
  BLB_QEMQuadric q = {0};
  q.m[0] = a * a;
  q.m[1] = a * b;
  q.m[2] = a * c;
  q.m[3] = a * d;
  q.m[4] = b * b;
  q.m[5] = b * c;
  q.m[6] = b * d;
  q.m[7] = c * c;
  q.m[8] = c * d;
  q.m[9] = d * d;

  return q;
}

static BLB_QEMQuadric quadric_from_triangle(BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c) {
  BLB_Vec3 ab = BLB_SubV3(b, a);
  BLB_Vec3 ac = BLB_SubV3(c, a);
  BLB_Vec3 n = BLB_Cross(ab, ac);
  float length = BLB_LenV3(n);

  if (length <= 0.000001f) {
    BLB_QEMQuadric empty = {0};

    return empty;
  }
  n = BLB_MulV3F(n, 1.0f / length);
  double pa = n.x;
  double pb = n.y;
  double pc = n.z;
  double pd = -(pa * a.x + pb * a.y + pc * a.z);

  return quadric_plane(pa, pb, pc, pd);
}

static double quadric_eval(const BLB_QEMQuadric *q, BLB_Vec3 p) {
  if (!q)
    return INFINITY;
  double x = p.x;
  double y = p.y;
  double z = p.z;

  return q->m[0] * x * x + 2.0 * q->m[1] * x * y + 2.0 * q->m[2] * x * z + 2.0 * q->m[3] * x + q->m[4] * y * y + 2.0 * q->m[5] * y * z +
         2.0 * q->m[6] * y + q->m[7] * z * z + 2.0 * q->m[8] * z + q->m[9];
}

static bool solve_optimal_position(const BLB_QEMQuadric *q, BLB_Vec3 *result) {
  if (!q || !result)
    return false;
  double a00 = q->m[0];
  double a01 = q->m[1];
  double a02 = q->m[2];
  double a11 = q->m[4];
  double a12 = q->m[5];
  double a22 = q->m[7];
  double b0 = -q->m[3];
  double b1 = -q->m[6];
  double b2 = -q->m[8];
  double det = a00 * (a11 * a22 - a12 * a12) - a01 * (a01 * a22 - a12 * a02) + a02 * (a01 * a12 - a11 * a02);

  if (fabs(det) <= BLB_QEM_EPSILON)
    return false;
  double inv_det = 1.0 / det;
  double x = (b0 * (a11 * a22 - a12 * a12) - a01 * (b1 * a22 - a12 * b2) + a02 * (b1 * a12 - a11 * b2)) * inv_det;
  double y = (a00 * (b1 * a22 - a12 * b2) - b0 * (a01 * a22 - a12 * a02) + a02 * (a01 * b2 - b1 * a02)) * inv_det;
  double z = (a00 * (a11 * b2 - b1 * a12) - a01 * (a01 * b2 - b1 * a02) + b0 * (a01 * a12 - a11 * a02)) * inv_det;

  if (!isfinite(x) || !isfinite(y) || !isfinite(z))
    return false;
  result->x = (float)x;
  result->y = (float)y;
  result->z = (float)z;

  return isfinite(result->x) && isfinite(result->y) && isfinite(result->z);
}

static uint32_t float_bits(float value) {
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));

  return bits;
}

static int compare_weld_key(const void *lhs, const void *rhs) {
  const BLB_WeldKey *a = lhs;
  const BLB_WeldKey *b = rhs;

  if (a->px != b->px)
    return a->px < b->px ? -1 : 1;

  if (a->py != b->py)
    return a->py < b->py ? -1 : 1;

  if (a->pz != b->pz)
    return a->pz < b->pz ? -1 : 1;

  if (a->ux != b->ux)
    return a->ux < b->ux ? -1 : 1;

  if (a->uy != b->uy)
    return a->uy < b->uy ? -1 : 1;

  return 0;
}

static bool weld_mesh_vertices(const Mesh *source, Mesh **result_out) {
  if (!source || !result_out || !source->vertices || !source->indices || source->vertex_count == 0 || source->index_count < 3 ||
      source->index_count % 3 != 0)
    return false;
  *result_out = NULL;
  BLB_WeldKey *keys = malloc(source->vertex_count * sizeof(*keys));
  size_t *remap = malloc(source->vertex_count * sizeof(*remap));

  if (!keys || !remap) {
    free(keys);
    free(remap);

    return false;
  }

  for (size_t i = 0; i < source->vertex_count; ++i) {
    keys[i].px = float_bits(source->vertices[i].x);
    keys[i].py = float_bits(source->vertices[i].y);
    keys[i].pz = float_bits(source->vertices[i].z);
    keys[i].ux = source->uvs ? float_bits(source->uvs[i].x) : 0u;
    keys[i].uy = source->uvs ? float_bits(source->uvs[i].y) : 0u;
    keys[i].index = i;
  }
  qsort(keys, source->vertex_count, sizeof(*keys), compare_weld_key);
  size_t unique_count = 0;

  for (size_t i = 0; i < source->vertex_count; ++i) {
    if (i == 0 || compare_weld_key(&keys[i - 1], &keys[i]) != 0)
      ++unique_count;
    remap[keys[i].index] = unique_count - 1u;
  }
  Mesh *mesh = calloc(1, sizeof(*mesh));

  if (!mesh) {
    free(keys);
    free(remap);

    return false;
  }
  mesh->vertex_count = unique_count;
  mesh->index_count = source->index_count;
  mesh->vertices = malloc(unique_count * sizeof(*mesh->vertices));
  mesh->indices = malloc(source->index_count * sizeof(*mesh->indices));

  if (source->uvs)
    mesh->uvs = malloc(unique_count * sizeof(*mesh->uvs));

  if (!mesh->vertices || !mesh->indices || (source->uvs && !mesh->uvs)) {
    BLB_MeshDestroy(mesh);
    free(keys);
    free(remap);

    return false;
  }
  size_t unique_index = 0;

  for (size_t i = 0; i < source->vertex_count; ++i) {
    if (i != 0 && compare_weld_key(&keys[i - 1], &keys[i]) == 0)
      continue;
    size_t src = keys[i].index;
    mesh->vertices[unique_index] = source->vertices[src];

    if (mesh->uvs)
      mesh->uvs[unique_index] = source->uvs[src];
    ++unique_index;
  }

  for (size_t i = 0; i < source->index_count; ++i) {
    if (source->indices[i] >= source->vertex_count) {
      BLB_MeshDestroy(mesh);
      free(keys);
      free(remap);

      return false;
    }
    mesh->indices[i] = (unsigned int)remap[source->indices[i]];
  }
  free(keys);
  free(remap);
  *result_out = mesh;
  return true;
}

static bool edge_map_init(BLB_QEMWorkMesh *work, size_t expected_edges) {
  if (!work)
    return false;
  size_t capacity = next_power_of_two(expected_edges * 2u + 16u);

  if (!capacity)
    return false;
  work->edge_slots = malloc(capacity * sizeof(*work->edge_slots));

  if (!work->edge_slots)
    return false;

  for (size_t i = 0; i < capacity; ++i)
    work->edge_slots[i] = BLB_EDGE_EMPTY;
  work->edge_slot_count = capacity;
  work->edge_used_slots = 0;

  return true;
}

static size_t edge_map_find_slot(const BLB_QEMWorkMesh *work, uint64_t key, bool *found) {
  size_t mask = work->edge_slot_count - 1u;
  size_t slot = (size_t)(hash_u64(key) & mask);
  size_t first_tombstone = SIZE_MAX;

  for (;;) {
    size_t value = work->edge_slots[slot];

    if (value == BLB_EDGE_EMPTY) {
      if (found)
        *found = false;
      return first_tombstone != SIZE_MAX ? first_tombstone : slot;
    }

    if (value == BLB_EDGE_TOMBSTONE) {
      if (first_tombstone == SIZE_MAX)
        first_tombstone = slot;
    } else {
      size_t edge_id = value - 1u;

      if (edge_id < work->edge_count) {
        const BLB_QEMEdge *edge = &work->edges[edge_id];

        if (edge->active && edge_key(edge->a, edge->b) == key) {
          if (found)
            *found = true;
          return slot;
        }
      }
    }
    slot = (slot + 1u) & mask;
  }
}

static bool edge_map_rebuild(BLB_QEMWorkMesh *work, size_t new_capacity) {
  if (!work)
    return false;
  new_capacity = next_power_of_two(new_capacity);

  if (!new_capacity)
    return false;
  size_t *slots = malloc(new_capacity * sizeof(*slots));

  if (!slots)
    return false;

  for (size_t i = 0; i < new_capacity; ++i)
    slots[i] = BLB_EDGE_EMPTY;
  size_t old_capacity = work->edge_slot_count;
  size_t *old_slots = work->edge_slots;
  work->edge_slots = slots;
  work->edge_slot_count = new_capacity;
  work->edge_used_slots = 0;

  for (size_t i = 0; i < work->edge_count; ++i) {
    BLB_QEMEdge *edge = &work->edges[i];

    if (!edge->active)
      continue;
    bool found = false;
    size_t slot = edge_map_find_slot(work, edge_key(edge->a, edge->b), &found);
    work->edge_slots[slot] = i + 1u;

    if (!found)
      ++work->edge_used_slots;
  }
  free(old_slots);
  (void)old_capacity;

  return true;
}

static bool edge_map_prepare_insert(BLB_QEMWorkMesh *work) {
  if (!work || work->edge_slot_count == 0)
    return false;

  if ((work->edge_used_slots + 1u) * 10u >= work->edge_slot_count * 7u)
    return edge_map_rebuild(work, work->edge_slot_count * 2u);

  return true;
}

static BLB_QEMEdge *edge_find(BLB_QEMWorkMesh *work, unsigned int a, unsigned int b, size_t *edge_id_out) {
  if (!work || !work->edge_slots || work->edge_slot_count == 0)
    return NULL;
  bool found = false;
  size_t slot = edge_map_find_slot(work, edge_key(a, b), &found);

  if (!found)
    return NULL;
  size_t edge_id = work->edge_slots[slot] - 1u;

  if (edge_id_out)
    *edge_id_out = edge_id;
  return &work->edges[edge_id];
}

static bool mark_edge_dirty(BLB_QEMWorkMesh *work, size_t edge_id) {
  if (!work || edge_id >= work->edge_count)
    return false;
  BLB_QEMEdge *edge = &work->edges[edge_id];

  if (!edge->active)
    return true;

  if (edge->dirty_stamp == work->dirty_token)
    return true;
  edge->dirty_stamp = work->dirty_token;

  if (!grow_array((void **)&work->dirty_edges, &work->dirty_capacity, work->dirty_count + 1u, sizeof(*work->dirty_edges)))
    return false;
  work->dirty_edges[work->dirty_count++] = edge_id;
  ++edge->version;

  if (edge->version == 0)
    edge->version = 1;

  return true;
}

static bool edge_add_face(BLB_QEMWorkMesh *work, unsigned int a, unsigned int b) {
  if (!work || a == b)
    return false;

  if (a > b) {
    unsigned int t = a;
    a = b;
    b = t;
  }
  size_t edge_id = SIZE_MAX;
  BLB_QEMEdge *edge = edge_find(work, a, b, &edge_id);

  if (edge) {
    if (edge->face_count < UINT_MAX)
      ++edge->face_count;

    return mark_edge_dirty(work, edge_id);
  }

  if (!edge_map_prepare_insert(work))
    return false;

  if (!grow_array((void **)&work->edges, &work->edge_capacity, work->edge_count + 1u, sizeof(*work->edges)))
    return false;
  edge_id = work->edge_count++;
  work->edges[edge_id] = (BLB_QEMEdge){a, b, 1u, 1u, 0u, true};
  bool found = false;
  size_t slot = edge_map_find_slot(work, edge_key(a, b), &found);

  if (found)
    return false;
  work->edge_slots[slot] = edge_id + 1u;
  ++work->edge_used_slots;

  return mark_edge_dirty(work, edge_id);
}

static bool edge_delete_from_map(BLB_QEMWorkMesh *work, unsigned int a, unsigned int b) {
  if (!work || !work->edge_slots)
    return false;
  bool found = false;
  size_t slot = edge_map_find_slot(work, edge_key(a, b), &found);

  if (!found)
    return false;
  work->edge_slots[slot] = BLB_EDGE_TOMBSTONE;

  return true;
}

static bool edge_remove_face(BLB_QEMWorkMesh *work, unsigned int a, unsigned int b) {
  size_t edge_id = SIZE_MAX;
  BLB_QEMEdge *edge = edge_find(work, a, b, &edge_id);

  if (!edge || edge->face_count == 0)
    return false;
  --edge->face_count;

  if (edge->face_count == 0) {
    bool removed_from_map = edge_delete_from_map(work, a, b);
    edge->active = false;
    ++edge->version;

    if (edge->version == 0)
      edge->version = 1;

    return removed_from_map;
  }

  return mark_edge_dirty(work, edge_id);
}

static bool add_triangle_edges(BLB_QEMWorkMesh *work, unsigned int ia, unsigned int ib, unsigned int ic) {
  if (!edge_add_face(work, ia, ib))
    return false;

  if (!edge_add_face(work, ib, ic))
    return false;

  if (!edge_add_face(work, ic, ia))
    return false;

  return true;
}

static bool remove_triangle_edges(BLB_QEMWorkMesh *work, unsigned int ia, unsigned int ib, unsigned int ic) {
  if (!edge_remove_face(work, ia, ib))
    return false;

  if (!edge_remove_face(work, ib, ic))
    return false;

  if (!edge_remove_face(work, ic, ia))
    return false;

  return true;
}

static bool init_incident_and_edges(BLB_QEMWorkMesh *work) {
  if (!work)
    return false;
  work->incident_triangles = calloc(work->vertex_count, sizeof(*work->incident_triangles));
  work->triangle_stamp = calloc(work->triangle_count, sizeof(*work->triangle_stamp));
  work->neighbor_stamp = calloc(work->vertex_count, sizeof(*work->neighbor_stamp));
  work->common_stamp = calloc(work->vertex_count, sizeof(*work->common_stamp));

  if (!work->incident_triangles || !work->triangle_stamp || !work->neighbor_stamp || !work->common_stamp)
    return false;

  if (!edge_map_init(work, work->triangle_count * 3u))
    return false;

  for (size_t t = 0; t < work->triangle_count; ++t) {
    unsigned int ia = work->indices[t * 3u + 0u];
    unsigned int ib = work->indices[t * 3u + 1u];
    unsigned int ic = work->indices[t * 3u + 2u];

    if (ia >= work->vertex_count || ib >= work->vertex_count || ic >= work->vertex_count)
      return false;

    if (ia == ib || ib == ic || ic == ia) {
      work->triangle_active[t] = false;
      --work->active_triangles;
      continue;
    }

    if (!u32_vector_push(&work->incident_triangles[ia], (unsigned int)t) || !u32_vector_push(&work->incident_triangles[ib], (unsigned int)t) ||
        !u32_vector_push(&work->incident_triangles[ic], (unsigned int)t))
      return false;

    if (!add_triangle_edges(work, ia, ib, ic))
      return false;
  }

  return true;
}

static bool clone_work_mesh(const Mesh *source, BLB_QEMWorkMesh *work) {
  if (!source || !work || !source->vertices || !source->indices || source->vertex_count < 3 || source->index_count < 3 ||
      source->index_count % 3 != 0)
    return false;
  memset(work, 0, sizeof(*work));
  work->vertex_count = source->vertex_count;
  work->triangle_count = source->index_count / 3u;
  work->active_triangles = work->triangle_count;
  work->vertices = malloc(work->vertex_count * sizeof(*work->vertices));
  work->indices = malloc(source->index_count * sizeof(*work->indices));
  work->quadrics = calloc(work->vertex_count, sizeof(*work->quadrics));
  work->removed = calloc(work->vertex_count, sizeof(*work->removed));
  work->triangle_active = calloc(work->triangle_count, sizeof(*work->triangle_active));

  if (!work->vertices || !work->indices || !work->quadrics || !work->removed || !work->triangle_active)
    return false;
  memcpy(work->vertices, source->vertices, work->vertex_count * sizeof(*work->vertices));
  memcpy(work->indices, source->indices, source->index_count * sizeof(*work->indices));

  if (source->uvs) {
    work->uvs = malloc(work->vertex_count * sizeof(*work->uvs));

    if (!work->uvs)
      return false;
    memcpy(work->uvs, source->uvs, work->vertex_count * sizeof(*work->uvs));
  }

  for (size_t t = 0; t < work->triangle_count; ++t) {
    work->triangle_active[t] = true;
    unsigned int ia = work->indices[t * 3u + 0u];
    unsigned int ib = work->indices[t * 3u + 1u];
    unsigned int ic = work->indices[t * 3u + 2u];

    if (ia >= work->vertex_count || ib >= work->vertex_count || ic >= work->vertex_count) {
      work->triangle_active[t] = false;
      --work->active_triangles;
      continue;
    }
    BLB_QEMQuadric q = quadric_from_triangle(work->vertices[ia], work->vertices[ib], work->vertices[ic]);
    work->quadrics[ia] = quadric_add(work->quadrics[ia], q);
    work->quadrics[ib] = quadric_add(work->quadrics[ib], q);
    work->quadrics[ic] = quadric_add(work->quadrics[ic], q);
  }
  work->dirty_token = 1u;
  work->triangle_visit_token = 1u;

  if (!init_incident_and_edges(work))
    return false;

  return true;
}

static void destroy_work_mesh(BLB_QEMWorkMesh *work) {
  if (!work)
    return;
  free(work->vertices);
  free(work->uvs);
  free(work->indices);
  free(work->quadrics);
  free(work->removed);
  free(work->triangle_active);
  u32_vectors_destroy(work->incident_triangles, work->vertex_count);
  free(work->triangle_stamp);
  free(work->neighbor_stamp);
  free(work->common_stamp);
  free(work->edges);
  free(work->edge_slots);
  free(work->dirty_edges);
  heap_destroy(&work->heap);
  memset(work, 0, sizeof(*work));
}

static bool gather_affected_triangles(BLB_QEMWorkMesh *work, unsigned int a, unsigned int b, BLB_U32Vector *out) {
  if (!work || !out || a >= work->vertex_count || b >= work->vertex_count)
    return false;

  if (++work->triangle_visit_token == 0) {
    memset(work->triangle_stamp, 0, work->triangle_count * sizeof(*work->triangle_stamp));
    work->triangle_visit_token = 1;
  }
  BLB_U32Vector *va = &work->incident_triangles[a];
  BLB_U32Vector *vb = &work->incident_triangles[b];

  for (int source = 0; source < 2; ++source) {
    BLB_U32Vector *v = source == 0 ? va : vb;

    for (size_t i = 0; i < v->count; ++i) {
      unsigned int t = v->items[i];

      if (t >= work->triangle_count || !work->triangle_active[t])
        continue;

      if (work->triangle_stamp[t] == work->triangle_visit_token)
        continue;
      work->triangle_stamp[t] = (uint32_t)work->triangle_visit_token;

      if (!u32_vector_push(out, t))
        return false;
    }
  }

  return true;
}

static bool triangle_contains(const unsigned int *tri, unsigned int v) { return tri && (tri[0] == v || tri[1] == v || tri[2] == v); }

static bool vector_contains_u32(const BLB_U32Vector *vector, unsigned int value) {
  if (!vector)
    return false;

  for (size_t i = 0; i < vector->count; ++i)
    if (vector->items[i] == value)
      return true;

  return false;
}

static bool vertex_is_boundary(BLB_QEMWorkMesh *work, unsigned int vertex) {
  if (!work || vertex >= work->vertex_count)
    return false;
  BLB_U32Vector *incident = &work->incident_triangles[vertex];

  for (size_t i = 0; i < incident->count; ++i) {
    unsigned int t = incident->items[i];

    if (t >= work->triangle_count || !work->triangle_active[t])
      continue;
    unsigned int *tri = &work->indices[t * 3u];
    unsigned int n0;
    unsigned int n1;

    if (tri[0] == vertex) {
      n0 = tri[1];
      n1 = tri[2];
    } else if (tri[1] == vertex) {
      n0 = tri[0];
      n1 = tri[2];
    } else {
      n0 = tri[0];
      n1 = tri[1];
    }
    BLB_QEMEdge *e0 = edge_find(work, vertex, n0, NULL);
    BLB_QEMEdge *e1 = edge_find(work, vertex, n1, NULL);

    if ((e0 && e0->face_count < 2u) || (e1 && e1->face_count < 2u))
      return true;
  }

  return false;
}

static bool validate_link_condition(BLB_QEMWorkMesh *work, unsigned int a, unsigned int b, const BLB_QEMEdge *edge) {
  if (!work || !edge || a >= work->vertex_count || b >= work->vertex_count)
    return false;

  if (!work->neighbor_stamp || !work->common_stamp)
    return false;

  ++work->neighbor_token;

  if (work->neighbor_token == 0u) {
    memset(work->neighbor_stamp, 0, work->vertex_count * sizeof(*work->neighbor_stamp));
    memset(work->common_stamp, 0, work->vertex_count * sizeof(*work->common_stamp));
    work->neighbor_token = 1u;
  }
  uint32_t token = work->neighbor_token;

  BLB_U32Vector *ia = &work->incident_triangles[a];
  BLB_U32Vector *ib = &work->incident_triangles[b];
  size_t common = 0;

  for (size_t i = 0; i < ia->count; ++i) {
    unsigned int t = ia->items[i];

    if (t >= work->triangle_count || !work->triangle_active[t])
      continue;
    unsigned int *tri = &work->indices[t * 3u];

    for (int j = 0; j < 3; ++j) {
      unsigned int n = tri[j];

      if (n < work->vertex_count && n != a && n != b && !work->removed[n])
        work->neighbor_stamp[n] = token;
    }
  }

  for (size_t i = 0; i < ib->count; ++i) {
    unsigned int t = ib->items[i];

    if (t >= work->triangle_count || !work->triangle_active[t])
      continue;
    unsigned int *tri = &work->indices[t * 3u];

    for (int j = 0; j < 3; ++j) {
      unsigned int n = tri[j];

      if (n >= work->vertex_count || n == a || n == b || work->removed[n])
        continue;

      if (work->neighbor_stamp[n] == token && work->common_stamp[n] != token) {
        work->common_stamp[n] = token;
        ++common;
      }
    }
  }

  unsigned int expected = edge->face_count == 1u ? 1u : edge->face_count == 2u ? 2u : 0u;

  if (expected == 0u || common != expected)
    return false;

  if (BLB_OBJECT_LOD_PRESERVE_BOUNDARIES) {
    bool ba = vertex_is_boundary(work, a);
    bool bb = vertex_is_boundary(work, b);

    if (ba != bb)
      return false;

    if (edge->face_count == 1u && !(ba && bb))
      return false;

    if (edge->face_count == 2u && (ba || bb))
      return false;
  }

  return true;
}

static BLB_Vec3 triangle_normal(BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c) {
  BLB_Vec3 n = BLB_Cross(BLB_SubV3(b, a), BLB_SubV3(c, a));
  float len = BLB_LenV3(n);

  if (len > 0.000001f)
    return BLB_MulV3F(n, 1.0f / len);

  return BLB_V3(0.0f, 0.0f, 0.0f);
}

static float triangle_area2(BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c) { return BLB_LenV3(BLB_Cross(BLB_SubV3(b, a), BLB_SubV3(c, a))); }

static bool validate_collapse_geometry(BLB_QEMWorkMesh *work, unsigned int a, unsigned int b, BLB_Vec3 position) {
  BLB_U32Vector affected = {0};

  if (!gather_affected_triangles(work, a, b, &affected)) {
    free(affected.items);

    return false;
  }

  for (size_t i = 0; i < affected.count; ++i) {
    unsigned int t = affected.items[i];
    unsigned int old_tri[3] = {work->indices[t * 3u + 0u], work->indices[t * 3u + 1u], work->indices[t * 3u + 2u]};
    bool has_a = triangle_contains(old_tri, a);
    bool has_b = triangle_contains(old_tri, b);

    if (!has_a && !has_b)
      continue;
    BLB_Vec3 old_a = work->vertices[old_tri[0]];
    BLB_Vec3 old_b = work->vertices[old_tri[1]];
    BLB_Vec3 old_c = work->vertices[old_tri[2]];
    float old_area = triangle_area2(old_a, old_b, old_c);

    if (old_area <= BLB_AREA_EPSILON)
      continue;
    unsigned int next_tri[3] = {old_tri[0], old_tri[1], old_tri[2]};

    for (int j = 0; j < 3; ++j)
      if (next_tri[j] == b)
        next_tri[j] = a;

    if (next_tri[0] == next_tri[1] || next_tri[1] == next_tri[2] || next_tri[2] == next_tri[0])
      continue;
    BLB_Vec3 na = next_tri[0] == a ? position : work->vertices[next_tri[0]];
    BLB_Vec3 nb = next_tri[1] == a ? position : work->vertices[next_tri[1]];
    BLB_Vec3 nc = next_tri[2] == a ? position : work->vertices[next_tri[2]];
    float new_area = triangle_area2(na, nb, nc);
    const float min_area_ratio = BLB_OBJECT_LOD_PRESERVE_NORMALS ? 0.01f : 0.001f;

    if (new_area <= BLB_AREA_EPSILON || new_area < old_area * min_area_ratio) {
      free(affected.items);

      return false;
    }
    BLB_Vec3 old_n = triangle_normal(old_a, old_b, old_c);
    BLB_Vec3 new_n = triangle_normal(na, nb, nc);
    float dot = old_n.x * new_n.x + old_n.y * new_n.y + old_n.z * new_n.z;

    if (dot < BLB_NORMAL_DOT_MIN) {
      free(affected.items);

      return false;
    }
  }
  free(affected.items);

  return true;
}

static bool position_within_local_edge_box(const BLB_QEMWorkMesh *work, unsigned int a, unsigned int b, BLB_Vec3 position) {
  if (!work || a >= work->vertex_count || b >= work->vertex_count)
    return false;

  BLB_Vec3 pa = work->vertices[a];
  BLB_Vec3 pb = work->vertices[b];
  float min_x = fminf(pa.x, pb.x);
  float max_x = fmaxf(pa.x, pb.x);
  float min_y = fminf(pa.y, pb.y);
  float max_y = fmaxf(pa.y, pb.y);
  float min_z = fminf(pa.z, pb.z);
  float max_z = fmaxf(pa.z, pb.z);

  float edge_len = BLB_LenV3(BLB_SubV3(pb, pa));
  float pad = fmaxf(edge_len * 1.5f, 0.0001f);
  min_x -= pad;
  max_x += pad;
  min_y -= pad;
  max_y += pad;
  min_z -= pad;
  max_z += pad;

  return isfinite(position.x) && isfinite(position.y) && isfinite(position.z) && position.x >= min_x && position.x <= max_x && position.y >= min_y &&
         position.y <= max_y && position.z >= min_z && position.z <= max_z;
}

static bool position_within_local_vertex_neighborhood(BLB_QEMWorkMesh *work, unsigned int a, unsigned int b, BLB_Vec3 position) {
  if (!work || a >= work->vertex_count || b >= work->vertex_count)
    return false;

  if (!work->neighbor_stamp || !work->common_stamp)
    return position_within_local_edge_box(work, a, b, position);

  ++work->neighbor_token;

  if (work->neighbor_token == 0u) {
    memset(work->neighbor_stamp, 0, work->vertex_count * sizeof(*work->neighbor_stamp));
    memset(work->common_stamp, 0, work->vertex_count * sizeof(*work->common_stamp));
    work->neighbor_token = 1u;
  }

  BLB_Vec3 min_v = BLB_V3(fminf(work->vertices[a].x, work->vertices[b].x), fminf(work->vertices[a].y, work->vertices[b].y),
                          fminf(work->vertices[a].z, work->vertices[b].z));
  BLB_Vec3 max_v = BLB_V3(fmaxf(work->vertices[a].x, work->vertices[b].x), fmaxf(work->vertices[a].y, work->vertices[b].y),
                          fmaxf(work->vertices[a].z, work->vertices[b].z));
  float edge_len = BLB_LenV3(BLB_SubV3(work->vertices[b], work->vertices[a]));
  float pad = fmaxf(edge_len * 1.5f, 0.0001f);
  min_v.x -= pad;
  min_v.y -= pad;
  min_v.z -= pad;
  max_v.x += pad;
  max_v.y += pad;
  max_v.z += pad;

  return isfinite(position.x) && isfinite(position.y) && isfinite(position.z) && position.x >= min_v.x && position.x <= max_v.x &&
         position.y >= min_v.y && position.y <= max_v.y && position.z >= min_v.z && position.z <= max_v.z;
}

static BLB_QEMCandidate calculate_candidate(const BLB_QEMWorkMesh *work, size_t edge_id) {
  BLB_QEMCandidate candidate = {0};
  candidate.edge_id = edge_id;
  candidate.error = INFINITY;

  if (!work || edge_id >= work->edge_count)
    return candidate;
  const BLB_QEMEdge *edge = &work->edges[edge_id];

  if (!edge->active || edge->a >= work->vertex_count || edge->b >= work->vertex_count || work->removed[edge->a] || work->removed[edge->b])
    return candidate;
  candidate.edge_version = edge->version;
  candidate.a = edge->a;
  candidate.b = edge->b;
  BLB_QEMQuadric q = quadric_add(work->quadrics[edge->a], work->quadrics[edge->b]);
  BLB_Vec3 pa = work->vertices[edge->a];
  BLB_Vec3 pb = work->vertices[edge->b];
  BLB_Vec3 midpoint = BLB_MulV3F(BLB_AddV3(pa, pb), 0.5f);
  BLB_Vec3 optimal;
  BLB_Vec3 best_position = pa;
  double best_error = quadric_eval(&q, pa);
  double error = quadric_eval(&q, pb);

  if (error < best_error) {
    best_error = error;
    best_position = pb;
  }
  error = quadric_eval(&q, midpoint);

  if (error < best_error) {
    best_error = error;
    best_position = midpoint;
  }

  if (solve_optimal_position(&q, &optimal) && position_within_local_edge_box(work, edge->a, edge->b, optimal) &&
      position_within_local_vertex_neighborhood((BLB_QEMWorkMesh *)work, edge->a, edge->b, optimal)) {
    error = quadric_eval(&q, optimal);

    if (error < best_error) {
      best_error = error;
      best_position = optimal;
    }
  }

  if (!isfinite(best_error))
    return candidate;
  candidate.position = best_position;
  candidate.error = best_error;

  return candidate;
}

static bool seed_heap(BLB_QEMWorkMesh *work) {
  if (!work)
    return false;

  for (size_t i = 0; i < work->edge_count; ++i) {
    if (!work->edges[i].active)
      continue;
    BLB_QEMCandidate candidate = calculate_candidate(work, i);

    if (!isfinite(candidate.error))
      continue;

    if (!heap_push(&work->heap, candidate))
      return false;
  }

  return true;
}

static void prepare_dirty_edges(BLB_QEMWorkMesh *work, unsigned int a) {
  if (!work || a >= work->vertex_count)
    return;
  BLB_U32Vector *incident = &work->incident_triangles[a];

  for (size_t i = 0; i < incident->count; ++i) {
    unsigned int t = incident->items[i];

    if (t >= work->triangle_count || !work->triangle_active[t])
      continue;
    unsigned int *tri = &work->indices[t * 3u];
    size_t edge_id;
    BLB_QEMEdge *e = edge_find(work, tri[0], tri[1], &edge_id);

    if (e)
      mark_edge_dirty(work, edge_id);
    e = edge_find(work, tri[1], tri[2], &edge_id);

    if (e)
      mark_edge_dirty(work, edge_id);
    e = edge_find(work, tri[2], tri[0], &edge_id);

    if (e)
      mark_edge_dirty(work, edge_id);
  }
}

static bool push_dirty_candidates(BLB_QEMWorkMesh *work) {
  if (!work)
    return false;

  for (size_t i = 0; i < work->dirty_count; ++i) {
    size_t edge_id = work->dirty_edges[i];

    if (edge_id >= work->edge_count || !work->edges[edge_id].active)
      continue;
    BLB_QEMCandidate candidate = calculate_candidate(work, edge_id);

    if (!isfinite(candidate.error))
      continue;

    if (!heap_push(&work->heap, candidate))
      return false;
  }
  work->dirty_count = 0;

  return true;
}

static bool collapse_edge(BLB_QEMWorkMesh *work, const BLB_QEMCandidate *candidate) {
  if (!work || !candidate || candidate->edge_id >= work->edge_count)
    return false;
  BLB_QEMEdge *edge = &work->edges[candidate->edge_id];

  if (!edge->active || edge->version != candidate->edge_version)
    return false;
  unsigned int a = edge->a;
  unsigned int b = edge->b;

  if (a == b || work->removed[a] || work->removed[b])
    return false;

  if (!validate_link_condition(work, a, b, edge))
    return false;

  if (!validate_collapse_geometry(work, a, b, candidate->position))
    return false;

  BLB_U32Vector affected = {0};

  if (!gather_affected_triangles(work, a, b, &affected)) {
    free(affected.items);

    return false;
  }

  work->dirty_count = 0;
  ++work->dirty_token;

  if (work->dirty_token == 0)
    work->dirty_token = 1;

  for (size_t i = 0; i < affected.count; ++i) {
    unsigned int t = affected.items[i];
    unsigned int ia = work->indices[t * 3u + 0u];
    unsigned int ib = work->indices[t * 3u + 1u];
    unsigned int ic = work->indices[t * 3u + 2u];

    if (!remove_triangle_edges(work, ia, ib, ic)) {
      free(affected.items);

      return false;
    }
  }

  work->vertices[a] = candidate->position;

  if (work->uvs)
    work->uvs[a] = BLB_V2((work->uvs[a].x + work->uvs[b].x) * 0.5f, (work->uvs[a].y + work->uvs[b].y) * 0.5f);
  work->quadrics[a] = quadric_add(work->quadrics[a], work->quadrics[b]);
  work->removed[b] = true;

  for (size_t i = 0; i < affected.count; ++i) {
    unsigned int t = affected.items[i];
    unsigned int tri[3] = {work->indices[t * 3u + 0u], work->indices[t * 3u + 1u], work->indices[t * 3u + 2u]};

    for (int j = 0; j < 3; ++j) {
      if (tri[j] == b)
        tri[j] = a;
    }

    if (tri[0] == tri[1] || tri[1] == tri[2] || tri[2] == tri[0]) {
      if (work->triangle_active[t]) {
        work->triangle_active[t] = false;

        if (work->active_triangles > 0)
          --work->active_triangles;
      }
      continue;
    }
    work->indices[t * 3u + 0u] = tri[0];
    work->indices[t * 3u + 1u] = tri[1];
    work->indices[t * 3u + 2u] = tri[2];

    if (triangle_contains(tri, a) && !vector_contains_u32(&work->incident_triangles[a], t)) {
      if (!u32_vector_push(&work->incident_triangles[a], t)) {
        free(affected.items);

        return false;
      }
    }

    if (!add_triangle_edges(work, tri[0], tri[1], tri[2])) {
      free(affected.items);

      return false;
    }
  }

  prepare_dirty_edges(work, a);
  free(affected.items);

  return push_dirty_candidates(work);
}

static bool calculate_bounds(const BLB_QEMWorkMesh *work, BLB_Vec3 *min_value, BLB_Vec3 *max_value) {
  if (!work || !min_value || !max_value || work->vertex_count == 0)
    return false;
  bool have_value = false;
  BLB_Vec3 min_v = BLB_V3(0, 0, 0);
  BLB_Vec3 max_v = BLB_V3(0, 0, 0);

  for (size_t i = 0; i < work->vertex_count; ++i) {
    if (work->removed[i])
      continue;
    BLB_Vec3 v = work->vertices[i];

    if (!have_value) {
      min_v = max_v = v;
      have_value = true;
      continue;
    }
    min_v.x = fminf(min_v.x, v.x);
    min_v.y = fminf(min_v.y, v.y);
    min_v.z = fminf(min_v.z, v.z);
    max_v.x = fmaxf(max_v.x, v.x);
    max_v.y = fmaxf(max_v.y, v.y);
    max_v.z = fmaxf(max_v.z, v.z);
  }

  if (!have_value)
    return false;
  *min_value = min_v;
  *max_value = max_v;
  return true;
}

static Mesh *build_output_mesh(BLB_QEMWorkMesh *work) {
  if (!work || work->active_triangles == 0)
    return NULL;
  size_t output_index_count = work->active_triangles * 3u;
  bool *used = calloc(work->vertex_count, sizeof(*used));
  size_t *map = malloc(work->vertex_count * sizeof(*map));

  if (!used || !map) {
    free(used);
    free(map);

    return NULL;
  }

  for (size_t t = 0; t < work->triangle_count; ++t) {
    if (!work->triangle_active[t])
      continue;
    work->ia = work->indices[t * 3u + 0u];
    work->ib = work->indices[t * 3u + 1u];
    work->ic = work->indices[t * 3u + 2u];

    if (work->ia >= work->vertex_count || work->ib >= work->vertex_count || work->ic >= work->vertex_count) {
      free(used);
      free(map);

      return NULL;
    }
    used[work->ia] = true;
    used[work->ib] = true;
    used[work->ic] = true;
  }
  size_t vertex_count = 0;

  for (size_t i = 0; i < work->vertex_count; ++i)
    map[i] = used[i] ? vertex_count++ : SIZE_MAX;
  Mesh *mesh = calloc(1, sizeof(*mesh));

  if (!mesh) {
    free(used);
    free(map);

    return NULL;
  }
  mesh->vertex_count = vertex_count;
  mesh->index_count = output_index_count;
  mesh->vertices = malloc(vertex_count * sizeof(*mesh->vertices));
  mesh->normals = calloc(vertex_count, sizeof(*mesh->normals));
  mesh->indices = malloc(output_index_count * sizeof(*mesh->indices));

  if (work->uvs)
    mesh->uvs = malloc(vertex_count * sizeof(*mesh->uvs));

  if (!mesh->vertices || !mesh->normals || !mesh->indices || (work->uvs && !mesh->uvs)) {
    BLB_MeshDestroy(mesh);
    free(used);
    free(map);

    return NULL;
  }

  for (size_t i = 0; i < work->vertex_count; ++i) {
    if (!used[i])
      continue;
    size_t dst = map[i];
    mesh->vertices[dst] = work->vertices[i];

    if (mesh->uvs)
      mesh->uvs[dst] = work->uvs[i];
  }
  size_t out_i = 0;

  for (size_t t = 0; t < work->triangle_count; ++t) {
    if (!work->triangle_active[t])
      continue;
    unsigned int ia = work->indices[t * 3u + 0u];
    unsigned int ib = work->indices[t * 3u + 1u];
    unsigned int ic = work->indices[t * 3u + 2u];
    mesh->indices[out_i++] = (unsigned int)map[ia];
    mesh->indices[out_i++] = (unsigned int)map[ib];
    mesh->indices[out_i++] = (unsigned int)map[ic];
    BLB_Vec3 a = mesh->vertices[map[ia]];
    BLB_Vec3 b = mesh->vertices[map[ib]];
    BLB_Vec3 c = mesh->vertices[map[ic]];
    BLB_Vec3 normal = BLB_Cross(BLB_SubV3(b, a), BLB_SubV3(c, a));
    mesh->normals[map[ia]] = BLB_AddV3(mesh->normals[map[ia]], normal);
    mesh->normals[map[ib]] = BLB_AddV3(mesh->normals[map[ib]], normal);
    mesh->normals[map[ic]] = BLB_AddV3(mesh->normals[map[ic]], normal);
  }

  for (size_t i = 0; i < vertex_count; ++i) {
    float length = BLB_LenV3(mesh->normals[i]);

    if (length > 0.000001f)
      mesh->normals[i] = BLB_MulV3F(mesh->normals[i], 1.0f / length);
    else
      mesh->normals[i] = BLB_V3(0.0f, 1.0f, 0.0f);
  }
  free(used);
  free(map);

  return mesh;
}

static Mesh *mesh_simplify_qem_impl(const Mesh *mesh, float max_error, float reduction) {
  if (!mesh || !mesh->vertices || !mesh->indices || mesh->vertex_count < 3 || mesh->index_count < 3 || mesh->index_count % 3 != 0)
    return NULL;

  if (max_error <= 0.0f)
    return clone_mesh(mesh);
  Mesh *topology_mesh = NULL;

  if (!weld_mesh_vertices(mesh, &topology_mesh))
    return NULL;
  BLB_QEMWorkMesh work;

  if (!clone_work_mesh(topology_mesh, &work)) {
    BLB_MeshDestroy(topology_mesh);
    destroy_work_mesh(&work);

    return NULL;
  }
  BLB_MeshDestroy(topology_mesh);

  if (work.active_triangles < 2) {
    Mesh *copy = clone_mesh(mesh);
    destroy_work_mesh(&work);

    return copy;
  }
  BLB_Vec3 min_v, max_v;

  if (!calculate_bounds(&work, &min_v, &max_v)) {
    destroy_work_mesh(&work);

    return NULL;
  }
  float diagonal = BLB_LenV3(BLB_SubV3(max_v, min_v));

  if (!isfinite(diagonal) || diagonal <= 0.000001f) {
    Mesh *copy = clone_mesh(mesh);
    destroy_work_mesh(&work);

    return copy;
  }
  float allowed_distance = diagonal * fmaxf(max_error, 0.0f);
  double allowed_error = (double)allowed_distance * (double)allowed_distance;

  if (!(reduction > 0.0f && reduction < 1.0f))
    reduction = 0.5f;
  reduction = fminf(fmaxf(reduction, 0.10f), 0.95f);
  size_t target_triangles = (size_t)((double)work.active_triangles * (double)reduction);

  if (target_triangles < 1u)
    target_triangles = 1u;

  if (target_triangles >= work.active_triangles) {
    Mesh *copy = clone_mesh(mesh);
    destroy_work_mesh(&work);

    return copy;
  }

  if (!seed_heap(&work)) {
    destroy_work_mesh(&work);

    return NULL;
  }

  while (work.active_triangles > target_triangles && work.heap.count > 0) {
    BLB_QEMCandidate candidate;

    if (!heap_pop(&work.heap, &candidate))
      break;

    if (candidate.edge_id >= work.edge_count)
      continue;
    BLB_QEMEdge *edge = &work.edges[candidate.edge_id];

    if (!edge->active || edge->version != candidate.edge_version || edge->a != candidate.a || edge->b != candidate.b)
      continue;

    if (candidate.error > allowed_error)
      break;

    if (!collapse_edge(&work, &candidate))
      continue;
  }
  Mesh *result = build_output_mesh(&work);
  destroy_work_mesh(&work);

  if (!result)
    return NULL;

  if (!mesh_topology_safe(mesh, result)) {
    BLB_MeshDestroy(result);

    return clone_mesh(mesh);
  }

  if (result->index_count >= mesh->index_count && result->vertex_count >= mesh->vertex_count) {
    BLB_MeshDestroy(result);

    return clone_mesh(mesh);
  }

  return result;
}

static Mesh *clone_mesh(const Mesh *source) {
  if (!source || !source->vertices || !source->indices)
    return NULL;
  Mesh *mesh = calloc(1, sizeof(*mesh));

  if (!mesh)
    return NULL;
  mesh->vertex_count = source->vertex_count;
  mesh->index_count = source->index_count;
  mesh->vertices = malloc(mesh->vertex_count * sizeof(*mesh->vertices));
  mesh->indices = malloc(mesh->index_count * sizeof(*mesh->indices));

  if (source->normals)
    mesh->normals = malloc(mesh->vertex_count * sizeof(*mesh->normals));

  if (source->uvs)
    mesh->uvs = malloc(mesh->vertex_count * sizeof(*mesh->uvs));

  if (!mesh->vertices || !mesh->indices || (source->normals && !mesh->normals) || (source->uvs && !mesh->uvs)) {
    BLB_MeshDestroy(mesh);

    return NULL;
  }
  memcpy(mesh->vertices, source->vertices, mesh->vertex_count * sizeof(*mesh->vertices));
  memcpy(mesh->indices, source->indices, mesh->index_count * sizeof(*mesh->indices));

  if (source->normals)
    memcpy(mesh->normals, source->normals, mesh->vertex_count * sizeof(*mesh->normals));

  if (source->uvs)
    memcpy(mesh->uvs, source->uvs, mesh->vertex_count * sizeof(*mesh->uvs));

  return mesh;
}

Mesh *BLB_MeshSimplifyQEM(const Mesh *mesh, float max_error) { return mesh_simplify_qem_impl(mesh, max_error, BLB_HSA_REDUCTION_FACTOR); }

Mesh *BLB_MeshSimplifyQEMWithReduction(const Mesh *mesh, float max_error, float reduction) {
  return mesh_simplify_qem_impl(mesh, max_error, reduction);
}

bool BLB_MeshBuildHSA(const Mesh *source, int levels, float error, Mesh ***lods, size_t *lod_count) {
  if (!source || levels <= 0 || error <= 0.0f || !lods || !lod_count)
    return false;
  *lods = NULL;
  *lod_count = 0;
  Mesh **result = calloc((size_t)levels, sizeof(*result));

  if (!result)
    return false;
  Mesh *current = clone_mesh(source);

  if (!current) {
    free(result);

    return false;
  }
  result[0] = current;
  *lod_count = 1;
  for (int level = 1; level < levels; ++level) {
    float level_error = error * powf(2.0f, (float)(level - 1));
    Mesh *next = BLB_MeshSimplifyQEM(current, level_error);

    if (!next)
      break;

    if (next->index_count >= current->index_count && next->vertex_count >= current->vertex_count) {
      BLB_MeshDestroy(next);
      break;
    }
    result[level] = next;
    *lod_count = (size_t)level + 1u;
    current = next;
  }
  *lods = result;
  return true;
}

void BLB_MeshDestroy(Mesh *mesh) {
  if (!mesh)
    return;
  free(mesh->vertices);
  free(mesh->normals);
  free(mesh->uvs);
  free(mesh->indices);
  free(mesh);
}

void BLB_MeshDestroyLODChain(Mesh **lods, size_t lod_count) {
  if (!lods)
    return;

  for (size_t i = 0; i < lod_count; ++i)
    BLB_MeshDestroy(lods[i]);
  free(lods);
}
