/*  softbody.c      
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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
******
variables on the UI for now

	float mediafrict;  friction to env 
	float nodemass;	  softbody mass of *vertex* 
	float grav;        softbody amount of gravitaion to apply 
	
	float goalspring;  softbody goal springs 
	float goalfrict;   softbody goal springs friction 
	float mingoal;     quick limits for goal 
	float maxgoal;

	float inspring;	  softbody inner springs 
	float infrict;     softbody inner springs friction 

*****
*/


#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

/* types */
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"	/* here is the softbody struct */
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "BKE_curve.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "BKE_pointcache.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"
//XXX #include  "BIF_editdeform.h"
//XXX #include  "BIF_graphics.h"
#include  "PIL_time.h"
// #include  "ONL_opennl.h" remove linking to ONL for now

/* callbacks for errors and interrupts and some goo */
static int (*SB_localInterruptCallBack)(void) = NULL;


/* ********** soft body engine ******* */

typedef	enum {SB_EDGE=1,SB_BEND=2,SB_STIFFQUAD=3} type_spring;

typedef struct BodySpring {
	int v1, v2;
	float len,cf,load;
	float ext_force[3]; /* edges colliding and sailing */
	type_spring springtype;
	short flag;
} BodySpring;

typedef struct BodyFace {
	int v1, v2, v3 ,v4;
	float ext_force[3]; /* faces colliding */
	short flag;
} BodyFace;

typedef struct ReferenceVert {
	float pos[3]; /* position relative to com */
	float mass;   /* node mass */
} ReferenceVert;

typedef struct ReferenceState {
	float com[3]; /* center of mass*/
	ReferenceVert *ivert; /* list of intial values */
}ReferenceState ;


/*private scratch pad for caching and other data only needed when alive*/
typedef struct SBScratch {
	GHash *colliderhash;
	short needstobuildcollider;
	short flag;
	BodyFace *bodyface;
	int totface;
	float aabbmin[3],aabbmax[3];
	ReferenceState Ref;
}SBScratch;

typedef struct  SB_thread_context {
		Scene *scene;
        Object *ob;
		float forcetime;
		float timenow;
		int ifirst;
		int ilast;
		ListBase *do_effector;
		int do_deflector;
		float fieldfactor;
		float windfactor;
		int nr;
		int tot;
}SB_thread_context;

#define NLF_BUILD  1 
#define NLF_SOLVE  2 

#define MID_PRESERVE 1

#define SOFTGOALSNAP  0.999f 
/* if bp-> goal is above make it a *forced follow original* and skip all ODE stuff for this bp
   removes *unnecessary* stiffnes from ODE system
*/
#define HEUNWARNLIMIT 1 /* 500 would be fine i think for detecting severe *stiff* stuff */


#define BSF_INTERSECT   1 /* edge intersects collider face */
#define SBF_DOFUZZY     1 /* edge intersects collider face */

#define BFF_INTERSECT   1 /* collider edge   intrudes face */
#define BFF_CLOSEVERT   2 /* collider vertex repulses face */


float SoftHeunTol = 1.0f; /* humm .. this should be calculated from sb parameters and sizes */

/* local prototypes */
static void free_softbody_intern(SoftBody *sb);
/* aye this belongs to arith.c */
static void Vec3PlusStVec(float *v, float s, float *v1);

/*+++ frame based timing +++*/

/*physical unit of force is [kg * m / sec^2]*/

static float sb_grav_force_scale(Object *ob)
/* since unit of g is [m/sec^2] and F = mass * g we rescale unit mass of node to 1 gramm
  put it to a function here, so we can add user options later without touching simulation code
*/
{
	return (0.001f);
}

static float sb_fric_force_scale(Object *ob)
/* rescaling unit of drag [1 / sec] to somehow reasonable
  put it to a function here, so we can add user options later without touching simulation code
*/
{
	return (0.01f);
}

static float sb_time_scale(Object *ob)
/* defining the frames to *real* time relation */
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	if (sb){
		return(sb->physics_speed); 
		/*hrms .. this could be IPO as well :) 
		 estimated range [0.001 sluggish slug - 100.0 very fast (i hope ODE solver can handle that)]
		 1 approx = a unit 1 pendulum at g = 9.8 [earth conditions]  has period 65 frames
         theory would give a 50 frames period .. so there must be something inaccurate .. looking for that (BM) 
		 */
	}
	return (1.0f);
	/* 
	this would be frames/sec independant timing assuming 25 fps is default
	but does not work very well with NLA
		return (25.0f/scene->r.frs_sec)
	*/
}
/*--- frame based timing ---*/

/*+++ collider caching and dicing +++*/

/********************
for each target object/face the axis aligned bounding box (AABB) is stored
faces paralell to global axes 
so only simple "value" in [min,max] ckecks are used
float operations still
*/

/* just an ID here to reduce the prob for killing objects
** ob->sumohandle points to we should not kill :)
*/ 
const int CCD_SAVETY = 190561; 

typedef struct ccdf_minmax{
float minx,miny,minz,maxx,maxy,maxz;
}ccdf_minmax;



typedef struct ccd_Mesh {
	int totvert, totface;
	MVert *mvert;
	MVert *mprevvert;
	MFace *mface;
	int savety;
	ccdf_minmax *mima;
	/* Axis Aligned Bounding Box AABB */
	float bbmin[3];
	float bbmax[3];
}ccd_Mesh;




