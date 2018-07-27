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

#include "DNA_object_enums.h"

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

/* Face Maps*/
typedef struct bFaceMap {
	struct bFaceMap *next, *prev;
	char name[64];  /* MAX_VGROUP_NAME */
	char flag;
	char pad[7];
} bFaceMap;

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
	float distance, pad;
	int obhysteresis;
} LodLevel;

typedef struct ObjectDisplay {
	int flag;
} ObjectDisplay;

/* Not saved in file! */
typedef struct Object_Runtime {
	/* Original mesh pointer, before object->data was changed to point
	 * to mesh_eval.
	 * Is assigned by dependency graph's copy-on-write evaluation.
	 */
	struct Mesh *mesh_orig;
	/* Mesh structure created during object evaluation.
	 * It has all modifiers applied.
	 */
	struct Mesh *mesh_eval;
	/* Mesh structure created during object evaluation.
	 * It has deforemation only modifiers applied on it.
	 */
	struct Mesh *mesh_deform_eval;
} Object_Runtime;

typedef struct Object {
	ID id;
	struct AnimData *adt;		/* animation data (must be immediately after id for utilities to use it) */
	struct DrawDataList drawdata; /* runtime (must be immediately after id for utilities to use it). */

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
	struct BoundBox *bb;  /* axis aligned boundbox (in localspace) */
	struct bAction *action  DNA_DEPRECATED;	 // XXX deprecated... old animation system
	struct bAction *poselib;
	struct bPose *pose;  /* pose data, armature objects only */
	void *data;  /* pointer to objects data - an 'ID' or NULL */

	struct bGPdata *gpd;	/* Grease Pencil data */

	bAnimVizSettings avs;	/* settings for visualization of object-transform animation */
	bMotionPath *mpath;		/* motion path cache for this object */
	void *pad1;

	ListBase constraintChannels  DNA_DEPRECATED; // XXX deprecated... old animation system
	ListBase effect  DNA_DEPRECATED;             // XXX deprecated... keep for readfile
	ListBase defbase;   /* list of bDeformGroup (vertex groups) names and flag only */
	ListBase modifiers; /* list of ModifierData structures */
	ListBase fmaps;     /* list of facemaps */

	int mode;           /* Local object mode */
	int restore_mode;

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

	short flag;			/* copy of Base */
	short colbits DNA_DEPRECATED;		/* deprecated, use 'matbits' */

	short transflag, protectflag;	/* transformation settings and transform locks  */
	short trackflag, upflag;
	short nlaflag;				/* used for DopeSheet filtering settings (expanded/collapsed) */
	short pad[2];

	/* did last modifier stack generation need mapping support? */
	char lastNeedMapping;  /* bool */
	char duplicator_visibility_flag;

	/* dupli-frame settings */
	int dupon, dupoff, dupsta, dupend;

	/* Depsgraph */
	short base_flag; /* used by depsgraph, flushed from base */
	short pad8;

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

	float sf; /* sf is time-offset */

	short index;			/* custom index, for renderpasses */
	unsigned short actdef;	/* current deformation group, note: index starts at 1 */
	unsigned short actfmap;	/* current face map, note: index starts at 1 */
	unsigned char pad5[6];
	float col[4];			/* object color */

	char restrictflag;		/* for restricting view, select, render etc. accessible in outliner */
	char pad3;
	short softflag;			/* softbody settings */
	int pad2;

	ListBase constraints;		/* object constraints */
	ListBase nlastrips  DNA_DEPRECATED;			// XXX deprecated... old animation system
	ListBase hooks  DNA_DEPRECATED;				// XXX deprecated... old animation system
	ListBase particlesystem;	/* particle systems */

	struct PartDeflect *pd;		/* particle deflector/attractor/collision data */
	struct SoftBody *soft;		/* if exists, saved in file */
	struct Collection *dup_group;	/* object duplicator for group */
	void *pad10;

	char  pad4;
	char  shapeflag;			/* flag for pinning */
	short shapenr;				/* current shape key for menu or pinned */
	float smoothresh;			/* smoothresh is phong interpolation ray_shadow correction in render */

	struct FluidsimSettings *fluidsimSettings; /* if fluidsim enabled, store additional settings */

	struct DerivedMesh *derivedDeform, *derivedFinal;
	void *pad7;
	uint64_t lastDataMask;   /* the custom data layer mask that was last used to calculate derivedDeform and derivedFinal */
	uint64_t customdata_mask; /* (extra) custom data layer mask to use for creating derivedmesh, set by depsgraph */

	/* Runtime valuated curve-specific data, not stored in the file */
	struct CurveCache *curve_cache;

	ListBase pc_ids;

	struct RigidBodyOb *rigidbody_object;		/* settings for Bullet rigid body */
	struct RigidBodyCon *rigidbody_constraint;	/* settings for Bullet constraint */

	float ima_ofs[2];		/* offset for image empties */
	ImageUser *iuser;		/* must be non-null when oject is an empty image */

	ListBase lodlevels;		/* contains data for levels of detail */
	LodLevel *currentlod;

	struct PreviewImage *preview;

	int pad6;
	int select_color;

	/* Runtime evaluation data. */
	Object_Runtime runtime;

	/* Object Display */
	struct ObjectDisplay display;
	int pad9;
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
	OB_LIGHTPROBE = 13,

/*	OB_WAVE       = 21, */
	OB_LATTICE    = 22,

/* 23 and 24 are for life and sector (old file compat.) */
	OB_ARMATURE   = 25,
	OB_TYPE_MAX,
};

