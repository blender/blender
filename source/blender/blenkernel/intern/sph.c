/*  pw.c
*
*
* ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version. The Blender
* Foundation also sells licenses for use in proprietary software under
* the Blender License.  See http://www.blender.org/BL/ for information
* about this.
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
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* Contributor(s): Daniel Genrich
*
* ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <malloc.h>

#include "MEM_guardedalloc.h"

#include "BKE_sph.h"

#include "DNA_sph_types.h"

#include "sph_extern.h"

#include "DNA_effect_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"


// necessary
#include "float.h"
#include "math.h"
#include "BLI_kdtree.h"
#include "BLI_arithb.h"

// #include "omp.h"

#ifdef _WIN32
void ststart ( void )
{}
void stend ( void )
{
}
double stval()
{
	return 0;
}
#else
#include <sys/time.h>

static struct timeval _ststart, _stend;
static struct timezone stz;

void ststart(void)
{
	gettimeofday(&_ststart, &stz);
}
void stend(void)
{
	gettimeofday(&_stend,&stz);
}
double stval()
{
	double t1, t2;
	t1 =  (double)_ststart.tv_sec + (double)_ststart.tv_usec/(1000*1000);
	t2 =  (double)_stend.tv_sec + (double)_stend.tv_usec/(1000*1000);
	return t2-t1;
}
#endif 

void sph_init (SphModifierData *sphmd)
{	
	/* fill modifier with standard settings */
	sphmd->sim_parms->timestep = 0.001; // 0.001
	sphmd->sim_parms->viscosity = 80000.0;
	sphmd->sim_parms->incompressibility = 8000000.0;
	sphmd->sim_parms->surfacetension = 8000.0;
	sphmd->sim_parms->density = 1000.0;
	sphmd->sim_parms->gravity[2] = -9.81;
	sphmd->sim_parms->gravity[1] = 0.0;
	sphmd->sim_parms->gravity[0] = 0.0;
	sphmd->sim_parms->samplingdistance = 0.02; // length of one cell? 0.01
	sphmd->sim_parms->smoothinglength = 2.5;
	sphmd->sim_parms->flags = SPH_SIMSETTINGS_FLAG_GHOSTS | SPH_SIMSETTINGS_FLAG_OFFLINE | SPH_SIMSETTINGS_FLAG_MULTIRES | SPH_SIMSETTINGS_FLAG_DOMAIN;
	sphmd->sim_parms->computesurfaceevery = 5; // 30000000
	sphmd->sim_parms->fastmarchingevery = 5;
	sphmd->sim_parms->dumppovrayevery = 300000;
	sphmd->sim_parms->dumpimageevery = 30; 
	sphmd->sim_parms->totaltime = 0.01; // 40.0
	sphmd->sim_parms->tangentialfriction = 0.1;
	sphmd->sim_parms->normalfriction = 0.95;
	sphmd->sim_parms->initiallevel = 1;
	sphmd->sim_parms->rotation_angle = 0.0;
	
	sphmd->sim_parms->rotation_axis[0] = 1.0;
	sphmd->sim_parms->rotation_axis[1] = 1.0;
	sphmd->sim_parms->rotation_axis[2] = 1.0;
	
	sphmd->sim_parms->rotation_center[0] = 0.0;
	sphmd->sim_parms->rotation_center[1] = 0.0;
	sphmd->sim_parms->rotation_center[2] = 0.0;

	sphmd->sim_parms->scenelowerbound[0] = -1.0;
	sphmd->sim_parms->scenelowerbound[1] = -1.0;
	sphmd->sim_parms->scenelowerbound[2] = -1.0;
	
	sphmd->sim_parms->sceneupperbound[0] = 1.0;
	sphmd->sim_parms->sceneupperbound[1] = 1.0;
	sphmd->sim_parms->sceneupperbound[2] = 1.0;

	sphmd->sim_parms->alpha = 2.0;
	sphmd->sim_parms->beta = 3.0;
	sphmd->sim_parms->gamma = 1.5;
	
	sphmd->sim_parms->numverts = 0;
	sphmd->sim_parms->numtris = 0;
	sphmd->sim_parms->verts = NULL;
	sphmd->sim_parms->tris = NULL;
	sphmd->sim_parms->normals = NULL;
	
	sphmd->sim_parms->resolution = 70;
	
	sphmd->sim_parms->co = NULL;
	sphmd->sim_parms->r = NULL;
	sphmd->sim_parms->numpart = 0;
}

void sph_free_modifier (SphModifierData *sphmd)
{
	
	// sph_free_cpp(sphmd);
	
	if(sphmd->sim_parms->verts)
		free(sphmd->sim_parms->verts);
	if(sphmd->sim_parms->tris)
		free(sphmd->sim_parms->tris);
	if(sphmd->sim_parms->normals)
		free(sphmd->sim_parms->normals);
	if(sphmd->sim_parms->co)
		MEM_freeN(sphmd->sim_parms->co);
	if(sphmd->sim_parms->r)
		free(sphmd->sim_parms->r);
	
}


