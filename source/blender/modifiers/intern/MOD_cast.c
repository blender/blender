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

/** \file blender/modifiers/intern/MOD_cast.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"


#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"


#include "depsgraph_private.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	CastModifierData *cmd = (CastModifierData *) md;

	cmd->fac = 0.5f;
	cmd->radius = 0.0f;
	cmd->size = 0.0f;
	cmd->flag = MOD_CAST_X | MOD_CAST_Y | MOD_CAST_Z | MOD_CAST_SIZE_FROM_RADIUS;
	cmd->type = MOD_CAST_TYPE_SPHERE;
	cmd->defgrp_name[0] = '\0';
	cmd->object = NULL;
}


static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	CastModifierData *cmd = (CastModifierData *) md;
	CastModifierData *tcmd = (CastModifierData *) target;
#endif
	modifier_copyData_generic(md, target);
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	CastModifierData *cmd = (CastModifierData *) md;
	short flag;
	
	flag = cmd->flag & (MOD_CAST_X | MOD_CAST_Y | MOD_CAST_Z);

	if ((cmd->fac == 0.0f) || flag == 0) return true;

	return false;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	CastModifierData *cmd = (CastModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (cmd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	CastModifierData *cmd = (CastModifierData *) md;

	walk(userData, ob, &cmd->object, IDWALK_CB_NOP);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	CastModifierData *cmd = (CastModifierData *) md;

	if (cmd->object) {
		DagNode *curNode = dag_get_node(forest, cmd->object);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA,
		                 "Cast Modifier");
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *object,
                            struct DepsNodeHandle *node)
{
	CastModifierData *cmd = (CastModifierData *)md;
	if (cmd->object != NULL) {
		DEG_add_object_relation(node, cmd->object, DEG_OB_COMP_TRANSFORM, "Cast Modifier");
		DEG_add_object_relation(node, object, DEG_OB_COMP_TRANSFORM, "Cast Modifier");
	}
}

static void sphere_do(
        CastModifierData *cmd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	MDeformVert *dvert = NULL;

	Object *ctrl_ob = NULL;

	int i, defgrp_index;
	bool has_radius = false;
	short flag, type;
	float len = 0.0f;
	float fac = cmd->fac;
	float facm = 1.0f - fac;
	const float fac_orig = fac;
	float vec[3], center[3] = {0.0f, 0.0f, 0.0f};
	float mat[4][4], imat[4][4];

	flag = cmd->flag;
	type = cmd->type; /* projection type: sphere or cylinder */

	if (type == MOD_CAST_TYPE_CYLINDER) 
		flag &= ~MOD_CAST_Z;

	ctrl_ob = cmd->object;

	/* spherify's center is {0, 0, 0} (the ob's own center in its local
	 * space), by default, but if the user defined a control object,
	 * we use its location, transformed to ob's local space */
	if (ctrl_ob) {
		if (flag & MOD_CAST_USE_OB_TRANSFORM) {
			invert_m4_m4(imat, ctrl_ob->obmat);
			mul_m4_m4m4(mat, imat, ob->obmat);
			invert_m4_m4(imat, mat);
		}

		invert_m4_m4(ob->imat, ob->obmat);
		mul_v3_m4v3(center, ob->imat, ctrl_ob->obmat[3]);
	}

	/* now we check which options the user wants */

	/* 1) (flag was checked in the "if (ctrl_ob)" block above) */
	/* 2) cmd->radius > 0.0f: only the vertices within this radius from
	 * the center of the effect should be deformed */
	if (cmd->radius > FLT_EPSILON) has_radius = 1;

	/* 3) if we were given a vertex group name,
	 * only those vertices should be affected */
	modifier_get_vgroup(ob, dm, cmd->defgrp_name, &dvert, &defgrp_index);

	if (flag & MOD_CAST_SIZE_FROM_RADIUS) {
		len = cmd->radius;
	}
	else {
		len = cmd->size;
	}

	if (len <= 0) {
		for (i = 0; i < numVerts; i++) {
			len += len_v3v3(center, vertexCos[i]);
		}
		len /= numVerts;

		if (len == 0.0f) len = 10.0f;
	}

	for (i = 0; i < numVerts; i++) {
		float tmp_co[3];

		copy_v3_v3(tmp_co, vertexCos[i]);
		if (ctrl_ob) {
			if (flag & MOD_CAST_USE_OB_TRANSFORM) {
				mul_m4_v3(mat, tmp_co);
			}
			else {
				sub_v3_v3(tmp_co, center);
			}
		}

		copy_v3_v3(vec, tmp_co);

		if (type == MOD_CAST_TYPE_CYLINDER)
			vec[2] = 0.0f;

		if (has_radius) {
			if (len_v3(vec) > cmd->radius) continue;
		}

		if (dvert) {
			const float weight = defvert_find_weight(&dvert[i], defgrp_index);
			if (weight == 0.0f) {
				continue;
			}

			fac = fac_orig * weight;
			facm = 1.0f - fac;
		}

		normalize_v3(vec);

		if (flag & MOD_CAST_X)
			tmp_co[0] = fac * vec[0] * len + facm * tmp_co[0];
		if (flag & MOD_CAST_Y)
			tmp_co[1] = fac * vec[1] * len + facm * tmp_co[1];
		if (flag & MOD_CAST_Z)
			tmp_co[2] = fac * vec[2] * len + facm * tmp_co[2];

		if (ctrl_ob) {
			if (flag & MOD_CAST_USE_OB_TRANSFORM) {
				mul_m4_v3(imat, tmp_co);
			}
			else {
				add_v3_v3(tmp_co, center);
			}
		}

		copy_v3_v3(vertexCos[i], tmp_co);
	}
}

