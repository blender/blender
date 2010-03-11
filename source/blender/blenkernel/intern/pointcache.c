/**
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
* Contributor(s): Campbell Barton <ideasman42@gmail.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_cloth_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"

#include "PIL_time.h"

#include "WM_api.h"

#include "BKE_blender.h"
#include "BKE_cloth.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BIK_api.h"

/* both in intern */
#include "smoke_API.h"

#ifdef WITH_LZO
#include "minilzo.h"
#else
/* used for non-lzo cases */
#define LZO_OUT_LEN(size)     ((size) + (size) / 16 + 64 + 3)
#endif

#ifdef WITH_LZMA
#include "LzmaLib.h"
#endif

/* needed for directory lookup */
/* untitled blend's need getpid for a unique name */
#ifndef WIN32
  #include <dirent.h>
#include <unistd.h>
#else
#include <process.h>
  #include "BLI_winstuff.h"
#endif

#define PTCACHE_DATA_FROM(data, type, from)		if(data[type]) { memcpy(data[type], from, ptcache_data_size[type]); }
#define PTCACHE_DATA_TO(data, type, index, to)	if(data[type]) { memcpy(to, (char*)data[type] + (index ? index * ptcache_data_size[type] : 0), ptcache_data_size[type]); }

int ptcache_data_size[] = {	
		sizeof(int), // BPHYS_DATA_INDEX
		3 * sizeof(float), // BPHYS_DATA_LOCATION:
		3 * sizeof(float), // BPHYS_DATA_VELOCITY:
		4 * sizeof(float), // BPHYS_DATA_ROTATION:
		3 * sizeof(float), // BPHYS_DATA_AVELOCITY: /* also BPHYS_DATA_XCONST */
		sizeof(float), // BPHYS_DATA_SIZE:
		3 * sizeof(float), // BPHYS_DATA_TIMES:	
		sizeof(BoidData) // case BPHYS_DATA_BOIDS:
};

/* Common functions */
static int ptcache_read_basic_header(PTCacheFile *pf)
{
	int error=0;

	/* Custom functions should read these basic elements too! */
	if(!error && !fread(&pf->totpoint, sizeof(int), 1, pf->fp))
		error = 1;
	
	if(!error && !fread(&pf->data_types, sizeof(int), 1, pf->fp))
		error = 1;

	return !error;
}
static int ptcache_write_basic_header(PTCacheFile *pf)
{
	/* Custom functions should write these basic elements too! */
	if(!fwrite(&pf->totpoint, sizeof(int), 1, pf->fp))
		return 0;
	
	if(!fwrite(&pf->data_types, sizeof(int), 1, pf->fp))
		return 0;

	return 1;
}
/* Softbody functions */
static int ptcache_write_softbody(int index, void *soft_v, void **data, int cfra)
{
	SoftBody *soft= soft_v;
	BodyPoint *bp = soft->bpoint + index;

	PTCACHE_DATA_FROM(data, BPHYS_DATA_LOCATION, bp->pos);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_VELOCITY, bp->vec);

	return 1;
}
static void ptcache_read_softbody(int index, void *soft_v, void **data, float frs_sec, float cfra, float *old_data)
{
	SoftBody *soft= soft_v;
	BodyPoint *bp = soft->bpoint + index;

	if(old_data) {
		memcpy(bp->pos, data, 3 * sizeof(float));
		memcpy(bp->vec, data + 3, 3 * sizeof(float));
	}
	else {
		PTCACHE_DATA_TO(data, BPHYS_DATA_LOCATION, 0, bp->pos);
		PTCACHE_DATA_TO(data, BPHYS_DATA_VELOCITY, 0, bp->vec);
	}
}
static void ptcache_interpolate_softbody(int index, void *soft_v, void **data, float frs_sec, float cfra, float cfra1, float cfra2, float *old_data)
{
	SoftBody *soft= soft_v;
	BodyPoint *bp = soft->bpoint + index;
	ParticleKey keys[4];
	float dfra;

	if(cfra1 == cfra2)
		return;

	VECCOPY(keys[1].co, bp->pos);
	VECCOPY(keys[1].vel, bp->vec);

	if(old_data) {
		memcpy(keys[2].co, old_data, 3 * sizeof(float));
		memcpy(keys[2].vel, old_data + 3, 3 * sizeof(float));
	}
	else
		BKE_ptcache_make_particle_key(keys+2, 0, data, cfra2);

	dfra = cfra2 - cfra1;

	mul_v3_fl(keys[1].vel, dfra);
	mul_v3_fl(keys[2].vel, dfra);

	psys_interpolate_particle(-1, keys, (cfra - cfra1) / dfra, keys, 1);

	mul_v3_fl(keys->vel, 1.0f / dfra);

	VECCOPY(bp->pos, keys->co);
	VECCOPY(bp->vec, keys->vel);
}
static int ptcache_totpoint_softbody(void *soft_v, int cfra)
{
	SoftBody *soft= soft_v;
	return soft->totpoint;
}
/* Particle functions */
static int ptcache_write_particle(int index, void *psys_v, void **data, int cfra)
{
	ParticleSystem *psys= psys_v;
	ParticleData *pa = psys->particles + index;
	BoidParticle *boid = (psys->part->phystype == PART_PHYS_BOIDS) ? pa->boid : NULL;
	float times[3] = {pa->time, pa->dietime, pa->lifetime};
	int step = psys->pointcache->step;

	/* No need to store unborn or died particles outside cache step bounds */
	if(data[BPHYS_DATA_INDEX] && (cfra < pa->time - step || cfra > pa->dietime + step))
		return 0;
	
	PTCACHE_DATA_FROM(data, BPHYS_DATA_INDEX, &index);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_LOCATION, pa->state.co);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_VELOCITY, pa->state.vel);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_ROTATION, pa->state.rot);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_AVELOCITY, pa->state.ave);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_SIZE, &pa->size);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_TIMES, times);

	if(boid)
		PTCACHE_DATA_FROM(data, BPHYS_DATA_BOIDS, &boid->data);

	/* return flag 1+1=2 for newly born particles to copy exact birth location to previously cached frame */
	return 1 + (pa->state.time >= pa->time && pa->prev_state.time <= pa->time);
}
void BKE_ptcache_make_particle_key(ParticleKey *key, int index, void **data, float time)
{
	PTCACHE_DATA_TO(data, BPHYS_DATA_LOCATION, index, key->co);
	PTCACHE_DATA_TO(data, BPHYS_DATA_VELOCITY, index, key->vel);
	PTCACHE_DATA_TO(data, BPHYS_DATA_ROTATION, index, key->rot);
	PTCACHE_DATA_TO(data, BPHYS_DATA_AVELOCITY, index, key->ave);
	key->time = time;
}
static void ptcache_read_particle(int index, void *psys_v, void **data, float frs_sec, float cfra, float *old_data)
{
	ParticleSystem *psys= psys_v;
	ParticleData *pa;
	BoidParticle *boid;

	if(index >= psys->totpart)
		return;

	pa = psys->particles + index;
	boid = (psys->part->phystype == PART_PHYS_BOIDS) ? pa->boid : NULL;

	if(cfra > pa->state.time)
		memcpy(&pa->prev_state, &pa->state, sizeof(ParticleKey));

	if(old_data){
		/* old format cache */
		memcpy(&pa->state, old_data, sizeof(ParticleKey));
		return;
	}

	BKE_ptcache_make_particle_key(&pa->state, 0, data, cfra);

	/* set frames cached before birth to birth time */
	if(cfra < pa->time)
		pa->state.time = pa->time;

	if(data[BPHYS_DATA_SIZE])
		PTCACHE_DATA_TO(data, BPHYS_DATA_SIZE, 0, &pa->size);
	
	if(data[BPHYS_DATA_TIMES]) {
		float times[3];
		PTCACHE_DATA_TO(data, BPHYS_DATA_TIMES, 0, &times);
		pa->time = times[0];
		pa->dietime = times[1];
		pa->lifetime = times[2];
	}

	if(boid)
		PTCACHE_DATA_TO(data, BPHYS_DATA_BOIDS, 0, &boid->data);

	/* determine velocity from previous location */
	if(data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_VELOCITY]) {
		if(cfra > pa->prev_state.time) {
			sub_v3_v3v3(pa->state.vel, pa->state.co, pa->prev_state.co);
			mul_v3_fl(pa->state.vel, (cfra - pa->prev_state.time) / frs_sec);
		}
		else {
			sub_v3_v3v3(pa->state.vel, pa->prev_state.co, pa->state.co);
			mul_v3_fl(pa->state.vel, (pa->prev_state.time - cfra) / frs_sec);
		}
	}

	/* determine rotation from velocity */
	if(data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_ROTATION]) {
		vec_to_quat( pa->state.rot,pa->state.vel, OB_NEGX, OB_POSZ);
	}
}
static void ptcache_interpolate_particle(int index, void *psys_v, void **data, float frs_sec, float cfra, float cfra1, float cfra2, float *old_data)
{
	ParticleSystem *psys= psys_v;
	ParticleData *pa;
	ParticleKey keys[4];
	float dfra;

	if(index >= psys->totpart)
		return;

	pa = psys->particles + index;

	/* particle wasn't read from first cache so can't interpolate */
	if((int)cfra1 < pa->time - psys->pointcache->step || (int)cfra1 > pa->dietime + psys->pointcache->step)
		return;

	cfra = MIN2(cfra, pa->dietime);
	cfra1 = MIN2(cfra1, pa->dietime);
	cfra2 = MIN2(cfra2, pa->dietime);

	if(cfra1 == cfra2)
		return;

	memcpy(keys+1, &pa->state, sizeof(ParticleKey));
	if(old_data)
		memcpy(keys+2, old_data, sizeof(ParticleKey));
	else
		BKE_ptcache_make_particle_key(keys+2, 0, data, cfra2);

	/* determine velocity from previous location */
	if(data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_VELOCITY]) {
		if(keys[1].time > keys[2].time) {
			sub_v3_v3v3(keys[2].vel, keys[1].co, keys[2].co);
			mul_v3_fl(keys[2].vel, (keys[1].time - keys[2].time) / frs_sec);
		}
		else {
			sub_v3_v3v3(keys[2].vel, keys[2].co, keys[1].co);
			mul_v3_fl(keys[2].vel, (keys[2].time - keys[1].time) / frs_sec);
		}
	}

	/* determine rotation from velocity */
	if(data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_ROTATION]) {
		vec_to_quat( keys[2].rot,keys[2].vel, OB_NEGX, OB_POSZ);
	}

	if(cfra > pa->time)
		cfra1 = MAX2(cfra1, pa->time);

	dfra = cfra2 - cfra1;

	mul_v3_fl(keys[1].vel, dfra / frs_sec);
	mul_v3_fl(keys[2].vel, dfra / frs_sec);

	psys_interpolate_particle(-1, keys, (cfra - cfra1) / dfra, &pa->state, 1);
	interp_qt_qtqt(pa->state.rot, keys[1].rot, keys[2].rot, (cfra - cfra1) / dfra);

	mul_v3_fl(pa->state.vel, frs_sec / dfra);

	pa->state.time = cfra;
}

static int ptcache_totpoint_particle(void *psys_v, int cfra)
{
	ParticleSystem *psys = psys_v;
	return psys->totpart;
}
static int ptcache_totwrite_particle(void *psys_v, int cfra)
{
	ParticleSystem *psys = psys_v;
	ParticleData *pa= psys->particles;
	int p, step = psys->pointcache->step;
	int totwrite = 0;

	for(p=0; p<psys->totpart; p++,pa++)
		totwrite += (cfra >= pa->time - step && cfra <= pa->dietime + step);

	return totwrite;
}

