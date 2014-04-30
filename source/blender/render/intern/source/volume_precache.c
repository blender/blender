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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Matt Ebb, Ra˙l Fern·ndez Hern·ndez (Farsthary).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/volume_precache.c
 *  \ingroup render
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_voxel.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "PIL_time.h"

#include "RE_shader_ext.h"

#include "DNA_material_types.h"

#include "rayintersection.h"
#include "rayobject.h"
#include "render_types.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "volumetric.h"
#include "volume_precache.h"


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* *** utility code to set up an individual raytree for objectinstance, for checking inside/outside *** */

/* Recursive test for intersections, from a point inside the mesh, to outside
 * Number of intersections (depth) determine if a point is inside or outside the mesh */
static int intersect_outside_volume(RayObject *tree, Isect *isect, float *offset, int limit, int depth)
{
	if (limit == 0) return depth;
	
	if (RE_rayobject_raycast(tree, isect)) {
		
		isect->start[0] = isect->start[0] + isect->dist*isect->dir[0];
		isect->start[1] = isect->start[1] + isect->dist*isect->dir[1];
		isect->start[2] = isect->start[2] + isect->dist*isect->dir[2];
		
		isect->dist = FLT_MAX;
		isect->skip = RE_SKIP_VLR_NEIGHBOUR;
		isect->orig.face= isect->hit.face;
		isect->orig.ob= isect->hit.ob;
		
		return intersect_outside_volume(tree, isect, offset, limit-1, depth+1);
	}
	else {
		return depth;
	}
}

/* Uses ray tracing to check if a point is inside or outside an ObjectInstanceRen */
static int point_inside_obi(RayObject *tree, ObjectInstanceRen *obi, const float co[3])
{
	Isect isect= {{0}};
	float dir[3] = {0.0f, 0.0f, 1.0f};
	int final_depth=0, depth=0, limit=20;
	
	/* set up the isect */
	copy_v3_v3(isect.start, co);
	copy_v3_v3(isect.dir, dir);
	isect.mode= RE_RAY_MIRROR;
	isect.last_hit= NULL;
	isect.lay= -1;
	
	isect.dist = FLT_MAX;
	isect.orig.face= NULL;
	isect.orig.ob = NULL;

	RE_instance_rotate_ray(obi, &isect);
	final_depth = intersect_outside_volume(tree, &isect, dir, limit, depth);
	RE_instance_rotate_ray_restore(obi, &isect);
	
	/* even number of intersections: point is outside
	 * odd number: point is inside */
	if (final_depth % 2 == 0) return 0;
	else return 1;
}

