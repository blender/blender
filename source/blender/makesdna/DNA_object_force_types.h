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
 * The Original Code is Copyright (C) 2004-2005 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_object_force_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_OBJECT_FORCE_TYPES_H__
#define __DNA_OBJECT_FORCE_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_listBase.h"
#include "DNA_defs.h"

/* pd->forcefield:  Effector Fields types */
typedef enum ePFieldType {
	PFIELD_NULL   = 0,	/* (this is used for general effector weight)							*/
	PFIELD_FORCE  = 1,	/* Force away/towards a point depending on force strength				*/
	PFIELD_VORTEX = 2,	/* Force around the effector normal										*/
	PFIELD_MAGNET = 3,	/* Force from the cross product of effector normal and point velocity	*/
	PFIELD_WIND   = 4,	/* Force away and towards a point depending which side of the effector 	*/
				/*	 normal the point is												*/
	PFIELD_GUIDE      = 5,	/* Force along curve for dynamics, a shaping curve for hair paths		*/
	PFIELD_TEXTURE    = 6,	/* Force based on texture values calculated at point coordinates		*/
	PFIELD_HARMONIC   = 7,	/* Force of a harmonic (damped) oscillator								*/
	PFIELD_CHARGE     = 8,	/* Force away/towards a point depending on point charge					*/
	PFIELD_LENNARDJ   = 9,	/* Force due to a Lennard-Jones potential								*/
	PFIELD_BOID       = 10,	/* Defines predator / goal for boids									*/
	PFIELD_TURBULENCE = 11,	/* Force defined by BLI_gTurbulence										*/
	PFIELD_DRAG       = 12,	/* Linear & quadratic drag												*/
	PFIELD_SMOKEFLOW  = 13,	/* Force based on smoke simulation air flow								*/
	NUM_PFIELD_TYPES
} ePFieldType;

typedef struct PartDeflect {
	int	  flag;			/* general settings flag										*/
	short deflect;		/* Deflection flag - does mesh deflect particles				*/
	short forcefield;	/* Force field type, do the vertices attract / repel particles?	*/
	short falloff;		/* fall-off type												*/
	short shape;		/* point, plane or surface										*/
	short tex_mode;		/* texture effector												*/
	short kink, kink_axis; /* for curve guide											*/
	short zdir;

	/* Main effector values */
	float f_strength;	/* The strength of the force (+ or - )					*/
	float f_damp;		/* Damping ratio of the harmonic effector.				*/
	float f_flow;		/* How much force is converted into "air flow", i.e.	*/
						/* force used as the velocity of surrounding medium.	*/

	float f_size;		/* Noise size for noise effector, restlength for harmonic effector */

	/* fall-off */
	float f_power;		/* The power law - real gravitation is 2 (square)	*/
	float maxdist;		/* if indicated, use this maximum					*/
	float mindist;		/* if indicated, use this minimum					*/
	float f_power_r;	/* radial fall-off power							*/
	float maxrad;		/* radial versions of above							*/
	float minrad;

	/* particle collisions */
	float pdef_damp;	/* Damping factor for particle deflection       */
	float pdef_rdamp;	/* Random element of damping for deflection     */
	float pdef_perm;	/* Chance of particle passing through mesh      */
	float pdef_frict;	/* Friction factor for particle deflection		*/
	float pdef_rfrict;	/* Random element of friction for deflection	*/
	float pdef_stickness;/* surface particle stickiness				*/

	float absorption;	/* used for forces */

	/* softbody collisions */
	float pdef_sbdamp;	/* Damping factor for softbody deflection       */
	float pdef_sbift;	/* inner face thickness for softbody deflection */
	float pdef_sboft;	/* outer face thickness for softbody deflection */

	/* guide curve, same as for particle child effects */
	float clump_fac, clump_pow;
	float kink_freq, kink_shape, kink_amp, free_end;

	/* texture effector */
	float tex_nabla;	/* Used for calculating partial derivatives */
	struct Tex *tex;	/* Texture of the texture effector			*/

	/* effector noise */
	struct RNG *rng;	/* random noise generator for e.g. wind */
	float f_noise;		/* noise of force						*/
	int seed;			/* noise random seed					*/

	/* Display Size */
	float drawvec1[4]; /* Runtime only : start of the curve or draw scale */
	float drawvec2[4]; /* Runtime only : end of the curve */
	float drawvec_falloff_min[3], pad1; /* Runtime only */
	float drawvec_falloff_max[3], pad2; /* Runtime only */

	struct Object *f_source; /* force source object */
} PartDeflect;

