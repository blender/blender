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
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"

#include  "BIF_editdeform.h"

extern bDeformGroup *get_named_vertexgroup(Object *ob, char *name);
extern int  get_defgroup_num (Object *ob, bDeformGroup        *dg);


/* ********** soft body engine ******* */
#define SOFTGOALSNAP  0.999f 
// if bp-> goal is above make it a *forced follow original* and skip all ODE stuff for this bp
// removes *unnecessary* stiffnes from ODE system
#define HEUNWARNLIMIT 1 // 50 would be fine i think for detecting severe *stiff* stuff


float SoftHeunTol = 1.0f; // humm .. this should be calculated from sb parameters and sizes

/* local prototypes */
static void free_softbody_intern(SoftBody *sb);


/*+++ frame based timing +++*/

//physical unit of force is [kg * m / sec^2]

static float sb_grav_force_scale(Object *ob)
// since unit of g is [m/sec^2] and F = mass * g we rescale unit mass of node to 1 gramm
// put it to a function here, so we can add user options later without touching simulation code
{
	return (0.001f);
}

static float sb_fric_force_scale(Object *ob)
// rescaling unit of drag [1 / sec] to somehow reasonable
// put it to a function here, so we can add user options later without touching simulation code
{
	return (0.01f);
}

static float sb_time_scale(Object *ob)
// defining the frames to *real* time relation
{
	SoftBody *sb= ob->soft;	// is supposed to be there
	if (sb){
		return(sb->physics_speed); //hrms .. this could be IPO as well :) 
		// estimated range [0.001 sluggish slug - 100.0 very fast (i hope ODE solver can handle that)]
		// 1 approx = a unit 1 pendulum at g = 9.8 [earth conditions]  has period 65 frames
        // theory would give a 50 frames period .. so there must be something inaccurate .. looking for that (BM) 
	}
	return (1.0f);
	/* 
	this would be frames/sec independant timing assuming 25 fps is default
	but does not work very well with NLA
		return (25.0f/G.scene->r.frs_sec)
	*/
}
/*--- frame based timing ---*/


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
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;	
	BodySpring *bs;	
	int a,b;
	
	if (sb==NULL) return; // paranoya check
	
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
		// if (bp->nofsprings) printf(" node %d has %d spring links\n",a,bp->nofsprings);
	}//for bp		
}


/* creates new softbody if didn't exist yet, makes new points and springs arrays */
/* called in mesh_to_softbody */
static void renew_softbody(Object *ob, int totpoint, int totspring)  
{
	SoftBody *sb;
	
	if(ob->soft==NULL) ob->soft= sbNew();
	else free_softbody_intern(ob->soft);
	sb= ob->soft;
	   
	if(totpoint) {
		sb->totpoint= totpoint;
		sb->totspring= totspring;
		
		sb->bpoint= MEM_mallocN( totpoint*sizeof(BodyPoint), "bodypoint");
		if(totspring) 
			sb->bspring= MEM_mallocN( totspring*sizeof(BodySpring), "bodyspring");
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
		
		free_softbody_baked(sb);
	}
}


/* ************ dynamics ********** */

/* aye this belongs to arith.c */
static void Vec3PlusStVec(float *v, float s, float *v1)
{
	v[0] += s*v1[0];
	v[1] += s*v1[1];
	v[2] += s*v1[2];
}

static int sb_deflect_face(Object *ob,float *actpos, float *futurepos,float *collisionpos, float *facenormal,float *force,float *cf ,float *bounce)
{
	int deflected;
	float s_actpos[3], s_futurepos[3];
	VECCOPY(s_actpos,actpos);
	if(futurepos)
	VECCOPY(s_futurepos,futurepos);
	if (bounce) *bounce *= 1.5f;
				
				
	deflected= SoftBodyDetectCollision(s_actpos, s_futurepos, collisionpos,
					facenormal, cf, force , 1,
					G.scene->r.cfra, ob->lay, ob);
	return(deflected);
				
}

/* for future use (BM)
static int sb_deflect_edge_face(Object *ob,float *actpos, float *futurepos,float *collisionpos, float *facenormal,float *slip ,float *bounce)
{
	int deflected;
	float dummy[3],s_actpos[3], s_futurepos[3];
	SoftBody *sb= ob->soft;	// is supposed to be there
	VECCOPY(s_actpos,actpos);
	VECCOPY(s_futurepos,futurepos);
	if (slip)   *slip   *= 0.98f;
	if (bounce) *bounce *= 1.5f;
				
				
	deflected= SoftBodyDetectCollision(s_actpos, s_futurepos, collisionpos,
					facenormal, dummy, dummy , 2,
					G.scene->r.cfra, ob->lay, ob);
	return(deflected);
				
}
*/
// some functions removed here .. to help HOS on next merge (BM)

