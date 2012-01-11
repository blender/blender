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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_particle_types.h
 *  \ingroup DNA
 */

#ifndef DNA_PARTICLE_TYPES_H
#define DNA_PARTICLE_TYPES_H

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_boid_types.h"

struct AnimData;

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

typedef struct BoidParticle {
	struct Object *ground;
	struct BoidData data;
	float gravity[3];
	float wander[3];
	float rt;
} BoidParticle;

typedef struct ParticleSpring {
	float rest_length;
	unsigned int particle_index[2], delete_flag;
} ParticleSpring;

/* Child particles are created around or between parent particles */
typedef struct ChildParticle {
	int num, parent;	/* num is face index on the final derived mesh */
	int pa[4];			/* nearest particles to the child, used for the interpolation */
	float w[4];			/* interpolation weights for the above particles */
	float fuv[4], foffset; /* face vertex weights and offset */
	float rt;
} ChildParticle;

typedef struct ParticleTarget {
	struct ParticleTarget *next, *prev;
	struct Object *ob;
	int psys;
	short flag, mode;
	float time, duration;
} ParticleTarget;

typedef struct ParticleDupliWeight {
	struct ParticleDupliWeight *next, *prev;
	struct Object *ob;
	short count;
	short flag;
	short index, rt; /* only updated on file save and used on file load */
} ParticleDupliWeight;

typedef struct ParticleData {
	ParticleKey state;		/* current global coordinates */

	ParticleKey prev_state; /* previous state */
	
	HairKey *hair;			/* hair vertices */

	ParticleKey *keys;		/* keyed keys */

	BoidParticle *boid;		/* boids data */

	int totkey;				/* amount of hair or keyed keys*/

	float time, lifetime;	/* dietime is not nescessarily time+lifetime as	*/
	float dietime;			/* particles can die unnaturally (collision)	*/

	int num;				/* index to vert/edge/face */
	int num_dmcache;		/* index to derived mesh data (face) to avoid slow lookups */

	float fuv[4], foffset;	/* coordinates on face/edge number "num" and depth along*/
							/* face normal for volume emission						*/

	float size;				/* size and multiplier so that we can update size when ever */

	int hair_index;
	short flag;
	short alive;			/* the life state of a particle */
} ParticleData;

typedef struct SPHFluidSettings {
	/*Particle Fluid*/
	float radius, spring_k, rest_length;
	float plasticity_constant, yield_ratio;
	float plasticity_balance, yield_balance;
	float viscosity_omega, viscosity_beta;
	float stiffness_k, stiffness_knear, rest_density;
	float buoyancy;
	int flag, spring_frames;
} SPHFluidSettings;

/* fluid->flag */
#define SPH_VISCOELASTIC_SPRINGS	1
#define SPH_CURRENT_REST_LENGTH		2
#define SPH_FAC_REPULSION			4
#define SPH_FAC_DENSITY				8
#define SPH_FAC_RADIUS				16
#define SPH_FAC_VISCOSITY			32
#define SPH_FAC_REST_LENGTH			64

