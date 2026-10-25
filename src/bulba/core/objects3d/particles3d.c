#include "bulba/core/objects3d/particles3d.h"
#include "bulba/core/utils/delta_time.h"

#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static BLB_Polygon3D *cube_polygon = NULL;
static BLB_Polygon3D *quad_polygon = NULL;

static Mesh cube_mesh;
static Mesh quad_mesh;

static size_t cube_polygon_refs = 0;
static size_t quad_polygon_refs = 0;

static BLB_ParticleID generate_particle_id(void);

static uint32_t next_particle_id = 1;
static uint32_t next_particle_system_id = 1;

static void free_polygon(BLB_Polygon3D **polygon) {
  if (polygon == NULL || *polygon == NULL)
    return;

  free((*polygon)->vertices);
  free((*polygon)->base_vertices);
  free((*polygon)->uvs);
  free((*polygon)->indices);
  free((*polygon)->normals);
  free(*polygon);

  *polygon = NULL;
}

static float variation_value(uint32_t id) {
  float value = fmodf((float)id * 0.61803398875f, 1.0f);

  return value * 2.0f - 1.0f;
}

static BLB_Vec4 color_lerp(BLB_Vec4 a, BLB_Vec4 b, float t) {
  if (t < 0.0f)
    t = 0.0f;

  if (t > 1.0f)
    t = 1.0f;

  return BLB_V4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

static BLB_Polygon3D *create_quad_polygon(void) {
  BLB_Polygon3D *polygon = calloc(1, sizeof(*polygon));

  if (polygon == NULL)
    return NULL;

  polygon->vertices = malloc(sizeof(BLB_Vec3) * 4);
  polygon->base_vertices = malloc(sizeof(BLB_Vec3) * 4);
  polygon->uvs = malloc(sizeof(BLB_Vec2) * 4);
  polygon->indices = malloc(sizeof(unsigned int) * 6);
  polygon->normals = malloc(sizeof(BLB_Vec3) * 4);

  if (polygon->vertices == NULL || polygon->base_vertices == NULL || polygon->uvs == NULL || polygon->indices == NULL || polygon->normals == NULL) {
    free_polygon(&polygon);

    return NULL;
  }

  BLB_Vec3 vertices[4] = {BLB_V3(-0.5f, -0.5f, 0.0f), BLB_V3(0.5f, -0.5f, 0.0f), BLB_V3(0.5f, 0.5f, 0.0f), BLB_V3(-0.5f, 0.5f, 0.0f)};
  BLB_Vec2 uvs[4] = {BLB_V2(0.0f, 0.0f), BLB_V2(1.0f, 0.0f), BLB_V2(1.0f, 1.0f), BLB_V2(0.0f, 1.0f)};
  BLB_Vec3 normals[4] = {BLB_V3(0.0f, 0.0f, 1.0f), BLB_V3(0.0f, 0.0f, 1.0f), BLB_V3(0.0f, 0.0f, 1.0f), BLB_V3(0.0f, 0.0f, 1.0f)};
  unsigned int indices[6] = {0, 1, 2, 0, 2, 3};

  for (size_t i = 0; i < 4; i++) {
    polygon->vertices[i] = vertices[i];
    polygon->base_vertices[i] = vertices[i];
    polygon->uvs[i] = uvs[i];
    polygon->normals[i] = normals[i];
  }

  for (size_t i = 0; i < 6; i++)
    polygon->indices[i] = indices[i];

  polygon->vertex_count = 4;
  polygon->index_count = 6;

  quad_mesh.vertices = polygon->vertices;
  quad_mesh.normals = polygon->normals;
  quad_mesh.uvs = polygon->uvs;
  quad_mesh.vertex_count = polygon->vertex_count;
  quad_mesh.indices = polygon->indices;
  quad_mesh.index_count = polygon->index_count;

  return polygon;
}

static BLB_Polygon3D *create_cube_polygon(void) {
  BLB_Polygon3D *polygon = calloc(1, sizeof(*polygon));

  if (polygon == NULL)
    return NULL;

  polygon->vertices = malloc(sizeof(BLB_Vec3) * 24);
  polygon->base_vertices = malloc(sizeof(BLB_Vec3) * 24);
  polygon->uvs = malloc(sizeof(BLB_Vec2) * 24);
  polygon->indices = malloc(sizeof(unsigned int) * 36);
  polygon->normals = malloc(sizeof(BLB_Vec3) * 24);

  if (polygon->vertices == NULL || polygon->base_vertices == NULL || polygon->uvs == NULL || polygon->indices == NULL || polygon->normals == NULL) {
    free_polygon(&polygon);

    return NULL;
  }

  BLB_Vec3 vertices[24] = {BLB_V3(-0.5f, -0.5f, -0.5f), BLB_V3(0.5f, -0.5f, -0.5f),  BLB_V3(0.5f, 0.5f, -0.5f),  BLB_V3(-0.5f, 0.5f, -0.5f),

                           BLB_V3(0.5f, -0.5f, -0.5f),  BLB_V3(0.5f, -0.5f, 0.5f),   BLB_V3(0.5f, 0.5f, 0.5f),   BLB_V3(0.5f, 0.5f, -0.5f),

                           BLB_V3(0.5f, -0.5f, 0.5f),   BLB_V3(-0.5f, -0.5f, 0.5f),  BLB_V3(-0.5f, 0.5f, 0.5f),  BLB_V3(0.5f, 0.5f, 0.5f),

                           BLB_V3(-0.5f, -0.5f, 0.5f),  BLB_V3(-0.5f, -0.5f, -0.5f), BLB_V3(-0.5f, 0.5f, -0.5f), BLB_V3(-0.5f, 0.5f, 0.5f),

                           BLB_V3(-0.5f, 0.5f, -0.5f),  BLB_V3(0.5f, 0.5f, -0.5f),   BLB_V3(0.5f, 0.5f, 0.5f),   BLB_V3(-0.5f, 0.5f, 0.5f),

                           BLB_V3(-0.5f, -0.5f, 0.5f),  BLB_V3(0.5f, -0.5f, 0.5f),   BLB_V3(0.5f, -0.5f, -0.5f), BLB_V3(-0.5f, -0.5f, -0.5f)};

  unsigned int indices[36] = {0,  3,  2,  0,  2,  1,  4,  7,  6,  4,  6,  5,  8,  10, 9,  8,  11, 10,
                              12, 15, 14, 12, 14, 13, 16, 19, 18, 16, 18, 17, 20, 23, 22, 20, 22, 21};

  BLB_Vec3 normals[24] = {BLB_V3(0.0f, 0.0f, -1.0f), BLB_V3(0.0f, 0.0f, -1.0f), BLB_V3(0.0f, 0.0f, -1.0f), BLB_V3(0.0f, 0.0f, -1.0f),

                          BLB_V3(1.0f, 0.0f, 0.0f),  BLB_V3(1.0f, 0.0f, 0.0f),  BLB_V3(1.0f, 0.0f, 0.0f),  BLB_V3(1.0f, 0.0f, 0.0f),

                          BLB_V3(0.0f, 0.0f, 1.0f),  BLB_V3(0.0f, 0.0f, 1.0f),  BLB_V3(0.0f, 0.0f, 1.0f),  BLB_V3(0.0f, 0.0f, 1.0f),

                          BLB_V3(-1.0f, 0.0f, 0.0f), BLB_V3(-1.0f, 0.0f, 0.0f), BLB_V3(-1.0f, 0.0f, 0.0f), BLB_V3(-1.0f, 0.0f, 0.0f),

                          BLB_V3(0.0f, 1.0f, 0.0f),  BLB_V3(0.0f, 1.0f, 0.0f),  BLB_V3(0.0f, 1.0f, 0.0f),  BLB_V3(0.0f, 1.0f, 0.0f),

                          BLB_V3(0.0f, -1.0f, 0.0f), BLB_V3(0.0f, -1.0f, 0.0f), BLB_V3(0.0f, -1.0f, 0.0f), BLB_V3(0.0f, -1.0f, 0.0f)};

  BLB_Vec2 face_uvs[6][4] = {{BLB_V2(1.00f, 0.333333f), BLB_V2(0.75f, 0.333333f), BLB_V2(0.75f, 0.666667f), BLB_V2(1.00f, 0.666667f)},
                             {BLB_V2(0.75f, 0.333333f), BLB_V2(0.50f, 0.333333f), BLB_V2(0.50f, 0.666667f), BLB_V2(0.75f, 0.666667f)},
                             {BLB_V2(0.50f, 0.333333f), BLB_V2(0.25f, 0.333333f), BLB_V2(0.25f, 0.666667f), BLB_V2(0.50f, 0.666667f)},
                             {BLB_V2(0.25f, 0.333333f), BLB_V2(0.00f, 0.333333f), BLB_V2(0.00f, 0.666667f), BLB_V2(0.25f, 0.666667f)},
                             {BLB_V2(0.25f, 1.000000f), BLB_V2(0.50f, 1.000000f), BLB_V2(0.50f, 0.666667f), BLB_V2(0.25f, 0.666667f)},
                             {BLB_V2(0.25f, 0.333333f), BLB_V2(0.50f, 0.333333f), BLB_V2(0.50f, 0.000000f), BLB_V2(0.25f, 0.000000f)}};

  for (size_t face = 0; face < 6; face++) {
    for (size_t corner = 0; corner < 4; corner++) {
      size_t index = face * 4 + corner;
      polygon->vertices[index] = vertices[index];
      polygon->base_vertices[index] = vertices[index];
      polygon->uvs[index] = face_uvs[face][corner];
      polygon->normals[index] = normals[index];
    }
  }

  for (size_t i = 0; i < 36; i++)
    polygon->indices[i] = indices[i];

  polygon->vertex_count = 24;
  polygon->index_count = 36;

  cube_mesh.vertices = polygon->vertices;
  cube_mesh.normals = polygon->normals;
  cube_mesh.uvs = polygon->uvs;
  cube_mesh.vertex_count = polygon->vertex_count;
  cube_mesh.indices = polygon->indices;
  cube_mesh.index_count = polygon->index_count;

  return polygon;
}

static BLB_Polygon3D *acquire_particle_polygon(BLB_Vec3 scale) {
  if (scale.z == 0.0f) {
    if (quad_polygon == NULL)
      quad_polygon = create_quad_polygon();

    if (quad_polygon != NULL)
      quad_polygon_refs++;

    return quad_polygon;
  }

  if (cube_polygon == NULL)
    cube_polygon = create_cube_polygon();

  if (cube_polygon != NULL)
    cube_polygon_refs++;

  return cube_polygon;
}

static Mesh *get_particle_mesh(BLB_Polygon3D *polygon) {
  if (polygon == quad_polygon)
    return &quad_mesh;

  if (polygon == cube_polygon)
    return &cube_mesh;

  return NULL;
}

static void release_particle_polygon(BLB_Polygon3D *polygon) {
  if (polygon == NULL)
    return;

  if (polygon == quad_polygon) {
    if (quad_polygon_refs > 0)
      quad_polygon_refs--;

    if (quad_polygon_refs == 0) {
      free_polygon(&quad_polygon);
      quad_mesh = (Mesh){0};
    }

    return;
  }

  if (polygon == cube_polygon) {
    if (cube_polygon_refs > 0)
      cube_polygon_refs--;

    if (cube_polygon_refs == 0) {
      free_polygon(&cube_polygon);
      cube_mesh = (Mesh){0};
    }
  }
}

static void release_particle_resources(BLB_Particle3D *particle) {
  if (particle == NULL)
    return;

  if (particle->material) {
    BLB_Material_Release(particle->material);
    particle->material = NULL;
  }

  if (particle->texture) {
    BLB_Texture_Release(particle->texture);
    particle->texture = NULL;
  }

  release_particle_polygon(particle->polygon);

  particle->polygon = NULL;
  particle->mesh = NULL;
}

static BLB_Vec3 particle_scale_from_system(const BLB_ParticleSystem3D *ps, uint32_t id) {
  float variation = variation_value(id);

  return BLB_V3((ps->particle_scale.x + ps->particle_scale_variation.x * variation) * ps->particle_system_scale.x,
                (ps->particle_scale.y + ps->particle_scale_variation.y * variation) * ps->particle_system_scale.y,
                (ps->particle_scale.z + ps->particle_scale_variation.z * variation) * ps->particle_system_scale.z);
}

static float particle_lifetime_max(const BLB_ParticleSystem3D *ps, uint32_t id) {
  return ps->lifetime + ps->lifetime_variation * variation_value(id);
}

static BLB_Vec3 particle_velocity_from_system(const BLB_ParticleSystem3D *ps, uint32_t id) {
  return BLB_V3(ps->velocity.x + ps->velocity_variation.x * variation_value(id + 17u),
                ps->velocity.y + ps->velocity_variation.y * variation_value(id + 31u),
                ps->velocity.z + ps->velocity_variation.z * variation_value(id + 47u));
}

static bool initialize_particle(BLB_Particle3D *particle, BLB_ParticleID id, BLB_Vec3 position, BLB_Vec3 velocity, BLB_Vec3 scale, BLB_Vec3 rotation,
                                BLB_Vec4 color, float lifetime, float lifetime_max, BLB_Texture *texture, BLB_Material *material) {
  if (particle == NULL)
    return false;

  memset(particle, 0, sizeof(*particle));

  if (id.id == BLB_INVALID_PARTICLE_ID)
    return false;

  particle->id = id;
  particle->position = position;
  particle->velocity = velocity;
  particle->scale = scale;
  particle->rotation = rotation;
  particle->rotation_speed = BLB_V3(0.0f, 0.0f, 0.0f);
  particle->color = color;
  particle->lifetime = lifetime;
  particle->lifetime_max = lifetime_max;
  particle->delta_time = BLB_DeltaTime_Get();
  particle->render_mode = BLB_RENDER_OPAQUE;
  particle->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE | BLB_PARTICLE;
  particle->visible = true;

  particle->polygon = acquire_particle_polygon(scale);

  if (particle->polygon == NULL)
    return false;

  particle->mesh = get_particle_mesh(particle->polygon);

  if (particle->mesh == NULL) {
    release_particle_polygon(particle->polygon);
    particle->polygon = NULL;

    return false;
  }

  if (material != NULL) {
    BLB_Material_Retain(material);
    particle->material = material;
  } else {
    particle->material = BLB_Material_Create3D();

    if (particle->material == NULL) {
      release_particle_polygon(particle->polygon);
      particle->polygon = NULL;
      particle->mesh = NULL;

      return false;
    }
    BLB_Material_SetAlphaMode(particle->material, BLB_ALPHA_BLEND);
    BLB_Material_SetRenderMode(particle->material, BLB_RENDER_ADDITIVE);
    BLB_Material_SetLighting(particle->material, false);
    BLB_Material_SetUnlit(particle->material, true);
    BLB_Material_SetDepth(particle->material, true, false);
    BLB_Material_SetDoubleSided(particle->material, true);
  }

  if (texture != NULL) {
    BLB_Texture_Retain(texture);
    particle->texture = texture;
  }

  return true;
}

static BLB_ParticleID generate_particle_id(void) {
  BLB_ParticleID id = {0};

  id.id = next_particle_id++;

  if (next_particle_id == BLB_INVALID_PARTICLE_ID)
    next_particle_id = 1;

  id.type[0] = 'p';
  id.type[1] = '\0';

  return id;
}

static BLB_ParticleSystemID generate_particle_system_id(void) {
  BLB_ParticleSystemID id = {0};

  id.id = next_particle_system_id++;

  if (next_particle_system_id == BLB_INVALID_PARTICLE_SYSTEM_ID)
    next_particle_system_id = 1;

  id.type[0] = 'p';
  id.type[1] = 's';
  id.type[2] = '\0';

  return id;
}

BLB_Particle3D *BLB_CreateParticle3D(BLB_ParticleID id, BLB_Vec3 position, BLB_Vec3 velocity, BLB_Vec3 scale, BLB_Vec3 rotation, BLB_Vec4 color,
                                     float lifetime, float lifetime_max, BLB_Texture *texture, BLB_Material *material) {
  if (id.id == BLB_INVALID_PARTICLE_ID)
    id = generate_particle_id();

  BLB_Particle3D *particle = calloc(1, sizeof(*particle));

  if (particle == NULL)
    return NULL;

  if (!initialize_particle(particle, id, position, velocity, scale, rotation, color, lifetime, lifetime_max, texture, material)) {
    free(particle);

    return NULL;
  }

  return particle;
}

void BLB_DestroyParticle3D(BLB_Particle3D *p) {
  if (p == NULL)
    return;

  release_particle_resources(p);
  free(p);
}

void BLB_Particle3D_Move(BLB_Particle3D *p, BLB_Vec3 velocity) {
  if (!p)
    return;

  if (p->delta_time >= 0.0f)
    velocity = BLB_MulV3F(velocity, p->delta_time);

  p->position = BLB_AddV3(p->position, velocity);
}

void BLB_Particle3D_SetPosition(BLB_Particle3D *p, BLB_Vec3 position) {
  if (!p)
    return;

  p->position = position;
}

void BLB_Particle3D_Rotate(BLB_Particle3D *p, BLB_Vec3 angular_velocity) {
  if (!p)
    return;

  if (p->delta_time >= 0.0f)
    angular_velocity = BLB_MulV3F(angular_velocity, p->delta_time);

  p->rotation = BLB_AddV3(p->rotation, angular_velocity);
}

void BLB_Particle3D_SetRotation(BLB_Particle3D *p, BLB_Vec3 rotation) {
  if (!p)
    return;

  p->rotation = rotation;
}

void BLB_Particle3D_Scale(BLB_Particle3D *p, BLB_Vec3 scale_velocity) {
  if (!p)
    return;

  if (p->delta_time >= 0.0f)
    scale_velocity = BLB_MulV3F(scale_velocity, p->delta_time);

  p->scale = BLB_AddV3(p->scale, scale_velocity);
}

void BLB_Particle3D_SetScale(BLB_Particle3D *p, BLB_Vec3 scale) {
  if (!p)
    return;

  p->scale = scale;
}

void BLB_Particle3D_Transform(BLB_Particle3D *p, BLB_Vec3 position, BLB_Vec3 rotation, BLB_Vec3 scale) {
  if (!p)
    return;

  p->position = position;
  p->rotation = rotation;
  p->scale = scale;
}

void BLB_Particle3D_SetTexture(BLB_Particle3D *p, BLB_Texture *texture) {
  if (!p || p->texture == texture)
    return;

  if (texture)
    BLB_Texture_Retain(texture);

  if (p->texture)
    BLB_Texture_Release(p->texture);

  p->texture = texture;
}

void BLB_Particle3D_SetMaterial(BLB_Particle3D *p, BLB_Material *material) {
  if (!p || p->material == material)
    return;

  if (material)
    BLB_Material_Retain(material);

  if (p->material)
    BLB_Material_Release(p->material);

  p->material = material;
}

void BLB_Particle3D_FlipX(BLB_Particle3D *p) {
  if (!p)
    return;

  p->scale.x *= -1.0f;
}

void BLB_Particle3D_FlipY(BLB_Particle3D *p) {
  if (!p)
    return;

  p->scale.y *= -1.0f;
}

void BLB_Particle3D_FlipZ(BLB_Particle3D *p) {
  if (!p)
    return;

  p->scale.z *= -1.0f;
}

BLB_ParticleSystem3D *BLB_CreateParticleSystem3D(BLB_Vec3 particle_system_scale, BLB_Vec3 position, size_t count, size_t capacity, float lifetime,
                                                 float lifetime_variation, BLB_Texture *texture) {
  BLB_ParticleSystem3D *ps = calloc(1, sizeof(*ps));

  if (ps == NULL)
    return NULL;

  if (capacity < count)
    capacity = count;

  ps->id = generate_particle_system_id();

  ps->count = 0;
  ps->capacity = capacity;

  ps->particles = NULL;

  if (capacity > 0) {
    ps->particles = calloc(capacity, sizeof(*ps->particles));

    if (ps->particles == NULL) {
      free(ps);

      return NULL;
    }
  }

  ps->emission_rate = 0.0f;
  ps->emission_accumulator = 0.0f;
  ps->lifetime = lifetime;
  ps->lifetime_variation = lifetime_variation;
  ps->delta_time = BLB_DeltaTime_Get();

  ps->render_mode = BLB_RENDER_OPAQUE;
  ps->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE | BLB_PARTICLE_SYSTEM;
  ps->visible = true;

  ps->position = position;
  ps->velocity = BLB_V3(0.0f, 0.0f, 0.0f);
  ps->velocity_variation = BLB_V3(0.0f, 0.0f, 0.0f);
  ps->gravity = BLB_V3(0.0f, -9.81f, 0.0f);

  ps->particle_system_scale = particle_system_scale;
  ps->particle_scale = BLB_V3(1.0f, 1.0f, 0.0f);
  ps->particle_scale_variation = BLB_V3(0.0f, 0.0f, 0.0f);

  ps->color = BLB_V4(255.0f, 255.0f, 255.0f, 255.0f);
  ps->color_end = ps->color;

  ps->rotation = BLB_V3(0.0f, 0.0f, 0.0f);
  ps->rotation_speed = BLB_V3(0.0f, 0.0f, 0.0f);

  ps->looping = false;
  ps->playing = true;

  if (texture != NULL) {
    BLB_Texture_Retain(texture);
    ps->texture = texture;
  }

  ps->material = BLB_Material_Create3D();

  if (ps->material == NULL) {
    BLB_DestroyParticleSystem3D(ps);

    return NULL;
  }

  BLB_Material_SetAlphaMode(ps->material, BLB_ALPHA_BLEND);
  BLB_Material_SetRenderMode(ps->material, BLB_RENDER_ADDITIVE);
  BLB_Material_SetLighting(ps->material, false);
  BLB_Material_SetUnlit(ps->material, true);
  BLB_Material_SetDepth(ps->material, true, false);
  BLB_Material_SetDoubleSided(ps->material, true);

  for (size_t i = 0; i < count; i++) {
    BLB_ParticleID id = generate_particle_id();

    BLB_Vec3 scale = particle_scale_from_system(ps, id.id);
    float particle_lifetime = particle_lifetime_max(ps, id.id);

    if (particle_lifetime < 0.0f)
      particle_lifetime = 0.0f;

    if (!initialize_particle(&ps->particles[i], id, ps->position, particle_velocity_from_system(ps, id.id), scale, ps->rotation, ps->color, 0.0f,
                             particle_lifetime, ps->texture, ps->material)) {
      ps->count = i;
      BLB_DestroyParticleSystem3D(ps);

      return NULL;
    }

    ps->particles[i].rotation_speed = ps->rotation_speed;
    ps->particles[i].camera_attached = ps->camera_attached;
    ps->particles[ps->count].camera_attached = ps->camera_attached;
    ps->count++;
  }

  return ps;
}

void BLB_DestroyParticleSystem3D(BLB_ParticleSystem3D *ps) {
  if (!ps)
    return;

  for (size_t i = 0; i < ps->count; i++)
    release_particle_resources(&ps->particles[i]);

  free(ps->particles);

  if (ps->material)
    BLB_Material_Release(ps->material);

  if (ps->texture)
    BLB_Texture_Release(ps->texture);

  free(ps);
}

size_t BLB_ParticleSystem3D_Emit(BLB_ParticleSystem3D *ps, size_t count) {
  if (!ps || count == 0)
    return 0;
  size_t emitted = 0;

  for (size_t i = 0; i < ps->count && emitted < count; ++i) {
    if (ps->particles[i].visible)
      continue;
    BLB_ParticleID id = generate_particle_id();
    BLB_Vec3 scale = particle_scale_from_system(ps, id.id);
    float lifetime = particle_lifetime_max(ps, id.id);

    if (lifetime < 0.0f)
      lifetime = 0.0f;
    release_particle_resources(&ps->particles[i]);

    if (!initialize_particle(&ps->particles[i], id, ps->position, particle_velocity_from_system(ps, id.id), scale, ps->rotation, ps->color, 0.0f,
                             lifetime, ps->texture, ps->material))
      continue;
    ps->particles[i].rotation_speed = ps->rotation_speed;
    ps->particles[i].camera_attached = ps->camera_attached;
    emitted++;
  }

  while (emitted < count && ps->count < ps->capacity) {
    BLB_ParticleID id = generate_particle_id();
    BLB_Vec3 scale = particle_scale_from_system(ps, id.id);
    float lifetime = particle_lifetime_max(ps, id.id);

    if (lifetime < 0.0f)
      lifetime = 0.0f;

    if (!initialize_particle(&ps->particles[ps->count], id, ps->position, particle_velocity_from_system(ps, id.id), scale, ps->rotation, ps->color,
                             0.0f, lifetime, ps->texture, ps->material))
      break;
    ps->particles[ps->count].rotation_speed = ps->rotation_speed;
    ps->particles[ps->count].camera_attached = ps->camera_attached;
    ps->count++;
    emitted++;
  }

  return emitted;
}

bool BLB_ParticleSystem3D_SetCount(BLB_ParticleSystem3D *ps, size_t count) {
  if (!ps)
    return false;

  if (count < ps->count) {
    for (size_t i = count; i < ps->count; i++)
      release_particle_resources(&ps->particles[i]);

    memset(ps->particles + count, 0, sizeof(*ps->particles) * (ps->count - count));
    ps->count = count;

    return true;
  }

  if (count == ps->count)
    return true;

  if (count > ps->capacity) {
    size_t new_capacity = ps->capacity == 0 ? 1 : ps->capacity;

    while (new_capacity < count) {
      if (new_capacity > SIZE_MAX / 2) {
        new_capacity = count;
        break;
      }

      new_capacity *= 2;
    }

    BLB_Particle3D *particles = realloc(ps->particles, sizeof(*particles) * new_capacity);

    if (particles == NULL)
      return false;

    memset(particles + ps->capacity, 0, sizeof(*particles) * (new_capacity - ps->capacity));

    ps->particles = particles;
    ps->capacity = new_capacity;
  }

  size_t old_count = ps->count;

  for (size_t i = old_count; i < count; i++) {
    BLB_ParticleID id = generate_particle_id();

    BLB_Vec3 scale = particle_scale_from_system(ps, id.id);
    float particle_lifetime = particle_lifetime_max(ps, id.id);

    if (particle_lifetime < 0.0f)
      particle_lifetime = 0.0f;

    if (!initialize_particle(&ps->particles[i], id, ps->position, particle_velocity_from_system(ps, id.id), scale, ps->rotation, ps->color, 0.0f,
                             particle_lifetime, ps->texture, ps->material)) {
      for (size_t j = old_count; j < i; j++)
        release_particle_resources(&ps->particles[j]);

      memset(ps->particles + old_count, 0, sizeof(*ps->particles) * (i - old_count));
      ps->count = old_count;

      return false;
    }
  }

  for (size_t i = old_count; i < count; ++i) {
    ps->particles[i].rotation_speed = ps->rotation_speed;
    ps->particles[i].camera_attached = ps->camera_attached;
  }

  ps->count = count;

  return true;
}

void BLB_ParticleSystem3D_Clear(BLB_ParticleSystem3D *ps) {
  if (!ps)
    return;

  for (size_t i = 0; i < ps->count; i++)
    release_particle_resources(&ps->particles[i]);

  if (ps->particles != NULL)
    memset(ps->particles, 0, sizeof(*ps->particles) * ps->count);

  ps->count = 0;
  ps->emission_accumulator = 0.0f;
}

void BLB_ParticleSystem3D_Update(BLB_ParticleSystem3D *ps, float delta_time) {
  if (!ps || !isfinite(delta_time))
    return;

  if (delta_time < 0.0f)
    delta_time = 0.0f;

  ps->delta_time = delta_time;

  if (!ps->playing || !ps->visible)
    return;

  for (size_t i = 0; i < ps->count; i++) {
    BLB_Particle3D *p = &ps->particles[i];

    p->delta_time = delta_time;

    if (ps->camera_attached)
      p->camera_attached = true;

    if (!p->visible)
      continue;

    p->velocity = BLB_AddV3(p->velocity, BLB_MulV3F(ps->gravity, delta_time));
    p->position = BLB_AddV3(p->position, BLB_MulV3F(p->velocity, delta_time));
    p->rotation = BLB_AddV3(p->rotation, BLB_MulV3F(p->rotation_speed, delta_time));
    p->lifetime += delta_time;

    if (p->lifetime_max > 0.0f) {
      float t = p->lifetime / p->lifetime_max;

      if (t > 1.0f)
        t = 1.0f;

      p->color = color_lerp(ps->color, ps->color_end, t);

      if (p->lifetime >= p->lifetime_max) {
        if (ps->looping) {
          p->position = ps->position;
          p->velocity = particle_velocity_from_system(ps, p->id.id);
          p->rotation = ps->rotation;
          p->rotation_speed = ps->rotation_speed;
          p->lifetime = 0.0f;
          p->visible = true;
          p->color = ps->color;
        } else {
          p->visible = false;
        }
      }
    }
  }

  if (ps->emission_rate > 0.0f) {
    ps->emission_accumulator += ps->emission_rate * delta_time;
    size_t emit_count = (size_t)ps->emission_accumulator;

    if (emit_count > 0) {
      size_t emitted = BLB_ParticleSystem3D_Emit(ps, emit_count);
      ps->emission_accumulator -= (float)emitted;

      if (ps->emission_accumulator > (float)(ps->capacity > 0 ? ps->capacity : 1u))
        ps->emission_accumulator = (float)(ps->capacity > 0 ? ps->capacity : 1u);
    }
  }
}

void BLB_ParticleSystem3D_Play(BLB_ParticleSystem3D *ps) {
  if (!ps)
    return;

  ps->playing = true;
}

void BLB_ParticleSystem3D_Stop(BLB_ParticleSystem3D *ps) {
  if (!ps)
    return;

  ps->playing = false;
}

void BLB_ParticleSystem3D_Restart(BLB_ParticleSystem3D *ps) {
  if (!ps)
    return;

  for (size_t i = 0; i < ps->count; i++) {
    BLB_Particle3D *p = &ps->particles[i];

    p->position = ps->position;
    p->velocity = particle_velocity_from_system(ps, p->id.id);
    p->rotation = ps->rotation;
    p->rotation_speed = ps->rotation_speed;
    p->scale = particle_scale_from_system(ps, p->id.id);
    p->lifetime = 0.0f;
    p->lifetime_max = particle_lifetime_max(ps, p->id.id);

    if (p->lifetime_max < 0.0f)
      p->lifetime_max = 0.0f;

    p->color = ps->color;
    p->visible = true;
    p->delta_time = 0.0f;
  }

  ps->emission_accumulator = 0.0f;
  ps->playing = true;
}

void BLB_ParticleSystem3D_Move(BLB_ParticleSystem3D *ps, BLB_Vec3 velocity) {
  if (!ps)
    return;

  if (ps->delta_time >= 0.0f)
    velocity = BLB_MulV3F(velocity, ps->delta_time);

  ps->position = BLB_AddV3(ps->position, velocity);

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].position = BLB_AddV3(ps->particles[i].position, velocity);
}

