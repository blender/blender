/**
 * smoke.c
 *
 * $Id$
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* Part of the code copied from elbeem fluid library, copyright by Nils Thuerey */

#include <GL/glew.h>

#include "MEM_guardedalloc.h"

#include <float.h>
#include <math.h>
#include "stdio.h"
#include "string.h" /* memset */

#include "BLI_linklist.h"
#include "BLI_rand.h"
#include "BLI_jitter.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_kdtree.h"
#include "BLI_kdopbvh.h"

#include "BKE_bvhutils.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_smoke.h"
#include "BKE_utildefines.h"

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

#include "smoke_API.h"

#include "BKE_smoke.h"

#ifdef _WIN32
#include <time.h>
#include <stdio.h>
#include <conio.h>
#include <windows.h>

static LARGE_INTEGER liFrequency;
static LARGE_INTEGER liStartTime;
static LARGE_INTEGER liCurrentTime;

static void tstart ( void )
{
	QueryPerformanceFrequency ( &liFrequency );
	QueryPerformanceCounter ( &liStartTime );
}
static void tend ( void )
{
	QueryPerformanceCounter ( &liCurrentTime );
}
static double tval()
{
	return ((double)( (liCurrentTime.QuadPart - liStartTime.QuadPart)* (double)1000.0/(double)liFrequency.QuadPart ));
}
#else
#include <sys/time.h>
static struct timeval _tstart, _tend;
static struct timezone tz;
static void tstart ( void )
{
	gettimeofday ( &_tstart, &tz );
}
static void tend ( void )
{
	gettimeofday ( &_tend,&tz );
}
static double tval()
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec*1000 + ( double ) _tstart.tv_usec/ ( 1000 );
	t2 = ( double ) _tend.tv_sec*1000 + ( double ) _tend.tv_usec/ ( 1000 );
	return t2-t1;
}
#endif

struct Object;
struct Scene;
struct DerivedMesh;
struct SmokeModifierData;

// forward declerations
static void get_cell(float *p0, int res[3], float dx, float *pos, int *cell, int correct);
void calcTriangleDivs(Object *ob, MVert *verts, int numverts, MFace *tris, int numfaces, int numtris, int **tridivs, float cell_len);
static void fill_scs_points(Object *ob, DerivedMesh *dm, SmokeCollSettings *scs);

#define TRI_UVOFFSET (1./4.)