//static int ptcache_write_particle_stream(PTCacheFile *pf, PTCacheMem *pm, void *psys_v)
//{
//	ParticleSystem *psys= psys_v;
//	ParticleData *pa = psys->particles;
//	BoidParticle *boid = NULL;
//	float times[3];
//	int i = 0;
//
//	if(!pf && !pm)
//		return 0;
//
//	for(i=0; i<psys->totpart; i++, pa++) {
//
//		if(data[BPHYS_DATA_INDEX]) {
//			int step = psys->pointcache->step;
//			/* No need to store unborn or died particles */
//			if(pa->time - step > pa->state.time || pa->dietime + step < pa->state.time)
//				continue;
//		}
//
//		times[0] = pa->time;
//		times[1] = pa->dietime;
//		times[2] = pa->lifetime;
//		
//		PTCACHE_DATA_FROM(data, BPHYS_DATA_INDEX, &index);
//		PTCACHE_DATA_FROM(data, BPHYS_DATA_LOCATION, pa->state.co);
//		PTCACHE_DATA_FROM(data, BPHYS_DATA_VELOCITY, pa->state.vel);
//		PTCACHE_DATA_FROM(data, BPHYS_DATA_ROTATION, pa->state.rot);
//		PTCACHE_DATA_FROM(data, BPHYS_DATA_AVELOCITY, pa->state.ave);
//		PTCACHE_DATA_FROM(data, BPHYS_DATA_SIZE, &pa->size);
//		PTCACHE_DATA_FROM(data, BPHYS_DATA_TIMES, times);
//
//		boid = (psys->part->phystype == PART_PHYS_BOIDS) ? pa->boid : NULL;
//		if(boid)
//			PTCACHE_DATA_FROM(data, BPHYS_DATA_BOIDS, &boid->data);
//
//		if(pf && !ptcache_file_write_data(pf))
//			return 0;
//
//		if(pm)
//			BKE_ptcache_mem_incr_pointers(pm);
//	}
//
//	return 1;
//}
//static void ptcache_read_particle_stream(PTCacheFile *pf, PTCacheMem *pm, void *psys_v, void **data, float frs_sec, float cfra, float *old_data)
//{
//	ParticleSystem *psys= psys_v;
//	ParticleData *pa = psys->particles + index;
//	BoidParticle *boid = (psys->part->phystype == PART_PHYS_BOIDS) ? pa->boid : NULL;
//
//	if(cfra > pa->state.time)
//		memcpy(&pa->prev_state, &pa->state, sizeof(ParticleKey));
//
//	if(old_data){
//		/* old format cache */
//		memcpy(&pa->state, old_data, sizeof(ParticleKey));
//		return;
//	}
//
//	BKE_ptcache_make_particle_key(&pa->state, 0, data, cfra);
//
//	if(data[BPHYS_DATA_SIZE])
//		PTCACHE_DATA_TO(data, BPHYS_DATA_SIZE, 0, &pa->size);
//	
//	if(data[BPHYS_DATA_TIMES]) {
//		float times[3];
//		PTCACHE_DATA_TO(data, BPHYS_DATA_TIMES, 0, &times);
//		pa->time = times[0];
//		pa->dietime = times[1];
//		pa->lifetime = times[2];
//	}
//
//	if(boid)
//		PTCACHE_DATA_TO(data, BPHYS_DATA_BOIDS, 0, &boid->data);
//
//	/* determine velocity from previous location */
//	if(data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_VELOCITY]) {
//		if(cfra > pa->prev_state.time) {
//			sub_v3_v3v3(pa->state.vel, pa->state.co, pa->prev_state.co);
//			mul_v3_fl(pa->state.vel, (cfra - pa->prev_state.time) / frs_sec);
//		}
//		else {
//			sub_v3_v3v3(pa->state.vel, pa->prev_state.co, pa->state.co);
//			mul_v3_fl(pa->state.vel, (pa->prev_state.time - cfra) / frs_sec);
//		}
//	}
//
//	/* determine rotation from velocity */
//	if(data[BPHYS_DATA_LOCATION] && !data[BPHYS_DATA_ROTATION]) {
//		vec_to_quat( pa->state.rot,pa->state.vel, OB_POSX, OB_POSZ);
//	}
//}
//static void ptcache_interpolate_particle_stream(int index, void *psys_v, void **data, float frs_sec, float cfra, float cfra1, float cfra2, float *old_data)
//{
//	ParticleSystem *psys= psys_v;
//	ParticleData *pa = psys->particles + index;
//	ParticleKey keys[4];
//	float dfra;
//
//	cfra = MIN2(cfra, pa->dietime);
//	cfra1 = MIN2(cfra1, pa->dietime);
//	cfra2 = MIN2(cfra2, pa->dietime);
//
//	if(cfra1 == cfra2)
//		return;
//
//	memcpy(keys+1, &pa->state, sizeof(ParticleKey));
//	if(old_data)
//		memcpy(keys+2, old_data, sizeof(ParticleKey));
//	else
//		BKE_ptcache_make_particle_key(keys+2, 0, data, cfra2);
//
//	dfra = cfra2 - cfra1;
//
//	mul_v3_fl(keys[1].vel, dfra / frs_sec);
//	mul_v3_fl(keys[2].vel, dfra / frs_sec);
//
//	psys_interpolate_particle(-1, keys, (cfra - cfra1) / dfra, &pa->state, 1);
//	interp_qt_qtqt(pa->state.rot, keys[1].rot,keys[2].rot, (cfra - cfra1) / dfra);
//
//	mul_v3_fl(pa->state.vel, frs_sec / dfra);
//
//	pa->state.time = cfra;
//}
//
/* Cloth functions */
static int ptcache_write_cloth(int index, void *cloth_v, void **data, int cfra)
{
	ClothModifierData *clmd= cloth_v;
	Cloth *cloth= clmd->clothObject;
	ClothVertex *vert = cloth->verts + index;

	PTCACHE_DATA_FROM(data, BPHYS_DATA_LOCATION, vert->x);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_VELOCITY, vert->v);
	PTCACHE_DATA_FROM(data, BPHYS_DATA_XCONST, vert->xconst);

	return 1;
}
static void ptcache_read_cloth(int index, void *cloth_v, void **data, float frs_sec, float cfra, float *old_data)
{
	ClothModifierData *clmd= cloth_v;
	Cloth *cloth= clmd->clothObject;
	ClothVertex *vert = cloth->verts + index;
	
	if(old_data) {
		memcpy(vert->x, data, 3 * sizeof(float));
		memcpy(vert->xconst, data + 3, 3 * sizeof(float));
		memcpy(vert->v, data + 6, 3 * sizeof(float));
	}
	else {
		PTCACHE_DATA_TO(data, BPHYS_DATA_LOCATION, 0, vert->x);
		PTCACHE_DATA_TO(data, BPHYS_DATA_VELOCITY, 0, vert->v);
		PTCACHE_DATA_TO(data, BPHYS_DATA_XCONST, 0, vert->xconst);
	}
}
static void ptcache_interpolate_cloth(int index, void *cloth_v, void **data, float frs_sec, float cfra, float cfra1, float cfra2, float *old_data)
{
	ClothModifierData *clmd= cloth_v;
	Cloth *cloth= clmd->clothObject;
	ClothVertex *vert = cloth->verts + index;
	ParticleKey keys[4];
	float dfra;

	if(cfra1 == cfra2)
		return;

	VECCOPY(keys[1].co, vert->x);
	VECCOPY(keys[1].vel, vert->v);

	if(old_data) {
		memcpy(keys[2].co, old_data, 3 * sizeof(float));
		memcpy(keys[2].vel, old_data + 6, 3 * sizeof(float));
	}
	else
		BKE_ptcache_make_particle_key(keys+2, 0, data, cfra2);

	dfra = cfra2 - cfra1;

	mul_v3_fl(keys[1].vel, dfra);
	mul_v3_fl(keys[2].vel, dfra);

	psys_interpolate_particle(-1, keys, (cfra - cfra1) / dfra, keys, 1);

	mul_v3_fl(keys->vel, 1.0f / dfra);

	VECCOPY(vert->x, keys->co);
	VECCOPY(vert->v, keys->vel);

	/* should vert->xconst be interpolated somehow too? - jahka */
}

static int ptcache_totpoint_cloth(void *cloth_v, int cfra)
{
	ClothModifierData *clmd= cloth_v;
	return clmd->clothObject->numverts;
}

/* Creating ID's */
void BKE_ptcache_id_from_softbody(PTCacheID *pid, Object *ob, SoftBody *sb)
{
	memset(pid, 0, sizeof(PTCacheID));

	pid->ob= ob;
	pid->calldata= sb;
	pid->type= PTCACHE_TYPE_SOFTBODY;
	pid->cache= sb->pointcache;
	pid->cache_ptr= &sb->pointcache;
	pid->ptcaches= &sb->ptcaches;
	pid->totpoint= pid->totwrite= ptcache_totpoint_softbody;

	pid->write_elem= ptcache_write_softbody;
	pid->write_stream = NULL;
	pid->read_stream = NULL;
	pid->read_elem= ptcache_read_softbody;
	pid->interpolate_elem= ptcache_interpolate_softbody;

	pid->write_header= ptcache_write_basic_header;
	pid->read_header= ptcache_read_basic_header;

	pid->data_types= (1<<BPHYS_DATA_LOCATION) | (1<<BPHYS_DATA_VELOCITY);
	pid->info_types= 0;

	pid->stack_index = pid->cache->index;
}

void BKE_ptcache_id_from_particles(PTCacheID *pid, Object *ob, ParticleSystem *psys)
{
	memset(pid, 0, sizeof(PTCacheID));

	pid->ob= ob;
	pid->calldata= psys;
	pid->type= PTCACHE_TYPE_PARTICLES;
	pid->stack_index= psys->pointcache->index;
	pid->cache= psys->pointcache;
	pid->cache_ptr= &psys->pointcache;
	pid->ptcaches= &psys->ptcaches;

	if(psys->part->type != PART_HAIR)
		pid->flag |= PTCACHE_VEL_PER_SEC;

	pid->write_elem= ptcache_write_particle;
	pid->write_stream = NULL;
	pid->read_stream = NULL;
	pid->read_elem= ptcache_read_particle;
	pid->interpolate_elem= ptcache_interpolate_particle;

	pid->totpoint= ptcache_totpoint_particle;
	pid->totwrite= ptcache_totwrite_particle;

	pid->write_header= ptcache_write_basic_header;
	pid->read_header= ptcache_read_basic_header;

	pid->data_types= (1<<BPHYS_DATA_LOCATION) | (1<<BPHYS_DATA_VELOCITY) | (1<<BPHYS_DATA_INDEX);

	if(psys->part->phystype == PART_PHYS_BOIDS)
		pid->data_types|= (1<<BPHYS_DATA_AVELOCITY) | (1<<BPHYS_DATA_ROTATION) | (1<<BPHYS_DATA_BOIDS);

	if(psys->part->rotmode!=PART_ROT_VEL
		|| psys->part->avemode!=PART_AVE_SPIN || psys->part->avefac!=0.0f)
		pid->data_types|= (1<<BPHYS_DATA_AVELOCITY) | (1<<BPHYS_DATA_ROTATION);

	if(psys->part->flag & PART_ROT_DYN)
		pid->data_types|= (1<<BPHYS_DATA_ROTATION);

	pid->info_types= (1<<BPHYS_DATA_TIMES);
}

/* Smoke functions */
static int ptcache_totpoint_smoke(void *smoke_v, int cfra)
{
	SmokeModifierData *smd= (SmokeModifierData *)smoke_v;
	SmokeDomainSettings *sds = smd->domain;
	
	if(sds->fluid) {
		return sds->res[0]*sds->res[1]*sds->res[2];
	}
	else
		return 0;
}

/* Smoke functions */
static int ptcache_totpoint_smoke_turbulence(void *smoke_v, int cfra)
{
	SmokeModifierData *smd= (SmokeModifierData *)smoke_v;
	SmokeDomainSettings *sds = smd->domain;
	
	if(sds->wt) {
		return sds->res_wt[0]*sds->res_wt[1]*sds->res_wt[2];
	}
	else
		return 0;
}

// forward decleration
static int ptcache_file_write(PTCacheFile *pf, void *f, size_t tot, int size);