typedef struct ParticleSettings {
	ID id;
	struct AnimData *adt;

	struct BoidSettings *boids;
	struct SPHFluidSettings *fluid;

	struct EffectorWeights *effector_weights;

	int flag, rt;
	short type, from, distr, texact;
	/* physics modes */
	short phystype, rotmode, avemode, reactevent;
	short draw, draw_as, draw_size, childtype;
	short ren_as, subframes, draw_col;
	/* number of path segments, power of 2 except */
	short draw_step, ren_step;
	short hair_step, keys_step;

	/* adaptive path rendering */
	short adapt_angle, adapt_pix;

	short disp, omat, interpolation, rotfrom, integrator;
	short kink, kink_axis;

	/* billboards */
	short bb_align, bb_uv_split, bb_anim, bb_split_offset;
	float bb_tilt, bb_rand_tilt, bb_offset[2], bb_size[2], bb_vel_head, bb_vel_tail;

	/* draw color */
	float color_vec_max;

	/* simplification */
	short simplify_flag, simplify_refsize;
	float simplify_rate, simplify_transition;
	float simplify_viewport;

	/* time and emission */
	float sta, end, lifetime, randlife;
	float timetweak, courant_target;
	float jitfac, eff_hair, grid_rand, ps_offset[1];
	int totpart, userjit, grid_res, effector_amount;
	short time_flag, time_pad[3];

	/* initial velocity factors */
	float normfac, obfac, randfac, partfac, tanfac, tanphase, reactfac;
	float ob_vel[3];
	float avefac, phasefac, randrotfac, randphasefac;
	/* physical properties */
	float mass, size, randsize;
	/* global physical properties */
	float acc[3], dragfac, brownfac, dampfac;
	/* length */
	float randlength;
	/* children */
	int child_nbr, ren_child_nbr;
	float parents, childsize, childrandsize;
	float childrad, childflat;
	/* clumping */
	float clumpfac, clumppow;
	/* kink */
	float kink_amp, kink_freq, kink_shape, kink_flat;
	float kink_amp_clump;
	/* rough */
	float rough1, rough1_size;
	float rough2, rough2_size, rough2_thres;
	float rough_end, rough_end_shape;
	/* length */
	float clength, clength_thres;
	/* parting */
	float parting_fac;
	float parting_min, parting_max;
	/* branching */
	float branch_thres;
	/* drawing stuff */
	float draw_line[2];
	float path_start, path_end;
	int trail_count;
	/* keyed particles */
	int keyed_loops;

	struct MTex *mtex[18];		/* MAX_MTEX */

	struct Group *dup_group;
	struct ListBase dupliweights;
	struct Group *eff_group  DNA_DEPRECATED;		// deprecated
	struct Object *dup_ob;
	struct Object *bb_ob;
	struct Ipo *ipo  DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */
	struct PartDeflect *pd;
	struct PartDeflect *pd2;
} ParticleSettings;

typedef struct ParticleSystem
{	/* note1: make sure all (runtime) are NULL's in 'copy_particlesystem' XXX, this function is no more! - need to invstigate */
	/* note2: make sure any uses of this struct in DNA are accounted for in 'copy_object_particlesystems' */

	struct ParticleSystem *next, *prev;

	ParticleSettings *part;					/* particle settings */

	ParticleData *particles;				/* (parent) particles */
	ChildParticle *child;					/* child particles */

	struct PTCacheEdit *edit;						/* particle editmode (runtime) */
	void (*free_edit)(struct PTCacheEdit *edit);	/* free callback */

	struct ParticleCacheKey **pathcache;	/* path cache (runtime) */
	struct ParticleCacheKey **childcache;	/* child cache (runtime) */
	ListBase pathcachebufs, childcachebufs;	/* buffers for the above */

	struct ClothModifierData *clmd;					/* cloth simulation for hair */
	struct DerivedMesh *hair_in_dm, *hair_out_dm;	/* input/output for cloth simulation */

	struct Object *target_ob;
	struct Object *lattice;
	struct Object *parent;					/* particles from global space -> parent space */

	struct ListBase targets;				/* used for keyed and boid physics */

	char name[64];							/* particle system name, MAX_NAME */
	
	float imat[4][4];	/* used for duplicators */
	float cfra, tree_frame, bvhtree_frame;
	int seed, child_seed;
	int flag, totpart, totunexist, totchild, totcached, totchildcache;
	short recalc, target_psys, totkeyed, bakespace;

	char bb_uvname[3][64];					/* billboard uv name, MAX_CUSTOMDATA_LAYER_NAME */

	/* if you change these remember to update array lengths to PSYS_TOT_VG! */
	short vgroup[12], vg_neg, rt3;			/* vertex groups, 0==disable, 1==starting index */

	/* temporary storage during render */
	void *renderdata;

	/* point cache */
	struct PointCache *pointcache;
	struct ListBase ptcaches;

	struct ListBase *effectors;

	ParticleSpring *fluid_springs;
	int tot_fluidsprings, alloc_fluidsprings;

	struct KDTree *tree;								/* used for interactions with self and other systems */
	struct BVHTree *bvhtree;								/* used for interactions with self and other systems */

	struct ParticleDrawData *pdd;

	float *frand;							/* array of 1024 random floats for fast lookups */

	float dt_frac;							/* current time step, as a fraction of a frame */
	float _pad;								/* spare capacity */
} ParticleSystem;

