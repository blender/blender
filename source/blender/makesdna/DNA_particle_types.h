/* DNA_particle_types.h
 *
 *
 * $Id: DNA_particle_types.h $
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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef DNA_PARTICLE_TYPES_H
#define DNA_PARTICLE_TYPES_H

#include "DNA_ID.h"

typedef struct HairKey {
	float co[3];	/* location of hair vertex */
	float time;		/* time along hair, default 0-100 */
	float weight;	/* softbody weight */
	short editflag;	/* saved particled edit mode flags */
	short pad;
} HairKey;

typedef struct ParticleKey {	/* when changed update size of struct to copy_particleKey()!! */
	float co[3];	/* location */
	float vel[3];	/* velocity */
	float rot[4];	/* rotation quaternion */
	float ave[3];	/* angular velocity */
	float time;		/* when this key happens */
} ParticleKey;

/* Child particles are created around or between parent particles */
typedef struct ChildParticle {
	int num, parent;	/* num is face index on the final derived mesh */
	int pa[4];			/* nearest particles to the child, used for the interpolation */
	float w[4];			/* interpolation weights for the above particles */
	float fuv[4], foffset; /* face vertex weights and offset */
	float rand[3];
} ChildParticle;

/* Everything that's non dynamic for a particle:			*/
typedef struct ParticleData {
	struct Object *stick_ob;/* object that particle sticks to when dead */

	ParticleKey state;		/* normally current global coordinates or	*/
							/* in sticky object space if dead & sticky	*/

	ParticleKey prev_state; /* previous state */

	HairKey *hair;			/* hair vertices */

	ParticleKey *keys;		/* keyed states */

	float i_rot[4],r_rot[4];/* initial & random values (i_rot should be removed as it's not used anymore)*/
	float r_ave[3],r_ve[3];

	float fuv[4], foffset;	/* coordinates on face/edge number "num" and depth along*/
							/* face normal for volume emission						*/

	float time, lifetime;	/* dietime is not nescessarily time+lifetime as	*/
	float dietime;			/* particles can die unnaturally (collision)	*/

	float bank;				/* banking angle for boids */

	float size, sizemul;	/* size and multiplier so that we can update size when ever */

	int num;				/* index to vert/edge/face */
	int num_dmcache;		/* index to derived mesh data (face) to avoid slow lookups */
	int pad;

	int totkey;
	int bpi;				/* softbody body point start index */

	short flag;
	short alive;				/* the life state of a particle */
	short loop;				/* how many times particle life has looped */
	short rt2;
} ParticleData;

typedef struct ParticleSettings {
	ID id;

	int flag;
	short type, from, distr;
	/* physics modes */
	short phystype, rotmode, avemode, reactevent;
	short draw, draw_as, draw_size, childtype;
	/* number of path segments, power of 2 except */
	short draw_step, ren_step;
	short hair_step, keys_step;

	/* adaptive path rendering */
	short adapt_angle, adapt_pix;

	short disp, omat, interpolation, rotfrom, integrator;
	short kink, kink_axis, nbetween, boidneighbours;

	/* billboards */
	short bb_align, bb_uv_split, bb_anim, bb_split_offset;
	float bb_tilt, bb_rand_tilt, bb_offset[2];

	/* simplification */
	short simplify_flag, simplify_refsize;
	float simplify_rate, simplify_transition;
	float simplify_viewport;

	/* general values */
	float sta, end, lifetime, randlife;
	float timetweak, jitfac, keyed_time, eff_hair, rt;
	int totpart, userjit, grid_res;

	/* initial velocity factors */
	float normfac, obfac, randfac, partfac, tanfac, tanphase, reactfac;
	float avefac, phasefac, randrotfac, randphasefac;
	/* physical properties */
	float mass, size, randsize, reactshape;
	/* global physical properties */
	float acc[3], dragfac, brownfac, dampfac;
	/* length */
	float length, abslength, randlength;
	/* children */
	int child_nbr, ren_child_nbr;
	float parents, childsize, childrandsize;
	float childrad, childflat, childspread;
	/* clumping */
	float clumpfac, clumppow;
	/* kink */
	float kink_amp, kink_freq, kink_shape;
	/* rough */
	float rough1, rough1_size;
	float rough2, rough2_size, rough2_thres;
	float rough_end, rough_end_shape;
	/* branching */
	float branch_thres;
	/* drawing stuff */
	float draw_line[2];

	/* boids */
	float max_vel, max_lat_acc, max_tan_acc;
	float average_vel, banking, max_bank, groundz;
	float boidfac[8];
	char boidrule[8];

	struct Group *dup_group;
	struct Group *eff_group;
	struct Object *dup_ob;
	struct Object *bb_ob;
	struct Ipo *ipo;
	struct PartDeflect *pd;
	struct PartDeflect *pd2;
} ParticleSettings;

