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

/* Add the blendfile name after blendcache_ */
#define PTCACHE_EXT ".bphys"
#define PTCACHE_PATH "blendcache_"

/* File open options, for BKE_ptcache_file_open */
#define PTCACHE_FILE_READ	0
#define PTCACHE_FILE_WRITE	1

/* PTCacheID types */
#define PTCACHE_TYPE_SOFTBODY	0
#define PTCACHE_TYPE_PARTICLES	1
#define PTCACHE_TYPE_CLOTH	2
#define PTCACHE_TYPE_FLUIDSIM	3

/* Structs */
struct Object;
struct SoftBody;
struct ParticleSystem;
struct ClothModifierData;
struct PointCache;
struct ListBase;

typedef struct PTCacheFile {
	FILE *fp;
} PTCacheFile;

typedef struct PTCacheID {
	struct PTCacheID *next, *prev;

	struct Object *ob;
	void *data;
	int type;
	int stack_index;

	struct PointCache *cache;
} PTCacheID;

/* Creating ID's */
void BKE_ptcache_id_from_softbody(PTCacheID *pid, struct Object *ob, struct SoftBody *sb);
void BKE_ptcache_id_from_particles(PTCacheID *pid, struct Object *ob, struct ParticleSystem *psys);
void BKE_ptcache_id_from_cloth(PTCacheID *pid, struct Object *ob, struct ClothModifierData *clmd);
void BKE_ptcache_id_from_fluidsim(PTCacheID *pid, struct Object *ob, struct FluidsimModifierData *fluidmd);

void BKE_ptcache_ids_from_object(struct ListBase *lb, struct Object *ob);

/* Global funcs */
void BKE_ptcache_remove(void);

/* ID specific functions */
void	BKE_ptcache_id_clear(PTCacheID *id, int mode, int cfra);
int		BKE_ptcache_id_exist(PTCacheID *id, int cfra);
int		BKE_ptcache_id_reset(PTCacheID *id, int mode);
void	BKE_ptcache_id_time(PTCacheID *pid, float cfra, int *startframe, int *endframe, float *timescale);
int		BKE_ptcache_object_reset(struct Object *ob, int mode);

/* File reading/writing */
PTCacheFile	*BKE_ptcache_file_open(PTCacheID *id, int mode, int cfra);
void         BKE_ptcache_file_close(PTCacheFile *pf);
int          BKE_ptcache_file_read_floats(PTCacheFile *pf, float *f, int tot);
int          BKE_ptcache_file_write_floats(PTCacheFile *pf, float *f, int tot);

/* Continue physics */
void BKE_ptcache_set_continue_physics(int enable);
int BKE_ptcache_get_continue_physics(void);

/* Point Cache */
struct PointCache *BKE_ptcache_add(void);
void BKE_ptcache_free(struct PointCache *cache);
struct PointCache *BKE_ptcache_copy(struct PointCache *cache);

#endif