typedef struct EffectorWeights {
	struct Collection *group;		/* only use effectors from this group of objects */

	float weight[14];			/* effector type specific weights */
	float global_gravity;
	short flag, rt[3];
	int pad;
} EffectorWeights;

/* EffectorWeights->flag */
#define EFF_WEIGHT_DO_HAIR		1

/* Point cache file data types:
 * - used as (1<<flag) so poke jahka if you reach the limit of 15
 * - to add new data types update:
 *		* BKE_ptcache_data_size()
 *		* ptcache_file_init_pointers()
 */
#define BPHYS_DATA_INDEX		0
#define BPHYS_DATA_LOCATION		1
#define BPHYS_DATA_SMOKE_LOW	1
#define BPHYS_DATA_VELOCITY		2
#define BPHYS_DATA_SMOKE_HIGH	2
#define BPHYS_DATA_ROTATION		3
#define BPHYS_DATA_DYNAMICPAINT 3
#define BPHYS_DATA_AVELOCITY	4	/* used for particles */
#define BPHYS_DATA_XCONST		4	/* used for cloth */
#define BPHYS_DATA_SIZE			5
#define BPHYS_DATA_TIMES		6
#define BPHYS_DATA_BOIDS		7

#define BPHYS_TOT_DATA			8

#define BPHYS_EXTRA_FLUID_SPRINGS	1

typedef struct PTCacheExtra {
	struct PTCacheExtra *next, *prev;
	unsigned int type, totdata;
	void *data;
} PTCacheExtra;

typedef struct PTCacheMem {
	struct PTCacheMem *next, *prev;
	unsigned int frame, totpoint;
	unsigned int data_types, flag;

	void *data[8]; /* BPHYS_TOT_DATA */
	void *cur[8]; /* BPHYS_TOT_DATA */

	struct ListBase extradata;
} PTCacheMem;

typedef struct PointCache {
	struct PointCache *next, *prev;
	int flag;		/* generic flag */

	int step;		/* The number of frames between cached frames.
					 * This should probably be an upper bound for a per point adaptive step in the future,
					 * buf for now it's the same for all points. Without adaptivity this can effect the perceived
					 * simulation quite a bit though. If for example particles are colliding with a horizontal
					 * plane (with high damping) they quickly come to a stop on the plane, however there are still
					 * forces acting on the particle (gravity and collisions), so the particle velocity isn't necessarily
					 * zero for the whole duration of the frame even if the particle seems stationary. If all simulation
					 * frames aren't cached (step > 1) these velocities are interpolated into movement for the non-cached
					 * frames. The result will look like the point is oscillating around the collision location. So for
					 * now cache step should be set to 1 for accurate reproduction of collisions.
					 */

	int simframe;	/* current frame of simulation (only if SIMULATION_VALID) */
	int startframe;	/* simulation start frame */
	int endframe;	/* simulation end frame */
	int editframe;	/* frame being edited (runtime only) */
	int last_exact; /* last exact frame that's cached */
	int last_valid; /* used for editing cache - what is the last baked frame */
	int pad;

	/* for external cache files */
	int totpoint;   /* number of cached points */
	int index;	/* modifier stack index */
	short compression, rt;

	char name[64];
	char prev_name[64];
	char info[64];
	char path[1024]; /* file path, 1024 = FILE_MAX */
	char *cached_frames;	/* array of length endframe-startframe+1 with flags to indicate cached frames */
							/* can be later used for other per frame flags too if needed */
	struct ListBase mem_cache;

	struct PTCacheEdit *edit;
	void (*free_edit)(struct PTCacheEdit *edit);	/* free callback */
} PointCache;

typedef struct SBVertex {
	float vec[4];
} SBVertex;

