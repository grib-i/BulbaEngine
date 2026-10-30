#include "bulba/core/objects2d/particles2d.h"
#include "bulba/core/utils/colors.h"
#include "bulba/core/utils/delta_time.h"
#undef BLB_CreateParticle2D
#undef BLB_CreateParticleSystem2D

#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static BLB_Polygon2D *quad_polygon = NULL;
static size_t quad_polygon_refs = 0;

static BLB_ParticleID generate_particle_id(void);

static uint32_t next_particle_id = 1;
static uint32_t next_particle_system_id = 1;

static void BLB_Particle2D_FreePolygon(void) {
  if (quad_polygon == NULL)
    return;

  free(quad_polygon->vertices);
  free(quad_polygon->base_vertices);
  free(quad_polygon->uvs);
  free(quad_polygon->indices);
  free(quad_polygon);

  quad_polygon = NULL;
}

static BLB_Polygon2D *create_quad_polygon(void) {
  BLB_Polygon2D *polygon = calloc(1, sizeof(*polygon));

  if (polygon == NULL)
    return NULL;

  polygon->vertices = malloc(sizeof(BLB_Vec2) * 4);
  polygon->base_vertices = malloc(sizeof(BLB_Vec2) * 4);
  polygon->uvs = malloc(sizeof(BLB_Vec2) * 4);
  polygon->indices = malloc(sizeof(unsigned int) * 6);

  if (polygon->vertices == NULL || polygon->base_vertices == NULL || polygon->uvs == NULL || polygon->indices == NULL) {
    free(polygon->vertices);
    free(polygon->base_vertices);
    free(polygon->uvs);
    free(polygon->indices);
    free(polygon);

    return NULL;
  }

  BLB_Vec2 vertices[4] = {BLB_V2(-0.5f, -0.5f), BLB_V2(0.5f, -0.5f), BLB_V2(0.5f, 0.5f), BLB_V2(-0.5f, 0.5f)};

  BLB_Vec2 uvs[4] = {BLB_V2(0.0f, 0.0f), BLB_V2(1.0f, 0.0f), BLB_V2(1.0f, 1.0f), BLB_V2(0.0f, 1.0f)};

  unsigned int indices[6] = {0, 1, 2, 0, 2, 3};

  for (size_t i = 0; i < 4; i++) {
    polygon->vertices[i] = vertices[i];
    polygon->base_vertices[i] = vertices[i];
    polygon->uvs[i] = uvs[i];
  }

  for (size_t i = 0; i < 6; i++)
    polygon->indices[i] = indices[i];

  polygon->vertex_count = 4;
  polygon->index_count = 6;

  return polygon;
}

static BLB_Polygon2D *acquire_quad_polygon(void) {
  if (quad_polygon == NULL)
    quad_polygon = create_quad_polygon();

  if (quad_polygon != NULL)
    ++quad_polygon_refs;

  return quad_polygon;
}

static void release_quad_polygon(BLB_Polygon2D *polygon) {
  if (polygon == NULL || polygon != quad_polygon)
    return;

  if (quad_polygon_refs > 0)
    --quad_polygon_refs;

  if (quad_polygon_refs == 0)
    BLB_Particle2D_FreePolygon();
}

static float variation_value(uint32_t id) {
  float value = fmodf((float)id * 0.61803398875f, 1.0f);

  return value * 2.0f - 1.0f;
}

static BLB_ParticleID generate_particle_id(void) {
  BLB_ParticleID id = {0};

  id.id = next_particle_id++;

  if (next_particle_id == BLB_INVALID_PARTICLE_ID)
    next_particle_id = 1;

  id.type[0] = 'p';
  id.type[1] = '2';
  id.type[2] = '\0';

  return id;
}

static BLB_ParticleSystemID generate_particle_system_id(void) {
  BLB_ParticleSystemID id = {0};

  id.id = next_particle_system_id++;

  if (next_particle_system_id == BLB_INVALID_PARTICLE_SYSTEM_ID)
    next_particle_system_id = 1;

  id.type[0] = 'p';
  id.type[1] = 's';
  id.type[2] = '2';
  id.type[3] = '\0';

  return id;
}

static BLB_Vec2 particle_scale_from_system(const BLB_ParticleSystem2D *ps, uint32_t id) {
  float variation = variation_value(id);

  return BLB_V2((ps->particle_scale.x + ps->particle_scale_variation.x * variation) * ps->particle_system_scale.x,
                (ps->particle_scale.y + ps->particle_scale_variation.y * variation) * ps->particle_system_scale.y);
}

