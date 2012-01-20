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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_build.c
 *  \ingroup modifiers
 */


#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"

#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	BuildModifierData *bmd = (BuildModifierData*) md;

	bmd->start = 1.0;
	bmd->length = 100.0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	BuildModifierData *bmd = (BuildModifierData*) md;
	BuildModifierData *tbmd = (BuildModifierData*) target;

	tbmd->start = bmd->start;
	tbmd->length = bmd->length;
	tbmd->randomize = bmd->randomize;
	tbmd->seed = bmd->seed;
}

static int dependsOnTime(ModifierData *UNUSED(md))
{
	return 1;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *UNUSED(ob),
                                  DerivedMesh *derivedData,
                                  int UNUSED(useRenderParams),
                                  int UNUSED(isFinalCalc))
{
	DerivedMesh *dm = derivedData;
	DerivedMesh *result;
	BuildModifierData *bmd = (BuildModifierData*) md;
	int i;
	int numFaces_dst, numEdges_dst;
	int *vertMap, *edgeMap, *faceMap;
	float frac;
	GHashIterator *hashIter;
	/* maps vert indices in old mesh to indices in new mesh */
	GHash *vertHash = BLI_ghash_new(BLI_ghashutil_inthash,
	                                BLI_ghashutil_intcmp, "build ve apply gh");
	/* maps edge indices in new mesh to indices in old mesh */
	GHash *edgeHash = BLI_ghash_new(BLI_ghashutil_inthash,
	                                BLI_ghashutil_intcmp, "build ed apply gh");

	const int numVert_src= dm->getNumVerts(dm);
	const int numEdge_src= dm->getNumEdges(dm);
	const int numFace_src= dm->getNumFaces(dm);

	vertMap = MEM_callocN(sizeof(*vertMap) * numVert_src, "build modifier vertMap");
	for (i = 0; i < numVert_src; i++) vertMap[i] = i;
	edgeMap = MEM_callocN(sizeof(*edgeMap) * numEdge_src, "build modifier edgeMap");
	for (i = 0; i < numEdge_src; i++) edgeMap[i] = i;
	faceMap = MEM_callocN(sizeof(*faceMap) * numFace_src, "build modifier faceMap");
	for (i = 0; i < numFace_src; i++) faceMap[i] = i;

	frac = (BKE_curframe(md->scene) - bmd->start) / bmd->length;
	CLAMP(frac, 0.0f, 1.0f);

	numFaces_dst = dm->getNumFaces(dm) * frac;
	numEdges_dst = dm->getNumEdges(dm) * frac;

	/* if there's at least one face, build based on faces */
	if (numFaces_dst) {
		if (bmd->randomize) {
			BLI_array_randomize(faceMap, sizeof(*faceMap),
			                    numFace_src, bmd->seed);
		}

		/* get the set of all vert indices that will be in the final mesh,
		 * mapped to the new indices
		 */
		for (i = 0; i < numFaces_dst; i++) {
			MFace mf;
			dm->getFace(dm, faceMap[i], &mf);

			if (!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v1))) {
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(mf.v1),
				                 SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			}
			if (!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v2))) {
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(mf.v2),
				                 SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			}
			if (!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v3))) {
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(mf.v3),
				                 SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			}
			if (mf.v4 && !BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v4))) {
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(mf.v4),
				                 SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			}
		}

		/* get the set of edges that will be in the new mesh (i.e. all edges
		 * that have both verts in the new mesh)
		 */
		for (i = 0; i < numEdge_src; i++) {
			MEdge me;
			dm->getEdge(dm, i, &me);

			if ( BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v1)) &&
			     BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v2)))
			{
				BLI_ghash_insert(edgeHash, SET_INT_IN_POINTER(BLI_ghash_size(edgeHash)),
				                 SET_INT_IN_POINTER(i));
			}
		}
	}
	else if (numEdges_dst) {
		if (bmd->randomize) {
			BLI_array_randomize(edgeMap, sizeof(*edgeMap),
			                    numEdge_src, bmd->seed);
		}

		/* get the set of all vert indices that will be in the final mesh,
		 * mapped to the new indices
		 */
		for (i = 0; i < numEdges_dst; i++) {
			MEdge me;
			dm->getEdge(dm, edgeMap[i], &me);

			if (!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v1))) {
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(me.v1),
				                 SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			}
			if (!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v2))) {
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(me.v2),
				                 SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			}
		}

		/* get the set of edges that will be in the new mesh
		 */
		for (i = 0; i < numEdges_dst; i++) {
			MEdge me;
			dm->getEdge(dm, edgeMap[i], &me);

			BLI_ghash_insert(edgeHash, SET_INT_IN_POINTER(BLI_ghash_size(edgeHash)),
			                 SET_INT_IN_POINTER(edgeMap[i]));
		}
	}
	else {
		int numVerts = dm->getNumVerts(dm) * frac;

		if (bmd->randomize) {
			BLI_array_randomize(vertMap, sizeof(*vertMap),
			                    numVert_src, bmd->seed);
		}

		/* get the set of all vert indices that will be in the final mesh,
		* mapped to the new indices
		*/
		for (i = 0; i < numVerts; i++) {
			BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(vertMap[i]),
			                 SET_INT_IN_POINTER(i));
		}
	}

	/* now we know the number of verts, edges and faces, we can create
	 * the mesh
	 */
	result = CDDM_from_template(dm, BLI_ghash_size(vertHash),
	                            BLI_ghash_size(edgeHash), numFaces_dst);

	/* copy the vertices across */
	for ( hashIter = BLI_ghashIterator_new(vertHash);
	      !BLI_ghashIterator_isDone(hashIter);
	      BLI_ghashIterator_step(hashIter))
	{
		MVert source;
		MVert *dest;
		int oldIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(hashIter));
		int newIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getValue(hashIter));

		dm->getVert(dm, oldIndex, &source);
		dest = CDDM_get_vert(result, newIndex);

		DM_copy_vert_data(dm, result, oldIndex, newIndex, 1);
		*dest = source;
	}
	BLI_ghashIterator_free(hashIter);
	
	/* copy the edges across, remapping indices */
	for (i = 0; i < BLI_ghash_size(edgeHash); i++) {
		MEdge source;
		MEdge *dest;
		int oldIndex = GET_INT_FROM_POINTER(BLI_ghash_lookup(edgeHash, SET_INT_IN_POINTER(i)));
		
		dm->getEdge(dm, oldIndex, &source);
		dest = CDDM_get_edge(result, i);
		
		source.v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v1)));
		source.v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v2)));
		
		DM_copy_edge_data(dm, result, oldIndex, i, 1);
		*dest = source;
	}

	/* copy the faces across, remapping indices */
	for (i = 0; i < numFaces_dst; i++) {
		MFace source;
		MFace *dest;
		int orig_v4;
		
		dm->getFace(dm, faceMap[i], &source);
		dest = CDDM_get_face(result, i);
		
		orig_v4 = source.v4;
		
		source.v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v1)));
		source.v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v2)));
		source.v3 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v3)));
		if (source.v4)
			source.v4 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v4)));
		
		DM_copy_face_data(dm, result, faceMap[i], i, 1);
		*dest = source;
		
		test_index_face(dest, &result->faceData, i, (orig_v4 ? 4 : 3));
	}

	CDDM_calc_normals(result);

	BLI_ghash_free(vertHash, NULL, NULL);
	BLI_ghash_free(edgeHash, NULL, NULL);
	
	MEM_freeN(vertMap);
	MEM_freeN(edgeMap);
	MEM_freeN(faceMap);

	return result;
}


ModifierTypeInfo modifierType_Build = {
	/* name */              "Build",
	/* structName */        "BuildModifierData",
	/* structSize */        sizeof(BuildModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_AcceptsCVs,
	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
