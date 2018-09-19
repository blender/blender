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
#include "BLI_math_vector.h"
#include "BLI_ghash.h"

#include "DNA_meshdata_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_scene.h"



#ifdef _OPENMP
#  include "BKE_mesh.h"  /* BKE_MESH_OMP_LIMIT */
#endif

static void initData(ModifierData *md)
{
	BuildModifierData *bmd = (BuildModifierData *) md;

	bmd->start = 1.0;
	bmd->length = 100.0;
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static DerivedMesh *applyModifier(
        ModifierData *md, Object *UNUSED(ob),
        DerivedMesh *derivedData,
        ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = derivedData;
	DerivedMesh *result;
	BuildModifierData *bmd = (BuildModifierData *) md;
	int i, j, k;
	int numFaces_dst, numEdges_dst, numLoops_dst = 0;
	int *vertMap, *edgeMap, *faceMap;
	float frac;
	MPoly *mpoly_dst;
	MLoop *ml_dst, *ml_src /*, *mloop_dst */;
	GHashIterator gh_iter;
	/* maps vert indices in old mesh to indices in new mesh */
	GHash *vertHash = BLI_ghash_int_new("build ve apply gh");
	/* maps edge indices in new mesh to indices in old mesh */
	GHash *edgeHash = BLI_ghash_int_new("build ed apply gh");
	GHash *edgeHash2 = BLI_ghash_int_new("build ed apply gh");

	const int numVert_src = dm->getNumVerts(dm);
	const int numEdge_src = dm->getNumEdges(dm);
	const int numPoly_src = dm->getNumPolys(dm);
	MPoly *mpoly_src = dm->getPolyArray(dm);
	MLoop *mloop_src = dm->getLoopArray(dm);
	MEdge *medge_src = dm->getEdgeArray(dm);
	MVert *mvert_src = dm->getVertArray(dm);


	vertMap = MEM_malloc_arrayN(numVert_src, sizeof(*vertMap), "build modifier vertMap");
	edgeMap = MEM_malloc_arrayN(numEdge_src, sizeof(*edgeMap), "build modifier edgeMap");
	faceMap = MEM_malloc_arrayN(numPoly_src, sizeof(*faceMap), "build modifier faceMap");

	range_vn_i(vertMap, numVert_src, 0);
	range_vn_i(edgeMap, numEdge_src, 0);
	range_vn_i(faceMap, numPoly_src, 0);

	frac = (BKE_scene_frame_get(md->scene) - bmd->start) / bmd->length;
	CLAMP(frac, 0.0f, 1.0f);

	if (bmd->flag & MOD_BUILD_FLAG_REVERSE) {
		frac = 1.0f - frac;
	}

	numFaces_dst = numPoly_src * frac;
	numEdges_dst = numEdge_src * frac;

	/* if there's at least one face, build based on faces */
	if (numFaces_dst) {
		MPoly *mpoly, *mp;
		MLoop *ml, *mloop;
		MEdge *medge;
		uintptr_t hash_num, hash_num_alt;

		if (bmd->flag & MOD_BUILD_FLAG_RANDOMIZE) {
			BLI_array_randomize(faceMap, sizeof(*faceMap),
			                    numPoly_src, bmd->seed);
		}

		/* get the set of all vert indices that will be in the final mesh,
		 * mapped to the new indices
		 */
		mpoly = mpoly_src;
		mloop = mloop_src;
		hash_num = 0;
		for (i = 0; i < numFaces_dst; i++) {
			mp = mpoly + faceMap[i];
			ml = mloop + mp->loopstart;

			for (j = 0; j < mp->totloop; j++, ml++) {
				void **val_p;
				if (!BLI_ghash_ensure_p(vertHash, POINTER_FROM_INT(ml->v), &val_p)) {
					*val_p = (void *)hash_num;
					hash_num++;
				}
			}

			numLoops_dst += mp->totloop;
		}
		BLI_assert(hash_num == BLI_ghash_len(vertHash));

		/* get the set of edges that will be in the new mesh (i.e. all edges
		 * that have both verts in the new mesh)
		 */
		medge = medge_src;
		hash_num = 0;
		hash_num_alt = 0;
		for (i = 0; i < numEdge_src; i++, hash_num_alt++) {
			MEdge *me = medge + i;

			if (BLI_ghash_haskey(vertHash, POINTER_FROM_INT(me->v1)) &&
			    BLI_ghash_haskey(vertHash, POINTER_FROM_INT(me->v2)))
			{
				BLI_ghash_insert(edgeHash, (void *)hash_num, (void *)hash_num_alt);
				BLI_ghash_insert(edgeHash2, (void *)hash_num_alt, (void *)hash_num);
				hash_num++;
			}
		}
	}
	else if (numEdges_dst) {
		MEdge *medge, *me;
		uintptr_t hash_num;

		if (bmd->flag & MOD_BUILD_FLAG_RANDOMIZE)
			BLI_array_randomize(edgeMap, sizeof(*edgeMap),
			                    numEdge_src, bmd->seed);

		/* get the set of all vert indices that will be in the final mesh,
		 * mapped to the new indices
		 */
		medge = medge_src;
		hash_num = 0;
		BLI_assert(hash_num == BLI_ghash_len(vertHash));
		for (i = 0; i < numEdges_dst; i++) {
			void **val_p;
			me = medge + edgeMap[i];

			if (!BLI_ghash_ensure_p(vertHash, POINTER_FROM_INT(me->v1), &val_p)) {
				*val_p = (void *)hash_num;
				hash_num++;
			}
			if (!BLI_ghash_ensure_p(vertHash, POINTER_FROM_INT(me->v2), &val_p)) {
				*val_p = (void *)hash_num;
				hash_num++;
			}
		}
		BLI_assert(hash_num == BLI_ghash_len(vertHash));

		/* get the set of edges that will be in the new mesh */
		for (i = 0; i < numEdges_dst; i++) {
			j = BLI_ghash_len(edgeHash);

			BLI_ghash_insert(edgeHash, POINTER_FROM_INT(j),
			                 POINTER_FROM_INT(edgeMap[i]));
			BLI_ghash_insert(edgeHash2,  POINTER_FROM_INT(edgeMap[i]),
			                 POINTER_FROM_INT(j));
		}
	}
	else {
		int numVerts = numVert_src * frac;

		if (bmd->flag & MOD_BUILD_FLAG_RANDOMIZE) {
			BLI_array_randomize(vertMap, sizeof(*vertMap),
			                    numVert_src, bmd->seed);
		}

		/* get the set of all vert indices that will be in the final mesh,
		 * mapped to the new indices
		 */
		for (i = 0; i < numVerts; i++) {
			BLI_ghash_insert(vertHash, POINTER_FROM_INT(vertMap[i]), POINTER_FROM_INT(i));
		}
	}

	/* now we know the number of verts, edges and faces, we can create
	 * the mesh
	 */
	result = CDDM_from_template(dm, BLI_ghash_len(vertHash),
	                            BLI_ghash_len(edgeHash), 0, numLoops_dst, numFaces_dst);

	/* copy the vertices across */
	GHASH_ITER (gh_iter, vertHash) {
		MVert source;
		MVert *dest;
		int oldIndex = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
		int newIndex = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));

		source = mvert_src[oldIndex];
		dest = CDDM_get_vert(result, newIndex);

		DM_copy_vert_data(dm, result, oldIndex, newIndex, 1);
		*dest = source;
	}

	/* copy the edges across, remapping indices */
	for (i = 0; i < BLI_ghash_len(edgeHash); i++) {
		MEdge source;
		MEdge *dest;
		int oldIndex = POINTER_AS_INT(BLI_ghash_lookup(edgeHash, POINTER_FROM_INT(i)));

		source = medge_src[oldIndex];
		dest = CDDM_get_edge(result, i);

		source.v1 = POINTER_AS_INT(BLI_ghash_lookup(vertHash, POINTER_FROM_INT(source.v1)));
		source.v2 = POINTER_AS_INT(BLI_ghash_lookup(vertHash, POINTER_FROM_INT(source.v2)));

		DM_copy_edge_data(dm, result, oldIndex, i, 1);
		*dest = source;
	}

	mpoly_dst = CDDM_get_polys(result);
	/* mloop_dst = */ ml_dst = CDDM_get_loops(result);

	/* copy the faces across, remapping indices */
	k = 0;
	for (i = 0; i < numFaces_dst; i++) {
		MPoly *source;
		MPoly *dest;

		source = mpoly_src + faceMap[i];
		dest = mpoly_dst + i;
		DM_copy_poly_data(dm, result, faceMap[i], i, 1);

		*dest = *source;
		dest->loopstart = k;

		DM_copy_loop_data(dm, result, source->loopstart, dest->loopstart, dest->totloop);

		ml_src = mloop_src + source->loopstart;
		for (j = 0; j < source->totloop; j++, k++, ml_src++, ml_dst++) {
			ml_dst->v = POINTER_AS_INT(BLI_ghash_lookup(vertHash, POINTER_FROM_INT(ml_src->v)));
			ml_dst->e = POINTER_AS_INT(BLI_ghash_lookup(edgeHash2, POINTER_FROM_INT(ml_src->e)));
		}
	}

	BLI_ghash_free(vertHash, NULL, NULL);
	BLI_ghash_free(edgeHash, NULL, NULL);
	BLI_ghash_free(edgeHash2, NULL, NULL);

	MEM_freeN(vertMap);
	MEM_freeN(edgeMap);
	MEM_freeN(faceMap);

	if (dm->dirty & DM_DIRTY_NORMALS) {
		result->dirty |= DM_DIRTY_NORMALS;
	}

	return result;
}


ModifierTypeInfo modifierType_Build = {
	/* name */              "Build",
	/* structName */        "BuildModifierData",
	/* structSize */        sizeof(BuildModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_AcceptsCVs,
	/* copyData */          modifier_copyData_generic,
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
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