static float particle_lifetime_max(const BLB_ParticleSystem2D *ps, uint32_t id) {
  return ps->lifetime + ps->lifetime_variation * variation_value(id);
}

static void release_particle_resources(BLB_Particle2D *particle) {
  if (particle == NULL)
    return;

  if (particle->material != NULL) {
    BLB_Material_Release(particle->material);
    particle->material = NULL;
  }

  if (particle->texture != NULL) {
    BLB_Texture_Release(particle->texture);
    particle->texture = NULL;
  }

  release_quad_polygon(particle->polygon);
  particle->polygon = NULL;
}

static BLB_Vec2 particle_velocity_from_system(const BLB_ParticleSystem2D *ps, uint32_t id) {
  return BLB_V2(ps->velocity.x + ps->velocity_variation.x * variation_value(id + 17u),
                ps->velocity.y + ps->velocity_variation.y * variation_value(id + 31u));
}

static bool initialize_particle(BLB_Particle2D *particle, BLB_ParticleID id, BLB_Vec2 position, BLB_Vec2 velocity,
                                BLB_Vec2 scale, float rotation, float rotation_speed, BLB_Color color, float lifetime,
                                float lifetime_max, BLB_Texture *texture, BLB_Material *material,
                                bool camera_attached) {
  if (particle == NULL || id.id == BLB_INVALID_PARTICLE_ID)
    return false;

  memset(particle, 0, sizeof(*particle));

  particle->id = id;
  particle->position = position;
  particle->velocity = velocity;
  particle->scale = scale;
  particle->rotation = rotation;
  particle->rotation_speed = rotation_speed;
  particle->color = color;
  particle->lifetime = lifetime;
  particle->lifetime_max = lifetime_max;
  particle->delta_time = BLB_DeltaTime_Get();

  particle->render_mode = BLB_RENDER_OPAQUE;
  particle->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE | BLB_PARTICLE;
  particle->visible = true;
  particle->camera_attached = camera_attached;

  particle->polygon = acquire_quad_polygon();

  if (particle->polygon == NULL)
    return false;

  if (material != NULL) {
    BLB_Material_Retain(material);
    particle->material = material;
  } else {
    particle->material = BLB_Material_Create2D();

    if (particle->material == NULL) {
      release_quad_polygon(particle->polygon);
      particle->polygon = NULL;

      return false;
    }
    BLB_Material_SetAlphaMode(particle->material, BLB_ALPHA_BLEND);
    BLB_Material_SetRenderMode(particle->material, BLB_RENDER_ADDITIVE);
    BLB_Material_SetLighting(particle->material, false);
    BLB_Material_SetUnlit(particle->material, true);
    BLB_Material_SetDepth(particle->material, false, false);
    BLB_Material_SetDoubleSided(particle->material, true);
  }

  if (texture != NULL) {
    BLB_Texture_Retain(texture);
    particle->texture = texture;
  }

  return true;
}

BLB_Particle2D *BLB_CreateParticle2D(BLB_ParticleID id, BLB_Vec2 position, BLB_Vec2 velocity, BLB_Vec2 scale,
                                     float rotation, float rotation_speed, BLB_Color color, float lifetime,
                                     float lifetime_max, BLB_Texture *texture, BLB_Material *material) {
  if (id.id == BLB_INVALID_PARTICLE_ID)
    id = generate_particle_id();

  BLB_Particle2D *particle = calloc(1, sizeof(*particle));

  if (particle == NULL)
    return NULL;

  if (!initialize_particle(particle, id, position, velocity, scale, rotation, rotation_speed, color, lifetime,
                           lifetime_max, texture, material, false)) {
    free(particle);

    return NULL;
  }

  return particle;
}

void BLB_DestroyParticle2D(BLB_Particle2D *p) {
  if (p == NULL)
    return;

  release_particle_resources(p);
  free(p);
}

void BLB_Particle2D_Move(BLB_Particle2D *p, BLB_Vec2 velocity) {
  if (p == NULL)
    return;

  if (p->delta_time >= 0.0f)
    velocity = BLB_MulV2F(velocity, p->delta_time);

  p->position = BLB_AddV2(p->position, velocity);
}

