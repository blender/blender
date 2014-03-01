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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *                 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/smoke.c
 *  \ingroup bke
 */


/* Part of the code copied from elbeem fluid library, copyright by Nils Thuerey */

#include <GL/glew.h>

#include "MEM_guardedalloc.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h> /* memset */

#include "BLI_linklist.h"
#include "BLI_rand.h"
#include "BLI_jitter.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_kdtree.h"
#include "BLI_kdopbvh.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_voxel.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_customdata_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"

#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_bvhutils.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_collision.h"
#include "BKE_constraint.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_effect.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_texture.h"

#include "RE_shader_ext.h"

/* UNUSED so far, may be enabled later */
/* #define USE_SMOKE_COLLISION_DM */

#include "smoke_API.h"

#ifdef WITH_SMOKE

#ifdef _WIN32
#include <time.h>
#include <stdio.h>
#include <conio.h>
#include <windows.h>

static LARGE_INTEGER liFrequency;
static LARGE_INTEGER liStartTime;
static LARGE_INTEGER liCurrentTime;

static void tstart(void)
{
	QueryPerformanceFrequency(&liFrequency);
	QueryPerformanceCounter(&liStartTime);
}
static void tend(void)
{
	QueryPerformanceCounter(&liCurrentTime);
}
static double UNUSED_FUNCTION(tval) (void)
{
	return ((double)( (liCurrentTime.QuadPart - liStartTime.QuadPart) * (double)1000.0 / (double)liFrequency.QuadPart));
}
#else
#include <sys/time.h>
static struct timeval _tstart, _tend;
static struct timezone tz;
static void tstart(void)
{
	gettimeofday(&_tstart, &tz);
}
static void tend(void)
{
	gettimeofday(&_tend, &tz);
}

static double UNUSED_FUNCTION(tval) (void)
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec * 1000 + ( double ) _tstart.tv_usec / (1000);
	t2 = ( double ) _tend.tv_sec * 1000 + ( double ) _tend.tv_usec / (1000);
	return t2 - t1;
}
#endif

struct Object;
struct Scene;
struct DerivedMesh;
struct SmokeModifierData;

// timestep default value for nice appearance 0.1f
#define DT_DEFAULT 0.1f

#define ADD_IF_LOWER_POS(a, b) (MIN2((a) + (b), MAX2((a), (b))))
#define ADD_IF_LOWER_NEG(a, b) (MAX2((a) + (b), MIN2((a), (b))))
#define ADD_IF_LOWER(a, b) (((b) > 0) ? ADD_IF_LOWER_POS((a), (b)) : ADD_IF_LOWER_NEG((a), (b)))

#else /* WITH_SMOKE */

/* Stubs to use when smoke is disabled */
struct WTURBULENCE *smoke_turbulence_init(int *UNUSED(res), int UNUSED(amplify), int UNUSED(noisetype), const char *UNUSED(noisefile_path), int UNUSED(use_fire), int UNUSED(use_colors)) { return NULL; }
//struct FLUID_3D *smoke_init(int *UNUSED(res), float *UNUSED(dx), float *UNUSED(dtdef), int UNUSED(use_heat), int UNUSED(use_fire), int UNUSED(use_colors)) { return NULL; }
void smoke_free(struct FLUID_3D *UNUSED(fluid)) {}
float *smoke_get_density(struct FLUID_3D *UNUSED(fluid)) { return NULL; }
void smoke_turbulence_free(struct WTURBULENCE *UNUSED(wt)) {}
void smoke_initWaveletBlenderRNA(struct WTURBULENCE *UNUSED(wt), float *UNUSED(strength)) {}
void smoke_initBlenderRNA(struct FLUID_3D *UNUSED(fluid), float *UNUSED(alpha), float *UNUSED(beta), float *UNUSED(dt_factor), float *UNUSED(vorticity),
                          int *UNUSED(border_colli), float *UNUSED(burning_rate), float *UNUSED(flame_smoke), float *UNUSED(flame_smoke_color),
                          float *UNUSED(flame_vorticity), float *UNUSED(flame_ignition_temp), float *UNUSED(flame_max_temp)) {}
struct DerivedMesh *smokeModifier_do(SmokeModifierData *UNUSED(smd), Scene *UNUSED(scene), Object *UNUSED(ob), DerivedMesh *UNUSED(dm), bool UNUSED(for_render)) { return NULL; }
float smoke_get_velocity_at(struct Object *UNUSED(ob), float UNUSED(position[3]), float UNUSED(velocity[3])) { return 0.0f; }
void flame_get_spectrum(unsigned char *UNUSED(spec), int UNUSED(width), float UNUSED(t1), float UNUSED(t2)) {}

#endif /* WITH_SMOKE */

#ifdef WITH_SMOKE

void smoke_reallocate_fluid(SmokeDomainSettings *sds, float dx, int res[3], int free_old)
{
	int use_heat = (sds->active_fields & SM_ACTIVE_HEAT);
	int use_fire = (sds->active_fields & SM_ACTIVE_FIRE);
	int use_colors = (sds->active_fields & SM_ACTIVE_COLORS);

	if (free_old && sds->fluid)
		smoke_free(sds->fluid);
	if (!min_iii(res[0], res[1], res[2])) {
		sds->fluid = NULL;
		return;
	}
	sds->fluid = smoke_init(res, dx, DT_DEFAULT, use_heat, use_fire, use_colors);
	smoke_initBlenderRNA(sds->fluid, &(sds->alpha), &(sds->beta), &(sds->time_scale), &(sds->vorticity), &(sds->border_collisions),
	                     &(sds->burning_rate), &(sds->flame_smoke), sds->flame_smoke_color, &(sds->flame_vorticity), &(sds->flame_ignition), &(sds->flame_max_temp));

	/* reallocate shadow buffer */
	if (sds->shadow)
		MEM_freeN(sds->shadow);
	sds->shadow = MEM_callocN(sizeof(float) * res[0] * res[1] * res[2], "SmokeDomainShadow");
}

void smoke_reallocate_highres_fluid(SmokeDomainSettings *sds, float dx, int res[3], int free_old)
{
	int use_fire = (sds->active_fields & (SM_ACTIVE_HEAT | SM_ACTIVE_FIRE));
	int use_colors = (sds->active_fields & SM_ACTIVE_COLORS);

	if (free_old && sds->wt)
		smoke_turbulence_free(sds->wt);
	if (!min_iii(res[0], res[1], res[2])) {
		sds->wt = NULL;
		return;
	}

	/* smoke_turbulence_init uses non-threadsafe functions from fftw3 lib (like fftw_plan & co). */
	BLI_lock_thread(LOCK_FFTW);

	sds->wt = smoke_turbulence_init(res, sds->amplify + 1, sds->noise, BLI_temporary_dir(), use_fire, use_colors);

	BLI_unlock_thread(LOCK_FFTW);

	sds->res_wt[0] = res[0] * (sds->amplify + 1);
	sds->res_wt[1] = res[1] * (sds->amplify + 1);
	sds->res_wt[2] = res[2] * (sds->amplify + 1);
	sds->dx_wt = dx / (sds->amplify + 1);
	smoke_initWaveletBlenderRNA(sds->wt, &(sds->strength));
}

/* convert global position to domain cell space */
static void smoke_pos_to_cell(SmokeDomainSettings *sds, float pos[3])
{
	mul_m4_v3(sds->imat, pos);
	sub_v3_v3(pos, sds->p0);
	pos[0] *= 1.0f / sds->cell_size[0];
	pos[1] *= 1.0f / sds->cell_size[1];
	pos[2] *= 1.0f / sds->cell_size[2];
}

/* set domain transformations and base resolution from object derivedmesh */
static void smoke_set_domain_from_derivedmesh(SmokeDomainSettings *sds, Object *ob, DerivedMesh *dm, int init_resolution)
{
	size_t i;
	float min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
	float size[3];
	MVert *verts = dm->getVertArray(dm);
	float scale = 0.0;
	int res;

	res = sds->maxres;

	// get BB of domain
	for (i = 0; i < dm->getNumVerts(dm); i++)
	{
		// min BB
		min[0] = MIN2(min[0], verts[i].co[0]);
		min[1] = MIN2(min[1], verts[i].co[1]);
		min[2] = MIN2(min[2], verts[i].co[2]);

		// max BB
		max[0] = MAX2(max[0], verts[i].co[0]);
		max[1] = MAX2(max[1], verts[i].co[1]);
		max[2] = MAX2(max[2], verts[i].co[2]);
	}

	/* set domain bounds */
	copy_v3_v3(sds->p0, min);
	copy_v3_v3(sds->p1, max);
	sds->dx = 1.0f / res;

	/* calculate domain dimensions */
	sub_v3_v3v3(size, max, min);
	if (init_resolution) {
		zero_v3_int(sds->base_res);
		copy_v3_v3(sds->cell_size, size);
	}
	/* apply object scale */
	for (i = 0; i < 3; i++) {
		size[i] = fabs(size[i] * ob->size[i]);
	}
	copy_v3_v3(sds->global_size, size);
	copy_v3_v3(sds->dp0, min);

	invert_m4_m4(sds->imat, ob->obmat);

	// prevent crash when initializing a plane as domain
	if (!init_resolution || (size[0] < FLT_EPSILON) || (size[1] < FLT_EPSILON) || (size[2] < FLT_EPSILON))
		return;

	/* define grid resolutions from longest domain side */
	if (size[0] >= MAX2(size[1], size[2])) {
		scale = res / size[0];
		sds->scale = size[0] / fabsf(ob->size[0]);
		sds->base_res[0] = res;
		sds->base_res[1] = (int)(size[1] * scale + 0.5f);
		sds->base_res[2] = (int)(size[2] * scale + 0.5f);
	}
	else if (size[1] >= MAX2(size[0], size[2])) {
		scale = res / size[1];
		sds->scale = size[1] / fabsf(ob->size[1]);
		sds->base_res[0] = (int)(size[0] * scale + 0.5f);
		sds->base_res[1] = res;
		sds->base_res[2] = (int)(size[2] * scale + 0.5f);
	}
	else {
		scale = res / size[2];
		sds->scale = size[2] / fabsf(ob->size[2]);
		sds->base_res[0] = (int)(size[0] * scale + 0.5f);
		sds->base_res[1] = (int)(size[1] * scale + 0.5f);
		sds->base_res[2] = res;
	}

	/* set cell size */
	sds->cell_size[0] /= (float)sds->base_res[0];
	sds->cell_size[1] /= (float)sds->base_res[1];
	sds->cell_size[2] /= (float)sds->base_res[2];
}

static int smokeModifier_init(SmokeModifierData *smd, Object *ob, Scene *scene, DerivedMesh *dm)
{
	if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain && !smd->domain->fluid)
	{
		SmokeDomainSettings *sds = smd->domain;
		int res[3];
		/* set domain dimensions from derivedmesh */
		smoke_set_domain_from_derivedmesh(sds, ob, dm, TRUE);
		/* reset domain values */
		zero_v3_int(sds->shift);
		zero_v3(sds->shift_f);
		add_v3_fl(sds->shift_f, 0.5f);
		zero_v3(sds->prev_loc);
		mul_m4_v3(ob->obmat, sds->prev_loc);

		/* set resolutions */
		if (smd->domain->flags & MOD_SMOKE_ADAPTIVE_DOMAIN) {
			res[0] = res[1] = res[2] = 1; /* use minimum res for adaptive init */
		}
		else {
			VECCOPY(res, sds->base_res);
		}
		VECCOPY(sds->res, res);
		sds->total_cells = sds->res[0] * sds->res[1] * sds->res[2];
		sds->res_min[0] = sds->res_min[1] = sds->res_min[2] = 0;
		VECCOPY(sds->res_max, res);

		/* allocate fluid */
		smoke_reallocate_fluid(sds, sds->dx, sds->res, 0);

		smd->time = scene->r.cfra;

		/* allocate highres fluid */
		if (sds->flags & MOD_SMOKE_HIGHRES) {
			smoke_reallocate_highres_fluid(sds, sds->dx, sds->res, 0);
		}
		/* allocate shadow buffer */
		if (!sds->shadow)
			sds->shadow = MEM_callocN(sizeof(float) * sds->res[0] * sds->res[1] * sds->res[2], "SmokeDomainShadow");

		return 1;
	}
	else if ((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow)
	{
		smd->time = scene->r.cfra;

		return 1;
	}
	else if ((smd->type & MOD_SMOKE_TYPE_COLL))
	{
		if (!smd->coll)
		{
			smokeModifier_createType(smd);
		}

		smd->time = scene->r.cfra;

		return 1;
	}

	return 2;
}

#endif /* WITH_SMOKE */

static void smokeModifier_freeDomain(SmokeModifierData *smd)
{
	if (smd->domain)
	{
		if (smd->domain->shadow)
			MEM_freeN(smd->domain->shadow);
		smd->domain->shadow = NULL;

		if (smd->domain->fluid)
			smoke_free(smd->domain->fluid);

		if (smd->domain->fluid_mutex)
			BLI_rw_mutex_free(smd->domain->fluid_mutex);

		if (smd->domain->wt)
			smoke_turbulence_free(smd->domain->wt);

		if (smd->domain->effector_weights)
			MEM_freeN(smd->domain->effector_weights);
		smd->domain->effector_weights = NULL;

		BKE_ptcache_free_list(&(smd->domain->ptcaches[0]));
		smd->domain->point_cache[0] = NULL;

		MEM_freeN(smd->domain);
		smd->domain = NULL;
	}
}

static void smokeModifier_freeFlow(SmokeModifierData *smd)
{
	if (smd->flow)
	{
		if (smd->flow->dm) smd->flow->dm->release(smd->flow->dm);
		if (smd->flow->verts_old) MEM_freeN(smd->flow->verts_old);
		MEM_freeN(smd->flow);
		smd->flow = NULL;
	}
}

static void smokeModifier_freeCollision(SmokeModifierData *smd)
{
	if (smd->coll)
	{
		SmokeCollSettings *scs = smd->coll;

		if (scs->numverts)
		{
			if (scs->verts_old)
			{
				MEM_freeN(scs->verts_old);
				scs->verts_old = NULL;
			}
		}

		if (smd->coll->dm)
			smd->coll->dm->release(smd->coll->dm);
		smd->coll->dm = NULL;

		MEM_freeN(smd->coll);
		smd->coll = NULL;
	}
}

void smokeModifier_reset_turbulence(struct SmokeModifierData *smd)
{
	if (smd && smd->domain && smd->domain->wt)
	{
		smoke_turbulence_free(smd->domain->wt);
		smd->domain->wt = NULL;
	}
}