/* Container for data that is shared among CoW copies.
 *
 * This is placed in a separate struct so that values can be changed
 * without having to update all CoW copies. */
typedef struct SoftBody_Shared {
	struct PointCache *pointcache;
	struct ListBase ptcaches;
} SoftBody_Shared;

typedef struct SoftBody {
	/* dynamic data */
	int totpoint, totspring;
	struct BodyPoint *bpoint;		/* not saved in file */
	struct BodySpring *bspring;		/* not saved in file */
	char   pad;
	char   msg_lock;
	short  msg_value;

	/* part of UI: */

	/* general options */
	float nodemass;		/* softbody mass of *vertex* */
	char  namedVG_Mass[64]; /* MAX_VGROUP_NAME */
	                        /* along with it introduce mass painting
	                         * starting to fix old bug .. nastiness that VG are indexes
	                         * rather find them by name tag to find it -> jow20090613 */
	float grav;			/* softbody amount of gravitaion to apply */
	float mediafrict;	/* friction to env */
	float rklimit;		/* error limit for ODE solver */
	float physics_speed;/* user control over simulation speed */

	/* goal */
	float goalspring;	/* softbody goal springs */
	float goalfrict;	/* softbody goal springs friction */
	float mingoal;		/* quick limits for goal */
	float maxgoal;
	float defgoal;		/* default goal for vertices without vgroup */
	short vertgroup;	/* index starting at 1 */
	char  namedVG_Softgoal[64]; /* MAX_VGROUP_NAME */
	                            /* starting to fix old bug .. nastiness that VG are indexes
	                             * rather find them by name tag to find it -> jow20090613 */

	short fuzzyness;      /* */

	/* springs */
	float inspring;		/* softbody inner springs */
	float infrict;		/* softbody inner springs friction */
	char  namedVG_Spring_K[64]; /* MAX_VGROUP_NAME */
	                            /* along with it introduce Spring_K painting
	                             * starting to fix old bug .. nastiness that VG are indexes
	                             * rather find them by name tag to find it -> jow20090613 */

	/* baking */
	int sfra, efra;
	int interval;
	short local, solverflags;		/* local==1: use local coords for baking */

	/* -- these must be kept for backwards compatibility -- */
	SBVertex **keys;			/* array of size totpointkey */
	int totpointkey, totkey;	/* if totpointkey != totpoint or totkey!- (efra-sfra)/interval -> free keys */
	/* ---------------------------------------------------- */
	float secondspring;

	/* self collision*/
	float colball;		/* fixed collision ball size if > 0 */
	float balldamp;		/* cooling down collision response  */
	float ballstiff;	/* pressure the ball is loaded with  */
	short sbc_mode;
	short aeroedge,
		minloops,
		maxloops,
		choke,
		solver_ID,
		plastic, springpreload
		;

	struct SBScratch *scratch;	/* scratch pad/cache on live time not saved in file */
	float shearstiff;
	float inpush;

	struct SoftBody_Shared *shared;
	struct PointCache *pointcache DNA_DEPRECATED;  /* Moved to SoftBody_Shared */
	struct ListBase ptcaches DNA_DEPRECATED;  /* Moved to SoftBody_Shared */

	struct Collection *collision_group;

	struct EffectorWeights *effector_weights;
	/* reverse esimated obmatrix .. no need to store in blend file .. how ever who cares */
	float lcom[3];
	float lrot[3][3];
	float lscale[3][3];

	int last_frame;
} SoftBody;