int smokeModifier_init (SmokeModifierData *smd, Object *ob, Scene *scene, DerivedMesh *dm)
{
	if((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain && !smd->domain->fluid)
	{
		size_t i;
		float min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
		float size[3];
		MVert *verts = dm->getVertArray(dm);
		float scale = 0.0;
		int res;		

		res = smd->domain->maxres;

		// get BB of domain
		for(i = 0; i < dm->getNumVerts(dm); i++)
		{
			float tmp[3];

			VECCOPY(tmp, verts[i].co);
			Mat4MulVecfl(ob->obmat, tmp);

			// min BB
			min[0] = MIN2(min[0], tmp[0]);
			min[1] = MIN2(min[1], tmp[1]);
			min[2] = MIN2(min[2], tmp[2]);

			// max BB
			max[0] = MAX2(max[0], tmp[0]);
			max[1] = MAX2(max[1], tmp[1]);
			max[2] = MAX2(max[2], tmp[2]);
		}

		VECCOPY(smd->domain->p0, min);
		VECCOPY(smd->domain->p1, max);

		// calc other res with max_res provided
		VECSUB(size, max, min);

		// printf("size: %f, %f, %f\n", size[0], size[1], size[2]);

		// prevent crash when initializing a plane as domain
		if((size[0] < FLT_EPSILON) || (size[1] < FLT_EPSILON) || (size[2] < FLT_EPSILON))
			return 0;

		if(size[0] > size[1])
		{
			if(size[0] > size[1])
			{
				scale = res / size[0];
				smd->domain->dx = size[0] / res;
				smd->domain->res[0] = res;
				smd->domain->res[1] = (int)(size[1] * scale + 0.5);
				smd->domain->res[2] = (int)(size[2] * scale + 0.5);
			}
			else
			{
				scale = res / size[1];
				smd->domain->dx = size[1] / res;
				smd->domain->res[1] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[2] = (int)(size[2] * scale + 0.5);
			}
		}
		else
		{
			if(size[1] > size[2])
			{
				scale = res / size[1];
				smd->domain->dx = size[1] / res;
				smd->domain->res[1] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[2] = (int)(size[2] * scale + 0.5);
			}
			else
			{
				scale = res / size[2];
				smd->domain->dx = size[2] / res;
				smd->domain->res[2] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[1] = (int)(size[1] * scale + 0.5);
			}
		}

		// printf("smd->domain->dx: %f\n", smd->domain->dx);

		// TODO: put in failsafe if res<=0 - dg

		// printf("res[0]: %d, res[1]: %d, res[2]: %d\n", smd->domain->res[0], smd->domain->res[1], smd->domain->res[2]);
		// dt max is 0.1
		smd->domain->fluid = smoke_init(smd->domain->res, smd->domain->p0, 0.1);
		smd->time = scene->r.cfra;

		if(smd->domain->flags & MOD_SMOKE_HIGHRES)
		{
			smd->domain->wt = smoke_turbulence_init(smd->domain->res, smd->domain->amplify + 1, smd->domain->noise);
			smd->domain->res_wt[0] = smd->domain->res[0] * (smd->domain->amplify + 1);
			smd->domain->res_wt[1] = smd->domain->res[1] * (smd->domain->amplify + 1);			
			smd->domain->res_wt[2] = smd->domain->res[2] * (smd->domain->amplify + 1);			
			smd->domain->dx_wt = smd->domain->dx / (smd->domain->amplify + 1);		
			// printf("smd->domain->amplify: %d\n",  smd->domain->amplify);
			// printf("(smd->domain->flags & MOD_SMOKE_HIGHRES)\n");
		}

		if(!smd->domain->shadow)
			smd->domain->shadow = MEM_callocN(sizeof(float) * smd->domain->res[0] * smd->domain->res[1] * smd->domain->res[2], "SmokeDomainShadow");

		smoke_initBlenderRNA(smd->domain->fluid, &(smd->domain->alpha), &(smd->domain->beta));

		if(smd->domain->wt)	
		{
			smoke_initWaveletBlenderRNA(smd->domain->wt, &(smd->domain->strength));
			// printf("smoke_initWaveletBlenderRNA\n");
		}
		return 1;
	}
	else if((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow)
	{
		// handle flow object here
		// XXX TODO

		smd->time = scene->r.cfra;

		// update particle lifetime to be one frame
		// smd->flow->psys->part->lifetime = scene->r.efra + 1;
/*
		if(!smd->flow->bvh)
		{
			// smd->flow->bvh = MEM_callocN(sizeof(BVHTreeFromMesh), "smoke_bvhfromfaces");
			// bvhtree_from_mesh_faces(smd->flow->bvh, dm, 0.0, 2, 6);

			// copy obmat
			// Mat4CpyMat4(smd->flow->mat, ob->obmat);
			// Mat4CpyMat4(smd->flow->mat_old, ob->obmat);
		}
*/

		return 1;
	}
	else if((smd->type & MOD_SMOKE_TYPE_COLL))
	{
		smd->time = scene->r.cfra;

		// todo: delete this when loading colls work -dg
		if(!smd->coll)
			smokeModifier_createType(smd);

		if(!smd->coll->points)
		{
			// init collision points
			SmokeCollSettings *scs = smd->coll;

			// copy obmat
			Mat4CpyMat4(scs->mat, ob->obmat);
			Mat4CpyMat4(scs->mat_old, ob->obmat);

			fill_scs_points(ob, dm, scs);
		}

		if(!smd->coll->bvhtree)
		{
			smd->coll->bvhtree = NULL; // bvhtree_build_from_smoke ( ob->obmat, dm->getFaceArray(dm), dm->getNumFaces(dm), dm->getVertArray(dm), dm->getNumVerts(dm), 0.0 );
		}
		return 1;
	}

	return 1;
}

static void fill_scs_points(Object *ob, DerivedMesh *dm, SmokeCollSettings *scs)
{
	MVert *mvert = dm->getVertArray(dm);
	MFace *mface = dm->getFaceArray(dm);
	int i = 0, divs = 0;
	int *tridivs = NULL;
	float cell_len = 1.0 / 50.0; // for res = 50
	int newdivs = 0;
	int quads = 0, facecounter = 0;

	// count quads
	for(i = 0; i < dm->getNumFaces(dm); i++)
	{
		if(mface[i].v4)
			quads++;
	}

	calcTriangleDivs(ob, mvert, dm->getNumVerts(dm), mface,  dm->getNumFaces(dm), dm->getNumFaces(dm) + quads, &tridivs, cell_len);

	// count triangle divisions
	for(i = 0; i < dm->getNumFaces(dm) + quads; i++)
	{
		divs += (tridivs[3 * i] + 1) * (tridivs[3 * i + 1] + 1) * (tridivs[3 * i + 2] + 1);
	}

	// printf("divs: %d\n", divs);

	scs->points = MEM_callocN(sizeof(float) * (dm->getNumVerts(dm) + divs) * 3, "SmokeCollPoints");

	for(i = 0; i < dm->getNumVerts(dm); i++)
	{
		float tmpvec[3];
		VECCOPY(tmpvec, mvert[i].co);
		Mat4MulVecfl (ob->obmat, tmpvec);
		VECCOPY(&scs->points[i * 3], tmpvec);
	}
	
	for(i = 0, facecounter = 0; i < dm->getNumFaces(dm); i++)
	{
		int again = 0;
		do
		{
			int j, k;
			int divs1 = tridivs[3 * facecounter + 0];
			int divs2 = tridivs[3 * facecounter + 1];
			//int divs3 = tridivs[3 * facecounter + 2];
			float side1[3], side2[3], trinormorg[3], trinorm[3];
			
			if(again == 1 && mface[i].v4)
			{
				VECSUB(side1,  mvert[ mface[i].v3 ].co, mvert[ mface[i].v1 ].co);
				VECSUB(side2,  mvert[ mface[i].v4 ].co, mvert[ mface[i].v1 ].co);
			}
			else
			{
				VECSUB(side1,  mvert[ mface[i].v2 ].co, mvert[ mface[i].v1 ].co);
				VECSUB(side2,  mvert[ mface[i].v3 ].co, mvert[ mface[i].v1 ].co);
			}

			Crossf(trinormorg, side1, side2);
			Normalize(trinormorg);
			VECCOPY(trinorm, trinormorg);
			VecMulf(trinorm, 0.25 * cell_len);

			for(j = 0; j <= divs1; j++)
			{
				for(k = 0; k <= divs2; k++)
				{
					float p1[3], p2[3], p3[3], p[3]={0,0,0}; 
					const float uf = (float)(j + TRI_UVOFFSET) / (float)(divs1 + 0.0);
					const float vf = (float)(k + TRI_UVOFFSET) / (float)(divs2 + 0.0);
					float tmpvec[3];
					
					if(uf+vf > 1.0) 
					{
						// printf("bigger - divs1: %d, divs2: %d\n", divs1, divs2);
						continue;
					}

					VECCOPY(p1, mvert[ mface[i].v1 ].co);
					if(again == 1 && mface[i].v4)
					{
						VECCOPY(p2, mvert[ mface[i].v3 ].co);
						VECCOPY(p3, mvert[ mface[i].v4 ].co);
					}
					else
					{
						VECCOPY(p2, mvert[ mface[i].v2 ].co);
						VECCOPY(p3, mvert[ mface[i].v3 ].co);
					}

					VecMulf(p1, (1.0-uf-vf));
					VecMulf(p2, uf);
					VecMulf(p3, vf);
					
					VECADD(p, p1, p2);
					VECADD(p, p, p3);

					if(newdivs > divs)
						printf("mem problem\n");

					// mMovPoints.push_back(p + trinorm);
					VECCOPY(tmpvec, p);
					VECADD(tmpvec, tmpvec, trinorm);
					Mat4MulVecfl (ob->obmat, tmpvec);
					VECCOPY(&scs->points[3 * (dm->getNumVerts(dm) + newdivs)], tmpvec);
					newdivs++;

					if(newdivs > divs)
						printf("mem problem\n");

					// mMovPoints.push_back(p - trinorm);
					VECCOPY(tmpvec, p);
					VECSUB(tmpvec, tmpvec, trinorm);
					Mat4MulVecfl (ob->obmat, tmpvec);
					VECCOPY(&scs->points[3 * (dm->getNumVerts(dm) + newdivs)], tmpvec);
					newdivs++;
				}
			}

			if(again == 0 && mface[i].v4)
				again++;
			else
				again = 0;

			facecounter++;

		} while(again!=0);
	}

	scs->numpoints = dm->getNumVerts(dm) + newdivs;

	MEM_freeN(tridivs);
}

/*! init triangle divisions */
void calcTriangleDivs(Object *ob, MVert *verts, int numverts, MFace *faces, int numfaces, int numtris, int **tridivs, float cell_len) 
{
	// mTriangleDivs1.resize( faces.size() );
	// mTriangleDivs2.resize( faces.size() );
	// mTriangleDivs3.resize( faces.size() );

	size_t i = 0, facecounter = 0;
	float maxscale[3] = {1,1,1}; // = channelFindMaxVf(mcScale);
	float maxpart = ABS(maxscale[0]);
	float scaleFac = 0;
	float fsTri = 0;
	if(ABS(maxscale[1])>maxpart) maxpart = ABS(maxscale[1]);
	if(ABS(maxscale[2])>maxpart) maxpart = ABS(maxscale[2]);
	scaleFac = 1.0 / maxpart;
	// featureSize = mLevel[mMaxRefine].nodeSize
	fsTri = cell_len * 0.5 * scaleFac;

	if(*tridivs)
		MEM_freeN(*tridivs);

	*tridivs = MEM_callocN(sizeof(int) * numtris * 3, "Smoke_Tridivs");

	for(i = 0, facecounter = 0; i < numfaces; i++) 
	{
		float p0[3], p1[3], p2[3];
		float side1[3];
		float side2[3];
		float side3[3];
		int divs1=0, divs2=0, divs3=0;

		VECCOPY(p0, verts[faces[i].v1].co);
		Mat4MulVecfl (ob->obmat, p0);
		VECCOPY(p1, verts[faces[i].v2].co);
		Mat4MulVecfl (ob->obmat, p1);
		VECCOPY(p2, verts[faces[i].v3].co);
		Mat4MulVecfl (ob->obmat, p2);

		VECSUB(side1, p1, p0);
		VECSUB(side2, p2, p0);
		VECSUB(side3, p1, p2);

		if(INPR(side1, side1) > fsTri*fsTri) 
		{ 
			float tmp = Normalize(side1);
			divs1 = (int)ceil(tmp/fsTri); 
		}
		if(INPR(side2, side2) > fsTri*fsTri) 
		{ 
			float tmp = Normalize(side2);
			divs2 = (int)ceil(tmp/fsTri); 
			
			/*
			// debug
			if(i==0)
				printf("b tmp: %f, fsTri: %f, divs2: %d\n", tmp, fsTri, divs2);
			*/
		}

		(*tridivs)[3 * facecounter + 0] = divs1;
		(*tridivs)[3 * facecounter + 1] = divs2;
		(*tridivs)[3 * facecounter + 2] = divs3;

		// TODO quad case
		if(faces[i].v4)
		{
			divs1=0, divs2=0, divs3=0;

			facecounter++;
			
			VECCOPY(p0, verts[faces[i].v3].co);
			Mat4MulVecfl (ob->obmat, p0);
			VECCOPY(p1, verts[faces[i].v4].co);
			Mat4MulVecfl (ob->obmat, p1);
			VECCOPY(p2, verts[faces[i].v1].co);
			Mat4MulVecfl (ob->obmat, p2);

			VECSUB(side1, p1, p0);
			VECSUB(side2, p2, p0);
			VECSUB(side3, p1, p2);

			if(INPR(side1, side1) > fsTri*fsTri) 
			{ 
				float tmp = Normalize(side1);
				divs1 = (int)ceil(tmp/fsTri); 
			}
			if(INPR(side2, side2) > fsTri*fsTri) 
			{ 
				float tmp = Normalize(side2);
				divs2 = (int)ceil(tmp/fsTri); 
			}

			(*tridivs)[3 * facecounter + 0] = divs1;
			(*tridivs)[3 * facecounter + 1] = divs2;
			(*tridivs)[3 * facecounter + 2] = divs3;
		}
		facecounter++;
	}
}

static void smokeModifier_freeDomain(SmokeModifierData *smd)
{
	if(smd->domain)
	{
		if(smd->domain->shadow)
				MEM_freeN(smd->domain->shadow);
			smd->domain->shadow = NULL;

		if(smd->domain->fluid)
			smoke_free(smd->domain->fluid);

		if(smd->domain->wt)
			smoke_turbulence_free(smd->domain->wt);

		if(smd->domain->effector_weights)
				MEM_freeN(smd->domain->effector_weights);
		smd->domain->effector_weights = NULL;

		BKE_ptcache_free_list(&(smd->domain->ptcaches[0]));
		smd->domain->point_cache[0] = NULL;
		BKE_ptcache_free_list(&(smd->domain->ptcaches[1]));
		smd->domain->point_cache[1] = NULL;

		MEM_freeN(smd->domain);
		smd->domain = NULL;
	}
}

static void smokeModifier_freeFlow(SmokeModifierData *smd)
{
	if(smd->flow)
	{
/*
		if(smd->flow->bvh)
		{
			free_bvhtree_from_mesh(smd->flow->bvh);
			MEM_freeN(smd->flow->bvh);
		}
		smd->flow->bvh = NULL;
*/
		MEM_freeN(smd->flow);
		smd->flow = NULL;
	}
}

static void smokeModifier_freeCollision(SmokeModifierData *smd)
{
	if(smd->coll)
	{
		if(smd->coll->points)
		{
			MEM_freeN(smd->coll->points);
			smd->coll->points = NULL;
		}

		if(smd->coll->bvhtree)
		{
			BLI_bvhtree_free(smd->coll->bvhtree);
			smd->coll->bvhtree = NULL;
		}

		if(smd->coll->dm)
			smd->coll->dm->release(smd->coll->dm);
		smd->coll->dm = NULL;

		MEM_freeN(smd->coll);
		smd->coll = NULL;
	}
}

void smokeModifier_reset_turbulence(struct SmokeModifierData *smd)
{
	if(smd && smd->domain && smd->domain->wt)
	{
		smoke_turbulence_free(smd->domain->wt);
		smd->domain->wt = NULL;
	}
}

void smokeModifier_reset(struct SmokeModifierData *smd)
{
	if(smd)
	{
		if(smd->domain)
		{
			if(smd->domain->shadow)
				MEM_freeN(smd->domain->shadow);
			smd->domain->shadow = NULL;

			if(smd->domain->fluid)
			{
				smoke_free(smd->domain->fluid);
				smd->domain->fluid = NULL;
			}

			smd->domain->point_cache[0]->flag |= PTCACHE_OUTDATED;
			smd->domain->point_cache[1]->flag |= PTCACHE_OUTDATED;

			smokeModifier_reset_turbulence(smd);

			smd->time = -1;

			// printf("reset domain end\n");
		}
		else if(smd->flow)
		{
			/*
			if(smd->flow->bvh)
			{
				free_bvhtree_from_mesh(smd->flow->bvh);
				MEM_freeN(smd->flow->bvh);
			}
			smd->flow->bvh = NULL;
			*/
		}
		else if(smd->coll)
		{
			if(smd->coll->points)
			{
				MEM_freeN(smd->coll->points);
				smd->coll->points = NULL;
			}

			if(smd->coll->bvhtree)
			{
				BLI_bvhtree_free(smd->coll->bvhtree);
				smd->coll->bvhtree = NULL;
			}

			if(smd->coll->dm)
				smd->coll->dm->release(smd->coll->dm);
			smd->coll->dm = NULL;

		}
	}
}

void smokeModifier_free (SmokeModifierData *smd)
{
	if(smd)
	{
		smokeModifier_freeDomain(smd);
		smokeModifier_freeFlow(smd);
		smokeModifier_freeCollision(smd);
	}
}

void smokeModifier_createType(struct SmokeModifierData *smd)
{
	if(smd)
	{
		if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
		{
			if(smd->domain)
				smokeModifier_freeDomain(smd);

			smd->domain = MEM_callocN(sizeof(SmokeDomainSettings), "SmokeDomain");

			smd->domain->smd = smd;

			smd->domain->point_cache[0] = BKE_ptcache_add(&(smd->domain->ptcaches[0]));
			smd->domain->point_cache[0]->flag |= PTCACHE_DISK_CACHE;
			smd->domain->point_cache[0]->step = 1;

			smd->domain->point_cache[1] = BKE_ptcache_add(&(smd->domain->ptcaches[1]));
			smd->domain->point_cache[1]->flag |= PTCACHE_DISK_CACHE;
			smd->domain->point_cache[1]->step = 1;

			/* set some standard values */
			smd->domain->fluid = NULL;
			smd->domain->wt = NULL;			
			smd->domain->eff_group = NULL;
			smd->domain->fluid_group = NULL;
			smd->domain->coll_group = NULL;
			smd->domain->maxres = 32;
			smd->domain->amplify = 1;			
			smd->domain->omega = 1.0;			
			smd->domain->alpha = -0.001;
			smd->domain->beta = 0.1;
			smd->domain->flags = MOD_SMOKE_DISSOLVE_LOG;
			smd->domain->strength = 2.0;
			smd->domain->noise = MOD_SMOKE_NOISEWAVE;
			smd->domain->diss_speed = 5;
			// init 3dview buffer
			smd->domain->viewsettings = 0;
			smd->domain->effector_weights = BKE_add_effector_weights(NULL);
		}
		else if(smd->type & MOD_SMOKE_TYPE_FLOW)
		{
			if(smd->flow)
				smokeModifier_freeFlow(smd);

			smd->flow = MEM_callocN(sizeof(SmokeFlowSettings), "SmokeFlow");

			smd->flow->smd = smd;

			/* set some standard values */
			smd->flow->density = 1.0;
			smd->flow->temp = 1.0;

			smd->flow->psys = NULL;

		}
		else if(smd->type & MOD_SMOKE_TYPE_COLL)
		{
			if(smd->coll)
				smokeModifier_freeCollision(smd);

			smd->coll = MEM_callocN(sizeof(SmokeCollSettings), "SmokeColl");

			smd->coll->smd = smd;
			smd->coll->points = NULL;
			smd->coll->numpoints = 0;
			smd->coll->bvhtree = NULL;
			smd->coll->dm = NULL;
		}
	}
}

// forward decleration
static void smoke_calc_transparency(float *result, float *input, float *p0, float *p1, int res[3], float dx, float *light, bresenham_callback cb, float correct);
static float calc_voxel_transp(float *result, float *input, int res[3], int *pixel, float *tRay, float correct);
static int get_lamp(Scene *scene, float *light)
{	
	Base *base_tmp = NULL;	
	for(base_tmp = scene->base.first; base_tmp; base_tmp= base_tmp->next) 	
	{		
		if(base_tmp->object->type == OB_LAMP) 		
		{			
			Lamp *la = (Lamp *)base_tmp->object->data;	

			if(la->type == LA_LOCAL)			
			{				
				VECCOPY(light, base_tmp->object->obmat[3]);				
				return 1;			
			}		
		}	
	}	
	return 0;
}

static void smoke_calc_domain(Scene *scene, Object *ob, SmokeModifierData *smd)
{
	SmokeDomainSettings *sds = smd->domain;
	GroupObject *go = NULL;			
	Base *base = NULL;	

	// do flows and fluids
	if(1)			
	{				
		Object *otherobj = NULL;				
		ModifierData *md = NULL;
		if(sds->fluid_group) // we use groups since we have 2 domains
			go = sds->fluid_group->gobject.first;				
		else					
			base = scene->base.first;
		while(base || go)
		{					
			otherobj = NULL;
			if(sds->fluid_group) 
			{
				if(go->ob)							
					otherobj = go->ob;					
			}					
			else						
				otherobj = base->object;
			if(!otherobj)
			{
				if(sds->fluid_group)
					go = go->next;
				else
					base= base->next;

				continue;
			}

			md = modifiers_findByType(otherobj, eModifierType_Smoke);
			
			// check for active smoke modifier
			if(md && md->mode & (eModifierMode_Realtime | eModifierMode_Render))
			{
				SmokeModifierData *smd2 = (SmokeModifierData *)md;
				
				// check for initialized smoke object
				if((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow)						
				{
					// we got nice flow object
					SmokeFlowSettings *sfs = smd2->flow;
					
					if(sfs->psys && sfs->psys->part && sfs->psys->part->type==PART_EMITTER) // is particle system selected
					{
						ParticleSystem *psys = sfs->psys;
						ParticleSettings *part=psys->part;
						ParticleData *pa = NULL;								
						int p = 0;								
						float *density = smoke_get_density(sds->fluid);								
						float *bigdensity = smoke_turbulence_get_density(sds->wt);								
						float *heat = smoke_get_heat(sds->fluid);								
						float *velocity_x = smoke_get_velocity_x(sds->fluid);								
						float *velocity_y = smoke_get_velocity_y(sds->fluid);								
						float *velocity_z = smoke_get_velocity_z(sds->fluid);								
						unsigned char *obstacle = smoke_get_obstacle(sds->fluid);								
						int bigres[3];	
														
						// mostly copied from particle code								
						for(p=0, pa=psys->particles; p<psys->totpart; p++, pa++)								
						{									
							int cell[3];									
							size_t i = 0;									
							size_t index = 0;									
							int badcell = 0;																		
							if(pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN)==0) continue;									
							else if(pa->alive == PARS_DEAD && (part->flag & PART_DIED)==0) continue;									
							else if(pa->flag & (PARS_UNEXIST+PARS_NO_DISP)) continue;																		
							// VECCOPY(pos, pa->state.co);									
							// Mat4MulVecfl (ob->imat, pos);																		
							// 1. get corresponding cell	
							get_cell(smd->domain->p0, smd->domain->res, smd->domain->dx, pa->state.co, cell, 0);																	
							// check if cell is valid (in the domain boundary)									
							for(i = 0; i < 3; i++)									
							{										
								if((cell[i] > sds->res[i] - 1) || (cell[i] < 0))										
								{											
									badcell = 1;											
									break;										
								}									
							}																			
							if(badcell)										
								continue;																		
							// 2. set cell values (heat, density and velocity)									
							index = smoke_get_index(cell[0], sds->res[0], cell[1], sds->res[1], cell[2]);																		
							if(!(sfs->type & MOD_SMOKE_FLOW_TYPE_OUTFLOW) && !(obstacle[index] & 2)) // this is inflow									
							{										
								// heat[index] += sfs->temp * 0.1;										
								// density[index] += sfs->density * 0.1;
								heat[index] = sfs->temp;
								density[index] = sfs->density;

								/*
								velocity_x[index] = pa->state.vel[0];
								velocity_y[index] = pa->state.vel[1];
								velocity_z[index] = pa->state.vel[2];										
								*/										
								
								// obstacle[index] |= 2;
								// we need different handling for the high-res feature
								if(bigdensity)
								{
									// init all surrounding cells according to amplification, too
									int i, j, k;

									smoke_turbulence_get_res(smd->domain->wt, bigres);

									for(i = 0; i < smd->domain->amplify + 1; i++)
										for(j = 0; j < smd->domain->amplify + 1; j++)
											for(k = 0; k < smd->domain->amplify + 1; k++)													
											{														
												index = smoke_get_index((smd->domain->amplify + 1)* cell[0] + i, bigres[0], (smd->domain->amplify + 1)* cell[1] + j, bigres[1], (smd->domain->amplify + 1)* cell[2] + k);														
												bigdensity[index] = sfs->density;													
											}										
								}									
							}									
							else if(sfs->type & MOD_SMOKE_FLOW_TYPE_OUTFLOW) // outflow									
							{										
								heat[index] = 0.f;										
								density[index] = 0.f;										
								velocity_x[index] = 0.f;										
								velocity_y[index] = 0.f;										
								velocity_z[index] = 0.f;
								// we need different handling for the high-res feature
								if(bigdensity)
								{
									// init all surrounding cells according to amplification, too											
									int i, j, k;
									smoke_turbulence_get_res(smd->domain->wt, bigres);

									for(i = 0; i < smd->domain->amplify + 1; i++)
										for(j = 0; j < smd->domain->amplify + 1; j++)
											for(k = 0; k < smd->domain->amplify + 1; k++)
											{														
												index = smoke_get_index((smd->domain->amplify + 1)* cell[0] + i, bigres[0], (smd->domain->amplify + 1)* cell[1] + j, bigres[1], (smd->domain->amplify + 1)* cell[2] + k);														
												bigdensity[index] = 0.f;													
											}										
								}									
							}	// particles loop							
					}							
				}							
				else							
				{								
					/*								
					for()								
					{									
						// no psys									
						BVHTreeNearest nearest;
						nearest.index = -1;
						nearest.dist = FLT_MAX;

						BLI_bvhtree_find_nearest(sfs->bvh->tree, pco, &nearest, sfs->bvh->nearest_callback, sfs->bvh);
					}*/							
				}						
			}						
		}
			if(sds->fluid_group)
				go = go->next;
			else
				base= base->next;
		}
	}

	// do effectors
	{
		ListBase *effectors = pdInitEffectors(scene, ob, NULL, sds->effector_weights);

		if(effectors)
		{
			float *density = smoke_get_density(sds->fluid);
			float *force_x = smoke_get_force_x(sds->fluid);
			float *force_y = smoke_get_force_y(sds->fluid);
			float *force_z = smoke_get_force_z(sds->fluid);
			float *velocity_x = smoke_get_velocity_x(sds->fluid);
			float *velocity_y = smoke_get_velocity_y(sds->fluid);
			float *velocity_z = smoke_get_velocity_z(sds->fluid);
			int x, y, z;

			// precalculate wind forces
			for(x = 0; x < sds->res[0]; x++)
				for(y = 0; y < sds->res[1]; y++)
					for(z = 0; z < sds->res[2]; z++)
			{	
				EffectedPoint epoint;
				float voxelCenter[3] = {0,0,0} , vel[3] = {0,0,0} , retvel[3] = {0,0,0};
				unsigned int index = smoke_get_index(x, sds->res[0], y, sds->res[1], z);

				if(density[index] < FLT_EPSILON)					
					continue;	

				vel[0] = velocity_x[index];
				vel[1] = velocity_y[index];
				vel[2] = velocity_z[index];

				voxelCenter[0] = sds->p0[0] + sds->dx *  x + sds->dx * 0.5;
				voxelCenter[1] = sds->p0[1] + sds->dx *  y + sds->dx * 0.5;
				voxelCenter[2] = sds->p0[2] + sds->dx *  z + sds->dx * 0.5;

				pd_point_from_loc(scene, voxelCenter, vel, index, &epoint);
				pdDoEffectors(effectors, NULL, sds->effector_weights, &epoint, retvel, NULL);

				// TODO dg - do in force!
				force_x[index] = MIN2(MAX2(-1.0, retvel[0] * 0.2), 1.0); 
				force_y[index] = MIN2(MAX2(-1.0, retvel[1] * 0.2), 1.0); 
				force_z[index] = MIN2(MAX2(-1.0, retvel[2] * 0.2), 1.0);
			}
		}

		pdEndEffectors(&effectors);
	}

	// do collisions	
	if(1)
	{
		Object *otherobj = NULL;
		ModifierData *md = NULL;

		if(sds->coll_group) // we use groups since we have 2 domains
			go = sds->coll_group->gobject.first;
		else
			base = scene->base.first;

		while(base || go)
		{
			otherobj = NULL;
			if(sds->coll_group) 
			{						
				if(go->ob)							
					otherobj = go->ob;					
			}					
			else						
				otherobj = base->object;					
			if(!otherobj)					
			{						
				if(sds->coll_group)							
					go = go->next;						
				else							
					base= base->next;						
				continue;					
			}			
			md = modifiers_findByType(otherobj, eModifierType_Smoke);
			
			// check for active smoke modifier
			if(md && md->mode & (eModifierMode_Realtime | eModifierMode_Render))					
			{
				SmokeModifierData *smd2 = (SmokeModifierData *)md;

				if((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll)
				{
					// we got nice collision object
					SmokeCollSettings *scs = smd2->coll;
					size_t i, j;
					unsigned char *obstacles = smoke_get_obstacle(smd->domain->fluid);

					for(i = 0; i < scs->numpoints; i++)
					{
						int badcell = 0;
						size_t index = 0;
						int cell[3];

						// 1. get corresponding cell
						get_cell(smd->domain->p0, smd->domain->res, smd->domain->dx, &scs->points[3 * i], cell, 0);
					
						// check if cell is valid (in the domain boundary)
						for(j = 0; j < 3; j++)
							if((cell[j] > sds->res[j] - 1) || (cell[j] < 0))
							{
								badcell = 1;
								break;
							}
																
							if(badcell)									
								continue;
						// 2. set cell values (heat, density and velocity)
						index = smoke_get_index(cell[0], sds->res[0], cell[1], sds->res[1], cell[2]);
														
						// printf("cell[0]: %d, cell[1]: %d, cell[2]: %d\n", cell[0], cell[1], cell[2]);								
						// printf("res[0]: %d, res[1]: %d, res[2]: %d, index: %d\n\n", sds->res[0], sds->res[1], sds->res[2], index);																	
						obstacles[index] = 1;
						// for moving gobstacles								
						/*
						const LbmFloat maxVelVal = 0.1666;
						const LbmFloat maxusqr = maxVelVal*maxVelVal*3. *1.5;

						LbmVec objvel = vec2L((mMOIVertices[n]-mMOIVerticesOld[n]) /dvec); 
						{ 								
						const LbmFloat usqr = (objvel[0]*objvel[0]+objvel[1]*objvel[1]+objvel[2]*objvel[2])*1.5; 								
						USQRMAXCHECK(usqr, objvel[0],objvel[1],objvel[2], mMaxVlen, mMxvx,mMxvy,mMxvz); 								
						if(usqr>maxusqr) { 									
						// cutoff at maxVelVal 									
						for(int jj=0; jj<3; jj++) { 										
						if(objvel[jj]>0.) objvel[jj] =  maxVelVal;  										
						if(objvel[jj]<0.) objvel[jj] = -maxVelVal; 									
						} 								
						} 
						} 								
						const LbmFloat dp=dot(objvel, vec2L((*pNormals)[n]) ); 								
						const LbmVec oldov=objvel; // debug								
						objvel = vec2L((*pNormals)[n]) *dp;								
						*/
					}
				}
			}

			if(sds->coll_group)
				go = go->next;
			else
				base= base->next;
		}
	}
}
void smokeModifier_do(SmokeModifierData *smd, Scene *scene, Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{	
	if((smd->type & MOD_SMOKE_TYPE_FLOW))
	{
		if(scene->r.cfra >= smd->time)
			smokeModifier_init(smd, ob, scene, dm);

		if(scene->r.cfra > smd->time)
		{
			// XXX TODO
			smd->time = scene->r.cfra;

			// rigid movement support
			/*
			Mat4CpyMat4(smd->flow->mat_old, smd->flow->mat);
			Mat4CpyMat4(smd->flow->mat, ob->obmat);
			*/
		}
		else if(scene->r.cfra < smd->time)
		{
			smd->time = scene->r.cfra;
			smokeModifier_reset(smd);
		}
	}
	else if(smd->type & MOD_SMOKE_TYPE_COLL)
	{
		if(scene->r.cfra >= smd->time)
			smokeModifier_init(smd, ob, scene, dm);

		if(scene->r.cfra > smd->time)
		{
			// XXX TODO
			smd->time = scene->r.cfra;
			
			if(smd->coll->dm)
				smd->coll->dm->release(smd->coll->dm);

			smd->coll->dm = CDDM_copy(dm);

			// rigid movement support
			Mat4CpyMat4(smd->coll->mat_old, smd->coll->mat);
			Mat4CpyMat4(smd->coll->mat, ob->obmat);
		}
		else if(scene->r.cfra < smd->time)
		{
			smd->time = scene->r.cfra;
			smokeModifier_reset(smd);
		}
	}
	else if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
	{
		SmokeDomainSettings *sds = smd->domain;
		float light[3];	
		PointCache *cache = NULL;
		PTCacheID pid;
		PointCache *cache_wt = NULL;
		PTCacheID pid_wt;
		int startframe, endframe, framenr;
		float timescale;
		int cache_result = 0, cache_result_wt = 0;

		framenr = scene->r.cfra;

		// printf("time: %d\n", scene->r.cfra);

		if(framenr == smd->time)
			return;

		cache = sds->point_cache[0];
		BKE_ptcache_id_from_smoke(&pid, ob, smd);
		BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, &timescale);

		cache_wt = sds->point_cache[1];
		BKE_ptcache_id_from_smoke_turbulence(&pid_wt, ob, smd);

		if(!smd->domain->fluid)
		{
			BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
			BKE_ptcache_id_reset(scene, &pid_wt, PTCACHE_RESET_OUTDATED);
		}

		if(framenr < startframe)
			return;

		if(framenr > endframe)
			return;

		if(!smd->domain->fluid && (framenr != startframe))
			return;

		// printf("startframe: %d, framenr: %d\n", startframe, framenr);

		if(!smokeModifier_init(smd, ob, scene, dm))
		{
			printf("bad smokeModifier_init\n");
			return;
		}

		/* try to read from cache */
		cache_result =  BKE_ptcache_read_cache(&pid, (float)framenr, scene->r.frs_sec);
		// printf("cache_result: %d\n", cache_result);

		if(cache_result == PTCACHE_READ_EXACT) 
		{
			cache->flag |= PTCACHE_SIMULATION_VALID;
			cache->simframe= framenr;

			if(sds->wt)
			{
				cache_result_wt = BKE_ptcache_read_cache(&pid_wt, (float)framenr, scene->r.frs_sec);
				
				if(cache_result_wt == PTCACHE_READ_EXACT) 
				{
					cache_wt->flag |= PTCACHE_SIMULATION_VALID;
					cache_wt->simframe= framenr;
				}
			}
			return;
		}

		tstart();

		smoke_calc_domain(scene, ob, smd);
		
		// set new time
		smd->time = scene->r.cfra;

		/* do simulation */

		// low res
		cache->flag |= PTCACHE_SIMULATION_VALID;
		cache->simframe= framenr;

		// simulate the actual smoke (c++ code in intern/smoke)
		// DG: interesting commenting this line + deactivating loading of noise files
		if(framenr!=startframe)
		{
			if(sds->flags & MOD_SMOKE_DISSOLVE)
				smoke_dissolve(sds->fluid, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
			smoke_step(sds->fluid, smd->time);
		}

		// create shadows before writing cache so we get nice shadows for sstartframe, too
		if(get_lamp(scene, light))
			smoke_calc_transparency(sds->shadow, smoke_get_density(sds->fluid), sds->p0, sds->p1, sds->res, sds->dx, light, calc_voxel_transp, -7.0*sds->dx);
	
		BKE_ptcache_write_cache(&pid, framenr);

		if(sds->wt)
		{
			if(framenr!=startframe)
			{
				if(sds->flags & MOD_SMOKE_DISSOLVE)
					smoke_dissolve_wavelet(sds->wt, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
				smoke_turbulence_step(sds->wt, sds->fluid);
			}

			cache_wt->flag |= PTCACHE_SIMULATION_VALID;
			cache_wt->simframe= framenr;
			BKE_ptcache_write_cache(&pid_wt, framenr);
		}

		tend();
		printf ( "Frame: %d, Time: %f\n", (int)smd->time, ( float ) tval() );
	}
}

static float calc_voxel_transp(float *result, float *input, int res[3], int *pixel, float *tRay, float correct)
{
	const size_t index = smoke_get_index(pixel[0], res[0], pixel[1], res[1], pixel[2]);

	// T_ray *= T_vox
	*tRay *= exp(input[index]*correct);
	
	if(result[index] < 0.0f)	
	{
#pragma omp critical		
		result[index] = *tRay;	
	}	

	return *tRay;
}

long long smoke_get_mem_req(int xres, int yres, int zres, int amplify)
{
	  int totalCells = xres * yres * zres;
	  int amplifiedCells = totalCells * amplify * amplify * amplify;

	  // print out memory requirements
	  long long int coarseSize = sizeof(float) * totalCells * 22 +
	                   sizeof(unsigned char) * totalCells;

	  long long int fineSize = sizeof(float) * amplifiedCells * 7 + // big grids
	                 sizeof(float) * totalCells * 8 +     // small grids
	                 sizeof(float) * 128 * 128 * 128;     // noise tile

	  long long int totalMB = (coarseSize + fineSize) / (1024 * 1024);

	  return totalMB;
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
        	if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
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
    } else if ((m >= l) && (m >= n)) {
        err_1 = dx2 - m;
        err_2 = dz2 - m;
        for (i = 0; i < m; i++) {
        	if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
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
    } else {
        err_1 = dy2 - n;
        err_2 = dx2 - n;
        for (i = 0; i < n; i++) {
        	if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
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

static void get_cell(float *p0, int res[3], float dx, float *pos, int *cell, int correct)
{
	float tmp[3];

	VECSUB(tmp, pos, p0);
	VecMulf(tmp, 1.0 / dx);

	if(correct)
	{
		cell[0] = MIN2(res[0] - 1, MAX2(0, (int)floor(tmp[0])));
		cell[1] = MIN2(res[1] - 1, MAX2(0, (int)floor(tmp[1])));
		cell[2] = MIN2(res[2] - 1, MAX2(0, (int)floor(tmp[2])));
	}
	else
	{
		cell[0] = (int)floor(tmp[0]);
		cell[1] = (int)floor(tmp[1]);
		cell[2] = (int)floor(tmp[2]);
	}
}

static void smoke_calc_transparency(float *result, float *input, float *p0, float *p1, int res[3], float dx, float *light, bresenham_callback cb, float correct)
{
	int x, y, z;
	float bv[6];

	memset(result, -1, sizeof(float)*res[0]*res[1]*res[2]);	// x
	bv[0] = p0[0];
	bv[1] = p1[0];
	// y
	bv[2] = p0[1];
	bv[3] = p1[1];
	// z
	bv[4] = p0[2];
	bv[5] = p1[2];

#pragma omp parallel for schedule(static) private(y, z)
	for(x = 0; x < res[0]; x++)
		for(y = 0; y < res[1]; y++)
			for(z = 0; z < res[2]; z++)
			{
				float voxelCenter[3];
				size_t index;
				float pos[3];
				int cell[3];
				float tRay = 1.0;

				index = smoke_get_index(x, res[0], y, res[1], z);

				if(result[index] >= 0.0f)					
					continue;								
				voxelCenter[0] = p0[0] + dx *  x + dx * 0.5;
				voxelCenter[1] = p0[1] + dx *  y + dx * 0.5;
				voxelCenter[2] = p0[2] + dx *  z + dx * 0.5;

				// get starting position (in voxel coords)
				if(BLI_bvhtree_bb_raycast(bv, light, voxelCenter, pos) > FLT_EPSILON)
				{
					// we're ouside
					get_cell(p0, res, dx, pos, cell, 1);
				}
				else
				{
					// we're inside
					get_cell(p0, res, dx, light, cell, 1);
				}

				bresenham_linie_3D(cell[0], cell[1], cell[2], x, y, z, &tRay, cb, result, input, res, correct);

				// convention -> from a RGBA float array, use G value for tRay
// #pragma omp critical
				result[index] = tRay;			
			}
}

