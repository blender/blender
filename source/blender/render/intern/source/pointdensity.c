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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_kdopbvh.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_particle.h"

#include "DNA_texture_types.h"
#include "DNA_particle_types.h"

#include "render_types.h"
#include "renderdatabase.h"
#include "texture.h"

static void pointdensity_cache_psys(Render *re, PointDensity *pd, Object *ob, ParticleSystem *psys)
{
	DerivedMesh* dm;
	ParticleKey state;
	float cfra=bsystem_time(ob,(float)G.scene->r.cfra,0.0);
	int i, childexists;
	int total_particles;
	float partco[3];
	float obview[4][4];
	
	/* init crap */
	if (!psys || !ob || !pd) return;
	
	Mat4MulMat4(obview, re->viewinv, ob->obmat);
	
	/* Just to create a valid rendering context */
	psys_render_set(ob, psys, re->viewmat, re->winmat, re->winx, re->winy, 0);
	
	dm = mesh_create_derived_render(ob,CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
	dm->release(dm);
	
	if ( !psys_check_enabled(ob, psys) ){
		psys_render_restore(ob, psys);
		return;
	}
	
	/* in case ob->imat isn't up-to-date */
	Mat4Invert(ob->imat, ob->obmat);
	
	total_particles = psys->totpart+psys->totchild;
	
	pd->point_tree = BLI_bvhtree_new(total_particles, 0.0, 4, 6);
	if (pd->noise_influence != TEX_PD_NOISE_STATIC)
		pd->point_data = MEM_mallocN(sizeof(float)*3*total_particles, "point_data");
	
	if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
		childexists = 1;
	
	for (i = 0; i < total_particles; i++) {

		state.time = cfra;
		if(psys_get_particle_state(ob, psys, i, &state, 0)) {
			
			VECCOPY(partco, state.co);
			
			if (pd->psys_cache_space == TEX_PD_OBJECTSPACE)
				Mat4MulVecfl(ob->imat, partco);
			else if (pd->psys_cache_space == TEX_PD_OBJECTLOC) {
				float obloc[3];
				VECCOPY(obloc, ob->loc);
				VecSubf(partco, partco, obloc);
			} else {
				/* TEX_PD_WORLDSPACE */
			}
			
			BLI_bvhtree_insert(pd->point_tree, i, partco, 1);
			
			if (pd->noise_influence == TEX_PD_NOISE_VEL) {
				pd->point_data[i*3 + 0] = state.vel[0];
				pd->point_data[i*3 + 1] = state.vel[1];
				pd->point_data[i*3 + 2] = state.vel[2];
			} else if (pd->noise_influence == TEX_PD_NOISE_ANGVEL) {
				pd->point_data[i*3 + 0] = state.ave[0];
				pd->point_data[i*3 + 1] = state.ave[1];
				pd->point_data[i*3 + 2] = state.ave[2];
			}
		}
	}
	
	BLI_bvhtree_balance(pd->point_tree);
	
	psys_render_restore(ob, psys);
}


static void pointdensity_cache_object(Render *re, PointDensity *pd, ObjectRen *obr)
{
	int i;
	
	if (!obr || !pd) return;
	if(!obr->vertnodes) return;
	
	/* in case ob->imat isn't up-to-date */
	Mat4Invert(obr->ob->imat, obr->ob->obmat);
	
	pd->point_tree = BLI_bvhtree_new(obr->totvert, 0.0, 4, 6);
	
	for(i=0; i<obr->totvert; i++) {
		float ver_co[3];
		VertRen *ver= RE_findOrAddVert(obr, i);
		
		VECCOPY(ver_co, ver->co);
		Mat4MulVecfl(re->viewinv, ver_co);
		
		if (pd->ob_cache_space == TEX_PD_OBJECTSPACE) {
			Mat4MulVecfl(obr->ob->imat, ver_co);
		} else if (pd->psys_cache_space == TEX_PD_OBJECTLOC) {
			VecSubf(ver_co, ver_co, obr->ob->loc);
		} else {
			/* TEX_PD_WORLDSPACE */
		}
		
		BLI_bvhtree_insert(pd->point_tree, i, ver_co, 1);
	}
	
	BLI_bvhtree_balance(pd->point_tree);

}
static void cache_pointdensity(Render *re, Tex *tex)
{
	PointDensity *pd = tex->pd;

	if (pd->point_tree) {
		BLI_bvhtree_free(pd->point_tree);
		pd->point_tree = NULL;
	}
	
	if (pd->source == TEX_PD_PSYS) {
		ParticleSystem *psys;
		Object *ob = pd->object;
		int i;
		
		if (!ob) return;
		
		for(psys=ob->particlesystem.first, i=0; i< pd->psysindex-1; i++)
			psys= psys->next;
		
		if (!psys) return;
		
		pointdensity_cache_psys(re, pd, ob, psys);
	}
	else if (pd->source == TEX_PD_OBJECT) {
		Object *ob = pd->object;
		ObjectRen *obr;
		int found=0;

		/* find the obren that corresponds to the object */
		for (obr=re->objecttable.first; obr; obr=obr->next) {
			if (obr->ob == ob) {
				found=1;
				break;
			}
		}
		if (!found) return;
		
		pointdensity_cache_object(re, pd, obr);
	}
}

static void free_pointdensity(Render *re, Tex *tex)
{
	PointDensity *pd = tex->pd;

	if (pd->point_tree) {
		BLI_bvhtree_free(pd->point_tree);
		pd->point_tree = NULL;
	}

	if (pd->point_data) {
		MEM_freeN(pd->point_data);
		pd->point_data = NULL;
	}
}



void make_pointdensities(Render *re)
{
	Tex *tex;
	
	if(re->scene->r.scemode & R_PREVIEWBUTS)
		return;
	
	re->i.infostr= "Caching Point Densities";
	re->stats_draw(&re->i);

	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->id.us && tex->type==TEX_POINTDENSITY) {
			cache_pointdensity(re, tex);
		}
	}
}