#define USES_FIELD		1
#define USES_DEFLECT	2
static int is_there_deflection(unsigned int layer)
{
	Base *base;
	int retval= 0;
	
	for(base = G.scene->base.first; base; base= base->next) {
		if( (base->lay & layer) && base->object->pd) {
			if(base->object->pd->forcefield) retval |= USES_FIELD;
			if(base->object->pd->deflect) retval |= USES_DEFLECT;
		}
	}
	return retval;
}

static void softbody_calc_forces(Object *ob, float forcetime)
{
/* rule we never alter free variables :bp->vec bp->pos in here ! 
 * this will ruin adaptive stepsize AKA heun! (BM) 
 */
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint  *bp;
	BodyPoint *bproot;
	BodySpring *bs;	
	float iks, ks, kd, gravity, actspringlen, forcefactor, sd[3],
	fieldfactor = 1000.0f, 
	windfactor  = 250.0f;   
	int a, b, do_effector;
	
	/* clear forces */
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		bp->force[0]= bp->force[1]= bp->force[2]= 0.0;
	}
	
	gravity = sb->grav * sb_grav_force_scale(ob);	
	/* check! */
	do_effector= is_there_deflection(ob->lay);
	iks  = 1.0f/(1.0f-sb->inspring)-1.0f ;/* inner spring constants function */
	bproot= sb->bpoint; /* need this for proper spring addressing */
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		if(bp->goal < SOFTGOALSNAP){ // ommit this bp when i snaps
			float auxvect[3]; // aux unit vector  
			float velgoal[3];
			float absvel =0, projvel= 0;
			
			/* do goal stuff */
			if(ob->softflag & OB_SB_GOAL) {
				/* true elastic goal */
				VecSubf(auxvect,bp->origT,bp->pos);
				ks  = 1.0f/(1.0f- bp->goal*sb->goalspring)-1.0f ;
				bp->force[0]= ks*(auxvect[0]);
				bp->force[1]= ks*(auxvect[1]);
				bp->force[2]= ks*(auxvect[2]);
				/* calulate damping forces generated by goals*/
				VecSubf(velgoal,bp->origS, bp->origE);
				kd =  sb->goalfrict * sb_fric_force_scale(ob) ;
				
				if (forcetime > 0.0 ) { // make sure friction does not become rocket motor on time reversal
					bp->force[0]-= kd * (velgoal[0] + bp->vec[0]);
					bp->force[1]-= kd * (velgoal[1] + bp->vec[1]);
					bp->force[2]-= kd * (velgoal[2] + bp->vec[2]);
				}
				else {
					bp->force[0]-= kd * (velgoal[0] - bp->vec[0]);
					bp->force[1]-= kd * (velgoal[1] - bp->vec[1]);
					bp->force[2]-= kd * (velgoal[2] - bp->vec[2]);
				}
			}
			/* done goal stuff */
			
			
			/* gravitation */
			bp->force[2]-= gravity*sb->nodemass; /* individual mass of node here */
			
			/* particle field & vortex */
			if(do_effector & USES_FIELD) {
				float force[3]= {0.0f, 0.0f, 0.0f};
				float speed[3]= {0.0f, 0.0f, 0.0f};
				float eval_sb_fric_force_scale = sb_fric_force_scale(ob); // just for calling functio once
				
				pdDoEffector(bp->pos, force, speed, (float)G.scene->r.cfra, ob->lay,PE_WIND_AS_SPEED);
				// note: now we have wind as motion of media, so we can do anisotropic stuff here, 
				// if we had vertex normals here(BM)
				/* apply forcefield*/
				VecMulf(force,fieldfactor* eval_sb_fric_force_scale); 
				VECADD(bp->force, bp->force, force);
				
				/* friction in moving media */	
				kd= sb->mediafrict* eval_sb_fric_force_scale;  
				bp->force[0] -= kd * (bp->vec[0] + windfactor*speed[0]/eval_sb_fric_force_scale);
				bp->force[1] -= kd * (bp->vec[1] + windfactor*speed[1]/eval_sb_fric_force_scale);
				bp->force[2] -= kd * (bp->vec[2] + windfactor*speed[2]/eval_sb_fric_force_scale);
				/* now we'll have nice centrifugal effect for vortex */
				
			}
			else {
				/* friction in media (not) moving*/
				kd= sb->mediafrict* sb_fric_force_scale(ob);  
				/* assume it to be proportional to actual velocity */
				bp->force[0]-= bp->vec[0]*kd;
				bp->force[1]-= bp->vec[1]*kd;
				bp->force[2]-= bp->vec[2]*kd;
				/* friction in media done */
			}
			
			/*other forces*/
			/* this is the place where other forces can be added
			yes, constraints and collision stuff should go here too (read baraff papers on that!)
			*/
			/* try to match moving collision targets */
			/* master switch to turn collision off (BM)*/
			//if(0) {
			if(do_effector & USES_DEFLECT) {
				/*sorry for decl. here i'll move 'em up when WIP is done (BM) */
				float defforce[3];
				float collisionpos[3],facenormal[3];
				float cf = 1.0f;
				float bounce = 0.5f;
				kd = 1.0f;
				defforce[0] = 0.0f;
				defforce[1] = 0.0f;
				defforce[2] = 0.0f;
				
				if (sb_deflect_face(ob,bp->pos, bp->pos, collisionpos, facenormal,defforce,&cf,&bounce)){
					bp->force[0] += defforce[0]*kd;
					bp->force[1] += defforce[1]*kd;
					bp->force[2] += defforce[2]*kd;
					bp->contactfrict = cf;
				}
				else{ 
					bp->contactfrict = 0.0f;
				}
				
			}
			
			/*other forces done*/
			/* nice things could be done with anisotropic friction
			like wind/air resistance in normal direction
			--> having a piece of cloth sailing down 
			but this needs to have a *valid* vertex normal
			*valid* means to be calulated on time axis
			hrms .. may be a rough one could be used as well .. let's see 
			*/
			
			if(ob->softflag & OB_SB_EDGES) {
				if (sb->bspring){ // spring list exists at all ? 
					for(b=bp->nofsprings;b>0;b--){
						bs = sb->bspring + bp->springs[b-1];
						if (( (sb->totpoint-a) == bs->v1) ){ 
							actspringlen= VecLenf( (bproot+bs->v2)->pos, bp->pos);
							VecSubf(sd,(bproot+bs->v2)->pos, bp->pos);
							Normalise(sd);
							
							// friction stuff V1
							VecSubf(velgoal,bp->vec,(bproot+bs->v2)->vec);
							kd = sb->infrict * sb_fric_force_scale(ob);
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
							kd = sb->infrict * sb_fric_force_scale(ob);
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
					}/* loop springs */
				}/* existing spring list */ 
			}/*any edges*/
		}/*omit on snap	*/
	}/*loop all bp's*/
}

