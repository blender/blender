/*
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

/* Array modifier: duplicates the object multiple times along an axis */

#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_edgehash.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "depsgraph_private.h"

static void initData(ModifierData *md)
{
	ArrayModifierData *amd = (ArrayModifierData*) md;

	/* default to 2 duplicates distributed along the x-axis by an
	offset of 1 object-width
	*/
	amd->start_cap = amd->end_cap = amd->curve_ob = amd->offset_ob = NULL;
	amd->count = 2;
	amd->offset[0] = amd->offset[1] = amd->offset[2] = 0;
	amd->scale[0] = 1;
	amd->scale[1] = amd->scale[2] = 0;
	amd->length = 0;
	amd->merge_dist = 0.01;
	amd->fit_type = MOD_ARR_FIXEDCOUNT;
	amd->offset_type = MOD_ARR_OFF_RELATIVE;
	amd->flags = 0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	ArrayModifierData *amd = (ArrayModifierData*) md;
	ArrayModifierData *tamd = (ArrayModifierData*) target;

	tamd->start_cap = amd->start_cap;
	tamd->end_cap = amd->end_cap;
	tamd->curve_ob = amd->curve_ob;
	tamd->offset_ob = amd->offset_ob;
	tamd->count = amd->count;
	copy_v3_v3(tamd->offset, amd->offset);
	copy_v3_v3(tamd->scale, amd->scale);
	tamd->length = amd->length;
	tamd->merge_dist = amd->merge_dist;
	tamd->fit_type = amd->fit_type;
	tamd->offset_type = amd->offset_type;
	tamd->flags = amd->flags;
}

static void foreachObjectLink(
						ModifierData *md, Object *ob,
	 void (*walk)(void *userData, Object *ob, Object **obpoin),
		void *userData)
{
	ArrayModifierData *amd = (ArrayModifierData*) md;

	walk(userData, ob, &amd->start_cap);
	walk(userData, ob, &amd->end_cap);
	walk(userData, ob, &amd->curve_ob);
	walk(userData, ob, &amd->offset_ob);
}

