/**
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
struct EditMesh;

typedef struct Mesh {
	ID id;

	struct BoundBox *bb;

	ListBase effect;
	
	struct Ipo *ipo;
	struct Key *key;
	struct Material **mat;

	struct MFace *mface;	/* array of mesh object mode faces */
	struct MTFace *mtface;	/* store face UV's and texture here */
	struct TFace *tface;	/* depecrated, use mtface */
	struct MVert *mvert;	/* array of verts */
	struct MEdge *medge;	/* array of edges */
	struct MDeformVert *dvert;	/* deformgroup vertices */
	struct MCol *mcol;		/* array of colors, this must be the number of faces * 4 */
	struct MSticky *msticky;
	struct Mesh *texcomesh;
	struct MSelect *mselect;
	
	struct EditMesh *edit_mesh;	/* not saved in file! */

	struct CustomData vdata, edata, fdata;

	int totvert, totedge, totface, totselect;
	
	/* the last selected vertex/edge/face are used for the active face however
	 * this means the active face must always be selected, this is to keep track
	 * of the last selected face and is similar to the old active face flag where
	 * the face does not need to be selected, -1 is inactive */
	int act_face; 
	
	int texflag;
	
	/* texture space, copied as one block in editobject.c */
	float loc[3];
	float size[3];
	float rot[3];
	
	float cubemapsize;
	
	int drawflag;
	short smoothresh, flag;

	short subdiv, subdivr;
	short totcol;
	short subsurftype;		/* only kept for backwards compat, not used anymore */

	struct Multires *mr;		/* Multiresolution modeling data */
	struct PartialVisibility *pv;
} Mesh;

/* deprecated by MTFace, only here for file reading */
typedef struct TFace {
	void *tpage;	/* the faces image for the active UVLayer */
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

/* me->drawflag, int */ 
#define ME_DRAWEDGES	(1 << 0)
#define ME_DRAWFACES	(1 << 1)
#define ME_DRAWNORMALS	(1 << 2)
#define ME_DRAW_VNORMALS (1 << 3)

#define ME_ALLEDGES		(1 << 4)
#define ME_HIDDENEDGES  (1 << 5)

#define ME_DRAWCREASES	(1 << 6)
#define ME_DRAWSEAMS    (1 << 7)
#define ME_DRAWSHARP    (1 << 8)
#define ME_DRAWBWEIGHTS	(1 << 9)

#define ME_DRAW_EDGELEN  (1 << 10) 
#define ME_DRAW_FACEAREA (1 << 11)
#define ME_DRAW_EDGEANG  (1 << 12)

/* old global flags:
#define G_DRAWEDGES		(1 << 18)
#define G_DRAWFACES		(1 <<  7)
#define G_DRAWNORMALS	(1 <<  6)
#define G_DRAW_VNORMALS	(1 << 14)

#define G_ALLEDGES		(1 << 11)
#define G_HIDDENEDGES   (1 << 21)

#define G_DRAWCREASES	(1 << 19)
#define G_DRAWSEAMS     (1 << 20)
#define G_DRAWSHARP     (1 << 28)
#define G_DRAWBWEIGHTS	(1 << 31)

#define G_DRAW_EDGELEN  (1 << 22) 
#define G_DRAW_FACEAREA (1 << 23)
#define G_DRAW_EDGEANG  (1 << 24)
*/



/* Subsurf Type */
#define ME_CC_SUBSURF 		0
#define ME_SIMPLE_SUBSURF 	1

#define MESH_MAX_VERTS 2000000000L

#endif