static void softbody_apply_forces(Object *ob, float forcetime, int mode, float *err)
{
	/* time evolution */
	/* actually does an explicit euler step mode == 0 */
	/* or heun ~ 2nd order runge-kutta steps, mode 1,2 */
	SoftBody *sb= ob->soft;	// is supposed to be there
	BodyPoint *bp;
	float dx[3],dv[3];
	float timeovermass;
	float maxerr = 0.0;
	int a, do_effector;

    forcetime *= sb_time_scale(ob);
	/* check! */
	do_effector= is_there_deflection(ob->lay);
    
	// claim a minimum mass for vertex 
	if (sb->nodemass > 0.09999f) timeovermass = forcetime/sb->nodemass;
	else timeovermass = forcetime/0.09999f;
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		if(bp->goal < SOFTGOALSNAP){
			
			/* so here is (v)' = a(cceleration) = sum(F_springs)/m + gravitation + some friction forces  + more forces*/
			/* the ( ... )' operator denotes derivate respective time */
			/* the euler step for velocity then becomes */
			/* v(t + dt) = v(t) + a(t) * dt */ 
			bp->force[0]*= timeovermass; /* individual mass of node here */ 
			bp->force[1]*= timeovermass;
			bp->force[2]*= timeovermass;
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
				maxerr = MAX2(maxerr,ABS(dv[0] - bp->prevdv[0]));
				maxerr = MAX2(maxerr,ABS(dv[1] - bp->prevdv[1]));
				maxerr = MAX2(maxerr,ABS(dv[2] - bp->prevdv[2]));
			}
			else {VECADD(bp->vec, bp->vec, bp->force);}

			/* so here is (x)'= v(elocity) */
			/* the euler step for location then becomes */
			/* x(t + dt) = x(t) + v(t) * dt */ 
			
			VECCOPY(dx,bp->vec);
			dx[0]*=forcetime ; 
			dx[1]*=forcetime ; 
			dx[2]*=forcetime ; 
			
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
/* kind of hack .. while inside collision target .. make movement more *viscous* */
				if (bp->contactfrict > 0.0f){
					bp->vec[0] *= (1.0 - bp->contactfrict);
					bp->vec[1] *= (1.0 - bp->contactfrict);
					bp->vec[2] *= (1.0 - bp->contactfrict);
				}
			}
			else { VECADD(bp->pos, bp->pos, dx);}
