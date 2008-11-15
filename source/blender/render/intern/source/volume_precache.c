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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "PIL_time.h"

#include "RE_shader_ext.h"
#include "RE_raytrace.h"

#include "DNA_material_types.h"

#include "render_types.h"
#include "renderdatabase.h"
#include "volumetric.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* Recursive test for intersections, from a point inside the mesh, to outside
 * Number of intersections (depth) determine if a point is inside or outside the mesh */
int intersect_outside_volume(RayTree *tree, Isect *isect, float *offset, int limit, int depth)
{
	if (limit == 0) return depth;
	
	if (RE_ray_tree_intersect(tree, isect)) {
		float hitco[3];
		
		hitco[0] = isect->start[0] + isect->labda*isect->vec[0];
		hitco[1] = isect->start[1] + isect->labda*isect->vec[1];
		hitco[2] = isect->start[2] + isect->labda*isect->vec[2];
		VecAddf(isect->start, hitco, offset);

		return intersect_outside_volume(tree, isect, offset, limit-1, depth+1);
	} else {
		return depth;
	}
}

/* Uses ray tracing to check if a point is inside or outside an ObjectInstanceRen */
int point_inside_obi(RayTree *tree, ObjectInstanceRen *obi, float *co)
{
	float maxsize = RE_ray_tree_max_size(tree);
	Isect isect;
	float vec[3] = {0.0f,0.0f,1.0f};
	int final_depth=0, depth=0, limit=20;
	
	/* set up the isect */
	memset(&isect, 0, sizeof(isect));
	VECCOPY(isect.start, co);
	isect.end[0] = co[0] + vec[0] * maxsize;
	isect.end[1] = co[1] + vec[1] * maxsize;
	isect.end[2] = co[2] + vec[2] * maxsize;
	
	/* and give it a little offset to prevent self-intersections */
	VecMulf(vec, 1e-5);
	VecAddf(isect.start, isect.start, vec);
	
	isect.mode= RE_RAY_MIRROR;
	isect.face_last= NULL;
	isect.lay= -1;
	
	final_depth = intersect_outside_volume(tree, &isect, vec, limit, depth);
	
	/* even number of intersections: point is outside
	 * odd number: point is inside */
	if (final_depth % 2 == 0) return 0;
	else return 1;
}

static int inside_check_func(Isect *is, int ob, RayFace *face)
{
	return 1;
}
static void vlr_face_coords(RayFace *face, float **v1, float **v2, float **v3, float **v4)
{
	VlakRen *vlr= (VlakRen*)face;

	*v1 = (vlr->v1)? vlr->v1->co: NULL;
	*v2 = (vlr->v2)? vlr->v2->co: NULL;
	*v3 = (vlr->v3)? vlr->v3->co: NULL;
	*v4 = (vlr->v4)? vlr->v4->co: NULL;
}

RayTree *create_raytree_obi(ObjectInstanceRen *obi, float *bbmin, float *bbmax)
{
	int v;
	VlakRen *vlr= NULL;
	
	/* create empty raytree */
	RayTree *tree = RE_ray_tree_create(64, obi->obr->totvlak, bbmin, bbmax,
		vlr_face_coords, inside_check_func, NULL, NULL);
	
	/* fill it with faces */
	for(v=0; v<obi->obr->totvlak; v++) {
		if((v & 255)==0)
			vlr= obi->obr->vlaknodes[v>>8].vlak;
		else
			vlr++;
	
		RE_ray_tree_add_face(tree, 0, vlr);
	}
	
	RE_ray_tree_done(tree);
	
	return tree;
}

static float get_avg_surrounds(float *cache, int res, int res_2, int res_3, int rgb, int xx, int yy, int zz)
{
	int x, y, z, x_, y_, z_;
	int added=0;
	float tot=0.0f;
	int i;
	
	for (x=-1; x <= 1; x++) {
		x_ = xx+x;
		if (x_ >= 0 && x_ <= res-1) {
		
			for (y=-1; y <= 1; y++) {
				y_ = yy+y;
				if (y_ >= 0 && y_ <= res-1) {
				
					for (z=-1; z <= 1; z++) {
						z_ = zz+z;
						if (z_ >= 0 && z_ <= res-1) {
						
							i = rgb*res_3 + x_*res_2 + y_*res + z_;
							if (cache[i] > 0.0f) {
								tot += cache[i];
								added++;
							}
							
						}
					}
				}
			}
		}
	}
	
	tot /= added;
	
	return ((added>0)?tot:0.0f);
}

/* function to filter the edges of the light cache, where there was no volume originally.
 * For each voxel which was originally external to the mesh, it finds the average values of
 * the surrounding internal voxels and sets the original external voxel to that average amount.
 * Works almost a bit like a 'dilate' filter */
static void lightcache_filter(float *cache, int res)
{
	int x, y, z, rgb;
	int res_2, res_3;
	int i;
	
	res_2 = res*res;
	res_3 = res*res*res;

	for (x=0; x < res; x++) {
		for (y=0; y < res; y++) {
			for (z=0; z < res; z++) {
				for (rgb=0; rgb < 3; rgb++) {
					i = rgb*res_3 + x*res_2 + y*res + z;

					/* trigger for outside mesh */
					if (cache[i] < 0.5f) cache[i] = get_avg_surrounds(cache, res, res_2, res_3, rgb, x, y, z);
				}
			}
		}
	}
}

