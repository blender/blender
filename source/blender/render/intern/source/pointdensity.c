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

#include <stdlib.h>
#include <stdio.h>

#include "BLI_arithb.h"
#include "BLI_kdtree.h"

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
	
	pd->point_tree = BLI_kdtree_new(psys->totpart+psys->totchild);
	
	if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
		childexists = 1;
	
	for (i = 0; i < psys->totpart + psys->totchild; i++) {

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
			
			BLI_kdtree_insert(pd->point_tree, i, partco, NULL);
		}
	}
	
	BLI_kdtree_balance(pd->point_tree);
	psys_render_restore(ob, psys);
}


static void pointdensity_cache_object(Render *re, PointDensity *pd, ObjectRen *obr)
{
	int i;
	
	if (!obr || !pd) return;
	if(!obr->vertnodes) return;
	
	/* in case ob->imat isn't up-to-date */
	Mat4Invert(obr->ob->imat, obr->ob->obmat);
	
	pd->point_tree = BLI_kdtree_new(obr->totvert);
	
	for(i=0; i<obr->totvert; i++) {
		float ver_co[3];
		VertRen *ver= RE_findOrAddVert(obr, i);
		
		VECCOPY(ver_co, ver->co);
		
		if (pd->ob_cache_space == TEX_PD_OBJECTSPACE) {
			Mat4MulVecfl(re->viewinv, ver_co);
			Mat4MulVecfl(obr->ob->imat, ver_co);
		} else {
			/* TEX_PD_WORLDSPACE */
			Mat4MulVecfl(re->viewinv, ver_co);
		}
		
		BLI_kdtree_insert(pd->point_tree, i, ver_co, NULL);
	}
	
	BLI_kdtree_balance(pd->point_tree);

}
static void cache_pointdensity(Render *re, Tex *tex)
{
	PointDensity *pd = tex->pd;

	if (pd->point_tree) {
		BLI_kdtree_free(pd->point_tree);
		pd->point_tree = NULL;
	}
	
	if (pd->source == TEX_PD_PSYS) {
		ParticleSystem *psys;
		Object *ob = pd->object;
		int i;
		
		for(psys=ob->particlesystem.first, i=0; i< pd->psysindex-1; i++)
			psys= psys->next;
		
		if (!ob || !psys) return;
		
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
		BLI_kdtree_free(pd->point_tree);
		pd->point_tree = NULL;
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

#define MAX_POINTS_NEAREST	25
int pointdensitytex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	
	PointDensity *pd = tex->pd;
	KDTreeNearest nearest[MAX_POINTS_NEAREST];
	float density=0.0f;
	int n, neighbours=0;
	
	if ((!pd) || (!pd->point_tree)) {
		texres->tin = 0.0f;
		return 0;
	}
	
	neighbours = BLI_kdtree_find_n_nearest(pd->point_tree, pd->nearest, texvec, NULL, nearest);
	
	for(n=1; n<neighbours; n++) {
		if ( nearest[n].dist < pd->radius) {
			float dist = 1.0 - (nearest[n].dist / pd->radius);
			
			density += 3.0f*dist*dist - 2.0f*dist*dist*dist;
		}
	}
	
	density /= neighbours;
	density *= 1.0 / pd->radius;

	texres->tin = density;

	/*
	texres->tr = 1.0f;
	texres->tg = 1.0f;
	texres->tb = 0.0f;
	
	BRICONTRGB;
	
	texres->ta = 1.0;
	
	if (texres->nor!=NULL) {
		texres->nor[0] = texres->nor[1] = texres->nor[2] = 0.0f;
	}
	*/
	
	BRICONT;
	
	return rv;
}
