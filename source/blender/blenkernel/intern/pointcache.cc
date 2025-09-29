/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

/* needed for directory lookup */
#ifndef WIN32
#  include <dirent.h>
#else
#  include "BLI_winstuff.h"
#endif

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_fluid_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_compression.hh"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"
#include "BKE_cloth.hh"
#include "BKE_collection.hh"
#include "BKE_duplilist.hh"
#include "BKE_dynamicpaint.h"
#include "BKE_fluid.h"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.hh"
#include "BKE_softbody.h"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "BIK_api.h"

#ifdef WITH_BULLET
#  include "RBI_api.h"
#endif

#include <zstd.h>

#define PTCACHE_DATA_FROM(data, type, from) \
  if (data[type]) { \
    memcpy(data[type], from, ptcache_data_size[type]); \
  } \
  (void)0

#define PTCACHE_DATA_TO(data, type, index, to) \
  if (data[type]) { \
    memcpy(to, \
           (char *)(data)[type] + ((index) ? (index) * ptcache_data_size[type] : 0), \
           ptcache_data_size[type]); \
  } \
  (void)0

/* could be made into a pointcache option */
#define DURIAN_POINTCACHE_LIB_OK 1

static CLG_LogRef LOG = {"physics.pointcache"};

static int ptcache_data_size[] = {
    sizeof(uint),     /* BPHYS_DATA_INDEX */
    sizeof(float[3]), /* BPHYS_DATA_LOCATION */
    sizeof(float[3]), /* BPHYS_DATA_VELOCITY */
    sizeof(float[4]), /* BPHYS_DATA_ROTATION */
    sizeof(float[3]), /* BPHYS_DATA_AVELOCITY / BPHYS_DATA_XCONST */
    sizeof(float),    /* BPHYS_DATA_SIZE */
    sizeof(float[3]), /* BPHYS_DATA_TIMES */
    sizeof(BoidData), /* case BPHYS_DATA_BOIDS */
};

static int ptcache_extra_datasize[] = {
    0,
    sizeof(ParticleSpring),
    sizeof(float[3]),
};

/* forward declarations */
static int ptcache_file_compressed_read(PTCacheFile *pf,
                                        uchar *result,
                                        uint items_num,
                                        uint item_size);
static void ptcache_file_compressed_write(PTCacheFile *pf,
                                          const void *data,
                                          uint items_num,
                                          uint item_size);
static int ptcache_file_write(PTCacheFile *pf, const void *data, uint items_num, uint item_size);
static bool ptcache_file_read(PTCacheFile *pf, void *f, uint items_num, uint item_size);

/* Common functions */
static int ptcache_basic_header_read(PTCacheFile *pf)
{
  int error = 0;

  /* Custom functions should read these basic elements too! */
  if (!error && !fread(&pf->totpoint, sizeof(uint), 1, pf->fp)) {
    error = 1;
  }

  if (!error && !fread(&pf->data_types, sizeof(uint), 1, pf->fp)) {
    error = 1;
  }

  return !error;
}
static int ptcache_basic_header_write(PTCacheFile *pf)
{
  /* Custom functions should write these basic elements too! */
  if (!fwrite(&pf->totpoint, sizeof(uint), 1, pf->fp)) {
    return 0;
  }

  if (!fwrite(&pf->data_types, sizeof(uint), 1, pf->fp)) {
    return 0;
  }

  return 1;
}
static void ptcache_add_extra_data(PTCacheMem *pm, uint type, uint count, void *data)
{
  PTCacheExtra *extra = MEM_callocN<PTCacheExtra>("Point cache: extra data descriptor");

  extra->type = type;
  extra->totdata = count;

  size_t size = extra->totdata * ptcache_extra_datasize[extra->type];

  extra->data = MEM_mallocN(size, "Point cache: extra data");
  memcpy(extra->data, data, size);

  BLI_addtail(&pm->extradata, extra);
}
/* Softbody functions */
static int ptcache_softbody_write(int index, void *soft_v, void **data, int /*cfra*/)
{
  SoftBody *soft = static_cast<SoftBody *>(soft_v);
  BodyPoint *bp = soft->bpoint + index;

  PTCACHE_DATA_FROM(data, BPHYS_DATA_LOCATION, bp->pos);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_VELOCITY, bp->vec);

  return 1;
}
static void ptcache_softbody_read(
    int index, void *soft_v, void **data, float /*cfra*/, const float *old_data)
{
  SoftBody *soft = static_cast<SoftBody *>(soft_v);
  BodyPoint *bp = soft->bpoint + index;

  if (old_data) {
    memcpy(bp->pos, data, sizeof(float[3]));
    memcpy(bp->vec, data + 3, sizeof(float[3]));
  }
  else {
    PTCACHE_DATA_TO(data, BPHYS_DATA_LOCATION, 0, bp->pos);
    PTCACHE_DATA_TO(data, BPHYS_DATA_VELOCITY, 0, bp->vec);
  }
}
static void ptcache_softbody_interpolate(int index,
                                         void *soft_v,
                                         void **data,
                                         float cfra,
                                         float cfra1,
                                         float cfra2,
                                         const float *old_data)
{
  SoftBody *soft = static_cast<SoftBody *>(soft_v);
  BodyPoint *bp = soft->bpoint + index;
  ParticleKey keys[4];
  float dfra;

  if (cfra1 == cfra2) {
    return;
  }

  copy_v3_v3(keys[1].co, bp->pos);
  copy_v3_v3(keys[1].vel, bp->vec);

  if (old_data) {
    memcpy(keys[2].co, old_data, sizeof(float[3]));
    memcpy(keys[2].vel, old_data + 3, sizeof(float[3]));
  }
  else {
    BKE_ptcache_make_particle_key(keys + 2, 0, data, cfra2);
  }

  dfra = cfra2 - cfra1;

  mul_v3_fl(keys[1].vel, dfra);
  mul_v3_fl(keys[2].vel, dfra);

  psys_interpolate_particle(-1, keys, (cfra - cfra1) / dfra, keys, true);

  mul_v3_fl(keys->vel, 1.0f / dfra);

  copy_v3_v3(bp->pos, keys->co);
  copy_v3_v3(bp->vec, keys->vel);
}
static int ptcache_softbody_totpoint(void *soft_v, int /*cfra*/)
{
  SoftBody *soft = static_cast<SoftBody *>(soft_v);
  return soft->totpoint;
}
static void ptcache_softbody_error(const ID * /*owner_id*/,
                                   void * /*soft_v*/,
                                   const char * /*message*/)
{
  /* ignored for now */
}

/* Particle functions. */

void BKE_ptcache_make_particle_key(ParticleKey *key, int index, void **data, float time)
{
  PTCACHE_DATA_TO(data, BPHYS_DATA_LOCATION, index, key->co);
  PTCACHE_DATA_TO(data, BPHYS_DATA_VELOCITY, index, key->vel);

  /* no rotation info, so make something nice up */
  if (data[BPHYS_DATA_ROTATION] == nullptr) {
    vec_to_quat(key->rot, key->vel, OB_NEGX, OB_POSZ);
  }
  else {
    PTCACHE_DATA_TO(data, BPHYS_DATA_ROTATION, index, key->rot);
  }

  PTCACHE_DATA_TO(data, BPHYS_DATA_AVELOCITY, index, key->ave);
  key->time = time;
}
static int ptcache_particle_write(int index, void *psys_v, void **data, int cfra)
{
  ParticleSystem *psys = static_cast<ParticleSystem *>(psys_v);
  ParticleData *pa = psys->particles + index;
  BoidParticle *boid = (psys->part->phystype == PART_PHYS_BOIDS) ? pa->boid : nullptr;
  float times[3];
  int step = psys->pointcache->step;

  /* Skip some particles that are not stored in the cache. */
  if (data[BPHYS_DATA_INDEX]) {
    if (psys->part->flag & PART_DIED) {
      /* Dead particles are stored when they are displayed. */
      if (cfra < pa->time - step) {
        return 0;
      }
    }
    else {
      /* Inclusive ranges for particle lifetime (`dietime - 1` for an inclusive end-frame). */
      const int pa_sfra = int(pa->time) - step;
      const int pa_efra = (int(pa->dietime) - 1) + step;
      if (!(cfra >= pa_sfra && cfra <= pa_efra)) {
        return 0;
      }
    }
  }

  times[0] = pa->time;
  times[1] = pa->dietime;
  times[2] = pa->lifetime;

  PTCACHE_DATA_FROM(data, BPHYS_DATA_INDEX, &index);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_LOCATION, pa->state.co);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_VELOCITY, pa->state.vel);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_ROTATION, pa->state.rot);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_AVELOCITY, pa->state.ave);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_SIZE, &pa->size);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_TIMES, times);

  if (boid) {
    PTCACHE_DATA_FROM(data, BPHYS_DATA_BOIDS, &boid->data);
  }

  /* Return flag 1+1=2 for newly born particles
   * to copy exact birth location to previously cached frame. */
  return 1 + (pa->state.time >= pa->time && pa->prev_state.time <= pa->time);
}
static void ptcache_particle_read(
    int index, void *psys_v, void **data, float cfra, const float *old_data)
{
  ParticleSystem *psys = static_cast<ParticleSystem *>(psys_v);
  ParticleData *pa;
  BoidParticle *boid;
  float timestep = 0.04f * psys->part->timetweak;

  if (index >= psys->totpart) {
    return;
  }

  pa = psys->particles + index;
  boid = (psys->part->phystype == PART_PHYS_BOIDS) ? pa->boid : nullptr;

  if (cfra > pa->state.time) {
    memcpy(&pa->prev_state, &pa->state, sizeof(ParticleKey));
  }

  if (old_data) {
    /* old format cache */
    pa->state = *reinterpret_cast<const ParticleKey *>(old_data);
    return;
  }

  BKE_ptcache_make_particle_key(&pa->state, 0, data, cfra);

  /* set frames cached before birth to birth time */
  if (cfra < pa->time) {
    pa->state.time = pa->time;
  }
  else if (cfra > pa->dietime) {
    pa->state.time = pa->dietime;
  }

  if (data[BPHYS_DATA_SIZE]) {
    PTCACHE_DATA_TO(data, BPHYS_DATA_SIZE, 0, &pa->size);
  }

  if (data[BPHYS_DATA_TIMES]) {
    float times[3];
    PTCACHE_DATA_TO(data, BPHYS_DATA_TIMES, 0, &times);
    pa->time = times[0];
    pa->dietime = times[1];
    pa->lifetime = times[2];
  }

  if (boid) {
    PTCACHE_DATA_TO(data, BPHYS_DATA_BOIDS, 0, &boid->data);
  }

  /* determine velocity from previous location */
  if (data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_VELOCITY]) {
    if (cfra > pa->prev_state.time) {
      sub_v3_v3v3(pa->state.vel, pa->state.co, pa->prev_state.co);
      mul_v3_fl(pa->state.vel, (cfra - pa->prev_state.time) * timestep);
    }
    else {
      sub_v3_v3v3(pa->state.vel, pa->prev_state.co, pa->state.co);
      mul_v3_fl(pa->state.vel, (pa->prev_state.time - cfra) * timestep);
    }
  }

  /* default to no rotation */
  if (data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_ROTATION]) {
    unit_qt(pa->state.rot);
  }
}
static void ptcache_particle_interpolate(int index,
                                         void *psys_v,
                                         void **data,
                                         float cfra,
                                         float cfra1,
                                         float cfra2,
                                         const float *old_data)
{
  ParticleSystem *psys = static_cast<ParticleSystem *>(psys_v);
  ParticleData *pa;
  ParticleKey keys[4];
  float dfra, timestep = 0.04f * psys->part->timetweak;

  if (index >= psys->totpart) {
    return;
  }

  pa = psys->particles + index;

  /* Inclusive ranges for particle lifetime (`dietime - 1` for an inclusive end-frame). */
  const int pa_sfra = int(pa->time) - psys->pointcache->step;
  const int pa_efra = (int(pa->dietime) - 1) + psys->pointcache->step;

  /* Particle wasn't read from first cache so can't interpolate. */
  if (!(cfra1 >= pa_sfra && cfra1 <= pa_efra)) {
    return;
  }

  cfra = std::min(cfra, pa->dietime);
  cfra1 = std::min(cfra1, pa->dietime);
  cfra2 = std::min(cfra2, pa->dietime);

  if (cfra1 == cfra2) {
    return;
  }

  keys[1] = pa->state;
  if (old_data) {
    keys[2] = *reinterpret_cast<const ParticleKey *>(old_data);
  }
  else {
    BKE_ptcache_make_particle_key(keys + 2, 0, data, cfra2);
  }

  /* determine velocity from previous location */
  if (data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_VELOCITY]) {
    if (keys[1].time > keys[2].time) {
      sub_v3_v3v3(keys[2].vel, keys[1].co, keys[2].co);
      mul_v3_fl(keys[2].vel, (keys[1].time - keys[2].time) * timestep);
    }
    else {
      sub_v3_v3v3(keys[2].vel, keys[2].co, keys[1].co);
      mul_v3_fl(keys[2].vel, (keys[2].time - keys[1].time) * timestep);
    }
  }

  /* default to no rotation */
  if (data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_ROTATION]) {
    unit_qt(keys[2].rot);
  }

  if (cfra > pa->time) {
    cfra1 = std::max(cfra1, pa->time);
  }

  dfra = cfra2 - cfra1;

  mul_v3_fl(keys[1].vel, dfra * timestep);
  mul_v3_fl(keys[2].vel, dfra * timestep);

  psys_interpolate_particle(-1, keys, (cfra - cfra1) / dfra, &pa->state, true);
  interp_qt_qtqt(pa->state.rot, keys[1].rot, keys[2].rot, (cfra - cfra1) / dfra);

  mul_v3_fl(pa->state.vel, 1.0f / (dfra * timestep));

  pa->state.time = cfra;
}

static int ptcache_particle_totpoint(void *psys_v, int /*cfra*/)
{
  ParticleSystem *psys = static_cast<ParticleSystem *>(psys_v);
  return psys->totpart;
}

static void ptcache_particle_error(const ID * /*owner_id*/,
                                   void * /*psys_v*/,
                                   const char * /*message*/)
{
  /* ignored for now */
}

static int ptcache_particle_totwrite(void *psys_v, int cfra)
{
  ParticleSystem *psys = static_cast<ParticleSystem *>(psys_v);
  ParticleData *pa = psys->particles;
  int p, step = psys->pointcache->step;
  int totwrite = 0;

  if (cfra == 0) {
    return psys->totpart;
  }

  if (psys->part->flag & PART_DIED) {
    /* Also store dead particles when they are displayed. */
    for (p = 0; p < psys->totpart; p++, pa++) {
      const int pa_sfra = int(pa->time) - step;
      totwrite += (cfra >= pa_sfra);
    }
  }
  else {
    for (p = 0; p < psys->totpart; p++, pa++) {
      /* Inclusive ranges for particle lifetime (`dietime - 1` for an inclusive end-frame). */
      const int pa_sfra = int(pa->time) - step;
      const int pa_efra = (int(pa->dietime) - 1) + step;
      totwrite += (cfra >= pa_sfra) && (cfra <= pa_efra);
    }
  }

  return totwrite;
}

static void ptcache_particle_extra_write(void *psys_v, PTCacheMem *pm, int /*cfra*/)
{
  ParticleSystem *psys = static_cast<ParticleSystem *>(psys_v);

  if (psys->part->phystype == PART_PHYS_FLUID && psys->part->fluid &&
      psys->part->fluid->flag & SPH_VISCOELASTIC_SPRINGS && psys->tot_fluidsprings &&
      psys->fluid_springs)
  {
    ptcache_add_extra_data(
        pm, BPHYS_EXTRA_FLUID_SPRINGS, psys->tot_fluidsprings, psys->fluid_springs);
  }
}

