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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_object_types.h
 *  \ingroup DNA
 *  \brief Object is a sort of wrapper for general info.
 */

#ifndef __DNA_OBJECT_TYPES_H__
#define __DNA_OBJECT_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_action_types.h" /* bAnimVizSettings */

#ifdef __cplusplus
extern "C" {
#endif
	
struct Object;
struct AnimData;
struct Ipo;
struct BoundBox;
struct Path;
struct Material;
struct bConstraintChannel;
struct PartDeflect;
struct SoftBody;
struct FluidsimSettings;
struct ParticleSystem;
struct DerivedMesh;
struct SculptSession;
struct bGPdata;


/* Vertex Groups - Name Info */
typedef struct bDeformGroup {
	struct bDeformGroup *next, *prev;
	char name[64];	/* MAX_VGROUP_NAME */
	/* need this flag for locking weights */
	char flag, pad[7];
} bDeformGroup;
#define MAX_VGROUP_NAME 64

/* bDeformGroup->flag */
#define DG_LOCK_WEIGHT 1

/**
 * The following illustrates the orientation of the 
 * bounding box in local space
 * 
 *  
 * Z  Y
 * | /
 * |/
 * .-----X
 * 
 * 
 *     2----------6
 *    /|         /|
 *   / |        / |
 *  1----------5  |
 *  |  |       |  |
 *  |  3-------|--7
 *  | /        | /
 *  |/         |/
 *  0----------4
 */
typedef struct BoundBox {
	float vec[8][3];
	int flag, pad;
} BoundBox;

/* boundbox flag */
#define OB_BB_DISABLED	1

typedef struct Object {
	ID id;
	struct AnimData *adt;		/* animation data (must be immediately after id for utilities to use it) */ 

	struct SculptSession *sculpt;
	
	short type, partype;
	int par1, par2, par3;	/* can be vertexnrs */
	char parsubstr[64];	/* String describing subobject info, MAX_ID_NAME-2 */
	struct Object *parent, *track;
	/* if ob->proxy (or proxy_group), this object is proxy for object ob->proxy */
	/* proxy_from is set in target back to the proxy. */
	struct Object *proxy, *proxy_group, *proxy_from;
	struct Ipo *ipo  DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */
	/* struct Path *path; */
	struct BoundBox *bb;
	struct bAction *action  DNA_DEPRECATED;	 // XXX depreceated... old animation system
	struct bAction *poselib;
	struct bPose *pose;  /* pose data, armature objects only */
	void *data;  /* pointer to objects data - an 'ID' or NULL */
	
	struct bGPdata *gpd;	/* Grease Pencil data */
	
	bAnimVizSettings avs;	/* settings for visualisation of object-transform animation */
	bMotionPath *mpath;		/* motion path cache for this object */
	
	ListBase constraintChannels  DNA_DEPRECATED; // XXX depreceated... old animation system
	ListBase effect  DNA_DEPRECATED;             // XXX depreceated... keep for readfile
	ListBase disp;      /* list of DispList, used by lattice, metaballs curve & surfaces */
	ListBase defbase;   /* list of bDeformGroup (vertex groups) names and flag only */
	ListBase modifiers; /* list of ModifierData structures */

	int mode;           /* Local object mode */
	int restore_mode;   /* Keep track of what mode to return to after toggling a mode */

	/* materials */
	struct Material **mat;	/* material slots */
	char *matbits;			/* a boolean field, with each byte 1 if corresponding material is linked to object */
	int totcol;				/* copy of mesh or curve or meta */
	int actcol;				/* currently selected material in the UI */
	
	/* rot en drot have to be together! (transform('r' en 's')) */
	float loc[3], dloc[3], orig[3];
	float size[3];              /* scale in fact */
	float dsize[3] DNA_DEPRECATED ; /* DEPRECATED, 2.60 and older only */
	float dscale[3];            /* ack!, changing */
	float rot[3], drot[3];		/* euler rotation */
	float quat[4], dquat[4];	/* quaternion rotation */
	float rotAxis[3], drotAxis[3];	/* axis angle rotation - axis part */
	float rotAngle, drotAngle;	/* axis angle rotation - angle part */
	float obmat[4][4];		/* final worldspace matrix with constraints & animsys applied */
	float parentinv[4][4]; /* inverse result of parent, so that object doesn't 'stick' to parent */
	float constinv[4][4]; /* inverse result of constraints. doesn't include effect of parent or object local transform */
	float imat[4][4];	/* inverse matrix of 'obmat' for any other use than rendering! */
	                    /* note: this isn't assured to be valid as with 'obmat',
	                     *       before using this value you should do...
	                     *       invert_m4_m4(ob->imat, ob->obmat); */
	
	/* Previously 'imat' was used at render time, but as other places use it too
	 * the interactive ui of 2.5 creates problems. So now only 'imat_ren' should
	 * be used when ever the inverse of ob->obmat * re->viewmat is needed! - jahka
	 */
	float imat_ren[4][4];
	
	unsigned int lay;	/* copy of Base's layer in the scene */
	
	int pad6;

	short flag;			/* copy of Base */
	short colbits DNA_DEPRECATED;		/* deprecated */
	
	short transflag, protectflag;	/* transformation settings and transform locks  */
	short trackflag, upflag;
	short nlaflag;				/* used for DopeSheet filtering settings (expanded/collapsed) */
	short ipoflag;				// xxx depreceated... old animation system
	short scaflag;				/* ui state for game logic */
	char scavisflag;			/* more display settings for game logic */
	char pad5;

	int dupon, dupoff, dupsta, dupend;

	float sf, ctime; /* sf is time-offset, ctime is the objects current time (XXX timing needs to be revised) */
	
	/* during realtime */

	/* note that inertia is only called inertia for historical reasons
	 * and is not changed to avoid DNA surgery. It actually reflects the 
	 * Size value in the GameButtons (= radius) */

	float mass, damping, inertia;
	/* The form factor k is introduced to give the user more control
	 * and to fix incompatibility problems.
	 * For rotational symmetric objects, the inertia value can be
	 * expressed as: Theta = k * m * r^2
	 * where m = Mass, r = Radius
	 * For a Sphere, the form factor is by default = 0.4
	 */

	float formfactor;
	float rdamping, sizefac;
	float margin;
	float max_vel; /* clamp the maximum velocity 0.0 is disabled */
	float min_vel; /* clamp the minimum velocity 0.0 is disabled */
	float m_contactProcessingThreshold;
	float obstacleRad;
	
	/* "Character" physics properties */
	float step_height;
	float jump_speed;
	float fall_speed;
	char pad1[4];

	short rotmode;		/* rotation mode - uses defines set out in DNA_action_types.h for PoseChannel rotations... */

	char boundtype;            /* bounding box use for drawing */
	char collision_boundtype;  /* bounding box type used for collision */

	char  restrictflag;	/* for restricting view, select, render etc. accessible in outliner */

	char dt;			/* viewport draw type */
	char dtx;			/* viewport draw extra settings */
	char empty_drawtype;
	float empty_drawsize;
	float dupfacesca;	/* dupliface scale */
	
	ListBase prop;			/* game logic property list (not to be confused with IDProperties) */
	ListBase sensors;		/* game logic sensors */
	ListBase controllers;	/* game logic controllers */
	ListBase actuators;		/* game logic actuators */

	float bbsize[3]  DNA_DEPRECATED;
	short index;			/* custom index, for renderpasses */
	unsigned short actdef;	/* current deformation group, note: index starts at 1 */
	float col[4];			/* object color */

	int gameflag;
	int gameflag2;

	struct BulletSoftBody *bsoft;	/* settings for game engine bullet soft body */

	short softflag;			/* softbody settings */
	short recalc;			/* dependency flag */
	float anisotropicFriction[3];

	ListBase constraints;		/* object constraints */
	ListBase nlastrips  DNA_DEPRECATED;			// XXX depreceated... old animation system
	ListBase hooks  DNA_DEPRECATED;				// XXX depreceated... old animation system
	ListBase particlesystem;	/* particle systems */
	
	struct PartDeflect *pd;		/* particle deflector/attractor/collision data */
	struct SoftBody *soft;		/* if exists, saved in file */
	struct Group *dup_group;	/* object duplicator for group */

	char  body_type;			/* for now used to temporarily holds the type of collision object */
	char  shapeflag;			/* flag for pinning */
	short shapenr;				/* current shape key for menu or pinned */
	float smoothresh;			/* smoothresh is phong interpolation ray_shadow correction in render */

	struct FluidsimSettings *fluidsimSettings; /* if fluidsim enabled, store additional settings */

	struct DerivedMesh *derivedDeform, *derivedFinal;
	uint64_t lastDataMask;   /* the custom data layer mask that was last used to calculate derivedDeform and derivedFinal */
	uint64_t customdata_mask; /* (extra) custom data layer mask to use for creating derivedmesh, set by depsgraph */
	unsigned int state;			/* bit masks of game controllers that are active */
	unsigned int init_state;	/* bit masks of initial state as recorded by the users */

	ListBase gpulamp;		/* runtime, for glsl lamp display only */
	ListBase pc_ids;
	ListBase *duplilist;	/* for temporary dupli list storage, only for use by RNA API */

	float ima_ofs[2];		/* offset for image empties */
} Object;

/* Warning, this is not used anymore because hooks are now modifiers */
typedef struct ObHook {
	struct ObHook *next, *prev;
	
	struct Object *parent;
	float parentinv[4][4];	/* matrix making current transform unmodified */
	float mat[4][4];		/* temp matrix while hooking */
	float cent[3];			/* visualization of hook */
	float falloff;			/* if not zero, falloff is distance where influence zero */
	
	char name[64];	/* MAX_NAME */

	int *indexar;
	int totindex, curindex; /* curindex is cache for fast lookup */
	short type, active;		/* active is only first hook, for button menu */
	float force;
} ObHook;

/* runtime only, but include here for rna access */
typedef struct DupliObject {
	struct DupliObject *next, *prev;
	struct Object *ob;
	unsigned int origlay;
	int index;
	float mat[4][4], omat[4][4];
	float orco[3], uv[2];

	short type; /* from Object.transflag */
	char no_draw, animated;

	/* Lowest-level particle index.
	 * Note: This is needed for particle info in shaders.
	 * Otherwise dupli groups in particle systems would override the
	 * index value from higher dupli levels. Would be nice to have full generic access
	 * to all dupli levels somehow, but for now this should cover most use-cases.
	 */
	int particle_index;
	int pad;
} DupliObject;

/* **************** OBJECT ********************* */

/* used many places... should be specialized  */
#define SELECT			1

/* type */
#define OB_EMPTY		0
#define OB_MESH			1
#define OB_CURVE		2
#define OB_SURF			3
#define OB_FONT			4
#define OB_MBALL		5

#define OB_LAMP			10
#define OB_CAMERA		11

#define OB_SPEAKER		12

// #define OB_WAVE			21
#define OB_LATTICE		22

/* 23 and 24 are for life and sector (old file compat.) */
#define	OB_ARMATURE		25

/* check if the object type supports materials */
#define OB_TYPE_SUPPORT_MATERIAL(_type) \
	((_type)  >= OB_MESH && (_type) <= OB_MBALL)
#define OB_TYPE_SUPPORT_VGROUP(_type) \
	(ELEM(_type, OB_MESH, OB_LATTICE))
#define OB_TYPE_SUPPORT_EDITMODE(_type) \
	(ELEM7(_type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE))

/* partype: first 4 bits: type */
#define PARTYPE			15
#define PAROBJECT		0
#define PARCURVE		1
#define PARKEY			2

#define PARSKEL			4
#define PARVERT1		5
#define PARVERT3		6
#define PARBONE			7

	/* slow parenting - is not threadsafe and/or may give errors after jumping  */
#define PARSLOW			16

/* (short) transflag */
/* flags 1 and 2 were unused or relics from past features */
#define OB_NEG_SCALE		4
#define OB_DUPLI			(8+16+256+512+2048)
#define OB_DUPLIFRAMES		8
#define OB_DUPLIVERTS		16
#define OB_DUPLIROT			32
#define OB_DUPLINOSPEED		64
/*#define OB_POWERTRACK		128*/ /*UNUSED*/
#define OB_DUPLIGROUP		256
#define OB_DUPLIFACES		512
#define OB_DUPLIFACES_SCALE	1024
#define OB_DUPLIPARTS		2048
#define OB_RENDER_DUPLI		4096
#define OB_NO_CONSTRAINTS	8192 /* runtime constraints disable */

/* (short) ipoflag */
/* XXX: many old flags for features removed due to incompatibility
 * with new system and/or other design issues were here 
 */
	/* for stride/path editing (XXX: NEEDS REVIEW) */
#define OB_DISABLE_PATH		1024

/* (short) trackflag / upflag */
#define OB_POSX			0
#define OB_POSY			1
#define OB_POSZ			2
#define OB_NEGX			3
#define OB_NEGY			4
#define OB_NEGZ			5

/* gameflag in game.h */

/* dt: no flags */
#define OB_BOUNDBOX		1
#define OB_WIRE			2
#define OB_SOLID		3
#define OB_MATERIAL		4
#define OB_TEXTURE		5
#define OB_RENDER		6

#define OB_PAINT		100	/* temporary used in draw code */

/* dtx: flags, char! */
#define OB_AXIS			2
#define OB_TEXSPACE		4
#define OB_DRAWNAME		8
#define OB_DRAWIMAGE	16
	/* for solid+wire display */
#define OB_DRAWWIRE		32
	/* for overdraw */
#define OB_DRAWXRAY		64
	/* enable transparent draw */
#define OB_DRAWTRANSP	128

/* empty_drawtype: no flags */
#define OB_ARROWS		1
#define OB_PLAINAXES	2
#define OB_CIRCLE		3
#define OB_SINGLE_ARROW	4
#define OB_CUBE			5
#define OB_EMPTY_SPHERE	6
#define OB_EMPTY_CONE	7
#define OB_EMPTY_IMAGE	8

/* boundtype */
#define OB_BOUND_BOX           0
#define OB_BOUND_SPHERE        1
#define OB_BOUND_CYLINDER      2
#define OB_BOUND_CONE          3
#define OB_BOUND_TRIANGLE_MESH 4
#define OB_BOUND_CONVEX_HULL   5
/* #define OB_BOUND_DYN_MESH   6 */ /*UNUSED*/
#define OB_BOUND_CAPSULE       7


/* **************** BASE ********************* */

/* also needed for base!!!!! or rather, thy interfere....*/
/* base->flag and ob->flag */
#define BA_WAS_SEL			2
#define BA_HAS_RECALC_OB	4
#define BA_HAS_RECALC_DATA	8

	/* NOTE: this was used as a proper setting in past, so nullify before using */
#define BA_TEMP_TAG			32

/* #define BA_FROMSET			128 */ /*UNUSED*/

#define BA_TRANSFORM_CHILD	256 /* child of a transformed object */
#define BA_TRANSFORM_PARENT	8192 /* parent of a transformed object */


/* an initial attempt as making selection more specific! */
#define BA_DESELECT		0
#define BA_SELECT		1


#define OB_FROMDUPLI		512
#define OB_DONE				1024
// #define OB_RADIO			2048	/* deprecated */
#define OB_FROMGROUP		4096

/* WARNING - when adding flags check on PSYS_RECALC */
/* ob->recalc (flag bits!) */
#define OB_RECALC_OB        (1 << 0)
#define OB_RECALC_DATA      (1 << 1)
/* time flag is set when time changes need recalc, so baked systems can ignore it */
#define OB_RECALC_TIME      (1 << 2)
/* only use for matching any flag, NOT as an argument since more flags may be added. */
#define OB_RECALC_ALL       (OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME)

/* controller state */
#define OB_MAX_STATES		30

/* ob->gameflag */
#define OB_DYNAMIC		1
#define OB_CHILD		2
#define OB_ACTOR		4
#define OB_INERTIA_LOCK_X	8
#define OB_INERTIA_LOCK_Y	16
#define OB_INERTIA_LOCK_Z	32
#define OB_DO_FH			64
#define OB_ROT_FH			128
#define OB_ANISOTROPIC_FRICTION 256
#define OB_GHOST			512
#define OB_RIGID_BODY		1024
#define OB_BOUNDS		2048

#define OB_COLLISION_RESPONSE	4096
#define OB_SECTOR		8192
#define OB_PROP			16384
#define OB_MAINACTOR	32768

#define OB_COLLISION	65536
#define OB_SOFT_BODY	0x20000
#define OB_OCCLUDER		0x40000
#define OB_SENSOR		0x80000
#define OB_NAVMESH		0x100000
#define OB_HASOBSTACLE	0x200000
#define OB_CHARACTER		0x400000

/* ob->gameflag2 */
#define OB_NEVER_DO_ACTIVITY_CULLING	1
#define OB_LOCK_RIGID_BODY_X_AXIS	4
#define OB_LOCK_RIGID_BODY_Y_AXIS	8
#define OB_LOCK_RIGID_BODY_Z_AXIS	16
#define OB_LOCK_RIGID_BODY_X_ROT_AXIS	32
#define OB_LOCK_RIGID_BODY_Y_ROT_AXIS	64
#define OB_LOCK_RIGID_BODY_Z_ROT_AXIS	128

/* #define OB_LIFE			(OB_PROP|OB_DYNAMIC|OB_ACTOR|OB_MAINACTOR|OB_CHILD) */

/* ob->body_type */
#define OB_BODY_TYPE_NO_COLLISION	0
#define OB_BODY_TYPE_STATIC			1
#define OB_BODY_TYPE_DYNAMIC		2
#define OB_BODY_TYPE_RIGID			3
#define OB_BODY_TYPE_SOFT			4
#define OB_BODY_TYPE_OCCLUDER		5
#define OB_BODY_TYPE_SENSOR			6
#define OB_BODY_TYPE_NAVMESH		7
#define OB_BODY_TYPE_CHARACTER			8

/* ob->scavisflag */
#define OB_VIS_SENS		1
#define OB_VIS_CONT		2
#define OB_VIS_ACT		4

/* ob->scaflag */
#define OB_SHOWSENS		64
#define OB_SHOWACT		128
#define OB_ADDSENS		256
#define OB_ADDCONT		512
#define OB_ADDACT		1024
#define OB_SHOWCONT		2048
#define OB_ALLSTATE		4096
#define OB_INITSTBIT	8192
#define OB_DEBUGSTATE	16384
#define OB_SHOWSTATE	32768

/* ob->restrictflag */
#define OB_RESTRICT_VIEW	1
#define OB_RESTRICT_SELECT	2
#define OB_RESTRICT_RENDER	4

/* ob->shapeflag */
#define OB_SHAPE_LOCK		1
#define OB_SHAPE_TEMPLOCK	2		// deprecated
#define OB_SHAPE_EDIT_MODE	4

/* ob->nlaflag */
	/* WARNING: flags (1<<0) and (1<<1) were from old animsys */
	/* object-channel expanded status */
#define OB_ADS_COLLAPSED	(1<<10)
	/* object's ipo-block */
#define OB_ADS_SHOWIPO		(1<<11)
	/* object's constraint channels */
#define OB_ADS_SHOWCONS		(1<<12)
	/* object's material channels */
#define OB_ADS_SHOWMATS		(1<<13)
	/* object's marticle channels */
#define OB_ADS_SHOWPARTS	(1<<14)

/* ob->protectflag */
#define OB_LOCK_LOCX	1
#define OB_LOCK_LOCY	2
#define OB_LOCK_LOCZ	4
#define OB_LOCK_LOC		7
#define OB_LOCK_ROTX	8
#define OB_LOCK_ROTY	16
#define OB_LOCK_ROTZ	32
#define OB_LOCK_ROT		56
#define OB_LOCK_SCALEX	64
#define OB_LOCK_SCALEY	128
#define OB_LOCK_SCALEZ	256
#define OB_LOCK_SCALE	448
#define OB_LOCK_ROTW	512
#define OB_LOCK_ROT4D	1024

/* ob->mode */
typedef enum ObjectMode {
	OB_MODE_OBJECT = 0,
	OB_MODE_EDIT = 1,
	OB_MODE_SCULPT = 2,
	OB_MODE_VERTEX_PAINT = 4,
	OB_MODE_WEIGHT_PAINT = 8,
	OB_MODE_TEXTURE_PAINT = 16,
	OB_MODE_PARTICLE_EDIT = 32,
	OB_MODE_POSE = 64
} ObjectMode;

/* any mode where the brush system is used */
#define OB_MODE_ALL_PAINT (OB_MODE_SCULPT|OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)

#define MAX_DUPLI_RECUR 8

#ifdef __cplusplus
}
#endif

#endif