void BLB_ParticleSystem3D_SetPosition(BLB_ParticleSystem3D *ps, BLB_Vec3 position) {
  if (!ps)
    return;

  BLB_Vec3 delta = BLB_V3(position.x - ps->position.x, position.y - ps->position.y, position.z - ps->position.z);

  ps->position = position;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].position = BLB_AddV3(ps->particles[i].position, delta);
}

void BLB_ParticleSystem3D_Rotate(BLB_ParticleSystem3D *ps, BLB_Vec3 angular_velocity) {
  if (!ps)
    return;

  if (ps->delta_time >= 0.0f)
    angular_velocity = BLB_MulV3F(angular_velocity, ps->delta_time);

  ps->rotation = BLB_AddV3(ps->rotation, angular_velocity);

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].rotation = BLB_AddV3(ps->particles[i].rotation, angular_velocity);
}

void BLB_ParticleSystem3D_SetRotation(BLB_ParticleSystem3D *ps, BLB_Vec3 rotation) {
  if (!ps)
    return;

  BLB_Vec3 delta = BLB_V3(rotation.x - ps->rotation.x, rotation.y - ps->rotation.y, rotation.z - ps->rotation.z);

  ps->rotation = rotation;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].rotation = BLB_AddV3(ps->particles[i].rotation, delta);
}