static void smokeModifier_reset_ex(struct SmokeModifierData *smd, bool need_lock)
{
	if (smd)
	{
		if (smd->domain)
		{
			if (smd->domain->shadow)
				MEM_freeN(smd->domain->shadow);
			smd->domain->shadow = NULL;

			if (smd->domain->fluid)
			{
				if (need_lock)
					BLI_rw_mutex_lock(smd->domain->fluid_mutex, THREAD_LOCK_WRITE);

				smoke_free(smd->domain->fluid);
				smd->domain->fluid = NULL;

				if (need_lock)
					BLI_rw_mutex_unlock(smd->domain->fluid_mutex);
			}

			smokeModifier_reset_turbulence(smd);

			smd->time = -1;
			smd->domain->total_cells = 0;
			smd->domain->active_fields = 0;
		}
		else if (smd->flow)
		{
			if (smd->flow->verts_old) MEM_freeN(smd->flow->verts_old);
			smd->flow->verts_old = NULL;
			smd->flow->numverts = 0;
		}
		else if (smd->coll)
		{
			SmokeCollSettings *scs = smd->coll;

			if (scs->numverts && scs->verts_old)
			{
				MEM_freeN(scs->verts_old);
				scs->verts_old = NULL;
			}
		}
	}
}

void smokeModifier_reset(struct SmokeModifierData *smd)
{
	smokeModifier_reset_ex(smd, true);
}

void smokeModifier_free(SmokeModifierData *smd)
{
	if (smd)
	{
		smokeModifier_freeDomain(smd);
		smokeModifier_freeFlow(smd);
		smokeModifier_freeCollision(smd);
	}
}

void smokeModifier_createType(struct SmokeModifierData *smd)
{
	if (smd)
	{
		if (smd->type & MOD_SMOKE_TYPE_DOMAIN)
		{
			if (smd->domain)
				smokeModifier_freeDomain(smd);

			smd->domain = MEM_callocN(sizeof(SmokeDomainSettings), "SmokeDomain");

			smd->domain->smd = smd;

			smd->domain->point_cache[0] = BKE_ptcache_add(&(smd->domain->ptcaches[0]));
			smd->domain->point_cache[0]->flag |= PTCACHE_DISK_CACHE;
			smd->domain->point_cache[0]->step = 1;

			/* Deprecated */
			smd->domain->point_cache[1] = NULL;
			BLI_listbase_clear(&smd->domain->ptcaches[1]);
			/* set some standard values */
			smd->domain->fluid = NULL;
			smd->domain->fluid_mutex = BLI_rw_mutex_alloc();
			smd->domain->wt = NULL;
			smd->domain->eff_group = NULL;
			smd->domain->fluid_group = NULL;
			smd->domain->coll_group = NULL;
			smd->domain->maxres = 32;
			smd->domain->amplify = 1;
			smd->domain->alpha = -0.001;
			smd->domain->beta = 0.1;
			smd->domain->time_scale = 1.0;
			smd->domain->vorticity = 2.0;
			smd->domain->border_collisions = SM_BORDER_OPEN; // open domain
			smd->domain->flags = MOD_SMOKE_DISSOLVE_LOG;
			smd->domain->highres_sampling = SM_HRES_FULLSAMPLE;
			smd->domain->strength = 2.0;
			smd->domain->noise = MOD_SMOKE_NOISEWAVE;
			smd->domain->diss_speed = 5;
			smd->domain->active_fields = 0;

			smd->domain->adapt_margin = 4;
			smd->domain->adapt_res = 0;
			smd->domain->adapt_threshold = 0.02f;

			smd->domain->burning_rate = 0.75f;
			smd->domain->flame_smoke = 1.0f;
			smd->domain->flame_vorticity = 0.5f;
			smd->domain->flame_ignition = 1.25f;
			smd->domain->flame_max_temp = 1.75f;
			/* color */
			smd->domain->flame_smoke_color[0] = 0.7f;
			smd->domain->flame_smoke_color[1] = 0.7f;
			smd->domain->flame_smoke_color[2] = 0.7f;

			smd->domain->viewsettings = MOD_SMOKE_VIEW_SHOWBIG;
			smd->domain->effector_weights = BKE_add_effector_weights(NULL);
		}
		else if (smd->type & MOD_SMOKE_TYPE_FLOW)
		{
			if (smd->flow)
				smokeModifier_freeFlow(smd);

			smd->flow = MEM_callocN(sizeof(SmokeFlowSettings), "SmokeFlow");

			smd->flow->smd = smd;

			/* set some standard values */
			smd->flow->density = 1.0f;
			smd->flow->fuel_amount = 1.0f;
			smd->flow->temp = 1.0f;
			smd->flow->flags = MOD_SMOKE_FLOW_ABSOLUTE | MOD_SMOKE_FLOW_USE_PART_SIZE;
			smd->flow->vel_multi = 1.0f;
			smd->flow->volume_density = 0.0f;
			smd->flow->surface_distance = 1.5f;
			smd->flow->source = MOD_SMOKE_FLOW_SOURCE_MESH;
			smd->flow->texture_size = 1.0f;
			smd->flow->particle_size = 1.0f;
			smd->flow->subframes = 0;

			smd->flow->color[0] = 0.7f;
			smd->flow->color[1] = 0.7f;
			smd->flow->color[2] = 0.7f;

			smd->flow->dm = NULL;
			smd->flow->psys = NULL;

		}
		else if (smd->type & MOD_SMOKE_TYPE_COLL)
		{
			if (smd->coll)
				smokeModifier_freeCollision(smd);

			smd->coll = MEM_callocN(sizeof(SmokeCollSettings), "SmokeColl");

			smd->coll->smd = smd;
			smd->coll->verts_old = NULL;
			smd->coll->numverts = 0;
			smd->coll->type = 0; // static obstacle
			smd->coll->dm = NULL;

#ifdef USE_SMOKE_COLLISION_DM
			smd->coll->dm = NULL;
#endif
		}
	}
}

void smokeModifier_copy(struct SmokeModifierData *smd, struct SmokeModifierData *tsmd)
{
	tsmd->type = smd->type;
	tsmd->time = smd->time;

	smokeModifier_createType(tsmd);

	if (tsmd->domain) {
		tsmd->domain->fluid_group = smd->domain->fluid_group;
		tsmd->domain->coll_group = smd->domain->coll_group;

		tsmd->domain->adapt_margin = smd->domain->adapt_margin;
		tsmd->domain->adapt_res = smd->domain->adapt_res;
		tsmd->domain->adapt_threshold = smd->domain->adapt_threshold;

		tsmd->domain->alpha = smd->domain->alpha;
		tsmd->domain->beta = smd->domain->beta;
		tsmd->domain->amplify = smd->domain->amplify;
		tsmd->domain->maxres = smd->domain->maxres;
		tsmd->domain->flags = smd->domain->flags;
		tsmd->domain->highres_sampling = smd->domain->highres_sampling;
		tsmd->domain->viewsettings = smd->domain->viewsettings;
		tsmd->domain->noise = smd->domain->noise;
		tsmd->domain->diss_speed = smd->domain->diss_speed;
		tsmd->domain->strength = smd->domain->strength;

		tsmd->domain->border_collisions = smd->domain->border_collisions;
		tsmd->domain->vorticity = smd->domain->vorticity;
		tsmd->domain->time_scale = smd->domain->time_scale;

		tsmd->domain->burning_rate = smd->domain->burning_rate;
		tsmd->domain->flame_smoke = smd->domain->flame_smoke;
		tsmd->domain->flame_vorticity = smd->domain->flame_vorticity;
		tsmd->domain->flame_ignition = smd->domain->flame_ignition;
		tsmd->domain->flame_max_temp = smd->domain->flame_max_temp;
		copy_v3_v3(tsmd->domain->flame_smoke_color, smd->domain->flame_smoke_color);

		MEM_freeN(tsmd->domain->effector_weights);
		tsmd->domain->effector_weights = MEM_dupallocN(smd->domain->effector_weights);
	}
	else if (tsmd->flow) {
		tsmd->flow->psys = smd->flow->psys;
		tsmd->flow->noise_texture = smd->flow->noise_texture;

		tsmd->flow->vel_multi = smd->flow->vel_multi;
		tsmd->flow->vel_normal = smd->flow->vel_normal;
		tsmd->flow->vel_random = smd->flow->vel_random;

		tsmd->flow->density = smd->flow->density;
		copy_v3_v3(tsmd->flow->color, smd->flow->color);
		tsmd->flow->fuel_amount = smd->flow->fuel_amount;
		tsmd->flow->temp = smd->flow->temp;
		tsmd->flow->volume_density = smd->flow->volume_density;
		tsmd->flow->surface_distance = smd->flow->surface_distance;
		tsmd->flow->particle_size = smd->flow->particle_size;
		tsmd->flow->subframes = smd->flow->subframes;

		tsmd->flow->texture_size = smd->flow->texture_size;
		tsmd->flow->texture_offset = smd->flow->texture_offset;
		BLI_strncpy(tsmd->flow->uvlayer_name, tsmd->flow->uvlayer_name, sizeof(tsmd->flow->uvlayer_name));
		tsmd->flow->vgroup_density = smd->flow->vgroup_density;

		tsmd->flow->type = smd->flow->type;
		tsmd->flow->source = smd->flow->source;
		tsmd->flow->texture_type = smd->flow->texture_type;
		tsmd->flow->flags = smd->flow->flags;
	}
	else if (tsmd->coll) {
		/* leave it as initialized, collision settings is mostly caches */
	}
}

#ifdef WITH_SMOKE

// forward decleration
static void smoke_calc_transparency(SmokeDomainSettings *sds, Scene *scene);
static float calc_voxel_transp(float *result, float *input, int res[3], int *pixel, float *tRay, float correct);

static int get_lamp(Scene *scene, float *light)
{
	Base *base_tmp = NULL;
	int found_lamp = 0;

	// try to find a lamp, preferably local
	for (base_tmp = scene->base.first; base_tmp; base_tmp = base_tmp->next) {
		if (base_tmp->object->type == OB_LAMP) {
			Lamp *la = base_tmp->object->data;

			if (la->type == LA_LOCAL) {
				copy_v3_v3(light, base_tmp->object->obmat[3]);
				return 1;
			}
			else if (!found_lamp) {
				copy_v3_v3(light, base_tmp->object->obmat[3]);
				found_lamp = 1;
			}
		}
	}

	return found_lamp;
}

/**********************************************************
 *	Obstacles
 **********************************************************/

static void obstacles_from_derivedmesh(Object *coll_ob, SmokeDomainSettings *sds, SmokeCollSettings *scs, unsigned char *obstacle_map, float *velocityX, float *velocityY, float *velocityZ, float dt)
{
	if (!scs->dm) return;
	{
		DerivedMesh *dm = NULL;
		MVert *mvert = NULL;
		MFace *mface = NULL;
		BVHTreeFromMesh treeData = {NULL};
		int numverts, i, z;

		float surface_distance = 0.6;

		float *vert_vel = NULL;
		int has_velocity = 0;

		tstart();

		dm = CDDM_copy(scs->dm);
		CDDM_calc_normals(dm);
		mvert = dm->getVertArray(dm);
		mface = dm->getTessFaceArray(dm);
		numverts = dm->getNumVerts(dm);

		// DG TODO
		// if (scs->type > SM_COLL_STATIC)
		// if line above is used, the code is in trouble if the object moves but is declared as "does not move"

		{
			vert_vel = MEM_callocN(sizeof(float) * numverts * 3, "smoke_obs_velocity");

			if (scs->numverts != numverts || !scs->verts_old) {
				if (scs->verts_old) MEM_freeN(scs->verts_old);

				scs->verts_old = MEM_callocN(sizeof(float) * numverts * 3, "smoke_obs_verts_old");
				scs->numverts = numverts;
			}
			else {
				has_velocity = 1;
			}
		}

		/*	Transform collider vertices to
		 *   domain grid space for fast lookups */
		for (i = 0; i < numverts; i++) {
			float n[3];
			float co[3];

			/* vert pos */
			mul_m4_v3(coll_ob->obmat, mvert[i].co);
			smoke_pos_to_cell(sds, mvert[i].co);

			/* vert normal */
			normal_short_to_float_v3(n, mvert[i].no);
			mul_mat3_m4_v3(coll_ob->obmat, n);
			mul_mat3_m4_v3(sds->imat, n);
			normalize_v3(n);
			normal_float_to_short_v3(mvert[i].no, n);

			/* vert velocity */
			VECADD(co, mvert[i].co, sds->shift);
			if (has_velocity)
			{
				sub_v3_v3v3(&vert_vel[i * 3], co, &scs->verts_old[i * 3]);
				mul_v3_fl(&vert_vel[i * 3], sds->dx / dt);
			}
			copy_v3_v3(&scs->verts_old[i * 3], co);
		}

		if (bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 4, 6)) {
#pragma omp parallel for schedule(static)
			for (z = sds->res_min[2]; z < sds->res_max[2]; z++) {
				int x, y;
				for (x = sds->res_min[0]; x < sds->res_max[0]; x++)
					for (y = sds->res_min[1]; y < sds->res_max[1]; y++) {
						int index = smoke_get_index(x - sds->res_min[0], sds->res[0], y - sds->res_min[1], sds->res[1], z - sds->res_min[2]);

						float ray_start[3] = {(float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f};
						BVHTreeNearest nearest = {0};
						nearest.index = -1;
						nearest.dist_sq = surface_distance * surface_distance; /* find_nearest uses squared distance */

						/* find the nearest point on the mesh */
						if (BLI_bvhtree_find_nearest(treeData.tree, ray_start, &nearest, treeData.nearest_callback, &treeData) != -1) {
							float weights[4];
							int v1, v2, v3, f_index = nearest.index;

							/* calculate barycentric weights for nearest point */
							v1 = mface[f_index].v1;
							v2 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v3 : mface[f_index].v2;
							v3 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v4 : mface[f_index].v3;
							interp_weights_face_v3(weights, mvert[v1].co, mvert[v2].co, mvert[v3].co, NULL, nearest.co);

							// DG TODO
							if (has_velocity)
							{
								/* apply object velocity */
								{
									float hit_vel[3];
									interp_v3_v3v3v3(hit_vel, &vert_vel[v1 * 3], &vert_vel[v2 * 3], &vert_vel[v3 * 3], weights);
									velocityX[index] += hit_vel[0];
									velocityY[index] += hit_vel[1];
									velocityZ[index] += hit_vel[2];
								}
							}

							/* tag obstacle cells */
							obstacle_map[index] = 1;

							if (has_velocity)
								obstacle_map[index] |= 8;
						}
					}
			}
		}
		/* free bvh tree */
		free_bvhtree_from_mesh(&treeData);
		dm->release(dm);

		if (vert_vel) MEM_freeN(vert_vel);
	}
}