static int ptcache_compress_write(PTCacheFile *pf, unsigned char *in, unsigned int in_len, unsigned char *out, int mode)
{
	int r = 0;
	unsigned char compressed = 0;
	unsigned int out_len= 0;
	unsigned char *props = MEM_callocN(16*sizeof(char), "tmp");
	size_t sizeOfIt = 5;

#ifdef WITH_LZO
	out_len= LZO_OUT_LEN(in_len);
	if(mode == 1) {
		LZO_HEAP_ALLOC(wrkmem, LZO1X_MEM_COMPRESS);
		
		r = lzo1x_1_compress(in, (lzo_uint)in_len, out, (lzo_uint *)&out_len, wrkmem);	
		if (!(r == LZO_E_OK) || (out_len >= in_len))
			compressed = 0;
		else
			compressed = 1;
	}
#endif
#ifdef WITH_LZMA
	if(mode == 2) {
		
		r = LzmaCompress(out, (size_t *)&out_len, in, in_len,//assume sizeof(char)==1....
						props, &sizeOfIt, 5, 1 << 24, 3, 0, 2, 32, 2);

		if(!(r == SZ_OK) || (out_len >= in_len))
			compressed = 0;
		else
			compressed = 2;
	}
#endif
	
	ptcache_file_write(pf, &compressed, 1, sizeof(unsigned char));
	if(compressed) {
		ptcache_file_write(pf, &out_len, 1, sizeof(unsigned int));
		ptcache_file_write(pf, out, out_len, sizeof(unsigned char));
	}
	else
		ptcache_file_write(pf, in, in_len, sizeof(unsigned char));

	if(compressed == 2)
	{
		ptcache_file_write(pf, &sizeOfIt, 1, sizeof(unsigned int));
		ptcache_file_write(pf, props, sizeOfIt, sizeof(unsigned char));
	}

	MEM_freeN(props);

	return r;
}

static int ptcache_write_smoke(PTCacheFile *pf, void *smoke_v)
{	
	SmokeModifierData *smd= (SmokeModifierData *)smoke_v;
	SmokeDomainSettings *sds = smd->domain;
	
	if(sds->fluid) {
		size_t res = sds->res[0]*sds->res[1]*sds->res[2];
		float dt, dx, *dens, *densold, *heat, *heatold, *vx, *vy, *vz, *vxold, *vyold, *vzold;
		unsigned char *obstacles;
		unsigned int in_len = sizeof(float)*(unsigned int)res;
		unsigned char *out = (unsigned char *)MEM_callocN(LZO_OUT_LEN(in_len)*4, "pointcache_lzo_buffer");
		//int mode = res >= 1000000 ? 2 : 1;
		int mode=1;		// light
		if (sds->cache_comp == SM_CACHE_HEAVY) mode=2;	// heavy

		smoke_export(sds->fluid, &dt, &dx, &dens, &densold, &heat, &heatold, &vx, &vy, &vz, &vxold, &vyold, &vzold, &obstacles);

		ptcache_compress_write(pf, (unsigned char *)sds->shadow, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)dens, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)densold, in_len, out, mode);	
		ptcache_compress_write(pf, (unsigned char *)heat, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)heatold, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)vx, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)vy, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)vz, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)vxold, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)vyold, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)vzold, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)obstacles, (unsigned int)res, out, mode);
		ptcache_file_write(pf, &dt, 1, sizeof(float));
		ptcache_file_write(pf, &dx, 1, sizeof(float));

		MEM_freeN(out);
		
		return 1;
	}
	return 0;
}

static int ptcache_write_smoke_turbulence(PTCacheFile *pf, void *smoke_v)
{
	SmokeModifierData *smd= (SmokeModifierData *)smoke_v;
	SmokeDomainSettings *sds = smd->domain;
	
	if(sds->wt) {
		int res_big_array[3];
		int res_big;
		int res = sds->res[0]*sds->res[1]*sds->res[2];
		float *dens, *densold, *tcu, *tcv, *tcw;
		unsigned int in_len = sizeof(float)*(unsigned int)res;
		unsigned int in_len_big;
		unsigned char *out;
		int mode;

		smoke_turbulence_get_res(sds->wt, res_big_array);
		res_big = res_big_array[0]*res_big_array[1]*res_big_array[2];
		//mode =  res_big >= 1000000 ? 2 : 1;
		mode = 1;	// light
		if (sds->cache_high_comp == SM_CACHE_HEAVY) mode=2;	// heavy

		in_len_big = sizeof(float) * (unsigned int)res_big;

		smoke_turbulence_export(sds->wt, &dens, &densold, &tcu, &tcv, &tcw);

		out = (unsigned char *)MEM_callocN(LZO_OUT_LEN(in_len_big), "pointcache_lzo_buffer");
		ptcache_compress_write(pf, (unsigned char *)dens, in_len_big, out, mode);
		ptcache_compress_write(pf, (unsigned char *)densold, in_len_big, out, mode);	
		MEM_freeN(out);

		out = (unsigned char *)MEM_callocN(LZO_OUT_LEN(in_len), "pointcache_lzo_buffer");
		ptcache_compress_write(pf, (unsigned char *)tcu, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)tcv, in_len, out, mode);
		ptcache_compress_write(pf, (unsigned char *)tcw, in_len, out, mode);
		MEM_freeN(out);
		
		return 1;
	}
	return 0;
}

// forward decleration
static int ptcache_file_read(PTCacheFile *pf, void *f, size_t tot, int size);

static int ptcache_compress_read(PTCacheFile *pf, unsigned char *result, unsigned int len)
{
	int r = 0;
	unsigned char compressed = 0;
	unsigned int in_len;
	unsigned int out_len = len;
	unsigned char *in;
	unsigned char *props = MEM_callocN(16*sizeof(char), "tmp");
	size_t sizeOfIt = 5;

	ptcache_file_read(pf, &compressed, 1, sizeof(unsigned char));
	if(compressed) {
		ptcache_file_read(pf, &in_len, 1, sizeof(unsigned int));
		in = (unsigned char *)MEM_callocN(sizeof(unsigned char)*in_len, "pointcache_compressed_buffer");
		ptcache_file_read(pf, in, in_len, sizeof(unsigned char));

#ifdef WITH_LZO
		if(compressed == 1)
				r = lzo1x_decompress(in, (lzo_uint)in_len, result, (lzo_uint *)&out_len, NULL);
#endif
#ifdef WITH_LZMA
		if(compressed == 2)
		{
			size_t leni = in_len, leno = out_len;
			ptcache_file_read(pf, &sizeOfIt, 1, sizeof(unsigned int));
			ptcache_file_read(pf, props, sizeOfIt, sizeof(unsigned char));
			r = LzmaUncompress(result, &leno, in, &leni, props, sizeOfIt);
		}
#endif
		MEM_freeN(in);
	}
	else {
		ptcache_file_read(pf, result, len, sizeof(unsigned char));
	}

	MEM_freeN(props);

	return r;
}

static void ptcache_read_smoke(PTCacheFile *pf, void *smoke_v)
{
	SmokeModifierData *smd= (SmokeModifierData *)smoke_v;
	SmokeDomainSettings *sds = smd->domain;
	
	if(sds->fluid) {
		size_t res = sds->res[0]*sds->res[1]*sds->res[2];
		float dt, dx, *dens, *densold, *heat, *heatold, *vx, *vy, *vz, *vxold, *vyold, *vzold;
		unsigned char *obstacles;
		unsigned int out_len = (unsigned int)res * sizeof(float);
		
		smoke_export(sds->fluid, &dt, &dx, &dens, &densold, &heat, &heatold, &vx, &vy, &vz, &vxold, &vyold, &vzold, &obstacles);

		ptcache_compress_read(pf, (unsigned char *)sds->shadow, out_len);
		ptcache_compress_read(pf, (unsigned char*)dens, out_len);
		ptcache_compress_read(pf, (unsigned char*)densold, out_len);
		ptcache_compress_read(pf, (unsigned char*)heat, out_len);
		ptcache_compress_read(pf, (unsigned char*)heatold, out_len);
		ptcache_compress_read(pf, (unsigned char*)vx, out_len);
		ptcache_compress_read(pf, (unsigned char*)vy, out_len);
		ptcache_compress_read(pf, (unsigned char*)vz, out_len);
		ptcache_compress_read(pf, (unsigned char*)vxold, out_len);
		ptcache_compress_read(pf, (unsigned char*)vyold, out_len);
		ptcache_compress_read(pf, (unsigned char*)vzold, out_len);
		ptcache_compress_read(pf, (unsigned char*)obstacles, (unsigned int)res);
		ptcache_file_read(pf, &dt, 1, sizeof(float));
		ptcache_file_read(pf, &dx, 1, sizeof(float));
	}
}

static void ptcache_read_smoke_turbulence(PTCacheFile *pf, void *smoke_v)
{
	SmokeModifierData *smd= (SmokeModifierData *)smoke_v;
	SmokeDomainSettings *sds = smd->domain;
	
	if(sds->fluid) {
		int res = sds->res[0]*sds->res[1]*sds->res[2];
		int res_big, res_big_array[3];
		float *dens, *densold, *tcu, *tcv, *tcw;
		unsigned int out_len = sizeof(float)*(unsigned int)res;
		unsigned int out_len_big;

		smoke_turbulence_get_res(sds->wt, res_big_array);
		res_big = res_big_array[0]*res_big_array[1]*res_big_array[2];
		out_len_big = sizeof(float) * (unsigned int)res_big;

		smoke_turbulence_export(sds->wt, &dens, &densold, &tcu, &tcv, &tcw);

		ptcache_compress_read(pf, (unsigned char*)dens, out_len_big);
		ptcache_compress_read(pf, (unsigned char*)densold, out_len_big);

		ptcache_compress_read(pf, (unsigned char*)tcu, out_len);
		ptcache_compress_read(pf, (unsigned char*)tcv, out_len);
		ptcache_compress_read(pf, (unsigned char*)tcw, out_len);		
	}
}

void BKE_ptcache_id_from_smoke(PTCacheID *pid, struct Object *ob, struct SmokeModifierData *smd)
{
	SmokeDomainSettings *sds = smd->domain;

	memset(pid, 0, sizeof(PTCacheID));

	pid->ob= ob;
	pid->calldata= smd;
	
	pid->type= PTCACHE_TYPE_SMOKE_DOMAIN;
	pid->stack_index= sds->point_cache[0]->index;

	pid->cache= sds->point_cache[0];
	pid->cache_ptr= &(sds->point_cache[0]);
	pid->ptcaches= &(sds->ptcaches[0]);

	pid->totpoint= pid->totwrite= ptcache_totpoint_smoke;

	pid->write_elem= NULL;
	pid->read_elem= NULL;

	pid->read_stream = ptcache_read_smoke;
	pid->write_stream = ptcache_write_smoke;
	
	pid->interpolate_elem= NULL;

	pid->write_header= ptcache_write_basic_header;
	pid->read_header= ptcache_read_basic_header;

	pid->data_types= (1<<BPHYS_DATA_LOCATION); // bogus values to make pointcache happy
	pid->info_types= 0;
}

void BKE_ptcache_id_from_smoke_turbulence(PTCacheID *pid, struct Object *ob, struct SmokeModifierData *smd)
{
	SmokeDomainSettings *sds = smd->domain;

	memset(pid, 0, sizeof(PTCacheID));

	pid->ob= ob;
	pid->calldata= smd;
	
	pid->type= PTCACHE_TYPE_SMOKE_HIGHRES;
	pid->stack_index= sds->point_cache[1]->index;

	pid->cache= sds->point_cache[1];
	pid->cache_ptr= &sds->point_cache[1];
	pid->ptcaches= &sds->ptcaches[1];

	pid->totpoint= pid->totwrite= ptcache_totpoint_smoke_turbulence;

	pid->write_elem= NULL;
	pid->read_elem= NULL;

	pid->read_stream = ptcache_read_smoke_turbulence;
	pid->write_stream = ptcache_write_smoke_turbulence;
	
	pid->interpolate_elem= NULL;

	pid->write_header= ptcache_write_basic_header;
	pid->read_header= ptcache_read_basic_header;

	pid->data_types= (1<<BPHYS_DATA_LOCATION); // bogus values tot make pointcache happy
	pid->info_types= 0;
}