void BLB_Particle2D_SetPosition(BLB_Particle2D *p, BLB_Vec2 position) {
  if (p == NULL)
    return;

  p->position = position;
}

void BLB_Particle2D_Rotate(BLB_Particle2D *p, float angular_velocity) {
  if (p == NULL)
    return;

  if (p->delta_time >= 0.0f)
    angular_velocity *= p->delta_time;

  p->rotation += angular_velocity;
}

void BLB_Particle2D_SetRotation(BLB_Particle2D *p, float rotation) {
  if (p == NULL)
    return;

  p->rotation = rotation;
}

void BLB_Particle2D_Scale(BLB_Particle2D *p, BLB_Vec2 scale_velocity) {
  if (p == NULL)
    return;

  if (p->delta_time >= 0.0f)
    scale_velocity = BLB_MulV2F(scale_velocity, p->delta_time);

  p->scale = BLB_AddV2(p->scale, scale_velocity);
}

void BLB_Particle2D_SetScale(BLB_Particle2D *p, BLB_Vec2 scale) {
  if (p == NULL)
    return;

  p->scale = scale;
}

void BLB_Particle2D_Transform(BLB_Particle2D *p, BLB_Vec2 position, float rotation, BLB_Vec2 scale) {
  if (p == NULL)
    return;

  p->position = position;
  p->rotation = rotation;
  p->scale = scale;
}

int BLB_Particle2D_SetTexture(BLB_Particle2D *p, BLB_Texture *texture) {
  if (p == NULL)
    return -1;

  if (texture && texture->type == BLB_TEXTURE_3D)
    return -1;

  if (p->texture == texture)
    return 0;

  if (texture != NULL)
    BLB_Texture_Retain(texture);

  if (p->texture != NULL)
    BLB_Texture_Release(p->texture);

  p->texture = texture;

  return 0;
}

void BLB_Particle2D_SetMaterial(BLB_Particle2D *p, BLB_Material *material) {
  if (p == NULL || p->material == material)
    return;

  if (material != NULL)
    BLB_Material_Retain(material);

  if (p->material != NULL)
    BLB_Material_Release(p->material);

  p->material = material;
}

void BLB_Particle2D_FlipX(BLB_Particle2D *p) {
  if (p == NULL)
    return;

  p->scale.x *= -1.0f;
}

void BLB_Particle2D_FlipY(BLB_Particle2D *p) {
  if (p == NULL)
    return;

  p->scale.y *= -1.0f;
}

BLB_ParticleSystem2D *BLB_CreateParticleSystem2D(BLB_Vec2 particle_system_scale, BLB_Vec2 position, size_t count,
                                                 size_t capacity, float lifetime, float lifetime_variation,
                                                 BLB_Texture *texture) {
  BLB_ParticleSystem2D *ps = calloc(1, sizeof(*ps));

  if (ps == NULL)
    return NULL;

  if (capacity < count)
    capacity = count;

  ps->id = generate_particle_system_id();

  ps->count = 0;
  ps->capacity = capacity;

  if (capacity > 0) {
    ps->particles = calloc(capacity, sizeof(*ps->particles));

    if (ps->particles == NULL) {
      free(ps);

      return NULL;
    }
  }

  ps->particle_system_scale = particle_system_scale;
  ps->particle_scale = BLB_V2(1.0f, 1.0f);
  ps->particle_scale_variation = BLB_V2(0.0f, 0.0f);

  ps->position = position;
  ps->velocity = BLB_V2(0.0f, 0.0f);
  ps->velocity_variation = BLB_V2(0.0f, 0.0f);
  ps->gravity = BLB_V2(0.0f, -9.81f);

  ps->rotation = 0.0f;
  ps->rotation_speed = 0.0f;

  ps->color = BLB_COLOR_WHITE;
  ps->color_end = ps->color;

  ps->lifetime = lifetime;
  ps->lifetime_variation = lifetime_variation;
  ps->delta_time = BLB_DeltaTime_Get();

  ps->emission_rate = 0.0f;
  ps->emission_accumulator = 0.0f;

  ps->render_mode = BLB_RENDER_OPAQUE;
  ps->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE | BLB_PARTICLE_SYSTEM;
  ps->visible = true;
  ps->camera_attached = false;

  ps->looping = false;
  ps->playing = true;

  if (texture != NULL) {
    BLB_Texture_Retain(texture);
    ps->texture = texture;
  }

  ps->material = BLB_Material_Create2D();

  if (ps->material == NULL) {
    BLB_DestroyParticleSystem2D(ps);

    return NULL;
  }

  BLB_Material_SetAlphaMode(ps->material, BLB_ALPHA_BLEND);
  BLB_Material_SetRenderMode(ps->material, BLB_RENDER_ADDITIVE);
  BLB_Material_SetLighting(ps->material, false);
  BLB_Material_SetUnlit(ps->material, true);
  BLB_Material_SetDepth(ps->material, false, false);
  BLB_Material_SetDoubleSided(ps->material, true);

  for (size_t i = 0; i < count; i++) {
    BLB_ParticleID id = generate_particle_id();

    BLB_Vec2 scale = particle_scale_from_system(ps, id.id);
    float particle_lifetime = particle_lifetime_max(ps, id.id);

    if (particle_lifetime < 0.0f)
      particle_lifetime = 0.0f;

    if (!initialize_particle(&ps->particles[i], id, ps->position, particle_velocity_from_system(ps, id.id), scale,
                             ps->rotation, ps->rotation_speed, ps->color, 0.0f, particle_lifetime, ps->texture,
                             ps->material, ps->camera_attached)) {
      ps->count = i;
      BLB_DestroyParticleSystem2D(ps);

      return NULL;
    }

    ++ps->count;
  }

  return ps;
}

