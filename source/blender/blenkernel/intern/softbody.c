/*  softbody.c      
 * 
 * $Id: 
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

/* types */
#include "DNA_ika_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_softbody.h"
#include "BKE_displist.h"

/* ********** soft body engine ******* */


static SoftBody *new_softbody(int totpoint, int totspring)  
{
	SoftBody *sb= NULL;
	
	if(totpoint) {
		sb= MEM_callocN(sizeof(SoftBody), "softbody");
		sb->totpoint= totpoint;
		sb->totspring= totspring;
		
		sb->bpoint= MEM_mallocN( totpoint*sizeof(BodyPoint), "bodypoint");
		if(totspring) 
			sb->bspring= MEM_mallocN( totspring*sizeof(BodySpring), "bodyspring");
	}
	return sb;
}

void free_softbody(SoftBody *sb)
{
	if(sb) {
		if(sb->bpoint) MEM_freeN(sb->bpoint);
		if(sb->bspring) MEM_freeN(sb->bspring);
		
		MEM_freeN(sb);
	}
}

static void softbody_calc_forces(Object *ob)
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	float ks;
	int a;
	
	/* clear forces */
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		bp->force[0]= bp->force[1]= bp->force[2]= 0.0;
	}
		
	/* spring constant */
	ks= ob->springf;

	/* accumulate forces, vertex stiffness */
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {

		bp->force[0]= ks*(bp->orig[0]-bp->pos[0]);
		bp->force[1]= ks*(bp->orig[1]-bp->pos[1]);
		bp->force[2]= ks*(bp->orig[2]-bp->pos[2]);
	}
}

static void softbody_apply_forces(Object *ob, float dtime)
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	float kd;
	int a;
	
	kd= 1.0-ob->damping;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		// friction
		bp->vec[0]*= kd;
		bp->vec[1]*= kd;
		bp->vec[2]*= kd;
		
		VECADD(bp->vec, bp->vec, bp->force);	// mass here!?
		VECADD(bp->pos, bp->pos, bp->vec);
	}
}

static void softbody_apply_goal(Object *ob, float dtime)
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	float vec[3], ks;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		ks= bp->goal;
		// this is hackish, screws up physics but stabilizes
		vec[0]= ks*(bp->orig[0]-bp->pos[0]);
		vec[1]= ks*(bp->orig[1]-bp->pos[1]);
		vec[2]= ks*(bp->orig[2]-bp->pos[2]);

		VECADD(bp->pos, bp->pos, vec);
		
		ks= 1.0-ks;
		bp->vec[0]*= ks;
		bp->vec[1]*= ks;
		bp->vec[2]*= ks;
	}
}


/* ************ convertors ********** */

/* copy original (new) situation in softbody, as result of matrices or deform */
static void mesh_update_softbody(Object *ob)
{
	Mesh *me= ob->data;
	MVert *mvert= me->mvert;
	MEdge *medge= me->medge;
	BodyPoint *bp;
	BodySpring *bs;
	int a;
	
	if(ob->soft) {
	
		bp= ob->soft->bpoint;
		for(a=0; a<me->totvert; a++, mvert++, bp++) {
			VECCOPY(bp->orig, mvert->co);
			Mat4MulVecfl(ob->obmat, bp->orig);
		}
		if(medge) {
			bs= ob->soft->bspring;
			bp= ob->soft->bpoint;
			for(a=0; a<me->totedge; a++, medge++, bs++) {
				bs->len= VecLenf( (bp+bs->v1)->orig, (bp+bs->v2)->orig);
			}
		}
	}

}