static void ptcache_particle_extra_read(void *psys_v, PTCacheMem *pm, float /*cfra*/)
{
  ParticleSystem *psys = static_cast<ParticleSystem *>(psys_v);
  PTCacheExtra *extra = static_cast<PTCacheExtra *>(pm->extradata.first);

  for (; extra; extra = extra->next) {
    switch (extra->type) {
      case BPHYS_EXTRA_FLUID_SPRINGS: {
        if (psys->fluid_springs) {
          MEM_freeN(psys->fluid_springs);
        }

        psys->fluid_springs = static_cast<ParticleSpring *>(MEM_dupallocN(extra->data));
        psys->tot_fluidsprings = psys->alloc_fluidsprings = extra->totdata;
        break;
      }
    }
  }
}

/* Cloth functions */
static int ptcache_cloth_write(int index, void *cloth_v, void **data, int /*cfra*/)
{
  ClothModifierData *clmd = static_cast<ClothModifierData *>(cloth_v);
  Cloth *cloth = clmd->clothObject;
  ClothVertex *vert = cloth->verts + index;

  PTCACHE_DATA_FROM(data, BPHYS_DATA_LOCATION, vert->x);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_VELOCITY, vert->v);
  PTCACHE_DATA_FROM(data, BPHYS_DATA_XCONST, vert->xconst);

  return 1;
}
static void ptcache_cloth_read(
    int index, void *cloth_v, void **data, float /*cfra*/, const float *old_data)
{
  ClothModifierData *clmd = static_cast<ClothModifierData *>(cloth_v);
  Cloth *cloth = clmd->clothObject;
  ClothVertex *vert = cloth->verts + index;

  if (old_data) {
    memcpy(vert->x, data, sizeof(float[3]));
    memcpy(vert->xconst, data + 3, sizeof(float[3]));
    memcpy(vert->v, data + 6, sizeof(float[3]));
  }
  else {
    PTCACHE_DATA_TO(data, BPHYS_DATA_LOCATION, 0, vert->x);
    PTCACHE_DATA_TO(data, BPHYS_DATA_VELOCITY, 0, vert->v);
    PTCACHE_DATA_TO(data, BPHYS_DATA_XCONST, 0, vert->xconst);
  }
}
static void ptcache_cloth_interpolate(int index,
                                      void *cloth_v,
                                      void **data,
                                      float cfra,
                                      float cfra1,
                                      float cfra2,
                                      const float *old_data)
{
  ClothModifierData *clmd = static_cast<ClothModifierData *>(cloth_v);
  Cloth *cloth = clmd->clothObject;
  ClothVertex *vert = cloth->verts + index;
  ParticleKey keys[4];
  float dfra;

  if (cfra1 == cfra2) {
    return;
  }

  copy_v3_v3(keys[1].co, vert->x);
  copy_v3_v3(keys[1].vel, vert->v);

  if (old_data) {
    memcpy(keys[2].co, old_data, sizeof(float[3]));
    memcpy(keys[2].vel, old_data + 6, sizeof(float[3]));
  }
  else {
    BKE_ptcache_make_particle_key(keys + 2, 0, data, cfra2);
  }

  dfra = cfra2 - cfra1;

  mul_v3_fl(keys[1].vel, dfra);
  mul_v3_fl(keys[2].vel, dfra);

  psys_interpolate_particle(-1, keys, (cfra - cfra1) / dfra, keys, true);

  mul_v3_fl(keys->vel, 1.0f / dfra);

  copy_v3_v3(vert->x, keys->co);
  copy_v3_v3(vert->v, keys->vel);

  /* should vert->xconst be interpolated somehow too? - jahka */
}

static void ptcache_cloth_extra_write(void *cloth_v, PTCacheMem *pm, int /*cfra*/)
{
  ClothModifierData *clmd = static_cast<ClothModifierData *>(cloth_v);
  Cloth *cloth = clmd->clothObject;

  if (!is_zero_v3(cloth->average_acceleration)) {
    ptcache_add_extra_data(pm, BPHYS_EXTRA_CLOTH_ACCELERATION, 1, cloth->average_acceleration);
  }
}
static void ptcache_cloth_extra_read(void *cloth_v, PTCacheMem *pm, float /*cfra*/)
{
  ClothModifierData *clmd = static_cast<ClothModifierData *>(cloth_v);
  Cloth *cloth = clmd->clothObject;
  PTCacheExtra *extra = static_cast<PTCacheExtra *>(pm->extradata.first);

  zero_v3(cloth->average_acceleration);

  for (; extra; extra = extra->next) {
    switch (extra->type) {
      case BPHYS_EXTRA_CLOTH_ACCELERATION: {
        copy_v3_v3(cloth->average_acceleration, static_cast<const float *>(extra->data));
        break;
      }
    }
  }
}

static int ptcache_cloth_totpoint(void *cloth_v, int /*cfra*/)
{
  ClothModifierData *clmd = static_cast<ClothModifierData *>(cloth_v);
  return clmd->clothObject ? clmd->clothObject->mvert_num : 0;
}

static void ptcache_cloth_error(const ID *owner_id, void *cloth_v, const char *message)
{
  ClothModifierData *clmd = static_cast<ClothModifierData *>(cloth_v);
  BLI_assert(GS(owner_id->name) == ID_OB);
  if (clmd->hairdata == nullptr) {
    /* If there is hair data, this modifier does not actually exist on the object. */
    BKE_modifier_set_error((Object *)owner_id, &clmd->modifier, "%s", message);
  }
}

static int ptcache_dynamicpaint_totpoint(void *sd, int /*cfra*/)
{
  DynamicPaintSurface *surface = (DynamicPaintSurface *)sd;

  if (!surface->data) {
    return 0;
  }

  return surface->data->total_points;
}

static void ptcache_dynamicpaint_error(const ID * /*owner_id*/,
                                       void * /*sd*/,
                                       const char * /*message*/)
{
  /* ignored for now */
}

#define DPAINT_CACHE_VERSION "1.01"

static int ptcache_dynamicpaint_write(PTCacheFile *pf, void *dp_v)
{
  DynamicPaintSurface *surface = (DynamicPaintSurface *)dp_v;

  /* version header */
  ptcache_file_write(pf, DPAINT_CACHE_VERSION, 1, sizeof(char[4]));

  if (surface->format != MOD_DPAINT_SURFACE_F_IMAGESEQ && surface->data) {
    const int total_points = surface->data->total_points;

    /* cache type */
    ptcache_file_write(pf, &surface->type, 1, sizeof(int));

    uint in_stride;
    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      in_stride = sizeof(PaintPoint);
    }
    else if (ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WEIGHT)) {
      in_stride = sizeof(float);
    }
    else if (surface->type == MOD_DPAINT_SURFACE_T_WAVE) {
      in_stride = sizeof(PaintWavePoint);
    }
    else {
      return 0;
    }

    ptcache_file_compressed_write(pf, surface->data->type_data, total_points, in_stride);
  }
  return 1;
}
static int ptcache_dynamicpaint_read(PTCacheFile *pf, void *dp_v)
{
  DynamicPaintSurface *surface = (DynamicPaintSurface *)dp_v;
  char version[4];

  /* version header */
  ptcache_file_read(pf, version, 1, sizeof(char[4]));
  if (!STREQLEN(version, DPAINT_CACHE_VERSION, 4)) {
    CLOG_ERROR(&LOG, "Dynamic Paint: Invalid cache version: '%c%c%c%c'!", UNPACK4(version));
    return 0;
  }

  if (surface->format != MOD_DPAINT_SURFACE_F_IMAGESEQ && surface->data) {
    uint data_len;
    int surface_type;

    /* cache type */
    ptcache_file_read(pf, &surface_type, 1, sizeof(int));

    if (surface_type != surface->type) {
      return 0;
    }

    /* read surface data */
    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      data_len = sizeof(PaintPoint);
    }
    else if (ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WEIGHT)) {
      data_len = sizeof(float);
    }
    else if (surface->type == MOD_DPAINT_SURFACE_T_WAVE) {
      data_len = sizeof(PaintWavePoint);
    }
    else {
      return 0;
    }

    int ret = ptcache_file_compressed_read(
        pf, static_cast<uchar *>(surface->data->type_data), surface->data->total_points, data_len);
    if (ret) {
      CLOG_ERROR(&LOG, "Dynamic Paint: Unable to read the compressed cache data");
      return 0;
    }
  }
  return 1;
}

/* Rigid Body functions */
static int ptcache_rigidbody_write(int index, void *rb_v, void **data, int /*cfra*/)
{
  RigidBodyWorld *rbw = static_cast<RigidBodyWorld *>(rb_v);
  Object *ob = nullptr;

  if (rbw->objects) {
    ob = rbw->objects[index];
  }

  if (ob && ob->rigidbody_object) {
    RigidBodyOb *rbo = ob->rigidbody_object;

    if (rbo->type == RBO_TYPE_ACTIVE && rbo->shared->physics_object != nullptr) {
#ifdef WITH_BULLET
      RB_body_get_position(static_cast<rbRigidBody *>(rbo->shared->physics_object), rbo->pos);
      RB_body_get_orientation(static_cast<rbRigidBody *>(rbo->shared->physics_object), rbo->orn);
#endif
      PTCACHE_DATA_FROM(data, BPHYS_DATA_LOCATION, rbo->pos);
      PTCACHE_DATA_FROM(data, BPHYS_DATA_ROTATION, rbo->orn);
    }
  }

  return 1;
}
static void ptcache_rigidbody_read(
    int index, void *rb_v, void **data, float /*cfra*/, const float *old_data)
{
  RigidBodyWorld *rbw = static_cast<RigidBodyWorld *>(rb_v);
  Object *ob = nullptr;

  if (rbw->objects) {
    ob = rbw->objects[index];
  }

  if (ob && ob->rigidbody_object) {
    RigidBodyOb *rbo = ob->rigidbody_object;

    if (rbo->type == RBO_TYPE_ACTIVE) {

      if (old_data) {
        memcpy(rbo->pos, data, sizeof(float[3]));
        memcpy(rbo->orn, data + 3, sizeof(float[4]));
      }
      else {
        PTCACHE_DATA_TO(data, BPHYS_DATA_LOCATION, 0, rbo->pos);
        PTCACHE_DATA_TO(data, BPHYS_DATA_ROTATION, 0, rbo->orn);
      }
    }
  }
}
static void ptcache_rigidbody_interpolate(int index,
                                          void *rb_v,
                                          void **data,
                                          float cfra,
                                          float cfra1,
                                          float cfra2,
                                          const float *old_data)
{
  RigidBodyWorld *rbw = static_cast<RigidBodyWorld *>(rb_v);
  Object *ob = nullptr;

  if (rbw->objects) {
    ob = rbw->objects[index];
  }

  if (ob && ob->rigidbody_object) {
    RigidBodyOb *rbo = ob->rigidbody_object;

    if (rbo->type == RBO_TYPE_ACTIVE) {
      /* It may be possible to improve results by taking into account velocity
       * for interpolation using psys_interpolate_particle, however this is
       * not currently cached. */
      float pos[3], orn[4];

      if (old_data) {
        memcpy(pos, data, sizeof(float[3]));
        memcpy(orn, data + 3, sizeof(float[4]));
      }
      else {
        PTCACHE_DATA_TO(data, BPHYS_DATA_LOCATION, 0, pos);
        PTCACHE_DATA_TO(data, BPHYS_DATA_ROTATION, 0, orn);
      }

      const float t = (cfra - cfra1) / (cfra2 - cfra1);
      interp_v3_v3v3(rbo->pos, rbo->pos, pos, t);
      interp_qt_qtqt(rbo->orn, rbo->orn, orn, t);
    }
  }
}
static int ptcache_rigidbody_totpoint(void *rb_v, int /*cfra*/)
{
  RigidBodyWorld *rbw = static_cast<RigidBodyWorld *>(rb_v);

  return rbw->numbodies;
}

static void ptcache_rigidbody_error(const ID * /*owner_id*/,
                                    void * /*rb_v*/,
                                    const char * /*message*/)
{
  /* ignored for now */
}

/* Creating ID's */

void BKE_ptcache_id_from_softbody(PTCacheID *pid, Object *ob, SoftBody *sb)
{
  memset(pid, 0, sizeof(PTCacheID));

  pid->owner_id = &ob->id;
  pid->calldata = sb;
  pid->type = PTCACHE_TYPE_SOFTBODY;
  pid->cache = sb->shared->pointcache;
  pid->cache_ptr = &sb->shared->pointcache;
  pid->ptcaches = &sb->shared->ptcaches;
  pid->totpoint = pid->totwrite = ptcache_softbody_totpoint;
  pid->error = ptcache_softbody_error;

  pid->write_point = ptcache_softbody_write;
  pid->read_point = ptcache_softbody_read;
  pid->interpolate_point = ptcache_softbody_interpolate;

  pid->write_stream = nullptr;
  pid->read_stream = nullptr;

  pid->write_extra_data = nullptr;
  pid->read_extra_data = nullptr;
  pid->interpolate_extra_data = nullptr;

  pid->write_header = ptcache_basic_header_write;
  pid->read_header = ptcache_basic_header_read;

  pid->data_types = (1 << BPHYS_DATA_LOCATION) | (1 << BPHYS_DATA_VELOCITY);
  pid->info_types = 0;

  pid->stack_index = pid->cache->index;

  pid->default_step = 1;
  pid->max_step = 20;
  pid->file_type = PTCACHE_FILE_PTCACHE;
}
void BKE_ptcache_id_from_particles(PTCacheID *pid, Object *ob, ParticleSystem *psys)
{
  memset(pid, 0, sizeof(PTCacheID));

  pid->owner_id = &ob->id;
  pid->calldata = psys;
  pid->type = PTCACHE_TYPE_PARTICLES;
  pid->stack_index = psys->pointcache->index;
  pid->cache = psys->pointcache;
  pid->cache_ptr = &psys->pointcache;
  pid->ptcaches = &psys->ptcaches;

  if (psys->part->type != PART_HAIR) {
    pid->flag |= PTCACHE_VEL_PER_SEC;
  }

  pid->totpoint = ptcache_particle_totpoint;
  pid->totwrite = ptcache_particle_totwrite;
  pid->error = ptcache_particle_error;

  pid->write_point = ptcache_particle_write;
  pid->read_point = ptcache_particle_read;
  pid->interpolate_point = ptcache_particle_interpolate;

  pid->write_stream = nullptr;
  pid->read_stream = nullptr;

  pid->write_extra_data = nullptr;
  pid->read_extra_data = nullptr;
  pid->interpolate_extra_data = nullptr;

  pid->write_header = ptcache_basic_header_write;
  pid->read_header = ptcache_basic_header_read;

  pid->data_types = (1 << BPHYS_DATA_LOCATION) | (1 << BPHYS_DATA_VELOCITY) |
                    (1 << BPHYS_DATA_INDEX);

  if (psys->part->phystype == PART_PHYS_BOIDS) {
    pid->data_types |= (1 << BPHYS_DATA_AVELOCITY) | (1 << BPHYS_DATA_ROTATION) |
                       (1 << BPHYS_DATA_BOIDS);
  }
  else if (psys->part->phystype == PART_PHYS_FLUID && psys->part->fluid &&
           psys->part->fluid->flag & SPH_VISCOELASTIC_SPRINGS)
  {
    pid->write_extra_data = ptcache_particle_extra_write;
    pid->read_extra_data = ptcache_particle_extra_read;
  }

  if (psys->part->flag & PART_ROTATIONS) {
    pid->data_types |= (1 << BPHYS_DATA_ROTATION);

    if (psys->part->rotmode != PART_ROT_VEL || psys->part->avemode == PART_AVE_RAND ||
        psys->part->avefac != 0.0f)
    {
      pid->data_types |= (1 << BPHYS_DATA_AVELOCITY);
    }
  }

  pid->info_types = (1 << BPHYS_DATA_TIMES);

  pid->default_step = 1;
  pid->max_step = 20;
  pid->file_type = PTCACHE_FILE_PTCACHE;
}
void BKE_ptcache_id_from_cloth(PTCacheID *pid, Object *ob, ClothModifierData *clmd)
{
  memset(pid, 0, sizeof(PTCacheID));

  pid->owner_id = &ob->id;
  pid->calldata = clmd;
  pid->type = PTCACHE_TYPE_CLOTH;
  pid->stack_index = clmd->point_cache->index;
  pid->cache = clmd->point_cache;
  pid->cache_ptr = &clmd->point_cache;
  pid->ptcaches = &clmd->ptcaches;
  pid->totpoint = pid->totwrite = ptcache_cloth_totpoint;
  pid->error = ptcache_cloth_error;

  pid->write_point = ptcache_cloth_write;
  pid->read_point = ptcache_cloth_read;
  pid->interpolate_point = ptcache_cloth_interpolate;

  pid->write_stream = nullptr;
  pid->read_stream = nullptr;

  pid->write_extra_data = ptcache_cloth_extra_write;
  pid->read_extra_data = ptcache_cloth_extra_read;
  pid->interpolate_extra_data = nullptr;

  pid->write_header = ptcache_basic_header_write;
  pid->read_header = ptcache_basic_header_read;

  pid->data_types = (1 << BPHYS_DATA_LOCATION) | (1 << BPHYS_DATA_VELOCITY) |
                    (1 << BPHYS_DATA_XCONST);
  pid->info_types = 0;

  pid->default_step = 1;
  pid->max_step = 1;
  pid->file_type = PTCACHE_FILE_PTCACHE;
}