void free_pointdensities(Render *re)
{
	Tex *tex;
	
	if(re->scene->r.scemode & R_PREVIEWBUTS)
		return;
	
	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->id.us && tex->type==TEX_POINTDENSITY) {
			free_pointdensity(re, tex);
		}
	}
}

typedef struct PointDensityRangeData
{
    float *density;
    float squared_radius;
    float *point_data;
	float *vec;
    short falloff_type;   
} PointDensityRangeData;

void accum_density(void *userdata, int index, float squared_dist)
{
	PointDensityRangeData *pdr = (PointDensityRangeData *)userdata;
	const float dist = pdr->squared_radius - squared_dist;
	float density;
	
	if (pdr->falloff_type == TEX_PD_FALLOFF_STD)
		density = dist;
	else if (pdr->falloff_type == TEX_PD_FALLOFF_SMOOTH)
		density = 3.0f*dist*dist - 2.0f*dist*dist*dist;
	else if (pdr->falloff_type == TEX_PD_FALLOFF_SHARP)
		density = dist*dist;
	else if (pdr->falloff_type == TEX_PD_FALLOFF_CONSTANT)
		density = pdr->squared_radius;
	else if (pdr->falloff_type == TEX_PD_FALLOFF_ROOT)
		density = sqrt(dist);
	
	if (pdr->point_data) {
		pdr->vec[0] += pdr->point_data[index*3 + 0];// * density;
		pdr->vec[1] += pdr->point_data[index*3 + 1];// * density;
		pdr->vec[2] += pdr->point_data[index*3 + 2];// * density;
	}
	
	*pdr->density += density;
}

#define MAX_POINTS_NEAREST	25
int pointdensitytex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	PointDensity *pd = tex->pd;
	PointDensityRangeData pdr;
	float density=0.0f;
	float vec[3] = {0.0, 0.0, 0.0};
	float tv[3];
	float turb;
	
	if ((!pd) || (!pd->point_tree)) {
		texres->tin = 0.0f;
		return 0;
	}
	
	pdr.squared_radius = pd->radius*pd->radius;
	pdr.density = &density;
	pdr.point_data = pd->point_data;
	pdr.falloff_type = pd->falloff_type;
	pdr.vec = vec;
	
	if (pd->flag & TEX_PD_TURBULENCE) {
		VECCOPY(tv, texvec);
		
		/* find the average speed vectors, for perturbing final density lookup with */
		BLI_bvhtree_range_query(pd->point_tree, texvec, pd->radius, accum_density, &pdr);
		
		density = 0.0f;
		Normalize(vec);
	
		turb = BLI_turbulence(pd->noise_size, texvec[0]+vec[0], texvec[1]+vec[1], texvec[2]+vec[2], pd->noise_depth);
		
		turb -= 0.5f;	/* re-center 0.0-1.0 range around 0 to prevent offsetting result */
		
		tv[0] = texvec[0] + pd->noise_fac * turb;
		tv[1] = texvec[1] + pd->noise_fac * turb;
		tv[2] = texvec[2] + pd->noise_fac * turb;
		
		/* do density lookup with altered coordinates */
		BLI_bvhtree_range_query(pd->point_tree, tv, pd->radius, accum_density, &pdr);
	}
	else
		BLI_bvhtree_range_query(pd->point_tree, texvec, pd->radius, accum_density, &pdr);

	texres->tin = density;

	//texres->tr = vec[0];
	//texres->tg = vec[1];
	//texres->tb = vec[2];
	
	return TEX_INT;
	
	/*
	BRICONTRGB;
	
	texres->ta = 1.0;
	
	if (texres->nor!=NULL) {
		texres->nor[0] = texres->nor[1] = texres->nor[2] = 0.0f;
	}
	*/
	
	//BRICONT;
	
	//return rv;
}