static ccd_Mesh *ccd_mesh_make(Object *ob, DerivedMesh *dm)
{
    ccd_Mesh *pccd_M = NULL;
	ccdf_minmax *mima =NULL;
	MFace *mface=NULL;
	float v[3],hull;
	int i;
	
	/* first some paranoia checks */
	if (!dm) return NULL;
	if (!dm->getNumVerts(dm) || !dm->getNumFaces(dm)) return NULL;
	
	pccd_M = MEM_mallocN(sizeof(ccd_Mesh),"ccd_Mesh");
	pccd_M->totvert = dm->getNumVerts(dm);
	pccd_M->totface = dm->getNumFaces(dm);
	pccd_M->savety  = CCD_SAVETY;
	pccd_M->bbmin[0]=pccd_M->bbmin[1]=pccd_M->bbmin[2]=1e30f;
	pccd_M->bbmax[0]=pccd_M->bbmax[1]=pccd_M->bbmax[2]=-1e30f;
	pccd_M->mprevvert=NULL;
	
	
    /* blow it up with forcefield ranges */
	hull = MAX2(ob->pd->pdef_sbift,ob->pd->pdef_sboft);
	
	/* alloc and copy verts*/
	pccd_M->mvert = dm->dupVertArray(dm);
    /* ah yeah, put the verices to global coords once */ 	
	/* and determine the ortho BB on the fly */ 
	for(i=0; i < pccd_M->totvert; i++){
		mul_m4_v3(ob->obmat, pccd_M->mvert[i].co);
		
        /* evaluate limits */
		VECCOPY(v,pccd_M->mvert[i].co);
		pccd_M->bbmin[0] = MIN2(pccd_M->bbmin[0],v[0]-hull);
		pccd_M->bbmin[1] = MIN2(pccd_M->bbmin[1],v[1]-hull);
		pccd_M->bbmin[2] = MIN2(pccd_M->bbmin[2],v[2]-hull);
		
		pccd_M->bbmax[0] = MAX2(pccd_M->bbmax[0],v[0]+hull);
		pccd_M->bbmax[1] = MAX2(pccd_M->bbmax[1],v[1]+hull);
		pccd_M->bbmax[2] = MAX2(pccd_M->bbmax[2],v[2]+hull);
		
	}
	/* alloc and copy faces*/
    pccd_M->mface = dm->dupFaceArray(dm);
	
	/* OBBs for idea1 */
    pccd_M->mima = MEM_mallocN(sizeof(ccdf_minmax)*pccd_M->totface,"ccd_Mesh_Faces_mima");
	mima  = pccd_M->mima;
	mface = pccd_M->mface;


	/* anyhoo we need to walk the list of faces and find OBB they live in */
	for(i=0; i < pccd_M->totface; i++){
		mima->minx=mima->miny=mima->minz=1e30f;
		mima->maxx=mima->maxy=mima->maxz=-1e30f;
		
        VECCOPY(v,pccd_M->mvert[mface->v1].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
        VECCOPY(v,pccd_M->mvert[mface->v2].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
		VECCOPY(v,pccd_M->mvert[mface->v3].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
        
		if(mface->v4){
			VECCOPY(v,pccd_M->mvert[mface->v4].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		}

		
	mima++;
	mface++;
		
	}
	return pccd_M;
}
static void ccd_mesh_update(Object *ob,ccd_Mesh *pccd_M, DerivedMesh *dm)
{
 	ccdf_minmax *mima =NULL;
	MFace *mface=NULL;
	float v[3],hull;
	int i;
	
	/* first some paranoia checks */
	if (!dm) return ;
	if (!dm->getNumVerts(dm) || !dm->getNumFaces(dm)) return ;

	if ((pccd_M->totvert != dm->getNumVerts(dm)) ||
		(pccd_M->totface != dm->getNumFaces(dm))) return;

	pccd_M->bbmin[0]=pccd_M->bbmin[1]=pccd_M->bbmin[2]=1e30f;
	pccd_M->bbmax[0]=pccd_M->bbmax[1]=pccd_M->bbmax[2]=-1e30f;
	
	
    /* blow it up with forcefield ranges */
	hull = MAX2(ob->pd->pdef_sbift,ob->pd->pdef_sboft);
	
	/* rotate current to previous */
	if(pccd_M->mprevvert) MEM_freeN(pccd_M->mprevvert);
    pccd_M->mprevvert = pccd_M->mvert;
	/* alloc and copy verts*/
    pccd_M->mvert = dm->dupVertArray(dm);
    /* ah yeah, put the verices to global coords once */ 	
	/* and determine the ortho BB on the fly */ 
	for(i=0; i < pccd_M->totvert; i++){
		mul_m4_v3(ob->obmat, pccd_M->mvert[i].co);
		
        /* evaluate limits */
		VECCOPY(v,pccd_M->mvert[i].co);
		pccd_M->bbmin[0] = MIN2(pccd_M->bbmin[0],v[0]-hull);
		pccd_M->bbmin[1] = MIN2(pccd_M->bbmin[1],v[1]-hull);
		pccd_M->bbmin[2] = MIN2(pccd_M->bbmin[2],v[2]-hull);
		
		pccd_M->bbmax[0] = MAX2(pccd_M->bbmax[0],v[0]+hull);
		pccd_M->bbmax[1] = MAX2(pccd_M->bbmax[1],v[1]+hull);
		pccd_M->bbmax[2] = MAX2(pccd_M->bbmax[2],v[2]+hull);

        /* evaluate limits */
		VECCOPY(v,pccd_M->mprevvert[i].co);
		pccd_M->bbmin[0] = MIN2(pccd_M->bbmin[0],v[0]-hull);
		pccd_M->bbmin[1] = MIN2(pccd_M->bbmin[1],v[1]-hull);
		pccd_M->bbmin[2] = MIN2(pccd_M->bbmin[2],v[2]-hull);
		
		pccd_M->bbmax[0] = MAX2(pccd_M->bbmax[0],v[0]+hull);
		pccd_M->bbmax[1] = MAX2(pccd_M->bbmax[1],v[1]+hull);
		pccd_M->bbmax[2] = MAX2(pccd_M->bbmax[2],v[2]+hull);
		
	}
	
	mima  = pccd_M->mima;
	mface = pccd_M->mface;


	/* anyhoo we need to walk the list of faces and find OBB they live in */
	for(i=0; i < pccd_M->totface; i++){
		mima->minx=mima->miny=mima->minz=1e30f;
		mima->maxx=mima->maxy=mima->maxz=-1e30f;
		
        VECCOPY(v,pccd_M->mvert[mface->v1].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
        VECCOPY(v,pccd_M->mvert[mface->v2].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
		VECCOPY(v,pccd_M->mvert[mface->v3].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
        
		if(mface->v4){
			VECCOPY(v,pccd_M->mvert[mface->v4].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		}


        VECCOPY(v,pccd_M->mprevvert[mface->v1].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
        VECCOPY(v,pccd_M->mprevvert[mface->v2].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		
		VECCOPY(v,pccd_M->mprevvert[mface->v3].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
        
		if(mface->v4){
			VECCOPY(v,pccd_M->mprevvert[mface->v4].co);
		mima->minx = MIN2(mima->minx,v[0]-hull);
		mima->miny = MIN2(mima->miny,v[1]-hull);
		mima->minz = MIN2(mima->minz,v[2]-hull);
		mima->maxx = MAX2(mima->maxx,v[0]+hull);
		mima->maxy = MAX2(mima->maxy,v[1]+hull);
		mima->maxz = MAX2(mima->maxz,v[2]+hull);
		}

		
	mima++;
	mface++;
		
	}
	return ;
}

static void ccd_mesh_free(ccd_Mesh *ccdm)
{
	if(ccdm && (ccdm->savety == CCD_SAVETY )){ /*make sure we're not nuking objects we don't know*/
		MEM_freeN(ccdm->mface);
		MEM_freeN(ccdm->mvert);
		if (ccdm->mprevvert) MEM_freeN(ccdm->mprevvert);
		MEM_freeN(ccdm->mima);
		MEM_freeN(ccdm);
		ccdm = NULL;
	}
}

static void ccd_build_deflector_hash(Scene *scene, Object *vertexowner, GHash *hash)
{
	Base *base= scene->base.first;
	Object *ob;

	if (!hash) return;
	while (base) {
		/*Only proceed for mesh object in same layer */
		if(base->object->type==OB_MESH && (base->lay & vertexowner->lay)) {
			ob= base->object;
			if((vertexowner) && (ob == vertexowner)) {
				/* if vertexowner is given  we don't want to check collision with owner object */ 
				base = base->next;
				continue;
			}

			/*+++ only with deflecting set */
			if(ob->pd && ob->pd->deflect && BLI_ghash_lookup(hash, ob) == 0) {
				DerivedMesh *dm= NULL;

				if(ob->softflag & OB_SB_COLLFINAL) /* so maybe someone wants overkill to collide with subsurfed */
					dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
				else
					dm = mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);

				if(dm){
					ccd_Mesh *ccdmesh = ccd_mesh_make(ob, dm);
					BLI_ghash_insert(hash, ob, ccdmesh);

					/* we did copy & modify all we need so give 'em away again */
					dm->release(dm);
					
				}
			}/*--- only with deflecting set */

		}/* mesh && layer*/		
	   base = base->next;
	} /* while (base) */
}

static void ccd_update_deflector_hash(Scene *scene, Object *vertexowner, GHash *hash)
{
	Base *base= scene->base.first;
	Object *ob;

	if ((!hash) || (!vertexowner)) return;
	while (base) {
		/*Only proceed for mesh object in same layer */
		if(base->object->type==OB_MESH && (base->lay & vertexowner->lay)) {
			ob= base->object;
			if(ob == vertexowner){ 
				/* if vertexowner is given  we don't want to check collision with owner object */ 
				base = base->next;
				continue;				
			}

			/*+++ only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				DerivedMesh *dm= NULL;
				
				if(ob->softflag & OB_SB_COLLFINAL) { /* so maybe someone wants overkill to collide with subsurfed */
					dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
				} else {
					dm = mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);
				}
				if(dm){
					ccd_Mesh *ccdmesh = BLI_ghash_lookup(hash,ob);
					if (ccdmesh)
						ccd_mesh_update(ob,ccdmesh,dm);

					/* we did copy & modify all we need so give 'em away again */
					dm->release(dm);
				}
			}/*--- only with deflecting set */

		}/* mesh && layer*/		
	   base = base->next;
	} /* while (base) */
}




/*--- collider caching and dicing ---*/


static int count_mesh_quads(Mesh *me)
{
	int a,result = 0;
	MFace *mface= me->mface;
	
	if(mface) {
		for(a=me->totface; a>0; a--, mface++) {
			if(mface->v4) result++;
		}
	}	
	return result;
}

static void add_mesh_quad_diag_springs(Object *ob)
{
	Mesh *me= ob->data;
	MFace *mface= me->mface;
	BodyPoint *bp;
	BodySpring *bs, *bs_new;
	int a ;
	
	if (ob->soft){
		int nofquads;
		//float s_shear = ob->soft->shearstiff*ob->soft->shearstiff;
		
		nofquads = count_mesh_quads(me);
		if (nofquads) {
			/* resize spring-array to hold additional quad springs */
			bs_new= MEM_callocN( (ob->soft->totspring + nofquads *2 )*sizeof(BodySpring), "bodyspring");
			memcpy(bs_new,ob->soft->bspring,(ob->soft->totspring )*sizeof(BodySpring));
			
			if(ob->soft->bspring)
				MEM_freeN(ob->soft->bspring); /* do this before reassigning the pointer  or have a 1st class memory leak */
			ob->soft->bspring = bs_new; 
			
			/* fill the tail */
			a = 0;
			bs = bs_new+ob->soft->totspring;
			bp= ob->soft->bpoint;
			if(mface ) {
				for(a=me->totface; a>0; a--, mface++) {
					if(mface->v4) {
						bs->v1= mface->v1;
						bs->v2= mface->v3;
						bs->springtype   =SB_STIFFQUAD;
						bs++;
						bs->v1= mface->v2;
						bs->v2= mface->v4;
						bs->springtype   =SB_STIFFQUAD;
						bs++;
						
					}
				}	
			}
			
            /* now we can announce new springs */
			ob->soft->totspring += nofquads *2;
		}
	}
}

static void add_2nd_order_roller(Object *ob,float stiffness,int *counter, int addsprings)
{
	/*assume we have a softbody*/
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp,*bpo;	
	BodySpring *bs,*bs2,*bs3= NULL;
	int a,b,c,notthis= 0,v0;
	if (!sb->bspring){return;} /* we are 2nd order here so 1rst should have been build :) */
	/* first run counting  second run adding */
	*counter = 0;
	if (addsprings) bs3 = ob->soft->bspring+ob->soft->totspring;
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		/*scan for neighborhood*/
		bpo = NULL;
		v0  = (sb->totpoint-a);
		for(b=bp->nofsprings;b>0;b--){
			bs = sb->bspring + bp->springs[b-1];
			/*nasty thing here that springs have two ends
			so here we have to make sure we examine the other */
			if (( v0 == bs->v1) ){ 
				bpo =sb->bpoint+bs->v2;
				notthis = bs->v2;
			}
			else {
			if (( v0 == bs->v2) ){
				bpo =sb->bpoint+bs->v1;
				notthis = bs->v1;
			} 
			else {printf("oops we should not get here -  add_2nd_order_springs");}
			}
            if (bpo){/* so now we have a 2nd order humpdidump */
				for(c=bpo->nofsprings;c>0;c--){
					bs2 = sb->bspring + bpo->springs[c-1];
					if ((bs2->v1 != notthis)  && (bs2->v1 > v0)){
						(*counter)++;/*hit */
						if (addsprings){
							bs3->v1= v0;
							bs3->v2= bs2->v1;
						    bs3->springtype   =SB_BEND;
							bs3++;
						}
					}
					if ((bs2->v2 !=notthis)&&(bs2->v2 > v0)){
					(*counter)++;/*hit */
						if (addsprings){
							bs3->v1= v0;
							bs3->v2= bs2->v2;
						    bs3->springtype   =SB_BEND;
							bs3++;
						}

					}
				}
				
			}
			
		}
		/*scan for neighborhood done*/
	}
}


static void add_2nd_order_springs(Object *ob,float stiffness)
{
	int counter = 0;
	BodySpring *bs_new;
	stiffness *=stiffness;
	
	add_2nd_order_roller(ob,stiffness,&counter,0); /* counting */
	if (counter) {
		/* resize spring-array to hold additional springs */
		bs_new= MEM_callocN( (ob->soft->totspring + counter )*sizeof(BodySpring), "bodyspring");
		memcpy(bs_new,ob->soft->bspring,(ob->soft->totspring )*sizeof(BodySpring));
		
		if(ob->soft->bspring)
			MEM_freeN(ob->soft->bspring); 
		ob->soft->bspring = bs_new; 
		
		add_2nd_order_roller(ob,stiffness,&counter,1); /* adding */
		ob->soft->totspring +=counter ;
	}
}

static void add_bp_springlist(BodyPoint *bp,int springID)
{
	int *newlist;
	
	if (bp->springs == NULL) {
		bp->springs = MEM_callocN( sizeof(int), "bpsprings");
		bp->springs[0] = springID;
		bp->nofsprings = 1;
	}
	else {
		bp->nofsprings++;
		newlist = MEM_callocN(bp->nofsprings * sizeof(int), "bpsprings");
		memcpy(newlist,bp->springs,(bp->nofsprings-1)* sizeof(int));
		MEM_freeN(bp->springs);
		bp->springs = newlist;
		bp->springs[bp->nofsprings-1] = springID;
	}
}

/* do this once when sb is build
it is O(N^2) so scanning for springs every iteration is too expensive
*/
static void build_bps_springlist(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp;	
	BodySpring *bs;	
	int a,b;
	
	if (sb==NULL) return; /* paranoya check */
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		/* throw away old list */
		if (bp->springs) {
			MEM_freeN(bp->springs);
			bp->springs=NULL;
		}
		/* scan for attached inner springs */	
		for(b=sb->totspring, bs= sb->bspring; b>0; b--, bs++) {
			if (( (sb->totpoint-a) == bs->v1) ){ 
				add_bp_springlist(bp,sb->totspring -b);
			}
			if (( (sb->totpoint-a) == bs->v2) ){ 
				add_bp_springlist(bp,sb->totspring -b);
			}
		}/*for springs*/
	}/*for bp*/		
}

static void calculate_collision_balls(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp;	
	BodySpring *bs;	
	int a,b,akku_count;
	float min,max,akku;

	if (sb==NULL) return; /* paranoya check */

	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		bp->colball=0;
		akku =0.0f;
		akku_count=0;
		min = 1e22f;
		max = -1e22f;
		/* first estimation based on attached */
		for(b=bp->nofsprings;b>0;b--){
			bs = sb->bspring + bp->springs[b-1];
			if (bs->springtype == SB_EDGE){
			akku += bs->len;
			akku_count++,
			min = MIN2(bs->len,min);
			max = MAX2(bs->len,max);
			}
		}

		if (akku_count > 0) {
			if (sb->sbc_mode == SBC_MODE_MANUAL){
				bp->colball=sb->colball;
			}
			if (sb->sbc_mode == SBC_MODE_AVG){
				bp->colball = akku/(float)akku_count*sb->colball;
			}
			if (sb->sbc_mode == SBC_MODE_MIN){
				bp->colball=min*sb->colball;
			}
			if (sb->sbc_mode == SBC_MODE_MAX){
				bp->colball=max*sb->colball;
			}
			if (sb->sbc_mode == SBC_MODE_AVGMINMAX){
				bp->colball = (min + max)/2.0f*sb->colball;
			}
		}
		else bp->colball=0;
	}/*for bp*/		
}


/* creates new softbody if didn't exist yet, makes new points and springs arrays */
static void renew_softbody(Scene *scene, Object *ob, int totpoint, int totspring)  
{
	SoftBody *sb;
	int i;
	short softflag;
	if(ob->soft==NULL) ob->soft= sbNew(scene);
	else free_softbody_intern(ob->soft);
	sb= ob->soft;
	softflag=ob->softflag;
	   
	if(totpoint) {
		sb->totpoint= totpoint;
		sb->totspring= totspring;
		
		sb->bpoint= MEM_mallocN( totpoint*sizeof(BodyPoint), "bodypoint");
		if(totspring) 
			sb->bspring= MEM_mallocN( totspring*sizeof(BodySpring), "bodyspring");

			/* initialise BodyPoint array */
		for (i=0; i<totpoint; i++) {
			BodyPoint *bp = &sb->bpoint[i];

			if(softflag & OB_SB_GOAL) {
				bp->goal= sb->defgoal;
			}
			else { 
				bp->goal= 0.0f; 
				/* so this will definily be below SOFTGOALSNAP */
			}
			
			bp->nofsprings= 0;
			bp->springs= NULL;
			bp->choke = 0.0f;
			bp->choke2 = 0.0f;
			bp->frozen = 1.0f;
			bp->colball = 0.0f;
			bp->flag = 0;
			bp->springweight = 1.0f;
			bp->mass = sb->nodemass;
		}
	}
}

static void free_softbody_baked(SoftBody *sb)
{
	SBVertex *key;
	int k;

	for(k=0; k<sb->totkey; k++) {
		key= *(sb->keys + k);
		if(key) MEM_freeN(key);
	}
	if(sb->keys) MEM_freeN(sb->keys);
	
	sb->keys= NULL;
	sb->totkey= 0;
}
static void free_scratch(SoftBody *sb)
{
	if(sb->scratch){
		/* todo make sure everything is cleaned up nicly */
		if (sb->scratch->colliderhash){
			BLI_ghash_free(sb->scratch->colliderhash, NULL,
					(GHashValFreeFP) ccd_mesh_free); /*this hoepfully will free all caches*/
			sb->scratch->colliderhash = NULL;
		}
		if (sb->scratch->bodyface){
			MEM_freeN(sb->scratch->bodyface);
		}
		if (sb->scratch->Ref.ivert){
			MEM_freeN(sb->scratch->Ref.ivert);
		}
		MEM_freeN(sb->scratch);
		sb->scratch = NULL;
	}
	
}

/* only frees internal data */
static void free_softbody_intern(SoftBody *sb)
{
	if(sb) {
		int a;
		BodyPoint *bp;
		
		if(sb->bpoint){
			for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
				/* free spring list */ 
				if (bp->springs != NULL) {
					MEM_freeN(bp->springs);
				}
			}
			MEM_freeN(sb->bpoint);
		}
		
		if(sb->bspring) MEM_freeN(sb->bspring);
		
		sb->totpoint= sb->totspring= 0;
		sb->bpoint= NULL;
		sb->bspring= NULL;

		free_scratch(sb);
		free_softbody_baked(sb);
	}
}


/* ************ dynamics ********** */

/* the most general (micro physics correct) way to do collision 
** (only needs the current particle position)  
**
** it actually checks if the particle intrudes a short range force field generated 
** by the faces of the target object and returns a force to drive the particel out
** the strenght of the field grows exponetially if the particle is on the 'wrong' side of the face
** 'wrong' side : projection to the face normal is negative (all referred to a vertex in the face)
**
** flaw of this: 'fast' particles as well as 'fast' colliding faces 
** give a 'tunnel' effect such that the particle passes through the force field 
** without ever 'seeing' it 
** this is fully compliant to heisenberg: h >= fuzzy(location) * fuzzy(time)
** besides our h is way larger than in QM because forces propagate way slower here
** we have to deal with fuzzy(time) in the range of 1/25 seconds (typical frame rate)
** yup collision targets are not known here any better 
** and 1/25 second is looong compared to real collision events
** Q: why not use 'simple' collision here like bouncing back a particle 
**   --> reverting is velocity on the face normal
** A: because our particles are not alone here 
**    and need to tell their neighbours exactly what happens via spring forces 
** unless sbObjectStep( .. ) is called on sub frame timing level
** BTW that also questions the use of a 'implicit' solvers on softbodies  
** since that would only valid for 'slow' moving collision targets and dito particles
*/

/* aye this belongs to arith.c */
static void Vec3PlusStVec(float *v, float s, float *v1)
{
	v[0] += s*v1[0];
	v[1] += s*v1[1];
	v[2] += s*v1[2];
}

/* +++ dependancy information functions*/

static int are_there_deflectors(Scene *scene, unsigned int layer)
{
	Base *base;
	
	for(base = scene->base.first; base; base= base->next) {
		if( (base->lay & layer) && base->object->pd) {
			if(base->object->pd->deflect) 
				return 1;
		}
	}
	return 0;
}

static int query_external_colliders(Scene *scene, Object *me)
{
	return(are_there_deflectors(scene, me->lay));
}
/* --- dependancy information functions*/


/* +++ the aabb "force" section*/
static int sb_detect_aabb_collisionCached(	float force[3], unsigned int par_layer,struct Object *vertexowner,float time)
{
	Object *ob;
	SoftBody *sb=vertexowner->soft;
	GHash *hash;
	GHashIterator *ihash;
	float  aabbmin[3],aabbmax[3];
	int a, deflected=0;

	if ((sb == NULL) || (sb->scratch ==NULL)) return 0;
	VECCOPY(aabbmin,sb->scratch->aabbmin);
	VECCOPY(aabbmax,sb->scratch->aabbmax);

	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;
				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;
					
					if ((aabbmax[0] < ccdm->bbmin[0]) || 
						(aabbmax[1] < ccdm->bbmin[1]) ||
						(aabbmax[2] < ccdm->bbmin[2]) ||
						(aabbmin[0] > ccdm->bbmax[0]) || 
						(aabbmin[1] > ccdm->bbmax[1]) || 
						(aabbmin[2] > ccdm->bbmax[2]) ) {
						/* boxes dont intersect */ 
						BLI_ghashIterator_step(ihash);
						continue;				
					}					

					/* so now we have the 2 boxes overlapping */
                    /* forces actually not used */
					deflected = 2;

				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
					BLI_ghashIterator_step(ihash);
					continue;				
				}
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */
	BLI_ghashIterator_free(ihash);
	return deflected;	
}
/* --- the aabb section*/


/* +++ the face external section*/
static int sb_detect_face_pointCached(float face_v1[3],float face_v2[3],float face_v3[3],float *damp,						
								   float force[3], unsigned int par_layer,struct Object *vertexowner,float time)
								   {
	Object *ob;
	GHash *hash;
	GHashIterator *ihash;
	float nv1[3], edge1[3], edge2[3], d_nvect[3], aabbmin[3],aabbmax[3];
	float facedist,outerfacethickness,tune = 10.f;
	int a, deflected=0;

	aabbmin[0] = MIN3(face_v1[0],face_v2[0],face_v3[0]);
	aabbmin[1] = MIN3(face_v1[1],face_v2[1],face_v3[1]);
	aabbmin[2] = MIN3(face_v1[2],face_v2[2],face_v3[2]);
	aabbmax[0] = MAX3(face_v1[0],face_v2[0],face_v3[0]);
	aabbmax[1] = MAX3(face_v1[1],face_v2[1],face_v3[1]);
	aabbmax[2] = MAX3(face_v1[2],face_v2[2],face_v3[2]);

	/* calculate face normal once again SIGH */
	VECSUB(edge1, face_v1, face_v2);
	VECSUB(edge2, face_v3, face_v2);
	cross_v3_v3v3(d_nvect, edge2, edge1);
	normalize_v3(d_nvect);


	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				if(ccdm){
					mvert= ccdm->mvert;
					a    = ccdm->totvert; 
					mprevvert= ccdm->mprevvert;				
					outerfacethickness =ob->pd->pdef_sboft;
					if ((aabbmax[0] < ccdm->bbmin[0]) || 
						(aabbmax[1] < ccdm->bbmin[1]) ||
						(aabbmax[2] < ccdm->bbmin[2]) ||
						(aabbmin[0] > ccdm->bbmax[0]) || 
						(aabbmin[1] > ccdm->bbmax[1]) || 
						(aabbmin[2] > ccdm->bbmax[2]) ) {
						/* boxes dont intersect */ 
						BLI_ghashIterator_step(ihash);
						continue;				
					}					

				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
					BLI_ghashIterator_step(ihash);
					continue;				
				}


				/* use mesh*/
				if (mvert) {
					while(a){
						VECCOPY(nv1,mvert[a-1].co);						
						if(mprevvert){
							mul_v3_fl(nv1,time);
							Vec3PlusStVec(nv1,(1.0f-time),mprevvert[a-1].co);
						}
						/* origin to face_v2*/
						VECSUB(nv1, nv1, face_v2);
						facedist = dot_v3v3(nv1,d_nvect);
						if (ABS(facedist)<outerfacethickness){
							if (isect_point_tri_prism_v3(nv1, face_v1,face_v2,face_v3) ){
								float df;
								if (facedist > 0){
									df = (outerfacethickness-facedist)/outerfacethickness;
								}
								else {
									df = (outerfacethickness+facedist)/outerfacethickness;
								}

								*damp=df*tune*ob->pd->pdef_sbdamp;

								df = 0.01f*exp(- 100.0f*df);
								Vec3PlusStVec(force,-df,d_nvect);
								deflected = 3;
							}
						}
						a--;
					}/* while(a)*/
				} /* if (mvert) */
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */
	BLI_ghashIterator_free(ihash);
	return deflected;	
}


static int sb_detect_face_collisionCached(float face_v1[3],float face_v2[3],float face_v3[3],float *damp,						
								   float force[3], unsigned int par_layer,struct Object *vertexowner,float time)
{
	Object *ob;
	GHash *hash;
	GHashIterator *ihash;
	float nv1[3], nv2[3], nv3[3], nv4[3], edge1[3], edge2[3], d_nvect[3], aabbmin[3],aabbmax[3];
	float t,tune = 10.0f;
	int a, deflected=0;

	aabbmin[0] = MIN3(face_v1[0],face_v2[0],face_v3[0]);
	aabbmin[1] = MIN3(face_v1[1],face_v2[1],face_v3[1]);
	aabbmin[2] = MIN3(face_v1[2],face_v2[2],face_v3[2]);
	aabbmax[0] = MAX3(face_v1[0],face_v2[0],face_v3[0]);
	aabbmax[1] = MAX3(face_v1[1],face_v2[1],face_v3[1]);
	aabbmax[2] = MAX3(face_v1[2],face_v2[2],face_v3[2]);

	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;
				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;
					
					if ((aabbmax[0] < ccdm->bbmin[0]) || 
						(aabbmax[1] < ccdm->bbmin[1]) ||
						(aabbmax[2] < ccdm->bbmin[2]) ||
						(aabbmin[0] > ccdm->bbmax[0]) || 
						(aabbmin[1] > ccdm->bbmax[1]) || 
						(aabbmin[2] > ccdm->bbmax[2]) ) {
						/* boxes dont intersect */ 
						BLI_ghashIterator_step(ihash);
						continue;				
					}					

				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
					BLI_ghashIterator_step(ihash);
					continue;				
				}


				/* use mesh*/
				while (a--) {
					if (
						(aabbmax[0] < mima->minx) || 
						(aabbmin[0] > mima->maxx) || 
						(aabbmax[1] < mima->miny) ||
						(aabbmin[1] > mima->maxy) || 
						(aabbmax[2] < mima->minz) ||
						(aabbmin[2] > mima->maxz) 
						) {
						mface++;
						mima++;
						continue;
					}


					if (mvert){

						VECCOPY(nv1,mvert[mface->v1].co);						
						VECCOPY(nv2,mvert[mface->v2].co);
						VECCOPY(nv3,mvert[mface->v3].co);
						if (mface->v4){
							VECCOPY(nv4,mvert[mface->v4].co);
						}
						if (mprevvert){
							mul_v3_fl(nv1,time);
							Vec3PlusStVec(nv1,(1.0f-time),mprevvert[mface->v1].co);
							
							mul_v3_fl(nv2,time);
							Vec3PlusStVec(nv2,(1.0f-time),mprevvert[mface->v2].co);
							
							mul_v3_fl(nv3,time);
							Vec3PlusStVec(nv3,(1.0f-time),mprevvert[mface->v3].co);
							
							if (mface->v4){
								mul_v3_fl(nv4,time);
								Vec3PlusStVec(nv4,(1.0f-time),mprevvert[mface->v4].co);
							}
						}	
					}

					/* switch origin to be nv2*/
					VECSUB(edge1, nv1, nv2);
					VECSUB(edge2, nv3, nv2);
					cross_v3_v3v3(d_nvect, edge2, edge1);
					normalize_v3(d_nvect);
					if ( 
						isect_line_tri_v3(nv1, nv2, face_v1, face_v2, face_v3, &t, NULL) ||
						isect_line_tri_v3(nv2, nv3, face_v1, face_v2, face_v3, &t, NULL) ||
						isect_line_tri_v3(nv3, nv1, face_v1, face_v2, face_v3, &t, NULL) ){
						Vec3PlusStVec(force,-0.5f,d_nvect);
						*damp=tune*ob->pd->pdef_sbdamp;
						deflected = 2;
					}
					if (mface->v4){ /* quad */
						/* switch origin to be nv4 */
						VECSUB(edge1, nv3, nv4);
						VECSUB(edge2, nv1, nv4);					
						cross_v3_v3v3(d_nvect, edge2, edge1);
						normalize_v3(d_nvect);	
						if ( 
							/* isect_line_tri_v3(nv1, nv3, face_v1, face_v2, face_v3, &t, NULL) ||
							 we did that edge allready */
							isect_line_tri_v3(nv3, nv4, face_v1, face_v2, face_v3, &t, NULL) ||
							isect_line_tri_v3(nv4, nv1, face_v1, face_v2, face_v3, &t, NULL) ){
							Vec3PlusStVec(force,-0.5f,d_nvect);
							*damp=tune*ob->pd->pdef_sbdamp;
							deflected = 2;
						}
					}
					mface++;
					mima++;					
				}/* while a */		
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */
	BLI_ghashIterator_free(ihash);
	return deflected;	
}



static void scan_for_ext_face_forces(Object *ob,float timenow)
{
	SoftBody *sb = ob->soft;
	BodyFace *bf;
	int a;
	float damp=0.0f,choke=1.0f; 
	float tune = -10.0f;
	float feedback[3];
	
	if (sb && sb->scratch->totface){
		
		
		bf = sb->scratch->bodyface;
		for(a=0; a<sb->scratch->totface; a++, bf++) {
			bf->ext_force[0]=bf->ext_force[1]=bf->ext_force[2]=0.0f; 
/*+++edges intruding*/
			bf->flag &= ~BFF_INTERSECT;		
			feedback[0]=feedback[1]=feedback[2]=0.0f;
			if (sb_detect_face_collisionCached(sb->bpoint[bf->v1].pos,sb->bpoint[bf->v2].pos, sb->bpoint[bf->v3].pos, 
				&damp,	feedback, ob->lay ,ob , timenow)){
				Vec3PlusStVec(sb->bpoint[bf->v1].force,tune,feedback);
				Vec3PlusStVec(sb->bpoint[bf->v2].force,tune,feedback);
				Vec3PlusStVec(sb->bpoint[bf->v3].force,tune,feedback);
//				Vec3PlusStVec(bf->ext_force,tune,feedback);
				bf->flag |= BFF_INTERSECT;
				choke = MIN2(MAX2(damp,choke),1.0f);
			}

			feedback[0]=feedback[1]=feedback[2]=0.0f;
			if ((bf->v4) && (sb_detect_face_collisionCached(sb->bpoint[bf->v1].pos,sb->bpoint[bf->v3].pos, sb->bpoint[bf->v4].pos, 
				&damp,	feedback, ob->lay ,ob , timenow))){
				Vec3PlusStVec(sb->bpoint[bf->v1].force,tune,feedback);
				Vec3PlusStVec(sb->bpoint[bf->v3].force,tune,feedback);
				Vec3PlusStVec(sb->bpoint[bf->v4].force,tune,feedback);
//				Vec3PlusStVec(bf->ext_force,tune,feedback);
				bf->flag |= BFF_INTERSECT;
				choke = MIN2(MAX2(damp,choke),1.0f);
			}
/*---edges intruding*/

/*+++ close vertices*/
			if  (( bf->flag & BFF_INTERSECT)==0){
				bf->flag &= ~BFF_CLOSEVERT;
				tune = -1.0f;
				feedback[0]=feedback[1]=feedback[2]=0.0f;
				if (sb_detect_face_pointCached(sb->bpoint[bf->v1].pos,sb->bpoint[bf->v2].pos, sb->bpoint[bf->v3].pos, 
					&damp,	feedback, ob->lay ,ob , timenow)){
				Vec3PlusStVec(sb->bpoint[bf->v1].force,tune,feedback);
				Vec3PlusStVec(sb->bpoint[bf->v2].force,tune,feedback);
				Vec3PlusStVec(sb->bpoint[bf->v3].force,tune,feedback);
//						Vec3PlusStVec(bf->ext_force,tune,feedback);
						bf->flag |= BFF_CLOSEVERT;
						choke = MIN2(MAX2(damp,choke),1.0f);
				}

				feedback[0]=feedback[1]=feedback[2]=0.0f;
				if ((bf->v4) && (sb_detect_face_pointCached(sb->bpoint[bf->v1].pos,sb->bpoint[bf->v3].pos, sb->bpoint[bf->v4].pos, 
					&damp,	feedback, ob->lay ,ob , timenow))){
				Vec3PlusStVec(sb->bpoint[bf->v1].force,tune,feedback);
				Vec3PlusStVec(sb->bpoint[bf->v3].force,tune,feedback);
				Vec3PlusStVec(sb->bpoint[bf->v4].force,tune,feedback);
//						Vec3PlusStVec(bf->ext_force,tune,feedback);
						bf->flag |= BFF_CLOSEVERT;
						choke = MIN2(MAX2(damp,choke),1.0f);
				}
			}
/*--- close vertices*/
		}
		bf = sb->scratch->bodyface;
		for(a=0; a<sb->scratch->totface; a++, bf++) {
			if (( bf->flag & BFF_INTERSECT) || ( bf->flag & BFF_CLOSEVERT))
			{
                sb->bpoint[bf->v1].choke2=MAX2(sb->bpoint[bf->v1].choke2,choke);
                sb->bpoint[bf->v2].choke2=MAX2(sb->bpoint[bf->v2].choke2,choke);
                sb->bpoint[bf->v3].choke2=MAX2(sb->bpoint[bf->v3].choke2,choke);
				if (bf->v4){
                sb->bpoint[bf->v2].choke2=MAX2(sb->bpoint[bf->v2].choke2,choke);
				}
			}	
		}
	}
}

/*  --- the face external section*/


/* +++ the spring external section*/

static int sb_detect_edge_collisionCached(float edge_v1[3],float edge_v2[3],float *damp,						
								   float force[3], unsigned int par_layer,struct Object *vertexowner,float time)
{
	Object *ob;
	GHash *hash;
	GHashIterator *ihash;
	float nv1[3], nv2[3], nv3[3], nv4[3], edge1[3], edge2[3], d_nvect[3], aabbmin[3],aabbmax[3];
	float t,el;
	int a, deflected=0;

	aabbmin[0] = MIN2(edge_v1[0],edge_v2[0]);
	aabbmin[1] = MIN2(edge_v1[1],edge_v2[1]);
	aabbmin[2] = MIN2(edge_v1[2],edge_v2[2]);
	aabbmax[0] = MAX2(edge_v1[0],edge_v2[0]);
	aabbmax[1] = MAX2(edge_v1[1],edge_v2[1]);
	aabbmax[2] = MAX2(edge_v1[2],edge_v2[2]);

	el = len_v3v3(edge_v1,edge_v2);

	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;
				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;
					
					if ((aabbmax[0] < ccdm->bbmin[0]) || 
						(aabbmax[1] < ccdm->bbmin[1]) ||
						(aabbmax[2] < ccdm->bbmin[2]) ||
						(aabbmin[0] > ccdm->bbmax[0]) || 
						(aabbmin[1] > ccdm->bbmax[1]) || 
						(aabbmin[2] > ccdm->bbmax[2]) ) {
						/* boxes dont intersect */ 
						BLI_ghashIterator_step(ihash);
						continue;				
					}					

				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
					BLI_ghashIterator_step(ihash);
					continue;				
				}


				/* use mesh*/
				while (a--) {
					if (
						(aabbmax[0] < mima->minx) || 
						(aabbmin[0] > mima->maxx) || 
						(aabbmax[1] < mima->miny) ||
						(aabbmin[1] > mima->maxy) || 
						(aabbmax[2] < mima->minz) ||
						(aabbmin[2] > mima->maxz) 
						) {
						mface++;
						mima++;
						continue;
					}


					if (mvert){

						VECCOPY(nv1,mvert[mface->v1].co);						
						VECCOPY(nv2,mvert[mface->v2].co);
						VECCOPY(nv3,mvert[mface->v3].co);
						if (mface->v4){
							VECCOPY(nv4,mvert[mface->v4].co);
						}
						if (mprevvert){
							mul_v3_fl(nv1,time);
							Vec3PlusStVec(nv1,(1.0f-time),mprevvert[mface->v1].co);
							
							mul_v3_fl(nv2,time);
							Vec3PlusStVec(nv2,(1.0f-time),mprevvert[mface->v2].co);
							
							mul_v3_fl(nv3,time);
							Vec3PlusStVec(nv3,(1.0f-time),mprevvert[mface->v3].co);
							
							if (mface->v4){
								mul_v3_fl(nv4,time);
								Vec3PlusStVec(nv4,(1.0f-time),mprevvert[mface->v4].co);
							}
						}	
					}

					/* switch origin to be nv2*/
					VECSUB(edge1, nv1, nv2);
					VECSUB(edge2, nv3, nv2);

					cross_v3_v3v3(d_nvect, edge2, edge1);
					normalize_v3(d_nvect);
					if ( isect_line_tri_v3(edge_v1, edge_v2, nv1, nv2, nv3, &t, NULL)){
						float v1[3],v2[3];
						float intrusiondepth,i1,i2;
						VECSUB(v1, edge_v1, nv2);
						VECSUB(v2, edge_v2, nv2);
						i1 = dot_v3v3(v1,d_nvect);
						i2 = dot_v3v3(v2,d_nvect);
						intrusiondepth = -MIN2(i1,i2)/el;
						Vec3PlusStVec(force,intrusiondepth,d_nvect);
						*damp=ob->pd->pdef_sbdamp;
						deflected = 2;
					}
					if (mface->v4){ /* quad */
						/* switch origin to be nv4 */
						VECSUB(edge1, nv3, nv4);
						VECSUB(edge2, nv1, nv4);

						cross_v3_v3v3(d_nvect, edge2, edge1);
						normalize_v3(d_nvect);						
						if (isect_line_tri_v3( edge_v1, edge_v2,nv1, nv3, nv4, &t, NULL)){
							float v1[3],v2[3];
							float intrusiondepth,i1,i2;
							VECSUB(v1, edge_v1, nv4);
							VECSUB(v2, edge_v2, nv4);
						i1 = dot_v3v3(v1,d_nvect);
						i2 = dot_v3v3(v2,d_nvect);
						intrusiondepth = -MIN2(i1,i2)/el;


							Vec3PlusStVec(force,intrusiondepth,d_nvect);
							*damp=ob->pd->pdef_sbdamp;
							deflected = 2;
						}
					}
					mface++;
					mima++;					
				}/* while a */		
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */
	BLI_ghashIterator_free(ihash);
	return deflected;	
}