void BKE_ptcache_id_from_smoke(PTCacheID *pid, Object *ob, FluidModifierData *fmd)
{
  FluidDomainSettings *fds = fmd->domain;

  memset(pid, 0, sizeof(PTCacheID));

  pid->owner_id = &ob->id;
  pid->calldata = fmd;

  pid->type = PTCACHE_TYPE_SMOKE_DOMAIN;
  pid->stack_index = fds->point_cache[0]->index;

  pid->cache = fds->point_cache[0];
  pid->cache_ptr = &(fds->point_cache[0]);
  pid->ptcaches = &(fds->ptcaches[0]);
}

void BKE_ptcache_id_from_dynamicpaint(PTCacheID *pid, Object *ob, DynamicPaintSurface *surface)
{

  memset(pid, 0, sizeof(PTCacheID));

  pid->owner_id = &ob->id;
  pid->calldata = surface;
  pid->type = PTCACHE_TYPE_DYNAMICPAINT;
  pid->cache = surface->pointcache;
  pid->cache_ptr = &surface->pointcache;
  pid->ptcaches = &surface->ptcaches;
  pid->totpoint = pid->totwrite = ptcache_dynamicpaint_totpoint;
  pid->error = ptcache_dynamicpaint_error;

  pid->write_point = nullptr;
  pid->read_point = nullptr;
  pid->interpolate_point = nullptr;

  pid->write_stream = ptcache_dynamicpaint_write;
  pid->read_stream = ptcache_dynamicpaint_read;

  pid->write_extra_data = nullptr;
  pid->read_extra_data = nullptr;
  pid->interpolate_extra_data = nullptr;

  pid->write_header = ptcache_basic_header_write;
  pid->read_header = ptcache_basic_header_read;

  pid->data_types = BPHYS_DATA_DYNAMICPAINT;
  pid->info_types = 0;

  pid->stack_index = pid->cache->index;

  pid->default_step = 1;
  pid->max_step = 1;
  pid->file_type = PTCACHE_FILE_PTCACHE;
}

void BKE_ptcache_id_from_rigidbody(PTCacheID *pid, Object *ob, RigidBodyWorld *rbw)
{

  memset(pid, 0, sizeof(PTCacheID));

  pid->owner_id = ob != nullptr ? &ob->id : nullptr;
  pid->calldata = rbw;
  pid->type = PTCACHE_TYPE_RIGIDBODY;
  pid->cache = rbw->shared->pointcache;
  pid->cache_ptr = &rbw->shared->pointcache;
  pid->ptcaches = &rbw->shared->ptcaches;
  pid->totpoint = pid->totwrite = ptcache_rigidbody_totpoint;
  pid->error = ptcache_rigidbody_error;

  pid->write_point = ptcache_rigidbody_write;
  pid->read_point = ptcache_rigidbody_read;
  pid->interpolate_point = ptcache_rigidbody_interpolate;

  pid->write_stream = nullptr;
  pid->read_stream = nullptr;

  pid->write_extra_data = nullptr;
  pid->read_extra_data = nullptr;
  pid->interpolate_extra_data = nullptr;

  pid->write_header = ptcache_basic_header_write;
  pid->read_header = ptcache_basic_header_read;

  pid->data_types = (1 << BPHYS_DATA_LOCATION) | (1 << BPHYS_DATA_ROTATION);
  pid->info_types = 0;

  pid->stack_index = pid->cache->index;

  pid->default_step = 1;
  pid->max_step = 1;
  pid->file_type = PTCACHE_FILE_PTCACHE;
}

PTCacheID BKE_ptcache_id_find(Object *ob, Scene *scene, PointCache *cache)
{
  PTCacheID result = {nullptr};

  ListBase pidlist;
  BKE_ptcache_ids_from_object(&pidlist, ob, scene, MAX_DUPLI_RECUR);

  LISTBASE_FOREACH (PTCacheID *, pid, &pidlist) {
    if (pid->cache == cache) {
      result = *pid;
      break;
    }
  }

  BLI_freelistN(&pidlist);

  return result;
}

/* Callback which is used by point cache foreach() family of functions.
 *
 * Receives ID of the point cache.
 *
 * NOTE: This ID is owned by foreach() routines and can not be used outside of
 * the foreach loop. This means that if one wants to store them those are to be
 * malloced and copied over.
 *
 * If the function returns false, then foreach() loop aborts.
 */

static bool foreach_object_particle_ptcache(Object *object, PointCacheIdFn callback)
{
  PTCacheID pid;
  for (ParticleSystem *psys = static_cast<ParticleSystem *>(object->particlesystem.first);
       psys != nullptr;
       psys = psys->next)
  {
    if (psys->part == nullptr) {
      continue;
    }
    /* Check to make sure point cache is actually used by the particles. */
    if (ELEM(psys->part->phystype, PART_PHYS_NO, PART_PHYS_KEYED)) {
      continue;
    }
/* Hair needs to be included in id-list for cache edit mode to work. */
#if 0
    if ((psys->part->type == PART_HAIR) && (psys->flag & PSYS_HAIR_DYNAMICS) == 0) {
      continue;
    }
#endif
    if (psys->part->type == PART_FLUID) {
      continue;
    }
    BKE_ptcache_id_from_particles(&pid, object, psys);
    /* TODO figure out the particle modifier for this system and put it here. */
    if (!callback(pid, nullptr)) {
      return false;
    }
  }
  return true;
}

static bool foreach_object_modifier_ptcache(Object *object, PointCacheIdFn callback)
{
  PTCacheID pid;
  for (ModifierData *md = static_cast<ModifierData *>(object->modifiers.first); md != nullptr;
       md = md->next)
  {
    if (md->type == eModifierType_Cloth) {
      BKE_ptcache_id_from_cloth(&pid, object, (ClothModifierData *)md);
      if (!callback(pid, md)) {
        return false;
      }
    }
    else if (md->type == eModifierType_Fluid) {
      FluidModifierData *fmd = (FluidModifierData *)md;
      if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
        BKE_ptcache_id_from_smoke(&pid, object, (FluidModifierData *)md);
        if (!callback(pid, md)) {
          return false;
        }
      }
    }
    else if (md->type == eModifierType_DynamicPaint) {
      DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
      if (pmd->canvas) {
        DynamicPaintSurface *surface = static_cast<DynamicPaintSurface *>(
            pmd->canvas->surfaces.first);
        for (; surface; surface = surface->next) {
          BKE_ptcache_id_from_dynamicpaint(&pid, object, surface);
          if (!callback(pid, md)) {
            return false;
          }
        }
      }
    }
  }
  return true;
}