void BLB_ParticleSystem3D_Scale(BLB_ParticleSystem3D *ps, BLB_Vec3 scale_velocity) {
  if (!ps)
    return;

  if (ps->delta_time >= 0.0f)
    scale_velocity = BLB_MulV3F(scale_velocity, ps->delta_time);

  ps->particle_system_scale = BLB_AddV3(ps->particle_system_scale, scale_velocity);

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale = BLB_AddV3(ps->particles[i].scale, scale_velocity);
}

void BLB_ParticleSystem3D_SetScale(BLB_ParticleSystem3D *ps, BLB_Vec3 scale) {
  if (!ps)
    return;

  BLB_Vec3 delta = BLB_V3(scale.x - ps->particle_system_scale.x, scale.y - ps->particle_system_scale.y, scale.z - ps->particle_system_scale.z);

  ps->particle_system_scale = scale;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale = BLB_AddV3(ps->particles[i].scale, delta);
}

void BLB_ParticleSystem3D_Transform(BLB_ParticleSystem3D *ps, BLB_Vec3 position, BLB_Vec3 rotation, BLB_Vec3 scale) {
  if (!ps)
    return;

  BLB_ParticleSystem3D_SetPosition(ps, position);
  BLB_ParticleSystem3D_SetRotation(ps, rotation);
  BLB_ParticleSystem3D_SetScale(ps, scale);
}