static void _scan_for_ext_spring_forces(Scene *scene, Object *ob, float timenow, int ifirst, int ilast, struct ListBase *do_effector)
{
	SoftBody *sb = ob->soft;
	int a;
	float damp; 
	float feedback[3];

	if (sb && sb->totspring){
		for(a=ifirst; a<ilast; a++) {
			BodySpring *bs = &sb->bspring[a];
			bs->ext_force[0]=bs->ext_force[1]=bs->ext_force[2]=0.0f; 
			feedback[0]=feedback[1]=feedback[2]=0.0f;
			bs->flag &= ~BSF_INTERSECT;

			if (bs->springtype == SB_EDGE){
				/* +++ springs colliding */
				if (ob->softflag & OB_SB_EDGECOLL){
					if ( sb_detect_edge_collisionCached (sb->bpoint[bs->v1].pos , sb->bpoint[bs->v2].pos,
						&damp,feedback,ob->lay,ob,timenow)){
							add_v3_v3v3(bs->ext_force,bs->ext_force,feedback);
							bs->flag |= BSF_INTERSECT;
							//bs->cf=damp;
                            bs->cf=sb->choke*0.01f;

					}
				}
				/* ---- springs colliding */

				/* +++ springs seeing wind ... n stuff depending on their orientation*/
				/* note we don't use sb->mediafrict but use sb->aeroedge for magnitude of effect*/ 
				if(sb->aeroedge){
					float vel[3],sp[3],pr[3],force[3];
					float f,windfactor  = 0.25f;   
					/*see if we have wind*/
					if(do_effector) {
						EffectedPoint epoint;
						float speed[3]={0.0f,0.0f,0.0f};
						float pos[3];
						mid_v3_v3v3(pos, sb->bpoint[bs->v1].pos , sb->bpoint[bs->v2].pos);
						mid_v3_v3v3(vel, sb->bpoint[bs->v1].vec , sb->bpoint[bs->v2].vec);
						pd_point_from_soft(scene, pos, vel, -1, &epoint);
						pdDoEffectors(do_effector, NULL, sb->effector_weights, &epoint, force, speed);

						mul_v3_fl(speed,windfactor); 
						add_v3_v3v3(vel,vel,speed);
					}
					/* media in rest */
					else{
						VECADD(vel, sb->bpoint[bs->v1].vec , sb->bpoint[bs->v2].vec);
					}
					f = normalize_v3(vel);
					f = -0.0001f*f*f*sb->aeroedge;
					/* (todo) add a nice angle dependant function done for now BUT */
					/* still there could be some nice drag/lift function, but who needs it */ 

					VECSUB(sp, sb->bpoint[bs->v1].pos , sb->bpoint[bs->v2].pos);
					project_v3_v3v3(pr,vel,sp);
					VECSUB(vel,vel,pr);
					normalize_v3(vel);
					if (ob->softflag & OB_SB_AERO_ANGLE){
						normalize_v3(sp);
						Vec3PlusStVec(bs->ext_force,f*(1.0f-ABS(dot_v3v3(vel,sp))),vel);
					}
					else{ 
						Vec3PlusStVec(bs->ext_force,f,vel); // to keep compatible with 2.45 release files
					}
				}
				/* --- springs seeing wind */
			}
		}
	}
}


static void scan_for_ext_spring_forces(Scene *scene, Object *ob, float timenow)
{
  SoftBody *sb = ob->soft;
  ListBase *do_effector = NULL; 
  
  do_effector = pdInitEffectors(scene, ob, NULL, sb->effector_weights);
  if (sb){
	  _scan_for_ext_spring_forces(scene, ob, timenow, 0, sb->totspring, do_effector);
  }
  pdEndEffectors(&do_effector);
}

static void *exec_scan_for_ext_spring_forces(void *data)
{
	SB_thread_context *pctx = (SB_thread_context*)data;
	_scan_for_ext_spring_forces(pctx->scene, pctx->ob, pctx->timenow, pctx->ifirst, pctx->ilast, pctx->do_effector);
	return 0;
} 

static void sb_sfesf_threads_run(Scene *scene, struct Object *ob, float timenow,int totsprings,int *ptr_to_break_func())
{
	ListBase *do_effector = NULL; 
	ListBase threads;
	SB_thread_context *sb_threads;
	int i, totthread,left,dec;
	int lowsprings =100; /* wild guess .. may increase with better thread management 'above' or even be UI option sb->spawn_cf_threads_nopts */

	do_effector= pdInitEffectors(scene, ob, NULL, ob->soft->effector_weights);

	/* figure the number of threads while preventing pretty pointless threading overhead */
	if(scene->r.mode & R_FIXED_THREADS)
		totthread= scene->r.threads;
	else
		totthread= BLI_system_thread_count();
	/* what if we got zillions of CPUs running but less to spread*/
	while ((totsprings/totthread < lowsprings) && (totthread > 1)) {
		totthread--;
	}

	sb_threads= MEM_callocN(sizeof(SB_thread_context)*totthread, "SBSpringsThread");
	memset(sb_threads, 0, sizeof(SB_thread_context)*totthread);
	left = totsprings;
	dec = totsprings/totthread +1;
	for(i=0; i<totthread; i++) {
		sb_threads[i].scene = scene;
		sb_threads[i].ob = ob; 
		sb_threads[i].forcetime = 0.0; // not used here 
		sb_threads[i].timenow = timenow; 
		sb_threads[i].ilast   = left; 
		left = left - dec;
		if (left >0){
			sb_threads[i].ifirst  = left;
		}
		else
			sb_threads[i].ifirst  = 0; 
        sb_threads[i].do_effector = do_effector;
        sb_threads[i].do_deflector = 0;// not used here
		sb_threads[i].fieldfactor = 0.0f;// not used here
		sb_threads[i].windfactor  = 0.0f;// not used here
		sb_threads[i].nr= i;
		sb_threads[i].tot= totthread;
	}
	if(totthread > 1) {
		BLI_init_threads(&threads, exec_scan_for_ext_spring_forces, totthread);

		for(i=0; i<totthread; i++)
			BLI_insert_thread(&threads, &sb_threads[i]);

		BLI_end_threads(&threads);
	}
	else
		exec_scan_for_ext_spring_forces(&sb_threads[0]);
    /* clean up */
	MEM_freeN(sb_threads);
	
	pdEndEffectors(&do_effector);
}


/* --- the spring external section*/

static int choose_winner(float*w, float* pos,float*a,float*b,float*c,float*ca,float*cb,float*cc)
{
	float mindist,cp;
	int winner =1;
	mindist = ABS(dot_v3v3(pos,a));

    cp = ABS(dot_v3v3(pos,b));
	if ( mindist < cp ){
		mindist = cp;
		winner =2;
	}

	cp = ABS(dot_v3v3(pos,c));
	if (mindist < cp ){
		mindist = cp;
		winner =3;
	}
	switch (winner){ 
		case 1: VECCOPY(w,ca); break; 
		case 2: VECCOPY(w,cb); break; 
		case 3: VECCOPY(w,cc); 
	}
	return(winner);
}