/* Animated obstacles: dx_step = ((x_new - x_old) / totalsteps) * substep */
static void update_obstacles(Scene *scene, Object *ob, SmokeDomainSettings *sds, float dt,
                             int UNUSED(substep), int UNUSED(totalsteps))
{
	Object **collobjs = NULL;
	unsigned int numcollobj = 0;

	unsigned int collIndex;
	unsigned char *obstacles = smoke_get_obstacle(sds->fluid);
	float *velx = NULL;
	float *vely = NULL;
	float *velz = NULL;
	float *velxOrig = smoke_get_velocity_x(sds->fluid);
	float *velyOrig = smoke_get_velocity_y(sds->fluid);
	float *velzOrig = smoke_get_velocity_z(sds->fluid);
	float *density = smoke_get_density(sds->fluid);
	float *fuel = smoke_get_fuel(sds->fluid);
	float *flame = smoke_get_flame(sds->fluid);
	float *r = smoke_get_color_r(sds->fluid);
	float *g = smoke_get_color_g(sds->fluid);
	float *b = smoke_get_color_b(sds->fluid);
	unsigned int z;

	smoke_get_ob_velocity(sds->fluid, &velx, &vely, &velz);

	// TODO: delete old obstacle flags
	for (z = 0; z < sds->res[0] * sds->res[1] * sds->res[2]; z++)
	{
		if (obstacles[z] & 8) // Do not delete static obstacles
		{
			obstacles[z] = 0;
		}

		velx[z] = 0;
		vely[z] = 0;
		velz[z] = 0;
	}


	collobjs = get_collisionobjects(scene, ob, sds->coll_group, &numcollobj, eModifierType_Smoke);

	// update obstacle tags in cells
	for (collIndex = 0; collIndex < numcollobj; collIndex++)
	{
		Object *collob = collobjs[collIndex];
		SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(collob, eModifierType_Smoke);

		// DG TODO: check if modifier is active?

		if ((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll)
		{
			SmokeCollSettings *scs = smd2->coll;
			obstacles_from_derivedmesh(collob, sds, scs, obstacles, velx, vely, velz, dt);
		}
	}

	if (collobjs)
		MEM_freeN(collobjs);

	/* obstacle cells should not contain any velocity from the smoke simulation */
	for (z = 0; z < sds->res[0] * sds->res[1] * sds->res[2]; z++)
	{
		if (obstacles[z])
		{
			velxOrig[z] = 0;
			velyOrig[z] = 0;
			velzOrig[z] = 0;
			density[z] = 0;
			if (fuel) {
				fuel[z] = 0;
				flame[z] = 0;
			}
			if (r) {
				r[z] = 0;
				g[z] = 0;
				b[z] = 0;
			}
		}
	}
}


/**********************************************************
 *	Object subframe update method from dynamicpaint.c
 **********************************************************/

/* set "ignore cache" flag for all caches on this object */
static void object_cacheIgnoreClear(Object *ob, int state)
{
	ListBase pidlist;
	PTCacheID *pid;
	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);

	for (pid = pidlist.first; pid; pid = pid->next) {
		if (pid->cache) {
			if (state)
				pid->cache->flag |= PTCACHE_IGNORE_CLEAR;
			else
				pid->cache->flag &= ~PTCACHE_IGNORE_CLEAR;
		}
	}

	BLI_freelistN(&pidlist);
}

static int subframe_updateObject(Scene *scene, Object *ob, int update_mesh, int parent_recursion, float frame, bool for_render)
{
	SmokeModifierData *smd = (SmokeModifierData *)modifiers_findByType(ob, eModifierType_Smoke);
	bConstraint *con;

	/* if other is dynamic paint canvas, don't update */
	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN))
		return 1;

	/* if object has parents, update them too */
	if (parent_recursion) {
		int recursion = parent_recursion - 1;
		int is_domain = 0;
		if (ob->parent) is_domain += subframe_updateObject(scene, ob->parent, 0, recursion, frame, for_render);
		if (ob->track) is_domain += subframe_updateObject(scene, ob->track, 0, recursion, frame, for_render);

		/* skip subframe if object is parented
		 *  to vertex of a dynamic paint canvas */
		if (is_domain && (ob->partype == PARVERT1 || ob->partype == PARVERT3))
			return 0;

		/* also update constraint targets */
		for (con = ob->constraints.first; con; con = con->next) {
			bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);
			ListBase targets = {NULL, NULL};

			if (cti && cti->get_constraint_targets) {
				bConstraintTarget *ct;
				cti->get_constraint_targets(con, &targets);
				for (ct = targets.first; ct; ct = ct->next) {
					if (ct->tar)
						subframe_updateObject(scene, ct->tar, 0, recursion, frame, for_render);
				}
				/* free temp targets */
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 0);
			}
		}
	}

	/* was originally OB_RECALC_ALL - TODO - which flags are really needed??? */
	ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
	BKE_animsys_evaluate_animdata(scene, &ob->id, ob->adt, frame, ADT_RECALC_ANIM);
	if (update_mesh) {
		/* ignore cache clear during subframe updates
		 *  to not mess up cache validity */
		object_cacheIgnoreClear(ob, 1);
		BKE_object_handle_update(G.main->eval_ctx, scene, ob);
		object_cacheIgnoreClear(ob, 0);
	}
	else
		BKE_object_where_is_calc_time(scene, ob, frame);

	/* for curve following objects, parented curve has to be updated too */
	if (ob->type == OB_CURVE) {
		Curve *cu = ob->data;
		BKE_animsys_evaluate_animdata(scene, &cu->id, cu->adt, frame, ADT_RECALC_ANIM);
	}
	/* and armatures... */
	if (ob->type == OB_ARMATURE) {
		bArmature *arm = ob->data;
		BKE_animsys_evaluate_animdata(scene, &arm->id, arm->adt, frame, ADT_RECALC_ANIM);
		BKE_pose_where_is(scene, ob);
	}

	return 0;
}

/**********************************************************
 *	Flow emission code
 **********************************************************/

typedef struct EmissionMap {
	float *influence;
	float *influence_high;
	float *velocity;
	int min[3], max[3], res[3];
	int hmin[3], hmax[3], hres[3];
	int total_cells, valid;
} EmissionMap;

static void em_boundInsert(EmissionMap *em, float point[3])
{
	int i = 0;
	if (!em->valid) {
		for (; i < 3; i++) {
			em->min[i] = (int)floor(point[i]);
			em->max[i] = (int)ceil(point[i]);
		}
		em->valid = 1;
	}
	else {
		for (; i < 3; i++) {
			if (point[i] < em->min[i]) em->min[i] = (int)floor(point[i]);
			if (point[i] > em->max[i]) em->max[i] = (int)ceil(point[i]);
		}
	}
}

static void clampBoundsInDomain(SmokeDomainSettings *sds, int min[3], int max[3], float *min_vel, float *max_vel, int margin, float dt)
{
	int i;
	for (i = 0; i < 3; i++) {
		int adapt = (sds->flags & MOD_SMOKE_ADAPTIVE_DOMAIN) ? sds->adapt_res : 0;
		/* add margin */
		min[i] -= margin;
		max[i] += margin;

		/* adapt to velocity */
		if (min_vel && min_vel[i] < 0.0f) {
			min[i] += (int)floor(min_vel[i] * dt);
		}
		if (max_vel && max_vel[i] > 0.0f) {
			max[i] += (int)ceil(max_vel[i] * dt);
		}

		/* clamp within domain max size */
		CLAMP(min[i], -adapt, sds->base_res[i] + adapt);
		CLAMP(max[i], -adapt, sds->base_res[i] + adapt);
	}
}

static void em_allocateData(EmissionMap *em, int use_velocity, int hires_mul)
{
	int i, res[3];

	for (i = 0; i < 3; i++) {
		res[i] = em->max[i] - em->min[i];
		if (res[i] <= 0)
			return;
	}
	em->total_cells = res[0] * res[1] * res[2];
	copy_v3_v3_int(em->res, res);


	em->influence = MEM_callocN(sizeof(float) * em->total_cells, "smoke_flow_influence");
	if (use_velocity)
		em->velocity = MEM_callocN(sizeof(float) * em->total_cells * 3, "smoke_flow_velocity");

	/* allocate high resolution map if required */
	if (hires_mul > 1) {
		int total_cells_high = em->total_cells * (hires_mul * hires_mul * hires_mul);

		for (i = 0; i < 3; i++) {
			em->hmin[i] = em->min[i] * hires_mul;
			em->hmax[i] = em->max[i] * hires_mul;
			em->hres[i] = em->res[i] * hires_mul;
		}

		em->influence_high = MEM_callocN(sizeof(float) * total_cells_high, "smoke_flow_influence_high");
	}
	em->valid = 1;
}

static void em_freeData(EmissionMap *em)
{
	if (em->influence)
		MEM_freeN(em->influence);
	if (em->influence_high)
		MEM_freeN(em->influence_high);
	if (em->velocity)
		MEM_freeN(em->velocity);
}

static void em_combineMaps(EmissionMap *output, EmissionMap *em2, int hires_multiplier, int additive, float sample_size)
{
	int i, x, y, z;

	/* copyfill input 1 struct and clear output for new allocation */
	EmissionMap em1;
	memcpy(&em1, output, sizeof(EmissionMap));
	memset(output, 0, sizeof(EmissionMap));

	for (i = 0; i < 3; i++) {
		if (em1.valid) {
			output->min[i] = MIN2(em1.min[i], em2->min[i]);
			output->max[i] = MAX2(em1.max[i], em2->max[i]);
		}
		else {
			output->min[i] = em2->min[i];
			output->max[i] = em2->max[i];
		}
	}
	/* allocate output map */
	em_allocateData(output, (em1.velocity || em2->velocity), hires_multiplier);

	/* base resolution inputs */
	for (x = output->min[0]; x < output->max[0]; x++)
		for (y = output->min[1]; y < output->max[1]; y++)
			for (z = output->min[2]; z < output->max[2]; z++) {
				int index_out = smoke_get_index(x - output->min[0], output->res[0], y - output->min[1], output->res[1], z - output->min[2]);

				/* initialize with first input if in range */
				if (x >= em1.min[0] && x < em1.max[0] &&
					y >= em1.min[1] && y < em1.max[1] &&
					z >= em1.min[2] && z < em1.max[2]) {
					int index_in = smoke_get_index(x - em1.min[0], em1.res[0], y - em1.min[1], em1.res[1], z - em1.min[2]);

					/* values */
					output->influence[index_out] = em1.influence[index_in];
					if (output->velocity && em1.velocity) {
						copy_v3_v3(&output->velocity[index_out * 3], &em1.velocity[index_in * 3]);
					}
				}

				/* apply second input if in range */
				if (x >= em2->min[0] && x < em2->max[0] &&
					y >= em2->min[1] && y < em2->max[1] &&
					z >= em2->min[2] && z < em2->max[2]) {
					int index_in = smoke_get_index(x - em2->min[0], em2->res[0], y - em2->min[1], em2->res[1], z - em2->min[2]);

					/* values */
					if (additive) {
						output->influence[index_out] += em2->influence[index_in] * sample_size;
					}
					else {
						output->influence[index_out] = MAX2(em2->influence[index_in], output->influence[index_out]);
					}
					if (output->velocity && em2->velocity) {
						/* last sample replaces the velocity */
						output->velocity[index_out * 3]		= ADD_IF_LOWER(output->velocity[index_out * 3], em2->velocity[index_in * 3]);
						output->velocity[index_out * 3 + 1] = ADD_IF_LOWER(output->velocity[index_out * 3 + 1], em2->velocity[index_in * 3 + 1]);
						output->velocity[index_out * 3 + 2] = ADD_IF_LOWER(output->velocity[index_out * 3 + 2], em2->velocity[index_in * 3 + 2]);
					}
				}
	} // low res loop



	/* initialize high resolution input if available */
	if (output->influence_high) {
		for (x = output->hmin[0]; x < output->hmax[0]; x++)
			for (y = output->hmin[1]; y < output->hmax[1]; y++)
				for (z = output->hmin[2]; z < output->hmax[2]; z++) {
					int index_out = smoke_get_index(x - output->hmin[0], output->hres[0], y - output->hmin[1], output->hres[1], z - output->hmin[2]);

					/* initialize with first input if in range */
					if (x >= em1.hmin[0] && x < em1.hmax[0] &&
						y >= em1.hmin[1] && y < em1.hmax[1] &&
						z >= em1.hmin[2] && z < em1.hmax[2]) {
						int index_in = smoke_get_index(x - em1.hmin[0], em1.hres[0], y - em1.hmin[1], em1.hres[1], z - em1.hmin[2]);
						/* values */
						output->influence_high[index_out] = em1.influence_high[index_in];
					}

					/* apply second input if in range */
					if (x >= em2->hmin[0] && x < em2->hmax[0] &&
						y >= em2->hmin[1] && y < em2->hmax[1] &&
						z >= em2->hmin[2] && z < em2->hmax[2]) {
						int index_in = smoke_get_index(x - em2->hmin[0], em2->hres[0], y - em2->hmin[1], em2->hres[1], z - em2->hmin[2]);

						/* values */
						if (additive) {
							output->influence_high[index_out] += em2->influence_high[index_in] * sample_size;
						}
						else {
							output->influence_high[index_out] = MAX2(em2->influence_high[index_in], output->influence_high[index_out]);
						}
					}
		} // high res loop
	}

	/* free original data */
	em_freeData(&em1);
}