/* Return false if any of callbacks returned false. */
static bool foreach_object_ptcache(Scene *scene,
                                   Object *object,
                                   int duplis,
                                   PointCacheIdFn callback)
{
  PTCacheID pid;

  if (object != nullptr) {
    /* Soft body. */
    if (object->soft != nullptr) {
      BKE_ptcache_id_from_softbody(&pid, object, object->soft);
      if (!callback(pid, nullptr)) {
        return false;
      }
    }
    /* Particle systems. */
    if (!foreach_object_particle_ptcache(object, callback)) {
      return false;
    }
    /* Modifiers. */
    if (!foreach_object_modifier_ptcache(object, callback)) {
      return false;
    }
    /* Consider all object in dupli-groups to be part of the same object,
     * for baking with linking dupli-groups. Once we have better overrides
     * this can be revisited so users select the local objects directly. */
    if (scene != nullptr && (duplis-- > 0) && (object->instance_collection != nullptr)) {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (object->instance_collection, current_object) {
        if (current_object == object) {
          continue;
        }
        foreach_object_ptcache(scene, current_object, duplis, callback);
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }
  }

  /* Rigid body. */
  if (scene != nullptr && (object == nullptr || object->rigidbody_object != nullptr) &&
      scene->rigidbody_world != nullptr)
  {
    BKE_ptcache_id_from_rigidbody(&pid, object, scene->rigidbody_world);
    if (!callback(pid, nullptr)) {
      return false;
    }
  }
  return true;
}

void BKE_ptcache_ids_from_object(ListBase *lb, Object *ob, Scene *scene, int duplis)
{
  lb->first = lb->last = nullptr;
  foreach_object_ptcache(scene, ob, duplis, [&](PTCacheID &pid, ModifierData * /*md*/) -> bool {
    PTCacheID *own_pid = MEM_mallocN<PTCacheID>("PTCacheID");
    *own_pid = pid;
    BLI_addtail(lb, own_pid);
    return true;
  });
}

void BKE_ptcache_foreach_object_cache(Object &ob, Scene &scene, bool duplis, PointCacheIdFn fn)
{
  foreach_object_ptcache(&scene, &ob, duplis, fn);
}

bool BKE_ptcache_object_has(Scene *scene, Object *ob, int duplis)
{
  bool has_point_cache = false;
  foreach_object_ptcache(scene, ob, duplis, [&](PTCacheID & /*pid*/, ModifierData * /*md*/) {
    has_point_cache = true;
    return false;
  });
  return has_point_cache;
}

/* File handling */

static const char *ptcache_file_extension(const PTCacheID *pid)
{
  switch (pid->file_type) {
    default:
    case PTCACHE_FILE_PTCACHE:
      return PTCACHE_EXT;
  }
}

/**
 * Similar to #BLI_path_frame_get, but takes into account the stack-index which is after the frame.
 */
static int ptcache_frame_from_filename(const char *filename, const char *ext)
{
  const int frame_len = 6;
  const int ext_len = frame_len + strlen(ext);
  const int len = strlen(filename);

  /* could crash if trying to copy a string out of this range */
  if (len > ext_len) {
    /* Using frame_len here gives compile error (VLA). */
    char num[/*frame_len*/ 6 + 1];
    STRNCPY(num, filename + len - ext_len);

    return atoi(num);
  }

  return -1;
}

/* Takes an Object ID and returns a unique name
 * - id: object id
 * - cfra: frame for the cache, can be negative
 * - stack_index: index in the modifier stack. we can have cache for more than one stack_index
 */

#define MAX_PTCACHE_PATH FILE_MAX
#define MAX_PTCACHE_FILE (FILE_MAX * 2)

static int ptcache_path(PTCacheID *pid, char dirname[MAX_PTCACHE_PATH])
{
  const char *blendfile_path = BKE_main_blendfile_path_from_global();
  Library *lib = (pid->owner_id) ? pid->owner_id->lib : nullptr;
  const char *blendfile_path_lib = (lib && (pid->cache->flag & PTCACHE_IGNORE_LIBPATH) == 0) ?
                                       lib->runtime->filepath_abs :
                                       blendfile_path;

  if (pid->cache->flag & PTCACHE_EXTERNAL) {
    BLI_strncpy(dirname, pid->cache->path, MAX_PTCACHE_PATH);

    if (BLI_path_is_rel(dirname)) {
      BLI_path_abs(dirname, blendfile_path_lib);
    }

    return BLI_path_slash_ensure(dirname, MAX_PTCACHE_PATH); /* new strlen() */
  }
  if ((blendfile_path[0] != '\0') || lib) {
    char file[MAX_PTCACHE_PATH]; /* we don't want the dir, only the file */

    BLI_path_split_file_part(blendfile_path_lib, file, sizeof(file));
    /* Remove the `.blend` extension. */
    BLI_path_extension_strip(file);

    /* Add blend file name to pointcache dir. */
    BLI_snprintf(dirname, MAX_PTCACHE_PATH, "//" PTCACHE_PATH "%s", file);

    BLI_path_abs(dirname, blendfile_path_lib);
    return BLI_path_slash_ensure(dirname, MAX_PTCACHE_PATH); /* new strlen() */
  }

  /* use the temp path. this is weak but better than not using point cache at all */
  /* temporary directory is assumed to exist and ALWAYS has a trailing slash */
  BLI_snprintf(dirname, MAX_PTCACHE_PATH, "%s" PTCACHE_PATH, BKE_tempdir_session());

  return BLI_path_slash_ensure(dirname, MAX_PTCACHE_PATH); /* new strlen() */
}

static size_t ptcache_filepath_ext_append(PTCacheID *pid,
                                          char filepath[MAX_PTCACHE_FILE],
                                          const size_t filepath_len,
                                          const bool use_frame_number,
                                          const int cfra)
{
  size_t len = filepath_len;
  char *filename_ext;
  filename_ext = filepath + filepath_len;
  *filename_ext = '\0';

  /* PointCaches are inserted in object's list on demand, we need a valid index now. */
  if (pid->cache->index < 0) {
    BLI_assert(GS(pid->owner_id->name) == ID_OB);
    pid->cache->index = pid->stack_index = BKE_object_insert_ptcache((Object *)pid->owner_id);
  }

  const char *ext = ptcache_file_extension(pid);
  if (use_frame_number) {
    if (pid->cache->flag & PTCACHE_EXTERNAL) {
      if (pid->cache->index >= 0) {
        len += BLI_snprintf_rlen(
            filename_ext, MAX_PTCACHE_FILE - len, "_%06d_%02u%s", cfra, pid->stack_index, ext);
      }
      else {
        len += BLI_snprintf_rlen(filename_ext, MAX_PTCACHE_FILE - len, "_%06d%s", cfra, ext);
      }
    }
    else {
      len += BLI_snprintf_rlen(
          filename_ext, MAX_PTCACHE_FILE - len, "_%06d_%02u%s", cfra, pid->stack_index, ext);
    }
  }
  else {
    if (pid->cache->flag & PTCACHE_EXTERNAL) {
      if (pid->cache->index >= 0) {
        len += BLI_snprintf_rlen(
            filename_ext, MAX_PTCACHE_FILE - len, "_%02u%s", pid->stack_index, ext);
      }
      else {
        len += BLI_strncpy_rlen(filename_ext, ext, MAX_PTCACHE_FILE - len);
      }
    }
    else {
      len += BLI_snprintf_rlen(
          filename_ext, MAX_PTCACHE_FILE - len, "_%02u%s", pid->stack_index, ext);
    }
  }

  return len;
}

static int ptcache_filepath(PTCacheID *pid,
                            char filepath[MAX_PTCACHE_FILE],
                            int cfra,
                            const bool do_path,
                            const bool do_ext)
{
  int len = 0;
  const char *idname;
  char *newname;
  filepath[0] = '\0';
  newname = filepath;

  if ((pid->cache->flag & PTCACHE_EXTERNAL) == 0) {
    const char *blendfile_path = BKE_main_blendfile_path_from_global();
    if (blendfile_path[0] == '\0') {
      return 0; /* save blend file before using disk pointcache */
    }
  }

  /* start with temp dir */
  if (do_path) {
    len = ptcache_path(pid, filepath);
    newname += len;
  }
  if (pid->cache->name[0] == '\0' && (pid->cache->flag & PTCACHE_EXTERNAL) == 0) {
    idname = (pid->owner_id->name + 2);
    /* convert chars to hex so they are always a valid filename */
    while ('\0' != *idname) {
      /* Always 2 unless there isn't enough room in the string. */
      const int temp = BLI_snprintf_rlen(newname, MAX_PTCACHE_FILE - len, "%02X", uint(*idname++));
      newname += temp;
      len += temp;
    }
  }
  else {
    int temp = BLI_strncpy_rlen(newname, pid->cache->name, MAX_PTCACHE_FILE - len);
    newname += temp;
    len += temp;
  }
  UNUSED_VARS(newname);

  if (do_ext) {
    len += ptcache_filepath_ext_append(pid, filepath, size_t(len), true, cfra);
  }

  return len; /* make sure the above string is always 16 chars */
}

/**
 * Caller must close after!
 */
static PTCacheFile *ptcache_file_open(PTCacheID *pid, int mode, int cfra)
{
  PTCacheFile *pf;
  FILE *fp = nullptr;
  char filepath[MAX_PTCACHE_FILE];

#ifndef DURIAN_POINTCACHE_LIB_OK
  /* don't allow writing for linked objects */
  if (pid->owner_id->lib && mode == PTCACHE_FILE_WRITE) {
    return nullptr;
  }
#endif
  if ((pid->cache->flag & PTCACHE_EXTERNAL) == 0) {
    const char *blendfile_path = BKE_main_blendfile_path_from_global();
    if (blendfile_path[0] == '\0') {
      return nullptr; /* save blend file before using disk pointcache */
    }
  }

  ptcache_filepath(pid, filepath, cfra, true, true);

  if (mode == PTCACHE_FILE_READ) {
    fp = BLI_fopen(filepath, "rb");
  }
  else if (mode == PTCACHE_FILE_WRITE) {
    BLI_file_ensure_parent_dir_exists(filepath);
    fp = BLI_fopen(filepath, "wb");
  }
  else if (mode == PTCACHE_FILE_UPDATE) {
    BLI_file_ensure_parent_dir_exists(filepath);
    fp = BLI_fopen(filepath, "rb+");
  }

  if (!fp) {
    return nullptr;
  }

  pf = MEM_mallocN<PTCacheFile>("PTCacheFile");
  pf->fp = fp;
  pf->old_format = 0;
  pf->frame = cfra;

  return pf;
}
static void ptcache_file_close(PTCacheFile *pf)
{
  if (pf) {
    fclose(pf->fp);
    MEM_freeN(pf);
  }
}

static int ptcache_file_compressed_read(PTCacheFile *pf,
                                        uchar *result,
                                        uint items_num,
                                        uint item_size)
{
  int r = 0;
  size_t in_len;

  uchar compressed_val = 0;
  ptcache_file_read(pf, &compressed_val, 1, sizeof(uchar));
  const PointCacheCompression compressed = PointCacheCompression(compressed_val);
  if (compressed != PTCACHE_COMPRESS_NO) {
    uint size;
    ptcache_file_read(pf, &size, 1, sizeof(uint));
    in_len = size_t(size);
    if (in_len == 0) {
      /* do nothing */
    }
    else {
      uchar *in = MEM_calloc_arrayN<uchar>(in_len, "pointcache_compressed_buffer");
      ptcache_file_read(pf, in, in_len, sizeof(uchar));

      uchar *decomp_result = result;
      if (compressed == PTCACHE_COMPRESS_ZSTD_FILTERED) {
        decomp_result = MEM_malloc_arrayN<uchar>(items_num * item_size,
                                                 "pointcache_unfilter_buffer");
      }
      if (ELEM(compressed,
               PTCACHE_COMPRESS_ZSTD_FILTERED,
               PTCACHE_COMPRESS_ZSTD_FAST_DEPRECATED,
               PTCACHE_COMPRESS_ZSTD_SLOW_DEPRECATED))
      {
        const size_t err = ZSTD_decompress(decomp_result, items_num * item_size, in, in_len);
        r = ZSTD_isError(err);
      }
      else {
        /* We are trying to read an unsupported compression format. */
        r = 1;
      }
      MEM_freeN(in);

      /* Un-filter the decompressed data, if needed. */
      if (compressed == PTCACHE_COMPRESS_ZSTD_FILTERED) {
        blender::unfilter_transpose_delta(decomp_result, result, items_num, item_size);
        MEM_freeN(decomp_result);
      }
    }
  }
  else {
    ptcache_file_read(pf, result, items_num * item_size, sizeof(uchar));
  }

  return r;
}

static void ptcache_file_compressed_write(PTCacheFile *pf,
                                          const void *data,
                                          uint items_num,
                                          uint item_size)
{
  /* Allocate space for compressed data. */
  const PointCacheCompression compression = PTCACHE_COMPRESS_ZSTD_FILTERED;
  const uint data_size = items_num * item_size;
  size_t out_len = ZSTD_compressBound(data_size);
  blender::Array<uchar> out(out_len);

  /* Filter the data: transpose by bytes; delta-encode. */
  blender::Array<uchar> filtered(data_size);
  blender::filter_transpose_delta((const uint8_t *)data, filtered.data(), items_num, item_size);

  /* Do compression: always zstd level 3. */
  const int zstd_level = 3;
  size_t res = ZSTD_compress(out.data(), out_len, filtered.data(), data_size, zstd_level);
  out_len = res;

  /* Write to file. */
  const uchar compression_val = compression;
  ptcache_file_write(pf, &compression_val, 1, sizeof(uchar));
  uint size = out_len;
  ptcache_file_write(pf, &size, 1, sizeof(uint));
  ptcache_file_write(pf, out.data(), out_len, sizeof(uchar));
}

static bool ptcache_file_read(PTCacheFile *pf, void *f, uint items_num, uint item_size)
{
  return (fread(f, item_size, items_num, pf->fp) == items_num);
}
static int ptcache_file_write(PTCacheFile *pf, const void *data, uint items_num, uint item_size)
{
  return (fwrite(data, item_size, items_num, pf->fp) == items_num);
}
static bool ptcache_file_data_read(PTCacheFile *pf)
{
  int i;

  for (i = 0; i < BPHYS_TOT_DATA; i++) {
    if ((pf->data_types & (1 << i)) && !ptcache_file_read(pf, pf->cur[i], 1, ptcache_data_size[i]))
    {
      return false;
    }
  }

  return true;
}
static int ptcache_file_header_begin_read(PTCacheFile *pf)
{
  uint typeflag = 0;
  int error = 0;
  char bphysics[8];

  pf->data_types = 0;

  if (fread(bphysics, sizeof(char), 8, pf->fp) != 8) {
    error = 1;
  }

  if (!error && !STREQLEN(bphysics, "BPHYSICS", 8)) {
    error = 1;
  }

  if (!error && !fread(&typeflag, sizeof(uint), 1, pf->fp)) {
    error = 1;
  }

  pf->type = (typeflag & PTCACHE_TYPEFLAG_TYPEMASK);
  pf->flag = (typeflag & PTCACHE_TYPEFLAG_FLAGMASK);

  /* if there was an error set file as it was */
  if (error) {
    BLI_fseek(pf->fp, 0, SEEK_SET);
  }

  return !error;
}
static int ptcache_file_header_begin_write(PTCacheFile *pf)
{
  const char *bphysics = "BPHYSICS";
  uint typeflag = pf->type + pf->flag;

  if (fwrite(bphysics, sizeof(char), 8, pf->fp) != 8) {
    return 0;
  }

  if (!fwrite(&typeflag, sizeof(uint), 1, pf->fp)) {
    return 0;
  }

  return 1;
}

/* Data pointer handling. */

int BKE_ptcache_data_size(int data_type)
{
  return ptcache_data_size[data_type];
}

static void ptcache_file_pointers_init(PTCacheFile *pf)
{
  int data_types = pf->data_types;

  pf->cur[BPHYS_DATA_INDEX] = (data_types & (1 << BPHYS_DATA_INDEX)) ? &pf->data.index : nullptr;
  pf->cur[BPHYS_DATA_LOCATION] = (data_types & (1 << BPHYS_DATA_LOCATION)) ? &pf->data.loc :
                                                                             nullptr;
  pf->cur[BPHYS_DATA_VELOCITY] = (data_types & (1 << BPHYS_DATA_VELOCITY)) ? &pf->data.vel :
                                                                             nullptr;
  pf->cur[BPHYS_DATA_ROTATION] = (data_types & (1 << BPHYS_DATA_ROTATION)) ? &pf->data.rot :
                                                                             nullptr;
  pf->cur[BPHYS_DATA_AVELOCITY] = (data_types & (1 << BPHYS_DATA_AVELOCITY)) ? &pf->data.ave :
                                                                               nullptr;
  pf->cur[BPHYS_DATA_SIZE] = (data_types & (1 << BPHYS_DATA_SIZE)) ? &pf->data.size : nullptr;
  pf->cur[BPHYS_DATA_TIMES] = (data_types & (1 << BPHYS_DATA_TIMES)) ? &pf->data.times : nullptr;
  pf->cur[BPHYS_DATA_BOIDS] = (data_types & (1 << BPHYS_DATA_BOIDS)) ? &pf->data.boids : nullptr;
}

int BKE_ptcache_mem_index_find(PTCacheMem *pm, uint index)
{
  if (pm->totpoint > 0 && pm->data[BPHYS_DATA_INDEX]) {
    uint *data = static_cast<uint *>(pm->data[BPHYS_DATA_INDEX]);
    uint mid, low = 0, high = pm->totpoint - 1;

    if (index < *data || index > *(data + high)) {
      return -1;
    }

    /* check simple case for continuous indexes first */
    if (index - *data < high && data[index - *data] == index) {
      return index - *data;
    }

    while (low <= high) {
      mid = (low + high) / 2;

      if (data[mid] > index) {
        high = mid - 1;
      }
      else if (data[mid] < index) {
        low = mid + 1;
      }
      else {
        return mid;
      }
    }

    return -1;
  }

  return (index < pm->totpoint ? index : -1);
}

void BKE_ptcache_mem_pointers_init(PTCacheMem *pm, void *cur[BPHYS_TOT_DATA])
{
  int data_types = pm->data_types;
  int i;

  for (i = 0; i < BPHYS_TOT_DATA; i++) {
    cur[i] = ((data_types & (1 << i)) ? pm->data[i] : nullptr);
  }
}

void BKE_ptcache_mem_pointers_incr(void *cur[BPHYS_TOT_DATA])
{
  int i;

  for (i = 0; i < BPHYS_TOT_DATA; i++) {
    if (cur[i]) {
      cur[i] = (char *)cur[i] + ptcache_data_size[i];
    }
  }
}
int BKE_ptcache_mem_pointers_seek(int point_index, PTCacheMem *pm, void *cur[BPHYS_TOT_DATA])
{
  int data_types = pm->data_types;
  int i, index = BKE_ptcache_mem_index_find(pm, point_index);

  if (index < 0) {
    /* Can't give proper location without reallocation, so don't give any location.
     * Some points will be cached improperly, but this only happens with simulation
     * steps bigger than cache->step, so the cache has to be recalculated anyways
     * at some point.
     */
    return 0;
  }

  for (i = 0; i < BPHYS_TOT_DATA; i++) {
    cur[i] = (data_types & (1 << i)) ? (char *)pm->data[i] + index * ptcache_data_size[i] :
                                       nullptr;
  }

  return 1;
}
static void ptcache_data_alloc(PTCacheMem *pm)
{
  int data_types = pm->data_types;
  int totpoint = pm->totpoint;
  int i;

  for (i = 0; i < BPHYS_TOT_DATA; i++) {
    if (data_types & (1 << i)) {
      pm->data[i] = MEM_callocN(totpoint * ptcache_data_size[i], "PTCache Data");
    }
  }
}
static void ptcache_data_free(PTCacheMem *pm)
{
  void **data = pm->data;
  int i;

  for (i = 0; i < BPHYS_TOT_DATA; i++) {
    if (data[i]) {
      MEM_freeN(data[i]);
    }
  }
}
static void ptcache_data_copy(void *from[], void *to[])
{
  int i;
  for (i = 0; i < BPHYS_TOT_DATA; i++) {
    /* NOTE: Durian file `03.4b_comp` crashes if `to[i]` is not tested
     * its null, not sure if this should be fixed elsewhere but for now its needed. */
    if (from[i] && to[i]) {
      memcpy(to[i], from[i], ptcache_data_size[i]);
    }
  }
}

static void ptcache_extra_free(PTCacheMem *pm)
{
  PTCacheExtra *extra = static_cast<PTCacheExtra *>(pm->extradata.first);

  if (extra) {
    for (; extra; extra = extra->next) {
      if (extra->data) {
        MEM_freeN(extra->data);
      }
    }

    BLI_freelistN(&pm->extradata);
  }
}

static void ptcache_mem_clear(PTCacheMem *pm)
{
  ptcache_data_free(pm);
  ptcache_extra_free(pm);
}

static int ptcache_old_elemsize(PTCacheID *pid)
{
  if (pid->type == PTCACHE_TYPE_SOFTBODY) {
    return sizeof(float[6]);
  }
  if (pid->type == PTCACHE_TYPE_PARTICLES) {
    return sizeof(ParticleKey);
  }
  if (pid->type == PTCACHE_TYPE_CLOTH) {
    return sizeof(float[9]);
  }

  return 0;
}

static void ptcache_find_frames_around(PTCacheID *pid, uint frame, int *r_fra1, int *r_fra2)
{
  if (pid->cache->flag & PTCACHE_DISK_CACHE) {
    int cfra1 = frame, cfra2 = frame + 1;

    while (cfra1 >= pid->cache->startframe && !BKE_ptcache_id_exist(pid, cfra1)) {
      cfra1--;
    }

    if (cfra1 < pid->cache->startframe) {
      cfra1 = 0;
    }

    while (cfra2 <= pid->cache->endframe && !BKE_ptcache_id_exist(pid, cfra2)) {
      cfra2++;
    }

    if (cfra2 > pid->cache->endframe) {
      cfra2 = 0;
    }

    if (cfra1 && !cfra2) {
      *r_fra1 = 0;
      *r_fra2 = cfra1;
    }
    else {
      *r_fra1 = cfra1;
      *r_fra2 = cfra2;
    }
  }
  else if (pid->cache->mem_cache.first) {
    PTCacheMem *pm = static_cast<PTCacheMem *>(pid->cache->mem_cache.first);
    PTCacheMem *pm2 = static_cast<PTCacheMem *>(pid->cache->mem_cache.last);

    while (pm->next && pm->next->frame <= frame) {
      pm = pm->next;
    }

    if (pm2->frame < frame) {
      pm2 = nullptr;
    }
    else {
      while (pm2->prev && pm2->prev->frame > frame) {
        pm2 = pm2->prev;
      }
    }

    if (!pm2) {
      *r_fra1 = 0;
      *r_fra2 = pm->frame;
    }
    else {
      *r_fra1 = pm->frame;
      *r_fra2 = pm2->frame;
    }
  }
}

static PTCacheMem *ptcache_disk_frame_to_mem(PTCacheID *pid, int cfra)
{
  PTCacheFile *pf = ptcache_file_open(pid, PTCACHE_FILE_READ, cfra);
  PTCacheMem *pm = nullptr;
  uint i, error = 0;

  if (pf == nullptr) {
    return nullptr;
  }

  if (!ptcache_file_header_begin_read(pf)) {
    error = 1;
  }

  if (!error && (pf->type != pid->type || !pid->read_header(pf))) {
    error = 1;
  }

  if (!error) {
    pm = MEM_callocN<PTCacheMem>("Pointcache mem");

    pm->totpoint = pf->totpoint;
    pm->data_types = pf->data_types;
    pm->frame = pf->frame;

    ptcache_data_alloc(pm);

    if (pf->flag & PTCACHE_TYPEFLAG_COMPRESS) {
      for (i = 0; !error && i < BPHYS_TOT_DATA; i++) {
        if (pf->data_types & (1 << i)) {
          error = ptcache_file_compressed_read(
              pf, static_cast<uchar *>(pm->data[i]), pm->totpoint, ptcache_data_size[i]);
        }
      }
    }
    else {
      void *cur[BPHYS_TOT_DATA];
      BKE_ptcache_mem_pointers_init(pm, cur);
      ptcache_file_pointers_init(pf);

      for (i = 0; i < pm->totpoint; i++) {
        if (!ptcache_file_data_read(pf)) {
          error = 1;
          break;
        }
        ptcache_data_copy(pf->cur, cur);
        BKE_ptcache_mem_pointers_incr(cur);
      }
    }
  }

  if (!error && pf->flag & PTCACHE_TYPEFLAG_EXTRADATA) {
    uint extratype = 0;

    while (!error && ptcache_file_read(pf, &extratype, 1, sizeof(uint))) {
      PTCacheExtra *extra = MEM_callocN<PTCacheExtra>("Pointcache extradata");

      extra->type = extratype;

      ptcache_file_read(pf, &extra->totdata, 1, sizeof(uint));

      extra->data = MEM_callocN(extra->totdata * ptcache_extra_datasize[extra->type],
                                "Pointcache extradata->data");

      if (pf->flag & PTCACHE_TYPEFLAG_COMPRESS) {
        error = ptcache_file_compressed_read(pf,
                                             static_cast<uchar *>(extra->data),
                                             extra->totdata,
                                             ptcache_extra_datasize[extra->type]);
      }
      else {
        ptcache_file_read(pf, extra->data, extra->totdata, ptcache_extra_datasize[extra->type]);
      }

      BLI_addtail(&pm->extradata, extra);
    }
  }

  if (error && pm) {
    ptcache_mem_clear(pm);
    MEM_freeN(pm);
    pm = nullptr;
  }

  ptcache_file_close(pf);

  if (error && G.debug & G_DEBUG) {
    printf("Error reading from disk cache\n");
  }

  return pm;
}
static int ptcache_mem_frame_to_disk(PTCacheID *pid, PTCacheMem *pm)
{
  PTCacheFile *pf = nullptr;
  uint i, error = 0;

  BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_FRAME, pm->frame);

  pf = ptcache_file_open(pid, PTCACHE_FILE_WRITE, pm->frame);

  if (pf == nullptr) {
    if (G.debug & G_DEBUG) {
      printf("Error opening disk cache file for writing\n");
    }
    return 0;
  }

  pf->data_types = pm->data_types;
  pf->totpoint = pm->totpoint;
  pf->type = pid->type;
  pf->flag = 0;

  if (pm->extradata.first) {
    pf->flag |= PTCACHE_TYPEFLAG_EXTRADATA;
  }

  pf->flag |= PTCACHE_TYPEFLAG_COMPRESS;

  if (!ptcache_file_header_begin_write(pf) || !pid->write_header(pf)) {
    error = 1;
  }

  if (!error) {
    for (i = 0; i < BPHYS_TOT_DATA; i++) {
      if (pm->data[i]) {
        ptcache_file_compressed_write(pf, pm->data[i], pm->totpoint, ptcache_data_size[i]);
      }
    }
  }

  if (!error && pm->extradata.first) {
    PTCacheExtra *extra = static_cast<PTCacheExtra *>(pm->extradata.first);

    for (; extra; extra = extra->next) {
      if (extra->data == nullptr || extra->totdata == 0) {
        continue;
      }

      ptcache_file_write(pf, &extra->type, 1, sizeof(uint));
      ptcache_file_write(pf, &extra->totdata, 1, sizeof(uint));

      ptcache_file_compressed_write(
          pf, extra->data, extra->totdata, ptcache_extra_datasize[extra->type]);
    }
  }

  ptcache_file_close(pf);

  if (error && G.debug & G_DEBUG) {
    printf("Error writing to disk cache\n");
  }

  return error == 0;
}