static int sb_detect_vertex_collisionCached(float opco[3], float facenormal[3], float *damp,
									 float force[3], unsigned int par_layer,struct Object *vertexowner,
									 float time,float vel[3], float *intrusion)
{
	Object *ob= NULL;
	GHash *hash;
	GHashIterator *ihash;
	float nv1[3], nv2[3], nv3[3], nv4[3], edge1[3], edge2[3],d_nvect[3], dv1[3],ve[3],avel[3]={0.0,0.0,0.0},
    vv1[3], vv2[3], vv3[3], vv4[3], coledge[3]={0.0f, 0.0f, 0.0f}, mindistedge = 1000.0f, 
	outerforceaccu[3],innerforceaccu[3],
		facedist,n_mag,force_mag_norm,minx,miny,minz,maxx,maxy,maxz,
		innerfacethickness = -0.5f, outerfacethickness = 0.2f,
		ee = 5.0f, ff = 0.1f, fa=1;
	int a, deflected=0, cavel=0,ci=0;
/* init */
	*intrusion = 0.0f;
	hash  = vertexowner->soft->scratch->colliderhash;
	ihash =	BLI_ghashIterator_new(hash);
	outerforceaccu[0]=outerforceaccu[1]=outerforceaccu[2]=0.0f;
	innerforceaccu[0]=innerforceaccu[1]=innerforceaccu[2]=0.0f;
/* go */
    while (!BLI_ghashIterator_isDone(ihash) ) {

		ccd_Mesh *ccdm = BLI_ghashIterator_getValue	(ihash);
		ob             = BLI_ghashIterator_getKey	(ihash);
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				MFace *mface= NULL;
				MVert *mvert= NULL;
				MVert *mprevvert= NULL;
				ccdf_minmax *mima= NULL;

				if(ccdm){
					mface= ccdm->mface;
					mvert= ccdm->mvert;
					mprevvert= ccdm->mprevvert;
					mima= ccdm->mima;
					a = ccdm->totface;

					minx =ccdm->bbmin[0]; 
					miny =ccdm->bbmin[1]; 
					minz =ccdm->bbmin[2];

					maxx =ccdm->bbmax[0]; 
					maxy =ccdm->bbmax[1]; 
					maxz =ccdm->bbmax[2]; 

					if ((opco[0] < minx) || 
						(opco[1] < miny) ||
						(opco[2] < minz) ||
						(opco[0] > maxx) || 
						(opco[1] > maxy) || 
						(opco[2] > maxz) ) {
							/* outside the padded boundbox --> collision object is too far away */ 
												BLI_ghashIterator_step(ihash);
							continue;				
					}					
				}
				else{
					/*aye that should be cached*/
					printf("missing cache error \n");
						BLI_ghashIterator_step(ihash);
					continue;				
				}

				/* do object level stuff */
				/* need to have user control for that since it depends on model scale */
				innerfacethickness =-ob->pd->pdef_sbift;
				outerfacethickness =ob->pd->pdef_sboft;
				fa = (ff*outerfacethickness-outerfacethickness);
				fa *= fa;
				fa = 1.0f/fa;
                avel[0]=avel[1]=avel[2]=0.0f;
				/* use mesh*/
				while (a--) {
					if (
						(opco[0] < mima->minx) || 
						(opco[0] > mima->maxx) || 
						(opco[1] < mima->miny) ||
						(opco[1] > mima->maxy) || 
						(opco[2] < mima->minz) ||
						(opco[2] > mima->maxz) 
						) {
							mface++;
							mima++;
							continue;
					}

					if (mvert){

						VECCOPY(nv1,mvert[mface->v1].co);						
						VECCOPY(nv2,mvert[mface->v2].co);
						VECCOPY(nv3,mvert[mface->v3].co);
						if (mface->v4){
							VECCOPY(nv4,mvert[mface->v4].co);
						}

						if (mprevvert){
							/* grab the average speed of the collider vertices
							before we spoil nvX 
							humm could be done once a SB steps but then we' need to store that too
							since the AABB reduced propabitlty to get here drasticallly
							it might be a nice tradeof CPU <--> memory
							*/
							VECSUB(vv1,nv1,mprevvert[mface->v1].co);
							VECSUB(vv2,nv2,mprevvert[mface->v2].co);
							VECSUB(vv3,nv3,mprevvert[mface->v3].co);
							if (mface->v4){
								VECSUB(vv4,nv4,mprevvert[mface->v4].co);
							}

							mul_v3_fl(nv1,time);
							Vec3PlusStVec(nv1,(1.0f-time),mprevvert[mface->v1].co);

							mul_v3_fl(nv2,time);
							Vec3PlusStVec(nv2,(1.0f-time),mprevvert[mface->v2].co);

							mul_v3_fl(nv3,time);
							Vec3PlusStVec(nv3,(1.0f-time),mprevvert[mface->v3].co);

							if (mface->v4){
								mul_v3_fl(nv4,time);
								Vec3PlusStVec(nv4,(1.0f-time),mprevvert[mface->v4].co);
							}
						}	
					}
					
					/* switch origin to be nv2*/
					VECSUB(edge1, nv1, nv2);
					VECSUB(edge2, nv3, nv2);
					VECSUB(dv1,opco,nv2); /* abuse dv1 to have vertex in question at *origin* of triangle */

					cross_v3_v3v3(d_nvect, edge2, edge1);
					n_mag = normalize_v3(d_nvect);
					facedist = dot_v3v3(dv1,d_nvect);
					// so rules are
					//

					if ((facedist > innerfacethickness) && (facedist < outerfacethickness)){		
						if (isect_point_tri_prism_v3(opco, nv1, nv2, nv3) ){
							force_mag_norm =(float)exp(-ee*facedist);
							if (facedist > outerfacethickness*ff)
								force_mag_norm =(float)force_mag_norm*fa*(facedist - outerfacethickness)*(facedist - outerfacethickness);
							*damp=ob->pd->pdef_sbdamp;
							if (facedist > 0.0f){
								*damp *= (1.0f - facedist/outerfacethickness);
								Vec3PlusStVec(outerforceaccu,force_mag_norm,d_nvect);
								deflected = 3;

							}
							else {
								Vec3PlusStVec(innerforceaccu,force_mag_norm,d_nvect);
								if (deflected < 2) deflected = 2;
							}
							if ((mprevvert) && (*damp > 0.0f)){
								choose_winner(ve,opco,nv1,nv2,nv3,vv1,vv2,vv3);
								VECADD(avel,avel,ve);
								cavel ++;
							}
							*intrusion += facedist;
							ci++;
						}
					}		
					if (mface->v4){ /* quad */
						/* switch origin to be nv4 */
						VECSUB(edge1, nv3, nv4);
						VECSUB(edge2, nv1, nv4);
						VECSUB(dv1,opco,nv4); /* abuse dv1 to have vertex in question at *origin* of triangle */

						cross_v3_v3v3(d_nvect, edge2, edge1);
						n_mag = normalize_v3(d_nvect);
						facedist = dot_v3v3(dv1,d_nvect);

						if ((facedist > innerfacethickness) && (facedist < outerfacethickness)){
							if (isect_point_tri_prism_v3(opco, nv1, nv3, nv4) ){
								force_mag_norm =(float)exp(-ee*facedist);
								if (facedist > outerfacethickness*ff)
									force_mag_norm =(float)force_mag_norm*fa*(facedist - outerfacethickness)*(facedist - outerfacethickness);
								*damp=ob->pd->pdef_sbdamp;
							if (facedist > 0.0f){
								*damp *= (1.0f - facedist/outerfacethickness);
								Vec3PlusStVec(outerforceaccu,force_mag_norm,d_nvect);
								deflected = 3;

							}
							else {
								Vec3PlusStVec(innerforceaccu,force_mag_norm,d_nvect);
								if (deflected < 2) deflected = 2;
							}

								if ((mprevvert) && (*damp > 0.0f)){
									choose_winner(ve,opco,nv1,nv3,nv4,vv1,vv3,vv4);
									VECADD(avel,avel,ve);
									cavel ++;
								}
							    *intrusion += facedist;
								ci++;
							}

						}
						if ((deflected < 2)&& (G.rt != 444)) // we did not hit a face until now
						{ // see if 'outer' hits an edge
							float dist;

							closest_to_line_segment_v3(ve, opco, nv1, nv2);
 				            VECSUB(ve,opco,ve); 
							dist = normalize_v3(ve);
							if ((dist < outerfacethickness)&&(dist < mindistedge )){
								VECCOPY(coledge,ve);
								mindistedge = dist,
								deflected=1;
							}

							closest_to_line_segment_v3(ve, opco, nv2, nv3);
 				            VECSUB(ve,opco,ve); 
							dist = normalize_v3(ve);
							if ((dist < outerfacethickness)&&(dist < mindistedge )){
								VECCOPY(coledge,ve);
								mindistedge = dist,
								deflected=1;
							}

							closest_to_line_segment_v3(ve, opco, nv3, nv1);
 				            VECSUB(ve,opco,ve); 
							dist = normalize_v3(ve);
							if ((dist < outerfacethickness)&&(dist < mindistedge )){
								VECCOPY(coledge,ve);
								mindistedge = dist,
								deflected=1;
							}
							if (mface->v4){ /* quad */
								closest_to_line_segment_v3(ve, opco, nv3, nv4);
								VECSUB(ve,opco,ve); 
								dist = normalize_v3(ve);
								if ((dist < outerfacethickness)&&(dist < mindistedge )){
									VECCOPY(coledge,ve);
									mindistedge = dist,
										deflected=1;
								}

								closest_to_line_segment_v3(ve, opco, nv1, nv4);
								VECSUB(ve,opco,ve); 
								dist = normalize_v3(ve);
								if ((dist < outerfacethickness)&&(dist < mindistedge )){
									VECCOPY(coledge,ve);
									mindistedge = dist,
										deflected=1;
								}
							
							}


						}
					}
					mface++;
					mima++;					
				}/* while a */		
			} /* if(ob->pd && ob->pd->deflect) */
			BLI_ghashIterator_step(ihash);
	} /* while () */

	if (deflected == 1){ // no face but 'outer' edge cylinder sees vert
		force_mag_norm =(float)exp(-ee*mindistedge);
		if (mindistedge > outerfacethickness*ff)
			force_mag_norm =(float)force_mag_norm*fa*(mindistedge - outerfacethickness)*(mindistedge - outerfacethickness);
		Vec3PlusStVec(force,force_mag_norm,coledge);
		*damp=ob->pd->pdef_sbdamp;
		if (mindistedge > 0.0f){
			*damp *= (1.0f - mindistedge/outerfacethickness);
		}

	}
	if (deflected == 2){ //  face inner detected
		VECADD(force,force,innerforceaccu);
	}
	if (deflected == 3){ //  face outer detected
		VECADD(force,force,outerforceaccu);
	}

	BLI_ghashIterator_free(ihash);
	if (cavel) mul_v3_fl(avel,1.0f/(float)cavel);
	VECCOPY(vel,avel);
	if (ci) *intrusion /= ci;
	if (deflected){ 
		VECCOPY(facenormal,force);
		normalize_v3(facenormal);
	}
	return deflected;	
}


/* sandbox to plug in various deflection algos */
static int sb_deflect_face(Object *ob,float *actpos,float *facenormal,float *force,float *cf,float time,float *vel,float *intrusion)
{
	float s_actpos[3];
	int deflected;	
	VECCOPY(s_actpos,actpos);
	deflected= sb_detect_vertex_collisionCached(s_actpos, facenormal, cf, force , ob->lay, ob,time,vel,intrusion);
	//deflected= sb_detect_vertex_collisionCachedEx(s_actpos, facenormal, cf, force , ob->lay, ob,time,vel,intrusion);
	return(deflected);
}

/* hiding this for now .. but the jacobian may pop up on other tasks .. so i'd like to keep it
static void dfdx_spring(int ia, int ic, int op, float dir[3],float L,float len,float factor)
{ 
	float m,delta_ij;
	int i ,j;
	if (L < len){
		for(i=0;i<3;i++)
			for(j=0;j<3;j++){
				delta_ij = (i==j ? (1.0f): (0.0f));
				m=factor*(dir[i]*dir[j] + (1-L/len)*(delta_ij - dir[i]*dir[j]));
				nlMatrixAdd(ia+i,op+ic+j,m);
			}
	}
	else{
		for(i=0;i<3;i++)
			for(j=0;j<3;j++){
				m=factor*dir[i]*dir[j];
				nlMatrixAdd(ia+i,op+ic+j,m);
			}
	}
}


static void dfdx_goal(int ia, int ic, int op, float factor)
{ 
	int i;
	for(i=0;i<3;i++) nlMatrixAdd(ia+i,op+ic+i,factor);
}

static void dfdv_goal(int ia, int ic,float factor)
{ 
	int i;
	for(i=0;i<3;i++) nlMatrixAdd(ia+i,ic+i,factor);
}
*/
static void sb_spring_force(Object *ob,int bpi,BodySpring *bs,float iks,float forcetime,int nl_flags)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint  *bp1,*bp2;

	float dir[3],dvel[3];
	float distance,forcefactor,kd,absvel,projvel,kw;
	int ia,ic;
	/* prepare depending on which side of the spring we are on */
	if (bpi == bs->v1){
		bp1 = &sb->bpoint[bs->v1];
		bp2 = &sb->bpoint[bs->v2];
		ia =3*bs->v1;
		ic =3*bs->v2;
	}
	else if (bpi == bs->v2){
		bp1 = &sb->bpoint[bs->v2];
		bp2 = &sb->bpoint[bs->v1];
		ia =3*bs->v2;
		ic =3*bs->v1;
	}
	else{
		/* TODO make this debug option */
		/**/
		printf("bodypoint <bpi> is not attached to spring  <*bs> --> sb_spring_force()\n");
		return;
	}

	/* do bp1 <--> bp2 elastic */
	sub_v3_v3v3(dir,bp1->pos,bp2->pos);
	distance = normalize_v3(dir);
	if (bs->len < distance)
		iks  = 1.0f/(1.0f-sb->inspring)-1.0f ;/* inner spring constants function */
	else
		iks  = 1.0f/(1.0f-sb->inpush)-1.0f ;/* inner spring constants function */

	if(bs->len > 0.0f) /* check for degenerated springs */
		forcefactor = iks/bs->len;
	else
		forcefactor = iks;
	kw = (bp1->springweight+bp2->springweight)/2.0f;
	kw = kw * kw;
	kw = kw * kw;
	switch (bs->springtype){
		case SB_EDGE:
			forcefactor *=  kw; 
			break;
		case SB_BEND:
			forcefactor *=sb->secondspring*kw; 
			break;
		case SB_STIFFQUAD:
			forcefactor *=sb->shearstiff*sb->shearstiff* kw; 
			break;
		default:
			break;
	}


	Vec3PlusStVec(bp1->force,(bs->len - distance)*forcefactor,dir);

	/* do bp1 <--> bp2 viscous */
	sub_v3_v3v3(dvel,bp1->vec,bp2->vec);
	kd = sb->infrict * sb_fric_force_scale(ob);
	absvel  = normalize_v3(dvel);
	projvel = dot_v3v3(dir,dvel);
	kd     *= absvel * projvel;
	Vec3PlusStVec(bp1->force,-kd,dir);

	/* do jacobian stuff if needed */
	if(nl_flags & NLF_BUILD){
		//int op =3*sb->totpoint;
		//float mvel = -forcetime*kd;
		//float mpos = -forcetime*forcefactor;
		/* depending on my pos */ 
		// dfdx_spring(ia,ia,op,dir,bs->len,distance,-mpos);
		/* depending on my vel */
		// dfdv_goal(ia,ia,mvel); // well that ignores geometie
		if(bp2->goal < SOFTGOALSNAP){ /* ommit this bp when it snaps */
			/* depending on other pos */ 
			// dfdx_spring(ia,ic,op,dir,bs->len,distance,mpos);
			/* depending on other vel */
			// dfdv_goal(ia,ia,-mvel); // well that ignores geometie
		}
	}
}


/* since this is definitely the most CPU consuming task here .. try to spread it */
/* core function _softbody_calc_forces_slice_in_a_thread */
/* result is int to be able to flag user break */
static int _softbody_calc_forces_slice_in_a_thread(Scene *scene, Object *ob, float forcetime, float timenow,int ifirst,int ilast,int *ptr_to_break_func(),ListBase *do_effector,int do_deflector,float fieldfactor, float windfactor)
{
	float iks;
	int bb,do_selfcollision,do_springcollision,do_aero;
	int number_of_points_here = ilast - ifirst;
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint  *bp;
	
	/* intitialize */
	if (sb) {
	/* check conditions for various options */
    /* +++ could be done on object level to squeeze out the last bits of it */
	do_selfcollision=((ob->softflag & OB_SB_EDGES) && (sb->bspring)&& (ob->softflag & OB_SB_SELF));
	do_springcollision=do_deflector && (ob->softflag & OB_SB_EDGES) &&(ob->softflag & OB_SB_EDGECOLL);
	do_aero=((sb->aeroedge)&& (ob->softflag & OB_SB_EDGES));
    /* --- could be done on object level to squeeze out the last bits of it */
	}
	else {
		printf("Error expected a SB here \n");
		return (999);
	}

/* debugerin */
	if  (sb->totpoint < ifirst) {
		printf("Aye 998");
		return (998);
	}
/* debugerin */


	bp = &sb->bpoint[ifirst]; 
	for(bb=number_of_points_here; bb>0; bb--, bp++) {
		/* clear forces  accumulator */
		bp->force[0]= bp->force[1]= bp->force[2]= 0.0;
		/* naive ball self collision */
		/* needs to be done if goal snaps or not */
		if(do_selfcollision){
			 	int attached;
				BodyPoint   *obp;
				BodySpring *bs;	
				int c,b;
				float velcenter[3],dvel[3],def[3];
				float distance;
				float compare;
     	        float bstune = sb->ballstiff;

				for(c=sb->totpoint, obp= sb->bpoint; c>=ifirst+bb; c--, obp++) {
					compare = (obp->colball + bp->colball);		
					sub_v3_v3v3(def, bp->pos, obp->pos);
					/* rather check the AABBoxes before ever calulating the real distance */
					/* mathematically it is completly nuts, but performace is pretty much (3) times faster */
					if ((ABS(def[0]) > compare) || (ABS(def[1]) > compare) || (ABS(def[2]) > compare)) continue;
                    distance = normalize_v3(def);
					if (distance < compare ){
						/* exclude body points attached with a spring */
						attached = 0;
						for(b=obp->nofsprings;b>0;b--){
							bs = sb->bspring + obp->springs[b-1];
							if (( ilast-bb == bs->v2)  || ( ilast-bb == bs->v1)){
								attached=1;
								continue;}
						}
						if (!attached){
							float f = bstune/(distance) + bstune/(compare*compare)*distance - 2.0f*bstune/compare ;

							mid_v3_v3v3(velcenter, bp->vec, obp->vec);
							sub_v3_v3v3(dvel,velcenter,bp->vec);
							mul_v3_fl(dvel,bp->mass);

							Vec3PlusStVec(bp->force,f*(1.0f-sb->balldamp),def);
							Vec3PlusStVec(bp->force,sb->balldamp,dvel);

							/* exploit force(a,b) == -force(b,a) part2/2 */
							sub_v3_v3v3(dvel,velcenter,obp->vec);
							mul_v3_fl(dvel,bp->mass);

							Vec3PlusStVec(obp->force,sb->balldamp,dvel);
							Vec3PlusStVec(obp->force,-f*(1.0f-sb->balldamp),def);
						}
					}
				}
		}
		/* naive ball self collision done */

		if(bp->goal < SOFTGOALSNAP){ /* ommit this bp when it snaps */
			float auxvect[3];  
			float velgoal[3];

			/* do goal stuff */
			if(ob->softflag & OB_SB_GOAL) {
				/* true elastic goal */
				float ks,kd;
				sub_v3_v3v3(auxvect,bp->pos,bp->origT);
				ks  = 1.0f/(1.0f- bp->goal*sb->goalspring)-1.0f ;
				bp->force[0]+= -ks*(auxvect[0]);
				bp->force[1]+= -ks*(auxvect[1]);
				bp->force[2]+= -ks*(auxvect[2]);

				/* calulate damping forces generated by goals*/
				sub_v3_v3v3(velgoal,bp->origS, bp->origE);
				kd =  sb->goalfrict * sb_fric_force_scale(ob) ;
				add_v3_v3v3(auxvect,velgoal,bp->vec);
				
				if (forcetime > 0.0 ) { /* make sure friction does not become rocket motor on time reversal */
					bp->force[0]-= kd * (auxvect[0]);
					bp->force[1]-= kd * (auxvect[1]);
					bp->force[2]-= kd * (auxvect[2]);
				}
				else {
					bp->force[0]-= kd * (velgoal[0] - bp->vec[0]);
					bp->force[1]-= kd * (velgoal[1] - bp->vec[1]);
					bp->force[2]-= kd * (velgoal[2] - bp->vec[2]);
				}
			}
			/* done goal stuff */
			
			/* gravitation */
			if (sb && scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY){ 
				float gravity[3];
				VECCOPY(gravity, scene->physics_settings.gravity);
				mul_v3_fl(gravity, sb_grav_force_scale(ob)*bp->mass*sb->effector_weights->global_gravity); /* individual mass of node here */
				add_v3_v3v3(bp->force, bp->force, gravity);
			}
			
			/* particle field & vortex */
			if(do_effector) {
				EffectedPoint epoint;
				float kd;
				float force[3]= {0.0f, 0.0f, 0.0f};
				float speed[3]= {0.0f, 0.0f, 0.0f};
				float eval_sb_fric_force_scale = sb_fric_force_scale(ob); /* just for calling function once */
				pd_point_from_soft(scene, bp->pos, bp->vec, sb->bpoint-bp, &epoint);
				pdDoEffectors(do_effector, NULL, sb->effector_weights, &epoint, force, speed);
				
				/* apply forcefield*/
				mul_v3_fl(force,fieldfactor* eval_sb_fric_force_scale); 
				VECADD(bp->force, bp->force, force);
				
				/* BP friction in moving media */	
				kd= sb->mediafrict* eval_sb_fric_force_scale;  
				bp->force[0] -= kd * (bp->vec[0] + windfactor*speed[0]/eval_sb_fric_force_scale);
				bp->force[1] -= kd * (bp->vec[1] + windfactor*speed[1]/eval_sb_fric_force_scale);
				bp->force[2] -= kd * (bp->vec[2] + windfactor*speed[2]/eval_sb_fric_force_scale);
				/* now we'll have nice centrifugal effect for vortex */
				
			}
			else {
				/* BP friction in media (not) moving*/
				float kd = sb->mediafrict* sb_fric_force_scale(ob);  
				/* assume it to be proportional to actual velocity */
				bp->force[0]-= bp->vec[0]*kd;
				bp->force[1]-= bp->vec[1]*kd;
				bp->force[2]-= bp->vec[2]*kd;
				/* friction in media done */
			}
			/* +++cached collision targets */
			bp->choke = 0.0f;
			bp->choke2 = 0.0f;
			bp->flag &= ~SBF_DOFUZZY;
			if(do_deflector) {
				float cfforce[3],defforce[3] ={0.0f,0.0f,0.0f}, vel[3] = {0.0f,0.0f,0.0f}, facenormal[3], cf = 1.0f,intrusion;
				float kd = 1.0f;

				if (sb_deflect_face(ob,bp->pos,facenormal,defforce,&cf,timenow,vel,&intrusion)){
						if (intrusion < 0.0f){
							sb->scratch->flag |= SBF_DOFUZZY;
							bp->flag |= SBF_DOFUZZY;
							bp->choke = sb->choke*0.01f;
						}

							VECSUB(cfforce,bp->vec,vel);
							Vec3PlusStVec(bp->force,-cf*50.0f,cfforce);
					
					Vec3PlusStVec(bp->force,kd,defforce);  
				}

			}
			/* ---cached collision targets */

			/* +++springs */
			iks  = 1.0f/(1.0f-sb->inspring)-1.0f ;/* inner spring constants function */
			if(ob->softflag & OB_SB_EDGES) {
				if (sb->bspring){ /* spring list exists at all ? */
					int b;
					BodySpring *bs;	
					for(b=bp->nofsprings;b>0;b--){
						bs = sb->bspring + bp->springs[b-1];
						if (do_springcollision || do_aero){
							add_v3_v3v3(bp->force,bp->force,bs->ext_force);
							if (bs->flag & BSF_INTERSECT)
								bp->choke = bs->cf; 

						}
                        // sb_spring_force(Object *ob,int bpi,BodySpring *bs,float iks,float forcetime,int nl_flags)
						sb_spring_force(ob,ilast-bb,bs,iks,forcetime,0);
					}/* loop springs */
				}/* existing spring list */ 
			}/*any edges*/
			/* ---springs */
		}/*omit on snap	*/
	}/*loop all bp's*/
return 0; /*done fine*/
}