static void emit_from_particles(Object *flow_ob, SmokeDomainSettings *sds, SmokeFlowSettings *sfs, EmissionMap *em, Scene *scene, float dt)
{
	if (sfs && sfs->psys && sfs->psys->part && ELEM(sfs->psys->part->type, PART_EMITTER, PART_FLUID)) // is particle system selected
	{
		ParticleSimulationData sim;
		ParticleSystem *psys = sfs->psys;
		float *particle_pos;
		float *particle_vel;
		int totpart = psys->totpart, totchild;
		int p = 0;
		int valid_particles = 0;
		int bounds_margin = 1;

		/* radius based flow */
		float solid = sfs->particle_size * 0.5f;
		float smooth = 0.5f; /* add 0.5 cells of linear falloff to reduce aliasing */
		int hires_multiplier = 1;
		int i, z;
		KDTree *tree;

		sim.scene = scene;
		sim.ob = flow_ob;
		sim.psys = psys;

		/* initialize particle cache */
		if (psys->part->type == PART_HAIR) {
			// TODO: PART_HAIR not supported whatsoever
			totchild = 0;
		}
		else {
			totchild = psys->totchild * psys->part->disp / 100;
		}

		particle_pos = MEM_callocN(sizeof(float) * (totpart + totchild) * 3, "smoke_flow_particles");
		particle_vel = MEM_callocN(sizeof(float) * (totpart + totchild) * 3, "smoke_flow_particles");

		/* setup particle radius emission if enabled */
		if (sfs->flags & MOD_SMOKE_FLOW_USE_PART_SIZE) {
			tree = BLI_kdtree_new(psys->totpart + psys->totchild);

			/* check need for high resolution map */
			if ((sds->flags & MOD_SMOKE_HIGHRES) && (sds->highres_sampling == SM_HRES_FULLSAMPLE)) {
				hires_multiplier = sds->amplify + 1;
			}

			bounds_margin = (int)ceil(solid + smooth);
		}

		/* calculate local position for each particle */
		for (p = 0; p < totpart + totchild; p++)
		{
			ParticleKey state;
			float *pos;
			if (p < totpart) {
				if (psys->particles[p].flag & (PARS_NO_DISP | PARS_UNEXIST))
					continue;
			}
			else {
				/* handle child particle */
				ChildParticle *cpa = &psys->child[p - totpart];
				if (psys->particles[cpa->parent].flag & (PARS_NO_DISP | PARS_UNEXIST))
					continue;
			}

			state.time = BKE_scene_frame_get(scene); /* use scene time */
			if (psys_get_particle_state(&sim, p, &state, 0) == 0)
				continue;

			/* location */
			pos = &particle_pos[valid_particles * 3];
			copy_v3_v3(pos, state.co);
			smoke_pos_to_cell(sds, pos);

			/* velocity */
			copy_v3_v3(&particle_vel[valid_particles * 3], state.vel);
			mul_mat3_m4_v3(sds->imat, &particle_vel[valid_particles * 3]);

			if (sfs->flags & MOD_SMOKE_FLOW_USE_PART_SIZE) {
				BLI_kdtree_insert(tree, valid_particles, pos, NULL);
			}

			/* calculate emission map bounds */
			em_boundInsert(em, pos);
			valid_particles++;
		}

		/* set emission map */
		clampBoundsInDomain(sds, em->min, em->max, NULL, NULL, bounds_margin, dt);
		em_allocateData(em, sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY, hires_multiplier);

		if (!(sfs->flags & MOD_SMOKE_FLOW_USE_PART_SIZE)) {
			for (p = 0; p < valid_particles; p++)
			{
				int cell[3];
				size_t i = 0;
				size_t index = 0;
				int badcell = 0;

				/* 1. get corresponding cell */
				cell[0] = floor(particle_pos[p * 3]) - em->min[0];
				cell[1] = floor(particle_pos[p * 3 + 1]) - em->min[1];
				cell[2] = floor(particle_pos[p * 3 + 2]) - em->min[2];
				/* check if cell is valid (in the domain boundary) */
				for (i = 0; i < 3; i++) {
					if ((cell[i] > em->res[i] - 1) || (cell[i] < 0)) {
						badcell = 1;
						break;
					}
				}
				if (badcell)
					continue;
				/* get cell index */
				index = smoke_get_index(cell[0], em->res[0], cell[1], em->res[1], cell[2]);
				/* Add influence to emission map */
				em->influence[index] = 1.0f;
				/* Uses particle velocity as initial velocity for smoke */
				if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY && (psys->part->phystype != PART_PHYS_NO))
				{
					VECADDFAC(&em->velocity[index * 3], &em->velocity[index * 3], &particle_vel[p * 3], sfs->vel_multi);
				}
			}   // particles loop
		}
		else if (valid_particles > 0) { // MOD_SMOKE_FLOW_USE_PART_SIZE

			int min[3], max[3], res[3];
			float hr = 1.0f / ((float)hires_multiplier);
			/* slightly adjust high res antialias smoothness based on number of divisions
			 * to allow smaller details but yet not differing too much from the low res size */
			const float hr_smooth = smooth * powf(hr, 1.0f / 3.0f);

			/* setup loop bounds */
			for (i = 0; i < 3; i++) {
				min[i] = em->min[i] * hires_multiplier;
				max[i] = em->max[i] * hires_multiplier;
				res[i] = em->res[i] * hires_multiplier;
			}

			BLI_kdtree_balance(tree);

			/* begin thread safe malloc */
			BLI_begin_threaded_malloc();

#pragma omp parallel for schedule(static)
			for (z = min[2]; z < max[2]; z++) {
				int x, y;
				for (x = min[0]; x < max[0]; x++)
					for (y = min[1]; y < max[1]; y++) {
						/* take low res samples where possible */
						if (hires_multiplier <= 1 || !(x % hires_multiplier || y % hires_multiplier || z % hires_multiplier)) {
							/* get low res space coordinates */
							int lx = x / hires_multiplier;
							int ly = y / hires_multiplier;
							int lz = z / hires_multiplier;

							int index = smoke_get_index(lx - em->min[0], em->res[0], ly - em->min[1], em->res[1], lz - em->min[2]);
							float ray_start[3] = {((float)lx) + 0.5f, ((float)ly) + 0.5f, ((float)lz) + 0.5f};

							/* find particle distance from the kdtree */
							KDTreeNearest nearest;
							float range = solid + smooth;
							BLI_kdtree_find_nearest(tree, ray_start, NULL, &nearest);

							if (nearest.dist < range) {
								em->influence[index] = (nearest.dist < solid) ? 1.0f : (1.0f - (nearest.dist-solid) / smooth);
								/* Uses particle velocity as initial velocity for smoke */
								if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY && (psys->part->phystype != PART_PHYS_NO))
								{
									VECADDFAC(&em->velocity[index * 3], &em->velocity[index * 3], &particle_vel[nearest.index * 3], sfs->vel_multi);
								}
							}
						}

						/* take high res samples if required */
						if (hires_multiplier > 1) {
							/* get low res space coordinates */
							float lx = ((float)x) * hr;
							float ly = ((float)y) * hr;
							float lz = ((float)z) * hr;

							int index = smoke_get_index(x - min[0], res[0], y - min[1], res[1], z - min[2]);
							float ray_start[3] = {lx + 0.5f*hr, ly + 0.5f*hr, lz + 0.5f*hr};

							/* find particle distance from the kdtree */
							KDTreeNearest nearest;
							float range = solid + hr_smooth;
							BLI_kdtree_find_nearest(tree, ray_start, NULL, &nearest);

							if (nearest.dist < range) {
								em->influence_high[index] = (nearest.dist < solid) ? 1.0f : (1.0f - (nearest.dist-solid) / smooth);
							}
						}

					}
			}
			BLI_end_threaded_malloc();
		}

		if (sfs->flags & MOD_SMOKE_FLOW_USE_PART_SIZE) {
			BLI_kdtree_free(tree);
		}

		/* free data */
		if (particle_pos)
			MEM_freeN(particle_pos);
		if (particle_vel)
			MEM_freeN(particle_vel);
	}
}

static void sample_derivedmesh(SmokeFlowSettings *sfs, MVert *mvert, MTFace *tface, MFace *mface, float *influence_map, float *velocity_map, int index, int base_res[3], float flow_center[3], BVHTreeFromMesh *treeData, float ray_start[3],
								float *vert_vel, int has_velocity, int defgrp_index, MDeformVert *dvert, float x, float y, float z)
{
	float ray_dir[3] = {1.0f, 0.0f, 0.0f};
	BVHTreeRayHit hit = {0};
	BVHTreeNearest nearest = {0};

	float volume_factor = 0.0f;
	float sample_str = 0.0f;

	hit.index = -1;
	hit.dist = 9999;
	nearest.index = -1;
	nearest.dist_sq = sfs->surface_distance * sfs->surface_distance; /* find_nearest uses squared distance */

	/* Check volume collision */
	if (sfs->volume_density) {
		if (BLI_bvhtree_ray_cast(treeData->tree, ray_start, ray_dir, 0.0f, &hit, treeData->raycast_callback, treeData) != -1) {
			float dot = ray_dir[0] * hit.no[0] + ray_dir[1] * hit.no[1] + ray_dir[2] * hit.no[2];
			/*  If ray and hit face normal are facing same direction
			 *	hit point is inside a closed mesh. */
			if (dot >= 0) {
				/* Also cast a ray in opposite direction to make sure
				 * point is at least surrounded by two faces */
				negate_v3(ray_dir);
				hit.index = -1;
				hit.dist = 9999;

				BLI_bvhtree_ray_cast(treeData->tree, ray_start, ray_dir, 0.0f, &hit, treeData->raycast_callback, treeData);
				if (hit.index != -1) {
					volume_factor = sfs->volume_density;
				}
			}
		}
	}

	/* find the nearest point on the mesh */
	if (BLI_bvhtree_find_nearest(treeData->tree, ray_start, &nearest, treeData->nearest_callback, treeData) != -1) {
		float weights[4];
		int v1, v2, v3, f_index = nearest.index;
		float n1[3], n2[3], n3[3], hit_normal[3];

		/* emit from surface based on distance */
		if (sfs->surface_distance) {
			sample_str = sqrtf(nearest.dist_sq) / sfs->surface_distance;
			CLAMP(sample_str, 0.0f, 1.0f);
			sample_str = pow(1.0f - sample_str, 0.5f);
		}
		else
			sample_str = 0.0f;

		/* calculate barycentric weights for nearest point */
		v1 = mface[f_index].v1;
		v2 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v3 : mface[f_index].v2;
		v3 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v4 : mface[f_index].v3;
		interp_weights_face_v3(weights, mvert[v1].co, mvert[v2].co, mvert[v3].co, NULL, nearest.co);

		if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY && velocity_map) {
			/* apply normal directional velocity */
			if (sfs->vel_normal) {
				/* interpolate vertex normal vectors to get nearest point normal */
				normal_short_to_float_v3(n1, mvert[v1].no);
				normal_short_to_float_v3(n2, mvert[v2].no);
				normal_short_to_float_v3(n3, mvert[v3].no);
				interp_v3_v3v3v3(hit_normal, n1, n2, n3, weights);
				normalize_v3(hit_normal);
				/* apply normal directional and random velocity
				 * - TODO: random disabled for now since it doesnt really work well as pressure calc smoothens it out... */
				velocity_map[index * 3]   += hit_normal[0] * sfs->vel_normal * 0.25f;
				velocity_map[index * 3 + 1] += hit_normal[1] * sfs->vel_normal * 0.25f;
				velocity_map[index * 3 + 2] += hit_normal[2] * sfs->vel_normal * 0.25f;
				/* TODO: for fire emitted from mesh surface we can use
				 *  Vf = Vs + (Ps/Pf - 1)*S to model gaseous expansion from solid to fuel */
			}
			/* apply object velocity */
			if (has_velocity && sfs->vel_multi) {
				float hit_vel[3];
				interp_v3_v3v3v3(hit_vel, &vert_vel[v1 * 3], &vert_vel[v2 * 3], &vert_vel[v3 * 3], weights);
				velocity_map[index * 3]   += hit_vel[0] * sfs->vel_multi;
				velocity_map[index * 3 + 1] += hit_vel[1] * sfs->vel_multi;
				velocity_map[index * 3 + 2] += hit_vel[2] * sfs->vel_multi;
			}
		}

		/* apply vertex group influence if used */
		if (defgrp_index != -1 && dvert) {
			float weight_mask = defvert_find_weight(&dvert[v1], defgrp_index) * weights[0] +
			                    defvert_find_weight(&dvert[v2], defgrp_index) * weights[1] +
			                    defvert_find_weight(&dvert[v3], defgrp_index) * weights[2];
			sample_str *= weight_mask;
		}

		/* apply emission texture */
		if ((sfs->flags & MOD_SMOKE_FLOW_TEXTUREEMIT) && sfs->noise_texture) {
			float tex_co[3] = {0};
			TexResult texres;

			if (sfs->texture_type == MOD_SMOKE_FLOW_TEXTURE_MAP_AUTO) {
				tex_co[0] = ((x - flow_center[0]) / base_res[0]) / sfs->texture_size;
				tex_co[1] = ((y - flow_center[1]) / base_res[1]) / sfs->texture_size;
				tex_co[2] = ((z - flow_center[2]) / base_res[2] - sfs->texture_offset) / sfs->texture_size;
			}
			else if (tface) {
				interp_v2_v2v2v2(tex_co, tface[f_index].uv[0], tface[f_index].uv[(nearest.flags & BVH_ONQUAD) ? 2 : 1],
				                 tface[f_index].uv[(nearest.flags & BVH_ONQUAD) ? 3 : 2], weights);
				/* map between -1.0f and 1.0f */
				tex_co[0] = tex_co[0] * 2.0f - 1.0f;
				tex_co[1] = tex_co[1] * 2.0f - 1.0f;
				tex_co[2] = sfs->texture_offset;
			}
			texres.nor = NULL;
			BKE_texture_get_value(NULL, sfs->noise_texture, tex_co, &texres, false);
			sample_str *= texres.tin;
		}
	}

	/* multiply initial velocity by emitter influence */
	if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY && velocity_map) {
		mul_v3_fl(&velocity_map[index * 3], sample_str);
	}

	/* apply final influence based on volume factor */
	influence_map[index] = MAX2(volume_factor, sample_str);
}