static int ptcache_read_stream(PTCacheID *pid, int cfra)
{
  PTCacheFile *pf = ptcache_file_open(pid, PTCACHE_FILE_READ, cfra);
  int error = 0;

  if (pid->read_stream == nullptr) {
    return 0;
  }

  if (pf == nullptr) {
    if (G.debug & G_DEBUG) {
      printf("Error opening disk cache file for reading\n");
    }
    return 0;
  }

  if (!ptcache_file_header_begin_read(pf)) {
    pid->error(pid->owner_id, pid->calldata, "Failed to read point cache file");
    error = 1;
  }
  else if (pf->type != pid->type) {
    pid->error(pid->owner_id, pid->calldata, "Point cache file has wrong type");
    error = 1;
  }
  else if (!pid->read_header(pf)) {
    pid->error(pid->owner_id, pid->calldata, "Failed to read point cache file header");
    error = 1;
  }
  else if (pf->totpoint != pid->totpoint(pid->calldata, cfra)) {
    pid->error(pid->owner_id, pid->calldata, "Number of points in cache does not match mesh");
    error = 1;
  }

  if (!error) {
    ptcache_file_pointers_init(pf);

    /* We have stream reading here. */
    if (!pid->read_stream(pf, pid->calldata)) {
      pid->error(pid->owner_id, pid->calldata, "Failed to read point cache file data");
      error = 1;
    }
  }

  ptcache_file_close(pf);

  return error == 0;
}

static int ptcache_read(PTCacheID *pid, int cfra)
{
  PTCacheMem *pm = nullptr;
  int i;
  int *index = &i;

  /* get a memory cache to read from */
  if (pid->cache->flag & PTCACHE_DISK_CACHE) {
    pm = ptcache_disk_frame_to_mem(pid, cfra);
  }
  else {
    pm = static_cast<PTCacheMem *>(pid->cache->mem_cache.first);

    while (pm && pm->frame != cfra) {
      pm = pm->next;
    }
  }

  /* read the cache */
  if (pm) {
    int totpoint = pm->totpoint;

    if ((pid->data_types & (1 << BPHYS_DATA_INDEX)) == 0) {
      int pid_totpoint = pid->totpoint(pid->calldata, cfra);

      if (totpoint != pid_totpoint) {
        pid->error(pid->owner_id, pid->calldata, "Number of points in cache does not match mesh");
        totpoint = std::min(totpoint, pid_totpoint);
      }
    }

    void *cur[BPHYS_TOT_DATA];
    BKE_ptcache_mem_pointers_init(pm, cur);

    for (i = 0; i < totpoint; i++) {
      if (pm->data_types & (1 << BPHYS_DATA_INDEX)) {
        index = static_cast<int *>(cur[BPHYS_DATA_INDEX]);
      }

      pid->read_point(*index, pid->calldata, cur, float(pm->frame), nullptr);

      BKE_ptcache_mem_pointers_incr(cur);
    }

    if (pid->read_extra_data && pm->extradata.first) {
      pid->read_extra_data(pid->calldata, pm, float(pm->frame));
    }

    /* clean up temporary memory cache */
    if (pid->cache->flag & PTCACHE_DISK_CACHE) {
      ptcache_mem_clear(pm);
      MEM_freeN(pm);
    }
  }

  return 1;
}
static int ptcache_interpolate(PTCacheID *pid, float cfra, int cfra1, int cfra2)
{
  PTCacheMem *pm = nullptr;
  int i;
  int *index = &i;

  /* get a memory cache to read from */
  if (pid->cache->flag & PTCACHE_DISK_CACHE) {
    pm = ptcache_disk_frame_to_mem(pid, cfra2);
  }
  else {
    pm = static_cast<PTCacheMem *>(pid->cache->mem_cache.first);

    while (pm && pm->frame != cfra2) {
      pm = pm->next;
    }
  }

  /* read the cache */
  if (pm) {
    int totpoint = pm->totpoint;

    if ((pid->data_types & (1 << BPHYS_DATA_INDEX)) == 0) {
      int pid_totpoint = pid->totpoint(pid->calldata, int(cfra));

      if (totpoint != pid_totpoint) {
        pid->error(pid->owner_id, pid->calldata, "Number of points in cache does not match mesh");
        totpoint = std::min(totpoint, pid_totpoint);
      }
    }

    void *cur[BPHYS_TOT_DATA];
    BKE_ptcache_mem_pointers_init(pm, cur);

    for (i = 0; i < totpoint; i++) {
      if (pm->data_types & (1 << BPHYS_DATA_INDEX)) {
        index = static_cast<int *>(cur[BPHYS_DATA_INDEX]);
      }

      pid->interpolate_point(
          *index, pid->calldata, cur, cfra, float(cfra1), float(cfra2), nullptr);
      BKE_ptcache_mem_pointers_incr(cur);
    }

    if (pid->interpolate_extra_data && pm->extradata.first) {
      pid->interpolate_extra_data(pid->calldata, pm, cfra, float(cfra1), float(cfra2));
    }

    /* clean up temporary memory cache */
    if (pid->cache->flag & PTCACHE_DISK_CACHE) {
      ptcache_mem_clear(pm);
      MEM_freeN(pm);
    }
  }

  return 1;
}

int BKE_ptcache_read(PTCacheID *pid, float cfra, bool no_extrapolate_old)
{
  int cfrai = int(floor(cfra)), cfra1 = 0, cfra2 = 0;
  int ret = 0;

  /* nothing to read to */
  if (pid->totpoint(pid->calldata, cfrai) == 0) {
    return 0;
  }

  if (pid->cache->flag & PTCACHE_READ_INFO) {
    pid->cache->flag &= ~PTCACHE_READ_INFO;
    ptcache_read(pid, 0);
  }

  /* first check if we have the actual frame cached */
  if (cfra == float(cfrai) && BKE_ptcache_id_exist(pid, cfrai)) {
    cfra1 = cfrai;
  }

  /* no exact cache frame found so try to find cached frames around cfra */
  if (cfra1 == 0) {
    ptcache_find_frames_around(pid, cfrai, &cfra1, &cfra2);
  }

  if (cfra1 == 0 && cfra2 == 0) {
    return 0;
  }

  /* don't read old cache if already simulated past cached frame */
  if (no_extrapolate_old) {
    if (cfra1 == 0 && cfra2 && cfra2 <= pid->cache->simframe) {
      return 0;
    }
    if (cfra1 && cfra1 == cfra2) {
      return 0;
    }
  }
  else {
    /* avoid calling interpolate between the same frame values */
    if (cfra1 && cfra1 == cfra2) {
      cfra1 = 0;
    }
  }

  if (cfra1) {
    if (pid->read_stream) {
      if (!ptcache_read_stream(pid, cfra1)) {
        return 0;
      }
    }
    else if (pid->read_point) {
      ptcache_read(pid, cfra1);
    }
  }

  if (cfra2) {
    if (pid->read_stream) {
      if (!ptcache_read_stream(pid, cfra2)) {
        return 0;
      }
    }
    else if (pid->read_point) {
      if (cfra1 && cfra2 && pid->interpolate_point) {
        ptcache_interpolate(pid, cfra, cfra1, cfra2);
      }
      else {
        ptcache_read(pid, cfra2);
      }
    }
  }

  if (cfra1) {
    ret = (cfra2 ? PTCACHE_READ_INTERPOLATED : PTCACHE_READ_EXACT);
  }
  else if (cfra2) {
    ret = PTCACHE_READ_OLD;
    pid->cache->simframe = cfra2;
  }

  cfrai = int(cfra);
  /* clear invalid cache frames so that better stuff can be simulated */
  if (pid->cache->flag & PTCACHE_OUTDATED) {
    BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, cfrai);
  }
  else if (pid->cache->flag & PTCACHE_FRAMES_SKIPPED) {
    if (cfra <= pid->cache->last_exact) {
      pid->cache->flag &= ~PTCACHE_FRAMES_SKIPPED;
    }

    BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, std::max(cfrai, pid->cache->last_exact));
  }

  return ret;
}
static int ptcache_write_stream(PTCacheID *pid, int cfra, int totpoint)
{
  PTCacheFile *pf = nullptr;
  int error = 0;

  BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_FRAME, cfra);

  pf = ptcache_file_open(pid, PTCACHE_FILE_WRITE, cfra);

  if (pf == nullptr) {
    if (G.debug & G_DEBUG) {
      printf("Error opening disk cache file for writing\n");
    }
    return 0;
  }

  pf->data_types = pid->data_types;
  pf->totpoint = totpoint;
  pf->type = pid->type;
  pf->flag = 0;

  if (!error && (!ptcache_file_header_begin_write(pf) || !pid->write_header(pf))) {
    error = 1;
  }

  if (!error && pid->write_stream) {
    pid->write_stream(pf, pid->calldata);
  }

  ptcache_file_close(pf);

  if (error && G.debug & G_DEBUG) {
    printf("Error writing to disk cache\n");
  }

  return error == 0;
}

static int ptcache_write(PTCacheID *pid, int cfra, int overwrite)
{
  PointCache *cache = pid->cache;
  PTCacheMem *pm = nullptr, *pm2 = nullptr;
  int totpoint = pid->totpoint(pid->calldata, cfra);
  int i, error = 0;

  pm = MEM_callocN<PTCacheMem>("Pointcache mem");

  pm->totpoint = pid->totwrite(pid->calldata, cfra);
  pm->data_types = cfra ? pid->data_types : pid->info_types;

  ptcache_data_alloc(pm);
  void *cur[BPHYS_TOT_DATA];
  BKE_ptcache_mem_pointers_init(pm, cur);

  if (overwrite) {
    if (cache->flag & PTCACHE_DISK_CACHE) {
      int fra = cfra - 1;

      while (fra >= cache->startframe && !BKE_ptcache_id_exist(pid, fra)) {
        fra--;
      }

      pm2 = ptcache_disk_frame_to_mem(pid, fra);
    }
    else {
      pm2 = static_cast<PTCacheMem *>(cache->mem_cache.last);
    }
  }

  if (pid->write_point) {
    for (i = 0; i < totpoint; i++) {
      int write = pid->write_point(i, pid->calldata, cur, cfra);
      if (write) {
        BKE_ptcache_mem_pointers_incr(cur);

        void *cur2[BPHYS_TOT_DATA];
        /* newly born particles have to be copied to previous cached frame */
        if (overwrite && write == 2 && pm2 && BKE_ptcache_mem_pointers_seek(i, pm2, cur2)) {
          pid->write_point(i, pid->calldata, cur2, cfra);
        }
      }
    }
  }

  if (pid->write_extra_data) {
    pid->write_extra_data(pid->calldata, pm, cfra);
  }

  pm->frame = cfra;

  if (cache->flag & PTCACHE_DISK_CACHE) {
    error += !ptcache_mem_frame_to_disk(pid, pm);

    // if (pm) /* pm is always set */
    {
      ptcache_mem_clear(pm);
      MEM_freeN(pm);
    }

    if (pm2) {
      error += !ptcache_mem_frame_to_disk(pid, pm2);
      ptcache_mem_clear(pm2);
      MEM_freeN(pm2);
    }
  }
  else {
    BLI_addtail(&cache->mem_cache, pm);
  }

  return error;
}
static int ptcache_write_needed(PTCacheID *pid, int cfra, int *overwrite)
{
  PointCache *cache = pid->cache;
  int ofra = 0, efra = cache->endframe;

  /* always start from scratch on the first frame */
  if (cfra && cfra == cache->startframe) {
    BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, cfra);
    cache->flag &= ~PTCACHE_REDO_NEEDED;
    return 1;
  }

  if (pid->cache->flag & PTCACHE_DISK_CACHE) {
    if (cfra == 0 && cache->startframe > 0) {
      return 1;
    }

    /* find last cached frame */
    while (efra > cache->startframe && !BKE_ptcache_id_exist(pid, efra)) {
      efra--;
    }

    /* find second last cached frame */
    ofra = efra - 1;
    while (ofra > cache->startframe && !BKE_ptcache_id_exist(pid, ofra)) {
      ofra--;
    }
  }
  else {
    PTCacheMem *pm = static_cast<PTCacheMem *>(cache->mem_cache.last);
    /* don't write info file in memory */
    if (cfra == 0) {
      return 0;
    }

    if (pm == nullptr) {
      return 1;
    }

    efra = pm->frame;
    ofra = (pm->prev ? pm->prev->frame : efra - cache->step);
  }

  if (efra >= cache->startframe && cfra > efra) {
    if (ofra >= cache->startframe && efra - ofra < cache->step) {
      /* overwrite previous frame */
      BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_FRAME, efra);
      *overwrite = 1;
    }
    return 1;
  }

  return 0;
}
int BKE_ptcache_write(PTCacheID *pid, uint cfra)
{
  PointCache *cache = pid->cache;
  if (!pid->totpoint) {
    /* This happens when `pid->type == PTCACHE_TYPE_SMOKE_DOMAIN`. The fluid system does not
     * actually use the pointcache anymore for caching. */
    return 0;
  }

  int totpoint = pid->totpoint(pid->calldata, cfra);
  int overwrite = 0, error = 0;

  if (totpoint == 0 || (cfra ? pid->data_types == 0 : pid->info_types == 0)) {
    return 0;
  }

  if (ptcache_write_needed(pid, cfra, &overwrite) == 0) {
    return 0;
  }

  if (pid->write_stream) {
    ptcache_write_stream(pid, cfra, totpoint);
  }
  else if (pid->write_point) {
    error += ptcache_write(pid, cfra, overwrite);
  }

  /* Mark frames skipped if more than 1 frame forwards since last non-skipped frame. */
  if (cfra - cache->last_exact == 1 || cfra == cache->startframe) {
    cache->last_exact = cfra;
    cache->flag &= ~PTCACHE_FRAMES_SKIPPED;
  }
  /* Don't mark skipped when writing info file (frame 0) */
  else if (cfra) {
    cache->flag |= PTCACHE_FRAMES_SKIPPED;
  }

  /* Update timeline cache display */
  if (cfra && cache->cached_frames) {
    cache->cached_frames[cfra - cache->startframe] = 1;
  }

  cache->flag |= PTCACHE_FLAG_INFO_DIRTY;

  return !error;
}
/* you'll need to close yourself after!
 * mode - PTCACHE_CLEAR_ALL,
 */