static void *exec_softbody_calc_forces(void *data)
{
	SB_thread_context *pctx = (SB_thread_context*)data;
    _softbody_calc_forces_slice_in_a_thread(pctx->scene, pctx->ob, pctx->forcetime, pctx->timenow, pctx->ifirst, pctx->ilast, NULL, pctx->do_effector,pctx->do_deflector,pctx->fieldfactor,pctx->windfactor);
	return 0;
} 

static void sb_cf_threads_run(Scene *scene, Object *ob, float forcetime, float timenow,int totpoint,int *ptr_to_break_func(),struct ListBase *do_effector,int do_deflector,float fieldfactor, float windfactor)
{
	ListBase threads;
	SB_thread_context *sb_threads;
	int i, totthread,left,dec;
	int lowpoints =100; /* wild guess .. may increase with better thread management 'above' or even be UI option sb->spawn_cf_threads_nopts */

	/* figure the number of threads while preventing pretty pointless threading overhead */
	if(scene->r.mode & R_FIXED_THREADS)
		totthread= scene->r.threads;
	else
		totthread= BLI_system_thread_count();
	/* what if we got zillions of CPUs running but less to spread*/
	while ((totpoint/totthread < lowpoints) && (totthread > 1)) {
		totthread--;
	}

    /* printf("sb_cf_threads_run spawning %d threads \n",totthread); */

	sb_threads= MEM_callocN(sizeof(SB_thread_context)*totthread, "SBThread");
	memset(sb_threads, 0, sizeof(SB_thread_context)*totthread);
	left = totpoint;
	dec = totpoint/totthread +1;
	for(i=0; i<totthread; i++) {
		sb_threads[i].scene = scene;
		sb_threads[i].ob = ob; 
		sb_threads[i].forcetime = forcetime; 
		sb_threads[i].timenow = timenow; 
		sb_threads[i].ilast   = left; 
		left = left - dec;
		if (left >0){
			sb_threads[i].ifirst  = left;
		}
		else
			sb_threads[i].ifirst  = 0; 
        sb_threads[i].do_effector = do_effector;
        sb_threads[i].do_deflector = do_deflector;
		sb_threads[i].fieldfactor = fieldfactor;
		sb_threads[i].windfactor  = windfactor;
		sb_threads[i].nr= i;
		sb_threads[i].tot= totthread;
	}


	if(totthread > 1) {
		BLI_init_threads(&threads, exec_softbody_calc_forces, totthread);

		for(i=0; i<totthread; i++)
			BLI_insert_thread(&threads, &sb_threads[i]);

		BLI_end_threads(&threads);
	}
	else
		exec_softbody_calc_forces(&sb_threads[0]);
    /* clean up */
	MEM_freeN(sb_threads);
}

static void softbody_calc_forcesEx(Scene *scene, Object *ob, float forcetime, float timenow, int nl_flags)
{
/* rule we never alter free variables :bp->vec bp->pos in here ! 
 * this will ruin adaptive stepsize AKA heun! (BM) 
 */
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bproot;
	ListBase *do_effector = NULL;
	float iks, gravity;
	float fieldfactor = -1.0f, windfactor  = 0.25;   
	int   do_deflector,do_selfcollision,do_springcollision,do_aero;
	
	gravity = sb->grav * sb_grav_force_scale(ob);	
	
	/* check conditions for various options */
	do_deflector= query_external_colliders(scene, ob);
	do_selfcollision=((ob->softflag & OB_SB_EDGES) && (sb->bspring)&& (ob->softflag & OB_SB_SELF));
	do_springcollision=do_deflector && (ob->softflag & OB_SB_EDGES) &&(ob->softflag & OB_SB_EDGECOLL);
	do_aero=((sb->aeroedge)&& (ob->softflag & OB_SB_EDGES));
	
	iks  = 1.0f/(1.0f-sb->inspring)-1.0f ;/* inner spring constants function */
	bproot= sb->bpoint; /* need this for proper spring addressing */
	
	if (do_springcollision || do_aero)  
	sb_sfesf_threads_run(scene, ob, timenow,sb->totspring,NULL);	
	
	/* after spring scan because it uses Effoctors too */
	do_effector= pdInitEffectors(scene, ob, NULL, sb->effector_weights);

	if (do_deflector) {
		float defforce[3];
		do_deflector = sb_detect_aabb_collisionCached(defforce,ob->lay,ob,timenow);
	}

	sb_cf_threads_run(scene, ob, forcetime, timenow, sb->totpoint, NULL, do_effector, do_deflector, fieldfactor, windfactor);

	/* finally add forces caused by face collision */
	if (ob->softflag & OB_SB_FACECOLL) scan_for_ext_face_forces(ob,timenow);
	
	/* finish matrix and solve */
	pdEndEffectors(&do_effector);
}




static void softbody_calc_forces(Scene *scene, Object *ob, float forcetime, float timenow, int nl_flags)
{
	/* redirection to the new threaded Version */
	if (!(G.rt & 0x10)){ // 16
		softbody_calc_forcesEx(scene, ob, forcetime, timenow, nl_flags);
		return;
	}
	else{
		/* so the following will die  */
		/* |||||||||||||||||||||||||| */
		/* VVVVVVVVVVVVVVVVVVVVVVVVVV */
		/*backward compatibility note:
		fixing bug [17428] which forces adaptive step size to tiny steps 
		in some situations 
		.. keeping G.rt==17 0x11 option for old files 'needing' the bug*/

		/* rule we never alter free variables :bp->vec bp->pos in here ! 
		* this will ruin adaptive stepsize AKA heun! (BM) 
		*/
		SoftBody *sb= ob->soft;	/* is supposed to be there */
		BodyPoint  *bp;
		BodyPoint *bproot;
		BodySpring *bs;	
		ListBase *do_effector = NULL;
		float iks, ks, kd, gravity[3] = {0.0f,0.0f,0.0f};
		float fieldfactor = -1.0f, windfactor  = 0.25f;   
		float tune = sb->ballstiff;
		int a, b,  do_deflector,do_selfcollision,do_springcollision,do_aero;


		/* jacobian
		NLboolean success;

		if(nl_flags){
		nlBegin(NL_SYSTEM);
		nlBegin(NL_MATRIX);
		}
		*/


		if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY){ 
			VECCOPY(gravity, scene->physics_settings.gravity);
			mul_v3_fl(gravity, sb_grav_force_scale(ob)*sb->effector_weights->global_gravity);
		}	

		/* check conditions for various options */
		do_deflector= query_external_colliders(scene, ob);
		do_selfcollision=((ob->softflag & OB_SB_EDGES) && (sb->bspring)&& (ob->softflag & OB_SB_SELF));
		do_springcollision=do_deflector && (ob->softflag & OB_SB_EDGES) &&(ob->softflag & OB_SB_EDGECOLL);
		do_aero=((sb->aeroedge)&& (ob->softflag & OB_SB_EDGES));

		iks  = 1.0f/(1.0f-sb->inspring)-1.0f ;/* inner spring constants function */
		bproot= sb->bpoint; /* need this for proper spring addressing */

		if (do_springcollision || do_aero)  scan_for_ext_spring_forces(scene, ob, timenow);
		/* after spring scan because it uses Effoctors too */
		do_effector= pdInitEffectors(scene, ob, NULL, ob->soft->effector_weights);

		if (do_deflector) {
			float defforce[3];
			do_deflector = sb_detect_aabb_collisionCached(defforce,ob->lay,ob,timenow);
		}

		for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
			/* clear forces  accumulator */
			bp->force[0]= bp->force[1]= bp->force[2]= 0.0;
			if(nl_flags & NLF_BUILD){
				//int ia =3*(sb->totpoint-a);
				//int op =3*sb->totpoint;
				/* dF/dV = v */ 
				/* jacobioan
				nlMatrixAdd(op+ia,ia,-forcetime);
				nlMatrixAdd(op+ia+1,ia+1,-forcetime);
				nlMatrixAdd(op+ia+2,ia+2,-forcetime);

				nlMatrixAdd(ia,ia,1);
				nlMatrixAdd(ia+1,ia+1,1);
				nlMatrixAdd(ia+2,ia+2,1);

				nlMatrixAdd(op+ia,op+ia,1);
				nlMatrixAdd(op+ia+1,op+ia+1,1);
				nlMatrixAdd(op+ia+2,op+ia+2,1);
				*/


			}

			/* naive ball self collision */
			/* needs to be done if goal snaps or not */
			if(do_selfcollision){
				int attached;
				BodyPoint   *obp;
				int c,b;
				float velcenter[3],dvel[3],def[3];
				float distance;
				float compare;

				for(c=sb->totpoint, obp= sb->bpoint; c>=a; c--, obp++) {

					//if ((bp->octantflag & obp->octantflag) == 0) continue;

					compare = (obp->colball + bp->colball);		
					sub_v3_v3v3(def, bp->pos, obp->pos);

					/* rather check the AABBoxes before ever calulating the real distance */
					/* mathematically it is completly nuts, but performace is pretty much (3) times faster */
					if ((ABS(def[0]) > compare) || (ABS(def[1]) > compare) || (ABS(def[2]) > compare)) continue;

					distance = normalize_v3(def);
					if (distance < compare ){
						/* exclude body points attached with a spring */
						attached = 0;
						for(b=obp->nofsprings;b>0;b--){
							bs = sb->bspring + obp->springs[b-1];
							if (( sb->totpoint-a == bs->v2)  || ( sb->totpoint-a == bs->v1)){
								attached=1;
								continue;}
						}
						if (!attached){
							float f = tune/(distance) + tune/(compare*compare)*distance - 2.0f*tune/compare ;

							mid_v3_v3v3(velcenter, bp->vec, obp->vec);
							sub_v3_v3v3(dvel,velcenter,bp->vec);
							mul_v3_fl(dvel,bp->mass);

							Vec3PlusStVec(bp->force,f*(1.0f-sb->balldamp),def);
							Vec3PlusStVec(bp->force,sb->balldamp,dvel);

							if(nl_flags & NLF_BUILD){
								//int ia =3*(sb->totpoint-a);
								//int ic =3*(sb->totpoint-c);
								//int op =3*sb->totpoint;
								//float mvel = forcetime*sb->nodemass*sb->balldamp;
								//float mpos = forcetime*tune*(1.0f-sb->balldamp);
								/*some quick and dirty entries to the jacobian*/
								//dfdx_goal(ia,ia,op,mpos);
								//dfdv_goal(ia,ia,mvel);
								/* exploit force(a,b) == -force(b,a) part1/2 */
								//dfdx_goal(ic,ic,op,mpos);
								//dfdv_goal(ic,ic,mvel);


								/*TODO sit down an X-out the true jacobian entries*/
								/*well does not make to much sense because the eigenvalues
								of the jacobian go negative; and negative eigenvalues
								on a complex iterative system z(n+1)=A * z(n) 
								give imaginary roots in the charcateristic polynom
								--> solutions that to z(t)=u(t)* exp ( i omega t) --> oscilations we don't want here 
								where u(t) is a unknown amplitude function (worst case rising fast)
								*/ 
							}

							/* exploit force(a,b) == -force(b,a) part2/2 */
							sub_v3_v3v3(dvel,velcenter,obp->vec);
							mul_v3_fl(dvel,(bp->mass+obp->mass)/2.0f);

							Vec3PlusStVec(obp->force,sb->balldamp,dvel);
							Vec3PlusStVec(obp->force,-f*(1.0f-sb->balldamp),def);


						}
					}
				}
			}
			/* naive ball self collision done */

			if(bp->goal < SOFTGOALSNAP){ /* ommit this bp when it snaps */
				float auxvect[3];  
				float velgoal[3];

				/* do goal stuff */
				if(ob->softflag & OB_SB_GOAL) {
					/* true elastic goal */
					sub_v3_v3v3(auxvect,bp->pos,bp->origT);
					ks  = 1.0f/(1.0f- bp->goal*sb->goalspring)-1.0f ;
					bp->force[0]+= -ks*(auxvect[0]);
					bp->force[1]+= -ks*(auxvect[1]);
					bp->force[2]+= -ks*(auxvect[2]);

					if(nl_flags & NLF_BUILD){
						//int ia =3*(sb->totpoint-a);
						//int op =3*(sb->totpoint);
						/* depending on my pos */ 
						//dfdx_goal(ia,ia,op,ks*forcetime);
					}


					/* calulate damping forces generated by goals*/
					sub_v3_v3v3(velgoal,bp->origS, bp->origE);
					kd =  sb->goalfrict * sb_fric_force_scale(ob) ;
					add_v3_v3v3(auxvect,velgoal,bp->vec);

					if (forcetime > 0.0 ) { /* make sure friction does not become rocket motor on time reversal */
						bp->force[0]-= kd * (auxvect[0]);
						bp->force[1]-= kd * (auxvect[1]);
						bp->force[2]-= kd * (auxvect[2]);
						if(nl_flags & NLF_BUILD){
							//int ia =3*(sb->totpoint-a);
							normalize_v3(auxvect);
							/* depending on my vel */ 
							//dfdv_goal(ia,ia,kd*forcetime);
						}

					}
					else {
						bp->force[0]-= kd * (velgoal[0] - bp->vec[0]);
						bp->force[1]-= kd * (velgoal[1] - bp->vec[1]);
						bp->force[2]-= kd * (velgoal[2] - bp->vec[2]);
					}
				}
				/* done goal stuff */


				/* gravitation */
				VECADDFAC(bp->force, bp->force, gravity, bp->mass); /* individual mass of node here */


				/* particle field & vortex */
				if(do_effector) {
					EffectedPoint epoint;
					float force[3]= {0.0f, 0.0f, 0.0f};
					float speed[3]= {0.0f, 0.0f, 0.0f};
					float eval_sb_fric_force_scale = sb_fric_force_scale(ob); /* just for calling function once */
					pd_point_from_soft(scene, bp->pos, bp->vec, sb->bpoint-bp, &epoint);
					pdDoEffectors(do_effector, NULL, sb->effector_weights, &epoint, force, speed);

					/* apply forcefield*/
					mul_v3_fl(force,fieldfactor* eval_sb_fric_force_scale); 
					VECADD(bp->force, bp->force, force);

					/* BP friction in moving media */	
					kd= sb->mediafrict* eval_sb_fric_force_scale;  
					bp->force[0] -= kd * (bp->vec[0] + windfactor*speed[0]/eval_sb_fric_force_scale);
					bp->force[1] -= kd * (bp->vec[1] + windfactor*speed[1]/eval_sb_fric_force_scale);
					bp->force[2] -= kd * (bp->vec[2] + windfactor*speed[2]/eval_sb_fric_force_scale);
					/* now we'll have nice centrifugal effect for vortex */

				}
				else {
					/* BP friction in media (not) moving*/
					kd= sb->mediafrict* sb_fric_force_scale(ob);  
					/* assume it to be proportional to actual velocity */
					bp->force[0]-= bp->vec[0]*kd;
					bp->force[1]-= bp->vec[1]*kd;
					bp->force[2]-= bp->vec[2]*kd;
					/* friction in media done */
					if(nl_flags & NLF_BUILD){
						//int ia =3*(sb->totpoint-a);
						/* da/dv =  */ 

						//					nlMatrixAdd(ia,ia,forcetime*kd);
						//					nlMatrixAdd(ia+1,ia+1,forcetime*kd);
						//					nlMatrixAdd(ia+2,ia+2,forcetime*kd);
					}

				}
				/* +++cached collision targets */
				bp->choke = 0.0f;
				bp->choke2 = 0.0f;
				bp->flag &= ~SBF_DOFUZZY;
				if(do_deflector) {
					float cfforce[3],defforce[3] ={0.0f,0.0f,0.0f}, vel[3] = {0.0f,0.0f,0.0f}, facenormal[3], cf = 1.0f,intrusion;
					kd = 1.0f;

					if (sb_deflect_face(ob,bp->pos,facenormal,defforce,&cf,timenow,vel,&intrusion)){
						if ((!nl_flags)&&(intrusion < 0.0f)){
							if(G.rt & 0x01){ // 17 we did check for bit 0x10 before
								/*fixing bug [17428] this forces adaptive step size to tiny steps 
								in some situations .. keeping G.rt==17 option for old files 'needing' the bug
								*/
								/*bjornmose:  uugh.. what an evil hack 
								violation of the 'don't touch bp->pos in here' rule 
								but works nice, like this-->
								we predict the solution beeing out of the collider
								in heun step No1 and leave the heun step No2 adapt to it
								so we kind of introduced a implicit solver for this case 
								*/
								Vec3PlusStVec(bp->pos,-intrusion,facenormal);
							}
							else{

								VECSUB(cfforce,bp->vec,vel);
								Vec3PlusStVec(bp->force,-cf*50.0f,cfforce);
							}


							sb->scratch->flag |= SBF_DOFUZZY;
							bp->flag |= SBF_DOFUZZY;
							bp->choke = sb->choke*0.01f;
						}
						else{
							VECSUB(cfforce,bp->vec,vel);
							Vec3PlusStVec(bp->force,-cf*50.0f,cfforce);
						}
						Vec3PlusStVec(bp->force,kd,defforce);
						if (nl_flags & NLF_BUILD){
							// int ia =3*(sb->totpoint-a);
							// int op =3*sb->totpoint;
							//dfdx_goal(ia,ia,op,mpos); // don't do unless you know
							//dfdv_goal(ia,ia,-cf);

						}

					}

				}
				/* ---cached collision targets */

				/* +++springs */
				if(ob->softflag & OB_SB_EDGES) {
					if (sb->bspring){ /* spring list exists at all ? */
						for(b=bp->nofsprings;b>0;b--){
							bs = sb->bspring + bp->springs[b-1];
							if (do_springcollision || do_aero){
								add_v3_v3v3(bp->force,bp->force,bs->ext_force);
								if (bs->flag & BSF_INTERSECT)
									bp->choke = bs->cf; 

							}
							// sb_spring_force(Object *ob,int bpi,BodySpring *bs,float iks,float forcetime,int nl_flags)
							// rather remove nl_falgs from code .. will make things a lot cleaner
							sb_spring_force(ob,sb->totpoint-a,bs,iks,forcetime,0);
						}/* loop springs */
					}/* existing spring list */ 
				}/*any edges*/
				/* ---springs */
			}/*omit on snap	*/
		}/*loop all bp's*/


		/* finally add forces caused by face collision */
		if (ob->softflag & OB_SB_FACECOLL) scan_for_ext_face_forces(ob,timenow);

		/* finish matrix and solve */