static void emit_from_derivedmesh(Object *flow_ob, SmokeDomainSettings *sds, SmokeFlowSettings *sfs, EmissionMap *em, float dt)
{
	if (!sfs->dm) return;
	{
		DerivedMesh *dm;
		int defgrp_index = sfs->vgroup_density - 1;
		MDeformVert *dvert = NULL;
		MVert *mvert = NULL;
		MVert *mvert_orig = NULL;
		MFace *mface = NULL;
		MTFace *tface = NULL;
		BVHTreeFromMesh treeData = {NULL};
		int numOfVerts, i, z;
		float flow_center[3] = {0};

		float *vert_vel = NULL;
		int has_velocity = 0;
		int min[3], max[3], res[3];
		int hires_multiplier = 1;

		/* copy derivedmesh for thread safety because we modify it,
		 * main issue is its VertArray being modified, then replaced and freed
		 */
		dm = CDDM_copy(sfs->dm);

		CDDM_calc_normals(dm);
		mvert = dm->getVertArray(dm);
		mvert_orig = dm->dupVertArray(dm);  /* copy original mvert and restore when done */
		mface = dm->getTessFaceArray(dm);
		numOfVerts = dm->getNumVerts(dm);
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
		tface = CustomData_get_layer_named(&dm->faceData, CD_MTFACE, sfs->uvlayer_name);

		if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
			vert_vel = MEM_callocN(sizeof(float) * numOfVerts * 3, "smoke_flow_velocity");

			if (sfs->numverts != numOfVerts || !sfs->verts_old) {
				if (sfs->verts_old) MEM_freeN(sfs->verts_old);
				sfs->verts_old = MEM_callocN(sizeof(float) * numOfVerts * 3, "smoke_flow_verts_old");
				sfs->numverts = numOfVerts;
			}
			else {
				has_velocity = 1;
			}
		}

		/*	Transform dm vertices to
		 *   domain grid space for fast lookups */
		for (i = 0; i < numOfVerts; i++) {
			float n[3];
			/* vert pos */
			mul_m4_v3(flow_ob->obmat, mvert[i].co);
			smoke_pos_to_cell(sds, mvert[i].co);
			/* vert normal */
			normal_short_to_float_v3(n, mvert[i].no);
			mul_mat3_m4_v3(flow_ob->obmat, n);
			mul_mat3_m4_v3(sds->imat, n);
			normalize_v3(n);
			normal_float_to_short_v3(mvert[i].no, n);
			/* vert velocity */
			if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
				float co[3];
				VECADD(co, mvert[i].co, sds->shift);
				if (has_velocity) {
					sub_v3_v3v3(&vert_vel[i * 3], co, &sfs->verts_old[i * 3]);
					mul_v3_fl(&vert_vel[i * 3], sds->dx / dt);
				}
				copy_v3_v3(&sfs->verts_old[i * 3], co);
			}

			/* calculate emission map bounds */
			em_boundInsert(em, mvert[i].co);
		}
		mul_m4_v3(flow_ob->obmat, flow_center);
		smoke_pos_to_cell(sds, flow_center);

		/* check need for high resolution map */
		if ((sds->flags & MOD_SMOKE_HIGHRES) && (sds->highres_sampling == SM_HRES_FULLSAMPLE)) {
			hires_multiplier = sds->amplify + 1;
		}

		/* set emission map */
		clampBoundsInDomain(sds, em->min, em->max, NULL, NULL, (int)ceil(sfs->surface_distance), dt);
		em_allocateData(em, sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY, hires_multiplier);

		/* setup loop bounds */
		for (i = 0; i < 3; i++) {
			min[i] = em->min[i] * hires_multiplier;
			max[i] = em->max[i] * hires_multiplier;
			res[i] = em->res[i] * hires_multiplier;
		}

		if (bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 4, 6)) {
#pragma omp parallel for schedule(static)
			for (z = min[2]; z < max[2]; z++) {
				int x, y;
				for (x = min[0]; x < max[0]; x++)
					for (y = min[1]; y < max[1]; y++) {
						/* take low res samples where possible */
						if (hires_multiplier <= 1 || !(x % hires_multiplier || y % hires_multiplier || z % hires_multiplier)) {
							/* get low res space coordinates */
							int lx = x / hires_multiplier;
							int ly = y / hires_multiplier;
							int lz = z / hires_multiplier;

							int index = smoke_get_index(lx - em->min[0], em->res[0], ly - em->min[1], em->res[1], lz - em->min[2]);
							float ray_start[3] = {((float)lx) + 0.5f, ((float)ly) + 0.5f, ((float)lz) + 0.5f};

							sample_derivedmesh(sfs, mvert, tface, mface, em->influence, em->velocity, index, sds->base_res, flow_center, &treeData, ray_start,
												vert_vel, has_velocity, defgrp_index, dvert, (float)lx, (float)ly, (float)lz);
						}

						/* take high res samples if required */
						if (hires_multiplier > 1) {
							/* get low res space coordinates */
							float hr = 1.0f / ((float)hires_multiplier);
							float lx = ((float)x) * hr;
							float ly = ((float)y) * hr;
							float lz = ((float)z) * hr;

							int index = smoke_get_index(x - min[0], res[0], y - min[1], res[1], z - min[2]);
							float ray_start[3] = {lx + 0.5f*hr, ly + 0.5f*hr, lz + 0.5f*hr};

							sample_derivedmesh(sfs, mvert, tface, mface, em->influence_high, NULL, index, sds->base_res, flow_center, &treeData, ray_start,
												vert_vel, has_velocity, defgrp_index, dvert, lx, ly, lz); /* x,y,z needs to be always lowres */
						}

					}
			}
		}
		/* free bvh tree */
		free_bvhtree_from_mesh(&treeData);
		/* restore original mverts */
		CustomData_set_layer(&dm->vertData, CD_MVERT, mvert_orig);
		if (mvert)
			MEM_freeN(mvert);

		if (vert_vel) MEM_freeN(vert_vel);
	}
}

/**********************************************************
 *	Smoke step
 **********************************************************/

static void adjustDomainResolution(SmokeDomainSettings *sds, int new_shift[3], EmissionMap *emaps, unsigned int numflowobj, float dt)
{
	int min[3] = {32767, 32767, 32767}, max[3] = {-32767, -32767, -32767}, res[3];
	int total_cells = 1, res_changed = 0, shift_changed = 0;
	float min_vel[3], max_vel[3];
	int x, y, z, i;
	float *density = smoke_get_density(sds->fluid);
	float *fuel = smoke_get_fuel(sds->fluid);
	float *bigdensity = smoke_turbulence_get_density(sds->wt);
	float *bigfuel = smoke_turbulence_get_fuel(sds->wt);
	float *vx = smoke_get_velocity_x(sds->fluid);
	float *vy = smoke_get_velocity_y(sds->fluid);
	float *vz = smoke_get_velocity_z(sds->fluid);
	int block_size = sds->amplify + 1;
	int wt_res[3];

	if (sds->flags & MOD_SMOKE_HIGHRES && sds->wt) {
		smoke_turbulence_get_res(sds->wt, wt_res);
	}

	INIT_MINMAX(min_vel, max_vel);

	/* Calculate bounds for current domain content */
	for (x = sds->res_min[0]; x <  sds->res_max[0]; x++)
		for (y =  sds->res_min[1]; y <  sds->res_max[1]; y++)
			for (z =  sds->res_min[2]; z <  sds->res_max[2]; z++)
			{
				int xn = x - new_shift[0];
				int yn = y - new_shift[1];
				int zn = z - new_shift[2];
				int index;
				float max_den;
				
				/* skip if cell already belongs to new area */
				if (xn >= min[0] && xn <= max[0] && yn >= min[1] && yn <= max[1] && zn >= min[2] && zn <= max[2])
					continue;

				index = smoke_get_index(x - sds->res_min[0], sds->res[0], y - sds->res_min[1], sds->res[1], z - sds->res_min[2]);
				max_den = (fuel) ? MAX2(density[index], fuel[index]) : density[index];

				/* check high resolution bounds if max density isnt already high enough */
				if (max_den < sds->adapt_threshold && sds->flags & MOD_SMOKE_HIGHRES && sds->wt) {
					int i, j, k;
					/* high res grid index */
					int xx = (x - sds->res_min[0]) * block_size;
					int yy = (y - sds->res_min[1]) * block_size;
					int zz = (z - sds->res_min[2]) * block_size;

					for (i = 0; i < block_size; i++)
						for (j = 0; j < block_size; j++)
							for (k = 0; k < block_size; k++)
							{
								int big_index = smoke_get_index(xx + i, wt_res[0], yy + j, wt_res[1], zz + k);
								float den = (bigfuel) ? MAX2(bigdensity[big_index], bigfuel[big_index]) : bigdensity[big_index];
								if (den > max_den) {
									max_den = den;
								}
							}
				}

				/* content bounds (use shifted coordinates) */
				if (max_den >= sds->adapt_threshold) {
					if (min[0] > xn) min[0] = xn;
					if (min[1] > yn) min[1] = yn;
					if (min[2] > zn) min[2] = zn;
					if (max[0] < xn) max[0] = xn;
					if (max[1] < yn) max[1] = yn;
					if (max[2] < zn) max[2] = zn;
				}

				/* velocity bounds */
				if (min_vel[0] > vx[index]) min_vel[0] = vx[index];
				if (min_vel[1] > vy[index]) min_vel[1] = vy[index];
				if (min_vel[2] > vz[index]) min_vel[2] = vz[index];
				if (max_vel[0] < vx[index]) max_vel[0] = vx[index];
				if (max_vel[1] < vy[index]) max_vel[1] = vy[index];
				if (max_vel[2] < vz[index]) max_vel[2] = vz[index];
			}

	/* also apply emission maps */
	for (i = 0; i < numflowobj; i++)
	{
		EmissionMap *em = &emaps[i];

		for (x = em->min[0]; x < em->max[0]; x++)
			for (y = em->min[1]; y < em->max[1]; y++)
				for (z = em->min[2]; z < em->max[2]; z++)
				{
					int index = smoke_get_index(x - em->min[0], em->res[0], y - em->min[1], em->res[1], z - em->min[2]);
					float max_den = em->influence[index];

					/* density bounds */
					if (max_den >= sds->adapt_threshold) {
						if (min[0] > x) min[0] = x;
						if (min[1] > y) min[1] = y;
						if (min[2] > z) min[2] = z;
						if (max[0] < x) max[0] = x;
						if (max[1] < y) max[1] = y;
						if (max[2] < z) max[2] = z;
					}
				}
	}

	/* calculate new bounds based on these values */
	mul_v3_fl(min_vel, 1.0f / sds->dx);
	mul_v3_fl(max_vel, 1.0f / sds->dx);
	clampBoundsInDomain(sds, min, max, min_vel, max_vel, sds->adapt_margin + 1, dt);

	for (i = 0; i < 3; i++) {
		/* calculate new resolution */
		res[i] = max[i] - min[i];
		total_cells *= res[i];

		if (new_shift[i])
			shift_changed = 1;

		/* if no content set minimum dimensions */
		if (res[i] <= 0) {
			int j;
			for (j = 0; j < 3; j++) {
				min[j] = 0;
				max[j] = 1;
				res[j] = 1;
			}
			res_changed = 1;
			total_cells = 1;
			break;
		}
		if (min[i] != sds->res_min[i] || max[i] != sds->res_max[i])
			res_changed = 1;
	}

	if (res_changed || shift_changed) {
		struct FLUID_3D *fluid_old = sds->fluid;
		struct WTURBULENCE *turb_old = sds->wt;
		/* allocate new fluid data */
		smoke_reallocate_fluid(sds, sds->dx, res, 0);
		if (sds->flags & MOD_SMOKE_HIGHRES) {
			smoke_reallocate_highres_fluid(sds, sds->dx, res, 0);
		}

		/* copy values from old fluid to new */
		if (sds->total_cells > 1 && total_cells > 1) {
			/* low res smoke */
			float *o_dens, *o_react, *o_flame, *o_fuel, *o_heat, *o_heatold, *o_vx, *o_vy, *o_vz, *o_r, *o_g, *o_b;
			float *n_dens, *n_react, *n_flame, *n_fuel, *n_heat, *n_heatold, *n_vx, *n_vy, *n_vz, *n_r, *n_g, *n_b;
			float dummy;
			unsigned char *dummy_p;
			/* high res smoke */
			int wt_res_old[3];
			float *o_wt_dens, *o_wt_react, *o_wt_flame, *o_wt_fuel, *o_wt_tcu, *o_wt_tcv, *o_wt_tcw, *o_wt_r, *o_wt_g, *o_wt_b;
			float *n_wt_dens, *n_wt_react, *n_wt_flame, *n_wt_fuel, *n_wt_tcu, *n_wt_tcv, *n_wt_tcw, *n_wt_r, *n_wt_g, *n_wt_b;

			smoke_export(fluid_old, &dummy, &dummy, &o_dens, &o_react, &o_flame, &o_fuel, &o_heat, &o_heatold, &o_vx, &o_vy, &o_vz, &o_r, &o_g, &o_b, &dummy_p);
			smoke_export(sds->fluid, &dummy, &dummy, &n_dens, &n_react, &n_flame, &n_fuel, &n_heat, &n_heatold, &n_vx, &n_vy, &n_vz, &n_r, &n_g, &n_b, &dummy_p);

			if (sds->flags & MOD_SMOKE_HIGHRES) {
				smoke_turbulence_export(turb_old, &o_wt_dens, &o_wt_react, &o_wt_flame, &o_wt_fuel, &o_wt_r, &o_wt_g, &o_wt_b, &o_wt_tcu, &o_wt_tcv, &o_wt_tcw);
				smoke_turbulence_get_res(turb_old, wt_res_old);
				smoke_turbulence_export(sds->wt, &n_wt_dens, &n_wt_react, &n_wt_flame, &n_wt_fuel, &n_wt_r, &n_wt_g, &n_wt_b, &n_wt_tcu, &n_wt_tcv, &n_wt_tcw);
			}


			for (x = sds->res_min[0]; x < sds->res_max[0]; x++)
				for (y = sds->res_min[1]; y < sds->res_max[1]; y++)
					for (z = sds->res_min[2]; z < sds->res_max[2]; z++)
					{
						/* old grid index */
						int xo = x - sds->res_min[0];
						int yo = y - sds->res_min[1];
						int zo = z - sds->res_min[2];
						int index_old = smoke_get_index(xo, sds->res[0], yo, sds->res[1], zo);
						/* new grid index */
						int xn = x - min[0] - new_shift[0];
						int yn = y - min[1] - new_shift[1];
						int zn = z - min[2] - new_shift[2];
						int index_new = smoke_get_index(xn, res[0], yn, res[1], zn);

						/* skip if outside new domain */
						if (xn < 0 || xn >= res[0] ||
						    yn < 0 || yn >= res[1] ||
						    zn < 0 || zn >= res[2])
							continue;

						/* copy data */
						n_dens[index_new] = o_dens[index_old];
						/* heat */
						if (n_heat && o_heat) {
							n_heat[index_new] = o_heat[index_old];
							n_heatold[index_new] = o_heatold[index_old];
						}
						/* fuel */
						if (n_fuel && o_fuel) {
							n_flame[index_new] = o_flame[index_old];
							n_fuel[index_new] = o_fuel[index_old];
							n_react[index_new] = o_react[index_old];
						}
						/* color */
						if (o_r && n_r) {
							n_r[index_new] = o_r[index_old];
							n_g[index_new] = o_g[index_old];
							n_b[index_new] = o_b[index_old];
						}
						n_vx[index_new] = o_vx[index_old];
						n_vy[index_new] = o_vy[index_old];
						n_vz[index_new] = o_vz[index_old];

						if (sds->flags & MOD_SMOKE_HIGHRES && turb_old) {
							int block_size = sds->amplify + 1;
							int i, j, k;
							/* old grid index */
							int xx_o = xo * block_size;
							int yy_o = yo * block_size;
							int zz_o = zo * block_size;
							/* new grid index */
							int xx_n = xn * block_size;
							int yy_n = yn * block_size;
							int zz_n = zn * block_size;

							n_wt_tcu[index_new] = o_wt_tcu[index_old];
							n_wt_tcv[index_new] = o_wt_tcv[index_old];
							n_wt_tcw[index_new] = o_wt_tcw[index_old];

							for (i = 0; i < block_size; i++)
								for (j = 0; j < block_size; j++)
									for (k = 0; k < block_size; k++)
									{
										int big_index_old = smoke_get_index(xx_o + i, wt_res_old[0], yy_o + j, wt_res_old[1], zz_o + k);
										int big_index_new = smoke_get_index(xx_n + i, sds->res_wt[0], yy_n + j, sds->res_wt[1], zz_n + k);
										/* copy data */
										n_wt_dens[big_index_new] = o_wt_dens[big_index_old];
										if (n_wt_flame && o_wt_flame) {
											n_wt_flame[big_index_new] = o_wt_flame[big_index_old];
											n_wt_fuel[big_index_new] = o_wt_fuel[big_index_old];
											n_wt_react[big_index_new] = o_wt_react[big_index_old];
										}
										if (n_wt_r && o_wt_r) {
											n_wt_r[big_index_new] = o_wt_r[big_index_old];
											n_wt_g[big_index_new] = o_wt_g[big_index_old];
											n_wt_b[big_index_new] = o_wt_b[big_index_old];
										}
									}
						}
					}
		}
		smoke_free(fluid_old);
		if (turb_old)
			smoke_turbulence_free(turb_old);

		/* set new domain dimensions */
		VECCOPY(sds->res_min, min);
		VECCOPY(sds->res_max, max);
		VECCOPY(sds->res, res);
		sds->total_cells = total_cells;
	}
}