/* Clears & resets. */

void BKE_ptcache_id_clear(PTCacheID *pid, int mode, uint cfra)
{
  uint len; /* store the length of the string */
  uint sta, end;

  /* mode is same as fopen's modes */
  DIR *dir;
  dirent *de;
  char path[MAX_PTCACHE_PATH];
  char filepath[MAX_PTCACHE_FILE];
  char path_full[MAX_PTCACHE_FILE];
  char ext[MAX_PTCACHE_FILE];

  if (!pid || !pid->cache || pid->cache->flag & PTCACHE_BAKED) {
    return;
  }

  if (pid->cache->flag & PTCACHE_IGNORE_CLEAR) {
    return;
  }

  sta = pid->cache->startframe;
  end = pid->cache->endframe;

#ifndef DURIAN_POINTCACHE_LIB_OK
  /* don't allow clearing for linked objects */
  if (pid->owner_id->lib) {
    return;
  }
#endif

  /* Clear all files in the temp dir with the prefix of the ID and the `.bphys` suffix. */
  switch (mode) {
    case PTCACHE_CLEAR_ALL:
    case PTCACHE_CLEAR_BEFORE:
    case PTCACHE_CLEAR_AFTER:
      if (pid->cache->flag & PTCACHE_DISK_CACHE) {
        ptcache_path(pid, path);

        dir = opendir(path);
        if (dir == nullptr) {
          return;
        }

        len = ptcache_filepath(pid, filepath, cfra, false, false); /* no path */
        /* append underscore terminator to ensure we don't match similar names
         * from objects whose names start with the same prefix
         */
        if (len < sizeof(filepath) - 2) {
          BLI_strncpy(filepath + len, "_", sizeof(filepath) - 2 - len);
          len += 1;
        }

        ptcache_filepath_ext_append(pid, ext, 0, false, 0);

        while ((de = readdir(dir)) != nullptr) {
          if (strstr(de->d_name, ext)) {               /* Do we have the right extension? */
            if (STREQLEN(filepath, de->d_name, len)) { /* Do we have the right prefix. */
              if (mode == PTCACHE_CLEAR_ALL) {
                pid->cache->last_exact = std::min(pid->cache->startframe, 0);
                BLI_path_join(path_full, sizeof(path_full), path, de->d_name);
                BLI_delete(path_full, false, false);
              }
              else {
                /* read the number of the file */
                const int frame = ptcache_frame_from_filename(de->d_name, ext);

                if (frame != -1) {
                  if ((mode == PTCACHE_CLEAR_BEFORE && frame < cfra) ||
                      (mode == PTCACHE_CLEAR_AFTER && frame > cfra))
                  {
                    BLI_path_join(path_full, sizeof(path_full), path, de->d_name);
                    BLI_delete(path_full, false, false);
                    if (pid->cache->cached_frames && frame >= sta && frame <= end) {
                      pid->cache->cached_frames[frame - sta] = 0;
                    }
                  }
                }
              }
            }
          }
        }
        closedir(dir);

        if (mode == PTCACHE_CLEAR_ALL && pid->cache->cached_frames) {
          memset(pid->cache->cached_frames, 0, MEM_allocN_len(pid->cache->cached_frames));
        }
      }
      else {
        PTCacheMem *pm = static_cast<PTCacheMem *>(pid->cache->mem_cache.first);
        PTCacheMem *link = nullptr;

        if (mode == PTCACHE_CLEAR_ALL) {
          /* We want startframe if the cache starts before zero. */
          pid->cache->last_exact = std::min(pid->cache->startframe, 0);
          for (; pm; pm = pm->next) {
            ptcache_mem_clear(pm);
          }
          BLI_freelistN(&pid->cache->mem_cache);

          if (pid->cache->cached_frames) {
            memset(pid->cache->cached_frames, 0, MEM_allocN_len(pid->cache->cached_frames));
          }
        }
        else {
          while (pm) {
            if ((mode == PTCACHE_CLEAR_BEFORE && pm->frame < cfra) ||
                (mode == PTCACHE_CLEAR_AFTER && pm->frame > cfra))
            {
              link = pm;
              if (pid->cache->cached_frames && pm->frame >= sta && pm->frame <= end) {
                pid->cache->cached_frames[pm->frame - sta] = 0;
              }
              ptcache_mem_clear(pm);
              pm = pm->next;
              BLI_freelinkN(&pid->cache->mem_cache, link);
            }
            else {
              pm = pm->next;
            }
          }
        }
      }
      break;

    case PTCACHE_CLEAR_FRAME:
      if (pid->cache->flag & PTCACHE_DISK_CACHE) {
        if (BKE_ptcache_id_exist(pid, cfra)) {
          ptcache_filepath(pid, filepath, cfra, true, true); /* no path */
          BLI_delete(filepath, false, false);
        }
      }
      else {
        PTCacheMem *pm = static_cast<PTCacheMem *>(pid->cache->mem_cache.first);

        for (; pm; pm = pm->next) {
          if (pm->frame == cfra) {
            ptcache_mem_clear(pm);
            BLI_freelinkN(&pid->cache->mem_cache, pm);
            break;
          }
        }
      }
      if (pid->cache->cached_frames && cfra >= sta && cfra <= end) {
        pid->cache->cached_frames[cfra - sta] = 0;
      }
      break;
  }

  pid->cache->flag |= PTCACHE_FLAG_INFO_DIRTY;
}

bool BKE_ptcache_id_exist(PTCacheID *pid, int cfra)
{
  if (!pid->cache) {
    return false;
  }

  if (cfra < pid->cache->startframe || cfra > pid->cache->endframe) {
    return false;
  }

  if (pid->cache->cached_frames && pid->cache->cached_frames[cfra - pid->cache->startframe] == 0) {
    return false;
  }

  if (pid->cache->flag & PTCACHE_DISK_CACHE) {
    char filepath[MAX_PTCACHE_FILE];

    ptcache_filepath(pid, filepath, cfra, true, true);

    return BLI_exists(filepath);
  }

  PTCacheMem *pm = static_cast<PTCacheMem *>(pid->cache->mem_cache.first);

  for (; pm; pm = pm->next) {
    if (pm->frame == cfra) {
      return true;
    }
  }
  return false;
}
void BKE_ptcache_id_time(
    PTCacheID *pid, Scene *scene, float cfra, int *startframe, int *endframe, float *timescale)
{
  // Object *ob; /* UNUSED */
  PointCache *cache;
  /* float offset; unused for now */
  float time, nexttime;

  /* TODO: this has to be sorted out once bsystem_time gets redone, */
  /*       now caches can handle interpolating etc. too - jahka */

  /* time handling for point cache:
   * - simulation time is scaled by result of bsystem_time
   * - for offsetting time only time offset is taken into account, since
   *   that's always the same and can't be animated. a time-offset which
   *   varies over time is not simple to support.
   * - field and motion blur offsets are currently ignored, proper solution
   *   is probably to interpolate results from two frames for that ..
   */

  cache = pid->cache;

  if (timescale) {
    time = BKE_scene_ctime_get(scene);
    nexttime = BKE_scene_frame_to_ctime(scene, scene->r.cfra + 1);

    *timescale = std::max(nexttime - time, 0.0f);
  }

  if (startframe && endframe) {
    *startframe = cache->startframe;
    *endframe = cache->endframe;
  }

  /* verify cached_frames array is up to date */
  if (cache->cached_frames) {
    if (cache->cached_frames_len != (cache->endframe - cache->startframe + 1)) {
      MEM_freeN(cache->cached_frames);
      cache->cached_frames = nullptr;
      cache->cached_frames_len = 0;
    }
  }

  if (cache->cached_frames == nullptr && cache->endframe > cache->startframe) {
    uint sta = cache->startframe;
    uint end = cache->endframe;

    cache->cached_frames_len = cache->endframe - cache->startframe + 1;
    cache->cached_frames = MEM_calloc_arrayN<char>(cache->cached_frames_len,
                                                   "cached frames array");

    if (pid->cache->flag & PTCACHE_DISK_CACHE) {
      /* mode is same as fopen's modes */
      DIR *dir;
      dirent *de;
      char path[MAX_PTCACHE_PATH];
      char filepath[MAX_PTCACHE_FILE];
      char ext[MAX_PTCACHE_FILE];
      uint len; /* store the length of the string */

      ptcache_path(pid, path);

      len = ptcache_filepath(pid, filepath, int(cfra), false, false); /* no path */

      dir = opendir(path);
      if (dir == nullptr) {
        return;
      }

      ptcache_filepath_ext_append(pid, ext, 0, false, 0);

      while ((de = readdir(dir)) != nullptr) {
        if (strstr(de->d_name, ext)) {               /* Do we have the right extension? */
          if (STREQLEN(filepath, de->d_name, len)) { /* Do we have the right prefix. */
            /* read the number of the file */
            const int frame = ptcache_frame_from_filename(de->d_name, ext);

            if ((frame != -1) && (frame >= sta && frame <= end)) {
              cache->cached_frames[frame - sta] = 1;
            }
          }
        }
      }
      closedir(dir);
    }
    else {
      PTCacheMem *pm = static_cast<PTCacheMem *>(pid->cache->mem_cache.first);

      while (pm) {
        if (pm->frame >= sta && pm->frame <= end) {
          cache->cached_frames[pm->frame - sta] = 1;
        }
        pm = pm->next;
      }
    }
  }
}
int BKE_ptcache_id_reset(Scene *scene, PTCacheID *pid, int mode)
{
  PointCache *cache;
  int reset, clear, after;

  if (!pid->cache) {
    return 0;
  }

  cache = pid->cache;
  reset = 0;
  clear = 0;
  after = 0;

  if (mode == PTCACHE_RESET_DEPSGRAPH) {
    if (!(cache->flag & PTCACHE_BAKED)) {

      after = 1;
    }

    cache->flag |= PTCACHE_OUTDATED;
  }
  else if (mode == PTCACHE_RESET_BAKED) {
    cache->flag |= PTCACHE_OUTDATED;
  }
  else if (mode == PTCACHE_RESET_OUTDATED) {
    reset = 1;

    if (cache->flag & PTCACHE_OUTDATED && !(cache->flag & PTCACHE_BAKED)) {
      clear = 1;
      cache->flag &= ~PTCACHE_OUTDATED;
    }
  }

  if (reset) {
    BKE_ptcache_invalidate(cache);
    cache->flag &= ~PTCACHE_REDO_NEEDED;

    if (pid->type == PTCACHE_TYPE_CLOTH) {
      cloth_free_modifier(static_cast<ClothModifierData *>(pid->calldata));
    }
    else if (pid->type == PTCACHE_TYPE_SOFTBODY) {
      sbFreeSimulation(static_cast<SoftBody *>(pid->calldata));
    }
    else if (pid->type == PTCACHE_TYPE_PARTICLES) {
      psys_reset(static_cast<ParticleSystem *>(pid->calldata), PSYS_RESET_DEPSGRAPH);
    }
    else if (pid->type == PTCACHE_TYPE_DYNAMICPAINT) {
      dynamicPaint_clearSurface(scene, (DynamicPaintSurface *)pid->calldata);
    }
  }
  if (clear) {
    BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
  }
  else if (after) {
    BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, scene->r.cfra);
  }

  return (reset || clear || after);
}
int BKE_ptcache_object_reset(Scene *scene, Object *ob, int mode)
{
  PTCacheID pid;
  int reset, skip;

  reset = 0;
  skip = 0;

  if (ob->soft) {
    BKE_ptcache_id_from_softbody(&pid, ob, ob->soft);
    reset |= BKE_ptcache_id_reset(scene, &pid, mode);
  }

  LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
    /* children or just redo can be calculated without resetting anything */
    if (psys->recalc & ID_RECALC_PSYS_REDO || psys->recalc & ID_RECALC_PSYS_CHILD) {
      skip = 1;
      /* Baked cloth hair has to be checked too, because we don't want to reset */
      /* particles or cloth in that case -jahka */
    }
    else if (psys->clmd) {
      BKE_ptcache_id_from_cloth(&pid, ob, psys->clmd);
      if (mode == PSYS_RESET_ALL ||
          !(psys->part->type == PART_HAIR && (pid.cache->flag & PTCACHE_BAKED)))
      {
        reset |= BKE_ptcache_id_reset(scene, &pid, mode);
      }
      else {
        skip = 1;
      }
    }

    if (skip == 0 && psys->part) {
      BKE_ptcache_id_from_particles(&pid, ob, psys);
      reset |= BKE_ptcache_id_reset(scene, &pid, mode);
    }
  }

  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type == eModifierType_Cloth) {
      BKE_ptcache_id_from_cloth(&pid, ob, (ClothModifierData *)md);
      reset |= BKE_ptcache_id_reset(scene, &pid, mode);
    }
    if (md->type == eModifierType_DynamicPaint) {
      DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
      if (pmd->canvas) {
        DynamicPaintSurface *surface = static_cast<DynamicPaintSurface *>(
            pmd->canvas->surfaces.first);

        for (; surface; surface = surface->next) {
          BKE_ptcache_id_from_dynamicpaint(&pid, ob, surface);
          reset |= BKE_ptcache_id_reset(scene, &pid, mode);
        }
      }
    }
    if (md->type == eModifierType_Fluid) {
      FluidModifierData *fmd = (FluidModifierData *)md;
      FluidDomainSettings *fds = fmd->domain;
      if ((fmd->type & MOD_FLUID_TYPE_DOMAIN) && fds &&
          fds->cache_type == FLUID_DOMAIN_CACHE_REPLAY)
      {
        BKE_ptcache_id_from_smoke(&pid, ob, fmd);
        reset |= BKE_ptcache_id_reset(scene, &pid, mode);
      }
    }
  }

  if (scene->rigidbody_world && (ob->rigidbody_object || ob->rigidbody_constraint)) {
    if (ob->rigidbody_object) {
      ob->rigidbody_object->flag |= RBO_FLAG_NEEDS_RESHAPE;
    }
    BKE_ptcache_id_from_rigidbody(&pid, ob, scene->rigidbody_world);
    /* only flag as outdated, resetting should happen on start frame */
    pid.cache->flag |= PTCACHE_OUTDATED;
  }

  if (ob->type == OB_ARMATURE) {
    BIK_clear_cache(ob->pose);
  }

  return reset;
}