typedef struct ParticleSystem{				/* note, make sure all (runtime) are NULL's in copy_particlesystem */
	struct ParticleSystem *next, *prev;

	ParticleSettings *part;					/* particle settings */

	ParticleData *particles;				/* (parent) particles */
	ChildParticle *child;					/* child particles */

	struct ParticleEdit *edit;				/* particle editmode (runtime) */

	struct ParticleCacheKey **pathcache;	/* path cache (runtime) */
	struct ParticleCacheKey **childcache;	/* child cache (runtime) */
	ListBase pathcachebufs, childcachebufs;	/* buffers for the above */

	struct SoftBody *soft;					/* hair softbody */

	struct Object *target_ob;
	struct Object *keyed_ob;
	struct Object *lattice;

	struct ListBase effectors, reactevents;	/* runtime */
	
	float imat[4][4];	/* used for duplicators */
	float cfra;
	int seed;
	int flag, totpart, totchild, totcached, totchildcache, rt;
	short recalc, target_psys, keyed_psys, totkeyed, softflag, bakespace;

	char bb_uvname[3][32];					/* billboard uv name */

	/* if you change these remember to update array lengths to PSYS_TOT_VG! */
	short vgroup[12], vg_neg, rt3;			/* vertex groups */

	/* temporary storage during render */
	void *renderdata;

	/* point cache */
	struct PointCache *pointcache;
}ParticleSystem;

/* general particle maximums */
/* no special why's, just seem reasonable */
/* changing these (atleast upwards) should not cause any major problems */
#define MAX_PARTS			100000	/* real particles/system */
#define MAX_PART_CHILDREN	10000	/* child particles/real particles */
#define MAX_BOIDNEIGHBOURS	10		/* neigbours considered/boid */

/* part->type */
/* hair is allways baked static in object/geometry space */
/* other types (normal particles) are in global space and not static baked */
#define PART_EMITTER		0
#define PART_REACTOR		1
#define PART_HAIR			2
#define PART_FLUID			3

/* part->flag */
#define PART_REACT_STA_END	1
#define PART_REACT_MULTIPLE	2

#define PART_LOOP			4
//#define PART_LOOP_INSTANT	8

#define PART_HAIR_GEOMETRY	16

#define PART_UNBORN			32	/*show unborn particles*/
#define PART_DIED			64	/*show died particles*/

#define PART_TRAND			128	
#define PART_EDISTR			256	/* particle/face from face areas */

#define PART_STICKY			512	/*collided particles can stick to collider*/
#define PART_DIE_ON_COL		(1<<12)
#define PART_SIZE_DEFL		(1<<13) /* swept sphere deflections */
#define PART_ROT_DYN		(1<<14)	/* dynamic rotation */
#define PART_SIZEMASS		(1<<16)

#define PART_ABS_LENGTH		(1<<15)

#define PART_ABS_TIME		(1<<17)
#define PART_GLOB_TIME		(1<<18)

#define PART_BOIDS_2D		(1<<19)

#define PART_BRANCHING		(1<<20)
#define PART_ANIM_BRANCHING	(1<<21)
#define PART_SYMM_BRANCHING	(1<<24)

#define PART_HAIR_BSPLINE	1024

#define PART_GRID_INVERT	(1<<26)

#define PART_CHILD_EFFECT	(1<<27)
#define PART_CHILD_SEAMS	(1<<28)
#define PART_CHILD_RENDER	(1<<29)
#define PART_CHILD_GUIDE	(1<<30)

#define PART_SELF_EFFECT	(1<<22)

/* part->rotfrom */
#define PART_ROT_KEYS		0	/* interpolate directly from keys */
#define PART_ROT_ZINCR		1	/* same as zdir but done incrementally from previous position */
#define PART_ROT_IINCR		2	/* same as idir but done incrementally from previous position */

/* part->from */
#define PART_FROM_VERT		0
#define PART_FROM_FACE		1
#define PART_FROM_VOLUME	2
#define PART_FROM_PARTICLE	3
#define PART_FROM_CHILD		4

/* part->distr */
#define PART_DISTR_JIT		0
#define PART_DISTR_RAND		1
#define PART_DISTR_GRID		2

/* part->phystype */
#define PART_PHYS_NO		0
#define PART_PHYS_NEWTON	1
#define PART_PHYS_KEYED		2
#define PART_PHYS_BOIDS		3

/* part->kink */
#define PART_KINK_NO		0
#define PART_KINK_CURL		1
#define PART_KINK_RADIAL	2
#define PART_KINK_WAVE		3
#define PART_KINK_BRAID		4

/* part->draw */
#define PART_DRAW_VEL		1
#define PART_DRAW_ANG		2
#define PART_DRAW_SIZE		4
#define PART_DRAW_EMITTER	8	/* render emitter also */
#define PART_DRAW_KEYS		16
#define PART_DRAW_ADAPT		32
#define PART_DRAW_COS		64
#define PART_DRAW_BB_LOCK	128
#define PART_DRAW_PARENT	256
#define PART_DRAW_NUM		512
#define PART_DRAW_RAND_GR	1024
#define PART_DRAW_REN_ADAPT	2048
#define PART_DRAW_VEL_LENGTH	(1<<12)
#define PART_DRAW_MAT_COL		(1<<13)
#define PART_DRAW_WHOLE_GR		(1<<14)
#define PART_DRAW_REN_STRAND	(1<<15)