BLI_INLINE void apply_outflow_fields(int index, float *density, float *heat, float *fuel, float *react, float *color_r, float *color_g, float *color_b)
{
	density[index] = 0.f;
	if (heat) {
		heat[index] = 0.f;
	}
	if (fuel) {
		fuel[index] = 0.f;
		react[index] = 0.f;
	}
	if (color_r) {
		color_r[index] = 0.f;
		color_g[index] = 0.f;
		color_b[index] = 0.f;
	}
}

BLI_INLINE void apply_inflow_fields(SmokeFlowSettings *sfs, float emission_value, int index, float *density, float *heat, float *fuel, float *react, float *color_r, float *color_g, float *color_b)
{
	int absolute_flow = (sfs->flags & MOD_SMOKE_FLOW_ABSOLUTE);
	float dens_old = density[index];
	// float fuel_old = (fuel) ? fuel[index] : 0.0f;  /* UNUSED */
	float dens_flow = (sfs->type == MOD_SMOKE_FLOW_TYPE_FIRE) ? 0.0f : emission_value * sfs->density;
	float fuel_flow = emission_value * sfs->fuel_amount;
	/* add heat */
	if (heat && emission_value > 0.0f) {
		heat[index] = ADD_IF_LOWER(heat[index], sfs->temp);
	}
	/* absolute */
	if (absolute_flow) {
		if (sfs->type != MOD_SMOKE_FLOW_TYPE_FIRE) {
			if (dens_flow > density[index])
				density[index] = dens_flow;
		}
		if (sfs->type != MOD_SMOKE_FLOW_TYPE_SMOKE && fuel && fuel_flow) {
			if (fuel_flow > fuel[index])
				fuel[index] = fuel_flow;
		}
	}
	/* additive */
	else {
		if (sfs->type != MOD_SMOKE_FLOW_TYPE_FIRE) {
			density[index] += dens_flow;
			CLAMP(density[index], 0.0f, 1.0f);
		}
		if (sfs->type != MOD_SMOKE_FLOW_TYPE_SMOKE && fuel && sfs->fuel_amount) {
			fuel[index] += fuel_flow;
			CLAMP(fuel[index], 0.0f, 10.0f);
		}
	}

	/* set color */
	if (color_r && dens_flow) {
		float total_dens = density[index] / (dens_old + dens_flow);
		color_r[index] = (color_r[index] + sfs->color[0] * dens_flow) * total_dens;
		color_g[index] = (color_g[index] + sfs->color[1] * dens_flow) * total_dens;
		color_b[index] = (color_b[index] + sfs->color[2] * dens_flow) * total_dens;
	}

	/* set fire reaction coordinate */
	if (fuel && fuel[index] > FLT_EPSILON) {
		/* instead of using 1.0 for all new fuel add slight falloff
		 * to reduce flow blockiness */
		float value = 1.0f - powf(1.0f - emission_value, 2.0f);

		if (value > react[index]) {
			float f = fuel_flow / fuel[index];
			react[index] = value * f + (1.0f - f) * react[index];
			CLAMP(react[index], 0.0f, value);
		}
	}
}

