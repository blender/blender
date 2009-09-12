/**
 * blenlib/DNA_ipo_types.h (mar-2001 nzc)
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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
/* ============================================== 
 * ATTENTION: 
 *
 * The contents of this file are now officially depreceated. They were used for the 'old' animation system,
 * which has (as of 2.50) been replaced with a completely new system by Joshua Leung (aligorith). All defines, 
 * etc. are only still maintained to provide backwards compatability for old files...
 *
 * =============================================
 */ 
 
#ifndef DNA_IPO_TYPES_H
#define DNA_IPO_TYPES_H

#include "DNA_listBase.h"
#include "DNA_curve_types.h"
#include "DNA_vec_types.h"

#include "DNA_ID.h"

/* -------------------------- Type Defines --------------------------- */

/* sometimes used - mainly for GE/Ketsji */
typedef short IPO_Channel;  


/* --- IPO Curve Driver --- */

/* IPO Curve Driver */
typedef struct IpoDriver {
	struct Object *ob;			/* target/driver ob */
	short blocktype, adrcode;	/* sub-channel to use */
	
	short type, flag;			/* driver settings */
	char name[128];	 			/* bone, or python expression here */
} IpoDriver;

/* --- IPO Curve --- */

/* IPO Curve */
typedef struct IpoCurve {
	struct IpoCurve *next,  *prev;
	
	struct BPoint *bp;					/* array of BPoints (sizeof(BPoint)*totvert) - i.e. baked/imported data */
	struct BezTriple *bezt;				/* array of BezTriples (sizeof(BezTriple)*totvert)  - i.e. user-editable keyframes  */

	rctf maxrct, totrct;				/* bounding boxes */

	short blocktype, adrcode, vartype;	/* blocktype= ipo-blocktype; adrcode= type of ipo-curve; vartype= 'format' of data */
	short totvert;						/* total number of BezTriples (i.e. keyframes) on curve */
	short ipo, extrap;					/* interpolation and extrapolation modes  */
	short flag, rt;						/* flag= settings; rt= ??? */
	float ymin, ymax;					/* minimum/maximum y-extents for curve */
	unsigned int bitmask;				/* ??? */
	
	float slide_min, slide_max;			/* minimum/maximum values for sliders (in action editor) */
	float curval;						/* value of ipo-curve for current frame */
	
	IpoDriver *driver;					/* pointer to ipo-driver for this curve */
} IpoCurve;

/* --- ID-Datablock --- */

/* IPO Data-Block */
typedef struct Ipo {
	ID id;
	
	ListBase curve;				/* A list of IpoCurve structs in a linked list. */
	rctf cur;					/* Rect defining extents of keyframes? */
	
	short blocktype, showkey;	/* blocktype: self-explanatory; showkey: either 0 or 1 (show vertical yellow lines for editing) */
	short muteipo, pad;			/* muteipo: either 0 or 1 (whether ipo block is muted) */	
} Ipo;

/* ----------- adrcodes (for matching ipo-curves to data) ------------- */

/* defines: are these duped or new? */
#define IPOBUTY	17

#define TOB_IPO	1
#define TOB_IPODROT	2

/* disptype */
#define IPO_DISPDEGR	1
#define IPO_DISPBITS	2
#define IPO_DISPTIME	3

/* ********** Object (ID_OB) ********** */

#define OB_TOTIPO	30
#define OB_TOTNAM	30

#define OB_LOC_X	1
#define OB_LOC_Y	2
#define OB_LOC_Z	3
#define OB_DLOC_X	4
#define OB_DLOC_Y	5
#define OB_DLOC_Z	6

#define OB_ROT_X	7
#define OB_ROT_Y	8
#define OB_ROT_Z	9
#define OB_DROT_X	10
#define OB_DROT_Y	11
#define OB_DROT_Z	12

#define OB_SIZE_X	13
#define OB_SIZE_Y	14
#define OB_SIZE_Z	15
#define OB_DSIZE_X	16
#define OB_DSIZE_Y	17
#define OB_DSIZE_Z	18

#define OB_LAY		19

#define OB_TIME		20

#define OB_COL_R	21
#define OB_COL_G	22
#define OB_COL_B	23
#define OB_COL_A	24

#define OB_PD_FSTR	25
#define OB_PD_FFALL	26
#define OB_PD_SDAMP	27
#define OB_PD_RDAMP	28
#define OB_PD_PERM	29
#define OB_PD_FMAXD	30

/* exception: driver channel, for bone driver only */
#define OB_ROT_DIFF	100


/* ********** Material (ID_MA) ********** */

#define MA_TOTIPO	40
#define MA_TOTNAM	26

