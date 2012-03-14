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
#ifndef __BKE_SUBSURF_H__
#define __BKE_SUBSURF_H__

/** \file BKE_subsurf.h
 *  \ingroup bke
 */

/* struct DerivedMesh is used directly */
#include "BKE_DerivedMesh.h"

struct DMFlagMat;
struct DMGridAdjacency;
struct DMGridData;
struct DerivedMesh;
struct IndexNode;
struct ListBase;
struct Mesh;
struct MultiresSubsurf;
struct Object;
struct PBVH;
struct SubsurfModifierData;
struct CCGEdge;
struct CCGFace;
struct CCGSubsurf;
struct CCGVert;
struct EdgeHash;
struct PBVH;
struct DMGridData;
struct DMGridAdjacency;

/**************************** External *****************************/

struct DerivedMesh *subsurf_make_derived_from_derived(
						struct DerivedMesh *dm,
						struct SubsurfModifierData *smd,
						int useRenderParams, float (*vertCos)[3],
						int isFinalCalc, int forEditMode, int inEditMode);

void subsurf_calculate_limit_positions(struct Mesh *me, float (*positions_r)[3]);

/* get gridsize from 'level', level must be greater than zero */
int ccg_gridsize(int level);

/* x/y grid coordinates at 'low_level' can be multiplied by the result
   of this function to convert to grid coordinates at 'high_level' */
int ccg_factor(int low_level, int high_level);

/**************************** Internal *****************************/

typedef struct CCGDerivedMesh {
	DerivedMesh dm;

	struct CCGSubSurf *ss;
	int freeSS;
	int drawInteriorEdges, useSubsurfUv;

	struct {int startVert; struct CCGVert *vert;} *vertMap;
	struct {int startVert; int startEdge; struct CCGEdge *edge;} *edgeMap;
	struct {int startVert; int startEdge;
			int startFace; struct CCGFace *face;} *faceMap;

	short *edgeFlags;
	struct DMFlagMat *faceFlags;

	int *reverseFaceMap;

	struct PBVH *pbvh;
	struct ListBase *fmap;
	struct IndexNode *fmap_mem;

	struct ListBase *pmap;
	struct IndexNode *pmap_mem;

	struct DMGridData **gridData;
	struct DMGridAdjacency *gridAdjacency;
	int *gridOffset;
	struct CCGFace **gridFaces;
	struct DMFlagMat *gridFlagMats;

	struct {
		struct MultiresModifierData *mmd;
		int local_mmd;

		int lvl, totlvl;
		float (*orco)[3];

		struct Object *ob;
		int modified;
	} multires;

	struct EdgeHash *ehash;
} CCGDerivedMesh;

#endif