static void update_flowsfluids(Scene *scene, Object *ob, SmokeDomainSettings *sds, float dt, bool for_render)
{
	Object **flowobjs = NULL;
	EmissionMap *emaps = NULL;
	unsigned int numflowobj = 0;
	unsigned int flowIndex;
	int new_shift[3] = {0};
	int active_fields = sds->active_fields;

	/* calculate domain shift for current frame if using adaptive domain */
	if (sds->flags & MOD_SMOKE_ADAPTIVE_DOMAIN) {
		int total_shift[3];
		float frame_shift_f[3];
		float ob_loc[3] = {0};

		mul_m4_v3(ob->obmat, ob_loc);

		VECSUB(frame_shift_f, ob_loc, sds->prev_loc);
		copy_v3_v3(sds->prev_loc, ob_loc);
		/* convert global space shift to local "cell" space */
		mul_mat3_m4_v3(sds->imat, frame_shift_f);
		frame_shift_f[0] = frame_shift_f[0] / sds->cell_size[0];
		frame_shift_f[1] = frame_shift_f[1] / sds->cell_size[1];
		frame_shift_f[2] = frame_shift_f[2] / sds->cell_size[2];
		/* add to total shift */
		VECADD(sds->shift_f, sds->shift_f, frame_shift_f);
		/* convert to integer */
		total_shift[0] = floor(sds->shift_f[0]);
		total_shift[1] = floor(sds->shift_f[1]);
		total_shift[2] = floor(sds->shift_f[2]);
		VECSUB(new_shift, total_shift, sds->shift);
		copy_v3_v3_int(sds->shift, total_shift);

		/* calculate new domain boundary points so that smoke doesnt slide on sub-cell movement */
		sds->p0[0] = sds->dp0[0] - sds->cell_size[0] * (sds->shift_f[0] - total_shift[0] - 0.5f);
		sds->p0[1] = sds->dp0[1] - sds->cell_size[1] * (sds->shift_f[1] - total_shift[1] - 0.5f);
		sds->p0[2] = sds->dp0[2] - sds->cell_size[2] * (sds->shift_f[2] - total_shift[2] - 0.5f);
		sds->p1[0] = sds->p0[0] + sds->cell_size[0] * sds->base_res[0];
		sds->p1[1] = sds->p0[1] + sds->cell_size[1] * sds->base_res[1];
		sds->p1[2] = sds->p0[2] + sds->cell_size[2] * sds->base_res[2];
	}

	flowobjs = get_collisionobjects(scene, ob, sds->fluid_group, &numflowobj, eModifierType_Smoke);

	/* init emission maps for each flow */
	emaps = MEM_callocN(sizeof(struct EmissionMap) * numflowobj, "smoke_flow_maps");

	/* Prepare flow emission maps */
	for (flowIndex = 0; flowIndex < numflowobj; flowIndex++)
	{
		Object *collob = flowobjs[flowIndex];
		SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(collob, eModifierType_Smoke);

		// check for initialized smoke object
		if ((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow)
		{
			// we got nice flow object
			SmokeFlowSettings *sfs = smd2->flow;
			int subframes = sfs->subframes;
			EmissionMap *em = &emaps[flowIndex];

			/* just sample flow directly to emission map if no subframes */
			if (!subframes) {
				if (sfs->source == MOD_SMOKE_FLOW_SOURCE_PARTICLES) {
					emit_from_particles(collob, sds, sfs, em, scene, dt);
				}
				else {
					emit_from_derivedmesh(collob, sds, sfs, em, dt);
				}
			}
			/* sample subframes */
			else {
				int scene_frame = scene->r.cfra;
				// float scene_subframe = scene->r.subframe;  // UNUSED
				int subframe;
				for (subframe = 0; subframe <= subframes; subframe++) {
					EmissionMap em_temp = {NULL};
					float sample_size = 1.0f / (float)(subframes+1);
					float prev_frame_pos = sample_size * (float)(subframe+1);
					float sdt = dt * sample_size;
					int hires_multiplier = 1;

					if ((sds->flags & MOD_SMOKE_HIGHRES) && (sds->highres_sampling == SM_HRES_FULLSAMPLE)) {
						hires_multiplier = sds->amplify + 1;
					}

					/* set scene frame to match previous frame + subframe
					 * or use current frame for last sample */
					if (subframe < subframes) {
						scene->r.cfra = scene_frame - 1;
						scene->r.subframe = prev_frame_pos;
					}
					else {
						scene->r.cfra = scene_frame;
						scene->r.subframe = 0.0f;
					}

					if (sfs->source == MOD_SMOKE_FLOW_SOURCE_PARTICLES) {
						/* emit_from_particles() updates timestep internally */
						emit_from_particles(collob, sds, sfs, &em_temp, scene, sdt);
					}
					else { /* MOD_SMOKE_FLOW_SOURCE_MESH */
						/* update flow object frame */
						subframe_updateObject(scene, collob, 1, 5, BKE_scene_frame_get(scene), for_render);

						/* apply flow */
						emit_from_derivedmesh(collob, sds, sfs, &em_temp, sdt);
					}

					/* combine emission maps */
					em_combineMaps(em, &em_temp, hires_multiplier, !(sfs->flags & MOD_SMOKE_FLOW_ABSOLUTE), sample_size);
					em_freeData(&em_temp);
				}
			}

			/* update required data fields */
			if (em->total_cells && sfs->type != MOD_SMOKE_FLOW_TYPE_OUTFLOW) {
				/* activate heat field if flow produces any heat */
				if (sfs->temp) {
					active_fields |= SM_ACTIVE_HEAT;
				}
				/* activate fuel field if flow adds any fuel */
				if (sfs->type != MOD_SMOKE_FLOW_TYPE_SMOKE && sfs->fuel_amount) {
					active_fields |= SM_ACTIVE_FIRE;
				}
				/* activate color field if flows add smoke with varying colors */
				if (sfs->type != MOD_SMOKE_FLOW_TYPE_FIRE && sfs->density) {
					if (!(active_fields & SM_ACTIVE_COLOR_SET)) {
						copy_v3_v3(sds->active_color, sfs->color);
						active_fields |= SM_ACTIVE_COLOR_SET;
					}
					else if (!equals_v3v3(sds->active_color, sfs->color)) {
						active_fields |= SM_ACTIVE_COLORS;
					}
				}
			}
		}
	}

	/* monitor active fields based on domain settings */
	/* if domain has fire, activate new fields if required */
	if (active_fields & SM_ACTIVE_FIRE) {
		/* heat is always needed for fire */
		active_fields |= SM_ACTIVE_HEAT;
		/* also activate colors if domain smoke color differs from active color */
		if (!(active_fields & SM_ACTIVE_COLOR_SET)) {
			copy_v3_v3(sds->active_color, sds->flame_smoke_color);
			active_fields |= SM_ACTIVE_COLOR_SET;
		}
		else if (!equals_v3v3(sds->active_color, sds->flame_smoke_color)) {
			active_fields |= SM_ACTIVE_COLORS;
		}
	}

	/* Adjust domain size if needed */
	if (sds->flags & MOD_SMOKE_ADAPTIVE_DOMAIN) {
		adjustDomainResolution(sds, new_shift, emaps, numflowobj, dt);
	}

	/* Initialize new data fields if any */
	if (active_fields & SM_ACTIVE_HEAT) {
		smoke_ensure_heat(sds->fluid);
	}
	if (active_fields & SM_ACTIVE_FIRE) {
		smoke_ensure_fire(sds->fluid, sds->wt);
	}
	if (active_fields & SM_ACTIVE_COLORS) {
		/* initialize all smoke with "active_color" */
		smoke_ensure_colors(sds->fluid, sds->wt, sds->active_color[0], sds->active_color[1], sds->active_color[2]);
	}
	sds->active_fields = active_fields;

	/* Apply emission data */
	if (sds->fluid) {
		for (flowIndex = 0; flowIndex < numflowobj; flowIndex++)
		{
			Object *collob = flowobjs[flowIndex];
			SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(collob, eModifierType_Smoke);

			// check for initialized smoke object
			if ((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow)
			{
				// we got nice flow object
				SmokeFlowSettings *sfs = smd2->flow;
				EmissionMap *em = &emaps[flowIndex];

				float *density = smoke_get_density(sds->fluid);
				float *color_r = smoke_get_color_r(sds->fluid);
				float *color_g = smoke_get_color_g(sds->fluid);
				float *color_b = smoke_get_color_b(sds->fluid);
				float *fuel = smoke_get_fuel(sds->fluid);
				float *react = smoke_get_react(sds->fluid);
				float *bigdensity = smoke_turbulence_get_density(sds->wt);
				float *bigfuel = smoke_turbulence_get_fuel(sds->wt);
				float *bigreact = smoke_turbulence_get_react(sds->wt);
				float *bigcolor_r = smoke_turbulence_get_color_r(sds->wt);
				float *bigcolor_g = smoke_turbulence_get_color_g(sds->wt);
				float *bigcolor_b = smoke_turbulence_get_color_b(sds->wt);
				float *heat = smoke_get_heat(sds->fluid);
				float *velocity_x = smoke_get_velocity_x(sds->fluid);
				float *velocity_y = smoke_get_velocity_y(sds->fluid);
				float *velocity_z = smoke_get_velocity_z(sds->fluid);
				//unsigned char *obstacle = smoke_get_obstacle(sds->fluid);
				// DG TODO UNUSED unsigned char *obstacleAnim = smoke_get_obstacle_anim(sds->fluid);
				int bigres[3];
				float *velocity_map = em->velocity;
				float *emission_map = em->influence;
				float *emission_map_high = em->influence_high;

				int ii, jj, kk, gx, gy, gz, ex, ey, ez, dx, dy, dz, block_size;
				size_t e_index, d_index, index_big;

				// loop through every emission map cell
				for (gx = em->min[0]; gx < em->max[0]; gx++)
					for (gy = em->min[1]; gy < em->max[1]; gy++)
						for (gz = em->min[2]; gz < em->max[2]; gz++)
						{
							/* get emission map index */
							ex = gx - em->min[0];
							ey = gy - em->min[1];
							ez = gz - em->min[2];
							e_index = smoke_get_index(ex, em->res[0], ey, em->res[1], ez);

							/* get domain index */
							dx = gx - sds->res_min[0];
							dy = gy - sds->res_min[1];
							dz = gz - sds->res_min[2];
							d_index = smoke_get_index(dx, sds->res[0], dy, sds->res[1], dz);
							/* make sure emission cell is inside the new domain boundary */
							if (dx < 0 || dy < 0 || dz < 0 || dx >= sds->res[0] || dy >= sds->res[1] || dz >= sds->res[2]) continue;

							if (sfs->type == MOD_SMOKE_FLOW_TYPE_OUTFLOW) { // outflow
								apply_outflow_fields(d_index, density, heat, fuel, react, color_r, color_g, color_b);
							}
							else { // inflow
								apply_inflow_fields(sfs, emission_map[e_index], d_index, density, heat, fuel, react, color_r, color_g, color_b);

								/* initial velocity */
								if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
									velocity_x[d_index] = ADD_IF_LOWER(velocity_x[d_index], velocity_map[e_index * 3]);
									velocity_y[d_index] = ADD_IF_LOWER(velocity_y[d_index], velocity_map[e_index * 3 + 1]);
									velocity_z[d_index] = ADD_IF_LOWER(velocity_z[d_index], velocity_map[e_index * 3 + 2]);
								}
							}

							/* loop through high res blocks if high res enabled */
							if (bigdensity) {
								// neighbor cell emission densities (for high resolution smoke smooth interpolation)
								float c000, c001, c010, c011,  c100, c101, c110, c111;

								smoke_turbulence_get_res(sds->wt, bigres);
								block_size = sds->amplify + 1;  // high res block size

								c000 = (ex > 0 && ey > 0 && ez > 0) ? emission_map[smoke_get_index(ex - 1, em->res[0], ey - 1, em->res[1], ez - 1)] : 0;
								c001 = (ex > 0 && ey > 0) ? emission_map[smoke_get_index(ex - 1, em->res[0], ey - 1, em->res[1], ez)] : 0;
								c010 = (ex > 0 && ez > 0) ? emission_map[smoke_get_index(ex - 1, em->res[0], ey, em->res[1], ez - 1)] : 0;
								c011 = (ex > 0) ? emission_map[smoke_get_index(ex - 1, em->res[0], ey, em->res[1], ez)] : 0;

								c100 = (ey > 0 && ez > 0) ? emission_map[smoke_get_index(ex, em->res[0], ey - 1, em->res[1], ez - 1)] : 0;
								c101 = (ey > 0) ? emission_map[smoke_get_index(ex, em->res[0], ey - 1, em->res[1], ez)] : 0;
								c110 = (ez > 0) ? emission_map[smoke_get_index(ex, em->res[0], ey, em->res[1], ez - 1)] : 0;
								c111 = emission_map[smoke_get_index(ex, em->res[0], ey, em->res[1], ez)]; // this cell

								for (ii = 0; ii < block_size; ii++)
									for (jj = 0; jj < block_size; jj++)
										for (kk = 0; kk < block_size; kk++)
										{

											float fx, fy, fz, interpolated_value;
											int shift_x = 0, shift_y = 0, shift_z = 0;


											/* Use full sample emission map if enabled and available */
											if ((sds->highres_sampling == SM_HRES_FULLSAMPLE) && emission_map_high) {
												interpolated_value = emission_map_high[smoke_get_index(ex * block_size + ii, em->res[0] * block_size, ey * block_size + jj, em->res[1] * block_size, ez * block_size + kk)]; // this cell
											}
											else if (sds->highres_sampling == SM_HRES_NEAREST) {
												/* without interpolation use same low resolution
												 * block value for all hi-res blocks */
												interpolated_value = c111;
											}
											/* Fall back to interpolated */
											else
											{
												/* get relative block position
												 * for interpolation smoothing */
												fx = (float)ii / block_size + 0.5f / block_size;
												fy = (float)jj / block_size + 0.5f / block_size;
												fz = (float)kk / block_size + 0.5f / block_size;

												/* calculate trilinear interpolation */
												interpolated_value = c000 * (1 - fx) * (1 - fy) * (1 - fz) +
												                     c100 * fx * (1 - fy) * (1 - fz) +
												                     c010 * (1 - fx) * fy * (1 - fz) +
												                     c001 * (1 - fx) * (1 - fy) * fz +
												                     c101 * fx * (1 - fy) * fz +
												                     c011 * (1 - fx) * fy * fz +
												                     c110 * fx * fy * (1 - fz) +
												                     c111 * fx * fy * fz;


												/* add some contrast / sharpness
												 * depending on hi-res block size */
												interpolated_value = (interpolated_value - 0.4f) * (block_size / 2) + 0.4f;
												CLAMP(interpolated_value, 0.0f, 1.0f);

												/* shift smoke block index
												 * (because pixel center is actually
												 * in halfway of the low res block) */
												shift_x = (dx < 1) ? 0 : block_size / 2;
												shift_y = (dy < 1) ? 0 : block_size / 2;
												shift_z = (dz < 1) ? 0 : block_size / 2;
											}

											/* get shifted index for current high resolution block */
											index_big = smoke_get_index(block_size * dx + ii - shift_x, bigres[0], block_size * dy + jj - shift_y, bigres[1], block_size * dz + kk - shift_z);

											if (sfs->type == MOD_SMOKE_FLOW_TYPE_OUTFLOW) { // outflow
												if (interpolated_value) {
													apply_outflow_fields(index_big, bigdensity, NULL, bigfuel, bigreact, bigcolor_r, bigcolor_g, bigcolor_b);
												}
											}
											else { // inflow
												apply_inflow_fields(sfs, interpolated_value, index_big, bigdensity, NULL, bigfuel, bigreact, bigcolor_r, bigcolor_g, bigcolor_b);
											}
										} // hires loop
							}  // bigdensity
						} // low res loop

				// free emission maps
				em_freeData(em);

			} // end emission
		}
	}

	if (flowobjs)
		MEM_freeN(flowobjs);
	if (emaps)
		MEM_freeN(emaps);
}

static void update_effectors(Scene *scene, Object *ob, SmokeDomainSettings *sds, float UNUSED(dt))
{
	ListBase *effectors;
	/* make sure smoke flow influence is 0.0f */
	sds->effector_weights->weight[PFIELD_SMOKEFLOW] = 0.0f;
	effectors = pdInitEffectors(scene, ob, NULL, sds->effector_weights, true);

	if (effectors)
	{
		float *density = smoke_get_density(sds->fluid);
		float *fuel = smoke_get_fuel(sds->fluid);
		float *force_x = smoke_get_force_x(sds->fluid);
		float *force_y = smoke_get_force_y(sds->fluid);
		float *force_z = smoke_get_force_z(sds->fluid);
		float *velocity_x = smoke_get_velocity_x(sds->fluid);
		float *velocity_y = smoke_get_velocity_y(sds->fluid);
		float *velocity_z = smoke_get_velocity_z(sds->fluid);
		unsigned char *obstacle = smoke_get_obstacle(sds->fluid);
		int x;

		// precalculate wind forces
#pragma omp parallel for schedule(static)
		for (x = 0; x < sds->res[0]; x++)
		{
			int y, z;
			for (y = 0; y < sds->res[1]; y++)
				for (z = 0; z < sds->res[2]; z++)
				{
					EffectedPoint epoint;
					float mag;
					float voxelCenter[3] = {0, 0, 0}, vel[3] = {0, 0, 0}, retvel[3] = {0, 0, 0};
					unsigned int index = smoke_get_index(x, sds->res[0], y, sds->res[1], z);

					if (((fuel ? MAX2(density[index], fuel[index]) : density[index]) < FLT_EPSILON) || obstacle[index])
						continue;

					vel[0] = velocity_x[index];
					vel[1] = velocity_y[index];
					vel[2] = velocity_z[index];

					/* convert vel to global space */
					mag = len_v3(vel);
					mul_mat3_m4_v3(sds->obmat, vel);
					normalize_v3(vel);
					mul_v3_fl(vel, mag);

					voxelCenter[0] = sds->p0[0] + sds->cell_size[0] * ((float)(x + sds->res_min[0]) + 0.5f);
					voxelCenter[1] = sds->p0[1] + sds->cell_size[1] * ((float)(y + sds->res_min[1]) + 0.5f);
					voxelCenter[2] = sds->p0[2] + sds->cell_size[2] * ((float)(z + sds->res_min[2]) + 0.5f);
					mul_m4_v3(sds->obmat, voxelCenter);

					pd_point_from_loc(scene, voxelCenter, vel, index, &epoint);
					pdDoEffectors(effectors, NULL, sds->effector_weights, &epoint, retvel, NULL);

					/* convert retvel to local space */
					mag = len_v3(retvel);
					mul_mat3_m4_v3(sds->imat, retvel);
					normalize_v3(retvel);
					mul_v3_fl(retvel, mag);

					// TODO dg - do in force!
					force_x[index] = min_ff(max_ff(-1.0f, retvel[0] * 0.2f), 1.0f);
					force_y[index] = min_ff(max_ff(-1.0f, retvel[1] * 0.2f), 1.0f);
					force_z[index] = min_ff(max_ff(-1.0f, retvel[2] * 0.2f), 1.0f);
				}
		}
	}

	pdEndEffectors(&effectors);
}

static void step(Scene *scene, Object *ob, SmokeModifierData *smd, DerivedMesh *domain_dm, float fps, bool for_render)
{
	SmokeDomainSettings *sds = smd->domain;
	/* stability values copied from wturbulence.cpp */
	const int maxSubSteps = 25;
	float maxVel;
	// maxVel should be 1.5 (1.5 cell max movement) * dx (cell size)

	float dt;
	float maxVelMag = 0.0f;
	int totalSubsteps;
	int substep = 0;
	float dtSubdiv;
	float gravity[3] = {0.0f, 0.0f, -1.0f};
	float gravity_mag;

#if 0  /* UNUSED */
	   /* get max velocity and lower the dt value if it is too high */
	size_t size = sds->res[0] * sds->res[1] * sds->res[2];
	float *velX = smoke_get_velocity_x(sds->fluid);
	float *velY = smoke_get_velocity_y(sds->fluid);
	float *velZ = smoke_get_velocity_z(sds->fluid);
	size_t i;
#endif

	/* update object state */
	invert_m4_m4(sds->imat, ob->obmat);
	copy_m4_m4(sds->obmat, ob->obmat);
	smoke_set_domain_from_derivedmesh(sds, ob, domain_dm, (sds->flags & MOD_SMOKE_ADAPTIVE_DOMAIN));

	/* use global gravity if enabled */
	if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		copy_v3_v3(gravity, scene->physics_settings.gravity);
		/* map default value to 1.0 */
		mul_v3_fl(gravity, 1.0f / 9.810f);
	}
	/* convert gravity to domain space */
	gravity_mag = len_v3(gravity);
	mul_mat3_m4_v3(sds->imat, gravity);
	normalize_v3(gravity);
	mul_v3_fl(gravity, gravity_mag);

	/* adapt timestep for different framerates, dt = 0.1 is at 25fps */
	dt = DT_DEFAULT * (25.0f / fps);
	// maximum timestep/"CFL" constraint: dt < 5.0 *dx / maxVel
	maxVel = (sds->dx * 5.0f);

#if 0
	for (i = 0; i < size; i++) {
		float vtemp = (velX[i] * velX[i] + velY[i] * velY[i] + velZ[i] * velZ[i]);
		if (vtemp > maxVelMag)
			maxVelMag = vtemp;
	}
#endif

	maxVelMag = sqrtf(maxVelMag) * dt * sds->time_scale;
	totalSubsteps = (int)((maxVelMag / maxVel) + 1.0f); /* always round up */
	totalSubsteps = (totalSubsteps < 1) ? 1 : totalSubsteps;
	totalSubsteps = (totalSubsteps > maxSubSteps) ? maxSubSteps : totalSubsteps;

	/* Disable substeps for now, since it results in numerical instability */
	totalSubsteps = 1.0f;

	dtSubdiv = (float)dt / (float)totalSubsteps;

	// printf("totalSubsteps: %d, maxVelMag: %f, dt: %f\n", totalSubsteps, maxVelMag, dt);

	for (substep = 0; substep < totalSubsteps; substep++)
	{
		// calc animated obstacle velocities
		update_flowsfluids(scene, ob, sds, dtSubdiv, for_render);
		update_obstacles(scene, ob, sds, dtSubdiv, substep, totalSubsteps);

		if (sds->total_cells > 1) {
			update_effectors(scene, ob, sds, dtSubdiv); // DG TODO? problem --> uses forces instead of velocity, need to check how they need to be changed with variable dt
			smoke_step(sds->fluid, gravity, dtSubdiv);
		}
	}
}

static DerivedMesh *createDomainGeometry(SmokeDomainSettings *sds, Object *ob)
{
	DerivedMesh *result;
	MVert *mverts;
	MPoly *mpolys;
	MLoop *mloops;
	float min[3];
	float max[3];
	float *co;
	MPoly *mp;
	MLoop *ml;

	int num_verts = 8;
	int num_faces = 6;
	int i;
	float ob_loc[3] = {0};
	float ob_cache_loc[3] = {0};

	/* dont generate any mesh if there isnt any content */
	if (sds->total_cells <= 1) {
		num_verts = 0;
		num_faces = 0;
	}

	result = CDDM_new(num_verts, 0, 0, num_faces * 4, num_faces);
	mverts = CDDM_get_verts(result);
	mpolys = CDDM_get_polys(result);
	mloops = CDDM_get_loops(result);


	if (num_verts) {
		/* volume bounds */
		VECMADD(min, sds->p0, sds->cell_size, sds->res_min);
		VECMADD(max, sds->p0, sds->cell_size, sds->res_max);

		/* set vertices */
		/* top slab */
		co = mverts[0].co; co[0] = min[0]; co[1] = min[1]; co[2] = max[2];
		co = mverts[1].co; co[0] = max[0]; co[1] = min[1]; co[2] = max[2];
		co = mverts[2].co; co[0] = max[0]; co[1] = max[1]; co[2] = max[2];
		co = mverts[3].co; co[0] = min[0]; co[1] = max[1]; co[2] = max[2];
		/* bottom slab */
		co = mverts[4].co; co[0] = min[0]; co[1] = min[1]; co[2] = min[2];
		co = mverts[5].co; co[0] = max[0]; co[1] = min[1]; co[2] = min[2];
		co = mverts[6].co; co[0] = max[0]; co[1] = max[1]; co[2] = min[2];
		co = mverts[7].co; co[0] = min[0]; co[1] = max[1]; co[2] = min[2];

		/* create faces */
		/* top */
		mp = &mpolys[0]; ml = &mloops[0 * 4]; mp->loopstart = 0 * 4; mp->totloop = 4;
		ml[0].v = 0; ml[1].v = 1; ml[2].v = 2; ml[3].v = 3;
		/* right */
		mp = &mpolys[1]; ml = &mloops[1 * 4]; mp->loopstart = 1 * 4; mp->totloop = 4;
		ml[0].v = 2; ml[1].v = 1; ml[2].v = 5; ml[3].v = 6;
		/* bottom */
		mp = &mpolys[2]; ml = &mloops[2 * 4]; mp->loopstart = 2 * 4; mp->totloop = 4;
		ml[0].v = 7; ml[1].v = 6; ml[2].v = 5; ml[3].v = 4;
		/* left */
		mp = &mpolys[3]; ml = &mloops[3 * 4]; mp->loopstart = 3 * 4; mp->totloop = 4;
		ml[0].v = 0; ml[1].v = 3; ml[2].v = 7; ml[3].v = 4;
		/* front */
		mp = &mpolys[4]; ml = &mloops[4 * 4]; mp->loopstart = 4 * 4; mp->totloop = 4;
		ml[0].v = 3; ml[1].v = 2; ml[2].v = 6; ml[3].v = 7;
		/* back */
		mp = &mpolys[5]; ml = &mloops[5 * 4]; mp->loopstart = 5 * 4; mp->totloop = 4;
		ml[0].v = 1; ml[1].v = 0; ml[2].v = 4; ml[3].v = 5;

		/* calculate required shift to match domain's global position
		 *  it was originally simulated at (if object moves without smoke step) */
		invert_m4_m4(ob->imat, ob->obmat);
		mul_m4_v3(ob->obmat, ob_loc);
		mul_m4_v3(sds->obmat, ob_cache_loc);
		VECSUB(sds->obj_shift_f, ob_cache_loc, ob_loc);
		/* convert shift to local space and apply to vertices */
		mul_mat3_m4_v3(ob->imat, sds->obj_shift_f);
		/* apply */
		for (i = 0; i < num_verts; i++) {
			add_v3_v3(mverts[i].co, sds->obj_shift_f);
		}
	}


	CDDM_calc_edges(result);
	result->dirty |= DM_DIRTY_NORMALS;
	return result;
}