#if (0) // remove onl linking for now .. still i am not sure .. the jacobian can be usefull .. so keep that BM
		if(nl_flags & NLF_SOLVE){
			//double sct,sst=PIL_check_seconds_timer();
			for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
				int iv =3*(sb->totpoint-a);
				int ip =3*(2*sb->totpoint-a);
				int n;
				for (n=0;n<3;n++) {nlRightHandSideSet(0, iv+n, bp->force[0+n]);}
				for (n=0;n<3;n++) {nlRightHandSideSet(0, ip+n, bp->vec[0+n]);}
			}
			nlEnd(NL_MATRIX);
			nlEnd(NL_SYSTEM);

			if ((G.rt == 32) && (nl_flags & NLF_BUILD))
			{ 
				printf("####MEE#####\n");
				nlPrintMatrix();
			}

			success= nlSolveAdvanced(NULL, 1);

			// nlPrintMatrix(); /* for debug purpose .. anyhow cropping B vector looks like working */
			if(success){
				float f;
				int index =0;
				/* for debug purpose .. anyhow cropping B vector looks like working */
				if (G.rt ==32)
					for(a=2*sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
						f=nlGetVariable(0,index);
						printf("(%f ",f);index++;
						f=nlGetVariable(0,index);
						printf("%f ",f);index++;
						f=nlGetVariable(0,index);
						printf("%f)",f);index++;
					}

					index =0;
					for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
						f=nlGetVariable(0,index);
						bp->impdv[0] = f; index++;
						f=nlGetVariable(0,index);
						bp->impdv[1] = f; index++;
						f=nlGetVariable(0,index);
						bp->impdv[2] = f; index++;
					}
					/*
					for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
					f=nlGetVariable(0,index);
					bp->impdx[0] = f; index++;
					f=nlGetVariable(0,index);
					bp->impdx[1] = f; index++;
					f=nlGetVariable(0,index);
					bp->impdx[2] = f; index++;
					}
					*/
			}
			else{
				printf("Matrix inversion failed \n");
				for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
					VECCOPY(bp->impdv,bp->force);
				}

			}

			//sct=PIL_check_seconds_timer();
			//if (sct-sst > 0.01f) printf(" implicit solver time %f %s \r",sct-sst,ob->id.name);
		}
		/* cleanup */
#endif
		pdEndEffectors(&do_effector);
	}
}

static void softbody_apply_forces(Object *ob, float forcetime, int mode, float *err, int mid_flags)
{
	/* time evolution */
	/* actually does an explicit euler step mode == 0 */
	/* or heun ~ 2nd order runge-kutta steps, mode 1,2 */
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp;
	float dx[3],dv[3],aabbmin[3],aabbmax[3],cm[3]={0.0f,0.0f,0.0f};
	float timeovermass/*,freezeloc=0.00001f,freezeforce=0.00000000001f*/;
	float maxerrpos= 0.0f,maxerrvel = 0.0f;
	int a,fuzzy=0;

    forcetime *= sb_time_scale(ob);
    
    aabbmin[0]=aabbmin[1]=aabbmin[2] = 1e20f;
    aabbmax[0]=aabbmax[1]=aabbmax[2] = -1e20f;

    /* old one with homogenous masses  */
	/* claim a minimum mass for vertex */
	/*
	if (sb->nodemass > 0.009999f) timeovermass = forcetime/sb->nodemass;
	else timeovermass = forcetime/0.009999f;
	*/
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
/* now we have individual masses   */
/* claim a minimum mass for vertex */
		if (bp->mass > 0.009999f) timeovermass = forcetime/bp->mass;
  	    else timeovermass = forcetime/0.009999f;


		if(bp->goal < SOFTGOALSNAP){
            /* this makes t~ = t */
			if(mid_flags & MID_PRESERVE) VECCOPY(dx,bp->vec);
			
			/* so here is (v)' = a(cceleration) = sum(F_springs)/m + gravitation + some friction forces  + more forces*/
			/* the ( ... )' operator denotes derivate respective time */
			/* the euler step for velocity then becomes */
			/* v(t + dt) = v(t) + a(t) * dt */ 
			mul_v3_fl(bp->force,timeovermass);/* individual mass of node here */
			/* some nasty if's to have heun in here too */
			VECCOPY(dv,bp->force); 

			if (mode == 1){
				VECCOPY(bp->prevvec, bp->vec);
				VECCOPY(bp->prevdv, dv);
			}

			if (mode ==2){
				/* be optimistic and execute step */
				bp->vec[0] = bp->prevvec[0] + 0.5f * (dv[0] + bp->prevdv[0]);
				bp->vec[1] = bp->prevvec[1] + 0.5f * (dv[1] + bp->prevdv[1]);
				bp->vec[2] = bp->prevvec[2] + 0.5f * (dv[2] + bp->prevdv[2]);
				/* compare euler to heun to estimate error for step sizing */
				maxerrvel = MAX2(maxerrvel,ABS(dv[0] - bp->prevdv[0]));
				maxerrvel = MAX2(maxerrvel,ABS(dv[1] - bp->prevdv[1]));
				maxerrvel = MAX2(maxerrvel,ABS(dv[2] - bp->prevdv[2]));
			}
			else {VECADD(bp->vec, bp->vec, bp->force);}

            /* this makes t~ = t+dt */
			if(!(mid_flags & MID_PRESERVE)) VECCOPY(dx,bp->vec);

			/* so here is (x)'= v(elocity) */
			/* the euler step for location then becomes */
			/* x(t + dt) = x(t) + v(t~) * dt */ 
			mul_v3_fl(dx,forcetime);

			/* the freezer coming sooner or later */
			/*
			if  ((dot_v3v3(dx,dx)<freezeloc )&&(dot_v3v3(bp->force,bp->force)<freezeforce )){
				bp->frozen /=2;
			}
			else{
				bp->frozen =MIN2(bp->frozen*1.05f,1.0f);
			}
			mul_v3_fl(dx,bp->frozen);
            */
			/* again some nasty if's to have heun in here too */
			if (mode ==1){
				VECCOPY(bp->prevpos,bp->pos);
				VECCOPY(bp->prevdx ,dx);
			}
			
			if (mode ==2){
				bp->pos[0] = bp->prevpos[0] + 0.5f * ( dx[0] + bp->prevdx[0]);
				bp->pos[1] = bp->prevpos[1] + 0.5f * ( dx[1] + bp->prevdx[1]);
				bp->pos[2] = bp->prevpos[2] + 0.5f * ( dx[2] + bp->prevdx[2]);
				maxerrpos = MAX2(maxerrpos,ABS(dx[0] - bp->prevdx[0]));
				maxerrpos = MAX2(maxerrpos,ABS(dx[1] - bp->prevdx[1]));
				maxerrpos = MAX2(maxerrpos,ABS(dx[2] - bp->prevdx[2]));

/* bp->choke is set when we need to pull a vertex or edge out of the collider. 
   the collider object signals to get out by pushing hard. on the other hand 
   we don't want to end up in deep space so we add some <viscosity> 
   to balance that out */
				if (bp->choke2 > 0.0f){
					mul_v3_fl(bp->vec,(1.0f - bp->choke2));
				}
				if (bp->choke > 0.0f){
					mul_v3_fl(bp->vec,(1.0f - bp->choke));
				}

			}
			else { VECADD(bp->pos, bp->pos, dx);}
		}/*snap*/
		/* so while we are looping BPs anyway do statistics on the fly */
		aabbmin[0] = MIN2(aabbmin[0],bp->pos[0]);
		aabbmin[1] = MIN2(aabbmin[1],bp->pos[1]);
		aabbmin[2] = MIN2(aabbmin[2],bp->pos[2]);
		aabbmax[0] = MAX2(aabbmax[0],bp->pos[0]);
		aabbmax[1] = MAX2(aabbmax[1],bp->pos[1]);
		aabbmax[2] = MAX2(aabbmax[2],bp->pos[2]);
		if (bp->flag & SBF_DOFUZZY) fuzzy =1;
	} /*for*/

	if (sb->totpoint) mul_v3_fl(cm,1.0f/sb->totpoint);
	if (sb->scratch){
		VECCOPY(sb->scratch->aabbmin,aabbmin);
		VECCOPY(sb->scratch->aabbmax,aabbmax);
	}
	
	if (err){ /* so step size will be controlled by biggest difference in slope */
		if (sb->solverflags & SBSO_OLDERR)
		*err = MAX2(maxerrpos,maxerrvel);
		else
		*err = maxerrpos;
		//printf("EP %f EV %f \n",maxerrpos,maxerrvel);
		if (fuzzy){
			*err /= sb->fuzzyness;
		}
	}
}

/* used by heun when it overshoots */
static void softbody_restore_prev_step(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there*/
	BodyPoint *bp;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		VECCOPY(bp->vec, bp->prevvec);
		VECCOPY(bp->pos, bp->prevpos);
	}
}

#if 0
static void softbody_store_step(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there*/
	BodyPoint *bp;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		VECCOPY(bp->prevvec,bp->vec);
		VECCOPY(bp->prevpos,bp->pos);
	}
}


/* used by predictors and correctors */
static void softbody_store_state(Object *ob,float *ppos,float *pvel)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there*/
	BodyPoint *bp;
	int a;
	float *pp=ppos,*pv=pvel;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		
		VECCOPY(pv, bp->vec); 
		pv+=3;
		
		VECCOPY(pp, bp->pos);
		pp+=3;
	}
}

/* used by predictors and correctors */
static void softbody_retrieve_state(Object *ob,float *ppos,float *pvel)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there*/
	BodyPoint *bp;
	int a;
	float *pp=ppos,*pv=pvel;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		
		VECCOPY(bp->vec,pv); 
		pv+=3;
		
		VECCOPY(bp->pos,pp);
		pp+=3;
	}
}

/* used by predictors and correctors */
static void softbody_swap_state(Object *ob,float *ppos,float *pvel)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there*/
	BodyPoint *bp;
	int a;
	float *pp=ppos,*pv=pvel;
	float temp[3];
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		
		VECCOPY(temp, bp->vec); 
		VECCOPY(bp->vec,pv); 
        VECCOPY(pv,temp); 
		pv+=3;
		
		VECCOPY(temp, bp->pos);
		VECCOPY(bp->pos,pp); 
        VECCOPY(pp,temp); 
		pp+=3;
	}
}
#endif


/* care for bodypoints taken out of the 'ordinary' solver step
** because they are screwed to goal by bolts
** they just need to move along with the goal in time 
** we need to adjust them on sub frame timing in solver 
** so now when frame is done .. put 'em to the position at the end of frame
*/
static void softbody_apply_goalsnap(Object *ob)
{
	SoftBody *sb= ob->soft;	/* is supposed to be there */
	BodyPoint *bp;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		if (bp->goal >= SOFTGOALSNAP){
			VECCOPY(bp->prevpos,bp->pos);
			VECCOPY(bp->pos,bp->origT);
		}		
	}
}


static void apply_spring_memory(Object *ob)
{
	SoftBody *sb = ob->soft;
	BodySpring *bs;
	BodyPoint *bp1, *bp2;
	int a;
	float b,l,r;

	if (sb && sb->totspring){
		b = sb->plastic;
		for(a=0; a<sb->totspring; a++) {
			bs  = &sb->bspring[a];
			bp1 =&sb->bpoint[bs->v1];
			bp2 =&sb->bpoint[bs->v2];
			l = len_v3v3(bp1->pos,bp2->pos);
			r = bs->len/l;
			if (( r > 1.05f) || (r < 0.95)){
			bs->len = ((100.0f - b) * bs->len  + b*l)/100.0f;
			}
		}
	}
}

/* expects full initialized softbody */
static void interpolate_exciter(Object *ob, int timescale, int time)
{
	SoftBody *sb= ob->soft;
	BodyPoint *bp;
	float f;
	int a;
	
	f = (float)time/(float)timescale;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {	
		bp->origT[0] = bp->origS[0] + f*(bp->origE[0] - bp->origS[0]); 
		bp->origT[1] = bp->origS[1] + f*(bp->origE[1] - bp->origS[1]); 
		bp->origT[2] = bp->origS[2] + f*(bp->origE[2] - bp->origS[2]); 
		if (bp->goal >= SOFTGOALSNAP){
			bp->vec[0] = bp->origE[0] - bp->origS[0];
			bp->vec[1] = bp->origE[1] - bp->origS[1];
			bp->vec[2] = bp->origE[2] - bp->origS[2];
		}
	}
	
}


/* ************ convertors ********** */

/*  for each object type we need;
    - xxxx_to_softbody(Object *ob)      : a full (new) copy, creates SB geometry
*/

static void get_scalar_from_vertexgroup(Object *ob, int vertID, short groupindex, float *target)
/* result 0 on success, else indicates error number
-- kind of *inverse* result defintion,
-- but this way we can signal error condition to caller  
-- and yes this function must not be here but in a *vertex group module*
*/
{
	MDeformVert *dv= NULL;
	int i;
	
	/* spot the vert in deform vert list at mesh */
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		if (me->dvert)
			dv = me->dvert + vertID;
	}
	else if(ob->type==OB_LATTICE) {	/* not yet supported in softbody btw */
		Lattice *lt= ob->data;
		if (lt->dvert)
			dv = lt->dvert + vertID;
	}
	if(dv) {
		/* Lets see if this vert is in the weight group */
		for (i=0; i<dv->totweight; i++){
			if (dv->dw[i].def_nr == groupindex){
				*target= dv->dw[i].weight; /* got it ! */
				break;
			}
		}
	}
} 

/* Resetting a Mesh SB object's springs */  
/* Spring lenght are caculted from'raw' mesh vertices that are NOT altered by modifier stack. */ 
static void springs_from_mesh(Object *ob)
{
	SoftBody *sb;
	Mesh *me= ob->data;
	BodyPoint *bp;
	int a;
	float scale =1.0f;
	
	sb= ob->soft;	
	if (me && sb)
	{ 
	/* using bp->origS as a container for spring calcualtions here
	** will be overwritten sbObjectStep() to receive 
	** actual modifier stack positions
	*/
		if(me->totvert) {    
			bp= ob->soft->bpoint;
			for(a=0; a<me->totvert; a++, bp++) {
				VECCOPY(bp->origS, me->mvert[a].co);                            
				mul_m4_v3(ob->obmat, bp->origS);
			}
			
		}
		/* recalculate spring length for meshes here */
		/* public version shrink to fit */
		if (sb->springpreload != 0 ){
			scale = sb->springpreload / 100.0f;
		}
		for(a=0; a<sb->totspring; a++) {
			BodySpring *bs = &sb->bspring[a];
			bs->len= scale*len_v3v3(sb->bpoint[bs->v1].origS, sb->bpoint[bs->v2].origS);
		}
	}
}