DerivedMesh *sphModifier_do(SphModifierData *sphmd,Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{
	SphSimSettings *sim_parms = sphmd->sim_parms;
	DerivedMesh *result = NULL;
	MVert *mvert = NULL;
	MFace *mface = NULL;
	int a = 0;
	float mat[4][4], imat[4][4];
	Mat4CpyMat4(mat, ob->obmat);
	Mat4Invert(imat, mat);
	
	// only domain is simulated
	if(!(sim_parms->flags & SPH_SIMSETTINGS_FLAG_DOMAIN) && !(sim_parms->flags & SPH_SIMSETTINGS_FLAG_BAKING))
	{
		return dm;
	}
	
	ststart();
	
	if(!(sim_parms->flags & SPH_SIMSETTINGS_FLAG_INIT))
	{
		if(!sph_init_all (sphmd, dm, ob))
		{
			sphmd->sim_parms->flags &= ~SPH_SIMSETTINGS_FLAG_INIT;
			return dm;
		}
	}

	
	
	// sph_simulate_cpp(ob, sphmd, 1.0, NULL);
	
	stend();
	
	printf ( "SPH simulation time: %f\n", ( float ) stval() );
	
	if(sim_parms->numverts && sim_parms->numtris)
	{
		
		
		result = CDDM_new ( sim_parms->numverts, 0, sim_parms->numtris);
		
		// copy verts
		mvert = CDDM_get_verts(result);
		for(a=0; a<sim_parms->numverts; a++) {
			MVert *mv = &mvert[a];
			float *vbCo = &sim_parms->verts[a*3];
			VECCOPY(mv->co, vbCo);
			Mat4MulVecfl(imat, mv->co);
		}
		
		mface = CDDM_get_faces(result);
		for(a=0; a<sim_parms->numtris; a++) {
			MFace *mf = &mface[a];
			int *tri = &sim_parms->tris[a*3];
			mf->v1 = tri[0];
			mf->v2 = tri[1];
			mf->v3 = tri[2];
			test_index_face(mf, NULL, 0, 3);
		}
		
		CDDM_calc_edges ( result );
		CDDM_calc_normals ( result );
		
		return result;
	}
	else 
		return dm;
}

static void set_min_max(float *min, float *max, float *co)
{
	// also calc min + max of bounding box for 3d grid
	min[0] = MIN2(min[0], co[0]);
	min[1] = MIN2(min[1], co[1]);
	min[2] = MIN2(min[2], co[2]);
		
	max[0] = MAX2(max[0], co[0]);
	max[1] = MAX2(max[1], co[1]);
	max[2] = MAX2(max[2], co[2]);
}

long calc_distance_field(Object *ob, DerivedMesh *dm, SphModifierData *sphmd, float mat[4][4])
{
	int numverts = dm->getNumVerts(dm);
	int numfaces = dm->getNumFaces(dm);
	MVert *mvert = dm->getVertArray(dm);
	MFace *mface = dm->getFaceArray(dm);
	int i, j, k;
	KDTree *tree;
	float co[3], co1[3], co2[3], co3[3], co4[3];
	int index;
	float min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX}, slice, maxdir;
	int resx, resy, resz;
	int maxres = 20;
	float *dist;
	float *normals;
	int totpart = 0;
	float *cos = NULL;
	int maxpart = 0;
	
	printf("calc_distance_field\n");
	
	dist = MEM_callocN(maxres*maxres*maxres*sizeof(float), "distance_field");
	normals = MEM_callocN(numfaces*3*sizeof(float), "triangle_normals");
	
	/////////////////////////////////////////////////
	// create + fill + balance kdtree
	/////////////////////////////////////////////////
	tree = BLI_kdtree_new(numverts);
	for(i = 0; i < numfaces; i++)
	{
		VECCOPY(co1, mvert[mface[i].v1].co);
		Mat4MulVecfl(mat, co1);
		set_min_max(min, max, co1);
		VECCOPY(co2, mvert[mface[i].v2].co);
		Mat4MulVecfl(mat, co2);
		set_min_max(min, max, co2);
		VECCOPY(co3, mvert[mface[i].v3].co);
		Mat4MulVecfl(mat, co3);
		set_min_max(min, max, co3);
		
		// calc triangle center
		VECCOPY(co, co1);
		VECADD(co, co, co2);
		VECADD(co, co, co3);
		if(mface[i].v4)
		{
			VECCOPY(co4, mvert[mface[i].v4].co);
			Mat4MulVecfl(mat, co4);
			set_min_max(min, max, co4);
			VECADD(co, co, co4);
			VecMulf(co, 0.25);
		}
		else
			VecMulf(co, 1.0 / 3.0);
		
		if(mface[i].v4)
			CalcNormFloat4(mvert[mface[i].v1].co, mvert[mface[i].v2].co, mvert[mface[i].v3].co, mvert[mface[i].v4].co, &normals[i*3]);
		else
			CalcNormFloat(mvert[mface[i].v1].co, mvert[mface[i].v2].co, mvert[mface[i].v3].co, &normals[i*3]);
		
		BLI_kdtree_insert(tree, i, co, NULL);
	}
	BLI_kdtree_balance(tree);
	/////////////////////////////////////////////////
	
	// calculate slice height + width
	maxdir = max[0] - min[0];
	maxdir = MAX2(max[1]-min[1], maxdir);
	maxdir = MAX2(max[2]-min[2], maxdir);
	slice = maxdir / (float)maxres;
	resx = MIN2(maxres, ceil((max[0] - min[0]) / slice));
	resy = MIN2(maxres, ceil((max[1] - min[1]) / slice));
	resz = MIN2(maxres, ceil((max[2] - min[2]) / slice));
	
	// adjust max
	max[0] = min[0] + slice * resx;
	max[1] = min[1] + slice * resy;
	max[2] = min[2] + slice * resz;
	
	if(sphmd->sim_parms->co)
		MEM_freeN(sphmd->sim_parms->co);
	
	cos = sphmd->sim_parms->co = MEM_callocN(sizeof(float)*3*resx*resy*resz, "sph_cos");
	// r = calloc(1, sizeof(float)*resx*resy*resz);
	maxpart = sizeof(float)*3*resx*resy*resz;
	
// #pragma omp parallel for private(i,j,k) schedule(static)
	for(i = 0; i < resx; i ++)
	{
		for(j = 0; j < resy; j++)
		{
			for(k = 0; k < resz; k++)
			{
				KDTreeNearest nearest;
				float tco[3];
				tco[0] = min[0] + slice * i + slice * 0.5;
				tco[1] = min[1] + slice * j + slice * 0.5;
				tco[2] = min[2] + slice * k + slice * 0.5;
				
				index = BLI_kdtree_find_nearest(tree, tco, NULL, &nearest);
				
				if(index != -1)
				{
					float t[3];
					float sgn;
					
					VECSUB(t, tco, nearest.co);
					sgn = INPR(t, &normals[nearest.index*3]);
					
					if(sgn < 0.0)
						sgn = -1.0;
					else 
						sgn = 1.0;
					
					dist[(i*resy*resz)+(j*resz)+k] = sgn * nearest.dist;
					
					if((int)sgn < 0)
					{	
						// create particle if inside object
						VECCOPY(&cos[totpart*3], tco);
						
						totpart++;	
					}
					
				}
				else
				{
					printf("Error: no nearest point!\n");
				}
			}
		}
	}
	printf("maxpart: %d, totpart: %d\n", maxpart, totpart);
	
	sphmd->sim_parms->numpart = totpart;
	MEM_freeN(dist);
	MEM_freeN(normals);
	BLI_kdtree_free(tree);
	return totpart;
}