void BLB_DestroyParticleSystem2D(BLB_ParticleSystem2D *ps) {
  if (ps == NULL)
    return;

  for (size_t i = 0; i < ps->count; i++)
    release_particle_resources(&ps->particles[i]);

  free(ps->particles);

  if (ps->material != NULL)
    BLB_Material_Release(ps->material);

  if (ps->texture != NULL)
    BLB_Texture_Release(ps->texture);

  free(ps);
}

size_t BLB_ParticleSystem2D_Emit(BLB_ParticleSystem2D *ps, size_t count) {
  if (!ps || count == 0)
    return 0;
  size_t emitted = 0;

  for (size_t i = 0; i < ps->count && emitted < count; ++i) {
    if (ps->particles[i].visible)
      continue;
    BLB_ParticleID id = generate_particle_id();
    BLB_Vec2 scale = particle_scale_from_system(ps, id.id);
    float lifetime = particle_lifetime_max(ps, id.id);

    if (lifetime < 0.0f)
      lifetime = 0.0f;
    release_particle_resources(&ps->particles[i]);

    if (!initialize_particle(&ps->particles[i], id, ps->position, particle_velocity_from_system(ps, id.id), scale,
                             ps->rotation, ps->rotation_speed, ps->color, 0.0f, lifetime, ps->texture, ps->material,
                             ps->camera_attached))
      continue;
    emitted++;
  }

  while (emitted < count && ps->count < ps->capacity) {
    BLB_ParticleID id = generate_particle_id();
    BLB_Vec2 scale = particle_scale_from_system(ps, id.id);
    float lifetime = particle_lifetime_max(ps, id.id);

    if (lifetime < 0.0f)
      lifetime = 0.0f;

    if (!initialize_particle(&ps->particles[ps->count], id, ps->position, particle_velocity_from_system(ps, id.id),
                             scale, ps->rotation, ps->rotation_speed, ps->color, 0.0f, lifetime, ps->texture,
                             ps->material, ps->camera_attached))
      break;
    ps->count++;
    emitted++;
  }

  return emitted;
}

bool BLB_ParticleSystem2D_SetCount(BLB_ParticleSystem2D *ps, size_t count) {
  if (ps == NULL)
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
    size_t new_capacity = ps->capacity != 0 ? ps->capacity : 1;

    while (new_capacity < count) {
      if (new_capacity > SIZE_MAX / 2) {
        new_capacity = count;
        break;
      }

      new_capacity *= 2;
    }

    BLB_Particle2D *particles = realloc(ps->particles, sizeof(*particles) * new_capacity);

    if (particles == NULL)
      return false;

    memset(particles + ps->capacity, 0, sizeof(*particles) * (new_capacity - ps->capacity));

    ps->particles = particles;
    ps->capacity = new_capacity;
  }

  size_t old_count = ps->count;

  for (size_t i = old_count; i < count; i++) {
    BLB_ParticleID id = generate_particle_id();

    BLB_Vec2 scale = particle_scale_from_system(ps, id.id);
    float particle_lifetime = particle_lifetime_max(ps, id.id);

    if (particle_lifetime < 0.0f)
      particle_lifetime = 0.0f;

    if (!initialize_particle(&ps->particles[i], id, ps->position, particle_velocity_from_system(ps, id.id), scale,
                             ps->rotation, ps->rotation_speed, ps->color, 0.0f, particle_lifetime, ps->texture,
                             ps->material, ps->camera_attached)) {
      for (size_t j = old_count; j < i; j++)
        release_particle_resources(&ps->particles[j]);

      memset(ps->particles + old_count, 0, sizeof(*ps->particles) * (i - old_count));
      ps->count = old_count;

      return false;
    }
  }

  ps->count = count;

  return true;
}