/* makes totally fresh start situation */
static void mesh_to_softbody(Scene *scene, Object *ob)
{
	SoftBody *sb;
	Mesh *me= ob->data;
	MEdge *medge= me->medge;
	BodyPoint *bp;
	BodySpring *bs;
	float goalfac;
	int a, totedge;
	if (ob->softflag & OB_SB_EDGES) totedge= me->totedge;
	else totedge= 0;
	
	/* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
	renew_softbody(scene, ob, me->totvert, totedge);
		
	/* we always make body points */
	sb= ob->soft;	
	bp= sb->bpoint;
	goalfac= ABS(sb->maxgoal - sb->mingoal);
	
	for(a=0; a<me->totvert; a++, bp++) {
		/* get scalar values needed  *per vertex* from vertex group functions,
		so we can *paint* them nicly .. 
		they are normalized [0.0..1.0] so may be we need amplitude for scale
		which can be done by caller but still .. i'd like it to go this way 
		*/ 
		
		if((ob->softflag & OB_SB_GOAL) && sb->vertgroup) {
			get_scalar_from_vertexgroup(ob, a,(short) (sb->vertgroup-1), &bp->goal);
			/* do this always, regardless successfull read from vertex group */
			bp->goal= sb->mingoal + bp->goal*goalfac;
		}
		/* a little ad hoc changing the goal control to be less *sharp* */
		bp->goal = (float)pow(bp->goal, 4.0f);
			
		/* to proove the concept
		this would enable per vertex *mass painting*
		*/
		/* first set the default */
		bp->mass = sb->nodemass;

		if (sb->namedVG_Mass[0])
		{
			int grp= get_named_vertexgroup_num (ob,sb->namedVG_Mass);
			/* printf("VGN  %s %d \n",sb->namedVG_Mass,grp); */
			if(grp > -1){
				get_scalar_from_vertexgroup(ob, a,(short) (grp), &bp->mass);
				bp->mass = bp->mass * sb->nodemass;
				/* printf("bp->mass  %f \n",bp->mass); */

			}
		}
		/* first set the default */
		bp->springweight = 1.0f;

		if (sb->namedVG_Spring_K[0])
		{
			int grp= get_named_vertexgroup_num (ob,sb->namedVG_Spring_K);
			//printf("VGN  %s %d \n",sb->namedVG_Spring_K,grp); 
			if(grp > -1){
				get_scalar_from_vertexgroup(ob, a,(short) (grp), &bp->springweight);
				//printf("bp->springweight  %f \n",bp->springweight);

			}
		}

		
	}

	/* but we only optionally add body edge springs */
	if (ob->softflag & OB_SB_EDGES) {
		if(medge) {
			bs= sb->bspring;
			for(a=me->totedge; a>0; a--, medge++, bs++) {
				bs->v1= medge->v1;
				bs->v2= medge->v2;
				bs->springtype=SB_EDGE;
			}
			
			
			/* insert *diagonal* springs in quads if desired */
			if (ob->softflag & OB_SB_QUADS) {
				add_mesh_quad_diag_springs(ob);
			}
			
			build_bps_springlist(ob); /* scan for springs attached to bodypoints ONCE */
			/* insert *other second order* springs if desired */
			if (sb->secondspring > 0.0000001f) {
				add_2nd_order_springs(ob,sb->secondspring); /* exploits the the first run of build_bps_springlist(ob);*/
				build_bps_springlist(ob); /* yes we need to do it again*/
			}
			springs_from_mesh(ob); /* write the 'rest'-lenght of the springs */
           	if (ob->softflag & OB_SB_SELF) {calculate_collision_balls(ob);}
			
		}
		
	}

}

static void mesh_faces_to_scratch(Object *ob)
{
	SoftBody *sb= ob->soft;	
	Mesh *me= ob->data;
	MFace *mface;
	BodyFace *bodyface;
	int a;
	/* alloc and copy faces*/
	
	bodyface = sb->scratch->bodyface = MEM_mallocN(sizeof(BodyFace)*me->totface,"SB_body_Faces");
	//memcpy(sb->scratch->mface,me->mface,sizeof(MFace)*me->totface);
	mface = me->mface;
	for(a=0; a<me->totface; a++, mface++, bodyface++) {
		bodyface->v1 = mface->v1;
		bodyface->v2 = mface->v2;
		bodyface->v3 = mface->v3;
		bodyface->v4 = mface->v4;
		bodyface->ext_force[0] = bodyface->ext_force[1] = bodyface->ext_force[2] = 0.0f;
		bodyface->flag =0;								
	}
	sb->scratch->totface = me->totface;
}
static void reference_to_scratch(Object *ob)
{
	SoftBody *sb= ob->soft;	
	ReferenceVert *rp;
	BodyPoint     *bp;
	float accu_pos[3] ={0.f,0.f,0.f};
	float accu_mass = 0.f;
	int a;

	sb->scratch->Ref.ivert = MEM_mallocN(sizeof(ReferenceVert)*sb->totpoint,"SB_Reference");
	bp= ob->soft->bpoint;
	rp= sb->scratch->Ref.ivert;
	for(a=0; a<sb->totpoint; a++, rp++, bp++) {
		VECCOPY(rp->pos,bp->pos);
		VECADD(accu_pos,accu_pos,bp->pos);
		accu_mass += bp-> mass;
	}
	mul_v3_fl(accu_pos,1.0f/accu_mass);
	VECCOPY(sb->scratch->Ref.com,accu_pos);
	/* printf("reference_to_scratch \n"); */
}

/*
helper function to get proper spring length 
when object is rescaled
*/
static float globallen(float *v1,float *v2,Object *ob)
{
	float p1[3],p2[3];
	VECCOPY(p1,v1);
	mul_m4_v3(ob->obmat, p1);	
	VECCOPY(p2,v2);
	mul_m4_v3(ob->obmat, p2);
	return len_v3v3(p1,p2);
}

static void makelatticesprings(Lattice *lt,	BodySpring *bs, int dostiff,Object *ob)
{
	BPoint *bp=lt->def, *bpu;
	int u, v, w, dv, dw, bpc=0, bpuc;
	
	dv= lt->pntsu;
	dw= dv*lt->pntsv;
	
	for(w=0; w<lt->pntsw; w++) {
		
		for(v=0; v<lt->pntsv; v++) {
			
			for(u=0, bpuc=0, bpu=NULL; u<lt->pntsu; u++, bp++, bpc++) {
				
				if(w) {
					bs->v1 = bpc;
					bs->v2 = bpc-dw;
					bs->springtype=SB_EDGE;
					bs->len= globallen((bp-dw)->vec, bp->vec,ob);
					bs++;
				}
				if(v) {
					bs->v1 = bpc;
					bs->v2 = bpc-dv;
					bs->springtype=SB_EDGE;
					bs->len= globallen((bp-dv)->vec, bp->vec,ob);
					bs++;
				}
				if(u) {
					bs->v1 = bpuc;
					bs->v2 = bpc;
					bs->springtype=SB_EDGE;
					bs->len= globallen((bpu)->vec, bp->vec,ob);
					bs++;
				}
				
				if (dostiff) {

					if(w){
						if( v && u ) {
							bs->v1 = bpc;
							bs->v2 = bpc-dw-dv-1;
							bs->springtype=SB_BEND;
							bs->len= globallen((bp-dw-dv-1)->vec, bp->vec,ob);
							bs++;
						}						
						if( (v < lt->pntsv-1) && (u) ) {
							bs->v1 = bpc;
							bs->v2 = bpc-dw+dv-1;
							bs->springtype=SB_BEND;
							bs->len= globallen((bp-dw+dv-1)->vec, bp->vec,ob);
							bs++;
						}						
					}

					if(w < lt->pntsw -1){
						if( v && u ) {
							bs->v1 = bpc;
							bs->v2 = bpc+dw-dv-1;
							bs->springtype=SB_BEND;
							bs->len= globallen((bp+dw-dv-1)->vec, bp->vec,ob);
							bs++;
						}						
						if( (v < lt->pntsv-1) && (u) ) {
							bs->v1 = bpc;
							bs->v2 = bpc+dw+dv-1;
							bs->springtype=SB_BEND;
							 bs->len= globallen((bp+dw+dv-1)->vec, bp->vec,ob);
							bs++;
						}						
					}
				}
				bpu = bp;
				bpuc = bpc;
			}
		}
	}
}


/* makes totally fresh start situation */
static void lattice_to_softbody(Scene *scene, Object *ob)
{
	Lattice *lt= ob->data;
	SoftBody *sb;
	int totvert, totspring = 0;

	totvert= lt->pntsu*lt->pntsv*lt->pntsw;

	if (ob->softflag & OB_SB_EDGES){
		totspring = ((lt->pntsu -1) * lt->pntsv 
		          + (lt->pntsv -1) * lt->pntsu) * lt->pntsw
				  +lt->pntsu*lt->pntsv*(lt->pntsw -1);
		if (ob->softflag & OB_SB_QUADS){
			totspring += 4*(lt->pntsu -1) *  (lt->pntsv -1)  * (lt->pntsw-1);
		}
	}
	

	/* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
	renew_softbody(scene, ob, totvert, totspring);
	sb= ob->soft;	/* can be created in renew_softbody() */
	
	/* weights from bpoints, same code used as for mesh vertices */
	if((ob->softflag & OB_SB_GOAL) && sb->vertgroup) {
		BodyPoint *bp= sb->bpoint;
		BPoint *bpnt= lt->def;
		float goalfac= ABS(sb->maxgoal - sb->mingoal);
		int a;

		for(a=0; a<totvert; a++, bp++, bpnt++) {
			bp->goal= sb->mingoal + bpnt->weight*goalfac;
			/* a little ad hoc changing the goal control to be less *sharp* */
			bp->goal = (float)pow(bp->goal, 4.0f);
		}
	}	
	
	/* create some helper edges to enable SB lattice to be usefull at all */
	if (ob->softflag & OB_SB_EDGES){
		makelatticesprings(lt,ob->soft->bspring,ob->softflag & OB_SB_QUADS,ob);
		build_bps_springlist(ob); /* link bps to springs */
	}
}

/* makes totally fresh start situation */
static void curve_surf_to_softbody(Scene *scene, Object *ob)
{
	Curve *cu= ob->data;
	SoftBody *sb;
	BodyPoint *bp;
	BodySpring *bs;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bpnt;
	float goalfac;
	int a, curindex=0;
	int totvert, totspring = 0, setgoal=0;
	
	totvert= count_curveverts(&cu->nurb);
	
	if (ob->softflag & OB_SB_EDGES){
		if(ob->type==OB_CURVE) {
			totspring= totvert - BLI_countlist(&cu->nurb);
		}
	}
	
	/* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
	renew_softbody(scene, ob, totvert, totspring);
	sb= ob->soft;	/* can be created in renew_softbody() */
		
	/* set vars now */
	goalfac= ABS(sb->maxgoal - sb->mingoal);
	bp= sb->bpoint;
	bs= sb->bspring;
	
	/* weights from bpoints, same code used as for mesh vertices */
	if((ob->softflag & OB_SB_GOAL) && sb->vertgroup)
		setgoal= 1;
		
	for(nu= cu->nurb.first; nu; nu= nu->next) {
		if(nu->bezt) {
			for(bezt=nu->bezt, a=0; a<nu->pntsu; a++, bezt++, bp+=3, curindex+=3) {
				if(setgoal) {
					bp->goal= sb->mingoal + bezt->weight*goalfac;
					/* a little ad hoc changing the goal control to be less *sharp* */
					bp->goal = (float)pow(bp->goal, 4.0f);
					
					/* all three triples */
					(bp+1)->goal= bp->goal;
					(bp+2)->goal= bp->goal;
				}
				
				if(totspring) {
					if(a>0) {
						bs->v1= curindex-1;
						bs->v2= curindex;
						bs->springtype=SB_EDGE;
						bs->len= globallen( (bezt-1)->vec[2], bezt->vec[0], ob );
						bs++;
					}
					bs->v1= curindex;
					bs->v2= curindex+1;
					bs->springtype=SB_EDGE;
					bs->len= globallen( bezt->vec[0], bezt->vec[1], ob );
					bs++;

					bs->v1= curindex+1;
					bs->v2= curindex+2;
					bs->springtype=SB_EDGE;
					bs->len= globallen( bezt->vec[1], bezt->vec[2], ob );
					bs++;
				}
			}
		}
		else {
			for(bpnt=nu->bp, a=0; a<nu->pntsu*nu->pntsv; a++, bpnt++, bp++, curindex++) {
				if(setgoal) {
					bp->goal= sb->mingoal + bpnt->weight*goalfac;
					/* a little ad hoc changing the goal control to be less *sharp* */
					bp->goal = (float)pow(bp->goal, 4.0f);
				}
				if(totspring && a>0) {
					bs->v1= curindex-1;
					bs->v2= curindex;
					bs->springtype=SB_EDGE;
					bs->len= globallen( (bpnt-1)->vec, bpnt->vec , ob );
					bs++;
				}
			}
		}
	}
	
	if(totspring)
	{
		build_bps_springlist(ob); /* link bps to springs */
		if (ob->softflag & OB_SB_SELF) {calculate_collision_balls(ob);}
	}
}

/* copies softbody result back in object */
static void softbody_to_object(Object *ob, float (*vertexCos)[3], int numVerts, int local)
{
	SoftBody *sb= ob->soft;
	if(sb){
		BodyPoint *bp= sb->bpoint;
		int a;
		if(sb->solverflags & SBSO_MONITOR ||sb->solverflags & SBSO_ESTIMATEIPO){SB_estimate_transform(ob,sb->lcom,sb->lrot,sb->lscale);}
		/* inverse matrix is not uptodate... */
		invert_m4_m4(ob->imat, ob->obmat);

		for(a=0; a<numVerts; a++, bp++) {
			VECCOPY(vertexCos[a], bp->pos);
			if(local==0) 
				mul_m4_v3(ob->imat, vertexCos[a]);	/* softbody is in global coords, baked optionally not */
		}
	}
}

/* +++ ************ maintaining scratch *************** */
static void sb_new_scratch(SoftBody *sb)
{
	if (!sb) return;
	sb->scratch = MEM_callocN(sizeof(SBScratch), "SBScratch");
	sb->scratch->colliderhash = BLI_ghash_new(BLI_ghashutil_ptrhash,BLI_ghashutil_ptrcmp);
	sb->scratch->bodyface = NULL;
	sb->scratch->totface = 0;
	sb->scratch->aabbmax[0]=sb->scratch->aabbmax[1]=sb->scratch->aabbmax[2] = 1.0e30f;
	sb->scratch->aabbmin[0]=sb->scratch->aabbmin[1]=sb->scratch->aabbmin[2] = -1.0e30f;
	sb->scratch->Ref.ivert = NULL;
	
}
/* --- ************ maintaining scratch *************** */

/* ************ Object level, exported functions *************** */

/* allocates and initializes general main data */
SoftBody *sbNew(Scene *scene)
{
	SoftBody *sb;
	
	sb= MEM_callocN(sizeof(SoftBody), "softbody");
	
	sb->mediafrict= 0.5f; 
	sb->nodemass= 1.0f;
	sb->grav= 9.8f; 
	sb->physics_speed= 1.0f;
	sb->rklimit= 0.1f;

	sb->goalspring= 0.5f; 
	sb->goalfrict= 0.0f; 
	sb->mingoal= 0.0f;  
	sb->maxgoal= 1.0f;
	sb->defgoal= 0.7f;
	
	sb->inspring= 0.5f;
	sb->infrict= 0.5f; 
	/*todo backward file compat should copy inspring to inpush while reading old files*/
	sb->inpush = 0.5f; 
	
	sb->interval= 10;
	sb->sfra= scene->r.sfra;
	sb->efra= scene->r.efra;

	sb->colball  = 0.49f;
	sb->balldamp = 0.50f;
	sb->ballstiff= 1.0f;
	sb->sbc_mode = 1;


	sb->minloops = 10;
	sb->maxloops = 300;

	sb->choke = 3;
	sb_new_scratch(sb);
	/*todo backward file compat should set sb->shearstiff = 1.0f while reading old files*/
	sb->shearstiff = 1.0f;
	sb->solverflags |= SBSO_OLDERR;

	sb->pointcache = BKE_ptcache_add(&sb->ptcaches);

	if(!sb->effector_weights)
		sb->effector_weights = BKE_add_effector_weights(NULL);

	return sb;
}

/* frees all */
void sbFree(SoftBody *sb)
{
	free_softbody_intern(sb);
	BKE_ptcache_free_list(&sb->ptcaches);
	sb->pointcache = NULL;
	if(sb->effector_weights)
		MEM_freeN(sb->effector_weights);
	MEM_freeN(sb);
}

void sbFreeSimulation(SoftBody *sb)
{
	free_softbody_intern(sb);
}

/* makes totally fresh start situation */
void sbObjectToSoftbody(Object *ob)
{
	//ob->softflag |= OB_SB_REDO;

	free_softbody_intern(ob->soft);
}

static int object_has_edges(Object *ob) 
{
	if(ob->type==OB_MESH) {
		return ((Mesh*) ob->data)->totedge;
	}
	else if(ob->type==OB_LATTICE) {
		return 1;
	}
	else {
		return 0;
	}
}

/* SB global visible functions */ 
void sbSetInterruptCallBack(int (*f)(void))
{
	SB_localInterruptCallBack = f;
}

static void softbody_update_positions(Object *ob, SoftBody *sb, float (*vertexCos)[3], int numVerts)
{
	BodyPoint *bp;
	int a;

	if(!sb || !sb->bpoint)
		return;

	for(a=0,bp=sb->bpoint; a<numVerts; a++, bp++) {
		/* store where goals are now */ 
		VECCOPY(bp->origS, bp->origE);
		/* copy the position of the goals at desired end time */
		VECCOPY(bp->origE, vertexCos[a]);
		/* vertexCos came from local world, go global */
		mul_m4_v3(ob->obmat, bp->origE);
		/* just to be save give bp->origT a defined value
		will be calulated in interpolate_exciter()*/
		VECCOPY(bp->origT, bp->origE); 
	}
}

/* void SB_estimate_transform */
/* input   Object *ob out (says any object that can do SB like mesh,lattice,curve )
   output  float lloc[3],float lrot[3][3],float lscale[3][3]
   that is:
   a precise position vector denoting the motion of the center of mass
   give a rotation/scale matrix using averaging method, that's why estimate and not calculate
   see: this is kind of reverse engeneering: having to states of a point cloud and recover what happend
   our advantage here we know the identity of the vertex
   there are others methods giving other results.
   lloc,lrot,lscale are allowed to be NULL, just in case you don't need it. 
   should be pretty useful for pythoneers :)
   not! velocity .. 2nd order stuff
   */