/* add constraints, inflows, fluid, ... */
int sph_init_all (SphModifierData *sphmd, DerivedMesh *dm, Object *ob)
{
	Base *base=NULL;
	Object *fobject = NULL;
	SphModifierData *fsphmd = NULL;
	int fluids = 0; // only one fluid object possible
	DerivedMesh *fdm = NULL;
	
	sphmd->sim_parms->flags |= SPH_SIMSETTINGS_FLAG_INIT;
	CDDM_calc_normals ( dm );
	
	/* create C++ object */
	// sph_init_cpp(sphmd);
	sphmd->sim_parms->numpart = calc_distance_field(ob, dm, sphmd, ob->obmat);
	return 1;
	
	/* create fluid domain */
	// sph_set_domain(sphmd, dm, ob->obmat);
	
	// check for constraints, fluid, etc. but ignore domains
	for ( base = G.scene->base.first; base; base = base->next )
	{
		fobject = base->object;
		fsphmd = ( SphModifierData * ) modifiers_findByType ( fobject, eModifierType_Sph );
		
		// TODO I could check for linked groups, too 
		if ( !fsphmd )
		{
			// TODO
		}
		else
		{	
			if(fsphmd == sphmd)
				continue;
			
			if(fsphmd->sim_parms)
			{	
				// check for fluid
				if(((short)fsphmd->sim_parms->flags == SPH_SIMSETTINGS_FLAG_FLUID)  && (!fluids))
				{
					// create fluids
					// particles have to be created AFTER constraints
					// TODO: no particles = crash
					
					// get derivedmesh from object
					fdm = mesh_get_derived_final(fobject, CD_MASK_BAREMESH);
					
					// create fluid object
					/*
					if(!sph_add_particles(sphmd, fdm, fobject->obmat, fsphmd->sim_parms->resolution))
					{
						fluids--;
					}
					*/
					if(fdm)
						fdm->release(fdm);
					
					fluids++;
				}
				else if((short)fsphmd->sim_parms->flags & SPH_SIMSETTINGS_FLAG_OBSTACLE)
				{
					
				}
			}
		}
	}
	
	if(!fluids)
		return 0;
		
	return 1;
}