/* part->type */
/* hair is allways baked static in object/geometry space */
/* other types (normal particles) are in global space and not static baked */
#define PART_EMITTER		0
//#define PART_REACTOR		1
#define PART_HAIR			2
#define PART_FLUID			3

/* part->flag */
#define PART_REACT_STA_END	1
#define PART_REACT_MULTIPLE	2

//#define PART_LOOP			4	/* not used anymore */
		/* for dopesheet */
#define PART_DS_EXPAND		8

#define PART_HAIR_REGROW	16	/* regrow hair for each frame */

#define PART_UNBORN			32	/*show unborn particles*/
#define PART_DIED			64	/*show died particles*/

#define PART_TRAND			128	
#define PART_EDISTR			256	/* particle/face from face areas */

//#define PART_STICKY			512	/*collided particles can stick to collider*/
#define PART_DIE_ON_COL		(1<<12)
#define PART_SIZE_DEFL		(1<<13) /* swept sphere deflections */
#define PART_ROT_DYN		(1<<14)	/* dynamic rotation */
#define PART_SIZEMASS		(1<<16)

#define PART_HIDE_ADVANCED_HAIR	(1<<15)

//#define PART_ABS_TIME		(1<<17)
//#define PART_GLOB_TIME		(1<<18)

#define PART_BOIDS_2D		(1<<19)

//#define PART_BRANCHING		(1<<20)
//#define PART_ANIM_BRANCHING	(1<<21)

#define PART_HAIR_BSPLINE	1024

#define PART_GRID_HEXAGONAL	(1<<24)
#define PART_GRID_INVERT	(1<<26)

#define PART_CHILD_EFFECT		(1<<27)
#define PART_CHILD_LONG_HAIR	(1<<28)
/* #define PART_CHILD_RENDER		(1<<29) */ /*UNUSED*/
#define PART_CHILD_GUIDE		(1<<30)

#define PART_SELF_EFFECT	(1<<22)

/* part->rotfrom */
#define PART_ROT_KEYS		0	/* interpolate directly from keys */
#define PART_ROT_ZINCR		1	/* same as zdir but done incrementally from previous position */
#define PART_ROT_IINCR		2	/* same as idir but done incrementally from previous position */

/* part->from */
#define PART_FROM_VERT		0
#define PART_FROM_FACE		1
#define PART_FROM_VOLUME	2
/* #define PART_FROM_PARTICLE	3  deprecated! */ 
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
#define PART_PHYS_FLUID		4

/* part->kink */
#define PART_KINK_NO		0
#define PART_KINK_CURL		1
#define PART_KINK_RADIAL	2
#define PART_KINK_WAVE		3
#define PART_KINK_BRAID		4

/* part->draw */
#define PART_DRAW_VEL		1
#define PART_DRAW_GLOBAL_OB	2
#define PART_DRAW_SIZE		4
#define PART_DRAW_EMITTER	8	/* render emitter also */
#define PART_DRAW_HEALTH	16
#define PART_ABS_PATH_TIME  32
#define PART_DRAW_COUNT_GR	64
#define PART_DRAW_BB_LOCK	128	/* used with billboards */
#define PART_DRAW_ROTATE_OB 128 /* used with dupliobjects/groups */
#define PART_DRAW_PARENT	256
#define PART_DRAW_NUM		512
#define PART_DRAW_RAND_GR	1024
#define PART_DRAW_REN_ADAPT	2048
#define PART_DRAW_VEL_LENGTH	(1<<12)
#define PART_DRAW_MAT_COL		(1<<13) /* deprecated, but used in do_versions */
#define PART_DRAW_WHOLE_GR		(1<<14)
#define PART_DRAW_REN_STRAND	(1<<15)

/* part->draw_col */
#define PART_DRAW_COL_NONE		0
#define PART_DRAW_COL_MAT		1
#define PART_DRAW_COL_VEL		2
#define PART_DRAW_COL_ACC		3


/* part->simplify_flag */
#define PART_SIMPLIFY_ENABLE	1
#define PART_SIMPLIFY_VIEWPORT	2

/* part->time_flag */
#define PART_TIME_AUTOSF	1 /* Automatic subframes */