void BKE_ptcache_id_from_cloth(PTCacheID *pid, Object *ob, ClothModifierData *clmd)
{
	memset(pid, 0, sizeof(PTCacheID));

	pid->ob= ob;
	pid->calldata= clmd;
	pid->type= PTCACHE_TYPE_CLOTH;
	pid->stack_index= clmd->point_cache->index;
	pid->cache= clmd->point_cache;
	pid->cache_ptr= &clmd->point_cache;
	pid->ptcaches= &clmd->ptcaches;
	pid->totpoint= pid->totwrite= ptcache_totpoint_cloth;

	pid->write_elem= ptcache_write_cloth;
	pid->write_stream = NULL;
	pid->read_stream = NULL;
	pid->read_elem= ptcache_read_cloth;
	pid->interpolate_elem= ptcache_interpolate_cloth;

	pid->write_header= ptcache_write_basic_header;
	pid->read_header= ptcache_read_basic_header;

	pid->data_types= (1<<BPHYS_DATA_LOCATION) | (1<<BPHYS_DATA_VELOCITY) | (1<<BPHYS_DATA_XCONST);
	pid->info_types= 0;
}

void BKE_ptcache_ids_from_object(ListBase *lb, Object *ob)
{
	PTCacheID *pid;
	ParticleSystem *psys;
	ModifierData *md;

	lb->first= lb->last= NULL;

	if(ob->soft) {
		pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
		BKE_ptcache_id_from_softbody(pid, ob, ob->soft);
		BLI_addtail(lb, pid);
	}

	for(psys=ob->particlesystem.first; psys; psys=psys->next) {
		if(psys->part) {
			pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
			BKE_ptcache_id_from_particles(pid, ob, psys);
			BLI_addtail(lb, pid);
		}
	}

	for(md=ob->modifiers.first; md; md=md->next) {
		if(md->type == eModifierType_Cloth) {
			pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
			BKE_ptcache_id_from_cloth(pid, ob, (ClothModifierData*)md);
			BLI_addtail(lb, pid);
		}
		if(md->type == eModifierType_Smoke) {
			SmokeModifierData *smd = (SmokeModifierData *)md;
			if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
			{
				pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
				BKE_ptcache_id_from_smoke(pid, ob, (SmokeModifierData*)md);
				BLI_addtail(lb, pid);

				pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
				BKE_ptcache_id_from_smoke_turbulence(pid, ob, (SmokeModifierData*)md);
				BLI_addtail(lb, pid);
			}
		}
	}
}


/* File handling */

/*	Takes an Object ID and returns a unique name
	- id: object id
	- cfra: frame for the cache, can be negative
	- stack_index: index in the modifier stack. we can have cache for more then one stack_index
*/

#define MAX_PTCACHE_PATH FILE_MAX
#define MAX_PTCACHE_FILE ((FILE_MAXDIR+FILE_MAXFILE)*2)

static int ptcache_path(PTCacheID *pid, char *filename)
{
	Library *lib;
	size_t i;

	lib= (pid)? pid->ob->id.lib: NULL;

	if(pid->cache->flag & PTCACHE_EXTERNAL) {
		strcpy(filename, pid->cache->path);
		return BLI_add_slash(filename); /* new strlen() */
	}
	else if (G.relbase_valid || lib) {
		char file[MAX_PTCACHE_PATH]; /* we dont want the dir, only the file */
		char *blendfilename;

		blendfilename= (lib)? lib->filename: G.sce;

		BLI_split_dirfile(blendfilename, NULL, file);
		i = strlen(file);
		
		/* remove .blend */
		if (i > 6)
			file[i-6] = '\0';
		
		snprintf(filename, MAX_PTCACHE_PATH, "//"PTCACHE_PATH"%s", file); /* add blend file name to pointcache dir */
		BLI_path_abs(filename, blendfilename);
		return BLI_add_slash(filename); /* new strlen() */
	}
	
	/* use the temp path. this is weak but better then not using point cache at all */
	/* btempdir is assumed to exist and ALWAYS has a trailing slash */
	snprintf(filename, MAX_PTCACHE_PATH, "%s"PTCACHE_PATH"%d", btempdir, abs(getpid()));
	
	return BLI_add_slash(filename); /* new strlen() */
}

static int BKE_ptcache_id_filename(PTCacheID *pid, char *filename, int cfra, short do_path, short do_ext)
{
	int len=0;
	char *idname;
	char *newname;
	filename[0] = '\0';
	newname = filename;
	
	if (!G.relbase_valid && (pid->cache->flag & PTCACHE_EXTERNAL)==0) return 0; /* save blend file before using disk pointcache */
	
	/* start with temp dir */
	if (do_path) {
		len = ptcache_path(pid, filename);
		newname += len;
	}
	if(strcmp(pid->cache->name, "")==0 && (pid->cache->flag & PTCACHE_EXTERNAL)==0) {
		idname = (pid->ob->id.name+2);
		/* convert chars to hex so they are always a valid filename */
		while('\0' != *idname) {
			snprintf(newname, MAX_PTCACHE_FILE, "%02X", (char)(*idname++));
			newname+=2;
			len += 2;
		}
	}
	else {
		int temp = (int)strlen(pid->cache->name); 
		strcpy(newname, pid->cache->name); 
		newname+=temp;
		len += temp;
	}

	if (do_ext) {

		if(pid->cache->index < 0)
			pid->cache->index =  pid->stack_index = object_insert_ptcache(pid->ob);

		if(pid->cache->flag & PTCACHE_EXTERNAL) {
			if(pid->cache->index >= 0)
				snprintf(newname, MAX_PTCACHE_FILE, "_%06d_%02d"PTCACHE_EXT, cfra, pid->stack_index); /* always 6 chars */
			else
				snprintf(newname, MAX_PTCACHE_FILE, "_%06d"PTCACHE_EXT, cfra); /* always 6 chars */
		}
		else {
			snprintf(newname, MAX_PTCACHE_FILE, "_%06d_%02d"PTCACHE_EXT, cfra, pid->stack_index); /* always 6 chars */
		}
		len += 16;
	}
	
	return len; /* make sure the above string is always 16 chars */
}

/* youll need to close yourself after! */
static PTCacheFile *ptcache_file_open(PTCacheID *pid, int mode, int cfra)
{
	PTCacheFile *pf;
	FILE *fp = NULL;
	char filename[(FILE_MAXDIR+FILE_MAXFILE)*2];

	/* don't allow writing for linked objects */
	if(pid->ob->id.lib && mode == PTCACHE_FILE_WRITE)
		return NULL;

	if (!G.relbase_valid && (pid->cache->flag & PTCACHE_EXTERNAL)==0) return NULL; /* save blend file before using disk pointcache */
	
	BKE_ptcache_id_filename(pid, filename, cfra, 1, 1);

	if (mode==PTCACHE_FILE_READ) {
		if (!BLI_exists(filename)) {
			return NULL;
		}
 		fp = fopen(filename, "rb");
	} else if (mode==PTCACHE_FILE_WRITE) {
		BLI_make_existing_file(filename); /* will create the dir if needs be, same as //textures is created */
		fp = fopen(filename, "wb");
	} else if (mode==PTCACHE_FILE_UPDATE) {
		BLI_make_existing_file(filename);
		fp = fopen(filename, "rb+");
	}

 	if (!fp)
 		return NULL;
	
	pf= MEM_mallocN(sizeof(PTCacheFile), "PTCacheFile");
	pf->fp= fp;
 	
 	return pf;
}

static void ptcache_file_close(PTCacheFile *pf)
{
	fclose(pf->fp);
	MEM_freeN(pf);
}

static int ptcache_file_read(PTCacheFile *pf, void *f, size_t tot, int size)
{
	return (fread(f, size, tot, pf->fp) == tot);
}
static int ptcache_file_write(PTCacheFile *pf, void *f, size_t tot, int size)
{
	return (fwrite(f, size, tot, pf->fp) == tot);
}
static int ptcache_file_read_data(PTCacheFile *pf)
{
	int i;

	for(i=0; i<BPHYS_TOT_DATA; i++) {
		if(pf->data_types & (1<<i) && !ptcache_file_read(pf, pf->cur[i], 1, ptcache_data_size[i]))
			return 0;
	}
	
	return 1;
}
static int ptcache_file_write_data(PTCacheFile *pf)
{		
	int i;

	for(i=0; i<BPHYS_TOT_DATA; i++) {
		if(pf->data_types & (1<<i) && !ptcache_file_write(pf, pf->cur[i], 1, ptcache_data_size[i]))
			return 0;
	}
	
	return 1;
}
static int ptcache_file_read_header_begin(PTCacheFile *pf)
{
	int error=0;
	char bphysics[8];
	
	pf->data_types = 0;
	
	if(fread(bphysics, sizeof(char), 8, pf->fp) != 8)
		error = 1;
	
	if(!error && strncmp(bphysics, "BPHYSICS", 8))
		error = 1;

	if(!error && !fread(&pf->type, sizeof(int), 1, pf->fp))
		error = 1;
	
	/* if there was an error set file as it was */
	if(error)
		fseek(pf->fp, 0, SEEK_SET);

	return !error;
}


static int ptcache_file_write_header_begin(PTCacheFile *pf)
{
	char *bphysics = "BPHYSICS";
	
	if(fwrite(bphysics, sizeof(char), 8, pf->fp) != 8)
		return 0;

	if(!fwrite(&pf->type, sizeof(int), 1, pf->fp))
		return 0;
	
	return 1;
}


/* Data pointer handling */
int BKE_ptcache_data_size(int data_type)
{
	return ptcache_data_size[data_type];
}

static void ptcache_file_init_pointers(PTCacheFile *pf)
{
	int data_types = pf->data_types;

	pf->cur[BPHYS_DATA_INDEX] =		data_types & (1<<BPHYS_DATA_INDEX) ?		&pf->data.index	: NULL;
	pf->cur[BPHYS_DATA_LOCATION] =	data_types & (1<<BPHYS_DATA_LOCATION) ?		&pf->data.loc	: NULL;
	pf->cur[BPHYS_DATA_VELOCITY] =	data_types & (1<<BPHYS_DATA_VELOCITY) ?		&pf->data.vel	: NULL;
	pf->cur[BPHYS_DATA_ROTATION] =	data_types & (1<<BPHYS_DATA_ROTATION) ?		&pf->data.rot	: NULL;
	pf->cur[BPHYS_DATA_AVELOCITY] =	data_types & (1<<BPHYS_DATA_AVELOCITY) ?	&pf->data.ave	: NULL;
	pf->cur[BPHYS_DATA_SIZE] =		data_types & (1<<BPHYS_DATA_SIZE)	?		&pf->data.size	: NULL;
	pf->cur[BPHYS_DATA_TIMES] =		data_types & (1<<BPHYS_DATA_TIMES) ?		&pf->data.times	: NULL;
	pf->cur[BPHYS_DATA_BOIDS] =		data_types & (1<<BPHYS_DATA_BOIDS) ?		&pf->data.boids	: NULL;
}

static void ptcache_file_seek_pointers(int index, PTCacheFile *pf)
{
	int i, size=0;
	int data_types = pf->data_types;

	if(data_types & (1<<BPHYS_DATA_INDEX)) {
		int totpoint;
		/* The simplest solution is to just write to the very end. This may cause
		 * some data duplication, but since it's on disk it's not so bad. The correct
		 * thing would be to search through the file for the correct index and only
		 * write to the end if it's not found, but this could be quite slow.
		 */
		fseek(pf->fp, 8 + sizeof(int), SEEK_SET);
		fread(&totpoint, sizeof(int), 1, pf->fp);
		
		totpoint++;

		fseek(pf->fp, 8 + sizeof(int), SEEK_SET);
		fwrite(&totpoint, sizeof(int), 1, pf->fp);

		fseek(pf->fp, 0, SEEK_END);
	}
	else {
		for(i=0; i<BPHYS_TOT_DATA; i++)
			size += pf->data_types & (1<<i) ? ptcache_data_size[i] : 0;

		/* size of default header + data up to index */
		fseek(pf->fp, 8 + 3*sizeof(int) + index * size, SEEK_SET);
	}

	ptcache_file_init_pointers(pf);
}
void BKE_ptcache_mem_init_pointers(PTCacheMem *pm)
{
	int data_types = pm->data_types;
	int i;

	for(i=0; i<BPHYS_TOT_DATA; i++)
		pm->cur[i] = data_types & (1<<i) ? pm->data[i] : NULL;
}

