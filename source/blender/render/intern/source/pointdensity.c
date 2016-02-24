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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/pointdensity.c
 *  \ingroup render
 */


#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_noise.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "BKE_DerivedMesh.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_colortools.h"

#include "DNA_meshdata_types.h"
#include "DNA_texture_types.h"
#include "DNA_particle_types.h"

#include "render_types.h"
#include "texture.h"
#include "pointdensity.h"

#include "RE_render_ext.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static ThreadMutex sample_mutex = PTHREAD_MUTEX_INITIALIZER;

static int point_data_used(PointDensity *pd)
{
	int pd_bitflag = 0;

	if (pd->source == TEX_PD_PSYS) {
		if ((pd->noise_influence == TEX_PD_NOISE_VEL) ||
		    (pd->falloff_type == TEX_PD_FALLOFF_PARTICLE_VEL) ||
		    (pd->color_source == TEX_PD_COLOR_PARTVEL) ||
		    (pd->color_source == TEX_PD_COLOR_PARTSPEED))
		{
			pd_bitflag |= POINT_DATA_VEL;
		}
		if ((pd->noise_influence == TEX_PD_NOISE_AGE) ||
		    (pd->color_source == TEX_PD_COLOR_PARTAGE) ||
		    (pd->falloff_type == TEX_PD_FALLOFF_PARTICLE_AGE))
		{
			pd_bitflag |= POINT_DATA_LIFE;
		}
	}

	return pd_bitflag;
}


/* additional data stored alongside the point density BVH,
 * accessible by point index number to retrieve other information
 * such as particle velocity or lifetime */
static void alloc_point_data(PointDensity *pd, int total_particles, int point_data_used)
{
	int data_size = 0;

	if (point_data_used & POINT_DATA_VEL) {
		/* store 3 channels of velocity data */
		data_size += 3;
	}
	if (point_data_used & POINT_DATA_LIFE) {
		/* store 1 channel of lifetime data */
		data_size += 1;
	}

	if (data_size) {
		pd->point_data = MEM_mallocN(sizeof(float) * data_size * total_particles,
		                             "particle point data");
	}
}

