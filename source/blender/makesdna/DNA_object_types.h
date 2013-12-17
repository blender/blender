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
struct RigidBodyOb;


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
 * <pre>
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
 * </pre>
 */
typedef struct BoundBox {
	float vec[8][3];
	int flag, pad;
} BoundBox;

/* boundbox flag */
enum {
	BOUNDBOX_DISABLED = (1 << 0),
	BOUNDBOX_DIRTY  = (1 << 1),
};

typedef struct LodLevel {
	struct LodLevel *next, *prev;
	struct Object *source;
	int flags;
	float distance;
} LodLevel;

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
	struct bAction *action  DNA_DEPRECATED;	 // XXX deprecated... old animation system
	struct bAction *poselib;
	struct bPose *pose;  /* pose data, armature objects only */
	void *data;  /* pointer to objects data - an 'ID' or NULL */
	
	struct bGPdata *gpd;	/* Grease Pencil data */
	
	bAnimVizSettings avs;	/* settings for visualization of object-transform animation */
	bMotionPath *mpath;		/* motion path cache for this object */
	
	ListBase constraintChannels  DNA_DEPRECATED; // XXX deprecated... old animation system
	ListBase effect  DNA_DEPRECATED;             // XXX deprecated... keep for readfile
	ListBase defbase;   /* list of bDeformGroup (vertex groups) names and flag only */
	ListBase modifiers; /* list of ModifierData structures */

	int mode;           /* Local object mode */
	int restore_mode;   /* Keep track of what mode to return to after toggling a mode */

	/* materials */
	struct Material **mat;	/* material slots */
	char *matbits;			/* a boolean field, with each byte 1 if corresponding material is linked to object */
	int totcol;				/* copy of mesh, curve & meta struct member of same name (keep in sync) */
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
	
	float sf; /* sf is time-offset */

	short flag;			/* copy of Base */
	short colbits DNA_DEPRECATED;		/* deprecated, use 'matbits' */
	
	short transflag, protectflag;	/* transformation settings and transform locks  */
	short trackflag, upflag;
	short nlaflag;				/* used for DopeSheet filtering settings (expanded/collapsed) */
	short ipoflag;				// xxx deprecated... old animation system
	short scaflag;				/* ui state for game logic */
	char scavisflag;			/* more display settings for game logic */
	char depsflag;

	int dupon, dupoff, dupsta, dupend;

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

	/** Collision mask settings */
	unsigned short col_group, col_mask;

	short rotmode;		/* rotation mode - uses defines set out in DNA_action_types.h for PoseChannel rotations... */

	char boundtype;            /* bounding box use for drawing */
	char collision_boundtype;  /* bounding box type used for collision */

	short dtx;			/* viewport draw extra settings */
	char dt;			/* viewport draw type */
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

	char restrictflag;		/* for restricting view, select, render etc. accessible in outliner */
	char recalc;			/* dependency flag */
	short softflag;			/* softbody settings */
	float anisotropicFriction[3];

	ListBase constraints;		/* object constraints */
	ListBase nlastrips  DNA_DEPRECATED;			// XXX deprecated... old animation system
	ListBase hooks  DNA_DEPRECATED;				// XXX deprecated... old animation system
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
	int *pad;
	uint64_t lastDataMask;   /* the custom data layer mask that was last used to calculate derivedDeform and derivedFinal */
	uint64_t customdata_mask; /* (extra) custom data layer mask to use for creating derivedmesh, set by depsgraph */
	unsigned int state;			/* bit masks of game controllers that are active */
	unsigned int init_state;	/* bit masks of initial state as recorded by the users */

	ListBase gpulamp;		/* runtime, for glsl lamp display only */
	ListBase pc_ids;
	ListBase *duplilist;	/* for temporary dupli list storage, only for use by RNA API */
	
	struct RigidBodyOb *rigidbody_object;		/* settings for Bullet rigid body */
	struct RigidBodyCon *rigidbody_constraint;	/* settings for Bullet constraint */

	float ima_ofs[2];		/* offset for image empties */

	ListBase lodlevels;		/* contains data for levels of detail */
	LodLevel *currentlod;

	/* Runtime valuated curve-specific data, not stored in the file */
	struct CurveCache *curve_cache;
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
	unsigned int origlay, pad;
	float mat[4][4], omat[4][4];
	float orco[3], uv[2];

	short type; /* from Object.transflag */
	char no_draw, animated;

	/* persistent identifier for a dupli object, for inter-frame matching of
	 * objects with motion blur, or inter-update matching for syncing */
	int persistent_id[8]; /* MAX_DUPLI_RECUR */

	/* particle this dupli was generated from */
	struct ParticleSystem *particle_system;
} DupliObject;

/* **************** OBJECT ********************* */

