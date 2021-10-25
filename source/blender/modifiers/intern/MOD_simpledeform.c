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

/** \file blender/modifiers/intern/MOD_simpledeform.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"


#include "depsgraph_private.h"

#include "MOD_util.h"

#define BEND_EPS 0.000001f

/* Clamps/Limits the given coordinate to:  limits[0] <= co[axis] <= limits[1]
 * The amount of clamp is saved on dcut */
static void axis_limit(int axis, const float limits[2], float co[3], float dcut[3])
{
	float val = co[axis];
	if (limits[0] > val) val = limits[0];
	if (limits[1] < val) val = limits[1];

	dcut[axis] = co[axis] - val;
	co[axis] = val;
}

static void simpleDeform_taper(const float factor, const float dcut[3], float r_co[3])
{
	float x = r_co[0], y = r_co[1], z = r_co[2];
	float scale = z * factor;

	r_co[0] = x + x * scale;
	r_co[1] = y + y * scale;
	r_co[2] = z;

	{
		r_co[0] += dcut[0];
		r_co[1] += dcut[1];
		r_co[2] += dcut[2];
	}
}

static void simpleDeform_stretch(const float factor, const float dcut[3], float r_co[3])
{
	float x = r_co[0], y = r_co[1], z = r_co[2];
	float scale;

	scale = (z * z * factor - factor + 1.0f);

	r_co[0] = x * scale;
	r_co[1] = y * scale;
	r_co[2] = z * (1.0f + factor);

	{
		r_co[0] += dcut[0];
		r_co[1] += dcut[1];
		r_co[2] += dcut[2];
	}
}

static void simpleDeform_twist(const float factor, const float *dcut, float r_co[3])
{
	float x = r_co[0], y = r_co[1], z = r_co[2];
	float theta, sint, cost;

	theta = z * factor;
	sint  = sinf(theta);
	cost  = cosf(theta);

	r_co[0] = x * cost - y * sint;
	r_co[1] = x * sint + y * cost;
	r_co[2] = z;

	{
		r_co[0] += dcut[0];
		r_co[1] += dcut[1];
		r_co[2] += dcut[2];
	}
}

static void simpleDeform_bend(const float factor, const float dcut[3], float r_co[3])
{
	float x = r_co[0], y = r_co[1], z = r_co[2];
	float theta, sint, cost;

	BLI_assert(!(fabsf(factor) < BEND_EPS));

	theta = x * factor;
	sint = sinf(theta);
	cost = cosf(theta);

	r_co[0] = -(y - 1.0f / factor) * sint;
	r_co[1] =  (y - 1.0f / factor) * cost + 1.0f / factor;
	r_co[2] = z;

	{
		r_co[0] += cost * dcut[0];
		r_co[1] += sint * dcut[0];
		r_co[2] += dcut[2];
	}

}