/* Precache a volume into a 3D voxel grid.
 * The voxel grid is stored in the ObjectInstanceRen, 
 * in camera space, aligned with the ObjectRen's bounding box.
 * Resolution is defined by the user.
 */
void vol_precache_objectinstance(Render *re, ObjectInstanceRen *obi, Material *ma, float *bbmin, float *bbmax)
{
	int x, y, z;

	float co[3], voxel[3], scatter_col[3];
	ShadeInput shi;
	float view[3] = {0.0,0.0,-1.0};
	float density;
	float stepsize;
	
	float resf, res_3f;
	int res_2, res_3;
	
	float i = 1.0f;
	double time, lasttime= PIL_check_seconds_timer();
	const int res = ma->vol_precache_resolution;
	RayTree *tree;
	
	R = *re;

	/* create a raytree with just the faces of the instanced ObjectRen, 
	 * used for checking if the cached point is inside or outside. */
	tree = create_raytree_obi(obi, bbmin, bbmax);
	if (!tree) return;

	/* Need a shadeinput to calculate scattering */
	memset(&shi, 0, sizeof(ShadeInput)); 
	shi.depth= 1;
	shi.mask= 1;
	shi.mat = ma;
	shi.vlr = NULL;
	memcpy(&shi.r, &shi.mat->r, 23*sizeof(float));	// note, keep this synced with render_types.h
	shi.har= shi.mat->har;
	shi.obi= obi;
	shi.obr= obi->obr;
	shi.lay = re->scene->lay;
	VECCOPY(shi.view, view);
	
	stepsize = vol_get_stepsize(&shi, STEPSIZE_VIEW);

	resf = (float)res;
	res_2 = res*res;
	res_3 = res*res*res;
	res_3f = (float)res_3;
	
	VecSubf(voxel, bbmax, bbmin);
	if ((voxel[0] < FLT_EPSILON) || (voxel[1] < FLT_EPSILON) || (voxel[2] < FLT_EPSILON))
		return;
	VecMulf(voxel, 1.0f/res);
	
	obi->volume_precache = MEM_callocN(sizeof(float)*res_3*3, "volume light cache");
	
	/* Iterate over the 3d voxel grid, and fill the voxels with scattering information
	 *
	 * It's stored in memory as 3 big float grids next to each other, one for each RGB channel.
	 * I'm guessing the memory alignment may work out better this way for the purposes
	 * of doing linear interpolation, but I haven't actually tested this theory! :)
	 */
	for (x=0; x < res; x++) {
		co[0] = bbmin[0] + (voxel[0] * x);
		
		for (y=0; y < res; y++) {
			co[1] = bbmin[1] + (voxel[1] * y);
			
			for (z=0; z < res; z++) {
				co[2] = bbmin[2] + (voxel[2] * z);
			
				time= PIL_check_seconds_timer();
				i++;
			
				/* display progress every second */
				if(re->test_break()) {
					if(tree) {
						RE_ray_tree_free(tree);
						tree= NULL;
					}
					return;
				}
				if(time-lasttime>1.0f) {
					char str[64];
					sprintf(str, "Precaching volume: %d%%", (int)(100.0f * (i / res_3f)));
					re->i.infostr= str;
					re->stats_draw(&re->i);
					re->i.infostr= NULL;
					lasttime= time;
				}
				
				/* don't bother if the point is not inside the volume mesh */
				if (!point_inside_obi(tree, obi, co)) {
					obi->volume_precache[0*res_3 + x*res_2 + y*res + z] = -1.0f;
					obi->volume_precache[1*res_3 + x*res_2 + y*res + z] = -1.0f;
					obi->volume_precache[2*res_3 + x*res_2 + y*res + z] = -1.0f;
					continue;
				}
				density = vol_get_density(&shi, co);
				vol_get_scattering(&shi, scatter_col, co, stepsize, density);
			
				obi->volume_precache[0*res_3 + x*res_2 + y*res + z] = scatter_col[0];
				obi->volume_precache[1*res_3 + x*res_2 + y*res + z] = scatter_col[1];
				obi->volume_precache[2*res_3 + x*res_2 + y*res + z] = scatter_col[2];
			}
		}
	}

	if(tree) {
		RE_ray_tree_free(tree);
		tree= NULL;
	}
	
	lightcache_filter(obi->volume_precache, res);

}

/* loop through all objects (and their associated materials)
 * marked for pre-caching in convertblender.c, and pre-cache them */
void volume_precache(Render *re)
{
	ObjectInstanceRen *obi;
	VolPrecache *vp;

	for(vp= re->vol_precache_obs.first; vp; vp= vp->next) {
		for(obi= re->instancetable.first; obi; obi= obi->next) {
			if (obi->obr == vp->obr) 
				vol_precache_objectinstance(re, obi, vp->ma, obi->obr->boundbox[0], obi->obr->boundbox[1]);
		}
	}
	
	re->i.infostr= NULL;
	re->stats_draw(&re->i);
}

void free_volume_precache(Render *re)
{
	ObjectInstanceRen *obi;
	
	for(obi= re->instancetable.first; obi; obi= obi->next) {
		if (obi->volume_precache)
			MEM_freeN(obi->volume_precache);
	}
	
	BLI_freelistN(&re->vol_precache_obs);
}