/* used many places... should be specialized  */
#define SELECT          1

/* type */
enum {
	OB_EMPTY      = 0,
	OB_MESH       = 1,
	OB_CURVE      = 2,
	OB_SURF       = 3,
	OB_FONT       = 4,
	OB_MBALL      = 5,

	OB_LAMP       = 10,
	OB_CAMERA     = 11,

	OB_SPEAKER    = 12,

/*	OB_WAVE       = 21, */
	OB_LATTICE    = 22,

/* 23 and 24 are for life and sector (old file compat.) */
	OB_ARMATURE   = 25,
};

/* check if the object type supports materials */
#define OB_TYPE_SUPPORT_MATERIAL(_type) \
	((_type) >= OB_MESH && (_type) <= OB_MBALL)
#define OB_TYPE_SUPPORT_VGROUP(_type) \
	(ELEM(_type, OB_MESH, OB_LATTICE))
#define OB_TYPE_SUPPORT_EDITMODE(_type) \
	(ELEM7(_type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE))
#define OB_TYPE_SUPPORT_PARVERT(_type) \
	(ELEM4(_type, OB_MESH, OB_SURF, OB_CURVE, OB_LATTICE))

/* is this ID type used as object data */
#define OB_DATA_SUPPORT_ID(_id_type) \
	(ELEM8(_id_type, ID_ME, ID_CU, ID_MB, ID_LA, ID_SPK, ID_CA, ID_LT, ID_AR))

#define OB_DATA_SUPPORT_ID_CASE \
	ID_ME: case ID_CU: case ID_MB: case ID_LA: case ID_SPK: case ID_CA: case ID_LT: case ID_AR

/* partype: first 4 bits: type */
enum {
	PARTYPE       = (1 << 4) - 1,
	PAROBJECT     = 0,
	PARCURVE      = 1,
	PARKEY        = 2,

	PARSKEL       = 4,
	PARVERT1      = 5,
	PARVERT3      = 6,
	PARBONE       = 7,

	/* slow parenting - is not threadsafe and/or may give errors after jumping  */
	PARSLOW       = 16,
};

/* (short) transflag */
/* flags 1 and 2 were unused or relics from past features */
enum {
	OB_NEG_SCALE        = 1 << 2,
	OB_DUPLIFRAMES      = 1 << 3,
	OB_DUPLIVERTS       = 1 << 4,
	OB_DUPLIROT         = 1 << 5,
	OB_DUPLINOSPEED     = 1 << 6,
/*	OB_POWERTRACK       = 1 << 7,*/ /*UNUSED*/
	OB_DUPLIGROUP       = 1 << 8,
	OB_DUPLIFACES       = 1 << 9,
	OB_DUPLIFACES_SCALE = 1 << 10,
	OB_DUPLIPARTS       = 1 << 11,
	OB_RENDER_DUPLI     = 1 << 12,
	OB_NO_CONSTRAINTS   = 1 << 13,  /* runtime constraints disable */
	OB_NO_PSYS_UPDATE   = 1 << 14,  /* hack to work around particle issue */

	OB_DUPLI            = OB_DUPLIFRAMES | OB_DUPLIVERTS | OB_DUPLIGROUP | OB_DUPLIFACES | OB_DUPLIPARTS,
};

/* (short) ipoflag */
/* XXX: many old flags for features removed due to incompatibility
 * with new system and/or other design issues were here 
 */
	/* for stride/path editing (XXX: NEEDS REVIEW) */
#define OB_DISABLE_PATH     (1 << 10)

/* (short) trackflag / upflag */
enum {
	OB_POSX = 0,
	OB_POSY = 1,
	OB_POSZ = 2,
	OB_NEGX = 3,
	OB_NEGY = 4,
	OB_NEGZ = 5,
};

/* gameflag in game.h */

/* dt: no flags */
enum {
	OB_BOUNDBOX  = 1,
	OB_WIRE      = 2,
	OB_SOLID     = 3,
	OB_MATERIAL  = 4,
	OB_TEXTURE   = 5,
	OB_RENDER    = 6,

	OB_PAINT     = 100,  /* temporary used in draw code */
};

/* dtx: flags (short) */
enum {
	OB_DRAWBOUNDOX    = 1 << 0,
	OB_AXIS           = 1 << 1,
	OB_TEXSPACE       = 1 << 2,
	OB_DRAWNAME       = 1 << 3,
	OB_DRAWIMAGE      = 1 << 4,
	/* for solid+wire display */
	OB_DRAWWIRE       = 1 << 5,
	/* for overdraw s*/
	OB_DRAWXRAY       = 1 << 6,
	/* enable transparent draw */
	OB_DRAWTRANSP     = 1 << 7,
	OB_DRAW_ALL_EDGES = 1 << 8,  /* only for meshes currently */
};