void BKE_ptcache_mem_incr_pointers(PTCacheMem *pm)
{
	int i;

	for(i=0; i<BPHYS_TOT_DATA; i++) {
		if(pm->cur[i])
			pm->cur[i] = (char*)pm->cur[i] + ptcache_data_size[i];
	}
}
static int BKE_ptcache_mem_seek_pointers(int point_index, PTCacheMem *pm)
{
	int data_types = pm->data_types;
	int i, index = pm->index_array ? pm->index_array[point_index] - 1 : point_index;

	if(index < 0) {
		/* Can't give proper location without reallocation, so don't give any location.
		 * Some points will be cached improperly, but this only happens with simulation
		 * steps bigger than cache->step, so the cache has to be recalculated anyways
		 * at some point.
		 */
		return 0;
	}

	for(i=0; i<BPHYS_TOT_DATA; i++)
		pm->cur[i] = data_types & (1<<i) ? (char*)pm->data[i] + index * ptcache_data_size[i] : NULL;

	return 1;
}
static void ptcache_alloc_data(PTCacheMem *pm)
{
	int data_types = pm->data_types;
	int totpoint = pm->totpoint;
	int i;

	for(i=0; i<BPHYS_TOT_DATA; i++) {
		if(data_types & (1<<i))
			pm->data[i] = MEM_callocN(totpoint * ptcache_data_size[i], "PTCache Data");
	}
}
static void ptcache_free_data(PTCacheMem *pm)
{
	void **data = pm->data;
	int i;

	for(i=0; i<BPHYS_TOT_DATA; i++) {
		if(data[i])
			MEM_freeN(data[i]);
	}

	if(pm->index_array) {
		MEM_freeN(pm->index_array);
		pm->index_array = NULL;
	}
}
static void ptcache_copy_data(void *from[], void *to[])
{
	int i;
	for(i=0; i<BPHYS_TOT_DATA; i++) {
		if(from[i])
			memcpy(to[i], from[i], ptcache_data_size[i]);
	}
}



static int ptcache_pid_old_elemsize(PTCacheID *pid)
{
	if(pid->type==PTCACHE_TYPE_SOFTBODY)
		return 6 * sizeof(float);
	else if(pid->type==PTCACHE_TYPE_PARTICLES)
		return sizeof(ParticleKey);
	else if(pid->type==PTCACHE_TYPE_CLOTH)
		return 9 * sizeof(float);

	return 0;
}

/* reads cache from disk or memory */
/* possible to get old or interpolated result */
int BKE_ptcache_read_cache(PTCacheID *pid, float cfra, float frs_sec)
{
	PTCacheFile *pf=NULL, *pf2=NULL;
	PTCacheMem *pm=NULL, *pm2=NULL;
	float old_data1[14], old_data2[14];
	int cfrai = (int)cfra;
	int old_elemsize = ptcache_pid_old_elemsize(pid);
	int i;

	int cfra1 = 0, cfra2 = 0;
	int totpoint = 0, totpoint2 = 0;
	int *index = &i, *index2 = &i;
	int use_old = 0, old_frame = 0;

	int ret = 0, error = 0;

	/* nothing to read to */
	if(pid->totpoint(pid->calldata, (int)cfra) == 0)
		return 0;

	if(pid->cache->flag & PTCACHE_READ_INFO) {
		pid->cache->flag &= ~PTCACHE_READ_INFO;
		BKE_ptcache_read_cache(pid, 0, frs_sec);
	}


	/* first check if we have the actual frame cached */
	if(cfra == (float)cfrai) {
		if(pid->cache->flag & PTCACHE_DISK_CACHE) {
			pf= ptcache_file_open(pid, PTCACHE_FILE_READ, cfrai);
		}
		else {
			pm = pid->cache->mem_cache.first;

			for(; pm; pm=pm->next) {
				if(pm->frame == cfrai)
					break;
			}
		}
	}

	/* no exact cache frame found so try to find cached frames around cfra */
	if(!pm && !pf) {
		if(pid->cache->flag & PTCACHE_DISK_CACHE) {
			pf=NULL;
			while(cfrai > pid->cache->startframe && !pf) {
				cfrai--;
				pf= ptcache_file_open(pid, PTCACHE_FILE_READ, cfrai);
				cfra1 = cfrai;
			}

			old_frame = cfrai;

			cfrai = (int)cfra;
			while(cfrai < pid->cache->endframe && !pf2) {
				cfrai++;
				pf2= ptcache_file_open(pid, PTCACHE_FILE_READ, cfrai);
				cfra2 = cfrai;
			}

			if(pf && !pf2) {
				pf2 = pf;
				pf = NULL;
			}
		}
		else if(pid->cache->mem_cache.first){
			pm = pid->cache->mem_cache.first;

			while(pm->next && pm->next->frame < cfra)
				pm= pm->next;

			if(pm) {
				old_frame = pm->frame;
				cfra1 = pm->frame;
			}

			pm2 = pid->cache->mem_cache.last;

			if(pm2 && pm2->frame < cfra)
				pm2 = NULL;
			else {
				while(pm2->prev && pm2->prev->frame > cfra)
					pm2= pm2->prev;

				if(pm2)
					cfra2 = pm2->frame;
			}

			if(pm && !pm2) {
				pm2 = pm;
				pm = NULL;
			}
		}
	}

	if(!pm && !pm2 && !pf && !pf2)
		return 0;

	if(pm) {
		BKE_ptcache_mem_init_pointers(pm);
		totpoint = pm->totpoint;
		index = pm->data_types & (1<<BPHYS_DATA_INDEX) ? pm->cur[BPHYS_DATA_INDEX] : &i;
	}
	if(pm2) {
		BKE_ptcache_mem_init_pointers(pm2);
		totpoint2 = pm2->totpoint;
		index2 = pm2->data_types & (1<<BPHYS_DATA_INDEX) ? pm2->cur[BPHYS_DATA_INDEX] : &i;
	}
	if(pf) {
		if(ptcache_file_read_header_begin(pf)) {
			if(pf->type != pid->type) {
				/* todo report error */
				ptcache_file_close(pf);
				pf = NULL;
			}
			else if(pid->read_header(pf)) {
				ptcache_file_init_pointers(pf);
				totpoint = pf->totpoint;
				index = pf->data_types & (1<<BPHYS_DATA_INDEX) ? &pf->data.index : &i;
			}
		}
		else {
			/* fall back to old cache file format */
			use_old = 1;
			totpoint = pid->totpoint(pid->calldata, (int) cfra);
		}
	}
	if(pf2) {
		if(ptcache_file_read_header_begin(pf2)) {
			if(pf2->type != pid->type) {
				/* todo report error */
				ptcache_file_close(pf2);
				pf2 = NULL;
			}
			else if(pid->read_header(pf2)) {
				ptcache_file_init_pointers(pf2);
				totpoint2 = pf2->totpoint;
				index2 = pf2->data_types & (1<<BPHYS_DATA_INDEX) ? &pf2->data.index : &i;
			}
		}
		else {
			/* fall back to old cache file format */
			use_old = 1;
			totpoint2 = pid->totpoint(pid->calldata, (int) cfra);
		}
	}

	/* don't read old cache if already simulated past cached frame */
	if(!pm && !pf && cfra1 && cfra1 <= pid->cache->simframe)
		error = 1;
	if(cfra1 && cfra1==cfra2)
		error = 1;

	if(!error) 
	{
		if(pf && pid->read_stream) {
			if(totpoint != pid->totpoint(pid->calldata, (int) cfra))
				error = 1;
			else
			{
				// we have stream writing here
				pid->read_stream(pf, pid->calldata);
			}
		}
	}

	if((pid->data_types & (1<<BPHYS_DATA_INDEX)) == 0)
		totpoint = MIN2(totpoint, pid->totpoint(pid->calldata, (int) cfra));

	if(!error) 
	{	
		for(i=0; i<totpoint; i++) {
			/* read old cache file format */
			if(use_old) {
				if(pid->read_elem && ptcache_file_read(pf, (void*)old_data1, 1, old_elemsize))
					pid->read_elem(i, pid->calldata, NULL, frs_sec, cfra, old_data1);
				else if(pid->read_elem)
					{ error = 1; break; }
			}
			else {
				if(pid->read_elem && (pm || ptcache_file_read_data(pf)))
					pid->read_elem(*index, pid->calldata, pm ? pm->cur : pf->cur, frs_sec, cfra1 ? (float)cfra1 : (float)cfrai, NULL);
				else if(pid->read_elem)
					{ error = 1; break; }
			}

			if(pm) {
				BKE_ptcache_mem_incr_pointers(pm);
				index = pm->data_types & (1<<BPHYS_DATA_INDEX) ? pm->cur[BPHYS_DATA_INDEX] : &i;
			}
		}
	}

	if(!error) 
	{
		if(pf2 && pid->read_stream) {
			if(totpoint2 != pid->totpoint(pid->calldata, (int) cfra))
				error = 1;
			else
			{
				// we have stream writing here
				pid->read_stream(pf2, pid->calldata);
			}
		}
	}

	if((pid->data_types & (1<<BPHYS_DATA_INDEX)) == 0)
		totpoint2 = MIN2(totpoint2, pid->totpoint(pid->calldata, (int) cfra));

	if(!error) 
	{
		for(i=0; i<totpoint2; i++) {
			/* read old cache file format */
			if(use_old) {
				if(pid->read_elem && ptcache_file_read(pf2, (void*)old_data2, 1, old_elemsize)) {
					if(!pf && pf2)
						pid->read_elem(i, pid->calldata, NULL, frs_sec, (float)cfra2, old_data2);
					else if(pid->interpolate_elem)
						pid->interpolate_elem(i, pid->calldata, NULL, frs_sec, cfra, (float)cfra1, (float)cfra2, old_data2);
					else
					{ error = 1; break; }
				}
				else if(pid->read_elem)
					{ error = 1; break; }
			}
			else {
				if(pid->read_elem && (pm2 || ptcache_file_read_data(pf2))) {
					if((!pf && pf2) || (!pm && pm2))
						pid->read_elem(*index2, pid->calldata, pm2 ? pm2->cur : pf2->cur, frs_sec, (float)cfra2, NULL);
					else if(pid->interpolate_elem)
						pid->interpolate_elem(*index2, pid->calldata, pm2 ? pm2->cur : pf2->cur, frs_sec, cfra, (float)cfra1, (float)cfra2, NULL);
					else
					{ error = 1; break; }
				}
				else if(pid->read_elem)
					{ error = 1; break; }
			}

			if(pm2) {
				BKE_ptcache_mem_incr_pointers(pm2);
				index2 = pm2->data_types & (1<<BPHYS_DATA_INDEX) ? pm2->cur[BPHYS_DATA_INDEX] : &i;
			}
		}
	}

	if(pm || pf)
		ret = (pm2 || pf2) ? PTCACHE_READ_INTERPOLATED : PTCACHE_READ_EXACT;
	else if(pm2 || pf2) {
		ret = PTCACHE_READ_OLD;
		pid->cache->simframe = old_frame;
	}

	if(pf) {
		ptcache_file_close(pf);
		pf = NULL;
	}

	if(pf2) {
		ptcache_file_close(pf2);
		pf = NULL;
	}

	if((pid->cache->flag & PTCACHE_QUICK_CACHE)==0) {
		cfrai = (int)cfra;
		/* clear invalid cache frames so that better stuff can be simulated */
		if(pid->cache->flag & PTCACHE_OUTDATED) {
			BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, cfrai);
		}
		else if(pid->cache->flag & PTCACHE_FRAMES_SKIPPED) {
			if(cfra <= pid->cache->last_exact)
				pid->cache->flag &= ~PTCACHE_FRAMES_SKIPPED;

			BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, MAX2(cfrai,pid->cache->last_exact));
		}
	}

	return (error ? 0 : ret);
}
/* TODO for later */
static void ptcache_make_index_array(PTCacheMem *pm, int totpoint)
{
	int i, *index;

	if(pm->index_array) {
		MEM_freeN(pm->index_array);
		pm->index_array = NULL;
	}

	if(!pm->data[BPHYS_DATA_INDEX])
		return;

	pm->index_array = MEM_callocN(totpoint * sizeof(int), "PTCacheMem index_array");
	index = pm->data[BPHYS_DATA_INDEX];

	for(i=0; i<pm->totpoint; i++, index++)
		pm->index_array[*index] = i + 1;
}
/* writes cache to disk or memory */
int BKE_ptcache_write_cache(PTCacheID *pid, int cfra)
{
	PointCache *cache = pid->cache;
	PTCacheFile *pf= NULL, *pf2= NULL;
	int i;
	int totpoint = pid->totpoint(pid->calldata, cfra);
	int add = 0, overwrite = 0;

	if(totpoint == 0 || cfra < 0
		|| (cfra ? pid->data_types == 0 : pid->info_types == 0))
		return 0;

	if(cache->flag & PTCACHE_DISK_CACHE) {
		int ofra, efra = cache->endframe;

		if(cfra==0)
			add = 1;
		/* allways start from scratch on the first frame */
		else if(cfra == cache->startframe) {
			BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, cfra);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
			add = 1;
		}
		else {
			/* find last cached frame */
			while(efra > cache->startframe && !BKE_ptcache_id_exist(pid, efra))
				efra--;

			/* find second last cached frame */
			ofra = efra-1;
			while(ofra > cache->startframe && !BKE_ptcache_id_exist(pid, ofra))
				ofra--;

			if(efra >= cache->startframe && cfra > efra) {
				if(ofra >= cache->startframe && efra - ofra < cache->step)
					overwrite = 1;
				else
					add = 1;
			}
		}

		if(add || overwrite) {
			if(overwrite)
				BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_FRAME, efra);

			pf = ptcache_file_open(pid, PTCACHE_FILE_WRITE, cfra);
			if(!pf)
				return 0;

			pf->type = pid->type;
			pf->totpoint = cfra ? pid->totwrite(pid->calldata, cfra) : totpoint;
			pf->data_types = cfra ? pid->data_types : pid->info_types;

			if(!ptcache_file_write_header_begin(pf) || !pid->write_header(pf)) {
				ptcache_file_close(pf);
				return 0;
			}

			ptcache_file_init_pointers(pf);

			if(pf && pid->write_stream) {
				// we have stream writing here
				pid->write_stream(pf, pid->calldata);
			}
			else
				for(i=0; i<totpoint; i++) {
					if(pid->write_elem) {
						int write = pid->write_elem(i, pid->calldata, pf->cur, cfra);
						if(write) {
							if(!ptcache_file_write_data(pf)) {
								ptcache_file_close(pf);
								if(pf2) ptcache_file_close(pf2);
								return 0;
							}
							/* newly born particles have to be copied to previous cached frame */
							else if(overwrite && write == 2) {
								if(!pf2) {
									pf2 = ptcache_file_open(pid, PTCACHE_FILE_UPDATE, ofra);
									if(!pf2) {
										ptcache_file_close(pf);
										return 0;
									}
									pf2->type = pid->type;
									pf2->totpoint = totpoint;
									pf2->data_types = pid->data_types;
								}
								ptcache_file_seek_pointers(i, pf2);
								pid->write_elem(i, pid->calldata, pf2->cur, cfra);
								if(!ptcache_file_write_data(pf2)) {
									ptcache_file_close(pf);
									ptcache_file_close(pf2);
									return 0;
								}
							}
						}
					}
				}
		}
	}
	else {
		PTCacheMem *pm;
		PTCacheMem *pm2;

		pm2 = cache->mem_cache.first;
		
		/* don't write info file in memory */
		if(cfra==0)
			return 1;
		/* allways start from scratch on the first frame */
		if(cfra == cache->startframe) {
			BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, cfra);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
			add = 1;
		}
		else if (cache->mem_cache.last) {
			pm2 = cache->mem_cache.last;

			if(pm2 && cfra > pm2->frame) {
				if(pm2->prev && pm2->frame - pm2->prev->frame < cache->step)
					overwrite = 1;
				else
					add = 1;
			}
		}
		else
			add = 1;

		if(add || overwrite) {
			if(overwrite)
				BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_FRAME, pm2->frame);

			pm = MEM_callocN(sizeof(PTCacheMem), "Pointcache mem");

			pm->totpoint = pid->totwrite(pid->calldata, cfra);
			pm->data_types = cfra ? pid->data_types : pid->info_types;

			ptcache_alloc_data(pm);
			BKE_ptcache_mem_init_pointers(pm);

			for(i=0; i<totpoint; i++) {
				if(pid->write_elem) {
					int write = pid->write_elem(i, pid->calldata, pm->cur, cfra);
					if(write) {
						BKE_ptcache_mem_incr_pointers(pm);

						/* newly born particles have to be copied to previous cached frame */
						if(overwrite && write == 2) {
							pm2 = cache->mem_cache.last;
							if(BKE_ptcache_mem_seek_pointers(i, pm2))
								pid->write_elem(i, pid->calldata, pm2->cur, cfra);
						}
					}
				}
			}
			ptcache_make_index_array(pm, pid->totpoint(pid->calldata, cfra));

			pm->frame = cfra;
			BLI_addtail(&cache->mem_cache, pm);
		}
	}

	if(add || overwrite) {
		if(cfra - cache->last_exact == 1
			|| cfra == cache->startframe) {
			cache->last_exact = cfra;
			cache->flag &= ~PTCACHE_FRAMES_SKIPPED;
		}
		else
			cache->flag |= PTCACHE_FRAMES_SKIPPED;
	}
	
	if(pf) ptcache_file_close(pf);

	if(pf2) ptcache_file_close(pf2);

	BKE_ptcache_update_info(pid);

	return 1;
}
/* youll need to close yourself after!
 * mode - PTCACHE_CLEAR_ALL, 

*/
/* Clears & resets */
void BKE_ptcache_id_clear(PTCacheID *pid, int mode, int cfra)
{
	int len; /* store the length of the string */

	/* mode is same as fopen's modes */
	DIR *dir; 
	struct dirent *de;
	char path[MAX_PTCACHE_PATH];
	char filename[MAX_PTCACHE_FILE];
	char path_full[MAX_PTCACHE_FILE];
	char ext[MAX_PTCACHE_PATH];

	if(!pid->cache || pid->cache->flag & PTCACHE_BAKED)
		return;

	/* don't allow clearing for linked objects */
	if(pid->ob->id.lib)
		return;

	/*if (!G.relbase_valid) return; *//* save blend file before using pointcache */
	
	/* clear all files in the temp dir with the prefix of the ID and the ".bphys" suffix */
	switch (mode) {
	case PTCACHE_CLEAR_ALL:
	case PTCACHE_CLEAR_BEFORE:	
	case PTCACHE_CLEAR_AFTER:
		if(pid->cache->flag & PTCACHE_DISK_CACHE) {
			ptcache_path(pid, path);
			
			len = BKE_ptcache_id_filename(pid, filename, cfra, 0, 0); /* no path */
			
			dir = opendir(path);
			if (dir==NULL)
				return;

			snprintf(ext, sizeof(ext), "_%02d"PTCACHE_EXT, pid->stack_index);
			
			while ((de = readdir(dir)) != NULL) {
				if (strstr(de->d_name, ext)) { /* do we have the right extension?*/
					if (strncmp(filename, de->d_name, len ) == 0) { /* do we have the right prefix */
						if (mode == PTCACHE_CLEAR_ALL) {
							pid->cache->last_exact = 0;
							BLI_join_dirfile(path_full, path, de->d_name);
							BLI_delete(path_full, 0, 0);
						} else {
							/* read the number of the file */
							int frame, len2 = (int)strlen(de->d_name);
							char num[7];

							if (len2 > 15) { /* could crash if trying to copy a string out of this range*/
								BLI_strncpy(num, de->d_name + (strlen(de->d_name) - 15), sizeof(num));
								frame = atoi(num);
								
								if((mode==PTCACHE_CLEAR_BEFORE && frame < cfra)	|| 
								(mode==PTCACHE_CLEAR_AFTER && frame > cfra)	) {
									
									BLI_join_dirfile(path_full, path, de->d_name);
									BLI_delete(path_full, 0, 0);
								}
							}
						}
					}
				}
			}
			closedir(dir);
		}
		else {
			PTCacheMem *pm= pid->cache->mem_cache.first;
			PTCacheMem *link= NULL;

			pm= pid->cache->mem_cache.first;

			if(mode == PTCACHE_CLEAR_ALL) {
				pid->cache->last_exact = 0;
				for(; pm; pm=pm->next)
					ptcache_free_data(pm);
				BLI_freelistN(&pid->cache->mem_cache);
			} else {
				while(pm) {
					if((mode==PTCACHE_CLEAR_BEFORE && pm->frame < cfra)	|| 
					(mode==PTCACHE_CLEAR_AFTER && pm->frame > cfra)	) {
						link = pm;
						ptcache_free_data(pm);
						pm = pm->next;
						BLI_freelinkN(&pid->cache->mem_cache, link);
					}
					else
						pm = pm->next;
				}
			}
		}
		break;
		
	case PTCACHE_CLEAR_FRAME:
		if(pid->cache->flag & PTCACHE_DISK_CACHE) {
			if(BKE_ptcache_id_exist(pid, cfra)) {
				BKE_ptcache_id_filename(pid, filename, cfra, 1, 1); /* no path */
				BLI_delete(filename, 0, 0);
			}
		}
		else {
			PTCacheMem *pm = pid->cache->mem_cache.first;

			for(; pm; pm=pm->next) {
				if(pm->frame == cfra) {
					ptcache_free_data(pm);
					BLI_freelinkN(&pid->cache->mem_cache, pm);
					break;
				}
			}
		}
		break;
	}

	BKE_ptcache_update_info(pid);
}