#define MA_COL_R	1
#define MA_COL_G	2
#define MA_COL_B	3
#define MA_SPEC_R	4
#define MA_SPEC_G	5
#define MA_SPEC_B	6
#define MA_MIR_R	7
#define MA_MIR_G	8
#define MA_MIR_B	9
#define MA_REF		10
#define MA_ALPHA	11
#define MA_EMIT		12
#define MA_AMB		13
#define MA_SPEC		14
#define MA_HARD		15
#define MA_SPTR		16
#define MA_IOR		17
#define MA_MODE		18
#define MA_HASIZE	19
#define MA_TRANSLU	20
#define MA_RAYM		21
#define MA_FRESMIR	22
#define MA_FRESMIRI	23
#define MA_FRESTRA	24
#define MA_FRESTRAI	25
#define MA_ADD		26

#define MA_MAP1		(1<<5)
#define MA_MAP2		(1<<6)
#define MA_MAP3		(1<<7)
#define MA_MAP4		(1<<8)
#define MA_MAP5		(1<<9)
#define MA_MAP6		(1<<10)
#define MA_MAP7		(1<<11)
#define MA_MAP8		(1<<12)
#define MA_MAP9		(1<<13)
#define MA_MAP10	(1<<14)
#define MA_MAP11	(1<<15)
#define MA_MAP12	(1<<16)
#define MA_MAP13	(1<<17)
#define MA_MAP14	(1<<18)
#define MA_MAP15	(1<<19)
#define MA_MAP16	(1<<20)
#define MA_MAP17	(1<<21)
#define MA_MAP18	(1<<22)

/* ********** Texture Slots (MTex) ********** */

#define TEX_TOTNAM	14

#define MAP_OFS_X	1
#define MAP_OFS_Y	2
#define MAP_OFS_Z	3
#define MAP_SIZE_X	4
#define MAP_SIZE_Y	5
#define MAP_SIZE_Z	6
#define MAP_R		7
#define MAP_G		8
#define MAP_B		9

#define MAP_DVAR	10
#define MAP_COLF	11
#define MAP_NORF	12
#define MAP_VARF	13
#define MAP_DISP	14

/* ********** Texture (ID_TE) ********** */

#define TE_TOTIPO	26
#define TE_TOTNAM	26

#define TE_NSIZE	1
#define TE_NDEPTH	2
#define TE_NTYPE	3
#define TE_TURB		4

#define TE_VNW1		5
#define TE_VNW2		6
#define TE_VNW3		7
#define TE_VNW4		8
#define TE_VNMEXP	9
#define TE_VN_DISTM	10
#define TE_VN_COLT	11

#define TE_ISCA		12
#define TE_DISTA	13

#define TE_MG_TYP	14
#define TE_MGH		15
#define TE_MG_LAC	16
#define TE_MG_OCT	17
#define TE_MG_OFF	18
#define TE_MG_GAIN	19

#define TE_N_BAS1	20
#define TE_N_BAS2	21

#define TE_COL_R    22
#define TE_COL_G    23
#define TE_COL_B    24
#define TE_BRIGHT	25
#define TE_CONTRA	26

/* ******** Sequence (ID_SEQ) ********** */

#define SEQ_TOTIPO	1
#define SEQ_TOTNAM	1

#define SEQ_FAC1	1

/* ********* Curve (ID_CU) *********** */

#define CU_TOTIPO	1
#define CU_TOTNAM	1

#define CU_SPEED	1

/* ********* ShapeKey (ID_KE) *********** */

#define KEY_TOTIPO	64
#define KEY_TOTNAM	64

#define KEY_SPEED	0
#define KEY_NR		1

/* ********* World (ID_WO) *********** */

#define WO_TOTIPO	29
#define WO_TOTNAM	16

#define WO_HOR_R	1
#define WO_HOR_G	2
#define WO_HOR_B	3
#define WO_ZEN_R	4
#define WO_ZEN_G	5
#define WO_ZEN_B	6

#define WO_EXPOS	7

#define WO_MISI		8
#define WO_MISTDI	9
#define WO_MISTSTA	10
#define WO_MISTHI	11

#define WO_STAR_R	12
#define WO_STAR_G	13
#define WO_STAR_B	14
#define WO_STARDIST	15
#define WO_STARSIZE	16

/* ********** Lamp (ID_LA) ********** */

#define LA_TOTIPO	21
#define LA_TOTNAM	10

#define LA_ENERGY	1
#define LA_COL_R	2
#define LA_COL_G	3
#define LA_COL_B	4
#define LA_DIST		5
#define LA_SPOTSI	6
#define LA_SPOTBL	7
#define LA_QUAD1	8
#define LA_QUAD2	9
#define LA_HALOINT	10

/* ********* Camera (ID_CA) ************ */

#define CAM_TOTIPO	7
#define CAM_TOTNAM	7

#define CAM_LENS	1
#define CAM_STA		2
#define CAM_END		3

/* yafray aperture & focal distance curves */
#define CAM_YF_APERT	4
#define CAM_YF_FDIST	5

#define CAM_SHIFT_X		6
#define CAM_SHIFT_Y		7

