/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell Barton <ideasman42@gmail.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_POINTCACHE_H__
#define __BKE_POINTCACHE_H__

/** \file BKE_pointcache.h
 *  \ingroup bke
 */

#include "DNA_ID.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_object_force.h"
#include "DNA_boid_types.h"
#include <stdio.h> /* for FILE */

/* Point cache clearing option, for BKE_ptcache_id_clear, before
 * and after are non inclusive (they wont remove the cfra) */
#define PTCACHE_CLEAR_ALL       0
#define PTCACHE_CLEAR_FRAME     1
#define PTCACHE_CLEAR_BEFORE    2
#define PTCACHE_CLEAR_AFTER     3

/* Point cache reset options */
#define PTCACHE_RESET_DEPSGRAPH     0
#define PTCACHE_RESET_BAKED         1
#define PTCACHE_RESET_OUTDATED      2
/* #define PTCACHE_RESET_FREE			3 */ /*UNUSED*/

/* Add the blendfile name after blendcache_ */
#define PTCACHE_EXT ".bphys"
#define PTCACHE_PATH "blendcache_"

/* File open options, for BKE_ptcache_file_open */
#define PTCACHE_FILE_READ   0
#define PTCACHE_FILE_WRITE  1
#define PTCACHE_FILE_UPDATE 2

/* PTCacheID types */
#define PTCACHE_TYPE_SOFTBODY           0
#define PTCACHE_TYPE_PARTICLES          1
#define PTCACHE_TYPE_CLOTH              2
#define PTCACHE_TYPE_SMOKE_DOMAIN       3
#define PTCACHE_TYPE_SMOKE_HIGHRES      4
#define PTCACHE_TYPE_DYNAMICPAINT       5
#define PTCACHE_TYPE_RIGIDBODY          6

/* high bits reserved for flags that need to be stored in file */
#define PTCACHE_TYPEFLAG_COMPRESS       (1 << 16)
#define PTCACHE_TYPEFLAG_EXTRADATA      (1 << 17)

#define PTCACHE_TYPEFLAG_TYPEMASK           0x0000FFFF
#define PTCACHE_TYPEFLAG_FLAGMASK           0xFFFF0000

/* PTCache read return code */
#define PTCACHE_READ_EXACT              1
#define PTCACHE_READ_INTERPOLATED       2
#define PTCACHE_READ_OLD                3

/* Structs */
struct ClothModifierData;
struct ListBase;
struct Main;
struct Object;
struct ParticleKey;
struct ParticleSystem;
struct PointCache;
struct Scene;
struct SmokeModifierData;
struct SoftBody;
struct RigidBodyWorld;

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

#define PTCACHE_VEL_PER_SEC     1

typedef struct PTCacheID {
	struct PTCacheID *next, *prev;

	struct Scene *scene;
	struct Object *ob;
	void *calldata;
	unsigned int type;
	unsigned int stack_index;
	unsigned int flag;

	unsigned int default_step;
	unsigned int max_step;

	/* flags defined in DNA_object_force.h */
	unsigned int data_types, info_types;

	/* copies point data to cache data */
	int (*write_point)(int index, void *calldata, void **data, int cfra);
	/* copies cache cata to point data */
	void (*read_point)(int index, void *calldata, void **data, float cfra, float *old_data);
	/* interpolated between previously read point data and cache data */
	void (*interpolate_point)(int index, void *calldata, void **data, float cfra, float cfra1, float cfra2, float *old_data);

	/* copies point data to cache data */
	int (*write_stream)(PTCacheFile *pf, void *calldata);
	/* copies cache cata to point data */
	int (*read_stream)(PTCacheFile *pf, void *calldata);

	/* copies custom extradata to cache data */
	void (*write_extra_data)(void *calldata, struct PTCacheMem *pm, int cfra);
	/* copies custom extradata to cache data */
	void (*read_extra_data)(void *calldata, struct PTCacheMem *pm, float cfra);
	/* copies custom extradata to cache data */
	void (*interpolate_extra_data)(void *calldata, struct PTCacheMem *pm, float cfra, float cfra1, float cfra2);

	/* total number of simulated points (the cfra parameter is just for using same function pointer with totwrite) */
	int (*totpoint)(void *calldata, int cfra);
	/* report error if number of points does not match */
	void (*error)(void *calldata, const char *message);
	/* number of points written for current cache frame */
	int (*totwrite)(void *calldata, int cfra);

	int (*write_header)(PTCacheFile *pf);
	int (*read_header)(PTCacheFile *pf);

	struct PointCache *cache;
	/* used for setting the current cache from ptcaches list */
	struct PointCache **cache_ptr;
	struct ListBase *ptcaches;
} PTCacheID;

typedef struct PTCacheBaker {
	struct Main *main;
	struct Scene *scene;
	int bake;
	int render;
	int anim_init;
	int quick_step;
	struct PTCacheID *pid;
	int (*break_test)(void *data);
	void *break_data;
	void (*progressbar)(void *data, int num);
	void (*progressend)(void *data);
	void *progresscontext;
} PTCacheBaker;

/* PTCacheEditKey->flag */
#define PEK_SELECT      1
#define PEK_TAG         2
#define PEK_HIDE        4
#define PEK_USE_WCO     8

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
#define PEP_TAG             1
#define PEP_EDIT_RECALC     2
#define PEP_TRANSFORM       4
#define PEP_HIDE            8