static void pointdensity_cache_psys(Scene *scene,
                                    PointDensity *pd,
                                    Object *ob,
                                    ParticleSystem *psys,
                                    float viewmat[4][4],
                                    float winmat[4][4],
                                    int winx, int winy,
                                    const bool use_render_params)
{
	DerivedMesh *dm;
	ParticleKey state;
	ParticleCacheKey *cache;
	ParticleSimulationData sim = {NULL};
	ParticleData *pa = NULL;
	float cfra = BKE_scene_frame_get(scene);
	int i /*, childexists*/ /* UNUSED */;
	int total_particles, offset = 0;
	int data_used = point_data_used(pd);
	float partco[3];

	/* init everything */
	if (!psys || !ob || !pd) {
		return;
	}

	/* Just to create a valid rendering context for particles */
	if (use_render_params) {
		psys_render_set(ob, psys, viewmat, winmat, winx, winy, 0);
	}

	if (use_render_params) {
		dm = mesh_create_derived_render(scene,
		                                ob,
		                                CD_MASK_BAREMESH | CD_MASK_MTFACE | CD_MASK_MCOL);
	}
	else {
		dm = mesh_get_derived_final(scene,
		                            ob,
		                            CD_MASK_BAREMESH | CD_MASK_MTFACE | CD_MASK_MCOL);
	}

	if (!psys_check_enabled(ob, psys)) {
		psys_render_restore(ob, psys);
		return;
	}

	sim.scene = scene;
	sim.ob = ob;
	sim.psys = psys;
	sim.psmd = psys_get_modifier(ob, psys);

	/* in case ob->imat isn't up-to-date */
	invert_m4_m4(ob->imat, ob->obmat);

	total_particles = psys->totpart + psys->totchild;
	psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

	pd->point_tree = BLI_bvhtree_new(total_particles, 0.0, 4, 6);
	alloc_point_data(pd, total_particles, data_used);
	pd->totpoints = total_particles;
	if (data_used & POINT_DATA_VEL) {
		offset = pd->totpoints * 3;
	}

#if 0 /* UNUSED */
	if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
		childexists = 1;
#endif

	for (i = 0, pa = psys->particles; i < total_particles; i++, pa++) {

		if (psys->part->type == PART_HAIR) {
			/* hair particles */
			if (i < psys->totpart && psys->pathcache)
				cache = psys->pathcache[i];
			else if (i >= psys->totpart && psys->childcache)
				cache = psys->childcache[i - psys->totpart];
			else
				continue;

			cache += cache->segments; /* use endpoint */

			copy_v3_v3(state.co, cache->co);
			zero_v3(state.vel);
			state.time = 0.0f;
		}
		else {
			/* emitter particles */
			state.time = cfra;

			if (!psys_get_particle_state(&sim, i, &state, 0))
				continue;

			if (data_used & POINT_DATA_LIFE) {
				if (i < psys->totpart) {
					state.time = (cfra - pa->time) / pa->lifetime;
				}
				else {
					ChildParticle *cpa = (psys->child + i) - psys->totpart;
					float pa_birthtime, pa_dietime;

					state.time = psys_get_child_time(psys, cpa, cfra, &pa_birthtime, &pa_dietime);
				}
			}
		}

		copy_v3_v3(partco, state.co);

		if (pd->psys_cache_space == TEX_PD_OBJECTSPACE)
			mul_m4_v3(ob->imat, partco);
		else if (pd->psys_cache_space == TEX_PD_OBJECTLOC) {
			sub_v3_v3(partco, ob->loc);
		}
		else {
			/* TEX_PD_WORLDSPACE */
		}

		BLI_bvhtree_insert(pd->point_tree, i, partco, 1);

		if (data_used & POINT_DATA_VEL) {
			pd->point_data[i * 3 + 0] = state.vel[0];
			pd->point_data[i * 3 + 1] = state.vel[1];
			pd->point_data[i * 3 + 2] = state.vel[2];
		}
		if (data_used & POINT_DATA_LIFE) {
			pd->point_data[offset + i] = state.time;
		}
	}

	BLI_bvhtree_balance(pd->point_tree);
	dm->release(dm);

	if (psys->lattice_deform_data) {
		end_latt_deform(psys->lattice_deform_data);
		psys->lattice_deform_data = NULL;
	}

	if (use_render_params) {
		psys_render_restore(ob, psys);
	}
}


static void pointdensity_cache_object(Scene *scene,
                                      PointDensity *pd,
                                      Object *ob,
                                      const bool use_render_params)
{
	int i;
	DerivedMesh *dm;
	MVert *mvert = NULL;

	if (use_render_params) {
		dm = mesh_create_derived_render(scene,
		                                ob,
		                                CD_MASK_BAREMESH | CD_MASK_MTFACE | CD_MASK_MCOL);
	}
	else {
		dm = mesh_get_derived_final(scene,
		                            ob,
		                            CD_MASK_BAREMESH | CD_MASK_MTFACE | CD_MASK_MCOL);

	}
	mvert = dm->getVertArray(dm);	/* local object space */

	pd->totpoints = dm->getNumVerts(dm);
	if (pd->totpoints == 0) {
		return;
	}

	pd->point_tree = BLI_bvhtree_new(pd->totpoints, 0.0, 4, 6);

	for (i = 0; i < pd->totpoints; i++, mvert++) {
		float co[3];

		copy_v3_v3(co, mvert->co);

		switch (pd->ob_cache_space) {
			case TEX_PD_OBJECTSPACE:
				break;
			case TEX_PD_OBJECTLOC:
				mul_m4_v3(ob->obmat, co);
				sub_v3_v3(co, ob->loc);
				break;
			case TEX_PD_WORLDSPACE:
			default:
				mul_m4_v3(ob->obmat, co);
				break;
		}

		BLI_bvhtree_insert(pd->point_tree, i, co, 1);
	}

	BLI_bvhtree_balance(pd->point_tree);
	dm->release(dm);

}

