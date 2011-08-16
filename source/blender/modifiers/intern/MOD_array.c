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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_edgehash.h"

#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_tessmesh.h"

#include "depsgraph_private.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

static void updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *UNUSED(scene),
					 Object *UNUSED(ob), DagNode *obNode)
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

/* finds the best possible flipped name. For renaming; check for unique names afterwards */
/* if strip_number: removes number extensions */
void vertgroup_flip_name (char *name, int strip_number)
{
	int     len;
	char    prefix[128]={""};   /* The part before the facing */
	char    suffix[128]={""};   /* The part after the facing */
	char    replace[128]={""};  /* The replacement string */
	char    number[128]={""};   /* The number extension string */
	char    *index=NULL;

	len= strlen(name);
	if(len<3) return; // we don't do names like .R or .L

	/* We first check the case with a .### extension, let's find the last period */
	if(isdigit(name[len-1])) {
		index= strrchr(name, '.'); // last occurrance
		if (index && isdigit(index[1]) ) { // doesnt handle case bone.1abc2 correct..., whatever!
			if(strip_number==0)
				strcpy(number, index);
			*index= 0;
			len= strlen(name);
		}
	}

	strcpy (prefix, name);

#define IS_SEPARATOR(a) ((a)=='.' || (a)==' ' || (a)=='-' || (a)=='_')

	/* first case; separator . - _ with extensions r R l L  */
	if( IS_SEPARATOR(name[len-2]) ) {
		switch(name[len-1]) {
			case 'l':
				prefix[len-1]= 0;
				strcpy(replace, "r");
				break;
			case 'r':
				prefix[len-1]= 0;
				strcpy(replace, "l");
				break;
			case 'L':
				prefix[len-1]= 0;
				strcpy(replace, "R");
				break;
			case 'R':
				prefix[len-1]= 0;
				strcpy(replace, "L");
				break;
		}
	}
	/* case; beginning with r R l L , with separator after it */
	else if( IS_SEPARATOR(name[1]) ) {
		switch(name[0]) {
			case 'l':
				strcpy(replace, "r");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'r':
				strcpy(replace, "l");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'L':
				strcpy(replace, "R");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'R':
				strcpy(replace, "L");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
		}
	}
	else if(len > 5) {
		/* hrms, why test for a separator? lets do the rule 'ultimate left or right' */
		index = BLI_strcasestr(prefix, "right");
		if (index==prefix || index==prefix+len-5) {
			if(index[0]=='r')
				strcpy (replace, "left");
			else {
				if(index[1]=='I')
					strcpy (replace, "LEFT");
				else
					strcpy (replace, "Left");
			}
			*index= 0;
			strcpy (suffix, index+5);
		}
		else {
			index = BLI_strcasestr(prefix, "left");
			if (index==prefix || index==prefix+len-4) {
				if(index[0]=='l')
					strcpy (replace, "right");
				else {
					if(index[1]=='E')
						strcpy (replace, "RIGHT");
					else
						strcpy (replace, "Right");
				}
				*index= 0;
				strcpy (suffix, index+4);
			}
		}
	}

#undef IS_SEPARATOR

	sprintf (name, "%s%s%s%s", prefix, replace, suffix, number);
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
					  Scene *scene, Object *ob, DerivedMesh *dm,
										  int UNUSED(initFlags))
{
	DerivedMesh *cddm = dm; //copying shouldn't be necassary here, as all modifiers return CDDM's
	BMEditMesh *em = CDDM_To_BMesh(ob, cddm, NULL);
	BMOperator op, oldop, weldop;
	int i, j, indexLen;
	/* offset matrix */
	float offset[4][4];
	float final_offset[4][4];
	float tmp_mat[4][4];
	float length = amd->length;
	int count = amd->count, maxVerts;
	int finalVerts, finalEdges, finalFaces;
	int *indexMap = NULL;
	DerivedMesh *start_cap = NULL, *end_cap = NULL;
	MVert *src_mvert;

	/* need to avoid infinite recursion here */
	if(amd->start_cap && amd->start_cap != ob)
		start_cap = mesh_get_derived_final(scene, amd->start_cap, CD_MASK_MESH);
	if(amd->end_cap && amd->end_cap != ob)
		end_cap = mesh_get_derived_final(scene, amd->end_cap, CD_MASK_MESH);

	unit_m4(offset);

	src_mvert = cddm->getVertArray(dm);
	maxVerts = cddm->getNumVerts(dm);

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
		  || amd->fit_type == MOD_ARR_FITCURVE)
	{
		float dist = sqrt(INPR(offset[3], offset[3]));

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

	/* calculate the offset matrix of the final copy (for merging) */
	unit_m4(final_offset);

	for(j=0; j < count - 1; j++) {
		mul_m4_m4m4(tmp_mat, final_offset, offset);
		copy_m4_m4(final_offset, tmp_mat);
	}

	BMO_Init_Op(&weldop, "weldverts");
	BMO_InitOpf(em->bm, &op, "dupe geom=%avef");
	oldop = op;
	for (j=0; j < count; j++) {
		BMVert *v, *v2;
		BMOpSlot *s1;
		BMOpSlot *s2;

		BMO_InitOpf(em->bm, &op, "dupe geom=%s", &oldop, j==0 ? "geom" : "newout");
		BMO_Exec_Op(em->bm, &op);

		s1 = BMO_GetSlot(&op, "geom");
		s2 = BMO_GetSlot(&op, "newout");

		BMO_CallOpf(em->bm, "transform mat=%m4 verts=%s", offset, &op, "newout");

		#define _E(s, i) ((BMVert**)(s)->data.buf)[i]

		/*calculate merge mapping*/
		if (j == 0) {
			BMOperator findop;
			BMOIter oiter;
			BMVert *v, *v2;
			BMHeader *h;

			BMO_InitOpf(em->bm, &findop,
				"finddoubles verts=%av dist=%f keepverts=%s",
				amd->merge_dist, &op, "geom");

			i = 0;
			BMO_ITER(h, &oiter, em->bm, &op, "geom", BM_ALL) {
				BM_SetIndex(h, i);
				i++;
			}

			BMO_ITER(h, &oiter, em->bm, &op, "newout", BM_ALL) {
				BM_SetIndex(h, i);
				i++;
			}

			BMO_Exec_Op(em->bm, &findop);

			indexLen = i;
			indexMap = MEM_callocN(sizeof(int)*indexLen, "indexMap");

			/*element type argument doesn't do anything here*/
			BMO_ITER(v, &oiter, em->bm, &findop, "targetmapout", 0) {
				v2 = BMO_IterMapValp(&oiter);

				indexMap[BM_GetIndex(v)] = BM_GetIndex(v2)+1;
			}

			BMO_Finish_Op(em->bm, &findop);
		}

		/*generate merge mappping using index map.  we do this by using the
		  operator slots as lookup arrays.*/
		#define E(i) (i) < s1->len ? _E(s1, i) : _E(s2, (i)-s1->len)

		for (i=0; i<indexLen; i++) {
			if (!indexMap[i]) continue;

			v = E(i);
			v2 = E(indexMap[i]-1);

			BMO_Insert_MapPointer(em->bm, &weldop, "targetmap", v, v2);
		}

		#undef E
		#undef _E

		BMO_Finish_Op(em->bm, &oldop);
		oldop = op;
	}

	if (j > 0) BMO_Finish_Op(em->bm, &op);

	if (amd->flags & MOD_ARR_MERGE)
		BMO_Exec_Op(em->bm, &weldop);

	BMO_Finish_Op(em->bm, &weldop);

	BMEdit_RecalcTesselation(em);
	cddm = CDDM_from_BMEditMesh(em, NULL, 0);

	BMEdit_Free(em);
	MEM_freeN(indexMap);

	return cddm;
}

static DerivedMesh *applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
		int UNUSED(useRenderParams), int UNUSED(isFinalCalc))
{
	DerivedMesh *result;
	ArrayModifierData *amd = (ArrayModifierData*) md;

	result = arrayModifier_doArray(amd, md->scene, ob, derivedData, 0);

	//if(result != derivedData)
	//	CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *applyModifierEM(
		ModifierData *md, Object *ob, struct BMEditMesh *UNUSED(editData),
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
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