/* pd->flag: various settings */
#define PFIELD_USEMAX			1
/*#define PDEFLE_DEFORM			2*/			/*UNUSED*/
#define PFIELD_GUIDE_PATH_ADD	4			/* TODO: do_versions for below */
#define PFIELD_PLANAR			8			/* used for do_versions */
#define PDEFLE_KILL_PART		16
#define PFIELD_POSZ				32			/* used for do_versions */
#define PFIELD_TEX_OBJECT		64
#define PFIELD_GLOBAL_CO		64			/* used for turbulence */
#define PFIELD_TEX_2D			128
#define PFIELD_MULTIPLE_SPRINGS	128			/* used for harmonic force */
#define PFIELD_USEMIN			256
#define PFIELD_USEMAXR			512
#define PFIELD_USEMINR			1024
#define PFIELD_TEX_ROOTCO		2048
#define PFIELD_SURFACE			(1<<12)		/* used for do_versions */
#define PFIELD_VISIBILITY		(1<<13)
#define PFIELD_DO_LOCATION		(1<<14)
#define PFIELD_DO_ROTATION		(1<<15)
#define PFIELD_GUIDE_PATH_WEIGHT (1<<16)	/* apply curve weights */
#define PFIELD_SMOKE_DENSITY    (1<<17)		/* multiply smoke force by density */
#define PFIELD_GRAVITATION		(1<<18)             /* used for (simple) force */

/* pd->falloff */
#define PFIELD_FALL_SPHERE		0
#define PFIELD_FALL_TUBE		1
#define PFIELD_FALL_CONE		2

/* pd->shape */
#define PFIELD_SHAPE_POINT		0
#define PFIELD_SHAPE_PLANE		1
#define PFIELD_SHAPE_SURFACE	2
#define PFIELD_SHAPE_POINTS		3

/* pd->tex_mode */
#define PFIELD_TEX_RGB	0
#define PFIELD_TEX_GRAD	1
#define PFIELD_TEX_CURL	2

/* pd->zdir */
#define PFIELD_Z_BOTH	0
#define PFIELD_Z_POS	1
#define PFIELD_Z_NEG	2

/* pointcache->flag */
#define PTCACHE_BAKED				1
#define PTCACHE_OUTDATED			2
#define PTCACHE_SIMULATION_VALID	4
#define PTCACHE_BAKING				8
//#define PTCACHE_BAKE_EDIT			16
//#define PTCACHE_BAKE_EDIT_ACTIVE	32
#define PTCACHE_DISK_CACHE			64
//#define PTCACHE_QUICK_CACHE		128  /* removed since 2.64 - [#30974], could be added back in a more useful way */
#define PTCACHE_FRAMES_SKIPPED		256
#define PTCACHE_EXTERNAL			512
#define PTCACHE_READ_INFO			1024
/* don't use the filename of the blendfile the data is linked from (write a local cache) */
#define PTCACHE_IGNORE_LIBPATH		2048
/* high resolution cache is saved for smoke for backwards compatibility, so set this flag to know it's a "fake" cache */
#define PTCACHE_FAKE_SMOKE			(1<<12)
#define PTCACHE_IGNORE_CLEAR		(1<<13)

/* PTCACHE_OUTDATED + PTCACHE_FRAMES_SKIPPED */
#define PTCACHE_REDO_NEEDED			258

#define PTCACHE_COMPRESS_NO			0
#define PTCACHE_COMPRESS_LZO		1
#define PTCACHE_COMPRESS_LZMA		2

/* ob->softflag */
#define OB_SB_ENABLE	1		/* deprecated, use modifier */
#define OB_SB_GOAL		2
#define OB_SB_EDGES		4
#define OB_SB_QUADS		8
#define OB_SB_POSTDEF	16
// #define OB_SB_REDO		32
// #define OB_SB_BAKESET	64
// #define OB_SB_BAKEDO	128
// #define OB_SB_RESET		256
#define OB_SB_SELF		512
#define OB_SB_FACECOLL  1024
#define OB_SB_EDGECOLL  2048
/* #define OB_SB_COLLFINAL 4096	*/ /* deprecated */
/* #define OB_SB_BIG_UI	8192 */    /* deprecated */
#define OB_SB_AERO_ANGLE	16384

/* sb->solverflags */
#define SBSO_MONITOR		1
#define SBSO_OLDERR			2
#define SBSO_ESTIMATEIPO    4

/* sb->sbc_mode */
#define SBC_MODE_MANUAL		0
#define SBC_MODE_AVG		1
#define SBC_MODE_MIN		2
#define SBC_MODE_MAX		3
#define SBC_MODE_AVGMINMAX	4

#ifdef __cplusplus
}
#endif

#endif  /* __DNA_OBJECT_FORCE_TYPES_H__ */