/* makes totally fresh start situation */
static void mesh_to_softbody(Object *ob)
{
	Mesh *me= ob->data;
	MVert *mvert= me->mvert;
	MEdge *medge= me->medge;
	MFace *mface= me->mface;
	BodyPoint *bp;
	BodySpring *bs;
	int a;
	
	ob->soft= new_softbody(me->totvert, me->totedge);
	if(ob->soft) {
		bp= ob->soft->bpoint;
		for(a=me->totvert; a>0; a--, mvert++, bp++) {
			VECCOPY(bp->pos, mvert->co);
			Mat4MulVecfl(ob->obmat, bp->pos);  // yep, sofbody is global coords
			VECCOPY(bp->orig, bp->pos);
			bp->vec[0]= bp->vec[1]= bp->vec[2]= 0.0;
			bp->weight= 1.0;
			bp->goal= 0.5;
		}
		if(medge) {
			bs= ob->soft->bspring;
			bp= ob->soft->bpoint;
			for(a=me->totedge; a>0; a--, medge++, bs++) {
				bs->v1= medge->v1;
				bs->v2= medge->v2;
				bs->strength= 1.0;
				bs->len= VecLenf( (bp+bs->v1)->orig, (bp+bs->v2)->orig);
			}
		}
		/* vertex colors are abused as weights here, however they're stored in faces... uhh */
		if(mface && me->mcol) {
			char *mcol= (char *)me->mcol;
			for(a=me->totface; a>0; a--, mface++, mcol+=16) {
				bp= ob->soft->bpoint+mface->v1;
				if(bp->goal==0.5) {
					bp->goal= ( (float)( (mcol + 0)[1] ) )/255.0;
				}
				bp= ob->soft->bpoint+mface->v2;
				if(bp->goal==0.5) {
					bp->goal= ( (float)( (mcol + 4)[1] ) )/255.0;
				}
				bp= ob->soft->bpoint+mface->v3;
				if(bp->goal==0.5) {
					bp->goal= ( (float)( (mcol + 8)[1]) )/255.0;
				}
				if(mface->v4) {
					bp= ob->soft->bpoint+mface->v4;
					if(bp->goal==0.5) {
						bp->goal= ( (float)( (mcol + 12)[1]) )/255.0;
					}
				}
			}
		}
		bp= ob->soft->bpoint;
		for(a=me->totvert; a>0; a--, bp++) {
			//printf("a %d goal %f\n", a, bp->goal);
		}
		
	}
}

/* copies current sofbody position in mesh, so do this within modifier stacks! */
static void softbody_to_mesh(Object *ob)
{
	Mesh *me= ob->data;
	MVert *mvert;
	BodyPoint *bp;
	int a;

	Mat4Invert(ob->imat, ob->obmat);
	
	bp= ob->soft->bpoint;
	mvert= me->mvert;
	for(a=me->totvert; a>0; a--, mvert++, bp++) {
		VECCOPY(mvert->co, bp->pos);
		Mat4MulVecfl(ob->imat, mvert->co);	// softbody is in global coords
	}

}

/* makes totally fresh start situation */
static void lattice_to_softbody(Object *ob)
{

}

/* copies current sofbody position */
static void softbody_to_lattice(Object *ob)
{

}



/* ************ Object level, exported functions *************** */

/* copy original (new) situation in softbody, as result of matrices or deform */
void object_update_softbody(Object *ob)
{
	switch(ob->type) {
	case OB_MESH:
		mesh_update_softbody(ob);
		break;
	case OB_LATTICE:
		//lattice_update_softbody(ob);
		break;
	}

}

/* makes totally fresh start situation */
void object_to_softbody(Object *ob)
{
	
	if(ob->soft) free_softbody(ob->soft);
	ob->soft= NULL;
	
	switch(ob->type) {
	case OB_MESH:
		mesh_to_softbody(ob);
		break;
	case OB_LATTICE:
		lattice_to_softbody(ob);
		break;
	}
}

/* copies softbody result back in object */
void softbody_to_object(Object *ob)
{

	if(ob->soft==NULL) return;
	
	switch(ob->type) {
	case OB_MESH:
		softbody_to_mesh(ob);
		break;
	case OB_LATTICE:
		softbody_to_lattice(ob);
		break;
	}
}


/* simulates one step. ctime is in frames not seconds */
void object_softbody_step(Object *ob, float ctime)
{
	float dtime;
	
	if(ob->soft==NULL) {
		object_to_softbody(ob);
		if(ob->soft==NULL) return;
		ob->soft->ctime= ctime;
	}
	
	dtime= ctime - ob->soft->ctime;
	dtime= ABS(dtime);
	if(dtime > 0.0) {
		/* desired vertex locations in oldloc */
		object_update_softbody(ob);
		
		/* extra for desired vertex locations */
		softbody_apply_goal(ob, dtime);
		
		softbody_calc_forces(ob);
		softbody_apply_forces(ob, dtime);

		/* and apply to vertices */
		softbody_to_object(ob);
		
		ob->soft->ctime= ctime;
	}
}