// experimental particle collision suff was here .. just to help HOS on next merge (BM)
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
		VECCOPY(bp->vec, bp->prevvec);
		VECCOPY(bp->pos, bp->prevpos);
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

/* unused */
#if 0
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
#endif

/* expects full initialized softbody */
static void interpolate_exciter(Object *ob, int timescale, int time)
{
	SoftBody *sb= ob->soft;
	BodyPoint *bp;
	float f;
	int a;

	// note: i removed Mesh usage here, softbody should remain generic! (ton)
	
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
	
	if(ob->softflag & OB_SB_EDGES) {
		/* hrms .. do springs alter their lenght ?
		bs= ob->soft->bspring;
		bp= ob->soft->bpoint;
		for(a=0; (a<me->totedge && a < ob->soft->totspring ); a++, bs++) {
			bs->len= VecLenf( (bp+bs->v1)->origT, (bp+bs->v2)->origT);
		}
		*/
	}
}


/* ************ convertors ********** */

/*  for each object type we need;
    - xxxx_to_softbody(Object *ob)      : a full (new) copy
	- xxxx_update_softbody(Object *ob)  : update refreshes current positions
    - softbody_to_xxxx(Object *ob)      : after simulation, copy vertex locations back
*/

/* helper  call */
static int object_has_edges(Object *ob) 
{
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		if(me->medge) return 1;
	}
	else if(ob->type==OB_LATTICE) {
		;
	}
	
	return 0;
}

/* helper  call */
static void set_body_point(Object *ob, BodyPoint *bp, float *vec)
{
	
	VECCOPY(bp->pos, vec);
	Mat4MulVecfl(ob->obmat, bp->pos);  // yep, sofbody is global coords
	VECCOPY(bp->origS, bp->pos);
	VECCOPY(bp->origE, bp->pos);
	VECCOPY(bp->origT, bp->pos);
	
	bp->vec[0]= bp->vec[1]= bp->vec[2]= 0.0;
	bp->weight= 1.0;
	if(ob->softflag & OB_SB_GOAL) {
		bp->goal= ob->soft->defgoal;
	}
	else { 
		bp->goal= 0.0f; 
		/* so this will definily be below SOFTGOALSNAP */
	}
	
	bp->nofsprings= 0;
	bp->springs= NULL;
	bp->contactfrict = 0.0f;
}