static void cache_pointdensity_ex(Scene *scene,
                                  PointDensity *pd,
                                  float viewmat[4][4],
                                  float winmat[4][4],
                                  int winx, int winy,
                                  const bool use_render_params)
{
	if (pd == NULL) {
		return;
	}

	if (pd->point_tree) {
		BLI_bvhtree_free(pd->point_tree);
		pd->point_tree = NULL;
	}

	if (pd->source == TEX_PD_PSYS) {
		Object *ob = pd->object;
		ParticleSystem *psys;

		if (!ob || !pd->psys) {
			return;
		}

		psys = BLI_findlink(&ob->particlesystem, pd->psys - 1);
		if (!psys) {
			return;
		}

		pointdensity_cache_psys(scene,
		                        pd,
		                        ob,
		                        psys,
		                        viewmat, winmat,
		                        winx, winy,
		                        use_render_params);
	}
	else if (pd->source == TEX_PD_OBJECT) {
		Object *ob = pd->object;
		if (ob && ob->type == OB_MESH)
			pointdensity_cache_object(scene, pd, ob, use_render_params);
	}
}

void cache_pointdensity(Render *re, PointDensity *pd)
{
	cache_pointdensity_ex(re->scene,
	                      pd,
	                      re->viewmat, re->winmat,
	                      re->winx, re->winy,
	                      true);
}

void free_pointdensity(PointDensity *pd)
{
	if (pd == NULL) {
		return;
	}

	if (pd->point_tree) {
		BLI_bvhtree_free(pd->point_tree);
		pd->point_tree = NULL;
	}

	if (pd->point_data) {
		MEM_freeN(pd->point_data);
		pd->point_data = NULL;
	}
	pd->totpoints = 0;
}

void make_pointdensities(Render *re)
{
	Tex *tex;

	if (re->scene->r.scemode & R_BUTS_PREVIEW) {
		return;
	}

	re->i.infostr = IFACE_("Caching Point Densities");
	re->stats_draw(re->sdh, &re->i);

	for (tex = re->main->tex.first; tex != NULL; tex = tex->id.next) {
		if (tex->id.us && tex->type == TEX_POINTDENSITY) {
			cache_pointdensity(re, tex->pd);
		}
	}

	re->i.infostr = NULL;
	re->stats_draw(re->sdh, &re->i);
}

void free_pointdensities(Render *re)
{
	Tex *tex;

	if (re->scene->r.scemode & R_BUTS_PREVIEW)
		return;

	for (tex = re->main->tex.first; tex != NULL; tex = tex->id.next) {
		if (tex->id.us && tex->type == TEX_POINTDENSITY) {
			free_pointdensity(tex->pd);
		}
	}
}

typedef struct PointDensityRangeData {
	float *density;
	float squared_radius;
	const float *point_data;
	float *vec;
	float softness;
	short falloff_type;
	short noise_influence;
	float *age;
	int point_data_used;
	int offset;
	struct CurveMapping *density_curve;
	float velscale;
} PointDensityRangeData;

static void accum_density(void *userdata, int index, float squared_dist)
{
	PointDensityRangeData *pdr = (PointDensityRangeData *)userdata;
	const float dist = (pdr->squared_radius - squared_dist) / pdr->squared_radius * 0.5f;
	float density = 0.0f;

	if (pdr->point_data_used & POINT_DATA_VEL) {
		pdr->vec[0] += pdr->point_data[index * 3 + 0]; // * density;
		pdr->vec[1] += pdr->point_data[index * 3 + 1]; // * density;
		pdr->vec[2] += pdr->point_data[index * 3 + 2]; // * density;
	}
	if (pdr->point_data_used & POINT_DATA_LIFE) {
		*pdr->age += pdr->point_data[pdr->offset + index]; // * density;
	}

	if (pdr->falloff_type == TEX_PD_FALLOFF_STD)
		density = dist;
	else if (pdr->falloff_type == TEX_PD_FALLOFF_SMOOTH)
		density = 3.0f * dist * dist - 2.0f * dist * dist * dist;
	else if (pdr->falloff_type == TEX_PD_FALLOFF_SOFT)
		density = pow(dist, pdr->softness);
	else if (pdr->falloff_type == TEX_PD_FALLOFF_CONSTANT)
		density = pdr->squared_radius;
	else if (pdr->falloff_type == TEX_PD_FALLOFF_ROOT)
		density = sqrtf(dist);
	else if (pdr->falloff_type == TEX_PD_FALLOFF_PARTICLE_AGE) {
		if (pdr->point_data_used & POINT_DATA_LIFE)
			density = dist * MIN2(pdr->point_data[pdr->offset + index], 1.0f);
		else
			density = dist;
	}
	else if (pdr->falloff_type == TEX_PD_FALLOFF_PARTICLE_VEL) {
		if (pdr->point_data_used & POINT_DATA_VEL)
			density = dist * len_v3(pdr->point_data + index * 3) * pdr->velscale;
		else
			density = dist;
	}

	if (pdr->density_curve && dist != 0.0f) {
		curvemapping_initialize(pdr->density_curve);
		density = curvemapping_evaluateF(pdr->density_curve, 0, density / dist) * dist;
	}

	*pdr->density += density;
}