int BKE_ptcache_id_exist(PTCacheID *pid, int cfra)
{
	if(!pid->cache)
		return 0;
	
	if(pid->cache->flag & PTCACHE_DISK_CACHE) {
		char filename[MAX_PTCACHE_FILE];
		
		BKE_ptcache_id_filename(pid, filename, cfra, 1, 1);

		return BLI_exists(filename);
	}
	else {
		PTCacheMem *pm = pid->cache->mem_cache.first;

		for(; pm; pm=pm->next) {
			if(pm->frame==cfra)
				return 1;
		}
		return 0;
	}
}

void BKE_ptcache_id_time(PTCacheID *pid, Scene *scene, float cfra, int *startframe, int *endframe, float *timescale)
{
	Object *ob;
	PointCache *cache;
	float offset, time, nexttime;

	/* TODO: this has to be sorter out once bsystem_time gets redone, */
	/*       now caches can handle interpolating etc. too - jahka */

	/* time handling for point cache:
	 * - simulation time is scaled by result of bsystem_time
	 * - for offsetting time only time offset is taken into account, since
	 *   that's always the same and can't be animated. a timeoffset which
	 *   varies over time is not simpe to support.
	 * - field and motion blur offsets are currently ignored, proper solution
	 *   is probably to interpolate results from two frames for that ..
	 */

	ob= pid->ob;
	cache= pid->cache;

	if(timescale) {
		time= bsystem_time(scene, ob, cfra, 0.0f);
		nexttime= bsystem_time(scene, ob, cfra+1.0f, 0.0f);

		*timescale= MAX2(nexttime - time, 0.0f);
	}

	if(startframe && endframe) {
		*startframe= cache->startframe;
		*endframe= cache->endframe;

		// XXX ipoflag is depreceated - old animation system stuff
		if (/*(ob->ipoflag & OB_OFFS_PARENT) &&*/ (ob->partype & PARSLOW)==0) {
			offset= give_timeoffset(ob);

			*startframe += (int)(offset+0.5f);
			*endframe += (int)(offset+0.5f);
		}
	}
}