/* copy original (new) situation in softbody, as result of matrices or deform */
/* is assumed to enter function with ob->soft, but can be without points */
static void mesh_update_softbody(Object *ob)
{
	Mesh *me= ob->data;
	MVert *mvert= me->mvert;
/*	MEdge *medge= me->medge;  */ /*unused*/
	BodyPoint *bp;
	int a;
	
	/* possible after a file read... */
	if(ob->soft->bpoint==NULL) sbObjectToSoftbody(ob);
	
	if(me->totvert) {
	
		bp= ob->soft->bpoint;
		for(a=0; a<me->totvert; a++, mvert++, bp++) {
 			VECCOPY(bp->origS, bp->origE);
			VECCOPY(bp->origE, mvert->co);
			Mat4MulVecfl(ob->obmat, bp->origE);
			VECCOPY(bp->origT, bp->origE);
		}
		
		if(ob->softflag & OB_SB_EDGES) {
			
			/* happens when in UI edges was set */
			if(ob->soft->bspring==NULL) 
				if(object_has_edges(ob)) sbObjectToSoftbody(ob);
		
			/* hrms .. do springs alter their lenght ? (yes, mesh keys would (ton))
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
}


static void get_scalar_from_vertexgroup(Object *ob, int vertID, short groupindex, float *target)
/* result 0 on success, else indicates error number
-- kind of *inverse* result defintion,
-- but this way we can signal error condition to caller  
-- and yes this function must not be here but in a *vertex group module*
*/
{
	MDeformVert *dv;
	int i;
	
	/* spot the vert in deform vert list at mesh */
	if(ob->type==OB_MESH) {
		if (((Mesh *)ob->data)->dvert) {
			dv = ((Mesh*)ob->data)->dvert + vertID;	
			/* Lets see if this vert is in the weight group */
			for (i=0; i<dv->totweight; i++){
				if (dv->dw[i].def_nr == groupindex){
					*target= dv->dw[i].weight; /* got it ! */
					break;
				}
			}
		}
	}
} 

/* makes totally fresh start situation */
static void mesh_to_softbody(Object *ob)
{
	SoftBody *sb;
	Mesh *me= ob->data;
	MVert *mvert= me->mvert;
	MEdge *medge= me->medge;
	BodyPoint *bp;
	BodySpring *bs;
	float goalfac;
	int a, totedge;
	
	if (ob->softflag & OB_SB_EDGES) totedge= me->totedge;
	else totedge= 0;
	
	/* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
	renew_softbody(ob, me->totvert, totedge);
		
	/* we always make body points */
	sb= ob->soft;	
	bp= sb->bpoint;
	goalfac= ABS(sb->maxgoal - sb->mingoal);
	
	for(a=me->totvert; a>0; a--, mvert++, bp++) {
		
		set_body_point(ob, bp, mvert->co);
		
		/* get scalar values needed  *per vertex* from vertex group functions,
		so we can *paint* them nicly .. 
		they are normalized [0.0..1.0] so may be we need amplitude for scale
		which can be done by caller but still .. i'd like it to go this way 
		*/ 
		
		if((ob->softflag & OB_SB_GOAL) && sb->vertgroup) {
			get_scalar_from_vertexgroup(ob, me->totvert - a, sb->vertgroup-1, &bp->goal);
			// do this always, regardless successfull read from vertex group
			bp->goal= sb->mingoal + bp->goal*goalfac;
		}
		/* a little ad hoc changing the goal control to be less *sharp* */
		bp->goal = (float)pow(bp->goal, 4.0f);
			
		/* to proove the concept
		this would enable per vertex *mass painting*
		strcpy(name,"SOFTMASS");
		error = get_scalar_from_named_vertexgroup(ob,name,me->totvert - a,&temp);
		if (!error) bp->mass = temp * ob->rangeofmass;
		*/
	}

	/* but we only optionally add body edge springs */
	if (ob->softflag & OB_SB_EDGES) {
		if(medge) {
			bs= sb->bspring;
			bp= sb->bpoint;
			for(a=me->totedge; a>0; a--, medge++, bs++) {
				bs->v1= medge->v1;
				bs->v2= medge->v2;
				bs->strength= 1.0;
				bs->len= VecLenf( (bp+bs->v1)->origS, (bp+bs->v2)->origS);
			}

		
			/* insert *diagonal* springs in quads if desired */
			if (ob->softflag & OB_SB_QUADS) {
				add_mesh_quad_diag_springs(ob);
			}

			build_bps_springlist(ob); /* big mesh optimization */
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
	SoftBody *sb;
	Lattice *lt= ob->data;
	BodyPoint *bop;
	BPoint *bp;
	int a, totvert;
	
	totvert= lt->pntsu*lt->pntsv*lt->pntsw;
	
	/* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
	renew_softbody(ob, totvert, 0);
	
	/* we always make body points */
	sb= ob->soft;	
	
	for(a= totvert, bp= lt->def, bop= sb->bpoint; a>0; a--, bp++, bop++) {
		set_body_point(ob, bop, bp->vec);
	}
}

/* copies current sofbody position */
static void softbody_to_lattice(Object *ob)
{
	SoftBody *sb;
	Lattice *lt= ob->data;
	BodyPoint *bop;
	BPoint *bp;
	int a, totvert;
	
	totvert= lt->pntsu*lt->pntsv*lt->pntsw;
	sb= ob->soft;	
	
	for(a= totvert, bp= lt->def, bop= sb->bpoint; a>0; a--, bp++, bop++) {
		VECCOPY(bp->vec, bop->pos);
		Mat4MulVecfl(ob->imat, bp->vec);	// softbody is in global coords
	}
}

/* copy original (new) situation in softbody, as result of matrices or deform */
/* is assumed to enter function with ob->soft, but can be without points */
static void lattice_update_softbody(Object *ob)
{
	Lattice *lt= ob->data;
	BodyPoint *bop;
	BPoint *bp;
	int a, totvert;
	
	totvert= lt->pntsu*lt->pntsv*lt->pntsw;
	
	/* possible after a file read... */
	if(ob->soft->bpoint==NULL) sbObjectToSoftbody(ob);
	
	for(a= totvert, bp= lt->def, bop= ob->soft->bpoint; a>0; a--, bp++, bop++) {
		VECCOPY(bop->origS, bop->origE);
		VECCOPY(bop->origE, bp->vec);
		Mat4MulVecfl(ob->obmat, bop->origE);
		VECCOPY(bop->origT, bop->origE);
	}	
}


/* copies softbody result back in object */
/* only used in sbObjectStep() */
static void softbody_to_object(Object *ob)
{
	
	if(ob->soft==NULL) return;
	
	/* inverse matrix is not uptodate... */
	Mat4Invert(ob->imat, ob->obmat);
	
	switch(ob->type) {
	case OB_MESH:
		softbody_to_mesh(ob);
		break;
	case OB_LATTICE:
		softbody_to_lattice(ob);
		break;
	}
}

/* copy original (new) situation in softbody, as result of matrices or deform */
/* used in sbObjectStep() and sbObjectReset() */
/* assumes to have ob->soft, but can be entered without points */
static void object_update_softbody(Object *ob)
{
	
	switch(ob->type) {
	case OB_MESH:
		mesh_update_softbody(ob);
		break;
	case OB_LATTICE:
		lattice_update_softbody(ob);
		break;
	}
	
}

/* return 1 if succesfully baked and applied step */
static int softbody_baked_step(Object *ob, float framenr)
{
	SoftBody *sb= ob->soft;
	SBVertex *key0, *key1, *key2, *key3;
	BodyPoint *bp;
	float data[4], sfra, efra, cfra, dfra, fac;	// start, end, current, delta 
	int ofs1, a;

	/* precondition check */
	if(sb==NULL || sb->keys==NULL || sb->totkey==0) return 0;
	/* so we got keys, but no bodypoints... even without simul we need it for the bake */
	if(sb->bpoint==NULL) sb->bpoint= MEM_callocN( sb->totpoint*sizeof(BodyPoint), "bodypoint");
	
	/* convert cfra time to system time */
	sfra= (float)sb->sfra;
	cfra= bsystem_time(ob, NULL, framenr, 0.0);
	efra= (float)sb->efra;
	dfra= (float)sb->interval;

	/* offset in keys array */
	ofs1= floor( (cfra-sfra)/dfra );

	if(ofs1 < 0) {
		key0=key1=key2=key3= *sb->keys;
	}
	else if(ofs1 >= sb->totkey-1) {
		key0=key1=key2=key3= *(sb->keys+sb->totkey-1);
	}
	else {
		key1= *(sb->keys+ofs1);
		key2= *(sb->keys+ofs1+1);

		if(ofs1>0) key0= *(sb->keys+ofs1-1);
		else key0= key1;
		
		if(ofs1<sb->totkey-2) key3= *(sb->keys+ofs1+2);
		else key3= key2;
	}
	
	sb->ctime= cfra;	// needed?
	
	/* timing */
	fac= ((cfra-sfra)/dfra) - (float)ofs1;
	CLAMP(fac, 0.0, 1.0);
	set_four_ipo(fac, data, KEY_BSPLINE);
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++, key0++, key1++, key2++, key3++) {
		bp->pos[0]= data[0]*key0->vec[0] +  data[1]*key1->vec[0] + data[2]*key2->vec[0] + data[3]*key3->vec[0];
		bp->pos[1]= data[0]*key0->vec[1] +  data[1]*key1->vec[1] + data[2]*key2->vec[1] + data[3]*key3->vec[1];
		bp->pos[2]= data[0]*key0->vec[2] +  data[1]*key1->vec[2] + data[2]*key2->vec[2] + data[3]*key3->vec[2];
	}
	
	softbody_to_object(ob);
	
	return 1;
}

/* only gets called after succesfully doing softbody_step */
/* already checked for OB_SB_BAKE flag */
static void softbody_baked_add(Object *ob, float framenr)
{
	SoftBody *sb= ob->soft;
	SBVertex *key;
	BodyPoint *bp;
	float sfra, efra, cfra, dfra, fac1;	// start, end, current, delta 
	int ofs1, a;
	
	/* convert cfra time to system time */
	sfra= (float)sb->sfra;
	cfra= bsystem_time(ob, NULL, framenr, 0.0);
	efra= (float)sb->efra;
	dfra= (float)sb->interval;
	
	if(sb->totkey==0) {
		if(sb->sfra >= sb->efra) return;		// safety, UI or py setting allows
		if(sb->interval<1) sb->interval= 1;		// just be sure
		
		sb->totkey= 1 + (int)(ceil( (efra-sfra)/dfra ) );
		sb->keys= MEM_callocN( sizeof(void *)*sb->totkey, "sb keys");
	}
	
	/* now find out if we have to store a key */
	
	/* offset in keys array */
	if(cfra==efra) {
		ofs1= sb->totkey-1;
		fac1= 0.0;
	}
	else {
		ofs1= floor( (cfra-sfra)/dfra );
		fac1= ((cfra-sfra)/dfra) - (float)ofs1;
	}	
	if( fac1 < 1.0/dfra ) {
		
		key= *(sb->keys+ofs1);
		if(key == NULL) {
			*(sb->keys+ofs1)= key= MEM_mallocN(sb->totpoint*sizeof(SBVertex), "softbody key");
			
			for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++, key++) {
				VECCOPY(key->vec, bp->pos);
			}
		}
	}
}

/* ************ Object level, exported functions *************** */

/* allocates and initializes general main data */
SoftBody *sbNew(void)
{
	SoftBody *sb;
	
	sb= MEM_callocN(sizeof(SoftBody), "softbody");
	
	sb->mediafrict= 0.5; 
	sb->nodemass= 1.0;
	sb->grav= 0.0; 
	sb->physics_speed= 1.0;
	sb->rklimit= 0.1;

	sb->goalspring= 0.5; 
	sb->goalfrict= 0.0; 
	sb->mingoal= 0.0;  
	sb->maxgoal= 1.0;
	sb->defgoal= 0.7;
	
	sb->inspring= 0.5;
	sb->infrict= 0.5; 
	
	sb->interval= 10;
	sb->sfra= G.scene->r.sfra;
	sb->efra= G.scene->r.efra;
	
	return sb;
}

/* frees all */
void sbFree(SoftBody *sb)
{
	free_softbody_intern(sb);
	MEM_freeN(sb);
}


/* makes totally fresh start situation */
void sbObjectToSoftbody(Object *ob)
{

	switch(ob->type) {
	case OB_MESH:
		mesh_to_softbody(ob);
		break;
	case OB_LATTICE:
		lattice_to_softbody(ob);
		break;
	}
	
	if(ob->soft) ob->soft->ctime= bsystem_time(ob, NULL, (float)G.scene->r.cfra, 0.0);
	ob->softflag &= ~OB_SB_REDO;
}

/* reset all motion */
void sbObjectReset(Object *ob)
{
	SoftBody *sb= ob->soft;
	BodyPoint *bp;
	int a;
	
	if(sb==NULL) return;
	if(sb->keys && sb->totkey) return; // only as cpu time saver
	
	sb->ctime= bsystem_time(ob, NULL, (float)G.scene->r.cfra, 0.0);
	
	object_update_softbody(ob);
	
	for(a=sb->totpoint, bp= sb->bpoint; a>0; a--, bp++) {
		// origS is previous timestep
		VECCOPY(bp->origS, bp->origE);
		VECCOPY(bp->pos, bp->origE);
		VECCOPY(bp->origT, bp->origE);
		bp->vec[0]= bp->vec[1]= bp->vec[2]= 0.0f;

		// no idea about the Heun stuff! (ton)
		VECCOPY(bp->prevpos, bp->pos);
		VECCOPY(bp->prevvec, bp->vec);
		VECCOPY(bp->prevdx, bp->vec);
		VECCOPY(bp->prevdv, bp->vec);
	}
}


/* simulates one step. framenr is in frames */
/* copies result back to object, displist */
void sbObjectStep(Object *ob, float framenr)
{
	SoftBody *sb;
	Base *base;
	float dtime;
	int timescale,t;
	float ctime, forcetime;
	float err;

	/* this variable is set while transform(). with lattices also having a softbody, 
	   it calls lattice_modifier() all the time... has no displist yet. Is temporal
	   hack which should be resolved with proper depgraph usage + storage of deformed
	   vertices in lattice (ton) */
	if(G.moving) return;
	
	/* baking works with global time */
	if(!(ob->softflag & OB_SB_BAKEDO) )
		if(softbody_baked_step(ob, framenr) ) return;
	
	/* remake softbody if: */
	if( (ob->softflag & OB_SB_REDO) ||		// signal after weightpainting
		(ob->soft==NULL) ||					// just to be nice we allow full init
		(ob->soft->bpoint==NULL) ) 			// after reading new file, or acceptable as signal to refresh
			sbObjectToSoftbody(ob);
	
	sb= ob->soft;

	/* still no points? go away */
	if(sb->totpoint==0) return;
	
	/* reset deflector cache, sumohandle is free, but its still sorta abuse... (ton) */
	for(base= G.scene->base.first; base; base= base->next) {
		base->object->sumohandle= NULL;
	}

	/* checking time: */
	ctime= bsystem_time(ob, NULL, framenr, 0.0);
	dtime= ctime - sb->ctime;
		// bail out for negative or for large steps
	if(dtime<0.0 || dtime >= 9.9*G.scene->r.framelen) { // G.scene->r.framelen corrects for frame-mapping, so this is actually 10 frames for UI
		sbObjectReset(ob);
		return;
	}
	
	/* the simulator */
	
	if(dtime > 0.0) {	// note: what does this mean now? (ton)
		//answer (BM) :
		//dtime is still in [frames]
		//we made sure dtime is >= 0.0
		//but still need to handle dtime == 0.0 -> just return sb as is, just to be nice
		object_update_softbody(ob);
		
		if (TRUE) {	// RSOL1 always true now (ton)
			/* special case of 2nd order Runge-Kutta type AKA Heun */
			float timedone =0.0; // how far did we get without violating error condition
			/* loops = counter for emergency brake
			 * we don't want to lock up the system if physics fail
			 */
			int loops =0 ; 
			SoftHeunTol = sb->rklimit; // humm .. this should be calculated from sb parameters and sizes

			forcetime = dtime; /* hope for integrating in one step */
			while ( (ABS(timedone) < ABS(dtime)) && (loops < 2000) )
			{
				if (ABS(dtime) > 9.0 ){
					if(G.f & G_DEBUG) printf("SB_STEPSIZE \n");
					break; // sorry but i must assume goal movement can't be interpolated any more
				}
				//set goals in time
				interpolate_exciter(ob,200,(int)(200.0*(timedone/dtime)));
				// do predictive euler step
				softbody_calc_forces(ob, forcetime);
				softbody_apply_forces(ob, forcetime, 1, NULL);
				// crop new slope values to do averaged slope step
				softbody_calc_forces(ob, forcetime);
				softbody_apply_forces(ob, forcetime, 2, &err);
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
			interpolate_exciter(ob, 2, 2);
			softbody_apply_goalsnap(ob);
			
			if(G.f & G_DEBUG) {
				if (loops > HEUNWARNLIMIT) /* monitor high loop counts say 1000 after testing */
					printf("%d heun integration loops/frame \n",loops);
			}
		}
		else{
			/* do brute force explicit euler */
			/* inner intagration loop */
			/* */
			// loop n times so that n*h = duration of one frame := 1
			// x(t+h) = x(t) + h*v(t);
			// v(t+h) = v(t) + h*f(x(t),t);
			timescale = (int)(sb->rklimit * ABS(dtime)); 
			for(t=1 ; t <= timescale; t++) {
				if (ABS(dtime) > 15 ) break;
				
				/* the *goal* mesh must use the n*h timing too !
				use *cheap* linear intepolation for that  */
				interpolate_exciter(ob,timescale,t);			
				if (timescale > 0 ) {
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
					
					//	if (0){
					/* ok here comes the überhammer
					use a semi implicit euler integration to tackle *all* stiff conditions 
					but i doubt the cost/benifit holds for most of the cases
					-- to be coded*/
					//	}
					
				}
			}
		}
		
		/* and apply to vertices */
		 softbody_to_object(ob);
		
		sb->ctime= ctime;
	} // if(ABS(dtime) > 0.0) 
	else {
		// rule : you have asked for the current state of the softobject 
		// since dtime= ctime - ob->soft->ctime== 0.0;
		// and we were not notifified about any other time changes 
		// so here it is !
		softbody_to_object(ob);
	}

	/* reset deflector cache */
	for(base= G.scene->base.first; base; base= base->next) {
		if(base->object->sumohandle) {
			MEM_freeN(base->object->sumohandle);
			base->object->sumohandle= NULL;
		}
	}
	
	if(ob->softflag & OB_SB_BAKEDO) softbody_baked_add(ob, framenr);
}