static void init_pointdensityrangedata(PointDensity *pd, PointDensityRangeData *pdr,
	float *density, float *vec, float *age, struct CurveMapping *density_curve, float velscale)
{
	pdr->squared_radius = pd->radius * pd->radius;
	pdr->density = density;
	pdr->point_data = pd->point_data;
	pdr->falloff_type = pd->falloff_type;
	pdr->vec = vec;
	pdr->age = age;
	pdr->softness = pd->falloff_softness;
	pdr->noise_influence = pd->noise_influence;
	pdr->point_data_used = point_data_used(pd);
	pdr->offset = (pdr->point_data_used & POINT_DATA_VEL) ? pd->totpoints * 3 : 0;
	pdr->density_curve = density_curve;
	pdr->velscale = velscale;
}


static int pointdensity(PointDensity *pd,
                        const float texvec[3],
                        TexResult *texres,
                        float *r_age,
                        float r_vec[3])
{
	int retval = TEX_INT;
	PointDensityRangeData pdr;
	float density = 0.0f, age = 0.0f, time = 0.0f;
	float vec[3] = {0.0f, 0.0f, 0.0f}, co[3];
	float turb, noise_fac;
	int num = 0;

	texres->tin = 0.0f;

	if ((!pd) || (!pd->point_tree))
		return 0;

	init_pointdensityrangedata(pd, &pdr, &density, vec, &age,
	        (pd->flag & TEX_PD_FALLOFF_CURVE ? pd->falloff_curve : NULL),
	        pd->falloff_speed_scale * 0.001f);
	noise_fac = pd->noise_fac * 0.5f;	/* better default */

	copy_v3_v3(co, texvec);

	if (point_data_used(pd)) {
		/* does a BVH lookup to find accumulated density and additional point data *
		 * stores particle velocity vector in 'vec', and particle lifetime in 'time' */
		num = BLI_bvhtree_range_query(pd->point_tree, co, pd->radius, accum_density, &pdr);
		if (num > 0) {
			age /= num;
			mul_v3_fl(vec, 1.0f / num);
		}

		/* reset */
		density = vec[0] = vec[1] = vec[2] = 0.0f;
	}

	if (pd->flag & TEX_PD_TURBULENCE) {

		if (pd->noise_influence == TEX_PD_NOISE_AGE) {
			turb = BLI_gTurbulence(pd->noise_size, texvec[0] + age, texvec[1] + age, texvec[2] + age,
			                       pd->noise_depth, 0, pd->noise_basis);
		}
		else if (pd->noise_influence == TEX_PD_NOISE_TIME) {
			time = R.r.cfra / (float)R.r.efra;
			turb = BLI_gTurbulence(pd->noise_size, texvec[0] + time, texvec[1] + time, texvec[2] + time,
			                       pd->noise_depth, 0, pd->noise_basis);
			//turb = BLI_turbulence(pd->noise_size, texvec[0]+time, texvec[1]+time, texvec[2]+time, pd->noise_depth);
		}
		else {
			turb = BLI_gTurbulence(pd->noise_size, texvec[0] + vec[0], texvec[1] + vec[1], texvec[2] + vec[2],
			                       pd->noise_depth, 0, pd->noise_basis);
		}

		turb -= 0.5f;	/* re-center 0.0-1.0 range around 0 to prevent offsetting result */

		/* now we have an offset coordinate to use for the density lookup */
		co[0] = texvec[0] + noise_fac * turb;
		co[1] = texvec[1] + noise_fac * turb;
		co[2] = texvec[2] + noise_fac * turb;
	}

	/* BVH query with the potentially perturbed coordinates */
	num = BLI_bvhtree_range_query(pd->point_tree, co, pd->radius, accum_density, &pdr);
	if (num > 0) {
		age /= num;
		mul_v3_fl(vec, 1.0f / num);
	}

	texres->tin = density;
	if (r_age != NULL) {
		*r_age = age;
	}
	if (r_vec != NULL) {
		copy_v3_v3(r_vec, vec);
	}

	return retval;
}