int BKE_ptcache_id_reset(Scene *scene, PTCacheID *pid, int mode)
{
	PointCache *cache;
	int reset, clear, after;

	if(!pid->cache)
		return 0;

	cache= pid->cache;
	reset= 0;
	clear= 0;
	after= 0;

	if(mode == PTCACHE_RESET_DEPSGRAPH) {
		if(!(cache->flag & PTCACHE_BAKED) && !BKE_ptcache_get_continue_physics()) {
			if(cache->flag & PTCACHE_QUICK_CACHE)
				clear= 1;

			after= 1;
		}

		cache->flag |= PTCACHE_OUTDATED;
	}
	else if(mode == PTCACHE_RESET_BAKED) {
		if(!BKE_ptcache_get_continue_physics()) {
			reset= 1;
			clear= 1;
		}
		else
			cache->flag |= PTCACHE_OUTDATED;
	}
	else if(mode == PTCACHE_RESET_OUTDATED) {
		reset = 1;

		if(cache->flag & PTCACHE_OUTDATED && !(cache->flag & PTCACHE_BAKED)) {
			clear= 1;
			cache->flag &= ~PTCACHE_OUTDATED;
		}
	}

	if(reset) {
		cache->flag &= ~(PTCACHE_REDO_NEEDED|PTCACHE_SIMULATION_VALID);
		cache->simframe= 0;
		cache->last_exact= 0;

		if(pid->type == PTCACHE_TYPE_CLOTH)
			cloth_free_modifier(pid->ob, pid->calldata);
		else if(pid->type == PTCACHE_TYPE_SOFTBODY)
			sbFreeSimulation(pid->calldata);
		else if(pid->type == PTCACHE_TYPE_PARTICLES)
			psys_reset(pid->calldata, PSYS_RESET_DEPSGRAPH);
		else if(pid->type == PTCACHE_TYPE_SMOKE_DOMAIN)
			smokeModifier_reset(pid->calldata);
		else if(pid->type == PTCACHE_TYPE_SMOKE_HIGHRES)
			smokeModifier_reset_turbulence(pid->calldata);
	}
	if(clear)
		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
	else if(after)
		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, CFRA);

	return (reset || clear || after);
}

int BKE_ptcache_object_reset(Scene *scene, Object *ob, int mode)
{
	PTCacheID pid;
	ParticleSystem *psys;
	ModifierData *md;
	int reset, skip;

	reset= 0;
	skip= 0;

	if(ob->soft) {
		BKE_ptcache_id_from_softbody(&pid, ob, ob->soft);
		reset |= BKE_ptcache_id_reset(scene, &pid, mode);
	}

	for(psys=ob->particlesystem.first; psys; psys=psys->next) {
		/* Baked cloth hair has to be checked first, because we don't want to reset */
		/* particles or cloth in that case -jahka */
		if(psys->clmd) {
			BKE_ptcache_id_from_cloth(&pid, ob, psys->clmd);
			if(mode == PSYS_RESET_ALL || !(psys->part->type == PART_HAIR && (pid.cache->flag & PTCACHE_BAKED))) 
				reset |= BKE_ptcache_id_reset(scene, &pid, mode);
			else
				skip = 1;
		}
		else if(psys->recalc & PSYS_RECALC_REDO || psys->recalc & PSYS_RECALC_CHILD)
			skip = 1;

		if(skip == 0 && psys->part) {
			BKE_ptcache_id_from_particles(&pid, ob, psys);
			reset |= BKE_ptcache_id_reset(scene, &pid, mode);
		}
	}

	for(md=ob->modifiers.first; md; md=md->next) {
		if(md->type == eModifierType_Cloth) {
			BKE_ptcache_id_from_cloth(&pid, ob, (ClothModifierData*)md);
			reset |= BKE_ptcache_id_reset(scene, &pid, mode);
		}
		if(md->type == eModifierType_Smoke) {
			SmokeModifierData *smd = (SmokeModifierData *)md;
			if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
			{
				BKE_ptcache_id_from_smoke(&pid, ob, (SmokeModifierData*)md);
				reset |= BKE_ptcache_id_reset(scene, &pid, mode);

				BKE_ptcache_id_from_smoke_turbulence(&pid, ob, (SmokeModifierData*)md);
				reset |= BKE_ptcache_id_reset(scene, &pid, mode);
			}
		}
	}

	if (ob->type == OB_ARMATURE)
		BIK_clear_cache(ob->pose);

	return reset;
}

/* Use this when quitting blender, with unsaved files */
void BKE_ptcache_remove(void)
{
	char path[MAX_PTCACHE_PATH];
	char path_full[MAX_PTCACHE_PATH];
	int rmdir = 1;
	
	ptcache_path(NULL, path);

	if (BLI_exist(path)) {
		/* The pointcache dir exists? - remove all pointcache */

		DIR *dir; 
		struct dirent *de;

		dir = opendir(path);
		if (dir==NULL)
			return;
		
		while ((de = readdir(dir)) != NULL) {
			if( strcmp(de->d_name, ".")==0 || strcmp(de->d_name, "..")==0) {
				/* do nothing */
			} else if (strstr(de->d_name, PTCACHE_EXT)) { /* do we have the right extension?*/
				BLI_join_dirfile(path_full, path, de->d_name);
				BLI_delete(path_full, 0, 0);
			} else {
				rmdir = 0; /* unknown file, dont remove the dir */
			}
		}

		closedir(dir);
	} else { 
		rmdir = 0; /* path dosnt exist  */
	}
	
	if (rmdir) {
		BLI_delete(path, 1, 0);
	}
}

/* Continuous Interaction */

static int CONTINUE_PHYSICS = 0;

void BKE_ptcache_set_continue_physics(Scene *scene, int enable)
{
	Object *ob;

	if(CONTINUE_PHYSICS != enable) {
		CONTINUE_PHYSICS = enable;

		if(CONTINUE_PHYSICS == 0) {
			for(ob=G.main->object.first; ob; ob=ob->id.next)
				if(BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_OUTDATED))
					DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		}
	}
}

int BKE_ptcache_get_continue_physics()
{
	return CONTINUE_PHYSICS;
}

/* Point Cache handling */

PointCache *BKE_ptcache_add(ListBase *ptcaches)
{
	PointCache *cache;

	cache= MEM_callocN(sizeof(PointCache), "PointCache");
	cache->startframe= 1;
	cache->endframe= 250;
	cache->step= 10;
	cache->index = -1;

	BLI_addtail(ptcaches, cache);

	return cache;
}

void BKE_ptcache_free_mem(ListBase *mem_cache)
{
	PTCacheMem *pm = mem_cache->first;

	if(pm) {
		for(; pm; pm=pm->next)
			ptcache_free_data(pm);

		BLI_freelistN(mem_cache);
	}
}
void BKE_ptcache_free(PointCache *cache)
{
	BKE_ptcache_free_mem(&cache->mem_cache);
	if(cache->edit && cache->free_edit)
		cache->free_edit(cache->edit);
	MEM_freeN(cache);
}
void BKE_ptcache_free_list(ListBase *ptcaches)
{
	PointCache *cache = ptcaches->first;

	while(cache) {
		BLI_remlink(ptcaches, cache);
		BKE_ptcache_free(cache);
		cache = ptcaches->first;
	}
}

static PointCache *ptcache_copy(PointCache *cache)
{
	PointCache *ncache;

	ncache= MEM_dupallocN(cache);

	/* hmm, should these be copied over instead? */
	ncache->mem_cache.first = NULL;
	ncache->mem_cache.last = NULL;

	ncache->flag= 0;
	ncache->simframe= 0;

	return ncache;
}
/* returns first point cache */
PointCache *BKE_ptcache_copy_list(ListBase *ptcaches_new, ListBase *ptcaches_old)
{
	PointCache *cache = ptcaches_old->first;

	ptcaches_new->first = ptcaches_new->last = NULL;

	for(; cache; cache=cache->next)
		BLI_addtail(ptcaches_new, ptcache_copy(cache));

	return ptcaches_new->first;
}


/* Baking */
static int count_quick_cache(Scene *scene, int *quick_step)
{
	Base *base = scene->base.first;
	PTCacheID *pid;
	ListBase pidlist;
	int autocache_count= 0;

	for(base = scene->base.first; base; base = base->next) {
		if(base->object) {
			BKE_ptcache_ids_from_object(&pidlist, base->object);

			for(pid=pidlist.first; pid; pid=pid->next) {
				if((pid->cache->flag & PTCACHE_BAKED)
					|| (pid->cache->flag & PTCACHE_QUICK_CACHE)==0)
					continue;

				if(pid->cache->flag & PTCACHE_OUTDATED || (pid->cache->flag & PTCACHE_SIMULATION_VALID)==0) {
					if(!autocache_count)
						*quick_step = pid->cache->step;
					else
						*quick_step = MIN2(*quick_step, pid->cache->step);

					autocache_count++;
				}
			}

			BLI_freelistN(&pidlist);
		}
	}

	return autocache_count;
}
void BKE_ptcache_quick_cache_all(Scene *scene)
{
	PTCacheBaker baker;

	baker.bake=0;
	baker.break_data=NULL;
	baker.break_test=NULL;
	baker.pid=NULL;
	baker.progressbar=NULL;
	baker.progressend=NULL;
	baker.progresscontext=NULL;
	baker.render=0;
	baker.anim_init = 0;
	baker.scene=scene;

	if(count_quick_cache(scene, &baker.quick_step))
		BKE_ptcache_make_cache(&baker);
}

/* Simulation thread, no need for interlocks as data written in both threads
 are only unitary integers (I/O assumed to be atomic for them) */
typedef struct {
	int break_operation;
	int thread_ended;
	int endframe;
	int step;
	int *cfra_ptr;
	Scene *scene;
} ptcache_make_cache_data;

static void *ptcache_make_cache_thread(void *ptr) {
	ptcache_make_cache_data *data = (ptcache_make_cache_data*)ptr;

	for(; (*data->cfra_ptr <= data->endframe) && !data->break_operation; *data->cfra_ptr+=data->step)
		scene_update_for_newframe(data->scene, data->scene->lay);

	data->thread_ended = TRUE;
	return NULL;
}

