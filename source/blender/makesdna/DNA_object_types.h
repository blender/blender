/**
 * blenlib/DNA_object_types.h (mar-2001 nzc)
 *	
 * Object is a sort of wrapper for general info.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DNA_OBJECT_TYPES_H
#define DNA_OBJECT_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_scriptlink_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bPose;	
struct Object;
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

typedef struct bDeformGroup {
	struct bDeformGroup *next, *prev;
	char name[32];
} bDeformGroup;

typedef struct BoundBox {
	float vec[8][3];
	int flag, pad;
} BoundBox;

/* boundbox flag */
#define OB_BB_DISABLED	1

typedef struct Object {
	ID id;

	short type, partype;
	int par1, par2, par3;	/* can be vertexnrs */
	char parsubstr[32];	/* String describing subobject info */
	struct Object *parent, *track;
	/* if ob->proxy (or proxy_group), this object is proxy for object ob->proxy */
	/* proxy_from is set in target back to the proxy. */
	struct Object *proxy, *proxy_group, *proxy_from;
	struct Ipo *ipo;
	struct Path *path;
	struct BoundBox *bb;
	struct bAction *action;	
	struct bAction *poselib;
	struct bPose *pose;	
	void *data;
	
	ListBase constraintChannels;
	ListBase effect;
	ListBase disp;
	ListBase defbase;
	ListBase modifiers; /* list of ModifierData structures */
	
	struct Material **mat;
	
	/* rot en drot have to be together! (transform('r' en 's')) */
	float loc[3], dloc[3], orig[3];
	float size[3], dsize[3];
	float rot[3], drot[3];
	/* float quat[4], dquat[4]; (not used yet) */
	float obmat[4][4];
	float parentinv[4][4]; /* inverse result of parent, so that object doesn't 'stick' to parent */
	float constinv[4][4]; /* inverse result of constraints. doesn't include effect of parent or object local transform */
	float imat[4][4];	/* for during render, old game engine, temporally: ipokeys of transform  */
	
	unsigned int lay;				/* copy of Base */
	
	short flag;			/* copy of Base */
	short colbits;		/* when zero, from obdata */
	
	short transflag, ipoflag;	/* transformation and ipo settings */
	short trackflag, upflag;
	short nlaflag, protectflag;	/* nlaflag defines NLA override, protectflag is bits to lock transform */
	short ipowin, scaflag;		/* ipowin: blocktype last ipowindow */
	short scavisflag, boundtype;
	
	int dupon, dupoff, dupsta, dupend;

	float sf, ctime; /* sf is time-offset, ctime is the objects current time */
	
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
	
	char dt, dtx;
	char totcol;	/* copy of mesh or curve or meta */
	char actcol;	/* currently selected material in the user interface */
	char empty_drawtype, pad1[3];
	float empty_drawsize;
	float dupfacesca;	/* dupliface scale */
	
	ScriptLink scriptlink;
	ListBase prop;
	ListBase sensors;
	ListBase controllers;
	ListBase actuators;
    
	float bbsize[3];
	short index;			/* custom index, for renderpasses */
	unsigned short actdef;	/* current deformation group */
	float col[4];			/* object color, adjusted via IPO's only */
	/**
	 * Settings for game objects
	 * bit 0: Object has dynamic behaviour
	 * bit 2: Object is evaluated by the gameengine
	 * bit 6: Use Fh settings in Materials
	 * bit 7: Use face normal to rotate Object
	 * bit 8: Friction is anisotropic
	 * bit 9: Object is a ghost
	 * bit 10: Do rigid body dynamics.
	 * bit 11: Use bounding object for physics
	 */
	int gameflag;
	/**
	 * More settings
	 * bit 15: Always ignore activity culling 
	 */
	int gameflag2;
	short softflag;			/* softboday settings */
	short recalc;			/* dependency flag */
	float anisotropicFriction[3];

	ListBase constraints;
	ListBase nlastrips;
	ListBase hooks;
	ListBase particlesystem;	/* particle systems */
	
	struct PartDeflect *pd;		/* particle deflector/attractor/collision data */
	struct SoftBody *soft;		/* if exists, saved in file */
	struct Group *dup_group;	/* object duplicator for group */
	
	short fluidsimFlag;			/* NT toggle fluidsim participation on/off */
	
	short restrictflag;			/* for restricting view, select, render etc. accessible in outliner */

	short shapenr, shapeflag;	/* current shape key for menu or pinned, flag for pinning */
	float smoothresh;			/* smoothresh is phong interpolation ray_shadow correction in render */
	short recalco, pad4;		/* recalco for temp storage of ob->recalc, bad design warning */
	
	struct FluidsimSettings *fluidsimSettings; /* if fluidsim enabled, store additional settings */

	struct DerivedMesh *derivedDeform, *derivedFinal;
	int lastDataMask;			/* the custom data layer mask that was last used to calculate derivedDeform and derivedFinal */
	int pad;

/*#ifdef WITH_VERSE*/
	void *vnode;			/* pointer at object VerseNode */
/*#endif*/
} Object;

