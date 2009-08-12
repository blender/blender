/*
*
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
* Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef BKE_POINTCACHE_H
#define BKE_POINTCACHE_H

#include "DNA_ID.h"
#include "DNA_object_force.h"
#include "DNA_boid_types.h"

#include "MEM_guardedalloc.h"

/* Point cache clearing option, for BKE_ptcache_id_clear, before
 * and after are non inclusive (they wont remove the cfra) */
#define PTCACHE_CLEAR_ALL		0
#define PTCACHE_CLEAR_FRAME		1
#define PTCACHE_CLEAR_BEFORE	2
#define PTCACHE_CLEAR_AFTER		3

/* Point cache reset options */
#define PTCACHE_RESET_DEPSGRAPH		0
#define PTCACHE_RESET_BAKED			1
#define PTCACHE_RESET_OUTDATED		2
#define PTCACHE_RESET_FREE			3

/* Add the blendfile name after blendcache_ */
#define PTCACHE_EXT ".bphys"
#define PTCACHE_PATH "blendcache_"

/* File open options, for BKE_ptcache_file_open */
#define PTCACHE_FILE_READ	0
#define PTCACHE_FILE_WRITE	1

/* PTCacheID types */
#define PTCACHE_TYPE_SOFTBODY	0
#define PTCACHE_TYPE_PARTICLES	1
#define PTCACHE_TYPE_CLOTH		2

/* PTCache read return code */
#define PTCACHE_READ_EXACT				1
#define PTCACHE_READ_INTERPOLATED		2
#define PTCACHE_READ_OLD				3

/* Structs */
struct Object;
struct Scene;
struct SoftBody;
struct ParticleSystem;
struct ClothModifierData;
struct PointCache;
struct ListBase;

/* temp structure for read/write */
typedef struct PTCacheData {
	int index;
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

	int totpoint, type;
	unsigned int data_types;

	struct PTCacheData data;
	void *cur[BPHYS_TOT_DATA];
} PTCacheFile;

typedef struct PTCacheID {
	struct PTCacheID *next, *prev;

	struct Scene *scene;
	struct Object *ob;
	void *calldata;
	int type;
	int stack_index;
	unsigned int data_types, info_types;

	int (*write_elem)(int index, void *calldata, void **data);
	void (*read_elem)(int index, void *calldata, void **data, float frs_sec, float cfra, float *old_data);
	void (*interpolate_elem)(int index, void *calldata, void **data, float frs_sec, float cfra, float cfra1, float cfra2, float *old_data);

	int (*totpoint)(void *calldata);
	int (*totwrite)(void *calldata);

	int (*write_header)(PTCacheFile *pf);
	int (*read_header)(PTCacheFile *pf);

	struct PointCache *cache;
	struct PointCache **cache_ptr;
	struct ListBase *ptcaches;
} PTCacheID;

typedef struct PTCacheBaker {
	struct Scene *scene;
	int bake;
	int render;
	int anim_init;
	int quick_step;
	struct PTCacheID *pid;
	int (*break_test)(void *data);
	void *break_data;
	void (*progressbar)(void *data, int num);
	void *progresscontext;
} PTCacheBaker;

/* Particle functions */
void BKE_ptcache_make_particle_key(struct ParticleKey *key, int index, void **data, float time);

/* Creating ID's */
void BKE_ptcache_id_from_softbody(PTCacheID *pid, struct Object *ob, struct SoftBody *sb);
void BKE_ptcache_id_from_particles(PTCacheID *pid, struct Object *ob, struct ParticleSystem *psys);
void BKE_ptcache_id_from_cloth(PTCacheID *pid, struct Object *ob, struct ClothModifierData *clmd);

void BKE_ptcache_ids_from_object(struct ListBase *lb, struct Object *ob);

/* Global funcs */
void BKE_ptcache_remove(void);

/* ID specific functions */
void	BKE_ptcache_id_clear(PTCacheID *id, int mode, int cfra);
int		BKE_ptcache_id_exist(PTCacheID *id, int cfra);
int		BKE_ptcache_id_reset(struct Scene *scene, PTCacheID *id, int mode);
void	BKE_ptcache_id_time(PTCacheID *pid, struct Scene *scene, float cfra, int *startframe, int *endframe, float *timescale);
int		BKE_ptcache_object_reset(struct Scene *scene, struct Object *ob, int mode);

void BKE_ptcache_update_info(PTCacheID *pid);

/* General cache reading/writing */
int		BKE_ptcache_data_size(int data_type);
void	BKE_ptcache_data_get(void **data, int type, int index, void *to);
void	BKE_ptcache_data_set(void **data, int type, void *from);
int		BKE_ptcache_read_cache(PTCacheID *pid, float cfra, float frs_sec);
int		BKE_ptcache_write_cache(PTCacheID *pid, int cfra);

/* Continue physics */
void BKE_ptcache_set_continue_physics(struct Scene *scene, int enable);
int BKE_ptcache_get_continue_physics(void);

/* Point Cache */
struct PointCache *BKE_ptcache_add(struct ListBase *ptcaches);
void BKE_ptache_free_mem(struct PointCache *cache);
void BKE_ptcache_free(struct PointCache *cache);
void BKE_ptcache_free_list(struct ListBase *ptcaches);
struct PointCache *BKE_ptcache_copy(struct PointCache *cache);

/* Baking */
void BKE_ptcache_quick_cache_all(struct Scene *scene);
void BKE_ptcache_make_cache(struct PTCacheBaker* baker);
void BKE_ptcache_disk_to_mem(struct PTCacheID *pid);
void BKE_ptcache_toggle_disk_cache(struct PTCacheID *pid);

void BKE_ptcache_load_external(struct PTCacheID *pid);

#endif
