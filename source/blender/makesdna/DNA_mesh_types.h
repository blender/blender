/**
 * blenlib/DNA_mesh_types.h (mar-2001 nzc)
 *
 * Mesh stuff.
 *
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
#ifndef DNA_MESH_TYPES_H
#define DNA_MESH_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

struct Ipo;
struct Key;
struct Material;
struct MVert;
struct MCol;
struct MSticky;
struct Mesh;
struct OcInfo;
struct bDeformGroup;

/* This little gem is needed to satisfy the curious realms of sdna. On
 * the other hand, it also interferes with sys/types.h, so we get rid
 * of it asap. */

typedef struct MFace {
	unsigned int v1, v2, v3, v4;
	char puno, mat_nr;
	char edcode, flag;
} MFace;

#undef ushort

typedef struct MFaceInt {
	int v1, v2, v3, v4;
	char puno, mat_nr;
	char edcode, flag;
} MFaceInt;


typedef struct TFace {

	/* this one gets interpreted as a image in texture.c  */
	void *tpage;

	float uv[4][2];		/* when you change this: also do function set_correct_uv in editmesh.c, and there are more locations that use the size of this part */
	unsigned int col[4];
	char flag, transp;
	short mode, tile, pad;
} TFace;

typedef struct MDeformWeight {	/* __NLA */
	int				def_nr;		/* __NLA */
	float			weight;		/* __NLA */
	struct Bone		*data;		/* __NLA */	/* Runtime: Does not need to be valid in file */
} MDeformWeight;				/* __NLA */

typedef struct MDeformVert {	/* __NLA */
	struct MDeformWeight *dw;	/* __NLA */
	int totweight;				/* __NLA */
	int reserved1;				/* __NLA */	
} MDeformVert;					/* __NLA */


typedef struct MVert {
	float	co[3];
	short	no[3];
	char flag, mat_nr;
} MVert;


typedef struct MCol {
	char a, r, g, b;
} MCol;


typedef struct MSticky {
	float co[2];
} MSticky;


typedef struct Mesh {
	ID id;

	struct BoundBox *bb;

	ListBase effect;
	ListBase disp;
	
	struct Ipo *ipo;
	struct Key *key;
	struct Material **mat;

	void *mface, *dface, *tface;
	struct MVert *mvert;
	struct MDeformVert *dvert;	/* __NLA */
	struct MCol *mcol;
	struct MSticky *msticky;
	struct Mesh *texcomesh;
	float *orco;
	
	struct OcInfo *oc;		/* not written in file */
	void *sumohandle;
    
	int totvert, totface;
	
	int texflag;
	float loc[3];
	float size[3];
	float rot[3];
	
	short smoothresh, flag;
	short subdiv, subdivr, subdivdone;
	short totcol;
	short subsurftype; 
	short reserved1;	/* Padding */

	float cubemapsize, rtf;

} Mesh;



/* **************** MESH ********************* */

/* texflag */
#define AUTOSPACE		1

/* me->flag */
#define ME_ISDONE		1
#define ME_NOPUNOFLIP	2
#define ME_TWOSIDED		4
#define ME_UVEFFECT		8
#define ME_VCOLEFFECT	16
#define ME_AUTOSMOOTH	32
#define ME_SMESH		64
#define ME_SUBSURF		128
#define ME_OPT_EDGES	256

/* Subsurf Type */
#define ME_CC_SUBSURF 		0
#define ME_SIMPLE_SUBSURF 	1

/* puno = vertexnormal (mface) */
#define ME_FLIPV1		1
#define ME_FLIPV2		2
#define ME_FLIPV3		4
#define ME_FLIPV4		8
#define ME_PROJXY		16
#define ME_PROJXZ		32
#define ME_PROJYZ		64

/* edcode (mface) */
#define ME_V1V2			1
#define ME_V2V3			2
#define ME_V3V1			4
#define ME_V3V4			4
#define ME_V4V1			8

/* flag (mface) */
#define ME_SMOOTH		1


#define TF_DYNAMIC		1
/* #define TF_INVISIBLE	2 */
#define TF_TEX			4
#define TF_SHAREDVERT	8
#define TF_LIGHT		16

#define TF_SHAREDCOL	64
#define TF_TILES		128
#define TF_BILLBOARD	256
#define TF_TWOSIDE		512
#define TF_INVISIBLE	1024

#define TF_OBCOL		2048
#define TF_BILLBOARD2		4096	/* with Z axis constraint */
#define TF_SHADOW		8192
#define TF_BMFONT		16384

/* tface->flag: 1=select 2=active*/
#define TF_SELECT	1
#define TF_ACTIVE	2
#define TF_SEL1		4
#define TF_SEL2		8
#define TF_SEL3		16
#define TF_SEL4		32
#define TF_HIDE		64

/* tface->transp */
#define TF_SOLID	0
#define TF_ADD		1
#define TF_ALPHA	2
#define TF_SUB		3

/* mvert->flag (1=select) */
#define ME_SPHERETEST	2
#define ME_SPHERETEMP	4
#define ME_HIDE			16

#endif

