/**
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
 * The Original Code is Copyright (C) 2004-2005 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DNA_OBJECT_FORCE_H
#define DNA_OBJECT_FORCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_listBase.h"
	
typedef struct PartDeflect {
	short deflect;		/* Deflection flag - does mesh deflect particles*/
	short forcefield;	/* Force field type, do the vertices attract / repel particles ? */
	short flag;			/* general settings flag */
	short falloff;		/* fall-off type*/
	
	float pdef_damp;	/* Damping factor for particle deflection       */
	float pdef_rdamp;	/* Random element of damping for deflection     */
	float pdef_perm;	/* Chance of particle passing through mesh      */
	float pdef_frict;	/* Friction factor for particle deflection		*/
	float pdef_rfrict;	/* Random element of friction for deflection	*/

	float f_strength;	/* The strength of the force (+ or - )       */
	float f_power;		/* The power law - real gravitation is 2 (square)  */
	float f_dist;
	float f_damp;		/* The dampening factor, currently only for harmonic force	*/
	float maxdist;		/* if indicated, use this maximum */
	float mindist;		/* if indicated, use this minimum */
	float maxrad;		/* radial versions of above */
	float minrad;
	float f_power_r;	/* radial fall-off power*/
	
	float pdef_sbdamp;	/* Damping factor for softbody deflection       */
	float pdef_sbift;	/* inner face thickness for softbody deflection */
	float pdef_sboft;	/* outer face thickness for softbody deflection */

	/* variables for guide curve */
	float clump_fac, clump_pow;
	float kink_freq, kink_shape, kink_amp, free_end;

	float tex_nabla;
	short tex_mode, kink, kink_axis, rt2;
	struct Tex *tex;	/* Texture of the texture effector */
	struct RNG *rng; /* random noise generator for e.g. wind */
	float f_noise; /* noise of force (currently used for wind) */
	int seed; /* wind noise random seed */
} PartDeflect;

/* Point cache file data types:
 * - used as (1<<flag) so poke jahka if you reach the limit of 15
 * - to add new data types update:
 *		* BKE_ptcache_data_size()
 *		* ptcache_file_init_pointers()
*/
#define BPHYS_DATA_INDEX		0
#define BPHYS_DATA_LOCATION		1
#define BPHYS_DATA_VELOCITY		2
#define BPHYS_DATA_ROTATION		3
#define BPHYS_DATA_AVELOCITY	4	/* used for particles */
#define BPHYS_DATA_XCONST		4	/* used for cloth */
#define BPHYS_DATA_SIZE			5
#define BPHYS_DATA_TIMES		6
#define BPHYS_DATA_BOIDS		7

#define BPHYS_TOT_DATA			8

typedef struct PTCacheMem {
	struct PTCacheMem *next, *prev;
	int frame, totpoint;
	unsigned int data_types, flag;
	int *index_array; /* quick access to stored points with index */

	void *data[8]; /* BPHYS_TOT_DATA */
	void *cur[8]; /* BPHYS_TOT_DATA */
} PTCacheMem;

typedef struct PointCache {
	struct PointCache *next, *prev;
	int flag;		/* generic flag */
	int step;		/* frames between cached frames */
	int simframe;	/* current frame of simulation (only if SIMULATION_VALID) */
	int startframe;	/* simulation start frame */
	int endframe;	/* simulation end frame */
	int editframe;	/* frame being edited (runtime only) */
	int last_exact; /* last exact frame that's cached */

	/* for external cache files */
	int totpoint;   /* number of cached points */
	int index, rt;	/* modifier stack index */
	
	char name[64];
	char prev_name[64];
	char info[64];
	char path[240]; /* file path */
	struct ListBase mem_cache;

	struct PTCacheEdit *edit;
	void (*free_edit)(struct PTCacheEdit *edit);	/* free callback */
} PointCache;

typedef struct SBVertex {
	float vec[4];
} SBVertex;

typedef struct BulletSoftBody {
	int flag;				/* various boolean options */
	float linStiff;			/* linear stiffness 0..1 */
	float	angStiff;		/* angular stiffness 0..1 */
	float	volume;			/* volume preservation 0..1 */

	int	viterations;		/* Velocities solver iterations */
	int	piterations;		/* Positions solver iterations */
	int	diterations;		/* Drift solver iterations */
	int	citerations;		/* Cluster solver iterations */

	float	kSRHR_CL;		/* Soft vs rigid hardness [0,1] (cluster only) */
	float	kSKHR_CL;		/* Soft vs kinetic hardness [0,1] (cluster only) */
	float	kSSHR_CL;		/* Soft vs soft hardness [0,1] (cluster only) */
	float	kSR_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */

	float	kSK_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
	float	kSS_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
	float	kVCF;			/* Velocities correction factor (Baumgarte) */
	float	kDP;			/* Damping coefficient [0,1] */

	float	kDG;			/* Drag coefficient [0,+inf] */
	float	kLF;			/* Lift coefficient [0,+inf] */
	float	kPR;			/* Pressure coefficient [-inf,+inf] */
	float	kVC;			/* Volume conversation coefficient [0,+inf] */

	float	kDF;			/* Dynamic friction coefficient [0,1] */
	float	kMT;			/* Pose matching coefficient [0,1] */
	float	kCHR;			/* Rigid contacts hardness [0,1] */
	float	kKHR;			/* Kinetic contacts hardness [0,1] */

	float	kSHR;			/* Soft contacts hardness [0,1] */
	float	kAHR;			/* Anchors hardness [0,1] */
	int		collisionflags;	/* Vertex/Face or Signed Distance Field(SDF) or Clusters, Soft versus Soft or Rigid */
	int		numclusteriterations;	/* number of iterations to refine collision clusters*/
	float	welding;		/* welding limit to remove duplicate/nearby vertices, 0.0..0.01 */
	float   margin;			/* margin specific to softbody */
} BulletSoftBody;