/* find the bounding box of an objectinstance in global space */
void global_bounds_obi(Render *re, ObjectInstanceRen *obi, float bbmin[3], float bbmax[3])
{
	ObjectRen *obr = obi->obr;
	VolumePrecache *vp = obi->volume_precache;
	VertRen *ver= NULL;
	float co[3];
	int a;
	
	if (vp->bbmin != NULL && vp->bbmax != NULL) {
		copy_v3_v3(bbmin, vp->bbmin);
		copy_v3_v3(bbmax, vp->bbmax);
		return;
	}
	
	vp->bbmin = MEM_callocN(sizeof(float)*3, "volume precache min boundbox corner");
	vp->bbmax = MEM_callocN(sizeof(float)*3, "volume precache max boundbox corner");
	
	INIT_MINMAX(bbmin, bbmax);
	
	for (a=0; a<obr->totvert; a++) {
		if ((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
		else ver++;
		
		copy_v3_v3(co, ver->co);
		
		/* transformed object instance in camera space */
		if (obi->flag & R_TRANSFORMED)
			mul_m4_v3(obi->mat, co);
		
		/* convert to global space */
		mul_m4_v3(re->viewinv, co);
		
		minmax_v3v3_v3(vp->bbmin, vp->bbmax, co);
	}
	
	copy_v3_v3(bbmin, vp->bbmin);
	copy_v3_v3(bbmax, vp->bbmax);
	
}

/* *** light cache filtering *** */

static float get_avg_surrounds(float *cache, int *res, int xx, int yy, int zz)
{
	int x, y, z, x_, y_, z_;
	int added=0;
	float tot=0.0f;
	
	for (z=-1; z <= 1; z++) {
		z_ = zz+z;
		if (z_ >= 0 && z_ <= res[2]-1) {
		
			for (y=-1; y <= 1; y++) {
				y_ = yy+y;
				if (y_ >= 0 && y_ <= res[1]-1) {
				
					for (x=-1; x <= 1; x++) {
						x_ = xx+x;
						if (x_ >= 0 && x_ <= res[0]-1) {
							const int i = BLI_VOXEL_INDEX(x_, y_, z_, res);
							
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
	
	if (added > 0) tot /= added;
	
	return tot;
}

/* function to filter the edges of the light cache, where there was no volume originally.
 * For each voxel which was originally external to the mesh, it finds the average values of
 * the surrounding internal voxels and sets the original external voxel to that average amount.
 * Works almost a bit like a 'dilate' filter */
static void lightcache_filter(VolumePrecache *vp)
{
	int x, y, z;

	for (z=0; z < vp->res[2]; z++) {
		for (y=0; y < vp->res[1]; y++) {
			for (x=0; x < vp->res[0]; x++) {
				/* trigger for outside mesh */
				const int i = BLI_VOXEL_INDEX(x, y, z, vp->res);
				
				if (vp->data_r[i] < -0.f)
					vp->data_r[i] = get_avg_surrounds(vp->data_r, vp->res, x, y, z);
				if (vp->data_g[i] < -0.f)
					vp->data_g[i] = get_avg_surrounds(vp->data_g, vp->res, x, y, z);
				if (vp->data_b[i] < -0.f)
					vp->data_b[i] = get_avg_surrounds(vp->data_b, vp->res, x, y, z);
			}
		}
	}
}

#if 0
static void lightcache_filter2(VolumePrecache *vp)
{
	int x, y, z;
	float *new_r, *new_g, *new_b;
	int field_size = vp->res[0]*vp->res[1]*vp->res[2]*sizeof(float);
	
	new_r = MEM_mallocN(field_size, "temp buffer for light cache filter r channel");
	new_g = MEM_mallocN(field_size, "temp buffer for light cache filter g channel");
	new_b = MEM_mallocN(field_size, "temp buffer for light cache filter b channel");
	
	memcpy(new_r, vp->data_r, field_size);
	memcpy(new_g, vp->data_g, field_size);
	memcpy(new_b, vp->data_b, field_size);
	
	for (z=0; z < vp->res[2]; z++) {
		for (y=0; y < vp->res[1]; y++) {
			for (x=0; x < vp->res[0]; x++) {
				/* trigger for outside mesh */
				const int i = BLI_VOXEL_INDEX(x, y, z, vp->res);
				if (vp->data_r[i] < -0.f)
					new_r[i] = get_avg_surrounds(vp->data_r, vp->res, x, y, z);
				if (vp->data_g[i] < -0.f)
					new_g[i] = get_avg_surrounds(vp->data_g, vp->res, x, y, z);
				if (vp->data_b[i] < -0.f)
					new_b[i] = get_avg_surrounds(vp->data_b, vp->res, x, y, z);
			}
		}
	}
	
	SWAP(float *, vp->data_r, new_r);
	SWAP(float *, vp->data_g, new_g);
	SWAP(float *, vp->data_b, new_b);
	
	if (new_r) { MEM_freeN(new_r); new_r=NULL; }
	if (new_g) { MEM_freeN(new_g); new_g=NULL; }
	if (new_b) { MEM_freeN(new_b); new_b=NULL; }
}
#endif

BLI_INLINE int ms_I(int x, int y, int z, int *n) /* has a pad of 1 voxel surrounding the core for boundary simulation */
{
	/* different ordering to light cache */
	return x*(n[1]+2)*(n[2]+2) + y*(n[2]+2) + z;
}

BLI_INLINE int v_I_pad(int x, int y, int z, int *n) /* has a pad of 1 voxel surrounding the core for boundary simulation */
{
	/* same ordering to light cache, with padding */
	return z*(n[1]+2)*(n[0]+2) + y*(n[0]+2) + x;
}

BLI_INLINE int lc_to_ms_I(int x, int y, int z, int *n)
{ 
	/* converting light cache index to multiple scattering index */
	return (x-1)*(n[1]*n[2]) + (y-1)*(n[2]) + z-1;
}

/* *** multiple scattering approximation *** */

/* get the total amount of light energy in the light cache. used to normalize after multiple scattering */
static float total_ss_energy(Render *re, int do_test_break, VolumePrecache *vp)
{
	int x, y, z;
	const int *res = vp->res;
	float energy=0.f;
	
	for (z=0; z < res[2]; z++) {
		for (y=0; y < res[1]; y++) {
			for (x=0; x < res[0]; x++) {
				const int i = BLI_VOXEL_INDEX(x, y, z, res);
			
				if (vp->data_r[i] > 0.f) energy += vp->data_r[i];
				if (vp->data_g[i] > 0.f) energy += vp->data_g[i];
				if (vp->data_b[i] > 0.f) energy += vp->data_b[i];
			}
		}

		if (do_test_break && re->test_break(re->tbh)) break;
	}
	
	return energy;
}

static float total_ms_energy(Render *re, int do_test_break, float *sr, float *sg, float *sb, int *res)
{
	int x, y, z;
	float energy=0.f;
	
	for (z=1;z<=res[2];z++) {
		for (y=1;y<=res[1];y++) {
			for (x=1;x<=res[0];x++) {
				const int i = ms_I(x, y, z, res);
				
				if (sr[i] > 0.f) energy += sr[i];
				if (sg[i] > 0.f) energy += sg[i];
				if (sb[i] > 0.f) energy += sb[i];
			}
		}

		if (do_test_break && re->test_break(re->tbh)) break;
	}
	
	return energy;
}

static void ms_diffuse(Render *re, int do_test_break, float *x0, float *x, float diff, int *n) //n is the unpadded resolution
{
	int i, j, k, l;
	const float dt = VOL_MS_TIMESTEP;
	size_t size = n[0]*n[1]*n[2];
	const float a = dt*diff*size;
	
	for (l=0; l<20; l++) {
		for (k=1; k<=n[2]; k++) {
			for (j=1; j<=n[1]; j++) {
				for (i=1; i<=n[0]; i++) {
				   x[v_I_pad(i, j, k, n)] = (x0[v_I_pad(i, j, k, n)]) + a*(	x0[v_I_pad(i-1, j, k, n)]+ x0[v_I_pad(i+1, j, k, n)]+ x0[v_I_pad(i, j-1, k, n)]+
																		x0[v_I_pad(i, j+1, k, n)]+ x0[v_I_pad(i, j, k-1, n)]+x0[v_I_pad(i, j, k+1, n)]
																		) / (1+6*a);
				}
			}

			if (do_test_break && re->test_break(re->tbh)) break;
		}

		if (re->test_break(re->tbh)) break;
	}
}

static void multiple_scattering_diffusion(Render *re, VolumePrecache *vp, Material *ma)
{
	const float diff = ma->vol.ms_diff * 0.001f; 	/* compensate for scaling for a nicer UI range */
	const int simframes = (int)(ma->vol.ms_spread * (float)max_iii(vp->res[0], vp->res[1], vp->res[2]));
	const int shade_type = ma->vol.shade_type;
	float fac = ma->vol.ms_intensity;
	
	int x, y, z, m;
	int *n = vp->res;
	const int size = (n[0]+2)*(n[1]+2)*(n[2]+2);
	const int do_test_break = (size > 100000);
	double time, lasttime= PIL_check_seconds_timer();
	float total;
	float c=1.0f;
	float origf;	/* factor for blending in original light cache */
	float energy_ss, energy_ms;

	float *sr0=(float *)MEM_callocN(size*sizeof(float), "temporary multiple scattering buffer");
	float *sr=(float *)MEM_callocN(size*sizeof(float), "temporary multiple scattering buffer");
	float *sg0=(float *)MEM_callocN(size*sizeof(float), "temporary multiple scattering buffer");
	float *sg=(float *)MEM_callocN(size*sizeof(float), "temporary multiple scattering buffer");
	float *sb0=(float *)MEM_callocN(size*sizeof(float), "temporary multiple scattering buffer");
	float *sb=(float *)MEM_callocN(size*sizeof(float), "temporary multiple scattering buffer");

	total = (float)(n[0]*n[1]*n[2]*simframes);
	
	energy_ss = total_ss_energy(re, do_test_break, vp);
	
	/* Scattering as diffusion pass */
	for (m=0; m<simframes; m++) {
		/* add sources */
		for (z=1; z<=n[2]; z++) {
			for (y=1; y<=n[1]; y++) {
				for (x=1; x<=n[0]; x++) {
					const int i = lc_to_ms_I(x, y, z, n);	//lc index
					const int j = ms_I(x, y, z, n);			//ms index
					
					time= PIL_check_seconds_timer();
					c++;
					if (vp->data_r[i] > 0.0f)
						sr[j] += vp->data_r[i];
					if (vp->data_g[i] > 0.0f)
						sg[j] += vp->data_g[i];
					if (vp->data_b[i] > 0.0f)
						sb[j] += vp->data_b[i];
					
					/* Displays progress every second */
					if (time-lasttime>1.0) {
						char str[64];
						BLI_snprintf(str, sizeof(str), IFACE_("Simulating multiple scattering: %d%%"),
						             (int)(100.0f * (c / total)));
						re->i.infostr = str;
						re->stats_draw(re->sdh, &re->i);
						re->i.infostr = NULL;
						lasttime= time;
					}
				}
			}

			if (do_test_break && re->test_break(re->tbh)) break;
		}

		if (re->test_break(re->tbh)) break;

		SWAP(float *, sr, sr0);
		SWAP(float *, sg, sg0);
		SWAP(float *, sb, sb0);

		/* main diffusion simulation */
		ms_diffuse(re, do_test_break, sr0, sr, diff, n);
		ms_diffuse(re, do_test_break, sg0, sg, diff, n);
		ms_diffuse(re, do_test_break, sb0, sb, diff, n);
		
		if (re->test_break(re->tbh)) break;
	}
	
	/* normalization factor to conserve energy */
	energy_ms = total_ms_energy(re, do_test_break, sr, sg, sb, n);
	fac *= (energy_ss / energy_ms);
	
	/* blend multiple scattering back in the light cache */
	if (shade_type == MA_VOL_SHADE_SHADEDPLUSMULTIPLE) {
		/* conserve energy - half single, half multiple */
		origf = 0.5f;
		fac *= 0.5f;
	}
	else {
		origf = 0.0f;
	}

	for (z=1;z<=n[2];z++) {
		for (y=1;y<=n[1];y++) {
			for (x=1;x<=n[0];x++) {
				const int i = lc_to_ms_I(x, y, z, n);	//lc index
				const int j = ms_I(x, y, z, n);			//ms index
				
				vp->data_r[i] = origf * vp->data_r[i] + fac * sr[j];
				vp->data_g[i] = origf * vp->data_g[i] + fac * sg[j];
				vp->data_b[i] = origf * vp->data_b[i] + fac * sb[j];
			}
		}

		if (do_test_break && re->test_break(re->tbh)) break;
	}

	MEM_freeN(sr0);
	MEM_freeN(sr);
	MEM_freeN(sg0);
	MEM_freeN(sg);
	MEM_freeN(sb0);
	MEM_freeN(sb);
}



#if 0  /* debug stuff */
static void *vol_precache_part_test(void *data)
{
	VolPrecachePart *pa = data;

	printf("part number: %d\n", pa->num);
	printf("done: %d\n", pa->done);
	printf("x min: %d   x max: %d\n", pa->minx, pa->maxx);
	printf("y min: %d   y max: %d\n", pa->miny, pa->maxy);
	printf("z min: %d   z max: %d\n", pa->minz, pa->maxz);

	return NULL;
}
#endif

/* Iterate over the 3d voxel grid, and fill the voxels with scattering information
 *
 * It's stored in memory as 3 big float grids next to each other, one for each RGB channel.
 * I'm guessing the memory alignment may work out better this way for the purposes
 * of doing linear interpolation, but I haven't actually tested this theory! :)
 */
typedef struct VolPrecacheState {
	double lasttime;
	int totparts;
} VolPrecacheState;

static void vol_precache_part(TaskPool *pool, void *taskdata, int UNUSED(threadid))
{
	VolPrecacheState *state = (VolPrecacheState *)BLI_task_pool_userdata(pool);
	VolPrecachePart *pa = (VolPrecachePart *)taskdata;
	Render *re = pa->re;

	ObjectInstanceRen *obi = pa->obi;
	RayObject *tree = pa->tree;
	ShadeInput *shi = pa->shi;
	float scatter_col[3] = {0.f, 0.f, 0.f};
	float co[3], cco[3], view[3];
	int x, y, z, i;
	int res[3];
	double time;

	if (re->test_break && re->test_break(re->tbh))
		return;
	
	//printf("thread id %d\n", threadid);

	res[0]= pa->res[0];
	res[1]= pa->res[1];
	res[2]= pa->res[2];

	for (z= pa->minz; z < pa->maxz; z++) {
		co[2] = pa->bbmin[2] + (pa->voxel[2] * (z + 0.5f));
		
		for (y= pa->miny; y < pa->maxy; y++) {
			co[1] = pa->bbmin[1] + (pa->voxel[1] * (y + 0.5f));
			
			for (x=pa->minx; x < pa->maxx; x++) {
				co[0] = pa->bbmin[0] + (pa->voxel[0] * (x + 0.5f));
				
				if (re->test_break && re->test_break(re->tbh))
					break;
				
				/* convert from world->camera space for shading */
				mul_v3_m4v3(cco, pa->viewmat, co);

				i = BLI_VOXEL_INDEX(x, y, z, res);

				/* don't bother if the point is not inside the volume mesh */
				if (!point_inside_obi(tree, obi, cco)) {
					obi->volume_precache->data_r[i] = -1.0f;
					obi->volume_precache->data_g[i] = -1.0f;
					obi->volume_precache->data_b[i] = -1.0f;
					continue;
				}
				
				copy_v3_v3(view, cco);
				normalize_v3(view);
				vol_get_scattering(shi, scatter_col, cco, view);
			
				obi->volume_precache->data_r[i] = scatter_col[0];
				obi->volume_precache->data_g[i] = scatter_col[1];
				obi->volume_precache->data_b[i] = scatter_col[2];
				
			}
		}
	}

	time = PIL_check_seconds_timer();
	if (time - state->lasttime > 1.0) {
		ThreadMutex *mutex = BLI_task_pool_user_mutex(pool);

		if (BLI_mutex_trylock(mutex)) {
			char str[64];
			float ratio = (float)BLI_task_pool_tasks_done(pool)/(float)state->totparts;
			BLI_snprintf(str, sizeof(str), IFACE_("Precaching volume: %d%%"), (int)(100.0f * ratio));
			re->i.infostr = str;
			re->stats_draw(re->sdh, &re->i);
			re->i.infostr = NULL;
			state->lasttime = time;

			BLI_mutex_unlock(mutex);
		}
	}
}

static void precache_setup_shadeinput(Render *re, ObjectInstanceRen *obi, Material *ma, ShadeInput *shi)
{
	memset(shi, 0, sizeof(ShadeInput)); 
	shi->depth= 1;
	shi->mask= 1;
	shi->mat = ma;
	shi->vlr = NULL;
	memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));	/* note, keep this synced with render_types.h */
	shi->har= shi->mat->har;
	shi->obi= obi;
	shi->obr= obi->obr;
	shi->lay = re->lay;
}

static void precache_launch_parts(Render *re, RayObject *tree, ShadeInput *shi, ObjectInstanceRen *obi)
{
	TaskScheduler *task_scheduler;
	TaskPool *task_pool;
	VolumePrecache *vp = obi->volume_precache;
	VolPrecacheState state;
	int i=0, x, y, z;
	float voxel[3];
	int sizex, sizey, sizez;
	float bbmin[3], bbmax[3];
	const int *res;
	int minx, maxx;
	int miny, maxy;
	int minz, maxz;
	int totthread = re->r.threads;
	int parts[3];
	
	if (!vp) return;

	/* currently we just subdivide the box, number of threads per side */
	parts[0] = parts[1] = parts[2] = totthread;
	res = vp->res;
	
	/* setup task scheduler */
	memset(&state, 0, sizeof(state));
	state.totparts = parts[0]*parts[1]*parts[2];
	state.lasttime = PIL_check_seconds_timer();
	
	task_scheduler = BLI_task_scheduler_create(totthread);
	task_pool = BLI_task_pool_create(task_scheduler, &state);

	/* using boundbox in worldspace */
	global_bounds_obi(re, obi, bbmin, bbmax);
	sub_v3_v3v3(voxel, bbmax, bbmin);
	
	voxel[0] /= (float)res[0];
	voxel[1] /= (float)res[1];
	voxel[2] /= (float)res[2];

	for (x=0; x < parts[0]; x++) {
		sizex = ceil(res[0] / (float)parts[0]);
		minx = x * sizex;
		maxx = minx + sizex;
		maxx = (maxx>res[0])?res[0]:maxx;
		
		for (y=0; y < parts[1]; y++) {
			sizey = ceil(res[1] / (float)parts[1]);
			miny = y * sizey;
			maxy = miny + sizey;
			maxy = (maxy>res[1])?res[1]:maxy;
			
			for (z=0; z < parts[2]; z++) {
				VolPrecachePart *pa= MEM_callocN(sizeof(VolPrecachePart), "new precache part");
				
				sizez = ceil(res[2] / (float)parts[2]);
				minz = z * sizez;
				maxz = minz + sizez;
				maxz = (maxz>res[2])?res[2]:maxz;
				
				pa->re = re;
				pa->num = i;
				pa->tree = tree;
				pa->shi = shi;
				pa->obi = obi;
				copy_m4_m4(pa->viewmat, re->viewmat);
				
				copy_v3_v3(pa->bbmin, bbmin);
				copy_v3_v3(pa->voxel, voxel);
				copy_v3_v3_int(pa->res, res);
				
				pa->minx = minx; pa->maxx = maxx;
				pa->miny = miny; pa->maxy = maxy;
				pa->minz = minz; pa->maxz = maxz;
				
				BLI_task_pool_push(task_pool, vol_precache_part, pa, true, TASK_PRIORITY_HIGH);
				
				i++;
			}
		}
	}

	/* work and wait until tasks are done */
	BLI_task_pool_work_and_wait(task_pool);

	/* free */
	BLI_task_pool_free(task_pool);
	BLI_task_scheduler_free(task_scheduler);
}

/* calculate resolution from bounding box in world space */
static int precache_resolution(Render *re, VolumePrecache *vp, ObjectInstanceRen *obi, int res)
{
	float dim[3], div;
	float bbmin[3], bbmax[3];
	
	/* bound box in global space */
	global_bounds_obi(re, obi, bbmin, bbmax);
	sub_v3_v3v3(dim, bbmax, bbmin);
	
	div = max_fff(dim[0], dim[1], dim[2]);
	dim[0] /= div;
	dim[1] /= div;
	dim[2] /= div;
	
	vp->res[0] = ceil(dim[0] * res);
	vp->res[1] = ceil(dim[1] * res);
	vp->res[2] = ceil(dim[2] * res);
	
	if ((vp->res[0] < 1) || (vp->res[1] < 1) || (vp->res[2] < 1))
		return 0;
	
	return 1;
}

/* Precache a volume into a 3D voxel grid.
 * The voxel grid is stored in the ObjectInstanceRen, 
 * in camera space, aligned with the ObjectRen's bounding box.
 * Resolution is defined by the user.
 */
static void vol_precache_objectinstance_threads(Render *re, ObjectInstanceRen *obi, Material *ma)
{
	VolumePrecache *vp;
	RayObject *tree;
	ShadeInput shi;
	
	R = *re;

	/* create a raytree with just the faces of the instanced ObjectRen, 
	 * used for checking if the cached point is inside or outside. */
	tree = makeraytree_object(&R, obi);
	if (!tree) return;

	vp = MEM_callocN(sizeof(VolumePrecache), "volume light cache");
	obi->volume_precache = vp;
	
	if (!precache_resolution(re, vp, obi, ma->vol.precache_resolution)) {
		MEM_freeN(vp);
		vp = NULL;
		return;
	}

	vp->data_r = MEM_callocN(sizeof(float)*vp->res[0]*vp->res[1]*vp->res[2], "volume light cache data red channel");
	vp->data_g = MEM_callocN(sizeof(float)*vp->res[0]*vp->res[1]*vp->res[2], "volume light cache data green channel");
	vp->data_b = MEM_callocN(sizeof(float)*vp->res[0]*vp->res[1]*vp->res[2], "volume light cache data blue channel");
	if (vp->data_r==NULL || vp->data_g==NULL || vp->data_b==NULL) {
		MEM_freeN(vp);
		return;
	}

	/* Need a shadeinput to calculate scattering */
	precache_setup_shadeinput(re, obi, ma, &shi);
	
	precache_launch_parts(re, tree, &shi, obi);

	if (tree) {
		/* TODO: makeraytree_object creates a tree and saves it on OBI,
		 * if we free this tree we should also clear other pointers to it */
		//RE_rayobject_free(tree);
		//tree= NULL;
	}
	
	if (ELEM(ma->vol.shade_type, MA_VOL_SHADE_MULTIPLE, MA_VOL_SHADE_SHADEDPLUSMULTIPLE)) {
		/* this should be before the filtering */
		multiple_scattering_diffusion(re, obi->volume_precache, ma);
	}
		
	lightcache_filter(obi->volume_precache);
}

static int using_lightcache(Material *ma)
{
	return (((ma->vol.shadeflag & MA_VOL_PRECACHESHADING) && (ma->vol.shade_type == MA_VOL_SHADE_SHADED)) ||
	        (ELEM(ma->vol.shade_type, MA_VOL_SHADE_MULTIPLE, MA_VOL_SHADE_SHADEDPLUSMULTIPLE)));
}

/* loop through all objects (and their associated materials)
 * marked for pre-caching in convertblender.c, and pre-cache them */
void volume_precache(Render *re)
{
	ObjectInstanceRen *obi;
	VolumeOb *vo;

	re->i.infostr = IFACE_("Volume preprocessing");
	re->stats_draw(re->sdh, &re->i);

	for (vo= re->volumes.first; vo; vo= vo->next) {
		if (using_lightcache(vo->ma)) {
			for (obi= re->instancetable.first; obi; obi= obi->next) {
				if (obi->obr == vo->obr) {
					vol_precache_objectinstance_threads(re, obi, vo->ma);

					if (re->test_break && re->test_break(re->tbh))
						break;
				}
			}

			if (re->test_break && re->test_break(re->tbh))
				break;
		}
	}
	
	re->i.infostr = NULL;
	re->stats_draw(re->sdh, &re->i);
}

void free_volume_precache(Render *re)
{
	ObjectInstanceRen *obi;
	
	for (obi= re->instancetable.first; obi; obi= obi->next) {
		if (obi->volume_precache != NULL) {
			MEM_freeN(obi->volume_precache->data_r);
			MEM_freeN(obi->volume_precache->data_g);
			MEM_freeN(obi->volume_precache->data_b);
			MEM_freeN(obi->volume_precache->bbmin);
			MEM_freeN(obi->volume_precache->bbmax);
			MEM_freeN(obi->volume_precache);
			obi->volume_precache = NULL;
		}
	}
	
	BLI_freelistN(&re->volumes);
}

int point_inside_volume_objectinstance(Render *re, ObjectInstanceRen *obi, const float co[3])
{
	RayObject *tree;
	int inside=0;
	
	tree = makeraytree_object(re, obi);
	if (!tree) return 0;
	
	inside = point_inside_obi(tree, obi, co);
	
	//TODO: makeraytree_object creates a tree and saves it on OBI, if we free this tree we should also clear other pointers to it
	//RE_rayobject_free(tree);
	//tree= NULL;
	
	return inside;
}