/* simple deform modifier */
static void SimpleDeformModifier_do(SimpleDeformModifierData *smd, struct Object *ob, struct DerivedMesh *dm,
                                    float (*vertexCos)[3], int numVerts)
{
	static const float lock_axis[2] = {0.0f, 0.0f};

	int i;
	int limit_axis = 0;
	float smd_limit[2], smd_factor;
	SpaceTransform *transf = NULL, tmp_transf;
	void (*simpleDeform_callback)(const float factor, const float dcut[3], float co[3]) = NULL;  /* Mode callback */
	int vgroup;
	MDeformVert *dvert;

	/* Safe-check */
	if (smd->origin == ob) smd->origin = NULL;  /* No self references */

	if (smd->limit[0] < 0.0f) smd->limit[0] = 0.0f;
	if (smd->limit[0] > 1.0f) smd->limit[0] = 1.0f;

	smd->limit[0] = min_ff(smd->limit[0], smd->limit[1]);  /* Upper limit >= than lower limit */

	/* Calculate matrixs do convert between coordinate spaces */
	if (smd->origin) {
		transf = &tmp_transf;
		BLI_SPACE_TRANSFORM_SETUP(transf, ob, smd->origin);
	}

	/* Setup vars,
	 * Bend limits on X.. all other modes limit on Z */
	limit_axis  = (smd->mode == MOD_SIMPLEDEFORM_MODE_BEND) ? 0 : 2;

	/* Update limits if needed */
	{
		float lower =  FLT_MAX;
		float upper = -FLT_MAX;

		for (i = 0; i < numVerts; i++) {
			float tmp[3];
			copy_v3_v3(tmp, vertexCos[i]);

			if (transf) {
				BLI_space_transform_apply(transf, tmp);
			}

			lower = min_ff(lower, tmp[limit_axis]);
			upper = max_ff(upper, tmp[limit_axis]);
		}


		/* SMD values are normalized to the BV, calculate the absolut values */
		smd_limit[1] = lower + (upper - lower) * smd->limit[1];
		smd_limit[0] = lower + (upper - lower) * smd->limit[0];

		smd_factor   = smd->factor / max_ff(FLT_EPSILON, smd_limit[1] - smd_limit[0]);
	}

	switch (smd->mode) {
		case MOD_SIMPLEDEFORM_MODE_TWIST:   simpleDeform_callback = simpleDeform_twist;     break;
		case MOD_SIMPLEDEFORM_MODE_BEND:    simpleDeform_callback = simpleDeform_bend;      break;
		case MOD_SIMPLEDEFORM_MODE_TAPER:   simpleDeform_callback = simpleDeform_taper;     break;
		case MOD_SIMPLEDEFORM_MODE_STRETCH: simpleDeform_callback = simpleDeform_stretch;   break;
		default:
			return; /* No simpledeform mode? */
	}

	if (smd->mode == MOD_SIMPLEDEFORM_MODE_BEND) {
		if (fabsf(smd_factor) < BEND_EPS) {
			return;
		}
	}

	modifier_get_vgroup(ob, dm, smd->vgroup_name, &dvert, &vgroup);
	const bool invert_vgroup = (smd->flag & MOD_SIMPLEDEFORM_FLAG_INVERT_VGROUP) != 0;

	for (i = 0; i < numVerts; i++) {
		float weight = defvert_array_find_weight_safe(dvert, i, vgroup);

		if (invert_vgroup) {
			weight = 1.0f - weight;
		}

		if (weight != 0.0f) {
			float co[3], dcut[3] = {0.0f, 0.0f, 0.0f};

			if (transf) {
				BLI_space_transform_apply(transf, vertexCos[i]);
			}

			copy_v3_v3(co, vertexCos[i]);

			/* Apply axis limits */
			if (smd->mode != MOD_SIMPLEDEFORM_MODE_BEND) { /* Bend mode shoulnt have any lock axis */
				if (smd->axis & MOD_SIMPLEDEFORM_LOCK_AXIS_X) axis_limit(0, lock_axis, co, dcut);
				if (smd->axis & MOD_SIMPLEDEFORM_LOCK_AXIS_Y) axis_limit(1, lock_axis, co, dcut);
			}
			axis_limit(limit_axis, smd_limit, co, dcut);

			simpleDeform_callback(smd_factor, dcut, co);  /* apply deform */
			interp_v3_v3v3(vertexCos[i], vertexCos[i], co, weight);  /* Use vertex weight has coef of linear interpolation */

			if (transf) {
				BLI_space_transform_invert(transf, vertexCos[i]);
			}
		}
	}
}


/* SimpleDeform */
static void initData(ModifierData *md)
{
	SimpleDeformModifierData *smd = (SimpleDeformModifierData *) md;

	smd->mode = MOD_SIMPLEDEFORM_MODE_TWIST;
	smd->axis = 0;

	smd->origin   =  NULL;
	smd->factor   =  DEG2RADF(45.0f);
	smd->limit[0] =  0.0f;
	smd->limit[1] =  1.0f;
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	SimpleDeformModifierData *smd  = (SimpleDeformModifierData *)md;
	SimpleDeformModifierData *tsmd = (SimpleDeformModifierData *)target;
#endif
	modifier_copyData_generic(md, target);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (smd->vgroup_name[0])
		dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	SimpleDeformModifierData *smd  = (SimpleDeformModifierData *)md;
	walk(userData, ob, &smd->origin, IDWALK_CB_NOP);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	SimpleDeformModifierData *smd  = (SimpleDeformModifierData *)md;

	if (smd->origin)
		dag_add_relation(forest, dag_get_node(forest, smd->origin), obNode, DAG_RL_OB_DATA, "SimpleDeform Modifier");
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *UNUSED(ob),
                            struct DepsNodeHandle *node)
{
	SimpleDeformModifierData *smd  = (SimpleDeformModifierData *)md;
	if (smd->origin != NULL) {
		DEG_add_object_relation(node, smd->origin, DEG_OB_COMP_TRANSFORM, "SimpleDeform Modifier");
	}
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = requiredDataMask(ob, md);

	/* we implement requiredDataMask but thats not really useful since
	 * mesh_calc_modifiers pass a NULL derivedData */
	if (dataMask)
		dm = get_dm(ob, NULL, dm, NULL, false, false);

	SimpleDeformModifier_do((SimpleDeformModifierData *)md, ob, dm, vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(ModifierData *md, Object *ob,
                          struct BMEditMesh *editData,
                          DerivedMesh *derivedData,
                          float (*vertexCos)[3],
                          int numVerts)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = requiredDataMask(ob, md);

	/* we implement requiredDataMask but thats not really useful since
	 * mesh_calc_modifiers pass a NULL derivedData */
	if (dataMask)
		dm = get_dm(ob, editData, dm, NULL, false, false);

	SimpleDeformModifier_do((SimpleDeformModifierData *)md, ob, dm, vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_SimpleDeform = {
	/* name */              "SimpleDeform",
	/* structName */        "SimpleDeformModifierData",
	/* structSize */        sizeof(SimpleDeformModifierData),
	/* type */              eModifierTypeType_OnlyDeform,

	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_AcceptsLattice |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,

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
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