/* empty_drawtype: no flags */
enum {
	OB_ARROWS        = 1,
	OB_PLAINAXES     = 2,
	OB_CIRCLE        = 3,
	OB_SINGLE_ARROW  = 4,
	OB_CUBE          = 5,
	OB_EMPTY_SPHERE  = 6,
	OB_EMPTY_CONE    = 7,
	OB_EMPTY_IMAGE   = 8,
};

/* boundtype */
enum {
	OB_BOUND_BOX           = 0,
	OB_BOUND_SPHERE        = 1,
	OB_BOUND_CYLINDER      = 2,
	OB_BOUND_CONE          = 3,
	OB_BOUND_TRIANGLE_MESH = 4,
	OB_BOUND_CONVEX_HULL   = 5,
/*	OB_BOUND_DYN_MESH      = 6, */ /*UNUSED*/
	OB_BOUND_CAPSULE       = 7,
};

/* lod flags */
enum {
	OB_LOD_USE_MESH		= 1 << 0,
	OB_LOD_USE_MAT		= 1 << 1,
};


/* **************** BASE ********************* */

/* also needed for base!!!!! or rather, they interfere....*/
/* base->flag and ob->flag */
#define BA_WAS_SEL          (1 << 1)
#define BA_HAS_RECALC_OB    (1 << 2)
#define BA_HAS_RECALC_DATA  (1 << 3)

	/* NOTE: this was used as a proper setting in past, so nullify before using */
#define BA_TEMP_TAG         (1 << 5)

/* #define BA_FROMSET          (1 << 7) */ /*UNUSED*/

#define BA_TRANSFORM_CHILD  (1 << 8)  /* child of a transformed object */
#define BA_TRANSFORM_PARENT (1 << 13)  /* parent of a transformed object */


/* an initial attempt as making selection more specific! */
#define BA_DESELECT     0
#define BA_SELECT       1


#define OB_FROMDUPLI        (1 << 9)
#define OB_DONE             (1 << 10)
/* #define OB_RADIO            (1 << 11) */  /* deprecated */
#define OB_FROMGROUP        (1 << 12)

/* WARNING - when adding flags check on PSYS_RECALC */
/* ob->recalc (flag bits!) */
enum {
	OB_RECALC_OB        = 1 << 0,
	OB_RECALC_DATA      = 1 << 1,
/* time flag is set when time changes need recalc, so baked systems can ignore it */
	OB_RECALC_TIME      = 1 << 2,
/* only use for matching any flag, NOT as an argument since more flags may be added. */
	OB_RECALC_ALL       = OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME,
};

/* controller state */
#define OB_MAX_STATES       30

/* collision masks */
#define OB_MAX_COL_MASKS    8

/* ob->gameflag */
enum {
	OB_DYNAMIC               = 1 << 0,
	OB_CHILD                 = 1 << 1,
	OB_ACTOR                 = 1 << 2,
	OB_INERTIA_LOCK_X        = 1 << 3,
	OB_INERTIA_LOCK_Y        = 1 << 4,
	OB_INERTIA_LOCK_Z        = 1 << 5,
	OB_DO_FH                 = 1 << 6,
	OB_ROT_FH                = 1 << 7,
	OB_ANISOTROPIC_FRICTION  = 1 << 8,
	OB_GHOST                 = 1 << 9,
	OB_RIGID_BODY            = 1 << 10,
	OB_BOUNDS                = 1 << 11,

	OB_COLLISION_RESPONSE    = 1 << 12,
	OB_SECTOR                = 1 << 13,
	OB_PROP                  = 1 << 14,
	OB_MAINACTOR             = 1 << 15,

	OB_COLLISION             = 1 << 16,
	OB_SOFT_BODY             = 1 << 17,
	OB_OCCLUDER              = 1 << 18,
	OB_SENSOR                = 1 << 19,
	OB_NAVMESH               = 1 << 20,
	OB_HASOBSTACLE           = 1 << 21,
	OB_CHARACTER             = 1 << 22,

	OB_RECORD_ANIMATION      = 1 << 23,
};

/* ob->gameflag2 */
enum {
	OB_NEVER_DO_ACTIVITY_CULLING    = 1 << 0,
	OB_LOCK_RIGID_BODY_X_AXIS       = 1 << 2,
	OB_LOCK_RIGID_BODY_Y_AXIS       = 1 << 3,
	OB_LOCK_RIGID_BODY_Z_AXIS       = 1 << 4,
	OB_LOCK_RIGID_BODY_X_ROT_AXIS   = 1 << 5,
	OB_LOCK_RIGID_BODY_Y_ROT_AXIS   = 1 << 6,
	OB_LOCK_RIGID_BODY_Z_ROT_AXIS   = 1 << 7,

/*	OB_LIFE     = OB_PROP | OB_DYNAMIC | OB_ACTOR | OB_MAINACTOR | OB_CHILD, */
};

