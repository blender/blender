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

/*
******
variables on the UI for now
typedef struct Object {
.....
	float formfactor, softtime;	 softtime = #euler integrations steps per frame	
.....
	float sb_goalspring;  softbody goal springs 
	float sb_goalfrict;   softbody goal springs friction 
	float sb_inspring;	  softbody inner springs 
	float sb_infrict;     softbody inner springs friction 
	float sb_nodemass;	  softbody mass of *vertex* 
	float sb_grav;        softbody amount of gravitaion to apply 
	float sb_mingoal;     quick limits for goal 
	float sb_maxgoal;
	float sb_mediafrict;  friction to env 
	float sb_pad1;        free 
  
  

*****
*/


#include <math.h>
#include <stdlib.h>
#include <string.h>

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

#include  "BIF_editdeform.h"

extern bDeformGroup *get_named_vertexgroup(Object *ob, char *name);
extern int  get_defgroup_num (Object *ob, bDeformGroup        *dg);


/* ********** soft body engine ******* */
#define SOFTGOALSNAP  0.999f 
// if bp-> goal is above make it a *forced follow original* and skip all ODE stuff for this bp
// removes *unnecessary* stiffnes from ODE system
#define HEUNWARNLIMIT 1 // 50 would be fine i think for detecting severe *stiff* stuff

float SoftHeunTol = 1.0f; // humm .. this should be calculated from sb parameters and sizes
float steptime =  1.0f/25.0f; // translate framerate to *real* time
float rescale_grav_to_framerate = 1.0f; // since unit of g is [m/sec^2] we need translation from frames to physics time
float rescale_friction_to_framerate = 1.0f; // since unit of drag is [kg/sec] we need translation from frames to physics time

short SB_ENABLE = 0; // quick hack to switch sb integration in 3d header


void softbody_scale_time(float steptime)
{
  rescale_grav_to_framerate = steptime*steptime; 
  rescale_friction_to_framerate = steptime;
}


static int count_quads(	Mesh *me)
{
	int a,result = 0;
	MFace *mface= me->mface;
	if(mface ) {
		for(a=me->totface; a>0; a--, mface++) {if(mface->v4) result++;}
	}	
	return result;
}

static void add_quad_diag_springs(Object *ob)
{
	Mesh *me= ob->data;
	MFace *mface= me->mface;
	BodyPoint *bp;
	BodySpring *bs, *bs_new;
	int a ;
	
	if (ob->soft){
		int nofquads;   
		nofquads = count_quads(me);
		if (nofquads) {
			/* resize spring-array to hold additional quad springs */
			bs_new= MEM_callocN( (ob->soft->totspring + nofquads *2 )*sizeof(BodySpring), "bodyspring");
			memcpy(bs_new,ob->soft->bspring,(ob->soft->totspring )*sizeof(BodySpring));
			MEM_freeN(ob->soft->bspring); /* do this before reassigning the pointer  or have a 1st class memory leak */
			ob->soft->bspring = bs_new;   
			/* fill the tail */
			a = 0;
			bs = bs_new+ob->soft->totspring;
			bp= ob->soft->bpoint;
			if(mface ) {
				for(a=me->totface; a>0; a--, mface++) {
					if(mface->v4) 
					{
						bs->v1= mface->v1;
						bs->v2= mface->v3;
						bs->strength= 1.0;
						bs->len= VecLenf( (bp+bs->v1)->origS, (bp+bs->v2)->origS);
						bs++;
						bs->v1= mface->v2;
						bs->v2= mface->v4;
						bs->strength= 1.0;
						bs->len= VecLenf( (bp+bs->v1)->origS, (bp+bs->v2)->origS);
						bs++;
						
					}
				}	
			}
			
            /* now we can announce new springs */
			ob->soft->totspring += nofquads *2;
			
		}
	}
}