/* part->simplify_flag */
#define PART_SIMPLIFY_ENABLE	1
#define PART_SIMPLIFY_VIEWPORT	2

/* part->bb_align */
#define PART_BB_X		0
#define PART_BB_Y		1
#define PART_BB_Z		2
#define PART_BB_VIEW	3
#define PART_BB_VEL		4

/* part->bb_anim */
#define PART_BB_ANIM_NONE	0
#define PART_BB_ANIM_TIME	1
#define PART_BB_ANIM_ANGLE	2
#define PART_BB_ANIM_OFF_TIME	3
#define PART_BB_ANIM_OFF_ANGLE	4

/* part->bb_split_offset */
#define PART_BB_OFF_NONE	0
#define PART_BB_OFF_LINEAR	1
#define PART_BB_OFF_RANDOM	2

/* part->draw as */
#define PART_DRAW_NOT		0
#define PART_DRAW_DOT		1
#define PART_DRAW_CIRC		2
#define PART_DRAW_CROSS		3
#define PART_DRAW_AXIS		4
#define PART_DRAW_LINE		5
#define PART_DRAW_PATH		6
#define PART_DRAW_OB		7
#define PART_DRAW_GR		8
#define PART_DRAW_BB		9

/* part->integrator */
#define PART_INT_EULER		0
#define PART_INT_MIDPOINT	1
#define PART_INT_RK4		2

/* part->rotmode */
#define PART_ROT_NOR		1
#define PART_ROT_VEL		2
#define PART_ROT_GLOB_X		3
#define PART_ROT_GLOB_Y		4
#define PART_ROT_GLOB_Z		5
#define PART_ROT_OB_X		6
#define PART_ROT_OB_Y		7
#define PART_ROT_OB_Z		8

/* part->avemode */
#define PART_AVE_SPIN		1
#define PART_AVE_RAND		2

/* part->reactevent */
#define PART_EVENT_DEATH	0
#define PART_EVENT_COLLIDE	1
#define PART_EVENT_NEAR		2

/* part->childtype */
#define PART_CHILD_PARTICLES	1
#define PART_CHILD_FACES		2

/* psys->recalc */
#define PSYS_INIT			1
#define PSYS_DISTR			2
#define PSYS_ALLOC			4
#define PSYS_TYPE			8
#define PSYS_RECALC_HAIR	16

/* psys->flag */
#define PSYS_CURRENT		1
//#define PSYS_BAKING			2
//#define PSYS_BAKE_UI		4
#define	PSYS_KEYED_TIME		8
#define PSYS_ENABLED		16	/* deprecated */
#define PSYS_FIRST_KEYED	32
#define PSYS_DRAWING		64
//#define PSYS_SOFT_BAKE		128
#define PSYS_DELETE			256	/* remove particlesystem as soon as possible */
#define PSYS_HAIR_DONE		512
#define PSYS_KEYED			1024
#define PSYS_EDITED			2048
//#define PSYS_PROTECT_CACHE	4096
#define PSYS_DISABLED		8192

/* pars->flag */
#define PARS_UNEXIST		1
#define PARS_NO_DISP		2
#define PARS_STICKY			4
#define PARS_TRANSFORM		8
#define PARS_HIDE			16
#define PARS_TAG			32
#define PARS_REKEY			64
#define PARS_EDIT_RECALC	128

/* pars->alive */
#define PARS_KILLED			0
#define PARS_DEAD			1
#define PARS_UNBORN			2
#define PARS_ALIVE			3
#define PARS_DYING			4

/* psys->vg */
#define PSYS_TOT_VG			12

#define PSYS_VG_DENSITY		0
#define PSYS_VG_VEL			1
#define PSYS_VG_LENGTH		2
#define PSYS_VG_CLUMP		3
#define PSYS_VG_KINK		4
#define PSYS_VG_ROUGH1		5
#define PSYS_VG_ROUGH2		6
#define PSYS_VG_ROUGHE		7
#define PSYS_VG_SIZE		8
#define PSYS_VG_TAN			9
#define PSYS_VG_ROT			10
#define PSYS_VG_EFFECTOR	11

/* part->boidrules */
#define BOID_TOT_RULES		8

#define BOID_COLLIDE		0
#define BOID_AVOID			1
#define BOID_CROWD			2
#define BOID_CENTER			3
#define BOID_AV_VEL			4
#define BOID_VEL_MATCH		5
#define BOID_GOAL			6
#define BOID_LEVEL			7


//#define PSYS_INTER_CUBIC	0
//#define PSYS_INTER_LINEAR	1
//#define PSYS_INTER_CARDINAL	2
//#define PSYS_INTER_BSPLINE	3

#endif