void BLB_ParticleSystem3D_SetTexture(BLB_ParticleSystem3D *ps, BLB_Texture *texture) {
  if (!ps || ps->texture == texture)
    return;

  if (texture)
    BLB_Texture_Retain(texture);

  for (size_t i = 0; i < ps->count; i++)
    BLB_Particle3D_SetTexture(&ps->particles[i], texture);

  if (ps->texture)
    BLB_Texture_Release(ps->texture);

  ps->texture = texture;
}

void BLB_ParticleSystem3D_SetMaterial(BLB_ParticleSystem3D *ps, BLB_Material *material) {
  if (!ps || ps->material == material)
    return;

  if (material)
    BLB_Material_Retain(material);

  for (size_t i = 0; i < ps->count; i++)
    BLB_Particle3D_SetMaterial(&ps->particles[i], material);

  if (ps->material)
    BLB_Material_Release(ps->material);

  ps->material = material;
}

void BLB_ParticleSystem3D_FlipX(BLB_ParticleSystem3D *ps) {
  if (!ps)
    return;

  ps->particle_system_scale.x *= -1.0f;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale.x *= -1.0f;
}

void BLB_ParticleSystem3D_FlipY(BLB_ParticleSystem3D *ps) {
  if (!ps)
    return;

  ps->particle_system_scale.y *= -1.0f;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale.y *= -1.0f;
}

void BLB_ParticleSystem3D_FlipZ(BLB_ParticleSystem3D *ps) {
  if (!ps)
    return;

  ps->particle_system_scale.z *= -1.0f;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale.z *= -1.0f;
}

void BLB_Particle3D_SetCameraAttached(BLB_Particle3D *p, bool attached) {
  if (!p)
    return;
  p->camera_attached = attached;
}

bool BLB_Particle3D_IsCameraAttached(const BLB_Particle3D *p) { return p && p->camera_attached; }

void BLB_ParticleSystem3D_SetCameraAttached(BLB_ParticleSystem3D *ps, bool attached) {
  if (!ps)
    return;
  ps->camera_attached = attached;

  for (size_t i = 0; i < ps->count; ++i)
    ps->particles[i].camera_attached = attached;
}

bool BLB_ParticleSystem3D_IsCameraAttached(const BLB_ParticleSystem3D *ps) { return ps && ps->camera_attached; }