static int pointdensity_color(PointDensity *pd, TexResult *texres, float age, const float vec[3])
{
	int retval = 0;
	float col[4];

	retval |= TEX_RGB;

	switch (pd->color_source) {
		case TEX_PD_COLOR_PARTAGE:
			if (pd->coba) {
				if (do_colorband(pd->coba, age, col)) {
					texres->talpha = true;
					copy_v3_v3(&texres->tr, col);
					texres->tin *= col[3];
					texres->ta = texres->tin;
				}
			}
			break;
		case TEX_PD_COLOR_PARTSPEED:
		{
			float speed = len_v3(vec) * pd->speed_scale;

			if (pd->coba) {
				if (do_colorband(pd->coba, speed, col)) {
					texres->talpha = true;
					copy_v3_v3(&texres->tr, col);
					texres->tin *= col[3];
					texres->ta = texres->tin;
				}
			}
			break;
		}
		case TEX_PD_COLOR_PARTVEL:
			texres->talpha = true;
			mul_v3_v3fl(&texres->tr, vec, pd->speed_scale);
			texres->ta = texres->tin;
			break;
		case TEX_PD_COLOR_CONSTANT:
		default:
			texres->tr = texres->tg = texres->tb = texres->ta = 1.0f;
			break;
	}
	
	return retval;
}

int pointdensitytex(Tex *tex, const float texvec[3], TexResult *texres)
{
	PointDensity *pd = tex->pd;
	float age = 0.0f;
	float vec[3] = {0.0f, 0.0f, 0.0f};
	int retval = pointdensity(pd, texvec, texres, &age, vec);

	BRICONT;

	if (pd->color_source == TEX_PD_COLOR_CONSTANT)
		return retval;

	retval |= pointdensity_color(pd, texres, age, vec);
	BRICONTRGB;

	return retval;

#if 0
	if (texres->nor!=NULL) {
		texres->nor[0] = texres->nor[1] = texres->nor[2] = 0.0f;
	}
#endif
}

static void sample_dummy_point_density(int resolution, float *values)
{
	memset(values, 0, sizeof(float) * 4 * resolution * resolution * resolution);
}

static void particle_system_minmax(Scene *scene,
                                   Object *object,
                                   ParticleSystem *psys,
                                   float radius,
                                   const bool use_render_params,
                                   float min[3], float max[3])
{
	const float size[3] = {radius, radius, radius};
	const float cfra = BKE_scene_frame_get(scene);
	ParticleSettings *part = psys->part;
	ParticleSimulationData sim = {NULL};
	ParticleData *pa = NULL;
	int i;
	int total_particles;
	float mat[4][4], imat[4][4];

	INIT_MINMAX(min, max);
	if (part->type == PART_HAIR) {
		/* TOOD(sergey): Not supported currently. */
		return;
	}

	unit_m4(mat);
	if (use_render_params) {
		psys_render_set(object, psys, mat, mat, 1, 1, 0);
	}

	sim.scene = scene;
	sim.ob = object;
	sim.psys = psys;
	sim.psmd = psys_get_modifier(object, psys);

	invert_m4_m4(imat, object->obmat);
	total_particles = psys->totpart + psys->totchild;
	psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

	for (i = 0, pa = psys->particles; i < total_particles; i++, pa++) {
		float co_object[3], co_min[3], co_max[3];
		ParticleKey state;
		state.time = cfra;
		if (!psys_get_particle_state(&sim, i, &state, 0)) {
			continue;
		}
		mul_v3_m4v3(co_object, imat, state.co);
		sub_v3_v3v3(co_min, co_object, size);
		add_v3_v3v3(co_max, co_object, size);
		minmax_v3v3_v3(min, max, co_min);
		minmax_v3v3_v3(min, max, co_max);
	}

	if (psys->lattice_deform_data) {
		end_latt_deform(psys->lattice_deform_data);
		psys->lattice_deform_data = NULL;
	}

	if (use_render_params) {
		psys_render_restore(object, psys);
	}
}