static	void add_bp_springlist(BodyPoint *bp,int springID)
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
static	void build_bps_springlist(Object *ob)
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;	
	BodySpring *bs;	
	
	int a,b;
	if (!sb) return; // paranoya check
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		/* scan for attached inner springs */	
		for(b=sb->totspring, bs= sb->bspring; b>0; b--, bs++) {
			if (( (sb->totpoint-a) == bs->v1) ){ 
				add_bp_springlist(bp,sb->totspring -b);
			}
			if (( (sb->totpoint-a) == bs->v2) ){ 
				add_bp_springlist(bp,sb->totspring -b);
			}
		}//for springs
		if (bp->nofsprings) printf(" node %d has %d spring links\n",a,bp->nofsprings);
	}//for bp		
}



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
		MEM_freeN(sb);
	}
}
/* ************ dynamics ********** */

/* aye this belongs to arith.c */
void Vec3PlusStVec(float *v, float s, float *v1)
{
	v[0] += s*v1[0];
	v[1] += s*v1[1];
	v[2] += s*v1[2];
}


static void softbody_calc_forces(Object *ob, float dtime)
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint  *bp;
	float iks,ks,kd,gravity,actspringlen,forcefactor,sd[3];
	int a,b;
	BodyPoint *bproot;
	BodySpring *bs;	
	/* clear forces */
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		bp->force[0]= bp->force[1]= bp->force[2]= 0.0;
	}
	
	gravity = ob->sb_nodemass * ob->sb_grav * rescale_grav_to_framerate;	
	iks  = 1.0f/(1.0f-ob->sb_inspring)-1.0f ;/* inner spring constants function */
	bproot= sb->bpoint; /* need this for proper spring addressing */
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		if(bp->goal < SOFTGOALSNAP){ // ommit this bp when i snaps
			float auxvect[3]; // aux unit vector  
			float velgoal[3];
			float absvel =0, projvel= 0;
			
			/* do goal stuff */
			/* true elastic goal */
			VecSubf(auxvect,bp->origT,bp->pos);
			ks  = 1.0f/(1.0f- bp->goal*ob->sb_goalspring)-1.0f ;
			bp->force[0]= ks*(auxvect[0]);
			bp->force[1]= ks*(auxvect[1]);
			bp->force[2]= ks*(auxvect[2]);
			/* calulate damping forces generated by goals*/
			VecSubf(velgoal,bp->origS, bp->origE);
			kd =  ob->sb_goalfrict * rescale_friction_to_framerate ;

			if (dtime > 0.0 ) { // make sure friction does not become rocket motor on time reversal
			bp->force[0]-= kd * (velgoal[0] + bp->vec[0]);
			bp->force[1]-= kd * (velgoal[1] + bp->vec[1]);
			bp->force[2]-= kd * (velgoal[2] + bp->vec[2]);
			}
			else {
			bp->force[0]-= kd * (velgoal[0] - bp->vec[0]);
			bp->force[1]-= kd * (velgoal[1] - bp->vec[1]);
			bp->force[2]-= kd * (velgoal[2] - bp->vec[2]);
			}
			/* done goal stuff */
			
			
			/* gravitation */
			bp->force[2]-= gravity*ob->sb_nodemass; /* individual mass of node here */
			
			/* friction in media */
			kd= ob->sb_mediafrict* rescale_friction_to_framerate;  
			/* assume it to be proportional to actual velocity */
			bp->force[0]-= bp->vec[0]*kd;
			bp->force[1]-= bp->vec[1]*kd;
			bp->force[2]-= bp->vec[2]*kd;
			/* friction in media done */

			/*other forces*/
			/* this is the place where other forces can be added
			yes, constraints and collision stuff should go here too (read baraff papers on that!)
			*/
			/*other forces done*/

			/* nice things could be done with anisotropic friction
			like wind/air resistance in normal direction
			--> having a piece of cloth sailing down 
			but this needs to have a *valid* vertex normal
			*valid* means to be calulated on time axis
			hrms .. may be a rough one could be used as well .. let's see 
			*/

			if (1){ /* big mesh optimization */
				/* run over attached inner spring list */	
				if (sb->bspring){ // spring list exists at all ? 
					for(b=bp->nofsprings;b>0;b--){
						bs = sb->bspring + bp->springs[b-1];
						if (( (sb->totpoint-a) == bs->v1) ){ 
							actspringlen= VecLenf( (bproot+bs->v2)->pos, bp->pos);
							VecSubf(sd,(bproot+bs->v2)->pos, bp->pos);
							Normalise(sd);

							// friction stuff V1
							VecSubf(velgoal,bp->vec,(bproot+bs->v2)->vec);
							kd = ob->sb_infrict * rescale_friction_to_framerate ;
							absvel  = Normalise(velgoal);
							projvel = ABS(Inpf(sd,velgoal));
							kd *= absvel * projvel;
							Vec3PlusStVec(bp->force,-kd,velgoal);
							
							if(bs->len > 0.0) /* check for degenerated springs */
								forcefactor = (bs->len - actspringlen)/bs->len * iks;
							else
								forcefactor = actspringlen * iks;
							
							Vec3PlusStVec(bp->force,-forcefactor,sd);

						}
						
						if (( (sb->totpoint-a) == bs->v2) ){ 
							actspringlen= VecLenf( (bproot+bs->v1)->pos, bp->pos);
							VecSubf(sd,bp->pos,(bproot+bs->v1)->pos);
							Normalise(sd);

							// friction stuff V2
							VecSubf(velgoal,bp->vec,(bproot+bs->v1)->vec);
							kd = ob->sb_infrict * rescale_friction_to_framerate ;
							absvel  = Normalise(velgoal);
							projvel = ABS(Inpf(sd,velgoal));
							kd *= absvel * projvel;
							Vec3PlusStVec(bp->force,-kd,velgoal);
							
							if(bs->len > 0.0)
								forcefactor = (bs->len - actspringlen)/bs->len * iks;
							else
								forcefactor = actspringlen * iks;
							Vec3PlusStVec(bp->force,+forcefactor,sd);							
						}
					}
				} //if spring list exists at all ?
			}
			else{ // this branch is not completly uptaded for friction stuff 
				/* scan for attached inner springs makes it a O(N^2) thing = bad !*/	
				/* obsolete .. but if someone wants to try the effect :) */
				for(b=sb->totspring, bs= sb->bspring; b>0; b--, bs++) {
					if (( (sb->totpoint-a) == bs->v1) ){ 
						actspringlen= VecLenf( (bproot+bs->v2)->pos, bp->pos);
						VecSubf(sd,(bproot+bs->v2)->pos, bp->pos);
						Normalise(sd);


						if(bs->len > 0.0) /* check for degenerated springs */
							forcefactor = (bs->len - actspringlen)/bs->len * iks;
						else
							forcefactor = actspringlen * iks;
						Vec3PlusStVec(bp->force,-forcefactor,sd);
					}
					
					if (( (sb->totpoint-a) == bs->v2) ){ 
						actspringlen= VecLenf( (bproot+bs->v1)->pos, bp->pos);
						VecSubf(sd,bp->pos,(bproot+bs->v1)->pos);
						Normalise(sd);
						
						if(bs->len > 0.0)
							forcefactor = (bs->len - actspringlen)/bs->len * iks;
						else
							forcefactor = actspringlen * iks;
						Vec3PlusStVec(bp->force,+forcefactor,sd);						
					}
				}// no snap
			}//for
		}	
	}
}

