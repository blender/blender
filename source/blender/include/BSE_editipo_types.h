/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BSE_EDITIPO_TYPES_H
#define BSE_EDITIPO_TYPES_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct BezTriple;

typedef struct IpoKey {
	struct IpoKey *next, *prev;
	short flag, rt;
	float val;
	struct BezTriple **data;
} IpoKey;

typedef struct EditIpo {
	char name[12];
	IpoCurve *icu;
	short adrcode, flag;
	short disptype, rt;
	unsigned int col;
} EditIpo;


#define IPOBUTY	17

#define TOB_IPO	1
#define TOB_IPODROT	2
#define TOB_IKA	4

/* disptype */
#define IPO_DISPDEGR	1
#define IPO_DISPBITS	2
#define IPO_DISPTIME	3

/* ******************** */

#define OB_TOTIPO	24

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

#define OB_EFF_X	21
#define OB_EFF_Y	22
#define OB_EFF_Z	23

#define OB_COL_R	21
#define OB_COL_G	22
#define OB_COL_B	23
#define OB_COL_A	24



/* ******************** */

#define MA_TOTIPO	32

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
#define MA_ANG		17
#define MA_MODE		18
#define MA_HASIZE	19

#define MA_MAP1		0x20
#define MA_MAP2		0x40
#define MA_MAP3		0x80
#define MA_MAP4		0x100
#define MA_MAP5		0x200
#define MA_MAP6		0x400
#define MA_MAP7		0x800
#define MA_MAP8		0x1000

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

/* ******************** */

#define SEQ_TOTIPO	1

#define SEQ_FAC1	1

/* ******************** */

#define CU_TOTIPO	1

#define CU_SPEED	1

/* ******************** */

#define KEY_TOTIPO	32

#define KEY_SPEED	0
#define KEY_NR		1

/* ******************** */

#define WO_TOTIPO	29

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

/* ******************** */

#define LA_TOTIPO	23

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

/* ******************** */

#define CAM_TOTIPO	3

#define CAM_LENS	1
#define CAM_STA		2
#define CAM_END		3


/* ******************** */

#define SND_TOTIPO	4

#define SND_VOLUME	1
#define SND_PITCH	2
#define SND_PANNING	3
#define SND_ATTEN	4

/* ******************** */

#define AC_TOTIPO	10	/* Action Ipos */

#define AC_LOC_X	1
#define AC_LOC_Y	2
#define AC_LOC_Z	3

#define AC_SIZE_X	13
#define AC_SIZE_Y	14
#define AC_SIZE_Z	15

#define AC_QUAT_W	25
#define AC_QUAT_X	26
#define AC_QUAT_Y	27
#define AC_QUAT_Z	28

/* ******************** */

#define CO_TOTIPO	1	/* Constraint Ipos */

#define CO_ENFORCE	1
/*
#define	CO_TIME		2
#define CO_OFFSET_X	3
#define CO_OFFSET_Y	4
#define CO_OFFSET_Z	5
#define CO_ORIENT_X	6
#define CO_ORIENT_Y	7
#define CO_ORIENT_Z	8
#define CO_ROLL		9
*/

#endif /*  BSE_EDITIPO_TYPES_H */

