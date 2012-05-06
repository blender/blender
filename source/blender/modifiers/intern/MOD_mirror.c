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

/** \file blender/modifiers/intern/MOD_mirror.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"
#include "BKE_utildefines.h"
#include "BKE_tessmesh.h"

#include "MEM_guardedalloc.h"
#include "depsgraph_private.h"

static void initData(ModifierData *md)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	mmd->flag |= (MOD_MIR_AXIS_X | MOD_MIR_VGROUP);
	mmd->tolerance = 0.001;
	mmd->mirror_ob = NULL;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;
	MirrorModifierData *tmmd = (MirrorModifierData*) target;

	tmmd->flag = mmd->flag;
	tmmd->tolerance = mmd->tolerance;
	tmmd->mirror_ob = mmd->mirror_ob;
}

static void foreachObjectLink(ModifierData *md, Object *ob,
                              void (*walk)(void *userData, Object *ob, Object **obpoin),
                              void *userData)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	walk(userData, ob, &mmd->mirror_ob);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	if (mmd->mirror_ob) {
		DagNode *latNode = dag_get_node(forest, mmd->mirror_ob);

		dag_add_relation(forest, latNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Mirror Modifier");
	}
}

static DerivedMesh *doMirrorOnAxis(MirrorModifierData *mmd,
                                   Object *ob,
                                   DerivedMesh *dm,
                                   int axis)
{
	const float tolerance_sq = mmd->tolerance * mmd->tolerance;
	const int do_vtargetmap = !(mmd->flag & MOD_MIR_NO_MERGE);
	int is_vtargetmap = FALSE; /* true when it should be used */

	DerivedMesh *result;
	const int maxVerts = dm->getNumVerts(dm);
	const int maxEdges = dm->getNumEdges(dm);
	const int maxLoops = dm->getNumLoops(dm);
	const int maxPolys = dm->getNumPolys(dm);
	MVert *mv, *mv_prev;
	MEdge *me;
	MLoop *ml;
	MPoly *mp;
	float mtx[4][4];
	int i, j;
	int a, totshape;
	int *vtargetmap = NULL, *vtmap_a = NULL, *vtmap_b = NULL;

	/* mtx is the mirror transformation */
	unit_m4(mtx);
	mtx[axis][axis] = -1.0f;

	if (mmd->mirror_ob) {
		float tmp[4][4];
		float itmp[4][4];

		/* tmp is a transform from coords relative to the object's own origin,
		 * to coords relative to the mirror object origin */
		invert_m4_m4(tmp, mmd->mirror_ob->obmat);
		mult_m4_m4m4(tmp, tmp, ob->obmat);

		/* itmp is the reverse transform back to origin-relative coordinates */
		invert_m4_m4(itmp, tmp);

		/* combine matrices to get a single matrix that translates coordinates into
		 * mirror-object-relative space, does the mirror, and translates back to
		 * origin-relative space */
		mult_m4_m4m4(mtx, mtx, tmp);
		mult_m4_m4m4(mtx, itmp, mtx);
	}

	result = CDDM_from_template(dm, maxVerts*2, maxEdges*2, 0, maxLoops*2, maxPolys*2);

	/*copy customdata to original geometry*/
	DM_copy_vert_data(dm, result, 0, 0, maxVerts);
	DM_copy_edge_data(dm, result, 0, 0, maxEdges);
	DM_copy_loop_data(dm, result, 0, 0, maxLoops);
	DM_copy_poly_data(dm, result, 0, 0, maxPolys);


	/* subsurf for eg wont have mesh data in the */
	/* now add mvert/medge/mface layers */

	if (!CustomData_has_layer(&dm->vertData, CD_MVERT)) {
		dm->copyVertArray(dm, CDDM_get_verts(result));
	}
	if (!CustomData_has_layer(&dm->edgeData, CD_MEDGE)) {
		dm->copyEdgeArray(dm, CDDM_get_edges(result));
	}
	if (!CustomData_has_layer(&dm->polyData, CD_MPOLY)) {
		dm->copyLoopArray(dm, CDDM_get_loops(result));
		dm->copyPolyArray(dm, CDDM_get_polys(result));
	}

	/* copy customdata to new geometry,
	 * copy from its self because this data may have been created in the checks above */
	DM_copy_vert_data(result, result, 0, maxVerts, maxVerts);
	DM_copy_edge_data(result, result, 0, maxEdges, maxEdges);
	/* loops are copied later */
	DM_copy_poly_data(result, result, 0, maxPolys, maxPolys);

	if (do_vtargetmap) {
		/* second half is filled with -1 */
		vtargetmap = MEM_mallocN(sizeof(int) * maxVerts * 2, "MOD_mirror tarmap");

		vtmap_a = vtargetmap;
		vtmap_b = vtargetmap + maxVerts;
	}

	/* mirror vertex coordinates */
	mv_prev = CDDM_get_verts(result);
	mv = mv_prev + maxVerts;
	for (i = 0; i < maxVerts; i++, mv++, mv_prev++) {
		mul_m4_v3(mtx, mv->co);

		if (do_vtargetmap) {
			/* compare location of the original and mirrored vertex, to see if they
			 * should be mapped for merging */
			if (UNLIKELY(len_squared_v3v3(mv_prev->co, mv->co) < tolerance_sq)) {
				*vtmap_a = maxVerts + i;
				is_vtargetmap = TRUE;
			}
			else {
				*vtmap_a = -1;
			}

			*vtmap_b = -1; /* fill here to avoid 2x loops */

			vtmap_a++;
			vtmap_b++;
		}
	}
	
	/* handle shape keys */
	totshape = CustomData_number_of_layers(&result->vertData, CD_SHAPEKEY);
	for (a = 0; a < totshape; a++) {
		float (*cos)[3] = CustomData_get_layer_n(&result->vertData, CD_SHAPEKEY, a);
		for (i = maxVerts; i < result->numVertData; i++) {
			mul_m4_v3(mtx, cos[i]);
		}
	}
	
	/* adjust mirrored edge vertex indices */
	me = CDDM_get_edges(result) + maxEdges;
	for (i = 0; i < maxEdges; i++, me++) {
		me->v1 += maxVerts;
		me->v2 += maxVerts;
	}
	
	/* adjust mirrored poly loopstart indices, and reverse loop order (normals) */
	mp = CDDM_get_polys(result) + maxPolys;
	ml = CDDM_get_loops(result);
	for (i = 0; i < maxPolys; i++, mp++) {
		MLoop *ml2;
		int e;

		/* reverse the loop, but we keep the first vertex in the face the same,
		 * to ensure that quads are split the same way as on the other side */
		DM_copy_loop_data(result, result, mp->loopstart, mp->loopstart + maxLoops, 1);
		for (j = 1; j < mp->totloop; j++)
			DM_copy_loop_data(result, result, mp->loopstart + j, mp->loopstart + maxLoops + mp->totloop - j, 1);

		ml2 = ml + mp->loopstart + maxLoops;
		e = ml2[0].e;
		for (j = 0; j < mp->totloop-1; j++) {
			ml2[j].e = ml2[j+1].e;
		}
		ml2[mp->totloop-1].e = e;
		
		mp->loopstart += maxLoops;
	}

	/* adjust mirrored loop vertex and edge indices */
	ml = CDDM_get_loops(result) + maxLoops;
	for (i = 0; i < maxLoops; i++, ml++) {
		ml->v += maxVerts;
		ml->e += maxEdges;
	}

	/* handle uvs,
	 * let tessface recalc handle updating the MTFace data */
	if (mmd->flag & (MOD_MIR_MIRROR_U | MOD_MIR_MIRROR_V)) {
		const int do_mirr_u= (mmd->flag & MOD_MIR_MIRROR_U) != 0;
		const int do_mirr_v= (mmd->flag & MOD_MIR_MIRROR_V) != 0;

		const int totuv = CustomData_number_of_layers(&result->loopData, CD_MLOOPUV);

		for (a = 0; a < totuv; a++) {
			MLoopUV *dmloopuv = CustomData_get_layer_n(&result->loopData, CD_MLOOPUV, a);
			int j = maxLoops;
			dmloopuv += j; /* second set of loops only */
			for ( ; i-- > 0; dmloopuv++) {
				if (do_mirr_u) dmloopuv->uv[0] = 1.0f - dmloopuv->uv[0];
				if (do_mirr_v) dmloopuv->uv[1] = 1.0f - dmloopuv->uv[1];
			}
		}
	}

	/* handle vgroup stuff */
	if ((mmd->flag & MOD_MIR_VGROUP) && CustomData_has_layer(&result->vertData, CD_MDEFORMVERT)) {
		MDeformVert *dvert = (MDeformVert *) CustomData_get_layer(&result->vertData, CD_MDEFORMVERT) + maxVerts;
		int *flip_map= NULL, flip_map_len= 0;

		flip_map= defgroup_flip_map(ob, &flip_map_len, FALSE);
		
		if (flip_map) {
			for (i = 0; i < maxVerts; dvert++, i++) {
				/* merged vertices get both groups, others get flipped */
				if (do_vtargetmap && (vtargetmap[i] != -1))
					defvert_flip_merged(dvert, flip_map, flip_map_len);
				else
					defvert_flip(dvert, flip_map, flip_map_len);
			}

			MEM_freeN(flip_map);
		}
	}

	if (do_vtargetmap) {
		/* slow - so only call if one or more merge verts are found,
		 * users may leave this on and not realize there is nothing to merge - campbell */
		if (is_vtargetmap) {
			result = CDDM_merge_verts(result, vtargetmap);
		}
		MEM_freeN(vtargetmap);
	}

	return result;
}