/* ob->body_type */
enum {
	OB_BODY_TYPE_NO_COLLISION   = 0,
	OB_BODY_TYPE_STATIC         = 1,
	OB_BODY_TYPE_DYNAMIC        = 2,
	OB_BODY_TYPE_RIGID          = 3,
	OB_BODY_TYPE_SOFT           = 4,
	OB_BODY_TYPE_OCCLUDER       = 5,
	OB_BODY_TYPE_SENSOR         = 6,
	OB_BODY_TYPE_NAVMESH        = 7,
	OB_BODY_TYPE_CHARACTER      = 8,
};

/* ob->depsflag */
enum {
	OB_DEPS_EXTRA_OB_RECALC     = 1 << 0,
	OB_DEPS_EXTRA_DATA_RECALC   = 1 << 1,
};

/* ob->scavisflag */
enum {
	OB_VIS_SENS     = 1 << 0,
	OB_VIS_CONT     = 1 << 1,
	OB_VIS_ACT      = 1 << 2,
};

/* ob->scaflag */
enum {
	OB_SHOWSENS     = 1 << 6,
	OB_SHOWACT      = 1 << 7,
	OB_ADDSENS      = 1 << 8,
	OB_ADDCONT      = 1 << 9,
	OB_ADDACT       = 1 << 10,
	OB_SHOWCONT     = 1 << 11,
	OB_ALLSTATE     = 1 << 12,
	OB_INITSTBIT    = 1 << 13,
	OB_DEBUGSTATE   = 1 << 14,
	OB_SHOWSTATE    = 1 << 15,
};

/* ob->restrictflag */
enum {
	OB_RESTRICT_VIEW    = 1 << 0,
	OB_RESTRICT_SELECT  = 1 << 1,
	OB_RESTRICT_RENDER  = 1 << 2,
};

/* ob->shapeflag */
enum {
	OB_SHAPE_LOCK       = 1 << 0,
	// OB_SHAPE_TEMPLOCK   = 1 << 1,  /* deprecated */
	OB_SHAPE_EDIT_MODE  = 1 << 2,
};

/* ob->nlaflag */
enum {
	/* WARNING: flags (1 << 0) and (1 << 1) were from old animsys */
	/* object-channel expanded status */
	OB_ADS_COLLAPSED    = 1 << 10,
	/* object's ipo-block */
	OB_ADS_SHOWIPO      = 1 << 11,
	/* object's constraint channels */
	OB_ADS_SHOWCONS     = 1 << 12,
	/* object's material channels */
	OB_ADS_SHOWMATS     = 1 << 13,
	/* object's marticle channels */
	OB_ADS_SHOWPARTS    = 1 << 14,
};

/* ob->protectflag */
enum {
	OB_LOCK_LOCX    = 1 << 0,
	OB_LOCK_LOCY    = 1 << 1,
	OB_LOCK_LOCZ    = 1 << 2,
	OB_LOCK_LOC     = OB_LOCK_LOCX | OB_LOCK_LOCY | OB_LOCK_LOCZ,
	OB_LOCK_ROTX    = 1 << 3,
	OB_LOCK_ROTY    = 1 << 4,
	OB_LOCK_ROTZ    = 1 << 5,
	OB_LOCK_ROT     = OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ,
	OB_LOCK_SCALEX  = 1 << 6,
	OB_LOCK_SCALEY  = 1 << 7,
	OB_LOCK_SCALEZ  = 1 << 8,
	OB_LOCK_SCALE   = OB_LOCK_SCALEX | OB_LOCK_SCALEY | OB_LOCK_SCALEZ,
	OB_LOCK_ROTW    = 1 << 9,
	OB_LOCK_ROT4D   = 1 << 10,
};

/* ob->mode */
typedef enum ObjectMode {
	OB_MODE_OBJECT        = 0,
	OB_MODE_EDIT          = 1 << 0,
	OB_MODE_SCULPT        = 1 << 1,
	OB_MODE_VERTEX_PAINT  = 1 << 2,
	OB_MODE_WEIGHT_PAINT  = 1 << 3,
	OB_MODE_TEXTURE_PAINT = 1 << 4,
	OB_MODE_PARTICLE_EDIT = 1 << 5,
	OB_MODE_POSE          = 1 << 6,
} ObjectMode;

/* any mode where the brush system is used */
#define OB_MODE_ALL_PAINT (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)

#define MAX_DUPLI_RECUR 8

#ifdef __cplusplus
}
#endif

#endif