static void smokeModifier_process(SmokeModifierData *smd, Scene *scene, Object *ob, DerivedMesh *dm, bool for_render)
{
	if ((smd->type & MOD_SMOKE_TYPE_FLOW))
	{
		if (scene->r.cfra >= smd->time)
			smokeModifier_init(smd, ob, scene, dm);

		if (smd->flow->dm) smd->flow->dm->release(smd->flow->dm);
		smd->flow->dm = CDDM_copy(dm);
		DM_ensure_tessface(smd->flow->dm);

		if (scene->r.cfra > smd->time)
		{
			smd->time = scene->r.cfra;
		}
		else if (scene->r.cfra < smd->time)
		{
			smd->time = scene->r.cfra;
			smokeModifier_reset_ex(smd, false);
		}
	}
	else if (smd->type & MOD_SMOKE_TYPE_COLL)
	{
		if (scene->r.cfra >= smd->time)
			smokeModifier_init(smd, ob, scene, dm);

		if (smd->coll)
		{
			if (smd->coll->dm)
				smd->coll->dm->release(smd->coll->dm);

			smd->coll->dm = CDDM_copy(dm);
			DM_ensure_tessface(smd->coll->dm);
		}

		smd->time = scene->r.cfra;
		if (scene->r.cfra < smd->time)
		{
			smokeModifier_reset_ex(smd, false);
		}
	}
	else if (smd->type & MOD_SMOKE_TYPE_DOMAIN)
	{
		SmokeDomainSettings *sds = smd->domain;
		PointCache *cache = NULL;
		PTCacheID pid;
		int startframe, endframe, framenr;
		float timescale;

		framenr = scene->r.cfra;

		//printf("time: %d\n", scene->r.cfra);

		cache = sds->point_cache[0];
		BKE_ptcache_id_from_smoke(&pid, ob, smd);
		BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, &timescale);

		if (!smd->domain->fluid || framenr == startframe)
		{
			BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
			smokeModifier_reset_ex(smd, false);
			BKE_ptcache_validate(cache, framenr);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}

		if (!smd->domain->fluid && (framenr != startframe) && (smd->domain->flags & MOD_SMOKE_FILE_LOAD) == 0 && (cache->flag & PTCACHE_BAKED) == 0)
			return;

		smd->domain->flags &= ~MOD_SMOKE_FILE_LOAD;
		CLAMP(framenr, startframe, endframe);

		/* If already viewing a pre/after frame, no need to reload */
		if ((smd->time == framenr) && (framenr != scene->r.cfra))
			return;

		if (smokeModifier_init(smd, ob, scene, dm) == 0)
		{
			printf("bad smokeModifier_init\n");
			return;
		}

		/* try to read from cache */
		if (BKE_ptcache_read(&pid, (float)framenr) == PTCACHE_READ_EXACT) {
			BKE_ptcache_validate(cache, framenr);
			smd->time = framenr;
			return;
		}

		/* only calculate something when we advanced a single frame */
		if (framenr != (int)smd->time + 1)
			return;

		/* don't simulate if viewing start frame, but scene frame is not real start frame */
		if (framenr != scene->r.cfra)
			return;

		tstart();

		/* if on second frame, write cache for first frame */
		if ((int)smd->time == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact == 0)) {
			BKE_ptcache_write(&pid, startframe);
		}

		// set new time
		smd->time = scene->r.cfra;

		/* do simulation */

		// simulate the actual smoke (c++ code in intern/smoke)
		// DG: interesting commenting this line + deactivating loading of noise files
		if (framenr != startframe)
		{
			if (sds->flags & MOD_SMOKE_DISSOLVE) {
				/* low res dissolve */
				smoke_dissolve(sds->fluid, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
				/* high res dissolve */
				if (sds->wt) {
					smoke_dissolve_wavelet(sds->wt, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
				}

			}

			step(scene, ob, smd, dm, scene->r.frs_sec / scene->r.frs_sec_base, for_render);
		}

		// create shadows before writing cache so they get stored
		smoke_calc_transparency(sds, scene);

		if (sds->wt)
		{
			smoke_turbulence_step(sds->wt, sds->fluid);
		}

		BKE_ptcache_validate(cache, framenr);
		if (framenr != startframe)
			BKE_ptcache_write(&pid, framenr);

		tend();
		// printf ( "Frame: %d, Time: %f\n\n", (int)smd->time, (float) tval() );
	}
}

struct DerivedMesh *smokeModifier_do(SmokeModifierData *smd, Scene *scene, Object *ob, DerivedMesh *dm, bool for_render)
{
	/* lock so preview render does not read smoke data while it gets modified */
	if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain)
		BLI_rw_mutex_lock(smd->domain->fluid_mutex, THREAD_LOCK_WRITE);

	smokeModifier_process(smd, scene, ob, dm, for_render);

	if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain)
		BLI_rw_mutex_unlock(smd->domain->fluid_mutex);

	/* return generated geometry for adaptive domain */
	if (smd->type & MOD_SMOKE_TYPE_DOMAIN && smd->domain &&
	    smd->domain->flags & MOD_SMOKE_ADAPTIVE_DOMAIN &&
	    smd->domain->base_res[0])
	{
		return createDomainGeometry(smd->domain, ob);
	}
	else {
		return CDDM_copy(dm);
	}
}

static float calc_voxel_transp(float *result, float *input, int res[3], int *pixel, float *tRay, float correct)
{
	const size_t index = smoke_get_index(pixel[0], res[0], pixel[1], res[1], pixel[2]);

	// T_ray *= T_vox
	*tRay *= expf(input[index] * correct);

	if (result[index] < 0.0f)
	{
// #pragma omp critical
		result[index] = *tRay;
	}

	return *tRay;
}

static void bresenham_linie_3D(int x1, int y1, int z1, int x2, int y2, int z2, float *tRay, bresenham_callback cb, float *result, float *input, int res[3], float correct)
{
	int dx, dy, dz, i, l, m, n, x_inc, y_inc, z_inc, err_1, err_2, dx2, dy2, dz2;
	int pixel[3];

	pixel[0] = x1;
	pixel[1] = y1;
	pixel[2] = z1;

	dx = x2 - x1;
	dy = y2 - y1;
	dz = z2 - z1;

	x_inc = (dx < 0) ? -1 : 1;
	l = abs(dx);
	y_inc = (dy < 0) ? -1 : 1;
	m = abs(dy);
	z_inc = (dz < 0) ? -1 : 1;
	n = abs(dz);
	dx2 = l << 1;
	dy2 = m << 1;
	dz2 = n << 1;

	if ((l >= m) && (l >= n)) {
		err_1 = dy2 - l;
		err_2 = dz2 - l;
		for (i = 0; i < l; i++) {
			if (cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[1] += y_inc;
				err_1 -= dx2;
			}
			if (err_2 > 0) {
				pixel[2] += z_inc;
				err_2 -= dx2;
			}
			err_1 += dy2;
			err_2 += dz2;
			pixel[0] += x_inc;
		}
	}
	else if ((m >= l) && (m >= n)) {
		err_1 = dx2 - m;
		err_2 = dz2 - m;
		for (i = 0; i < m; i++) {
			if (cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[0] += x_inc;
				err_1 -= dy2;
			}
			if (err_2 > 0) {
				pixel[2] += z_inc;
				err_2 -= dy2;
			}
			err_1 += dx2;
			err_2 += dz2;
			pixel[1] += y_inc;
		}
	}
	else {
		err_1 = dy2 - n;
		err_2 = dx2 - n;
		for (i = 0; i < n; i++) {
			if (cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[1] += y_inc;
				err_1 -= dz2;
			}
			if (err_2 > 0) {
				pixel[0] += x_inc;
				err_2 -= dz2;
			}
			err_1 += dy2;
			err_2 += dx2;
			pixel[2] += z_inc;
		}
	}
	cb(result, input, res, pixel, tRay, correct);
}

static void smoke_calc_transparency(SmokeDomainSettings *sds, Scene *scene)
{
	float bv[6] = {0};
	float light[3];
	int a, z, slabsize = sds->res[0] * sds->res[1], size = sds->res[0] * sds->res[1] * sds->res[2];
	float *density = smoke_get_density(sds->fluid);
	float correct = -7.0f * sds->dx;

	if (!get_lamp(scene, light)) return;

	/* convert light pos to sim cell space */
	mul_m4_v3(sds->imat, light);
	light[0] = (light[0] - sds->p0[0]) / sds->cell_size[0] - 0.5f - (float)sds->res_min[0];
	light[1] = (light[1] - sds->p0[1]) / sds->cell_size[1] - 0.5f - (float)sds->res_min[1];
	light[2] = (light[2] - sds->p0[2]) / sds->cell_size[2] - 0.5f - (float)sds->res_min[2];

	for (a = 0; a < size; a++)
		sds->shadow[a] = -1.0f;

	/* calculate domain bounds in sim cell space */
	// 0,2,4 = 0.0f
	bv[1] = (float)sds->res[0]; // x
	bv[3] = (float)sds->res[1]; // y
	bv[5] = (float)sds->res[2]; // z

// #pragma omp parallel for schedule(static, 1)
	for (z = 0; z < sds->res[2]; z++)
	{
		size_t index = z * slabsize;
		int x, y;

		for (y = 0; y < sds->res[1]; y++)
			for (x = 0; x < sds->res[0]; x++, index++)
			{
				float voxelCenter[3];
				float pos[3];
				int cell[3];
				float tRay = 1.0;

				if (sds->shadow[index] >= 0.0f)
					continue;
				voxelCenter[0] = (float)x;
				voxelCenter[1] = (float)y;
				voxelCenter[2] = (float)z;

				// get starting cell (light pos)
				if (BLI_bvhtree_bb_raycast(bv, light, voxelCenter, pos) > FLT_EPSILON)
				{
					// we're ouside -> use point on side of domain
					cell[0] = (int)floor(pos[0]);
					cell[1] = (int)floor(pos[1]);
					cell[2] = (int)floor(pos[2]);
				}
				else {
					// we're inside -> use light itself
					cell[0] = (int)floor(light[0]);
					cell[1] = (int)floor(light[1]);
					cell[2] = (int)floor(light[2]);
				}
				/* clamp within grid bounds */
				CLAMP(cell[0], 0, sds->res[0] - 1);
				CLAMP(cell[1], 0, sds->res[1] - 1);
				CLAMP(cell[2], 0, sds->res[2] - 1);

				bresenham_linie_3D(cell[0], cell[1], cell[2], x, y, z, &tRay, calc_voxel_transp, sds->shadow, density, sds->res, correct);

				// convention -> from a RGBA float array, use G value for tRay
// #pragma omp critical
				sds->shadow[index] = tRay;
			}
	}
}

/* get smoke velocity and density at given coordinates
 *  returns fluid density or -1.0f if outside domain*/
float smoke_get_velocity_at(struct Object *ob, float position[3], float velocity[3])
{
	SmokeModifierData *smd = (SmokeModifierData *)modifiers_findByType(ob, eModifierType_Smoke);
	zero_v3(velocity);

	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain && smd->domain->fluid) {
		SmokeDomainSettings *sds = smd->domain;
		float time_mult = 25.f * DT_DEFAULT;
		float vel_mag;
		float *velX = smoke_get_velocity_x(sds->fluid);
		float *velY = smoke_get_velocity_y(sds->fluid);
		float *velZ = smoke_get_velocity_z(sds->fluid);
		float density = 0.0f, fuel = 0.0f;
		float pos[3];
		copy_v3_v3(pos, position);
		smoke_pos_to_cell(sds, pos);

		/* check if point is outside domain max bounds */
		if (pos[0] < sds->res_min[0] || pos[1] < sds->res_min[1] || pos[2] < sds->res_min[2]) return -1.0f;
		if (pos[0] > sds->res_max[0] || pos[1] > sds->res_max[1] || pos[2] > sds->res_max[2]) return -1.0f;

		/* map pos between 0.0 - 1.0 */
		pos[0] = (pos[0] - sds->res_min[0]) / ((float)sds->res[0]);
		pos[1] = (pos[1] - sds->res_min[1]) / ((float)sds->res[1]);
		pos[2] = (pos[2] - sds->res_min[2]) / ((float)sds->res[2]);


		/* check if point is outside active area */
		if (smd->domain->flags & MOD_SMOKE_ADAPTIVE_DOMAIN) {
			if (pos[0] < 0.0f || pos[1] < 0.0f || pos[2] < 0.0f) return 0.0f;
			if (pos[0] > 1.0f || pos[1] > 1.0f || pos[2] > 1.0f) return 0.0f;
		}

		/* get interpolated velocity */
		velocity[0] = BLI_voxel_sample_trilinear(velX, sds->res, pos) * sds->global_size[0] * time_mult;
		velocity[1] = BLI_voxel_sample_trilinear(velY, sds->res, pos) * sds->global_size[1] * time_mult;
		velocity[2] = BLI_voxel_sample_trilinear(velZ, sds->res, pos) * sds->global_size[2] * time_mult;

		/* convert velocity direction to global space */
		vel_mag = len_v3(velocity);
		mul_mat3_m4_v3(sds->obmat, velocity);
		normalize_v3(velocity);
		mul_v3_fl(velocity, vel_mag);

		/* use max value of fuel or smoke density */
		density = BLI_voxel_sample_trilinear(smoke_get_density(sds->fluid), sds->res, pos);
		if (smoke_has_fuel(sds->fluid)) {
			fuel = BLI_voxel_sample_trilinear(smoke_get_fuel(sds->fluid), sds->res, pos);
		}
		return MAX2(density, fuel);
	}
	return -1.0f;
}

int smoke_get_data_flags(SmokeDomainSettings *sds)
{
	int flags = 0;
	if (smoke_has_heat(sds->fluid)) flags |= SM_ACTIVE_HEAT;
	if (smoke_has_fuel(sds->fluid)) flags |= SM_ACTIVE_FIRE;
	if (smoke_has_colors(sds->fluid)) flags |= SM_ACTIVE_COLORS;

	return flags;
}

#endif /* WITH_SMOKE */