static DerivedMesh *mirrorModifier__doMirror(MirrorModifierData *mmd,
                                             Object *ob, DerivedMesh *dm)
{
	DerivedMesh *result = dm;

	/* check which axes have been toggled and mirror accordingly */
	if (mmd->flag & MOD_MIR_AXIS_X) {
		result = doMirrorOnAxis(mmd, ob, result, 0);
	}
	if (mmd->flag & MOD_MIR_AXIS_Y) {
		DerivedMesh *tmp = result;
		result = doMirrorOnAxis(mmd, ob, result, 1);
		if (tmp != dm) tmp->release(tmp); /* free intermediate results */
	}
	if (mmd->flag & MOD_MIR_AXIS_Z) {
		DerivedMesh *tmp = result;
		result = doMirrorOnAxis(mmd, ob, result, 2);
		if (tmp != dm) tmp->release(tmp); /* free intermediate results */
	}

	return result;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  int UNUSED(useRenderParams),
                                  int UNUSED(isFinalCalc))
{
	DerivedMesh *result;
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	result = mirrorModifier__doMirror(mmd, ob, derivedData);

	if (result != derivedData)
		CDDM_calc_normals(result);
	
	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_Mirror = {
	/* name */              "Mirror",
	/* structName */        "MirrorModifierData",
	/* structSize */        sizeof(MirrorModifierData),
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
