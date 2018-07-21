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

/** \file blender/modifiers/intern/MOD_displace.c
 *  \ingroup modifiers
 */


#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_image.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"
#include "BKE_deform.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_util.h"

#include "RE_shader_ext.h"


/* Displace */

static void initData(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *) md;

	dmd->texture = NULL;
	dmd->strength = 1;
	dmd->direction = MOD_DISP_DIR_NOR;
	dmd->midlevel = 0.5;
	dmd->space = MOD_DISP_SPACE_LOCAL;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (dmd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	/* ask for UV coordinates if we need them */
	if (dmd->texmapping == MOD_DISP_MAP_UV) dataMask |= CD_MASK_MTFACE;

	if (dmd->direction == MOD_DISP_DIR_CLNOR) {
		dataMask |= CD_MASK_CUSTOMLOOPNORMAL;
	}

	return dataMask;
}

static bool dependsOnTime(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;

	if (dmd->texture) {
		return BKE_texture_dependsOnTime(dmd->texture);
	}
	else {
		return false;
	}
}

static bool dependsOnNormals(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;
	return ELEM(dmd->direction, MOD_DISP_DIR_NOR, MOD_DISP_DIR_CLNOR);
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *) md;

	walk(userData, ob, &dmd->map_object, IDWALK_CB_NOP);
}

static void foreachIDLink(
        ModifierData *md, Object *ob,
        IDWalkFunc walk, void *userData)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *) md;

	walk(userData, ob, (ID **)&dmd->texture, IDWALK_CB_USER);

	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void foreachTexLink(
        ModifierData *md, Object *ob,
        TexWalkFunc walk, void *userData)
{
	walk(userData, ob, md, "texture");
}

static bool isDisabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	DisplaceModifierData *dmd = (DisplaceModifierData *) md;
	return ((!dmd->texture && dmd->direction == MOD_DISP_DIR_RGB_XYZ) || dmd->strength == 0.0f);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;
	if (dmd->map_object != NULL && dmd->texmapping == MOD_DISP_MAP_OBJECT) {
		DEG_add_object_relation(ctx->node, dmd->map_object, DEG_OB_COMP_TRANSFORM, "Displace Modifier");
	}
	if (dmd->texmapping == MOD_DISP_MAP_GLOBAL ||
	    (ELEM(dmd->direction, MOD_DISP_DIR_X, MOD_DISP_DIR_Y, MOD_DISP_DIR_Z, MOD_DISP_DIR_RGB_XYZ) &&
	    dmd->space == MOD_DISP_SPACE_GLOBAL))
	{
		DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Displace Modifier");
	}
}

typedef struct DisplaceUserdata {
	/*const*/ DisplaceModifierData *dmd;
	struct Scene *scene;
	struct ImagePool *pool;
	MDeformVert *dvert;
	float weight;
	int defgrp_index;
	int direction;
	bool use_global_direction;
	float (*tex_co)[3];
	float (*vertexCos)[3];
	float local_mat[4][4];
	MVert *mvert;
	float (*vert_clnors)[3];
} DisplaceUserdata;

static void displaceModifier_do_task(
        void *__restrict userdata,
        const int iter,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	DisplaceUserdata *data = (DisplaceUserdata *)userdata;
	DisplaceModifierData *dmd = data->dmd;
	MDeformVert *dvert = data->dvert;
	float weight = data->weight;
	int defgrp_index = data->defgrp_index;
	int direction = data->direction;
	bool use_global_direction = data->use_global_direction;
	float (*tex_co)[3] = data->tex_co;
	float (*vertexCos)[3] = data->vertexCos;
	MVert *mvert = data->mvert;
	float (*vert_clnors)[3] = data->vert_clnors;

	const float delta_fixed = 1.0f - dmd->midlevel;  /* when no texture is used, we fallback to white */

	TexResult texres;
	float strength = dmd->strength;
	float delta;
	float local_vec[3];

	if (dvert) {
		weight = defvert_find_weight(dvert + iter, defgrp_index);
		if (weight == 0.0f) {
			return;
		}
	}

	if (dmd->texture) {
		texres.nor = NULL;
		BKE_texture_get_value_ex(data->scene, dmd->texture, tex_co[iter], &texres, data->pool, false);
		delta = texres.tin - dmd->midlevel;
	}
	else {
		delta = delta_fixed;  /* (1.0f - dmd->midlevel) */  /* never changes */
	}

	if (dvert) {
		strength *= weight;
	}

	delta *= strength;
	CLAMP(delta, -10000, 10000);

	switch (direction) {
		case MOD_DISP_DIR_X:
			if (use_global_direction) {
				vertexCos[iter][0] += delta * data->local_mat[0][0];
				vertexCos[iter][1] += delta * data->local_mat[1][0];
				vertexCos[iter][2] += delta * data->local_mat[2][0];
			}
			else {
				vertexCos[iter][0] += delta;
			}
			break;
		case MOD_DISP_DIR_Y:
			if (use_global_direction) {
				vertexCos[iter][0] += delta * data->local_mat[0][1];
				vertexCos[iter][1] += delta * data->local_mat[1][1];
				vertexCos[iter][2] += delta * data->local_mat[2][1];
			}
			else {
				vertexCos[iter][1] += delta;
			}
			break;
		case MOD_DISP_DIR_Z:
			if (use_global_direction) {
				vertexCos[iter][0] += delta * data->local_mat[0][2];
				vertexCos[iter][1] += delta * data->local_mat[1][2];
				vertexCos[iter][2] += delta * data->local_mat[2][2];
			}
			else {
				vertexCos[iter][2] += delta;
			}
			break;
		case MOD_DISP_DIR_RGB_XYZ:
			local_vec[0] = texres.tr - dmd->midlevel;
			local_vec[1] = texres.tg - dmd->midlevel;
			local_vec[2] = texres.tb - dmd->midlevel;
			if (use_global_direction) {
				mul_transposed_mat3_m4_v3(data->local_mat, local_vec);
			}
			mul_v3_fl(local_vec, strength);
			add_v3_v3(vertexCos[iter], local_vec);
			break;
		case MOD_DISP_DIR_NOR:
			vertexCos[iter][0] += delta * (mvert[iter].no[0] / 32767.0f);
			vertexCos[iter][1] += delta * (mvert[iter].no[1] / 32767.0f);
			vertexCos[iter][2] += delta * (mvert[iter].no[2] / 32767.0f);
			break;
		case MOD_DISP_DIR_CLNOR:
			madd_v3_v3fl(vertexCos[iter], vert_clnors[iter], delta);
			break;
	}
}

