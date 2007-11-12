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
#ifndef DNA_MESH_TYPES_H
#define DNA_MESH_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_customdata_types.h"

struct DerivedMesh;
struct Ipo;
struct Key;
struct Material;
struct MVert;
struct MEdge;
struct MFace;
struct MCol;
struct MSticky;
struct Mesh;
struct OcInfo;
struct Multires;
struct PartialVisibility;

typedef struct Mesh {
	ID id;

	struct BoundBox *bb;

	ListBase effect;
	
	struct Ipo *ipo;
	struct Key *key;
	struct Material **mat;

	struct MFace *mface;
	struct MTFace *mtface;
	struct TFace *tface;	/* depecrated, use mtface */
	struct MVert *mvert;
	struct MEdge *medge;
	struct MDeformVert *dvert;	/* __NLA */
	struct MCol *mcol;
	struct MSticky *msticky;
	struct Mesh *texcomesh;
	struct MSelect *mselect;
	
	struct MLoop *mloop; /*new bmesh ngon stuff*/
	struct MPoly *mpoly; /*new bmesh ngon stuff*/
	
	struct OcInfo *oc;		/* not written in file */
	void *sumohandle;

	struct CustomData vdata, edata, fdata, ldata, pdata;

	int totvert, totedge, totface, totselect, totloop, totpoly;
	int texflag;
	
	/* texture space, copied as one block in editobject.c */
	float loc[3];
	float size[3];
	float rot[3];
	
	float cubemapsize, pad;

	short smoothresh, flag;

	short subdiv, subdivr;
	short totcol;
	short subsurftype; 
	int padbleh;
	
	struct Multires *mr;		/* Multiresolution modeling data */
	struct PartialVisibility *pv;
/*ifdef WITH_VERSE*/
	/* not written in file, pointer at geometry VerseNode */
	void *vnode;
/*#endif*/
	void *trias;
	int tottrias, pad4;
} Mesh;

/* deprecated by MTFace, only here for file reading */
typedef struct TFace {
	void *tpage;
	float uv[4][2];
	unsigned int col[4];
	char flag, transp;
	short mode, tile, unwrap;
} TFace;

/* **************** MESH ********************* */

/* texflag */
#define AUTOSPACE	1

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

#define MESH_MAX_VERTS 2000000000L

#endif