void RE_point_density_cache(
        Scene *scene,
        PointDensity *pd,
        const bool use_render_params)
{
	float mat[4][4];
	/* Same matricies/resolution as dupli_render_particle_set(). */
	unit_m4(mat);
	BLI_mutex_lock(&sample_mutex);
	cache_pointdensity_ex(scene, pd, mat, mat, 1, 1, use_render_params);
	BLI_mutex_unlock(&sample_mutex);
}

void RE_point_density_minmax(
        struct Scene *scene,
        struct PointDensity *pd,
        const bool use_render_params,
        float r_min[3], float r_max[3])
{
	Object *object = pd->object;
	if (object == NULL) {
		zero_v3(r_min);
		zero_v3(r_max);
		return;
	}
	if (pd->source == TEX_PD_PSYS) {
		ParticleSystem *psys;
		if (pd->psys == 0) {
			zero_v3(r_min);
			zero_v3(r_max);
			return;
		}
		psys = BLI_findlink(&object->particlesystem, pd->psys - 1);
		if (psys == NULL) {
			zero_v3(r_min);
			zero_v3(r_max);
			return;
		}
		particle_system_minmax(scene,
		                       object,
		                       psys,
		                       pd->radius,
		                       use_render_params,
		                       r_min, r_max);
	}
	else {
		float radius[3] = {pd->radius, pd->radius, pd->radius};
		float *loc, *size;
		BKE_object_obdata_texspace_get(pd->object, NULL, &loc, &size, NULL);
		sub_v3_v3v3(r_min, loc, size);
		add_v3_v3v3(r_max, loc, size);
		/* Adjust texture space to include density points on the boundaries. */
		sub_v3_v3(r_min, radius);
		add_v3_v3(r_max, radius);
	}
}

typedef struct SampleCallbackData {
	PointDensity *pd;
	int resolution;
	float *min, *dim;
	float *values;
} SampleCallbackData;

static void point_density_sample_func(void *data_v, const int iter)
{
	SampleCallbackData *data = (SampleCallbackData *)data_v;

	const int resolution = data->resolution;
	const int resolution2 = resolution * resolution;
	const float *min = data->min, *dim = data->dim;
	PointDensity *pd = data->pd;
	float *values = data->values;

	size_t z = (size_t)iter;
	for (size_t y = 0; y < resolution; ++y) {
		for (size_t x = 0; x < resolution; ++x) {
			size_t index = z * resolution2 + y * resolution + x;
			float texvec[3];
			float age, vec[3];
			TexResult texres;

			copy_v3_v3(texvec, min);
			texvec[0] += dim[0] * (float)x / (float)resolution;
			texvec[1] += dim[1] * (float)y / (float)resolution;
			texvec[2] += dim[2] * (float)z / (float)resolution;

			pointdensity(pd, texvec, &texres, &age, vec);
			pointdensity_color(pd, &texres, age, vec);

			copy_v3_v3(&values[index*4 + 0], &texres.tr);
			values[index*4 + 3] = texres.tin;
		}
	}
}

/* NOTE 1: Requires RE_point_density_cache() to be called first.
 * NOTE 2: Frees point density structure after sampling.
 */
void RE_point_density_sample(
        Scene *scene,
        PointDensity *pd,
        const int resolution,
        const bool use_render_params,
        float *values)
{
	Object *object = pd->object;
	float min[3], max[3], dim[3];

	/* TODO(sergey): Implement some sort of assert() that point density
	 * was cached already.
	 */

	if (object == NULL) {
		sample_dummy_point_density(resolution, values);
		return;
	}

	BLI_mutex_lock(&sample_mutex);
	RE_point_density_minmax(scene,
	                        pd,
	                        use_render_params,
	                        min,
	                        max);
	BLI_mutex_unlock(&sample_mutex);
	sub_v3_v3v3(dim, max, min);
	if (dim[0] <= 0.0f || dim[1] <= 0.0f || dim[2] <= 0.0f) {
		sample_dummy_point_density(resolution, values);
		return;
	}

	SampleCallbackData data;
	data.pd = pd;
	data.resolution = resolution;
	data.min = min;
	data.dim = dim;
	data.values = values;
	BLI_task_parallel_range(0,
	                        resolution,
	                        &data,
	                        point_density_sample_func,
	                        resolution > 32);

	free_pointdensity(pd);
}

void RE_point_density_free(struct PointDensity *pd)
{
	free_pointdensity(pd);
}