void BLB_ParticleSystem2D_Clear(BLB_ParticleSystem2D *ps) {
  if (ps == NULL)
    return;

  for (size_t i = 0; i < ps->count; i++)
    release_particle_resources(&ps->particles[i]);

  if (ps->particles != NULL)
    memset(ps->particles, 0, sizeof(*ps->particles) * ps->count);

  ps->count = 0;
  ps->emission_accumulator = 0.0f;
}

void BLB_ParticleSystem2D_Update(BLB_ParticleSystem2D *ps, float delta_time) {
  if (ps == NULL || !isfinite(delta_time))
    return;

  if (delta_time < 0.0f)
    delta_time = 0.0f;

  ps->delta_time = delta_time;

  if (!ps->playing || !ps->visible)
    return;

  for (size_t i = 0; i < ps->count; i++) {
    BLB_Particle2D *p = &ps->particles[i];

    p->delta_time = delta_time;

    if (ps->camera_attached)
      p->camera_attached = true;

    if (!p->visible)
      continue;

    p->velocity = BLB_AddV2(p->velocity, BLB_MulV2F(ps->gravity, delta_time));

    p->position = BLB_AddV2(p->position, BLB_MulV2F(p->velocity, delta_time));

    p->rotation += p->rotation_speed * delta_time;

    p->lifetime += delta_time;

    p->color = BLB_ColorLerp(ps->color, ps->color_end, p->lifetime_max > 0.0f ? p->lifetime / p->lifetime_max : 0.0f);

    if (p->lifetime_max <= 0.0f)
      continue;

    if (p->lifetime >= p->lifetime_max) {
      if (ps->looping) {
        p->position = ps->position;
        p->velocity = particle_velocity_from_system(ps, p->id.id);
        p->rotation = ps->rotation;
        p->rotation_speed = ps->rotation_speed;
        p->scale = particle_scale_from_system(ps, p->id.id);
        p->lifetime = 0.0f;
        p->color = ps->color;
        p->visible = true;
      } else {
        p->visible = false;
      }
    }
  }

  if (ps->emission_rate > 0.0f) {
    ps->emission_accumulator += ps->emission_rate * delta_time;

    size_t emit_count = (size_t)ps->emission_accumulator;

    if (emit_count > 0) {
      size_t emitted = BLB_ParticleSystem2D_Emit(ps, emit_count);
      ps->emission_accumulator -= (float)emitted;
    }
  }
}

void BLB_ParticleSystem2D_Play(BLB_ParticleSystem2D *ps) {
  if (ps == NULL)
    return;

  ps->playing = true;
}

void BLB_ParticleSystem2D_Stop(BLB_ParticleSystem2D *ps) {
  if (ps == NULL)
    return;

  ps->playing = false;
}

void BLB_ParticleSystem2D_Restart(BLB_ParticleSystem2D *ps) {
  if (ps == NULL)
    return;

  for (size_t i = 0; i < ps->count; i++) {
    BLB_Particle2D *p = &ps->particles[i];

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

void BLB_ParticleSystem2D_Move(BLB_ParticleSystem2D *ps, BLB_Vec2 velocity) {
  if (ps == NULL)
    return;

  if (ps->delta_time >= 0.0f)
    velocity = BLB_MulV2F(velocity, ps->delta_time);

  ps->position = BLB_AddV2(ps->position, velocity);

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].position = BLB_AddV2(ps->particles[i].position, velocity);
}

void BLB_ParticleSystem2D_SetPosition(BLB_ParticleSystem2D *ps, BLB_Vec2 position) {
  if (ps == NULL)
    return;

  BLB_Vec2 delta = BLB_SubV2(position, ps->position);

  ps->position = position;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].position = BLB_AddV2(ps->particles[i].position, delta);
}