static void cuboid_do(
        CastModifierData *cmd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	MDeformVert *dvert = NULL;
	Object *ctrl_ob = NULL;

	int i, defgrp_index;
	bool has_radius = false;
	short flag;
	float fac = cmd->fac;
	float facm = 1.0f - fac;
	const float fac_orig = fac;
	float min[3], max[3], bb[8][3];
	float center[3] = {0.0f, 0.0f, 0.0f};
	float mat[4][4], imat[4][4];

	flag = cmd->flag;

	ctrl_ob = cmd->object;

	/* now we check which options the user wants */

	/* 1) (flag was checked in the "if (ctrl_ob)" block above) */
	/* 2) cmd->radius > 0.0f: only the vertices within this radius from
	 * the center of the effect should be deformed */
	if (cmd->radius > FLT_EPSILON) has_radius = 1;

	/* 3) if we were given a vertex group name,
	 * only those vertices should be affected */
	modifier_get_vgroup(ob, dm, cmd->defgrp_name, &dvert, &defgrp_index);

	if (ctrl_ob) {
		if (flag & MOD_CAST_USE_OB_TRANSFORM) {
			invert_m4_m4(imat, ctrl_ob->obmat);
			mul_m4_m4m4(mat, imat, ob->obmat);
			invert_m4_m4(imat, mat);
		}

		invert_m4_m4(ob->imat, ob->obmat);
		mul_v3_m4v3(center, ob->imat, ctrl_ob->obmat[3]);
	}

	if ((flag & MOD_CAST_SIZE_FROM_RADIUS) && has_radius) {
		for (i = 0; i < 3; i++) {
			min[i] = -cmd->radius;
			max[i] = cmd->radius;
		}
	}
	else if (!(flag & MOD_CAST_SIZE_FROM_RADIUS) && cmd->size > 0) {
		for (i = 0; i < 3; i++) {
			min[i] = -cmd->size;
			max[i] = cmd->size;
		}
	}
	else {
		/* get bound box */
		/* We can't use the object's bound box because other modifiers
		 * may have changed the vertex data. */
		INIT_MINMAX(min, max);

		/* Cast's center is the ob's own center in its local space,
		 * by default, but if the user defined a control object, we use
		 * its location, transformed to ob's local space. */
		if (ctrl_ob) {
			float vec[3];

			/* let the center of the ctrl_ob be part of the bound box: */
			minmax_v3v3_v3(min, max, center);

			for (i = 0; i < numVerts; i++) {
				sub_v3_v3v3(vec, vertexCos[i], center);
				minmax_v3v3_v3(min, max, vec);
			}
		}
		else {
			for (i = 0; i < numVerts; i++) {
				minmax_v3v3_v3(min, max, vertexCos[i]);
			}
		}

		/* we want a symmetric bound box around the origin */
		if (fabsf(min[0]) > fabsf(max[0])) max[0] = fabsf(min[0]);
		if (fabsf(min[1]) > fabsf(max[1])) max[1] = fabsf(min[1]);
		if (fabsf(min[2]) > fabsf(max[2])) max[2] = fabsf(min[2]);
		min[0] = -max[0];
		min[1] = -max[1];
		min[2] = -max[2];
	}

	/* building our custom bounding box */
	bb[0][0] = bb[2][0] = bb[4][0] = bb[6][0] = min[0];
	bb[1][0] = bb[3][0] = bb[5][0] = bb[7][0] = max[0];
	bb[0][1] = bb[1][1] = bb[4][1] = bb[5][1] = min[1];
	bb[2][1] = bb[3][1] = bb[6][1] = bb[7][1] = max[1];
	bb[0][2] = bb[1][2] = bb[2][2] = bb[3][2] = min[2];
	bb[4][2] = bb[5][2] = bb[6][2] = bb[7][2] = max[2];

	/* ready to apply the effect, one vertex at a time */
	for (i = 0; i < numVerts; i++) {
		int octant, coord;
		float d[3], dmax, apex[3], fbb;
		float tmp_co[3];

		copy_v3_v3(tmp_co, vertexCos[i]);
		if (ctrl_ob) {
			if (flag & MOD_CAST_USE_OB_TRANSFORM) {
				mul_m4_v3(mat, tmp_co);
			}
			else {
				sub_v3_v3(tmp_co, center);
			}
		}

		if (has_radius) {
			if (fabsf(tmp_co[0]) > cmd->radius ||
			    fabsf(tmp_co[1]) > cmd->radius ||
			    fabsf(tmp_co[2]) > cmd->radius)
			{
				continue;
			}
		}

		if (dvert) {
			const float weight = defvert_find_weight(&dvert[i], defgrp_index);
			if (weight == 0.0f) {
				continue;
			}

			fac = fac_orig * weight;
			facm = 1.0f - fac;
		}

		/* The algo used to project the vertices to their
		 * bounding box (bb) is pretty simple:
		 * for each vertex v:
		 * 1) find in which octant v is in;
		 * 2) find which outer "wall" of that octant is closer to v;
		 * 3) calculate factor (var fbb) to project v to that wall;
		 * 4) project. */

		/* find in which octant this vertex is in */
		octant = 0;
		if (tmp_co[0] > 0.0f) octant += 1;
		if (tmp_co[1] > 0.0f) octant += 2;
		if (tmp_co[2] > 0.0f) octant += 4;

		/* apex is the bb's vertex at the chosen octant */
		copy_v3_v3(apex, bb[octant]);

		/* find which bb plane is closest to this vertex ... */
		d[0] = tmp_co[0] / apex[0];
		d[1] = tmp_co[1] / apex[1];
		d[2] = tmp_co[2] / apex[2];

		/* ... (the closest has the higher (closer to 1) d value) */
		dmax = d[0];
		coord = 0;
		if (d[1] > dmax) {
			dmax = d[1];
			coord = 1;
		}
		if (d[2] > dmax) {
			/* dmax = d[2]; */ /* commented, we don't need it */
			coord = 2;
		}

		/* ok, now we know which coordinate of the vertex to use */

		if (fabsf(tmp_co[coord]) < FLT_EPSILON) /* avoid division by zero */
			continue;

		/* finally, this is the factor we wanted, to project the vertex
		 * to its bounding box (bb) */
		fbb = apex[coord] / tmp_co[coord];

		/* calculate the new vertex position */
		if (flag & MOD_CAST_X)
			tmp_co[0] = facm * tmp_co[0] + fac * tmp_co[0] * fbb;
		if (flag & MOD_CAST_Y)
			tmp_co[1] = facm * tmp_co[1] + fac * tmp_co[1] * fbb;
		if (flag & MOD_CAST_Z)
			tmp_co[2] = facm * tmp_co[2] + fac * tmp_co[2] * fbb;

		if (ctrl_ob) {
			if (flag & MOD_CAST_USE_OB_TRANSFORM) {
				mul_m4_v3(imat, tmp_co);
			}
			else {
				add_v3_v3(tmp_co, center);
			}
		}

		copy_v3_v3(vertexCos[i], tmp_co);
	}
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = NULL;
	CastModifierData *cmd = (CastModifierData *)md;

	dm = get_dm(ob, NULL, derivedData, NULL, false, false);

	if (cmd->type == MOD_CAST_TYPE_CUBOID) {
		cuboid_do(cmd, ob, dm, vertexCos, numVerts);
	}
	else { /* MOD_CAST_TYPE_SPHERE or MOD_CAST_TYPE_CYLINDER */
		sphere_do(cmd, ob, dm, vertexCos, numVerts);
	}

	if (dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = get_dm(ob, editData, derivedData, NULL, false, false);
	CastModifierData *cmd = (CastModifierData *)md;

	if (cmd->type == MOD_CAST_TYPE_CUBOID) {
		cuboid_do(cmd, ob, dm, vertexCos, numVerts);
	}
	else { /* MOD_CAST_TYPE_SPHERE or MOD_CAST_TYPE_CYLINDER */
		sphere_do(cmd, ob, dm, vertexCos, numVerts);
	}

	if (dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_Cast = {
	/* name */              "Cast",
	/* structName */        "CastModifierData",
	/* structSize */        sizeof(CastModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_AcceptsLattice |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