/* if bake is not given run simulations to current frame */
void BKE_ptcache_make_cache(PTCacheBaker* baker)
{
	Scene *scene = baker->scene;
	Base *base;
	ListBase pidlist;
	PTCacheID *pid = baker->pid;
	PointCache *cache = NULL;
	float frameleno = scene->r.framelen;
	int cfrao = CFRA;
	int startframe = MAXFRAME;
	int bake = baker->bake;
	int render = baker->render;
	ListBase threads;
	ptcache_make_cache_data thread_data;
	int progress, old_progress;
	
	thread_data.endframe = baker->anim_init ? scene->r.sfra : CFRA;
	thread_data.step = baker->quick_step;
	thread_data.cfra_ptr = &CFRA;
	thread_data.scene = baker->scene;

	G.afbreek = 0;

	/* set caches to baking mode and figure out start frame */
	if(pid) {
		/* cache/bake a single object */
		cache = pid->cache;
		if((cache->flag & PTCACHE_BAKED)==0) {
			if(pid->type==PTCACHE_TYPE_PARTICLES)
				psys_get_pointcache_start_end(scene, pid->calldata, &cache->startframe, &cache->endframe);
			else if(pid->type == PTCACHE_TYPE_SMOKE_HIGHRES) {
				/* get all pids from the object and search for smoke low res */
				ListBase pidlist2;
				PTCacheID *pid2;
				BKE_ptcache_ids_from_object(&pidlist2, pid->ob);
				for(pid2=pidlist2.first; pid2; pid2=pid2->next) {
					if(pid2->type == PTCACHE_TYPE_SMOKE_DOMAIN) 
					{
						if(pid2->cache && !(pid2->cache->flag & PTCACHE_BAKED)) {
							if(bake || pid2->cache->flag & PTCACHE_REDO_NEEDED)
								BKE_ptcache_id_clear(pid2, PTCACHE_CLEAR_ALL, 0);
							if(bake) {
								pid2->cache->flag |= PTCACHE_BAKING;
								pid2->cache->flag &= ~PTCACHE_BAKED;
							}
						}
					}
				}
				BLI_freelistN(&pidlist2);
			}

			if(bake || cache->flag & PTCACHE_REDO_NEEDED)
				BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

			startframe = MAX2(cache->last_exact, cache->startframe);

			if(bake) {
				thread_data.endframe = cache->endframe;
				cache->flag |= PTCACHE_BAKING;
			}
			else {
				thread_data.endframe = MIN2(thread_data.endframe, cache->endframe);
			}

			cache->flag &= ~PTCACHE_BAKED;
		}
	}
	else for(base=scene->base.first; base; base= base->next) {
		/* cache/bake everything in the scene */
		BKE_ptcache_ids_from_object(&pidlist, base->object);

		for(pid=pidlist.first; pid; pid=pid->next) {
			cache = pid->cache;
			if((cache->flag & PTCACHE_BAKED)==0) {
				if(pid->type==PTCACHE_TYPE_PARTICLES) {
					ParticleSystem *psys = (ParticleSystem*)pid->calldata;
					/* skip hair & keyed particles */
					if(psys->part->type == PART_HAIR || psys->part->phystype == PART_PHYS_KEYED)
						continue;

					psys_get_pointcache_start_end(scene, pid->calldata, &cache->startframe, &cache->endframe);
				}

				if((cache->flag & PTCACHE_REDO_NEEDED || (cache->flag & PTCACHE_SIMULATION_VALID)==0)
					&& ((cache->flag & PTCACHE_QUICK_CACHE)==0 || render || bake))
					BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

				startframe = MIN2(startframe, cache->startframe);

				if(bake || render) {
					cache->flag |= PTCACHE_BAKING;

					if(bake)
						thread_data.endframe = MAX2(thread_data.endframe, cache->endframe);
				}

				cache->flag &= ~PTCACHE_BAKED;

			}
		}
		BLI_freelistN(&pidlist);
	}

	CFRA = startframe;
	scene->r.framelen = 1.0;
	thread_data.break_operation = FALSE;
	thread_data.thread_ended = FALSE;
	old_progress = -1;
	
	BLI_init_threads(&threads, ptcache_make_cache_thread, 1);
	BLI_insert_thread(&threads, (void*)&thread_data);
	
	while (thread_data.thread_ended == FALSE) {

		if(bake)
			progress = (int)(100.0f * (float)(CFRA - startframe)/(float)(thread_data.endframe-startframe));
		else
			progress = CFRA;

		/* NOTE: baking should not redraw whole ui as this slows things down */
		if ((baker->progressbar) && (progress != old_progress)) {
			baker->progressbar(baker->progresscontext, progress);
			old_progress = progress;
		}
		
		/* Delay to lessen CPU load from UI thread */
		PIL_sleep_ms(200);

		/* NOTE: breaking baking should leave calculated frames in cache, not clear it */
		if(blender_test_break() && !thread_data.break_operation) {
			thread_data.break_operation = TRUE;
			if (baker->progressend)
				baker->progressend(baker->progresscontext);
			WM_cursor_wait(1);
		}
	}

	BLI_end_threads(&threads);

	/* clear baking flag */
	if(pid) {
		cache->flag &= ~(PTCACHE_BAKING|PTCACHE_REDO_NEEDED);
		cache->flag |= PTCACHE_SIMULATION_VALID;
		if(bake) {
			cache->flag |= PTCACHE_BAKED;
			/* write info file */
			if(cache->flag & PTCACHE_DISK_CACHE)
				BKE_ptcache_write_cache(pid, 0);
		}
	}
	else for(base=scene->base.first; base; base= base->next) {
		BKE_ptcache_ids_from_object(&pidlist, base->object);

		for(pid=pidlist.first; pid; pid=pid->next) {
			/* skip hair particles */
			if(pid->type==PTCACHE_TYPE_PARTICLES && ((ParticleSystem*)pid->calldata)->part->type == PART_HAIR)
				continue;
		
			cache = pid->cache;

			if(thread_data.step > 1)
				cache->flag &= ~(PTCACHE_BAKING|PTCACHE_OUTDATED);
			else
				cache->flag &= ~(PTCACHE_BAKING|PTCACHE_REDO_NEEDED);

			cache->flag |= PTCACHE_SIMULATION_VALID;

			if(bake) {
				cache->flag |= PTCACHE_BAKED;
				if(cache->flag & PTCACHE_DISK_CACHE)
					BKE_ptcache_write_cache(pid, 0);
			}
		}
		BLI_freelistN(&pidlist);
	}

	scene->r.framelen = frameleno;
	CFRA = cfrao;
	
	if(bake) /* already on cfra unless baking */
		scene_update_for_newframe(scene, scene->lay);

	if (thread_data.break_operation)
		WM_cursor_wait(0);
	else if (baker->progressend)
		baker->progressend(baker->progresscontext);

	/* TODO: call redraw all windows somehow */
}
/* Helpers */
void BKE_ptcache_disk_to_mem(PTCacheID *pid)
{
	PointCache *cache = pid->cache;
	PTCacheFile *pf;
	PTCacheMem *pm;

	int cfra, sfra = cache->startframe, efra = cache->endframe;
	int i;

	BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

	for(cfra=sfra; cfra <= efra; cfra++) {
		pf = ptcache_file_open(pid, PTCACHE_FILE_READ, cfra);

		if(pf) {
			if(!ptcache_file_read_header_begin(pf)) {
				printf("Can't yet convert old cache format\n");
				cache->flag |= PTCACHE_DISK_CACHE;
				ptcache_file_close(pf);
				return;
			}

			if(pf->type != pid->type || !pid->read_header(pf)) {
				cache->flag |= PTCACHE_DISK_CACHE;
				ptcache_file_close(pf);
				return;
			}
			
			pm = MEM_callocN(sizeof(PTCacheMem), "Pointcache mem");

			pm->totpoint = pf->totpoint;
			pm->data_types = pf->data_types;
			pm->frame = cfra;

			ptcache_alloc_data(pm);
			BKE_ptcache_mem_init_pointers(pm);
			ptcache_file_init_pointers(pf);

			for(i=0; i<pm->totpoint; i++) {
				if(!ptcache_file_read_data(pf)) {
					printf("Error reading from disk cache\n");
					
					cache->flag |= PTCACHE_DISK_CACHE;
					
					ptcache_free_data(pm);
					MEM_freeN(pm);
					ptcache_file_close(pf);

					return;
				}
				ptcache_copy_data(pf->cur, pm->cur);
				BKE_ptcache_mem_incr_pointers(pm);
			}

			ptcache_make_index_array(pm, pid->totpoint(pid->calldata, cfra));

			BLI_addtail(&pid->cache->mem_cache, pm);

			ptcache_file_close(pf);
		}
	}

}
void BKE_ptcache_mem_to_disk(PTCacheID *pid)
{
	PointCache *cache = pid->cache;
	PTCacheFile *pf;
	PTCacheMem *pm;
	int i;

	pm = cache->mem_cache.first;

	BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

	for(; pm; pm=pm->next) {
		pf = ptcache_file_open(pid, PTCACHE_FILE_WRITE, pm->frame);

		if(pf) {
			pf->data_types = pm->data_types;
			pf->totpoint = pm->totpoint;
			pf->type = pid->type;

			BKE_ptcache_mem_init_pointers(pm);
			ptcache_file_init_pointers(pf);

			if(!ptcache_file_write_header_begin(pf) || !pid->write_header(pf)) {
				printf("Error writing to disk cache\n");
				cache->flag &= ~PTCACHE_DISK_CACHE;

				ptcache_file_close(pf);
				return;
			}

			for(i=0; i<pm->totpoint; i++) {
				ptcache_copy_data(pm->cur, pf->cur);
				if(!ptcache_file_write_data(pf)) {
					printf("Error writing to disk cache\n");
					cache->flag &= ~PTCACHE_DISK_CACHE;

					ptcache_file_close(pf);
					return;
				}
				BKE_ptcache_mem_incr_pointers(pm);
			}

			ptcache_file_close(pf);

			/* write info file */
			if(cache->flag & PTCACHE_BAKED)
				BKE_ptcache_write_cache(pid, 0);
		}
		else
			printf("Error creating disk cache file\n");
	}
}
void BKE_ptcache_toggle_disk_cache(PTCacheID *pid)
{
	PointCache *cache = pid->cache;
	int last_exact = cache->last_exact;

	if (!G.relbase_valid){
		cache->flag &= ~PTCACHE_DISK_CACHE;
		printf("File must be saved before using disk cache!\n");
		return;
	}

	if(cache->flag & PTCACHE_DISK_CACHE)
		BKE_ptcache_mem_to_disk(pid);
	else
		BKE_ptcache_disk_to_mem(pid);

	cache->flag ^= PTCACHE_DISK_CACHE;
	BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
	cache->flag ^= PTCACHE_DISK_CACHE;
	
	cache->last_exact = last_exact;

	BKE_ptcache_update_info(pid);
}

void BKE_ptcache_load_external(PTCacheID *pid)
{
	/*todo*/
	PointCache *cache = pid->cache;
	int len; /* store the length of the string */
	int info = 0;

	/* mode is same as fopen's modes */
	DIR *dir; 
	struct dirent *de;
	char path[MAX_PTCACHE_PATH];
	char filename[MAX_PTCACHE_FILE];
	char ext[MAX_PTCACHE_PATH];

	if(!cache)
		return;

	cache->startframe = MAXFRAME;
	cache->endframe = -1;
	cache->totpoint = 0;

	ptcache_path(pid, path);
	
	len = BKE_ptcache_id_filename(pid, filename, 1, 0, 0); /* no path */
	
	dir = opendir(path);
	if (dir==NULL)
		return;

	if(cache->index >= 0)
		snprintf(ext, sizeof(ext), "_%02d"PTCACHE_EXT, cache->index);
	else
		strcpy(ext, PTCACHE_EXT);
	
	while ((de = readdir(dir)) != NULL) {
		if (strstr(de->d_name, ext)) { /* do we have the right extension?*/
			if (strncmp(filename, de->d_name, len ) == 0) { /* do we have the right prefix */
				/* read the number of the file */
				int frame, len2 = (int)strlen(de->d_name);
				char num[7];

				if (len2 > 15) { /* could crash if trying to copy a string out of this range*/
					BLI_strncpy(num, de->d_name + (strlen(de->d_name) - 15), sizeof(num));
					frame = atoi(num);

					if(frame) {
						cache->startframe = MIN2(cache->startframe, frame);
						cache->endframe = MAX2(cache->endframe, frame);
					}
					else
						info = 1;
				}
			}
		}
	}
	closedir(dir);

	if(cache->startframe != MAXFRAME) {
		PTCacheFile *pf;

		/* read totpoint from info file (frame 0) */
		if(info) {
			pf= ptcache_file_open(pid, PTCACHE_FILE_READ, 0);

			if(pf) {
				if(ptcache_file_read_header_begin(pf)) {
					if(pf->type == pid->type && pid->read_header(pf)) {
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
			int elemsize = ptcache_pid_old_elemsize(pid);
			pf= ptcache_file_open(pid, PTCACHE_FILE_READ, cache->startframe);

			if(pf) {
				while(ptcache_file_read(pf, old_data, 1, elemsize))
					cache->totpoint++;
				
				ptcache_file_close(pf);
			}
		}
	}

	cache->flag &= ~(PTCACHE_OUTDATED|PTCACHE_FRAMES_SKIPPED);

	BKE_ptcache_update_info(pid);
}

void BKE_ptcache_update_info(PTCacheID *pid)
{
	PointCache *cache = pid->cache;
	int totframes = 0;
	char mem_info[64];

	if(cache->flag & PTCACHE_EXTERNAL) {
		int cfra = cache->startframe;

		for(; cfra<=cache->endframe; cfra++) {
			if(BKE_ptcache_id_exist(pid, cfra))
				totframes++;
		}

		if(totframes && cache->totpoint)
			sprintf(cache->info, "%i points found!", cache->totpoint);
		else
			sprintf(cache->info, "No valid data to read!");
		return;
	}

	if(cache->flag & PTCACHE_DISK_CACHE) {
		int cfra = cache->startframe;

		for(; cfra<=cache->endframe; cfra++) {
			if(BKE_ptcache_id_exist(pid, cfra))
				totframes++;
		}

		sprintf(mem_info, "%i frames on disk", totframes);
	}
	else {
		PTCacheMem *pm = cache->mem_cache.first;		
		float bytes = 0.0f;
		int i, mb;
		
		for(; pm; pm=pm->next) {
			for(i=0; i<BPHYS_TOT_DATA; i++)
				bytes += pm->data[i] ? MEM_allocN_len(pm->data[i]) : 0.0f;
			totframes++;
		}

		mb = (bytes > 1024.0f * 1024.0f);

		sprintf(mem_info, "%i frames in memory (%.1f %s)",
			totframes,
			bytes / (mb ? 1024.0f * 1024.0f : 1024.0f),
			mb ? "Mb" : "kb");
	}

	if(cache->flag & PTCACHE_OUTDATED) {
		sprintf(cache->info, "%s, cache is outdated!", mem_info);
	}
	else if(cache->flag & PTCACHE_FRAMES_SKIPPED) {
		sprintf(cache->info, "%s, not exact since frame %i.", mem_info, cache->last_exact);
	}
	else
		sprintf(cache->info, "%s.", mem_info);
}