static void displaceModifier_do(
        DisplaceModifierData *dmd, const ModifierEvalContext *ctx,
        Mesh *mesh, float (*vertexCos)[3], const int numVerts)
{
	Object *ob = ctx->object;
	Depsgraph *depsgraph = ctx->depsgraph;
	MVert *mvert;
	MDeformVert *dvert;
	int direction = dmd->direction;
	int defgrp_index;
	float (*tex_co)[3];
	float weight = 1.0f; /* init value unused but some compilers may complain */
	float (*vert_clnors)[3] = NULL;
	float local_mat[4][4] = {{0}};
	const bool use_global_direction = dmd->space == MOD_DISP_SPACE_GLOBAL;

	if (!dmd->texture && dmd->direction == MOD_DISP_DIR_RGB_XYZ) return;
	if (dmd->strength == 0.0f) return;

	mvert = mesh->mvert;
	MOD_get_vgroup(ob, mesh, dmd->defgrp_name, &dvert, &defgrp_index);

	if (dmd->texture) {
		tex_co = MEM_calloc_arrayN((size_t)numVerts, sizeof(*tex_co),
		                     "displaceModifier_do tex_co");
		MOD_get_texture_coords((MappingInfoModifierData *)dmd, ob, mesh, vertexCos, tex_co);

		MOD_init_texture(depsgraph, dmd->texture);
	}
	else {
		tex_co = NULL;
	}

	if (direction == MOD_DISP_DIR_CLNOR) {
		CustomData *ldata = &mesh->ldata;

		if (CustomData_has_layer(ldata, CD_CUSTOMLOOPNORMAL)) {
			float (*clnors)[3] = NULL;

			if ((mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) || !CustomData_has_layer(ldata, CD_NORMAL)) {
				BKE_mesh_calc_normals_split(mesh);
			}

			clnors = CustomData_get_layer(ldata, CD_NORMAL);
			vert_clnors = MEM_malloc_arrayN(numVerts, sizeof(*vert_clnors), __func__);
			BKE_mesh_normals_loop_to_vertex(numVerts, mesh->mloop, mesh->totloop,
			                                (const float (*)[3])clnors, vert_clnors);
		}
		else {
			direction = MOD_DISP_DIR_NOR;
		}
	}
	else if (ELEM(direction, MOD_DISP_DIR_X, MOD_DISP_DIR_Y, MOD_DISP_DIR_Z, MOD_DISP_DIR_RGB_XYZ) &&
	         use_global_direction)
	{
		copy_m4_m4(local_mat, ob->obmat);
	}

	DisplaceUserdata data = {NULL};
	data.scene = DEG_get_evaluated_scene(ctx->depsgraph);
	data.dmd = dmd;
	data.dvert = dvert;
	data.weight = weight;
	data.defgrp_index = defgrp_index;
	data.direction = direction;
	data.use_global_direction = use_global_direction;
	data.tex_co = tex_co;
	data.vertexCos = vertexCos;
	copy_m4_m4(data.local_mat, local_mat);
	data.mvert = mvert;
	data.vert_clnors = vert_clnors;
	if (dmd->texture != NULL) {
		data.pool = BKE_image_pool_new();
		BKE_texture_fetch_images_for_pool(dmd->texture, data.pool);
	}
	ParallelRangeSettings settings;
	BLI_parallel_range_settings_defaults(&settings);
	settings.use_threading = (numVerts > 512);
	BLI_task_parallel_range(0, numVerts,
	                        &data,
	                        displaceModifier_do_task,
	                        &settings);

	if (data.pool != NULL) {
		BKE_image_pool_free(data.pool);
	}

	if (tex_co) {
		MEM_freeN(tex_co);
	}

	if (vert_clnors) {
		MEM_freeN(vert_clnors);
	}
}

static void deformVerts(
        ModifierData *md,
        const ModifierEvalContext *ctx,
        Mesh *mesh,
        float (*vertexCos)[3],
        int numVerts)
{
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, NULL, mesh, NULL, false, false);

	BLI_assert(mesh_src->totvert == numVerts);

	displaceModifier_do((DisplaceModifierData *)md, ctx, mesh_src, vertexCos, numVerts);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void deformVertsEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *editData,
        Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, editData, mesh, NULL, false, false);

	BLI_assert(mesh_src->totvert == numVerts);

	displaceModifier_do((DisplaceModifierData *)md, ctx, mesh_src, vertexCos, numVerts);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}


ModifierTypeInfo modifierType_Displace = {
	/* name */              "Displace",
	/* structName */        "DisplaceModifierData",
	/* structSize */        sizeof(DisplaceModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          modifier_copyData_generic,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

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
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	dependsOnNormals,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