/* ********* Sound (ID_SO) *********** */

#define SND_TOTIPO	4
#define SND_TOTNAM	4

#define SND_VOLUME	1
#define SND_PITCH	2
#define SND_PANNING	3
#define SND_ATTEN	4

/* ******* PoseChannel (ID_PO) ********* */

#define AC_TOTIPO	13
#define AC_TOTNAM	13

#define AC_LOC_X	1
#define AC_LOC_Y	2
#define AC_LOC_Z	3

#define AC_SIZE_X	13
#define AC_SIZE_Y	14
#define AC_SIZE_Z	15

#define AC_EUL_X	16
#define AC_EUL_Y	17
#define AC_EUL_Z	18

#define AC_QUAT_W	25
#define AC_QUAT_X	26
#define AC_QUAT_Y	27
#define AC_QUAT_Z	28

/* ******** Constraint (ID_CO) ********** */

#define CO_TOTIPO	2
#define CO_TOTNAM	2

#define CO_ENFORCE	1
#define CO_HEADTAIL	2

/* ****** FluidSim (ID_FLUIDSIM) ****** */

#define FLUIDSIM_TOTIPO	13
#define FLUIDSIM_TOTNAM	13

#define FLUIDSIM_VISC   1
#define FLUIDSIM_TIME   2

#define FLUIDSIM_GRAV_X 3
#define FLUIDSIM_GRAV_Y 4
#define FLUIDSIM_GRAV_Z 5

#define FLUIDSIM_VEL_X  6
#define FLUIDSIM_VEL_Y  7
#define FLUIDSIM_VEL_Z  8

#define FLUIDSIM_ACTIVE 9

#define FLUIDSIM_ATTR_FORCE_STR 	10
#define FLUIDSIM_ATTR_FORCE_RADIUS 	11
#define FLUIDSIM_VEL_FORCE_STR 		12
#define FLUIDSIM_VEL_FORCE_RADIUS 	13

/* ******************** */
/* particle ipos */

/* ******* Particle (ID_PA) ******** */
#define PART_TOTIPO		25
#define PART_TOTNAM		25

#define PART_EMIT_FREQ	1
#define PART_EMIT_LIFE	2
#define PART_EMIT_VEL	3
#define PART_EMIT_AVE	4
#define PART_EMIT_SIZE	5

#define PART_AVE		6
#define PART_SIZE		7
#define PART_DRAG		8
#define PART_BROWN		9
#define PART_DAMP		10
#define PART_LENGTH		11
#define PART_CLUMP		12

#define PART_GRAV_X		13
#define PART_GRAV_Y		14
#define PART_GRAV_Z		15

#define PART_KINK_AMP	16
#define PART_KINK_FREQ	17
#define PART_KINK_SHAPE	18

#define PART_BB_TILT	19

#define PART_PD_FSTR	20
#define PART_PD_FFALL	21
#define PART_PD_FMAXD	22

#define PART_PD2_FSTR	23
#define PART_PD2_FFALL	24
#define PART_PD2_FMAXD	25


/* -------------------- Defines: Flags and Types ------------------ */

/* ----- IPO Curve Defines ------- */

/* icu->vartype */
#define IPO_CHAR		0
#define IPO_SHORT		1
#define IPO_INT			2
#define IPO_LONG		3
#define IPO_FLOAT		4
#define IPO_DOUBLE		5
#define IPO_FLOAT_DEGR	6

	/* very special case, in keys */
#define IPO_BEZTRIPLE	100
#define IPO_BPOINT		101

/* icu->vartype */
#define IPO_BITS		16
#define IPO_CHAR_BIT	16
#define IPO_SHORT_BIT	17
#define IPO_INT_BIT		18

/* icu->ipo:  the type of curve */
#define IPO_CONST		0
#define IPO_LIN			1
#define IPO_BEZ			2
	/* not used yet */
#define IPO_MIXED		3 

/* icu->extrap */
#define IPO_HORIZ		0
#define IPO_DIR			1
#define IPO_CYCL		2
#define IPO_CYCLX		3

/* icu->flag */
#define IPO_VISIBLE		1
#define IPO_SELECT		2
#define IPO_EDIT		4
#define IPO_LOCK		8
#define IPO_AUTO_HORIZ	16
#define IPO_ACTIVE		32
#define IPO_PROTECT		64
#define IPO_MUTE		128

/* ---------- IPO Drivers ----------- */

/* offset in driver->name for finding second posechannel for rot-diff  */
#define DRIVER_NAME_OFFS	32 

/* driver->type */
#define	IPO_DRIVER_TYPE_NORMAL 		0
#define	IPO_DRIVER_TYPE_PYTHON 		1

/* driver->flag */
	/* invalid flag: currently only used for buggy pydriver expressions */
#define IPO_DRIVER_FLAG_INVALID 	(1<<0)

#endif