/* can't believe there is none in math utils */
float _det_m3(float m2[3][3])
{
	float det = 0.f;
	if (m2){
	det= m2[0][0]* (m2[1][1]*m2[2][2] - m2[1][2]*m2[2][1])
	    -m2[1][0]* (m2[0][1]*m2[2][2] - m2[0][2]*m2[2][1])
	    +m2[2][0]* (m2[0][1]*m2[1][2] - m2[0][2]*m2[1][1]);
	}
	return det;
}

void SB_estimate_transform(Object *ob,float lloc[3],float lrot[3][3],float lscale[3][3])
{
	BodyPoint *bp;
	ReferenceVert *rp;
	float accu_pos[3] = {0.0f,0.0f,0.0f};
	float com[3],va[3],vb[3],rcom[3];
	float accu_mass = 0.0f,la = 0.0f,lb = 0.0f,eps = 0.000001f;
	SoftBody *sb = 0;
	int a,i=0,imax=16;
    int _localdebug;
	
	if (lloc) zero_v3(lloc);
	if (lrot) zero_m3(lrot);
	if (lscale) zero_m3(lscale);


	if(!ob ||!ob->soft) return; /* why did we get here ? */
	sb= ob->soft;
	/*for threading there should be a lock */
	/* sb-> lock; */
	/* calculate center of mass */
	if(!sb || !sb->bpoint) return;
	_localdebug=sb->solverflags & SBSO_MONITOR; /* turn this on/off if you (don't) want to see progress on console */ 
	for(a=0,bp=sb->bpoint; a<sb->totpoint; a++, bp++) {
		VECADD(accu_pos,accu_pos,bp->pos);
		accu_mass += bp->mass;
	}
	VECCOPY(com,accu_pos);
	mul_v3_fl(com,1.0f/accu_mass);
	/* center of mass done*/
	if (sb->scratch){
		float dcom[3],stunt[3];
		float m[3][3],mr[3][3],q[3][3],qi[3][3];
		float odet,ndet;
		zero_m3(m);
		zero_m3(mr);
		VECSUB(dcom,com,sb->scratch->Ref.com);
		VECCOPY(rcom,sb->scratch->Ref.com);
		if (_localdebug) {
			printf("DCOM %f %f %f\n",dcom[0],dcom[1],dcom[2]);  
		}
		if (lloc) VECCOPY(lloc,dcom);
        VECCOPY(sb->lcom,dcom);
		/* build 'projection' matrix */
		for(a=0, bp=sb->bpoint, rp=sb->scratch->Ref.ivert; a<sb->totpoint; a++, bp++, rp++) {
			VECSUB(va,rp->pos,rcom); 
			la += len_v3(va);
			/* mul_v3_fl(va,bp->mass);  mass needs renormalzation here ?? */ 
			VECSUB(vb,bp->pos,com); 
			lb += len_v3(vb);
			/* mul_v3_fl(va,rp->mass); */
			m[0][0] += va[0] * vb[0];
			m[0][1] += va[0] * vb[1];
			m[0][2] += va[0] * vb[2];

			m[1][0] += va[1] * vb[0];
			m[1][1] += va[1] * vb[1];
			m[1][2] += va[1] * vb[2];

			m[2][0] += va[2] * vb[0];
			m[2][1] += va[2] * vb[1];
			m[2][2] += va[2] * vb[2];

			/* building the referenc matrix on the fly
			needed to scale properly later*/
          
			mr[0][0] += va[0] * va[0];
			mr[0][1] += va[0] * va[1];
			mr[0][2] += va[0] * va[2];

			mr[1][0] += va[1] * va[0];
			mr[1][1] += va[1] * va[1];
			mr[1][2] += va[1] * va[2];

			mr[2][0] += va[2] * va[0];
			mr[2][1] += va[2] * va[1];
			mr[2][2] += va[2] * va[2];
		}
		/* we are pretty much set up here and we could return that raw mess containing essential information
		but being nice fellows we proceed:
		knowing we did split off the tanslational part to the center of mass (com) part
		however let's do some more reverse engeneering and see if we can split 
		rotation from scale ->Polardecompose
		*/
		copy_m3_m3(q,m);
		stunt[0] = q[0][0]; stunt[1] = q[1][1]; stunt[2] = q[2][2];
		/* nothing to see here but renormalizing works nicely
		printf("lenght stunt %5.3f a %5.3f b %5.3f %5.3f\n",len_v3(stunt),la,lb,sqrt(la*lb));
		*/
		mul_m3_fl(q,1.f/len_v3(stunt)); 
		/* not too much to see here
			if(_localdebug){
				printf("q0 %5.3f %5.3f %5.3f\n",q[0][0],q[0][1],q[0][2]);
				printf("q1 %5.3f %5.3f %5.3f\n",q[1][0],q[1][1],q[1][2]);
				printf("q2 %5.3f %5.3f %5.3f\n",q[2][0],q[2][1],q[2][2]);
			}
			*/
		/* this is pretty much Polardecompose 'inline' the algo based on Higham's thesis */
		/* without the far case !!! but seems to work here pretty neat                   */
		odet = 0.f;
		ndet = _det_m3(q);
		while((odet-ndet)*(odet-ndet) > eps && i<imax){
			invert_m3_m3(qi,q);
			transpose_m3(qi);
			add_m3_m3m3(q,q,qi);
			mul_m3_fl(q,0.5f);
			odet =ndet;
			ndet =_det_m3(q);
			i++;
		}
		if (i){
			float scale[3][3];
			float irot[3][3];
			if(lrot) copy_m3_m3(lrot,q);
			copy_m3_m3(sb->lrot,q);
			if(_localdebug){
				printf("Rot .. i %d\n",i);
				printf("!q0 %5.3f %5.3f %5.3f\n",q[0][0],q[0][1],q[0][2]);
				printf("!q1 %5.3f %5.3f %5.3f\n",q[1][0],q[1][1],q[1][2]);
				printf("!q2 %5.3f %5.3f %5.3f\n",q[2][0],q[2][1],q[2][2]);
			}
			invert_m3_m3(irot,q);
			/* now that's where we need mr to get scaling right */
			invert_m3_m3(qi,mr);
			mul_m3_m3m3(q,m,qi); 

			//mul_m3_m3m3(scale,q,irot);
			mul_m3_m3m3(scale,irot,q); /*  i always have a problem with this C functions left/right operator applies first*/
  		    mul_m3_fl(scale,lb/la); /* 0 order scale was normalized away so put it back here  dunno if that is needed here ???*/

			if(lscale) copy_m3_m3(lscale,scale);
			copy_m3_m3(sb->lscale,scale);
			if(_localdebug){
				printf("Scale .. \n");
				printf("!s0 %5.3f %5.3f %5.3f\n",scale[0][0],scale[0][1],scale[0][2]);
				printf("!s1 %5.3f %5.3f %5.3f\n",scale[1][0],scale[1][1],scale[1][2]);
				printf("!s2 %5.3f %5.3f %5.3f\n",scale[2][0],scale[2][1],scale[2][2]);
			}

			
		}
	}
	/*for threading there should be a unlock */
	/* sb-> unlock; */
}

static void softbody_reset(Object *ob, SoftBody *sb, float (*vertexCos)[3], int numVerts)
{
	BodyPoint *bp;
	int a;

	for(a=0,bp=sb->bpoint; a<numVerts; a++, bp++) {
		VECCOPY(bp->pos, vertexCos[a]);
		mul_m4_v3(ob->obmat, bp->pos);  /* yep, sofbody is global coords*/
		VECCOPY(bp->origS, bp->pos);
		VECCOPY(bp->origE, bp->pos);
		VECCOPY(bp->origT, bp->pos);
		bp->vec[0]= bp->vec[1]= bp->vec[2]= 0.0f;

		/* the bp->prev*'s are for rolling back from a canceled try to propagate in time
		adaptive step size algo in a nutshell:
		1.  set sheduled time step to new dtime
		2.  try to advance the sheduled time step, beeing optimistic execute it
		3.  check for success
		3.a we 're fine continue, may be we can increase sheduled time again ?? if so, do so! 
		3.b we did exceed error limit --> roll back, shorten the sheduled time and try again at 2.
		4.  check if we did reach dtime 
		4.a nope we need to do some more at 2.
		4.b yup we're done
		*/

		VECCOPY(bp->prevpos, bp->pos);
		VECCOPY(bp->prevvec, bp->vec);
		VECCOPY(bp->prevdx, bp->vec);
		VECCOPY(bp->prevdv, bp->vec);
	}

	/* make a nice clean scratch struc */
	free_scratch(sb); /* clear if any */
	sb_new_scratch(sb); /* make a new */
	sb->scratch->needstobuildcollider=1; 

	/* copy some info to scratch */
	if (1) reference_to_scratch(ob); /* wa only need that if we want to reconstruct IPO */
	switch(ob->type) {
	case OB_MESH:
		if (ob->softflag & OB_SB_FACECOLL) mesh_faces_to_scratch(ob);
		break;
	case OB_LATTICE:
		break;
	case OB_CURVE:
	case OB_SURF:
		break;
	default:
		break;
	}
}

static void softbody_step(Scene *scene, Object *ob, SoftBody *sb, float dtime)
{
	/* the simulator */
	float forcetime;
	double sct,sst;
		
		
	sst=PIL_check_seconds_timer();
	/* Integration back in time is possible in theory, but pretty useless here. 
	So we refuse to do so. Since we do not know anything about 'outside' canges
	especially colliders we refuse to go more than 10 frames.
	*/
	if(dtime < 0 || dtime > 10.5f) return; 
	
	ccd_update_deflector_hash(scene, ob, sb->scratch->colliderhash);

	if(sb->scratch->needstobuildcollider){
		if (query_external_colliders(scene, ob)){
			ccd_build_deflector_hash(scene, ob, sb->scratch->colliderhash);
		}
		sb->scratch->needstobuildcollider=0;
	}

	if (sb->solver_ID < 2) {	
		/* special case of 2nd order Runge-Kutta type AKA Heun */
		int mid_flags=0;
		float err = 0;
		float forcetimemax = 1.0f; /* set defaults guess we shall do one frame */
		float forcetimemin = 0.01f; /* set defaults guess 1/100 is tight enough */
		float timedone =0.0; /* how far did we get without violating error condition */
							 /* loops = counter for emergency brake
							 * we don't want to lock up the system if physics fail
		*/
		int loops =0 ; 
		
		SoftHeunTol = sb->rklimit; /* humm .. this should be calculated from sb parameters and sizes */
		/* adjust loop limits */
		if (sb->minloops > 0) forcetimemax = dtime / sb->minloops;
		if (sb->maxloops > 0) forcetimemin = dtime / sb->maxloops;

		if(sb->solver_ID>0) mid_flags |= MID_PRESERVE;
		
		forcetime =forcetimemax; /* hope for integrating in one step */
		while ( (ABS(timedone) < ABS(dtime)) && (loops < 2000) )
		{
			/* set goals in time */ 
			interpolate_exciter(ob,200,(int)(200.0*(timedone/dtime)));
			
			sb->scratch->flag &= ~SBF_DOFUZZY;
			/* do predictive euler step */
			softbody_calc_forces(scene, ob, forcetime,timedone/dtime,0);

			softbody_apply_forces(ob, forcetime, 1, NULL,mid_flags);

			/* crop new slope values to do averaged slope step */
			softbody_calc_forces(scene, ob, forcetime,timedone/dtime,0);

			softbody_apply_forces(ob, forcetime, 2, &err,mid_flags);
			softbody_apply_goalsnap(ob);
			
			if (err > SoftHeunTol) { /* error needs to be scaled to some quantity */
				
				if (forcetime > forcetimemin){
					forcetime = MAX2(forcetime / 2.0f,forcetimemin);
					softbody_restore_prev_step(ob);
					//printf("down,");
				}
				else {
					timedone += forcetime;
				}
			}
			else {
				float newtime = forcetime * 1.1f; /* hope for 1.1 times better conditions in next step */
				
				if (sb->scratch->flag & SBF_DOFUZZY){
					//if (err > SoftHeunTol/(2.0f*sb->fuzzyness)) { /* stay with this stepsize unless err really small */
					newtime = forcetime;
					//}
				}
				else {
					if (err > SoftHeunTol/2.0f) { /* stay with this stepsize unless err really small */
						newtime = forcetime;
					}
				}
				timedone += forcetime;
				newtime=MIN2(forcetimemax,MAX2(newtime,forcetimemin));
				//if (newtime > forcetime) printf("up,");
				if (forcetime > 0.0)
					forcetime = MIN2(dtime - timedone,newtime);
				else 
					forcetime = MAX2(dtime - timedone,newtime);
			}
			loops++;
			if(sb->solverflags & SBSO_MONITOR ){
				sct=PIL_check_seconds_timer();
				if (sct-sst > 0.5f) printf("%3.0f%% \r",100.0f*timedone/dtime);
			}
			/* ask for user break */ 
			if (SB_localInterruptCallBack && SB_localInterruptCallBack()) break;

		}
		/* move snapped to final position */
		interpolate_exciter(ob, 2, 2);
		softbody_apply_goalsnap(ob);
		
		//				if(G.f & G_DEBUG){
		if(sb->solverflags & SBSO_MONITOR ){
			if (loops > HEUNWARNLIMIT) /* monitor high loop counts */
				printf("\r needed %d steps/frame",loops);
		}
		
	}
	else if (sb->solver_ID == 2)
	{/* do semi "fake" implicit euler */
		//removed
	}/*SOLVER SELECT*/
	else if (sb->solver_ID == 4)
	{
		/* do semi "fake" implicit euler */
	}/*SOLVER SELECT*/
	else if (sb->solver_ID == 3){
		/* do "stupid" semi "fake" implicit euler */
		//removed

	}/*SOLVER SELECT*/
	else{
		printf("softbody no valid solver ID!");
	}/*SOLVER SELECT*/
	if(sb->plastic){ apply_spring_memory(ob);}

	if(sb->solverflags & SBSO_MONITOR ){
		sct=PIL_check_seconds_timer();
		if ((sct-sst > 0.5f) || (G.f & G_DEBUG)) printf(" solver time %f sec %s \n",sct-sst,ob->id.name);
	}
}

/* simulates one step. framenr is in frames */
void sbObjectStep(Scene *scene, Object *ob, float cfra, float (*vertexCos)[3], int numVerts)
{
	SoftBody *sb= ob->soft;
	PointCache *cache;
	PTCacheID pid;
	float dtime, timescale;
	int framedelta, framenr, startframe, endframe;
	int cache_result;

	cache= sb->pointcache;

	framenr= (int)cfra;
	framedelta= framenr - cache->simframe;

	BKE_ptcache_id_from_softbody(&pid, ob, sb);
	BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, &timescale);

	/* check for changes in mesh, should only happen in case the mesh
	 * structure changes during an animation */
	if(sb->bpoint && numVerts != sb->totpoint) {
		cache->flag &= ~PTCACHE_SIMULATION_VALID;
		cache->simframe= 0;
		cache->last_exact= 0;
		return;
	}

	/* clamp frame ranges */
	if(framenr < startframe) {
		cache->flag &= ~PTCACHE_SIMULATION_VALID;
		cache->simframe= 0;
		//cache->last_exact= 0;
		return;
	}
	else if(framenr > endframe) {
		framenr = endframe;
	}

	/* verify if we need to create the softbody data */
	if(sb->bpoint == NULL ||
	   ((ob->softflag & OB_SB_EDGES) && !ob->soft->bspring && object_has_edges(ob))) {

		switch(ob->type) {
			case OB_MESH:
				mesh_to_softbody(scene, ob);
				break;
			case OB_LATTICE:
				lattice_to_softbody(scene, ob);
				break;
			case OB_CURVE:
			case OB_SURF:
				curve_surf_to_softbody(scene, ob);
				break;
			default:
				renew_softbody(scene, ob, numVerts, 0);
				break;
		}

		softbody_update_positions(ob, sb, vertexCos, numVerts);
		softbody_reset(ob, sb, vertexCos, numVerts);
	}

	/* continue physics special case */
	if(BKE_ptcache_get_continue_physics()) {
		cache->flag &= ~PTCACHE_SIMULATION_VALID;
		cache->simframe= 0;
		/* do simulation */
		dtime = timescale;
		softbody_update_positions(ob, sb, vertexCos, numVerts);
		softbody_step(scene, ob, sb, dtime);
		if(sb->solverflags & SBSO_MONITOR ){
			printf("Picked from cache continue_physics %d\n",framenr);
		}

		softbody_to_object(ob, vertexCos, numVerts, 0);
		return;
	}

	/* still no points? go away */
	if(sb->totpoint==0) {
		return;
	}
    if(framenr == startframe) {
		BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);

		/* first frame, no simulation to do, just set the positions */
		softbody_update_positions(ob, sb, vertexCos, numVerts);

		cache->simframe= framenr;
		cache->flag |= PTCACHE_SIMULATION_VALID;
		cache->flag &= ~PTCACHE_REDO_NEEDED;
		return;
	}

	/* try to read from cache */
	cache_result = BKE_ptcache_read_cache(&pid, framenr, scene->r.frs_sec);

	if(cache_result == PTCACHE_READ_EXACT || cache_result == PTCACHE_READ_INTERPOLATED) {
		if(sb->solverflags & SBSO_MONITOR ){
			printf("Picked from cache at frame %d\n",framenr);
		}
		softbody_to_object(ob, vertexCos, numVerts, sb->local);

		cache->simframe= framenr;
		cache->flag |= PTCACHE_SIMULATION_VALID;

		if(cache_result == PTCACHE_READ_INTERPOLATED && cache->flag & PTCACHE_REDO_NEEDED)
			BKE_ptcache_write_cache(&pid, framenr);

		return;
	}
	else if(cache_result==PTCACHE_READ_OLD) {
		cache->flag |= PTCACHE_SIMULATION_VALID;
	}
	else if(ob->id.lib || (cache->flag & PTCACHE_BAKED)) {
		/* if baked and nothing in cache, do nothing */
		cache->flag &= ~PTCACHE_SIMULATION_VALID;
		cache->simframe= 0;
		cache->last_exact= 0;
		return;
	}

	/* if on second frame, write cache for first frame */
	if(cache->simframe == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact==0))
		BKE_ptcache_write_cache(&pid, startframe);

	softbody_update_positions(ob, sb, vertexCos, numVerts);

	/* checking time: */
	dtime = framedelta*timescale;

	/* do simulation */
	softbody_step(scene, ob, sb, dtime);

	softbody_to_object(ob, vertexCos, numVerts, 0);

	cache->simframe= framenr;
	cache->flag |= PTCACHE_SIMULATION_VALID;
	BKE_ptcache_write_cache(&pid, framenr);
}