/* part->bb_align */
#define PART_BB_X		0
#define PART_BB_Y		1
#define PART_BB_Z		2
#define PART_BB_VIEW	3
#define PART_BB_VEL		4

/* part->bb_anim */
#define PART_BB_ANIM_NONE	0
#define PART_BB_ANIM_AGE	1
#define PART_BB_ANIM_ANGLE	2
#define PART_BB_ANIM_FRAME	3

/* part->bb_split_offset */
#define PART_BB_OFF_NONE	0
#define PART_BB_OFF_LINEAR	1
#define PART_BB_OFF_RANDOM	2

/* part->draw_as */
/* part->ren_as*/
#define PART_DRAW_NOT		0
#define PART_DRAW_DOT		1
#define PART_DRAW_HALO		1
#define PART_DRAW_CIRC		2
#define PART_DRAW_CROSS		3
#define PART_DRAW_AXIS		4
#define PART_DRAW_LINE		5
#define PART_DRAW_PATH		6
#define PART_DRAW_OB		7
#define PART_DRAW_GR		8
#define PART_DRAW_BB		9
#define PART_DRAW_REND		10

/* part->integrator */
#define PART_INT_EULER		0
#define PART_INT_MIDPOINT	1
#define PART_INT_RK4		2
#define PART_INT_VERLET		3

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
/* starts from 8 so that the first bits can be ob->recalc */
#define PSYS_RECALC_REDO	8	/* only do pathcache etc */
#define PSYS_RECALC_RESET	16	/* reset everything including pointcache */
#define PSYS_RECALC_TYPE	32	/* handle system type change */
#define PSYS_RECALC_CHILD	64	/* only child settings changed */
#define PSYS_RECALC_PHYS	128	/* physics type changed */
#define PSYS_RECALC			248

/* psys->flag */
#define PSYS_CURRENT		1
#define PSYS_GLOBAL_HAIR	2
#define PSYS_HAIR_DYNAMICS	4
#define	PSYS_KEYED_TIMING	8
#define PSYS_ENABLED		16	/* deprecated */
#define PSYS_HAIR_UPDATED	32  /* signal for updating hair particle mode */
#define PSYS_DRAWING		64
#define PSYS_USE_IMAT		128
#define PSYS_DELETE			256	/* remove particlesystem as soon as possible */
#define PSYS_HAIR_DONE		512
#define PSYS_KEYED			1024
#define PSYS_EDITED			2048
//#define PSYS_PROTECT_CACHE	4096 /* deprecated */
#define PSYS_DISABLED		8192

/* pars->flag */
#define PARS_UNEXIST		1
#define PARS_NO_DISP		2
//#define PARS_STICKY			4 /* deprecated */
#define PARS_REKEY			8

/* pars->alive */
//#define PARS_KILLED			0 /* deprecated */
#define PARS_DEAD			1
#define PARS_UNBORN			2
#define PARS_ALIVE			3
#define PARS_DYING			4

/* ParticleDupliWeight->flag */
#define PART_DUPLIW_CURRENT	1

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

/* ParticleTarget->flag */
#define PTARGET_CURRENT		1
#define PTARGET_VALID		2

/* ParticleTarget->mode */
#define PTARGET_MODE_NEUTRAL	0
#define PTARGET_MODE_FRIEND		1
#define PTARGET_MODE_ENEMY		2

/* mapto */
/* init */
#define PAMAP_INIT		15
#define PAMAP_TIME		(1<<0)	/* emission time */
#define PAMAP_LIFE		(1<<1)	/* life time */
#define PAMAP_DENS		(1<<2)	/* density */
#define PAMAP_SIZE		(1<<3)	/* physical size */
/* reset */
#define PAMAP_IVEL		(1<<5)	/* initial velocity */
/* physics */
#define PAMAP_PHYSICS	3136
#define PAMAP_FIELD		(1<<6)	/* force fields */
#define PAMAP_GRAVITY	(1<<10)
#define PAMAP_DAMP		(1<<11)
/* children */
#define PAMAP_CHILD		912
#define PAMAP_CLUMP		(1<<7)
#define PAMAP_KINK		(1<<8)
#define PAMAP_ROUGH		(1<<9)
#define PAMAP_LENGTH	(1<<4)

#endif