/* Point Cache handling */

PointCache *BKE_ptcache_add(ListBase *ptcaches)
{
  PointCache *cache;

  cache = MEM_callocN<PointCache>("PointCache");
  cache->startframe = 1;
  cache->endframe = 250;
  cache->step = 1;
  cache->index = -1;

  BLI_addtail(ptcaches, cache);

  return cache;
}

void BKE_ptcache_free_mem(ListBase *mem_cache)
{
  PTCacheMem *pm = static_cast<PTCacheMem *>(mem_cache->first);

  if (pm) {
    for (; pm; pm = pm->next) {
      ptcache_mem_clear(pm);
    }

    BLI_freelistN(mem_cache);
  }
}
void BKE_ptcache_free(PointCache *cache)
{
  BKE_ptcache_free_mem(&cache->mem_cache);
  if (cache->edit && cache->free_edit) {
    cache->free_edit(cache->edit);
  }
  if (cache->cached_frames) {
    MEM_freeN(cache->cached_frames);
  }
  MEM_freeN(cache);
}
void BKE_ptcache_free_list(ListBase *ptcaches)
{
  while (PointCache *cache = static_cast<PointCache *>(BLI_pophead(ptcaches))) {
    BKE_ptcache_free(cache);
  }
}

static PointCache *ptcache_copy(PointCache *cache, const bool copy_data)
{
  PointCache *ncache;

  ncache = static_cast<PointCache *>(MEM_dupallocN(cache));

  BLI_listbase_clear(&ncache->mem_cache);

  if (copy_data == false) {
    ncache->cached_frames = nullptr;
    ncache->cached_frames_len = 0;

    /* flag is a mix of user settings and simulator/baking state */
    ncache->flag = ncache->flag & (PTCACHE_DISK_CACHE | PTCACHE_EXTERNAL | PTCACHE_IGNORE_LIBPATH);
    ncache->simframe = 0;
  }
  else {
    LISTBASE_FOREACH (PTCacheMem *, pm, &cache->mem_cache) {
      PTCacheMem *pmn = static_cast<PTCacheMem *>(MEM_dupallocN(pm));
      int i;

      for (i = 0; i < BPHYS_TOT_DATA; i++) {
        if (pmn->data[i]) {
          pmn->data[i] = MEM_dupallocN(pm->data[i]);
        }
      }

      BLI_addtail(&ncache->mem_cache, pmn);
    }

    if (ncache->cached_frames) {
      ncache->cached_frames = static_cast<char *>(MEM_dupallocN(cache->cached_frames));
    }
  }

  /* hmm, should these be copied over instead? */
  ncache->edit = nullptr;

  return ncache;
}

PointCache *BKE_ptcache_copy_list(ListBase *ptcaches_new,
                                  const ListBase *ptcaches_old,
                                  const int flag)
{
  PointCache *cache = static_cast<PointCache *>(ptcaches_old->first);

  BLI_listbase_clear(ptcaches_new);

  for (; cache; cache = cache->next) {
    BLI_addtail(ptcaches_new, ptcache_copy(cache, (flag & LIB_ID_COPY_CACHES) != 0));
  }

  return static_cast<PointCache *>(ptcaches_new->first);
}

/* Disabled this code; this is being called on scene_update_tagged, and that in turn gets called on
 * every user action changing stuff, and then it runs a complete bake??? (ton) */

/* Baking */

void BKE_ptcache_quick_cache_all(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  PTCacheBaker baker;

  memset(&baker, 0, sizeof(baker));
  baker.bmain = bmain;
  baker.scene = scene;
  baker.view_layer = view_layer;
  baker.bake = false;
  baker.render = false;
  baker.anim_init = false;
  baker.quick_step = scene->physics_settings.quick_cache_step;

  BKE_ptcache_bake(&baker);
}

static void ptcache_dt_to_str(char *str, size_t str_maxncpy, double dtime)
{
  if (dtime > 60.0) {
    if (dtime > 3600.0) {
      BLI_snprintf(str,
                   str_maxncpy,
                   "%ih %im %is",
                   int(dtime / 3600),
                   int(dtime / 60) % 60,
                   int(dtime) % 60);
    }
    else {
      BLI_snprintf(str, str_maxncpy, "%im %is", int(dtime / 60) % 60, int(dtime) % 60);
    }
  }
  else {
    BLI_snprintf(str, str_maxncpy, "%is", int(dtime) % 60);
  }
}

void BKE_ptcache_bake(PTCacheBaker *baker)
{
  Scene *scene = baker->scene;
  ViewLayer *view_layer = baker->view_layer;
  Depsgraph *depsgraph = baker->depsgraph;
  Scene *sce_iter; /* SETLOOPER macro only */
  Base *base;
  ListBase pidlist;
  PTCacheID *pid = &baker->pid;
  PointCache *cache = nullptr;
  float frameleno = scene->r.framelen;
  int cfrao = scene->r.cfra;
  int startframe = MAXFRAME, endframe = baker->anim_init ? scene->r.sfra : scene->r.cfra;
  int bake = baker->bake;
  int render = baker->render;

  G.is_break = false;

  /* set caches to baking mode and figure out start frame */
  if (pid->owner_id) {
    /* cache/bake a single object */
    cache = pid->cache;
    if ((cache->flag & PTCACHE_BAKED) == 0) {
      if (pid->type == PTCACHE_TYPE_PARTICLES) {
        ParticleSystem *psys = static_cast<ParticleSystem *>(pid->calldata);

        /* a bit confusing, could make this work better in the UI */
        if (psys->part->type == PART_EMITTER) {
          psys_get_pointcache_start_end(scene,
                                        static_cast<ParticleSystem *>(pid->calldata),
                                        &cache->startframe,
                                        &cache->endframe);
        }
      }
      else if (pid->type == PTCACHE_TYPE_SMOKE_HIGHRES) {
        /* get all pids from the object and search for smoke low res */
        ListBase pidlist2;
        BLI_assert(GS(pid->owner_id->name) == ID_OB);
        BKE_ptcache_ids_from_object(&pidlist2, (Object *)pid->owner_id, scene, MAX_DUPLI_RECUR);
        LISTBASE_FOREACH (PTCacheID *, pid2, &pidlist2) {
          if (pid2->type == PTCACHE_TYPE_SMOKE_DOMAIN) {
            if (pid2->cache && !(pid2->cache->flag & PTCACHE_BAKED)) {
              if (bake || pid2->cache->flag & PTCACHE_REDO_NEEDED) {
                BKE_ptcache_id_clear(pid2, PTCACHE_CLEAR_ALL, 0);
              }
              if (bake) {
                pid2->cache->flag |= PTCACHE_BAKING;
                pid2->cache->flag &= ~PTCACHE_BAKED;
              }
            }
          }
        }
        BLI_freelistN(&pidlist2);
      }

      if (bake || cache->flag & PTCACHE_REDO_NEEDED) {
        BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
      }

      startframe = std::max(cache->last_exact, cache->startframe);

      if (bake) {
        endframe = cache->endframe;
        cache->flag |= PTCACHE_BAKING;
      }
      else {
        endframe = std::min(endframe, cache->endframe);
      }

      cache->flag &= ~PTCACHE_BAKED;
    }
  }
  else {
    for (SETLOOPER_VIEW_LAYER(scene, view_layer, sce_iter, base)) {
      /* cache/bake everything in the scene */
      BKE_ptcache_ids_from_object(&pidlist, base->object, scene, MAX_DUPLI_RECUR);

      for (pid = static_cast<PTCacheID *>(pidlist.first); pid; pid = pid->next) {
        cache = pid->cache;
        if ((cache->flag & PTCACHE_BAKED) == 0) {
          if (pid->type == PTCACHE_TYPE_PARTICLES) {
            ParticleSystem *psys = (ParticleSystem *)pid->calldata;
            /* skip hair & keyed particles */
            if (psys->part->type == PART_HAIR || psys->part->phystype == PART_PHYS_KEYED) {
              continue;
            }

            psys_get_pointcache_start_end(scene,
                                          static_cast<ParticleSystem *>(pid->calldata),
                                          &cache->startframe,
                                          &cache->endframe);
          }

          /* XXX: workaround for regression introduced in ee3fadd, needs looking into. */
          if (pid->type == PTCACHE_TYPE_RIGIDBODY) {
            if ((cache->flag & PTCACHE_REDO_NEEDED ||
                 (cache->flag & PTCACHE_SIMULATION_VALID) == 0) &&
                (render || bake))
            {
              BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
            }
          }
          else if (((cache->flag & PTCACHE_BAKED) == 0) && (render || bake)) {
            BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
          }

          startframe = std::min(startframe, cache->startframe);

          if (bake || render) {
            cache->flag |= PTCACHE_BAKING;

            if (bake) {
              endframe = std::max(endframe, cache->endframe);
            }
          }

          cache->flag &= ~PTCACHE_BAKED;
        }
      }
      BLI_freelistN(&pidlist);
    }
  }

  scene->r.cfra = startframe;
  scene->r.framelen = 1.0;

  /* bake */

  bool use_timer = false;
  double stime, ptime, ctime, fetd;
  char run[32], cur[32], etd[32];
  int cancel = 0;

  stime = ptime = BLI_time_now_seconds();

  for (int fr = scene->r.cfra; fr <= endframe; fr += baker->quick_step, scene->r.cfra = fr) {
    BKE_scene_graph_update_for_newframe(depsgraph);

    if (baker->update_progress) {
      float progress = (float(scene->r.cfra - startframe) / float(endframe - startframe));
      baker->update_progress(baker->bake_job, progress, &cancel);
    }

    if (G.background) {
      printf("bake: frame %d :: %d\n", scene->r.cfra, endframe);
    }
    else {
      ctime = BLI_time_now_seconds();

      fetd = (ctime - ptime) * (endframe - scene->r.cfra) / baker->quick_step;

      if (use_timer || fetd > 60.0) {
        use_timer = true;

        ptcache_dt_to_str(cur, sizeof(cur), ctime - ptime);
        ptcache_dt_to_str(run, sizeof(run), ctime - stime);
        ptcache_dt_to_str(etd, sizeof(etd), fetd);

        printf("Baked for %s, current frame: %i/%i (%.3fs), ETC: %s\r",
               run,
               scene->r.cfra - startframe + 1,
               endframe - startframe + 1,
               ctime - ptime,
               etd);
      }

      ptime = ctime;
    }

    /* NOTE: breaking baking should leave calculated frames in cache, not clear it */
    if (cancel || G.is_break) {
      break;
    }

    scene->r.cfra += 1;
  }

  if (use_timer) {
    /* start with newline because of \r above */
    ptcache_dt_to_str(run, sizeof(run), BLI_time_now_seconds() - stime);
    printf("\nBake %s %s (%i frames simulated).\n",
           (cancel ? "canceled after" : "finished in"),
           run,
           scene->r.cfra - startframe);
  }

  /* clear baking flag */
  if (pid && cache) {
    cache->flag &= ~(PTCACHE_BAKING | PTCACHE_REDO_NEEDED);
    cache->flag |= PTCACHE_SIMULATION_VALID;
    if (bake) {
      cache->flag |= PTCACHE_BAKED;
      /* write info file */
      if (cache->flag & PTCACHE_DISK_CACHE) {
        if (pid->type == PTCACHE_TYPE_PARTICLES) {
          /* Since writing this from outside the bake job, make sure the ParticleSystem and
           * PTCacheID is in a fully evaluated state. */
          PTCacheID pid_eval;
          Object *ob = reinterpret_cast<Object *>(pid->owner_id);
          Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
          ParticleSystem *psys = static_cast<ParticleSystem *>(pid->calldata);
          ParticleSystem *psys_eval = psys_eval_get(depsgraph, ob, psys);
          BKE_ptcache_id_from_particles(&pid_eval, ob_eval, psys_eval);
          BKE_ptcache_write(&pid_eval, 0);
        }
        else {
          BKE_ptcache_write(pid, 0);
        }
      }
    }
  }
  else {
    for (SETLOOPER_VIEW_LAYER(scene, view_layer, sce_iter, base)) {
      BKE_ptcache_ids_from_object(&pidlist, base->object, scene, MAX_DUPLI_RECUR);

      LISTBASE_FOREACH (PTCacheID *, pid, &pidlist) {
        /* skip hair particles */
        if (pid->type == PTCACHE_TYPE_PARTICLES &&
            ((ParticleSystem *)pid->calldata)->part->type == PART_HAIR)
        {
          continue;
        }

        cache = pid->cache;

        if (baker->quick_step > 1) {
          cache->flag &= ~(PTCACHE_BAKING | PTCACHE_OUTDATED);
        }
        else {
          cache->flag &= ~(PTCACHE_BAKING | PTCACHE_REDO_NEEDED);
        }

        cache->flag |= PTCACHE_SIMULATION_VALID;

        if (bake) {
          cache->flag |= PTCACHE_BAKED;
          if (cache->flag & PTCACHE_DISK_CACHE) {
            if (pid->type == PTCACHE_TYPE_PARTICLES) {
              /* Since writing this from outside the bake job, make sure the ParticleSystem and
               * PTCacheID is in a fully evaluated state. */
              PTCacheID pid_eval;
              Object *ob = reinterpret_cast<Object *>(pid->owner_id);
              Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
              ParticleSystem *psys = static_cast<ParticleSystem *>(pid->calldata);
              ParticleSystem *psys_eval = psys_eval_get(depsgraph, ob, psys);
              BKE_ptcache_id_from_particles(&pid_eval, ob_eval, psys_eval);
              BKE_ptcache_write(&pid_eval, 0);
            }
            else {
              BKE_ptcache_write(pid, 0);
            }
          }
        }
      }
      BLI_freelistN(&pidlist);
    }
  }

  scene->r.framelen = frameleno;
  scene->r.cfra = cfrao;

  if (bake) { /* already on cfra unless baking */
    BKE_scene_graph_update_for_newframe(depsgraph);
  }

  /* TODO: call redraw all windows somehow */
}

/* Helpers */