static void softbody_apply_forces(Object *ob, float dtime, int mode, float *err)
{
	/* time evolution */
	/* actually does an explicit euler step mode == 0 */
	/* or heun ~ 2nd order runge-kutta steps, mode 1,2 */
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	float dx[3],dv[3];
	int a;
	float timeovermass;
	float maxerr = 0.0;
	
	if (ob->sb_nodemass > 0.09999f) timeovermass = dtime/ob->sb_nodemass;
	else timeovermass = dtime/0.09999f;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		if(bp->goal < SOFTGOALSNAP){
			
			/* so here is dv/dt = a = sum(F_springs)/m + gravitation + some friction forces */
			/* the euler step for velocity then becomes */
			/* v(t + dt) = v(t) + a(t) * dt */ 
			bp->force[0]*= timeovermass; /* individual mass of node here */ 
			bp->force[1]*= timeovermass;
			bp->force[2]*= timeovermass;
			/* some nasty if's to have heun in here too */
			VECCOPY(dv,bp->force); 
			if (mode == 1){
				VECCOPY(bp->prevvec,bp->vec);
				VECCOPY(bp->prevdv ,dv);
			}
			if (mode ==2){
				/* be optimistic and execute step */
				bp->vec[0] = bp->prevvec[0] + 0.5f * (dv[0] + bp->prevdv[0]);
				bp->vec[1] = bp->prevvec[1] + 0.5f * (dv[1] + bp->prevdv[1]);
				bp->vec[2] = bp->prevvec[2] + 0.5f * (dv[2] + bp->prevdv[2]);
				/* compare euler to heun to estimate error for step sizing */
				maxerr = MAX2(maxerr,ABS(dv[0] - bp->prevdv[0]));
				maxerr = MAX2(maxerr,ABS(dv[1] - bp->prevdv[1]));
				maxerr = MAX2(maxerr,ABS(dv[2] - bp->prevdv[2]));
			}
			else {VECADD(bp->vec, bp->vec, bp->force);}

			/* so here is dx/dt = v */
			/* the euler step for location then becomes */
			/* x(t + dt) = x(t) + v(t) * dt */ 
			
			VECCOPY(dx,bp->vec);
			dx[0]*=dtime ; 
			dx[1]*=dtime ; 
			dx[2]*=dtime ; 
			
			/* again some nasty if's to have heun in here too */
			if (mode ==1){
				VECCOPY(bp->prevpos,bp->pos);
				VECCOPY(bp->prevdx ,dx);
			}
			
			if (mode ==2){
				bp->pos[0] = bp->prevpos[0] + 0.5f * ( dx[0] + bp->prevdx[0]);
				bp->pos[1] = bp->prevpos[1] + 0.5f * ( dx[1] + bp->prevdx[1]);
				bp->pos[2] = bp->prevpos[2] + 0.5f* ( dx[2] + bp->prevdx[2]);
				maxerr = MAX2(maxerr,ABS(dx[0] - bp->prevdx[0]));
				maxerr = MAX2(maxerr,ABS(dx[1] - bp->prevdx[1]));
				maxerr = MAX2(maxerr,ABS(dx[2] - bp->prevdx[2]));
			}
			else { VECADD(bp->pos, bp->pos, dx);}
			
		}//snap
	} //for
	if (err){ /* so step size will be controlled by biggest difference in slope */
		*err = maxerr;
	}
}