/* BulletSoftBody.flag */
#define OB_BSB_SHAPE_MATCHING	2
#define OB_BSB_UNUSED 4
#define OB_BSB_BENDING_CONSTRAINTS 8
#define OB_BSB_AERO_VPOINT 16 /* aero model, Vertex normals are oriented toward velocity*/
#define OB_BSB_AERO_VTWOSIDE 32 /* aero model, Vertex normals are flipped to match velocity */

/* BulletSoftBody.collisionflags */
#define OB_BSB_COL_SDF_RS	2 /* SDF based rigid vs soft */
#define OB_BSB_COL_CL_RS	4 /* Cluster based rigid vs soft */
#define OB_BSB_COL_CL_SS	8 /* Cluster based soft vs soft */
#define OB_BSB_COL_VF_SS	16 /* Vertex/Face based soft vs soft */


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
	char  namedVG_Mass[32]; /* along with it introduce mass painting
							starting to fix old bug .. nastyness that VG are indexes 
								rather find them by name tag to find it -> jow20090613 */
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
	char  namedVG_Softgoal[32]; /* starting to fix old bug .. nastyness that VG are indexes 
								rather find them by name tag to find it -> jow20090613 */
  
	short fuzzyness;      /* */
	
	/* springs */
	float inspring;		/* softbody inner springs */
	float infrict;		/* softbody inner springs friction */
 	char  namedVG_Spring_K[32]; /* along with it introduce Spring_K painting
							starting to fix old bug .. nastyness that VG are indexes 
								rather find them by name tag to find it -> jow20090613 */
	
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
		plastic,springpreload
		;   

	struct SBScratch *scratch;	/* scratch pad/cache on live time not saved in file */
	float shearstiff;
	float inpush;

	struct PointCache *pointcache;
	struct ListBase ptcaches;

} SoftBody;

/* pd->forcefield:  Effector Fields types */
#define PFIELD_FORCE	1
#define PFIELD_VORTEX	2
#define PFIELD_MAGNET	3
#define PFIELD_WIND		4
#define PFIELD_GUIDE	5
#define PFIELD_TEXTURE	6
#define PFIELD_HARMONIC	7
#define PFIELD_CHARGE	8
#define PFIELD_LENNARDJ	9
#define PFIELD_BOID		10


/* pd->flag: various settings */
#define PFIELD_USEMAX			1
#define PDEFLE_DEFORM			2
#define PFIELD_GUIDE_PATH_ADD	4
#define PFIELD_PLANAR			8
#define PDEFLE_KILL_PART		16
#define PFIELD_POSZ				32
#define PFIELD_TEX_OBJECT		64
#define PFIELD_TEX_2D			128
#define PFIELD_USEMIN			256
#define PFIELD_USEMAXR			512
#define PFIELD_USEMINR			1024
#define PFIELD_TEX_ROOTCO		2048
#define PFIELD_SURFACE			4096

/* pd->falloff */
#define PFIELD_FALL_SPHERE		0
#define PFIELD_FALL_TUBE		1
#define PFIELD_FALL_CONE		2
//reserved for near future
//#define PFIELD_FALL_INSIDE		3

/* pd->tex_mode */
#define PFIELD_TEX_RGB	0
#define PFIELD_TEX_GRAD	1
#define PFIELD_TEX_CURL	2

/* pointcache->flag */
#define PTCACHE_BAKED				1
#define PTCACHE_OUTDATED			2
#define PTCACHE_SIMULATION_VALID	4
#define PTCACHE_BAKING				8
//#define PTCACHE_BAKE_EDIT			16
//#define PTCACHE_BAKE_EDIT_ACTIVE	32
#define PTCACHE_DISK_CACHE			64
#define PTCACHE_QUICK_CACHE			128
#define PTCACHE_FRAMES_SKIPPED		256
#define PTCACHE_EXTERNAL			512
#define PTCACHE_READ_INFO			1024

/* PTCACHE_OUTDATED + PTCACHE_FRAMES_SKIPPED */
#define PTCACHE_REDO_NEEDED			258

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
#define OB_SB_COLLFINAL 4096
#define OB_SB_BIG_UI	8192
#define OB_SB_AERO_ANGLE	16384

/* sb->solverflags */
#define SBSO_MONITOR    1 
#define SBSO_OLDERR     2 

/* sb->sbc_mode */
#define SBC_MODE_MANUAL		0
#define SBC_MODE_AVG		1
#define SBC_MODE_MIN		2
#define SBC_MODE_MAX		3
#define SBC_MODE_AVGMINMAX	4

#ifdef __cplusplus
}
#endif

#endif