/* ObjectDisplay.flag */
enum {
	OB_SHOW_SHADOW = (1 << 0),
};

/* check if the object type supports materials */
#define OB_TYPE_SUPPORT_MATERIAL(_type) \
	((_type) >= OB_MESH && (_type) <= OB_MBALL)
#define OB_TYPE_SUPPORT_VGROUP(_type) \
	(ELEM(_type, OB_MESH, OB_LATTICE))
#define OB_TYPE_SUPPORT_EDITMODE(_type) \
	(ELEM(_type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE))
#define OB_TYPE_SUPPORT_PARVERT(_type) \
	(ELEM(_type, OB_MESH, OB_SURF, OB_CURVE, OB_LATTICE))

/** Matches #OB_TYPE_SUPPORT_EDITMODE. */
#define OB_DATA_SUPPORT_EDITMODE(_type) \
	(ELEM(_type, ID_ME, ID_CU, ID_MB, ID_LT, ID_AR))

/* is this ID type used as object data */
#define OB_DATA_SUPPORT_ID(_id_type) \
	(ELEM(_id_type, ID_ME, ID_CU, ID_MB, ID_LA, ID_SPK, ID_LP, ID_CA, ID_LT, ID_AR))

#define OB_DATA_SUPPORT_ID_CASE \
	ID_ME: case ID_CU: case ID_MB: case ID_LA: case ID_SPK: case ID_LP: case ID_CA: case ID_LT: case ID_AR

/* partype: first 4 bits: type */
enum {
	PARTYPE       = (1 << 4) - 1,
	PAROBJECT     = 0,
#ifdef DNA_DEPRECATED
	PARCURVE      = 1,  /* Deprecated. */
#endif
	PARKEY        = 2,  /* XXX Unused, deprecated? */

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
	OB_DUPLICALCDERIVED = 1 << 7, /* runtime, calculate derivedmesh for dupli before it's used */
	OB_DUPLICOLLECTION  = 1 << 8,
	OB_DUPLIFACES       = 1 << 9,
	OB_DUPLIFACES_SCALE = 1 << 10,
	OB_DUPLIPARTS       = 1 << 11,
	OB_RENDER_DUPLI     = 1 << 12,
	OB_NO_CONSTRAINTS   = 1 << 13,  /* runtime constraints disable */
	OB_NO_PSYS_UPDATE   = 1 << 14,  /* hack to work around particle issue */

	OB_DUPLI            = OB_DUPLIFRAMES | OB_DUPLIVERTS | OB_DUPLICOLLECTION | OB_DUPLIFACES | OB_DUPLIPARTS,
};

/* (short) trackflag / upflag */
enum {
	OB_POSX = 0,
	OB_POSY = 1,
	OB_POSZ = 2,
	OB_NEGX = 3,
	OB_NEGY = 4,
	OB_NEGZ = 5,
};

/* dt: no flags */
enum {
	OB_BOUNDBOX  = 1,
	OB_WIRE      = 2,
	OB_SOLID     = 3,
	OB_MATERIAL  = 4,
	OB_TEXTURE   = 5,
	OB_RENDER    = 6,
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
	OB_LOD_USE_HYST		= 1 << 2,
};


/* **************** BASE ********************* */

/* also needed for base!!!!! or rather, they interfere....*/
/* base->flag and ob->flag */
enum {
	BA_WAS_SEL = (1 << 1),
	/* NOTE: BA_HAS_RECALC_DATA can be re-used later if freed in readfile.c. */
	// BA_HAS_RECALC_OB = (1 << 2),  /* DEPRECATED */
	// BA_HAS_RECALC_DATA =  (1 << 3),  /* DEPRECATED */
	BA_SNAP_FIX_DEPS_FIASCO = (1 << 2),  /* Yes, re-use deprecated bit, all fine since it's runtime only. */
};

	/* NOTE: this was used as a proper setting in past, so nullify before using */
#define BA_TEMP_TAG         (1 << 5)

/* #define BA_FROMSET          (1 << 7) */ /*UNUSED*/

#define BA_TRANSFORM_CHILD  (1 << 8)  /* child of a transformed object */
#define BA_TRANSFORM_PARENT (1 << 13)  /* parent of a transformed object */

#define OB_FROMDUPLI        (1 << 9)
#define OB_DONE             (1 << 10)  /* unknown state, clear before use */
/* #define OB_RADIO            (1 << 11) */  /* deprecated */
/* #define OB_FROMGROUP        (1 << 12) */  /* deprecated */

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
#define OB_MAX_COL_MASKS    16

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

/* ob->duplicator_visibility_flag */
enum {
	OB_DUPLI_FLAG_VIEWPORT = 1 << 0,
	OB_DUPLI_FLAG_RENDER   = 1 << 1,
};

#define MAX_DUPLI_RECUR 8

#ifdef __cplusplus
}
#endif

#endif