/* used by heun when it overshoots */
static void softbody_restore_prev_step(Object *ob)
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	int a;	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		VECCOPY(bp->vec,bp->prevvec);
		VECCOPY(bp->pos,bp->prevpos);
	}
}


/* unused */
static void softbody_apply_goal(Object *ob, float dtime)
{

	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	float vec[3], ks;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		ks= bp->goal*dtime;
		// this is hackish, screws up physics but stabilizes
		vec[0]= ks*(bp->origT[0]-bp->pos[0]);
		vec[1]= ks*(bp->origT[1]-bp->pos[1]);
		vec[2]= ks*(bp->origT[2]-bp->pos[2]);

		VECADD(bp->pos, bp->pos, vec);
		
		ks= 1.0f-ks;
		bp->vec[0]*= ks;
		bp->vec[1]*= ks;
		bp->vec[2]*= ks;
		
	}
}

static void softbody_apply_goalsnap(Object *ob)
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		if (bp->goal >= SOFTGOALSNAP){
			VECCOPY(bp->prevpos,bp->pos);
			VECCOPY(bp->pos,bp->origT);
		}		
	}
}

static void softbody_force_goal(Object *ob)
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	int a;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {		
		VECCOPY(bp->pos,bp->origT);
		bp->vec[0] = bp->origE[0] - bp->origS[0];
		bp->vec[1] = bp->origE[1] - bp->origS[1];
		bp->vec[2] = bp->origE[2] - bp->origS[2];		
	}
}