/* Warning, this is not used anymore because hooks are now modifiers */
typedef struct ObHook {
	struct ObHook *next, *prev;
	
	struct Object *parent;
	float parentinv[4][4];	/* matrix making current transform unmodified */
	float mat[4][4];		/* temp matrix while hooking */
	float cent[3];			/* visualization of hook */
	float falloff;			/* if not zero, falloff is distance where influence zero */
	
	char name[32];

	int *indexar;
	int totindex, curindex; /* curindex is cache for fast lookup */
	short type, active;		/* active is only first hook, for button menu */
	float force;
} ObHook;


/* this work object is defined in object.c */
extern Object workob;


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

#define OB_WAVE			21
#define OB_LATTICE		22

/* 23 and 24 are for life and sector (old file compat.) */
#define	OB_ARMATURE		25

/* partype: first 4 bits: type */
#define PARTYPE			15
#define PAROBJECT		0
#define PARCURVE		1
#define PARKEY			2

#define PARSKEL			4
#define PARVERT1		5
#define PARVERT3		6
#define PARBONE			7
#define PARSLOW			16

/* (short) transflag */
#define OB_OFFS_LOCAL		1
#define OB_QUAT				2
#define OB_NEG_SCALE		4
#define OB_DUPLI			(8+16+256+512+2048)
#define OB_DUPLIFRAMES		8
#define OB_DUPLIVERTS		16
#define OB_DUPLIROT			32
#define OB_DUPLINOSPEED		64
#define OB_POWERTRACK		128
#define OB_DUPLIGROUP		256
#define OB_DUPLIFACES		512
#define OB_DUPLIFACES_SCALE	1024
#define OB_DUPLIPARTS		2048
#define OB_RENDER_DUPLI		4096

/* (short) ipoflag */
#define OB_DRAWKEY			1
#define OB_DRAWKEYSEL		2
#define OB_OFFS_OB			4
#define OB_OFFS_MAT			8
#define OB_OFFS_VKEY		16
#define OB_OFFS_PATH		32
#define OB_OFFS_PARENT		64
#define OB_OFFS_PARTICLE	128
	/* get ipo from from action or not? */
#define OB_ACTION_OB		256
#define OB_ACTION_KEY		512
	/* for stride edit */
#define OB_DISABLE_PATH		1024

#define OB_OFFS_PARENTADD	2048


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
#define OB_SHADED		4
#define OB_TEXTURE		5

/* this condition has been made more complex since editmode can draw textures */
#define CHECK_OB_DRAWTEXTURE(vd, dt) \
	((vd->drawtype==OB_TEXTURE && dt>OB_SOLID) || \
	(vd->drawtype==OB_SOLID && vd->flag2 & V3D_SOLID_TEX))

#define CHECK_OB_DRAWFACEDOT(sce, vd, dt) \
	(	(sce->selectmode & SCE_SELECT_FACE) && \
		(vd->drawtype<=OB_SOLID) && \
		(((vd->drawtype==OB_SOLID) && (dt>=OB_SOLID) && (vd->flag2 & V3D_SOLID_TEX) && (vd->flag & V3D_ZBUF_SELECT)) == 0) \
	)


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

/* boundtype */
#define OB_BOUND_BOX		0
#define OB_BOUND_SPHERE		1
#define OB_BOUND_CYLINDER	2
#define OB_BOUND_CONE		3
#define OB_BOUND_POLYH		4
#define OB_BOUND_POLYT		5
#define OB_BOUND_DYN_MESH   6


/* **************** BASE ********************* */

/* also needed for base!!!!! or rather, thy interfere....*/
/* base->flag and ob->flag */
#define BA_WAS_SEL			2
#define BA_HAS_RECALC_OB	4
#define BA_HAS_RECALC_DATA	8

#define BA_DO_IPO			32

#define BA_FROMSET			128

/* an initial attempt as making selection more specific! */
#define BA_DESELECT	0
#define BA_SELECT		1


#define OB_FROMDUPLI		512
#define OB_DONE				1024
#define OB_RADIO			2048
#define OB_FROMGROUP		4096
#define OB_POSEMODE			8192

/* ob->recalc (flag bits!) */
#define OB_RECALC_OB		1
#define OB_RECALC_DATA		2
		/* time flag is set when time changes need recalc, so baked systems can ignore it */
#define OB_RECALC_TIME		4
#define OB_RECALC			7

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

/* ob->gameflag2 */
#define OB_NEVER_DO_ACTIVITY_CULLING	1

#define OB_LIFE			(OB_PROP|OB_DYNAMIC|OB_ACTOR|OB_MAINACTOR|OB_CHILD)

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

/* ob->restrictflag */
#define OB_RESTRICT_VIEW	1
#define OB_RESTRICT_SELECT	2
#define OB_RESTRICT_RENDER	4

/* ob->shapeflag */
#define OB_SHAPE_LOCK		1
#define OB_SHAPE_TEMPLOCK	2

/* ob->nlaflag */
#define OB_NLA_OVERRIDE		1
#define OB_NLA_COLLAPSED	2

/* ob->protectflag */
#define OB_LOCK_LOCX	1
#define OB_LOCK_LOCY	2
#define OB_LOCK_LOCZ	4
#define OB_LOCK_LOC		7
#define OB_LOCK_ROTX	8
#define OB_LOCK_ROTY	16
#define OB_LOCK_ROTZ	32
#define OB_LOCK_SCALEX	64
#define OB_LOCK_SCALEY	128
#define OB_LOCK_SCALEZ	256

/* ob->softflag in DNA_object_force.h */

#ifdef __cplusplus
}
#endif

#endif