void BLB_ParticleSystem2D_Rotate(BLB_ParticleSystem2D *ps, float angular_velocity) {
  if (ps == NULL)
    return;

  if (ps->delta_time >= 0.0f)
    angular_velocity *= ps->delta_time;

  ps->rotation += angular_velocity;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].rotation += angular_velocity;
}

void BLB_ParticleSystem2D_SetRotation(BLB_ParticleSystem2D *ps, float rotation) {
  if (ps == NULL)
    return;

  float delta = rotation - ps->rotation;

  ps->rotation = rotation;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].rotation += delta;
}

void BLB_ParticleSystem2D_Scale(BLB_ParticleSystem2D *ps, BLB_Vec2 scale_velocity) {
  if (ps == NULL)
    return;

  if (ps->delta_time >= 0.0f)
    scale_velocity = BLB_MulV2F(scale_velocity, ps->delta_time);

  ps->particle_system_scale = BLB_AddV2(ps->particle_system_scale, scale_velocity);

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale = BLB_AddV2(ps->particles[i].scale, scale_velocity);
}

void BLB_ParticleSystem2D_SetScale(BLB_ParticleSystem2D *ps, BLB_Vec2 scale) {
  if (ps == NULL)
    return;

  BLB_Vec2 delta = BLB_SubV2(scale, ps->particle_system_scale);

  ps->particle_system_scale = scale;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale = BLB_AddV2(ps->particles[i].scale, delta);
}

void BLB_ParticleSystem2D_Transform(BLB_ParticleSystem2D *ps, BLB_Vec2 position, float rotation, BLB_Vec2 scale) {
  if (ps == NULL)
    return;

  BLB_ParticleSystem2D_SetPosition(ps, position);
  BLB_ParticleSystem2D_SetRotation(ps, rotation);
  BLB_ParticleSystem2D_SetScale(ps, scale);
}

int BLB_ParticleSystem2D_SetTexture(BLB_ParticleSystem2D *ps, BLB_Texture *texture) {
  if (ps == NULL)
    return -1;

  if (texture && texture->type == BLB_TEXTURE_3D)
    return -1;

  if (ps->texture == texture)
    return 0;

  if (texture != NULL)
    BLB_Texture_Retain(texture);

  for (size_t i = 0; i < ps->count; i++) {
    if (BLB_Particle2D_SetTexture(&ps->particles[i], texture) != 0) {
      if (texture != NULL)
        BLB_Texture_Release(texture);

      return -1;
    }
  }

  if (ps->texture != NULL)
    BLB_Texture_Release(ps->texture);

  ps->texture = texture;

  return 0;
}

void BLB_ParticleSystem2D_SetMaterial(BLB_ParticleSystem2D *ps, BLB_Material *material) {
  if (ps == NULL || ps->material == material)
    return;

  if (material != NULL)
    BLB_Material_Retain(material);

  for (size_t i = 0; i < ps->count; i++)
    BLB_Particle2D_SetMaterial(&ps->particles[i], material);

  if (ps->material != NULL)
    BLB_Material_Release(ps->material);

  ps->material = material;
}

void BLB_ParticleSystem2D_FlipX(BLB_ParticleSystem2D *ps) {
  if (ps == NULL)
    return;

  ps->particle_system_scale.x *= -1.0f;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale.x *= -1.0f;
}

void BLB_ParticleSystem2D_FlipY(BLB_ParticleSystem2D *ps) {
  if (ps == NULL)
    return;

  ps->particle_system_scale.y *= -1.0f;

  for (size_t i = 0; i < ps->count; i++)
    ps->particles[i].scale.y *= -1.0f;
}

void BLB_Particle2D_SetCameraAttached(BLB_Particle2D *p, bool attached) {
  if (!p)
    return;
  p->camera_attached = attached;
}

bool BLB_Particle2D_IsCameraAttached(const BLB_Particle2D *p) {
  return p && p->camera_attached;
}

void BLB_ParticleSystem2D_SetCameraAttached(BLB_ParticleSystem2D *ps, bool attached) {
  if (!ps)
    return;
  ps->camera_attached = attached;

  for (size_t i = 0; i < ps->count; ++i)
    ps->particles[i].camera_attached = attached;
}

bool BLB_ParticleSystem2D_IsCameraAttached(const BLB_ParticleSystem2D *ps) {
  return ps && ps->camera_attached;
}