static void interpolate_exciter(Object *ob, int timescale, int time)
{
	Mesh *me= ob->data;
	//MEdge *medge= me->medge;
	int a;
	BodyPoint *bp;
	float f;
	
	if(ob->soft) {
		f = (float)time/(float)timescale;
		bp= ob->soft->bpoint;
		for(a=0; a<me->totvert; a++, bp++) {
			bp->origT[0] = bp->origS[0] + f*(bp->origE[0] - bp->origS[0]); 
			bp->origT[1] = bp->origS[1] + f*(bp->origE[1] - bp->origS[1]); 
			bp->origT[2] = bp->origS[2] + f*(bp->origE[2] - bp->origS[2]); 
			if (bp->goal >= SOFTGOALSNAP){
				bp->vec[0] = bp->origE[0] - bp->origS[0];
				bp->vec[1] = bp->origE[1] - bp->origS[1];
				bp->vec[2] = bp->origE[2] - bp->origS[2];
			}
		}
		/* hrms .. do springs alter their lenght ?
		if(medge) {
		bs= ob->soft->bspring;
		bp= ob->soft->bpoint;
		for(a=0; (a<me->totedge && a < ob->soft->totspring ); a++, medge++, bs++) {
		bs->len= VecLenf( (bp+bs->v1)->origT, (bp+bs->v2)->origT);
		}
		}
		*/
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
	int a;
	if(ob->soft) {
	
		bp= ob->soft->bpoint;
		for(a=0; a<me->totvert; a++, mvert++, bp++) {
 			VECCOPY(bp->origS, bp->origE);
			VECCOPY(bp->origE, mvert->co);
			Mat4MulVecfl(ob->obmat, bp->origE);
			VECCOPY(bp->origT, bp->origE);
		}
		/* hrms .. do springs alter their lenght ?
		if(medge) {
			bs= ob->soft->bspring;
			bp= ob->soft->bpoint;
			for(a=0; (a<me->totedge && a < ob->soft->totspring ); a++, medge++, bs++) { 
 				bs->len= VecLenf( (bp+bs->v1)->origE, (bp+bs->v2)->origE);
			}
		}
		*/
	}

}


int get_scalar_from_named_vertexgroup(Object *ob, char *name, int vertID, float *target)
/* result 0 on success, else indicates error number
-- kind of *inverse* result defintion,
-- but this way we can signal error condition to caller  
-- and yes this function must not be here but in a *vertex group module*
*/
{
	int i,groupindex;
	bDeformGroup *locGroup = NULL;
	MDeformVert *dv;
	locGroup = get_named_vertexgroup(ob,name);
	if(locGroup){
		/* retrieve index for that group */
		groupindex =  get_defgroup_num(ob,locGroup); 
		/* spot the vert in deform vert list at mesh */
		/* todo (coder paranoya) what if ob->data is not a mesh .. */ 
		/* hrms.. would like to have the same for lattices anyhoo */
		if (((Mesh *)ob->data)->dvert) {
			dv = ((Mesh*)ob->data)->dvert + vertID;	
			/* Lets see if this vert is in the weight group */
			for (i=0; i<dv->totweight; i++){
				if (dv->dw[i].def_nr == groupindex){
					*target=dv->dw[i].weight; /* got it ! */
					return 0;
				}
			}
		}
		return 2;
	}/*if(locGroup)*/
	return 1;
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
			VECCOPY(bp->origS, bp->pos);
			VECCOPY(bp->origE, bp->pos);
			VECCOPY(bp->origT, bp->pos);
			bp->vec[0]= bp->vec[1]= bp->vec[2]= 0.0;
			bp->weight= 1.0;
			bp->goal= 0.5;
			bp->nofsprings=0;
			bp->springs=NULL;
			if (1) { /* switch to vg scalars*/
				/* get scalar values needed  *per vertex* from vertex group functions,
				   so we can *paint* them nicly .. 
				   they are normalized [0.0..1.0] so may be we need amplitude for scale
				   which can be done by caller
				   but still .. i'd like it to go this way 
				*/ 
				int error;
				char name[32] = "SOFTGOAL";
				float temp;
				error = get_scalar_from_named_vertexgroup(ob,name,me->totvert - a,&temp);
				if (!error) bp->goal = temp;
				if (bp->goal < ob->sb_mingoal) bp->goal = ob->sb_mingoal;
				if (bp->goal > ob->sb_maxgoal) bp->goal = ob->sb_maxgoal;
				/* a little ad hoc changing the goal control to be less *sharp* */
				bp->goal = (float)pow(bp->goal,4.0f);
/* to proove the concept
this would enable per vertex *mass painting*
				strcpy(name,"SOFTMASS");
				error = get_scalar_from_named_vertexgroup(ob,name,me->totvert - a,&temp);
				if (!error) bp->mass = temp * ob->rangeofmass;
*/



			} /* switch to vg scalars */
		}



		if(medge) {
			bs= ob->soft->bspring;
			bp= ob->soft->bpoint;
			for(a=me->totedge; a>0; a--, medge++, bs++) {
				bs->v1= medge->v1;
				bs->v2= medge->v2;
				bs->strength= 1.0;
				bs->len= VecLenf( (bp+bs->v1)->origS, (bp+bs->v2)->origS);
			}
		}
		
		/* insert *diagonal* springs in quads if desired */
		if (ob->softflag & 0x02) {
                add_quad_diag_springs(ob);

		}

		build_bps_springlist(ob); /* big mesh optimization */

		/* vertex colors are abused as weights here, however they're stored in faces... uhh */
        /* naah .. we don't do it any more bjornmose :-)
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
		*/
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
void object_to_softbody(Object *ob,float ctime)
{
	
	if(ob->soft) free_softbody(ob->soft);
	ob->soft= NULL;
	
	switch(ob->type) {
	case OB_MESH:
		mesh_to_softbody(ob);
		ob->soft->ctime = ctime;
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
	int timescale,t;
	float forcetime;
	float err;

    /* this is a NO! NO! 
	==========================
	if(ob->soft==NULL) {
		object_to_softbody(ob);
		if(ob->soft==NULL) return;
		ob->soft->ctime= ctime;
	}
	// you can't create a soft object on the fly
	// 1. inner spings need a *default* length for crinkles/wrinkles,
	//    surface area and volume preservation
	// 2. initial conditions for velocities and positions need to be defined
	//    for a certain point of time .. say t0 
	// 3. and since we have friction and *outer* movement
	//    the history of the *outer* movements will affect where we end up
    // sooo atm going to edit mode and back ( back calls object_to_softbody(ob,1.0f)
       is the only way to create softbody data 
	*/

	/* first attempt to set initial conditions for softbodies 
	rules
	1. ODE solving is disabled / via button in 3dview header /otherways do regular softbody stuff
	2. set SB positions to *goal*
	3. set SB velocities  to match *goal* movement

	*/
    if (SB_ENABLE == 0){
		if(ob->soft==NULL) {
			return; /* nothing to do */
		} 
		object_update_softbody(ob);
		ob->soft->ctime= ctime;
		interpolate_exciter(ob,200,200);
		softbody_force_goal(ob);
		softbody_to_object(ob);
		return; /* no dynamics wanted */
		
	}

	if(ob->soft==NULL) {
		/* aye no soft object created bail out here */
		printf("Softbody Zombie \n");
		return;
	}
	
    softbody_scale_time(steptime); // translate frames/sec and lenghts unit to SI system
	dtime= ctime - ob->soft->ctime;
	// dtime= ABS(dtime); no no we want to go back in time with IPOs
	timescale = (int)(ob->softtime * ABS(dtime)); 
	if(ABS(dtime) > 0.0) { 
		object_update_softbody(ob);
		if (ob->softflag & 0x04){
			/* special case of 2nd order Runge-Kutta type AKA Heun */
			float timedone =0.0;
			/* counter for emergency brake
			 * we don't want to lock up the system if physics fail
			 */
			int loops =0 ; 
			SoftHeunTol = ob->softtime; // humm .. this should be calculated from sb parameters and sizes

			forcetime = dtime; /* hope for integrating in one step */
			while ( (ABS(timedone) < ABS(dtime)) && (loops < 2000) )
			{
				if (ABS(dtime) > 3.0 ){
					printf("SB_STEPSIZE \n");
					break; // sorry but i must assume goal movement can't be interpolated any more
				}
				//set goals in time
				interpolate_exciter(ob,200,(int)(200.0*(timedone/dtime)));
				// do predictive euler step
				softbody_calc_forces(ob,forcetime);
				softbody_apply_forces(ob,forcetime,1, NULL);
				// crop new slope values to do averaged slope step
				softbody_calc_forces(ob,forcetime);
				softbody_apply_forces(ob,forcetime,2, &err);
				softbody_apply_goalsnap(ob);

				if (err > SoftHeunTol){ // error needs to be scaled to some quantity
					softbody_restore_prev_step(ob);
					forcetime /= 2.0;
				}
				else {
					
					float newtime = forcetime * 1.1f; // hope for 1.1 times better conditions in next step
					if (err > SoftHeunTol/2.0){ // stay with this stepsize unless err really small
						newtime = forcetime;
					}
					timedone += forcetime;
					if (forcetime > 0.0)
					forcetime = MIN2(dtime - timedone,newtime);
					else 
					forcetime = MAX2(dtime - timedone,newtime);
				}
				loops++;
			}
			// move snapped to final position
			interpolate_exciter(ob,2,2);
			softbody_apply_goalsnap(ob);
			if (loops > HEUNWARNLIMIT) /* monitor high loop counts say 1000 after testing */
			printf("%d heun integration loops/frame \n",loops);
		}
		else
        /* do brute force explicit euler */
		/* inner intagration loop */
		/* */
		// loop n times so that n*h = duration of one frame := 1
		// x(t+h) = x(t) + h*v(t);
		// v(t+h) = v(t) + h*f(x(t),t);
		for(t=1 ; t <= timescale; t++) {
			if (ABS(dtime) > 15 ) break;
			
			/* the *goal* mesh must use the n*h timing too !
			use *cheap* linear intepolation for that  */
			interpolate_exciter(ob,timescale,t);			
			if (timescale > 0 )
			{
			forcetime = dtime/timescale;

			/* does not fit the concept sloving ODEs :) */
			/*			softbody_apply_goal(ob,forcetime );  */
			
			/* explicit Euler integration */
			/* we are not controling a nuclear power plant! 
			so rought *almost* physical behaviour is acceptable.
			in cases of *mild* stiffnes cranking up timscale -> decreasing stepsize *h*
			avoids instability	*/
			softbody_calc_forces(ob,forcetime);
			softbody_apply_forces(ob,forcetime,0, NULL);
			softbody_apply_goalsnap(ob);
		

//			if (0){
				/* ok here comes the überhammer
				use a semi implicit euler integration to tackle *all* stiff conditions 
				but i doubt the cost/benifit holds for most of the cases
				-- to be coded*/
//			}
			
			}
		}
		
		/* and apply to vertices */
		 softbody_to_object(ob);
		
		ob->soft->ctime= ctime;
	} // if(ABS(dtime) > 0.0) 
	else {
    // rule : you have asked for the current state of the softobject 
	// since dtime= ctime - ob->soft->ctime;
    // and we were not notifified about any other time changes 
	// so here it is !
	softbody_to_object(ob);
	}
}

