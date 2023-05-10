/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_boid_types.h"       /* for #BoidData */
#include "DNA_pointcache_types.h" /* for #BPHYS_TOT_DATA */

#include <stdio.h> /* for #FILE */

#ifdef __cplusplus
extern "C" {
#endif

/* Point cache clearing option, for BKE_ptcache_id_clear, before
 * and after are non-inclusive (they won't remove the cfra) */
#define PTCACHE_CLEAR_ALL 0
#define PTCACHE_CLEAR_FRAME 1
#define PTCACHE_CLEAR_BEFORE 2
#define PTCACHE_CLEAR_AFTER 3

/* Point cache reset options */
#define PTCACHE_RESET_DEPSGRAPH 0
#define PTCACHE_RESET_BAKED 1
#define PTCACHE_RESET_OUTDATED 2
// #define PTCACHE_RESET_FREE 3  /*UNUSED*/

/* Add the blend-file name after `blendcache_`. */
#define PTCACHE_EXT ".bphys"
#define PTCACHE_PATH "blendcache_"

/* File open options, for BKE_ptcache_file_open */
#define PTCACHE_FILE_READ 0
#define PTCACHE_FILE_WRITE 1
#define PTCACHE_FILE_UPDATE 2

/* PTCacheID types */
#define PTCACHE_TYPE_SOFTBODY 0
#define PTCACHE_TYPE_PARTICLES 1
#define PTCACHE_TYPE_CLOTH 2
#define PTCACHE_TYPE_SMOKE_DOMAIN 3
#define PTCACHE_TYPE_SMOKE_HIGHRES 4
#define PTCACHE_TYPE_DYNAMICPAINT 5
#define PTCACHE_TYPE_RIGIDBODY 6

/* high bits reserved for flags that need to be stored in file */
#define PTCACHE_TYPEFLAG_COMPRESS (1 << 16)
#define PTCACHE_TYPEFLAG_EXTRADATA (1 << 17)

#define PTCACHE_TYPEFLAG_TYPEMASK 0x0000FFFF
#define PTCACHE_TYPEFLAG_FLAGMASK 0xFFFF0000

/* PTCache read return code */
#define PTCACHE_READ_EXACT 1
#define PTCACHE_READ_INTERPOLATED 2
#define PTCACHE_READ_OLD 3

/* Structs */
struct BlendDataReader;
struct BlendWriter;
struct ClothModifierData;
struct DynamicPaintSurface;
struct FluidModifierData;
struct ListBase;
struct Main;
struct Object;
struct ParticleKey;
struct ParticleSystem;
struct PointCache;
struct RigidBodyWorld;
struct Scene;
struct SoftBody;
struct ViewLayer;

/* temp structure for read/write */
typedef struct PTCacheData {
  unsigned int index;
  float loc[3];
  float vel[3];
  float rot[4];
  float ave[3];
  float size;
  float times[3];
  struct BoidData boids;
} PTCacheData;

typedef struct PTCacheFile {
  FILE *fp;

  int frame, old_format;
  unsigned int totpoint, type;
  unsigned int data_types, flag;

  struct PTCacheData data;
  void *cur[BPHYS_TOT_DATA];
} PTCacheFile;

#define PTCACHE_VEL_PER_SEC 1

enum {
  PTCACHE_FILE_PTCACHE = 0,
};

typedef struct PTCacheID {
  struct PTCacheID *next, *prev;

  struct Scene *scene;
  struct ID *owner_id;
  void *calldata;
  unsigned int type, file_type;
  unsigned int stack_index;
  unsigned int flag;

  unsigned int default_step;
  unsigned int max_step;

  /** flags defined in `DNA_object_force_types.h`. */
  unsigned int data_types, info_types;

  /** Copies point data to cache data. */
  int (*write_point)(int index, void *calldata, void **data, int cfra);
  /** Copies cache data to point data. */
  void (*read_point)(int index, void *calldata, void **data, float cfra, const float *old_data);
  /** Interpolated between previously read point data and cache data. */
  void (*interpolate_point)(int index,
                            void *calldata,
                            void **data,
                            float cfra,
                            float cfra1,
                            float cfra2,
                            const float *old_data);

  /** Copies point data to cache data. */
  int (*write_stream)(PTCacheFile *pf, void *calldata);
  /** Copies cache data to point data. */
  int (*read_stream)(PTCacheFile *pf, void *calldata);

  /** Copies custom #PTCacheMem::extradata to cache data. */
  void (*write_extra_data)(void *calldata, struct PTCacheMem *pm, int cfra);
  /** Copies custom #PTCacheMem::extradata to cache data. */
  void (*read_extra_data)(void *calldata, struct PTCacheMem *pm, float cfra);
  /** Copies custom #PTCacheMem::extradata to cache data */
  void (*interpolate_extra_data)(
      void *calldata, struct PTCacheMem *pm, float cfra, float cfra1, float cfra2);

  /**
   * Total number of simulated points
   * (the `cfra` parameter is just for using same function pointer with `totwrite`).
   */
  int (*totpoint)(void *calldata, int cfra);
  /** Report error if number of points does not match */
  void (*error)(const struct ID *owner_id, void *calldata, const char *message);
  /** Number of points written for current cache frame. */
  int (*totwrite)(void *calldata, int cfra);

  int (*write_header)(PTCacheFile *pf);
  int (*read_header)(PTCacheFile *pf);

  struct PointCache *cache;
  /** Used for setting the current cache from `ptcaches` list. */
  struct PointCache **cache_ptr;
  struct ListBase *ptcaches;
} PTCacheID;

typedef struct PTCacheBaker {
  struct Main *bmain;
  struct Scene *scene;
  struct ViewLayer *view_layer;
  struct Depsgraph *depsgraph;
  bool bake;
  bool render;
  bool anim_init;
  int quick_step;
  struct PTCacheID pid;

  void (*update_progress)(void *data, float progress, int *cancel);
  void *bake_job;
} PTCacheBaker;

/* PTCacheEditKey->flag */
#define PEK_SELECT 1
#define PEK_TAG 2
#define PEK_HIDE 4
#define PEK_USE_WCO 8

typedef struct PTCacheEditKey {
  float *co;
  float *vel;
  float *rot;
  float *time;

  float world_co[3];
  float ftime;
  float length;
  short flag;
} PTCacheEditKey;

/* PTCacheEditPoint->flag */
#define PEP_TAG 1
#define PEP_EDIT_RECALC 2
#define PEP_TRANSFORM 4
#define PEP_HIDE 8

typedef struct PTCacheEditPoint {
  struct PTCacheEditKey *keys;
  int totkey;
  short flag;
} PTCacheEditPoint;

typedef struct PTCacheUndo {
  struct PTCacheEditPoint *points;

  /* particles stuff */
  struct ParticleData *particles;
  struct KDTree_3d *emitter_field;
  float *emitter_cosnos;
  int psys_flag;

  /* cache stuff */
  struct ListBase mem_cache;

  int totpoint;

  size_t undo_size;
} PTCacheUndo;

enum {
  /* Modifier stack got evaluated during particle edit mode, need to copy
   * new evaluated particles to the edit struct.
   */
  PT_CACHE_EDIT_UPDATE_PARTICLE_FROM_EVAL = (1 << 0),
};

typedef struct PTCacheEdit {
  int flags;

  PTCacheEditPoint *points;

  struct PTCacheID pid;

  /* particles stuff */
  struct ParticleSystem *psys;
  struct ParticleSystem *psys_eval;
  struct ParticleSystemModifierData *psmd;
  struct ParticleSystemModifierData *psmd_eval;
  struct KDTree_3d *emitter_field;
  /* Localspace face centers and normals (average of its verts), from the derived mesh. */
  float *emitter_cosnos;
  int *mirror_cache;

  struct ParticleCacheKey **pathcache; /* path cache (runtime) */
  ListBase pathcachebufs;

  int totpoint, totframes, totcached, edited;
} PTCacheEdit;

void BKE_ptcache_make_particle_key(struct ParticleKey *key, int index, void **data, float time);

/**************** Creating ID's ****************************/

void BKE_ptcache_id_from_softbody(PTCacheID *pid, struct Object *ob, struct SoftBody *sb);
void BKE_ptcache_id_from_particles(PTCacheID *pid, struct Object *ob, struct ParticleSystem *psys);
void BKE_ptcache_id_from_cloth(PTCacheID *pid, struct Object *ob, struct ClothModifierData *clmd);
/**
 * The fluid modifier does not actually use this anymore, but some parts of Blender expect that it
 * still has a point cache currently. For example, the fluid modifier uses
 * #DEG_add_collision_relations, which internally creates relations with the point cache.
 */
void BKE_ptcache_id_from_smoke(PTCacheID *pid, struct Object *ob, struct FluidModifierData *fmd);
void BKE_ptcache_id_from_dynamicpaint(PTCacheID *pid,
                                      struct Object *ob,
                                      struct DynamicPaintSurface *surface);
void BKE_ptcache_id_from_rigidbody(PTCacheID *pid, struct Object *ob, struct RigidBodyWorld *rbw);

/**
 * \param ob: Optional, may be NULL.
 * \param scene: Optional may be NULL.
 */
PTCacheID BKE_ptcache_id_find(struct Object *ob, struct Scene *scene, struct PointCache *cache);
void BKE_ptcache_ids_from_object(struct ListBase *lb,
                                 struct Object *ob,
                                 struct Scene *scene,
                                 int duplis);

/****************** Query functions ****************************/

/**
 * Check whether object has a point cache.
 */
bool BKE_ptcache_object_has(struct Scene *scene, struct Object *ob, int duplis);

/************ ID specific functions ************************/

void BKE_ptcache_id_clear(PTCacheID *id, int mode, unsigned int cfra);
bool BKE_ptcache_id_exist(PTCacheID *id, int cfra);
int BKE_ptcache_id_reset(struct Scene *scene, PTCacheID *id, int mode);
void BKE_ptcache_id_time(PTCacheID *pid,
                         struct Scene *scene,
                         float cfra,
                         int *startframe,
                         int *endframe,
                         float *timescale);
int BKE_ptcache_object_reset(struct Scene *scene, struct Object *ob, int mode);

void BKE_ptcache_update_info(PTCacheID *pid);

/*********** General cache reading/writing ******************/

/**
 * Size of cache data type.
 */
int BKE_ptcache_data_size(int data_type);

/**
 * Is point with index in memory cache?
 * Check to see if point number "index" is in `pm` (uses binary search for index data).
 */
int BKE_ptcache_mem_index_find(struct PTCacheMem *pm, unsigned int index);

/* Memory cache read/write helpers. */

void BKE_ptcache_mem_pointers_init(struct PTCacheMem *pm, void *cur[BPHYS_TOT_DATA]);
void BKE_ptcache_mem_pointers_incr(void *cur[BPHYS_TOT_DATA]);
int BKE_ptcache_mem_pointers_seek(int point_index,
                                  struct PTCacheMem *pm,
                                  void *cur[BPHYS_TOT_DATA]);

/**
 * Main cache reading call which reads cache from disk or memory.
 * Possible to get old or interpolated result.
 */
int BKE_ptcache_read(PTCacheID *pid, float cfra, bool no_extrapolate_old);

/**
 * Main cache writing call.
 * Writes cache to disk or memory.
 */
int BKE_ptcache_write(PTCacheID *pid, unsigned int cfra);

/******************* Allocate & free ***************/

struct PointCache *BKE_ptcache_add(struct ListBase *ptcaches);
void BKE_ptcache_free_mem(struct ListBase *mem_cache);
void BKE_ptcache_free(struct PointCache *cache);
void BKE_ptcache_free_list(struct ListBase *ptcaches);
/** Returns first point cache. */
struct PointCache *BKE_ptcache_copy_list(struct ListBase *ptcaches_new,
                                         const struct ListBase *ptcaches_old,
                                         int flag);

/********************** Baking *********************/

/**
 * Bakes cache with cache_step sized jumps in time, not accurate but very fast.
 */
void BKE_ptcache_quick_cache_all(struct Main *bmain,
                                 struct Scene *scene,
                                 struct ViewLayer *view_layer);

/**
 * Bake cache or simulate to current frame with settings defined in the baker.
 * if bake is not given run simulations to current frame.
 */
void BKE_ptcache_bake(struct PTCacheBaker *baker);

/**
 * Convert disk cache to memory cache.
 */
void BKE_ptcache_disk_to_mem(struct PTCacheID *pid);
/**
 * Convert memory cache to disk cache.
 */
void BKE_ptcache_mem_to_disk(struct PTCacheID *pid);
/**
 * Convert disk cache to memory cache and vice versa. Clears the cache that was converted.
 */
void BKE_ptcache_toggle_disk_cache(struct PTCacheID *pid);
/**
 * Rename all disk cache files with a new name. Doesn't touch the actual content of the files.
 */
void BKE_ptcache_disk_cache_rename(struct PTCacheID *pid,
                                   const char *name_src,
                                   const char *name_dst);

/**
 * Loads simulation from external (disk) cache files.
 */
void BKE_ptcache_load_external(struct PTCacheID *pid);
/**
 * Set correct flags after successful simulation step.
 */
void BKE_ptcache_validate(struct PointCache *cache, int framenr);
/**
 * Set correct flags after unsuccessful simulation step.
 */
void BKE_ptcache_invalidate(struct PointCache *cache);

/********************** .blend File I/O *********************/

void BKE_ptcache_blend_write(struct BlendWriter *writer, struct ListBase *ptcaches);
void BKE_ptcache_blend_read_data(struct BlendDataReader *reader,
                                 struct ListBase *ptcaches,
                                 struct PointCache **ocache,
                                 int force_disk);

#ifdef __cplusplus
}
#endif