typedef struct PTCacheEditPoint {
	struct PTCacheEditKey *keys;
	int totkey;
	short flag;
} PTCacheEditPoint;

typedef struct PTCacheUndo {
	struct PTCacheUndo *next, *prev;
	struct PTCacheEditPoint *points;

	/* particles stuff */
	struct ParticleData *particles;
	struct KDTree *emitter_field;
	float *emitter_cosnos;
	int psys_flag;

	/* cache stuff */
	struct ListBase mem_cache;

	int totpoint;
	char name[64];
} PTCacheUndo;

typedef struct PTCacheEdit {
	ListBase undo;
	struct PTCacheUndo *curundo;
	PTCacheEditPoint *points;

	struct PTCacheID pid;

	/* particles stuff */
	struct ParticleSystem *psys;
	struct KDTree *emitter_field;
	float *emitter_cosnos; /* localspace face centers and normals (average of its verts), from the derived mesh */
	int *mirror_cache;

	struct ParticleCacheKey **pathcache;    /* path cache (runtime) */
	ListBase pathcachebufs;

	int totpoint, totframes, totcached, edited;

	unsigned char sel_col[3];
	unsigned char nosel_col[3];
} PTCacheEdit;

/* Particle functions */
void BKE_ptcache_make_particle_key(struct ParticleKey *key, int index, void **data, float time);

/**************** Creating ID's ****************************/
void BKE_ptcache_id_from_softbody(PTCacheID *pid, struct Object *ob, struct SoftBody *sb);
void BKE_ptcache_id_from_particles(PTCacheID *pid, struct Object *ob, struct ParticleSystem *psys);
void BKE_ptcache_id_from_cloth(PTCacheID *pid, struct Object *ob, struct ClothModifierData *clmd);
void BKE_ptcache_id_from_smoke(PTCacheID *pid, struct Object *ob, struct SmokeModifierData *smd);
void BKE_ptcache_id_from_dynamicpaint(PTCacheID *pid, struct Object *ob, struct DynamicPaintSurface *surface);
void BKE_ptcache_id_from_rigidbody(PTCacheID *pid, struct Object *ob, struct RigidBodyWorld *rbw);

void BKE_ptcache_ids_from_object(struct ListBase *lb, struct Object *ob, struct Scene *scene, int duplis);

/***************** Global funcs ****************************/
void BKE_ptcache_remove(void);

/************ ID specific functions ************************/
void    BKE_ptcache_id_clear(PTCacheID *id, int mode, unsigned int cfra);
int     BKE_ptcache_id_exist(PTCacheID *id, int cfra);
int     BKE_ptcache_id_reset(struct Scene *scene, PTCacheID *id, int mode);
void    BKE_ptcache_id_time(PTCacheID *pid, struct Scene *scene, float cfra, int *startframe, int *endframe, float *timescale);
int     BKE_ptcache_object_reset(struct Scene *scene, struct Object *ob, int mode);

void BKE_ptcache_update_info(PTCacheID *pid);

/*********** General cache reading/writing ******************/

/* Size of cache data type. */
int     BKE_ptcache_data_size(int data_type);

/* Is point with indes in memory cache */
int BKE_ptcache_mem_index_find(struct PTCacheMem *pm, unsigned int index);

/* Memory cache read/write helpers. */
void BKE_ptcache_mem_pointers_init(struct PTCacheMem *pm);
void BKE_ptcache_mem_pointers_incr(struct PTCacheMem *pm);
int  BKE_ptcache_mem_pointers_seek(int point_index, struct PTCacheMem *pm);

/* Main cache reading call. */
int     BKE_ptcache_read(PTCacheID *pid, float cfra);

/* Main cache writing call. */
int     BKE_ptcache_write(PTCacheID *pid, unsigned int cfra);

/******************* Allocate & free ***************/
struct PointCache *BKE_ptcache_add(struct ListBase *ptcaches);
void BKE_ptcache_free_mem(struct ListBase *mem_cache);
void BKE_ptcache_free(struct PointCache *cache);
void BKE_ptcache_free_list(struct ListBase *ptcaches);
struct PointCache *BKE_ptcache_copy_list(struct ListBase *ptcaches_new, struct ListBase *ptcaches_old, bool copy_data);

/********************** Baking *********************/

/* Bakes cache with cache_step sized jumps in time, not accurate but very fast. */
void BKE_ptcache_quick_cache_all(struct Main *bmain, struct Scene *scene);

/* Bake cache or simulate to current frame with settings defined in the baker. */
void BKE_ptcache_bake(struct PTCacheBaker *baker);

/* Convert disk cache to memory cache. */
void BKE_ptcache_disk_to_mem(struct PTCacheID *pid);

/* Convert memory cache to disk cache. */
void BKE_ptcache_mem_to_disk(struct PTCacheID *pid);

/* Convert disk cache to memory cache and vice versa. Clears the cache that was converted. */
void BKE_ptcache_toggle_disk_cache(struct PTCacheID *pid);

/* Rename all disk cache files with a new name. Doesn't touch the actual content of the files. */
void BKE_ptcache_disk_cache_rename(struct PTCacheID *pid, const char *name_src, const char *name_dst);

/* Loads simulation from external (disk) cache files. */
void BKE_ptcache_load_external(struct PTCacheID *pid);

/* Set correct flags after successful simulation step */
void BKE_ptcache_validate(struct PointCache *cache, int framenr);

/* Set correct flags after unsuccessful simulation step */
void BKE_ptcache_invalidate(struct PointCache *cache);

#endif