void BKE_ptcache_disk_to_mem(PTCacheID *pid)
{
  PointCache *cache = pid->cache;
  PTCacheMem *pm = nullptr;
  int baked = cache->flag & PTCACHE_BAKED;
  int cfra, sfra = cache->startframe, efra = cache->endframe;

  /* Remove possible bake flag to allow clear */
  cache->flag &= ~PTCACHE_BAKED;

  /* PTCACHE_DISK_CACHE flag was cleared already */
  BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

  /* restore possible bake flag */
  cache->flag |= baked;

  for (cfra = sfra; cfra <= efra; cfra++) {
    pm = ptcache_disk_frame_to_mem(pid, cfra);

    if (pm) {
      BLI_addtail(&pid->cache->mem_cache, pm);
    }
  }
}
void BKE_ptcache_mem_to_disk(PTCacheID *pid)
{
  PointCache *cache = pid->cache;
  PTCacheMem *pm = static_cast<PTCacheMem *>(cache->mem_cache.first);
  int baked = cache->flag & PTCACHE_BAKED;

  /* Remove possible bake flag to allow clear */
  cache->flag &= ~PTCACHE_BAKED;

  /* PTCACHE_DISK_CACHE flag was set already */
  BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

  /* restore possible bake flag */
  cache->flag |= baked;

  for (; pm; pm = pm->next) {
    if (ptcache_mem_frame_to_disk(pid, pm) == 0) {
      cache->flag &= ~PTCACHE_DISK_CACHE;
      break;
    }
  }

  /* write info file */
  if (cache->flag & PTCACHE_BAKED) {
    BKE_ptcache_write(pid, 0);
  }
}
void BKE_ptcache_toggle_disk_cache(PTCacheID *pid)
{
  PointCache *cache = pid->cache;
  int last_exact = cache->last_exact;
  const char *blendfile_path = BKE_main_blendfile_path_from_global();

  if (blendfile_path[0] == '\0') {
    cache->flag &= ~PTCACHE_DISK_CACHE;
    if (G.debug & G_DEBUG) {
      printf("File must be saved before using disk cache!\n");
    }
    return;
  }

  if (cache->cached_frames) {
    MEM_freeN(cache->cached_frames);
    cache->cached_frames = nullptr;
    cache->cached_frames_len = 0;
  }

  if (cache->flag & PTCACHE_DISK_CACHE) {
    BKE_ptcache_mem_to_disk(pid);
  }
  else {
    BKE_ptcache_disk_to_mem(pid);
  }

  cache->flag ^= PTCACHE_DISK_CACHE;
  BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
  cache->flag ^= PTCACHE_DISK_CACHE;

  cache->last_exact = last_exact;

  BKE_ptcache_id_time(pid, nullptr, 0.0f, nullptr, nullptr, nullptr);

  cache->flag |= PTCACHE_FLAG_INFO_DIRTY;

  if ((cache->flag & PTCACHE_DISK_CACHE) == 0) {
    if (cache->index) {
      BKE_object_delete_ptcache((Object *)pid->owner_id, cache->index);
      cache->index = -1;
    }
  }
}

void BKE_ptcache_disk_cache_rename(PTCacheID *pid, const char *name_src, const char *name_dst)
{
  char old_name[80];
  int len; /* store the length of the string */
  /* mode is same as fopen's modes */
  DIR *dir;
  dirent *de;
  char path[MAX_PTCACHE_PATH];
  char old_filepath[MAX_PTCACHE_FILE];
  char new_path_full[MAX_PTCACHE_FILE];
  char old_path_full[MAX_PTCACHE_FILE];
  char ext[MAX_PTCACHE_FILE];

  /* If both names are the same, there is nothing to do. */
  if (STREQ(name_src, name_dst)) {
    return;
  }

  /* save old name */
  STRNCPY(old_name, pid->cache->name);

  /* get "from" filename */
  STRNCPY(pid->cache->name, name_src);

  len = ptcache_filepath(pid, old_filepath, 0, false, false); /* no path */

  ptcache_path(pid, path);
  dir = opendir(path);
  if (dir == nullptr) {
    STRNCPY(pid->cache->name, old_name);
    return;
  }

  ptcache_filepath_ext_append(pid, ext, 0, false, 0);

  /* put new name into cache */
  STRNCPY(pid->cache->name, name_dst);

  while ((de = readdir(dir)) != nullptr) {
    if (strstr(de->d_name, ext)) {                   /* Do we have the right extension? */
      if (STREQLEN(old_filepath, de->d_name, len)) { /* Do we have the right prefix. */
        /* read the number of the file */
        const int frame = ptcache_frame_from_filename(de->d_name, ext);

        if (frame != -1) {
          BLI_path_join(old_path_full, sizeof(old_path_full), path, de->d_name);
          ptcache_filepath(pid, new_path_full, frame, true, true);
          BLI_rename_overwrite(old_path_full, new_path_full);
        }
      }
    }
  }
  closedir(dir);

  STRNCPY(pid->cache->name, old_name);
}

void BKE_ptcache_load_external(PTCacheID *pid)
{
  /* TODO: */
  PointCache *cache = pid->cache;
  int len; /* store the length of the string */
  int info = 0;
  int start = MAXFRAME;
  int end = -1;

  /* mode is same as fopen's modes */
  DIR *dir;
  dirent *de;
  char path[MAX_PTCACHE_PATH];
  char filepath[MAX_PTCACHE_FILE];
  char ext[MAX_PTCACHE_PATH];

  if (!cache) {
    return;
  }

  ptcache_path(pid, path);

  len = ptcache_filepath(pid, filepath, 1, false, false); /* no path */

  dir = opendir(path);
  if (dir == nullptr) {
    return;
  }

  const char *fext = ptcache_file_extension(pid);

  if (cache->index >= 0) {
    SNPRINTF(ext, "_%02d%s", cache->index, fext);
  }
  else {
    STRNCPY(ext, fext);
  }

  while ((de = readdir(dir)) != nullptr) {
    if (strstr(de->d_name, ext)) {               /* Do we have the right extension? */
      if (STREQLEN(filepath, de->d_name, len)) { /* Do we have the right prefix. */
        /* read the number of the file */
        const int frame = ptcache_frame_from_filename(de->d_name, ext);

        if (frame != -1) {
          if (frame) {
            start = std::min(start, frame);
            end = std::max(end, frame);
          }
          else {
            info = 1;
          }
        }
      }
    }
  }
  closedir(dir);

  if (start != MAXFRAME) {
    PTCacheFile *pf;

    cache->startframe = start;
    cache->endframe = end;
    cache->totpoint = 0;

    if (pid->type == PTCACHE_TYPE_SMOKE_DOMAIN) {
      /* necessary info in every file */
    }
    /* read totpoint from info file (frame 0) */
    else if (info) {
      pf = ptcache_file_open(pid, PTCACHE_FILE_READ, 0);

      if (pf) {
        if (ptcache_file_header_begin_read(pf)) {
          if (pf->type == pid->type && pid->read_header(pf)) {
            cache->totpoint = pf->totpoint;
            cache->flag |= PTCACHE_READ_INFO;
          }
          else {
            cache->totpoint = 0;
          }
        }
        ptcache_file_close(pf);
      }
    }
    /* or from any old format cache file */
    else {
      float old_data[14];
      int elemsize = ptcache_old_elemsize(pid);
      pf = ptcache_file_open(pid, PTCACHE_FILE_READ, cache->startframe);

      if (pf) {
        while (ptcache_file_read(pf, old_data, 1, elemsize)) {
          cache->totpoint++;
        }

        ptcache_file_close(pf);
      }
    }
    cache->flag |= (PTCACHE_BAKED | PTCACHE_DISK_CACHE | PTCACHE_SIMULATION_VALID);
    cache->flag &= ~(PTCACHE_OUTDATED | PTCACHE_FRAMES_SKIPPED);
  }

  /* make sure all new frames are loaded */
  if (cache->cached_frames) {
    MEM_freeN(cache->cached_frames);
    cache->cached_frames = nullptr;
    cache->cached_frames_len = 0;
  }

  cache->flag |= PTCACHE_FLAG_INFO_DIRTY;
}

void BKE_ptcache_update_info(PTCacheID *pid)
{
  PointCache *cache = pid->cache;
  int totframes = 0;
  char mem_info[sizeof(PointCache::info) / sizeof(*PointCache::info)];

  cache->flag &= ~PTCACHE_FLAG_INFO_DIRTY;

  if (cache->flag & PTCACHE_EXTERNAL) {
    int cfra = cache->startframe;

    for (; cfra <= cache->endframe; cfra++) {
      if (BKE_ptcache_id_exist(pid, cfra)) {
        totframes++;
      }
    }

    /* smoke doesn't use frame 0 as info frame so can't check based on totpoint */
    if (pid->type == PTCACHE_TYPE_SMOKE_DOMAIN && totframes) {
      SNPRINTF(cache->info, RPT_("%i frames found!"), totframes);
    }
    else if (totframes && cache->totpoint) {
      SNPRINTF(cache->info, RPT_("%i points found!"), cache->totpoint);
    }
    else {
      STRNCPY(cache->info, RPT_("No valid data to read!"));
    }
    return;
  }

  if (cache->flag & PTCACHE_DISK_CACHE) {
    if (pid->type == PTCACHE_TYPE_SMOKE_DOMAIN) {
      int totpoint = pid->totpoint(pid->calldata, 0);

      if (cache->totpoint > totpoint) {
        SNPRINTF(mem_info, RPT_("%i cells + High Resolution cached"), totpoint);
      }
      else {
        SNPRINTF(mem_info, RPT_("%i cells cached"), totpoint);
      }
    }
    else {
      int cfra = cache->startframe;

      for (; cfra <= cache->endframe; cfra++) {
        if (BKE_ptcache_id_exist(pid, cfra)) {
          totframes++;
        }
      }

      SNPRINTF(mem_info, RPT_("%i frames on disk"), totframes);
    }
  }
  else {
    PTCacheMem *pm = static_cast<PTCacheMem *>(cache->mem_cache.first);
    char formatted_tot[BLI_STR_FORMAT_INT32_GROUPED_SIZE];
    char formatted_mem[BLI_STR_FORMAT_INT64_BYTE_UNIT_SIZE];
    long long int bytes = 0.0f;
    int i;

    for (; pm; pm = pm->next) {
      for (i = 0; i < BPHYS_TOT_DATA; i++) {
        bytes += MEM_allocN_len(pm->data[i]);
      }

      LISTBASE_FOREACH (PTCacheExtra *, extra, &pm->extradata) {
        bytes += MEM_allocN_len(extra->data);
        bytes += sizeof(PTCacheExtra);
      }

      bytes += sizeof(PTCacheMem);

      totframes++;
    }

    BLI_str_format_int_grouped(formatted_tot, totframes);
    BLI_str_format_byte_unit(formatted_mem, bytes, false);

    SNPRINTF(mem_info, RPT_("%s frames in memory (%s)"), formatted_tot, formatted_mem);
  }

  if (cache->flag & PTCACHE_OUTDATED) {
    SNPRINTF(cache->info, RPT_("%s, cache is outdated!"), mem_info);
  }
  else if (cache->flag & PTCACHE_FRAMES_SKIPPED) {
    SNPRINTF(cache->info, RPT_("%s, not exact since frame %i"), mem_info, cache->last_exact);
  }
  else {
    SNPRINTF(cache->info, "%s.", mem_info);
  }
}

void BKE_ptcache_validate(PointCache *cache, int framenr)
{
  if (cache) {
    cache->flag |= PTCACHE_SIMULATION_VALID;
    cache->simframe = framenr;
  }
}
void BKE_ptcache_invalidate(PointCache *cache)
{
  if (cache) {
    cache->flag &= ~PTCACHE_SIMULATION_VALID;
    cache->simframe = 0;
    cache->last_exact = std::min(cache->startframe, 0);
  }
}

void BKE_ptcache_blend_write(BlendWriter *writer, ListBase *ptcaches)
{
  LISTBASE_FOREACH (PointCache *, cache, ptcaches) {
    BLO_write_struct(writer, PointCache, cache);

    if ((cache->flag & PTCACHE_DISK_CACHE) == 0) {
      LISTBASE_FOREACH (PTCacheMem *, pm, &cache->mem_cache) {
        BLO_write_struct(writer, PTCacheMem, pm);

        for (int i = 0; i < BPHYS_TOT_DATA; i++) {
          if (pm->data[i] && pm->data_types & (1 << i)) {
            if (i == BPHYS_DATA_BOIDS) {
              BLO_write_struct_array(writer, BoidData, pm->totpoint, pm->data[i]);
            }
            else if (i == BPHYS_DATA_INDEX) { /* Only 'cache type' to use uint values. */
              BLO_write_uint32_array(
                  writer, pm->totpoint, reinterpret_cast<uint32_t *>(pm->data[i]));
            }
            else { /* All other types of caches use (vectors of) floats. */
              /* data_size returns bytes. */
              const uint32_t items_num = pm->totpoint * (BKE_ptcache_data_size(i) / sizeof(float));
              BLO_write_float_array(writer, items_num, reinterpret_cast<float *>(pm->data[i]));
            }
          }
        }

        LISTBASE_FOREACH (PTCacheExtra *, extra, &pm->extradata) {
          BLO_write_struct(writer, PTCacheExtra, extra);
          if (extra->type == BPHYS_EXTRA_FLUID_SPRINGS) {
            BLO_write_struct_array(writer, ParticleSpring, extra->totdata, extra->data);
          }
          else if (extra->type == BPHYS_EXTRA_CLOTH_ACCELERATION) {
            BLO_write_struct_array(writer, vec3f, extra->totdata, extra->data);
          }
          else if (extra->data) {
            BLI_assert_unreachable();
          }
        }
      }
    }
  }
}

static void direct_link_pointcache_mem(BlendDataReader *reader, PTCacheMem *pm)
{
  for (int i = 0; i < BPHYS_TOT_DATA; i++) {
    if (i == BPHYS_DATA_BOIDS) {
      BLO_read_struct_array(reader, BoidData, pm->totpoint, &pm->data[i]);
    }
    else if (i == BPHYS_DATA_INDEX) { /* Only 'cache type' to use uint values. */
      BLO_read_uint32_array(reader, pm->totpoint, reinterpret_cast<uint32_t **>(&pm->data[i]));
    }
    else { /* All other types of caches use (vectors of) floats. */
      /* data_size returns bytes. */
      const uint32_t items_num = pm->totpoint * (BKE_ptcache_data_size(i) / sizeof(float));
      BLO_read_float_array(reader, items_num, reinterpret_cast<float **>(&pm->data[i]));
    }
  }

  BLO_read_struct_list(reader, PTCacheExtra, &pm->extradata);

  LISTBASE_FOREACH (PTCacheExtra *, extra, &pm->extradata) {
    if (extra->type == BPHYS_EXTRA_FLUID_SPRINGS) {
      BLO_read_struct_array(reader, ParticleSpring, extra->totdata, &extra->data);
    }
    else if (extra->type == BPHYS_EXTRA_CLOTH_ACCELERATION) {
      BLO_read_struct_array(reader, vec3f, extra->totdata, &extra->data);
    }
    else if (extra->data) {
      extra->data = nullptr;
    }
  }
}

static void direct_link_pointcache(BlendDataReader *reader, PointCache *cache)
{
  if ((cache->flag & PTCACHE_DISK_CACHE) == 0) {
    BLO_read_struct_list(reader, PTCacheMem, &cache->mem_cache);
    LISTBASE_FOREACH (PTCacheMem *, pm, &cache->mem_cache) {
      direct_link_pointcache_mem(reader, pm);
    }
  }
  else {
    BLI_listbase_clear(&cache->mem_cache);
  }

  cache->flag &= ~PTCACHE_SIMULATION_VALID;
  cache->simframe = 0;
  cache->edit = nullptr;
  cache->free_edit = nullptr;
  cache->cached_frames = nullptr;
  cache->cached_frames_len = 0;
}

void BKE_ptcache_blend_read_data(BlendDataReader *reader,
                                 ListBase *ptcaches,
                                 PointCache **ocache,
                                 int force_disk)
{
  if (ptcaches->first) {
    BLO_read_struct_list(reader, PointCache, ptcaches);
    LISTBASE_FOREACH (PointCache *, cache, ptcaches) {
      direct_link_pointcache(reader, cache);
      if (force_disk) {
        cache->flag |= PTCACHE_DISK_CACHE;
        cache->step = 1;
      }
    }

    BLO_read_struct(reader, PointCache, ocache);
  }
  else if (*ocache) {
    /* old "single" caches need to be linked too */
    BLO_read_struct(reader, PointCache, ocache);
    direct_link_pointcache(reader, *ocache);
    if (force_disk) {
      (*ocache)->flag |= PTCACHE_DISK_CACHE;
      (*ocache)->step = 1;
    }

    ptcaches->first = ptcaches->last = *ocache;
  }
}