static void updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *scene,
					 Object *ob, DagNode *obNode)
{
	ArrayModifierData *amd = (ArrayModifierData*) md;

	if (amd->start_cap) {
		DagNode *curNode = dag_get_node(forest, amd->start_cap);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->end_cap) {
		DagNode *curNode = dag_get_node(forest, amd->end_cap);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->curve_ob) {
		DagNode *curNode = dag_get_node(forest, amd->curve_ob);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->offset_ob) {
		DagNode *curNode = dag_get_node(forest, amd->offset_ob);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
}

static float vertarray_size(MVert *mvert, int numVerts, int axis)
{
	int i;
	float min_co, max_co;

	/* if there are no vertices, width is 0 */
	if(numVerts == 0) return 0;

	/* find the minimum and maximum coordinates on the desired axis */
	min_co = max_co = mvert->co[axis];
	++mvert;
	for(i = 1; i < numVerts; ++i, ++mvert) {
		if(mvert->co[axis] < min_co) min_co = mvert->co[axis];
		if(mvert->co[axis] > max_co) max_co = mvert->co[axis];
	}

	return max_co - min_co;
}

typedef struct IndexMapEntry {
	/* the new vert index that this old vert index maps to */
	int new;
	/* -1 if this vert isn't merged, otherwise the old vert index it
	* should be replaced with
	*/
	int merge;
	/* 1 if this vert's first copy is merged with the last copy of its
	* merge target, otherwise 0
	*/
	short merge_final;
} IndexMapEntry;

/* indexMap - an array of IndexMap entries
 * oldIndex - the old index to map
 * copyNum - the copy number to map to (original = 0, first copy = 1, etc.)
 */
static int calc_mapping(IndexMapEntry *indexMap, int oldIndex, int copyNum)
{
	if(indexMap[oldIndex].merge < 0) {
		/* vert wasn't merged, so use copy of this vert */
		return indexMap[oldIndex].new + copyNum;
	} else if(indexMap[oldIndex].merge == oldIndex) {
		/* vert was merged with itself */
		return indexMap[oldIndex].new;
	} else {
		/* vert was merged with another vert */
		/* follow the chain of merges to the end, or until we've passed
		* a number of vertices equal to the copy number
		*/
		if(copyNum <= 0)
			return indexMap[oldIndex].new;
		else
			return calc_mapping(indexMap, indexMap[oldIndex].merge,
						copyNum - 1);
	}
}

static DerivedMesh *arrayModifier_doArray(ArrayModifierData *amd,
					  struct Scene *scene, Object *ob, DerivedMesh *dm,
	   int initFlags)
{
	int i, j;
	/* offset matrix */
	float offset[4][4];
	float final_offset[4][4];
	float tmp_mat[4][4];
	float length = amd->length;
	int count = amd->count;
	int numVerts, numEdges, numFaces;
	int maxVerts, maxEdges, maxFaces;
	int finalVerts, finalEdges, finalFaces;
	DerivedMesh *result, *start_cap = NULL, *end_cap = NULL;
	MVert *mvert, *src_mvert;
	MEdge *medge;
	MFace *mface;

	IndexMapEntry *indexMap;

	EdgeHash *edges;

	/* need to avoid infinite recursion here */
	if(amd->start_cap && amd->start_cap != ob)
		start_cap = amd->start_cap->derivedFinal;
	if(amd->end_cap && amd->end_cap != ob)
		end_cap = amd->end_cap->derivedFinal;

	unit_m4(offset);

	indexMap = MEM_callocN(sizeof(*indexMap) * dm->getNumVerts(dm),
				   "indexmap");

	src_mvert = dm->getVertArray(dm);

	maxVerts = dm->getNumVerts(dm);

	if(amd->offset_type & MOD_ARR_OFF_CONST)
		add_v3_v3v3(offset[3], offset[3], amd->offset);
	if(amd->offset_type & MOD_ARR_OFF_RELATIVE) {
		for(j = 0; j < 3; j++)
			offset[3][j] += amd->scale[j] * vertarray_size(src_mvert,
					maxVerts, j);
	}

	if((amd->offset_type & MOD_ARR_OFF_OBJ) && (amd->offset_ob)) {
		float obinv[4][4];
		float result_mat[4][4];

		if(ob)
			invert_m4_m4(obinv, ob->obmat);
		else
			unit_m4(obinv);

		mul_serie_m4(result_mat, offset,
				 obinv, amd->offset_ob->obmat,
	 NULL, NULL, NULL, NULL, NULL);
		copy_m4_m4(offset, result_mat);
	}

	if(amd->fit_type == MOD_ARR_FITCURVE && amd->curve_ob) {
		Curve *cu = amd->curve_ob->data;
		if(cu) {
			float tmp_mat[3][3];
			float scale;
			
			object_to_mat3(amd->curve_ob, tmp_mat);
			scale = mat3_to_scale(tmp_mat);
				
			if(!cu->path) {
				cu->flag |= CU_PATH; // needed for path & bevlist
				makeDispListCurveTypes(scene, amd->curve_ob, 0);
			}
			if(cu->path)
				length = scale*cu->path->totdist;
		}
	}

	/* calculate the maximum number of copies which will fit within the
	prescribed length */
	if(amd->fit_type == MOD_ARR_FITLENGTH
		  || amd->fit_type == MOD_ARR_FITCURVE) {
		float dist = sqrt(dot_v3v3(offset[3], offset[3]));

		if(dist > 1e-6f)
			/* this gives length = first copy start to last copy end
			add a tiny offset for floating point rounding errors */
			count = (length + 1e-6f) / dist;
		else
			/* if the offset has no translation, just make one copy */
			count = 1;
		  }

		  if(count < 1)
			  count = 1;

	/* allocate memory for count duplicates (including original) plus
		  * start and end caps
	*/
		  finalVerts = dm->getNumVerts(dm) * count;
		  finalEdges = dm->getNumEdges(dm) * count;
		  finalFaces = dm->getNumFaces(dm) * count;
		  if(start_cap) {
			  finalVerts += start_cap->getNumVerts(start_cap);
			  finalEdges += start_cap->getNumEdges(start_cap);
			  finalFaces += start_cap->getNumFaces(start_cap);
		  }
		  if(end_cap) {
			  finalVerts += end_cap->getNumVerts(end_cap);
			  finalEdges += end_cap->getNumEdges(end_cap);
			  finalFaces += end_cap->getNumFaces(end_cap);
		  }
		  result = CDDM_from_template(dm, finalVerts, finalEdges, finalFaces);

		  /* calculate the offset matrix of the final copy (for merging) */ 
		  unit_m4(final_offset);

		  for(j=0; j < count - 1; j++) {
			  mul_m4_m4m4(tmp_mat, final_offset, offset);
			  copy_m4_m4(final_offset, tmp_mat);
		  }

		  numVerts = numEdges = numFaces = 0;
		  mvert = CDDM_get_verts(result);

		  for (i = 0; i < maxVerts; i++) {
			  indexMap[i].merge = -1; /* default to no merge */
			  indexMap[i].merge_final = 0; /* default to no merge */
		  }

		  for (i = 0; i < maxVerts; i++) {
			  MVert *inMV;
			  MVert *mv = &mvert[numVerts];
			  MVert *mv2;
			  float co[3];

			  inMV = &src_mvert[i];

			  DM_copy_vert_data(dm, result, i, numVerts, 1);
			  *mv = *inMV;
			  numVerts++;

			  indexMap[i].new = numVerts - 1;

			  copy_v3_v3(co, mv->co);
		
		/* Attempts to merge verts from one duplicate with verts from the
			  * next duplicate which are closer than amd->merge_dist.
			  * Only the first such vert pair is merged.
			  * If verts are merged in the first duplicate pair, they are merged
			  * in all pairs.
		*/
			  if((count > 1) && (amd->flags & MOD_ARR_MERGE)) {
				  float tmp_co[3];
				  mul_v3_m4v3(tmp_co, offset, mv->co);

				  for(j = 0; j < maxVerts; j++) {
					  /* if vertex already merged, don't use it */
					  if( indexMap[j].merge != -1 ) continue;

					  inMV = &src_mvert[j];
					  /* if this vert is within merge limit, merge */
					  if(compare_len_v3v3(tmp_co, inMV->co, amd->merge_dist)) {
						  indexMap[i].merge = j;

						  /* test for merging with final copy of merge target */
						  if(amd->flags & MOD_ARR_MERGEFINAL) {
							  copy_v3_v3(tmp_co, inMV->co);
							  inMV = &src_mvert[i];
							  mul_m4_v3(final_offset, tmp_co);
							  if(compare_len_v3v3(tmp_co, inMV->co, amd->merge_dist))
								  indexMap[i].merge_final = 1;
						  }
						  break;
					  }
				  }
			  }

			  /* if no merging, generate copies of this vert */
			  if(indexMap[i].merge < 0) {
				  for(j=0; j < count - 1; j++) {
					  mv2 = &mvert[numVerts];

					  DM_copy_vert_data(result, result, numVerts - 1, numVerts, 1);
					  *mv2 = *mv;
					  numVerts++;

					  mul_m4_v3(offset, co);
					  copy_v3_v3(mv2->co, co);
				  }
			  } else if(indexMap[i].merge != i && indexMap[i].merge_final) {
			/* if this vert is not merging with itself, and it is merging
				  * with the final copy of its merge target, remove the first copy
			*/
				  numVerts--;
				  DM_free_vert_data(result, numVerts, 1);
			  }
		  }

		  /* make a hashtable so we can avoid duplicate edges from merging */
		  edges = BLI_edgehash_new();

		  maxEdges = dm->getNumEdges(dm);
		  medge = CDDM_get_edges(result);
		  for(i = 0; i < maxEdges; i++) {
			  MEdge inMED;
			  MEdge med;
			  MEdge *med2;
			  int vert1, vert2;

			  dm->getEdge(dm, i, &inMED);

			  med = inMED;
			  med.v1 = indexMap[inMED.v1].new;
			  med.v2 = indexMap[inMED.v2].new;

		/* if vertices are to be merged with the final copies of their
			  * merge targets, calculate that final copy
		*/
			  if(indexMap[inMED.v1].merge_final) {
				  med.v1 = calc_mapping(indexMap, indexMap[inMED.v1].merge,
						  count - 1);
			  }
			  if(indexMap[inMED.v2].merge_final) {
				  med.v2 = calc_mapping(indexMap, indexMap[inMED.v2].merge,
						  count - 1);
			  }

			  if(med.v1 == med.v2) continue;

			  if (initFlags) {
				  med.flag |= ME_EDGEDRAW | ME_EDGERENDER;
			  }

			  if(!BLI_edgehash_haskey(edges, med.v1, med.v2)) {
				  DM_copy_edge_data(dm, result, i, numEdges, 1);
				  medge[numEdges] = med;
				  numEdges++;

				  BLI_edgehash_insert(edges, med.v1, med.v2, NULL);
			  }

			  for(j = 1; j < count; j++)
			  {
				  vert1 = calc_mapping(indexMap, inMED.v1, j);
				  vert2 = calc_mapping(indexMap, inMED.v2, j);
				  /* avoid duplicate edges */
				  if(!BLI_edgehash_haskey(edges, vert1, vert2)) {
					  med2 = &medge[numEdges];

					  DM_copy_edge_data(dm, result, i, numEdges, 1);
					  *med2 = med;
					  numEdges++;

					  med2->v1 = vert1;
					  med2->v2 = vert2;

					  BLI_edgehash_insert(edges, med2->v1, med2->v2, NULL);
				  }
			  }
		  }

		  maxFaces = dm->getNumFaces(dm);
		  mface = CDDM_get_faces(result);
		  for (i=0; i < maxFaces; i++) {
			  MFace inMF;
			  MFace *mf = &mface[numFaces];

			  dm->getFace(dm, i, &inMF);

			  DM_copy_face_data(dm, result, i, numFaces, 1);
			  *mf = inMF;

			  mf->v1 = indexMap[inMF.v1].new;
			  mf->v2 = indexMap[inMF.v2].new;
			  mf->v3 = indexMap[inMF.v3].new;
			  if(inMF.v4)
				  mf->v4 = indexMap[inMF.v4].new;

		/* if vertices are to be merged with the final copies of their
			  * merge targets, calculate that final copy
		*/
			  if(indexMap[inMF.v1].merge_final)
				  mf->v1 = calc_mapping(indexMap, indexMap[inMF.v1].merge, count-1);
			  if(indexMap[inMF.v2].merge_final)
				  mf->v2 = calc_mapping(indexMap, indexMap[inMF.v2].merge, count-1);
			  if(indexMap[inMF.v3].merge_final)
				  mf->v3 = calc_mapping(indexMap, indexMap[inMF.v3].merge, count-1);
			  if(inMF.v4 && indexMap[inMF.v4].merge_final)
				  mf->v4 = calc_mapping(indexMap, indexMap[inMF.v4].merge, count-1);

			  if(test_index_face(mf, &result->faceData, numFaces, inMF.v4?4:3) < 3)
				  continue;

			  numFaces++;

			  /* if the face has fewer than 3 vertices, don't create it */
			  if(mf->v3 == 0 || (mf->v1 && (mf->v1 == mf->v3 || mf->v1 == mf->v4))) {
				  numFaces--;
				  DM_free_face_data(result, numFaces, 1);
			  }

			  for(j = 1; j < count; j++)
			  {
				  MFace *mf2 = &mface[numFaces];

				  DM_copy_face_data(dm, result, i, numFaces, 1);
				  *mf2 = *mf;

				  mf2->v1 = calc_mapping(indexMap, inMF.v1, j);
				  mf2->v2 = calc_mapping(indexMap, inMF.v2, j);
				  mf2->v3 = calc_mapping(indexMap, inMF.v3, j);
				  if (inMF.v4)
					  mf2->v4 = calc_mapping(indexMap, inMF.v4, j);

				  test_index_face(mf2, &result->faceData, numFaces, inMF.v4?4:3);
				  numFaces++;

				  /* if the face has fewer than 3 vertices, don't create it */
				  if(mf2->v3 == 0 || (mf2->v1 && (mf2->v1 == mf2->v3 || mf2->v1 ==
								 mf2->v4))) {
					  numFaces--;
					  DM_free_face_data(result, numFaces, 1);
								 }
			  }
		  }

		  /* add start and end caps */
		  if(start_cap) {
			  float startoffset[4][4];
			  MVert *cap_mvert;
			  MEdge *cap_medge;
			  MFace *cap_mface;
			  int *origindex;
			  int *vert_map;
			  int capVerts, capEdges, capFaces;

			  capVerts = start_cap->getNumVerts(start_cap);
			  capEdges = start_cap->getNumEdges(start_cap);
			  capFaces = start_cap->getNumFaces(start_cap);
			  cap_mvert = start_cap->getVertArray(start_cap);
			  cap_medge = start_cap->getEdgeArray(start_cap);
			  cap_mface = start_cap->getFaceArray(start_cap);

			  invert_m4_m4(startoffset, offset);

			  vert_map = MEM_callocN(sizeof(*vert_map) * capVerts,
					  "arrayModifier_doArray vert_map");

			  origindex = result->getVertDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capVerts; i++) {
				  MVert *mv = &cap_mvert[i];
				  short merged = 0;

				  if(amd->flags & MOD_ARR_MERGE) {
					  float tmp_co[3];
					  MVert *in_mv;
					  int j;

					  copy_v3_v3(tmp_co, mv->co);
					  mul_m4_v3(startoffset, tmp_co);

					  for(j = 0; j < maxVerts; j++) {
						  in_mv = &src_mvert[j];
						  /* if this vert is within merge limit, merge */
						  if(compare_len_v3v3(tmp_co, in_mv->co, amd->merge_dist)) {
							  vert_map[i] = calc_mapping(indexMap, j, 0);
							  merged = 1;
							  break;
						  }
					  }
				  }

				  if(!merged) {
					  DM_copy_vert_data(start_cap, result, i, numVerts, 1);
					  mvert[numVerts] = *mv;
					  mul_m4_v3(startoffset, mvert[numVerts].co);
					  origindex[numVerts] = ORIGINDEX_NONE;

					  vert_map[i] = numVerts;

					  numVerts++;
				  }
			  }
			  origindex = result->getEdgeDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capEdges; i++) {
				  int v1, v2;

				  v1 = vert_map[cap_medge[i].v1];
				  v2 = vert_map[cap_medge[i].v2];

				  if(!BLI_edgehash_haskey(edges, v1, v2)) {
					  DM_copy_edge_data(start_cap, result, i, numEdges, 1);
					  medge[numEdges] = cap_medge[i];
					  medge[numEdges].v1 = v1;
					  medge[numEdges].v2 = v2;
					  origindex[numEdges] = ORIGINDEX_NONE;

					  numEdges++;
				  }
			  }
			  origindex = result->getFaceDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capFaces; i++) {
				  DM_copy_face_data(start_cap, result, i, numFaces, 1);
				  mface[numFaces] = cap_mface[i];
				  mface[numFaces].v1 = vert_map[mface[numFaces].v1];
				  mface[numFaces].v2 = vert_map[mface[numFaces].v2];
				  mface[numFaces].v3 = vert_map[mface[numFaces].v3];
				  if(mface[numFaces].v4) {
					  mface[numFaces].v4 = vert_map[mface[numFaces].v4];

					  test_index_face(&mface[numFaces], &result->faceData,
									  numFaces, 4);
				  }
				  else
				  {
					  test_index_face(&mface[numFaces], &result->faceData,
									  numFaces, 3);
				  }

				  origindex[numFaces] = ORIGINDEX_NONE;

				  numFaces++;
			  }

			  MEM_freeN(vert_map);
			  start_cap->release(start_cap);
		  }

		  if(end_cap) {
			  float endoffset[4][4];
			  MVert *cap_mvert;
			  MEdge *cap_medge;
			  MFace *cap_mface;
			  int *origindex;
			  int *vert_map;
			  int capVerts, capEdges, capFaces;

			  capVerts = end_cap->getNumVerts(end_cap);
			  capEdges = end_cap->getNumEdges(end_cap);
			  capFaces = end_cap->getNumFaces(end_cap);
			  cap_mvert = end_cap->getVertArray(end_cap);
			  cap_medge = end_cap->getEdgeArray(end_cap);
			  cap_mface = end_cap->getFaceArray(end_cap);

			  mul_m4_m4m4(endoffset, final_offset, offset);

			  vert_map = MEM_callocN(sizeof(*vert_map) * capVerts,
					  "arrayModifier_doArray vert_map");

			  origindex = result->getVertDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capVerts; i++) {
				  MVert *mv = &cap_mvert[i];
				  short merged = 0;

				  if(amd->flags & MOD_ARR_MERGE) {
					  float tmp_co[3];
					  MVert *in_mv;
					  int j;

					  copy_v3_v3(tmp_co, mv->co);
					  mul_m4_v3(offset, tmp_co);

					  for(j = 0; j < maxVerts; j++) {
						  in_mv = &src_mvert[j];
						  /* if this vert is within merge limit, merge */
						  if(compare_len_v3v3(tmp_co, in_mv->co, amd->merge_dist)) {
							  vert_map[i] = calc_mapping(indexMap, j, count - 1);
							  merged = 1;
							  break;
						  }
					  }
				  }

				  if(!merged) {
					  DM_copy_vert_data(end_cap, result, i, numVerts, 1);
					  mvert[numVerts] = *mv;
					  mul_m4_v3(endoffset, mvert[numVerts].co);
					  origindex[numVerts] = ORIGINDEX_NONE;

					  vert_map[i] = numVerts;

					  numVerts++;
				  }
			  }
			  origindex = result->getEdgeDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capEdges; i++) {
				  int v1, v2;

				  v1 = vert_map[cap_medge[i].v1];
				  v2 = vert_map[cap_medge[i].v2];

				  if(!BLI_edgehash_haskey(edges, v1, v2)) {
					  DM_copy_edge_data(end_cap, result, i, numEdges, 1);
					  medge[numEdges] = cap_medge[i];
					  medge[numEdges].v1 = v1;
					  medge[numEdges].v2 = v2;
					  origindex[numEdges] = ORIGINDEX_NONE;

					  numEdges++;
				  }
			  }
			  origindex = result->getFaceDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capFaces; i++) {
				  DM_copy_face_data(end_cap, result, i, numFaces, 1);
				  mface[numFaces] = cap_mface[i];
				  mface[numFaces].v1 = vert_map[mface[numFaces].v1];
				  mface[numFaces].v2 = vert_map[mface[numFaces].v2];
				  mface[numFaces].v3 = vert_map[mface[numFaces].v3];
				  if(mface[numFaces].v4) {
					  mface[numFaces].v4 = vert_map[mface[numFaces].v4];

					  test_index_face(&mface[numFaces], &result->faceData,
									  numFaces, 4);
				  }
				  else
				  {
					  test_index_face(&mface[numFaces], &result->faceData,
									  numFaces, 3);
				  }
				  origindex[numFaces] = ORIGINDEX_NONE;

				  numFaces++;
			  }

			  MEM_freeN(vert_map);
			  end_cap->release(end_cap);
		  }

		  BLI_edgehash_free(edges, NULL);
		  MEM_freeN(indexMap);

		  CDDM_lower_num_verts(result, numVerts);
		  CDDM_lower_num_edges(result, numEdges);
		  CDDM_lower_num_faces(result, numFaces);

		  return result;
}

static DerivedMesh *applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	ArrayModifierData *amd = (ArrayModifierData*) md;

	result = arrayModifier_doArray(amd, md->scene, ob, derivedData, 0);

	if(result != derivedData)
		CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *applyModifierEM(
		ModifierData *md, Object *ob, struct EditMesh *editData,
  DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_Array = {
	/* name */              "Array",
	/* structName */        "ArrayModifierData",
	/* structSize */        sizeof(ArrayModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode
							| eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
