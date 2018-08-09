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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Ben Batt
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_util.c
 *  \ingroup modifiers
 */


#include <string.h>

#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_bitmap.h"
#include "BLI_math_vector.h"
#include "BLI_math_matrix.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_mesh.h"

#include "BKE_modifier.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_util.h"
#include "MOD_modifiertypes.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"

void MOD_init_texture(const Depsgraph *depsgraph, Tex *tex)
{
	if (!tex)
		return;

	if (tex->ima && BKE_image_is_animated(tex->ima)) {
		BKE_image_user_frame_calc(&tex->iuser, DEG_get_ctime(depsgraph), 0);
	}
}

/* TODO to be renamed to get_texture_coords once we are done with moving modifiers to Mesh. */
/** \param cos may be NULL, in which case we use directly mesh vertices' coordinates. */
void MOD_get_texture_coords(
        MappingInfoModifierData *dmd,
        Object *ob,
        Mesh *mesh,
        float (*cos)[3],
        float (*r_texco)[3])
{
	const int numVerts = mesh->totvert;
	int i;
	int texmapping = dmd->texmapping;
	float mapob_imat[4][4];

	if (texmapping == MOD_DISP_MAP_OBJECT) {
		if (dmd->map_object)
			invert_m4_m4(mapob_imat, dmd->map_object->obmat);
		else /* if there is no map object, default to local */
			texmapping = MOD_DISP_MAP_LOCAL;
	}

	/* UVs need special handling, since they come from faces */
	if (texmapping == MOD_DISP_MAP_UV) {
		if (CustomData_has_layer(&mesh->ldata, CD_MLOOPUV)) {
			MPoly *mpoly = mesh->mpoly;
			MPoly *mp;
			MLoop *mloop = mesh->mloop;
			BLI_bitmap *done = BLI_BITMAP_NEW(numVerts, __func__);
			const int numPolys = mesh->totpoly;
			char uvname[MAX_CUSTOMDATA_LAYER_NAME];
			MLoopUV *mloop_uv;

			CustomData_validate_layer_name(&mesh->ldata, CD_MLOOPUV, dmd->uvlayer_name, uvname);
			mloop_uv = CustomData_get_layer_named(&mesh->ldata, CD_MLOOPUV, uvname);

			/* verts are given the UV from the first face that uses them */
			for (i = 0, mp = mpoly; i < numPolys; ++i, ++mp) {
				unsigned int fidx = mp->totloop - 1;

				do {
					unsigned int lidx = mp->loopstart + fidx;
					unsigned int vidx = mloop[lidx].v;

					if (!BLI_BITMAP_TEST(done, vidx)) {
						/* remap UVs from [0, 1] to [-1, 1] */
						r_texco[vidx][0] = (mloop_uv[lidx].uv[0] * 2.0f) - 1.0f;
						r_texco[vidx][1] = (mloop_uv[lidx].uv[1] * 2.0f) - 1.0f;
						BLI_BITMAP_ENABLE(done, vidx);
					}

				} while (fidx--);
			}

			MEM_freeN(done);
			return;
		}
		else {
			/* if there are no UVs, default to local */
			texmapping = MOD_DISP_MAP_LOCAL;
		}
	}

	MVert *mv = mesh->mvert;
	for (i = 0; i < numVerts; ++i, ++mv, ++r_texco) {
		switch (texmapping) {
			case MOD_DISP_MAP_LOCAL:
				copy_v3_v3(*r_texco, cos != NULL ? *cos : mv->co);
				break;
			case MOD_DISP_MAP_GLOBAL:
				mul_v3_m4v3(*r_texco, ob->obmat, cos != NULL ? *cos : mv->co);
				break;
			case MOD_DISP_MAP_OBJECT:
				mul_v3_m4v3(*r_texco, ob->obmat, cos != NULL ? *cos : mv->co);
				mul_m4_v3(mapob_imat, *r_texco);
				break;
		}
		if (cos != NULL) {
			cos++;
		}
	}
}

void MOD_previous_vcos_store(ModifierData *md, float (*vertexCos)[3])
{
	while ((md = md->next) && md->type == eModifierType_Armature) {
		ArmatureModifierData *amd = (ArmatureModifierData *) md;
		if (amd->multi && amd->prevCos == NULL)
			amd->prevCos = MEM_dupallocN(vertexCos);
		else
			break;
	}
	/* lattice/mesh modifier too */
}

/* returns a mesh if mesh == NULL, for deforming modifiers that need it */
Mesh *MOD_get_mesh_eval(
        Object *ob, struct BMEditMesh *em, Mesh *mesh,
        float (*vertexCos)[3], bool use_normals, bool use_orco)
{
	if (mesh) {
		/* pass */
	}
	else if (ob->type == OB_MESH) {
		if (em) {
			mesh = BKE_bmesh_to_mesh_nomain(em->bm, &(struct BMeshToMeshParams){0});
		}
		else {
			/* TODO(sybren): after modifier conversion of DM to Mesh is done, check whether
			 * we really need a copy here. Maybe the CoW ob->data can be directly used. */
			BKE_id_copy_ex(
			        NULL, ob->data, (ID **)&mesh,
			        (LIB_ID_CREATE_NO_MAIN |
			         LIB_ID_CREATE_NO_USER_REFCOUNT |
			         LIB_ID_CREATE_NO_DEG_TAG |
			         LIB_ID_COPY_NO_PREVIEW |
			         LIB_ID_COPY_CD_REFERENCE),
			        false);
			mesh->runtime.deformed_only = 1;
		}

		/* TODO(sybren): after modifier conversion of DM to Mesh is done, check whether
		 * we really need vertexCos here. */
		if (vertexCos) {
			BKE_mesh_apply_vert_coords(mesh, vertexCos);
			mesh->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
		}

		if (use_orco) {
			CustomData_add_layer(&mesh->vdata, CD_ORCO, CD_ASSIGN, BKE_mesh_orco_verts_get(ob), mesh->totvert);
		}
	}
	else if (ELEM(ob->type, OB_FONT, OB_CURVE, OB_SURF)) {
		/* TODO(sybren): get evaluated mesh from depsgraph once that's properly generated for curves. */
		mesh = BKE_mesh_new_nomain_from_curve(ob);
	}

	if (use_normals) {
		if (LIKELY(mesh)) {
			BKE_mesh_ensure_normals(mesh);
		}
	}

	return mesh;
}

void MOD_get_vgroup(Object *ob, struct Mesh *mesh, const char *name, MDeformVert **dvert, int *defgrp_index)
{
	*defgrp_index = defgroup_name_index(ob, name);
	*dvert = NULL;

	if (*defgrp_index != -1) {
		if (ob->type == OB_LATTICE)
			*dvert = BKE_lattice_deform_verts_get(ob);
		else if (mesh)
			*dvert = mesh->dvert;
	}
}


/* only called by BKE_modifier.h/modifier.c */
void modifier_type_init(ModifierTypeInfo *types[])
{
#define INIT_TYPE(typeName) (types[eModifierType_##typeName] = &modifierType_##typeName)
	INIT_TYPE(None);
	INIT_TYPE(Curve);
	INIT_TYPE(Lattice);
	INIT_TYPE(Subsurf);
	INIT_TYPE(Build);
	INIT_TYPE(Array);
	INIT_TYPE(Mirror);
	INIT_TYPE(EdgeSplit);
	INIT_TYPE(Bevel);
	INIT_TYPE(Displace);
	INIT_TYPE(UVProject);
	INIT_TYPE(Decimate);
	INIT_TYPE(Smooth);
	INIT_TYPE(Cast);
	INIT_TYPE(Wave);
	INIT_TYPE(Armature);
	INIT_TYPE(Hook);
	INIT_TYPE(Softbody);
	INIT_TYPE(Cloth);
	INIT_TYPE(Collision);
	INIT_TYPE(Boolean);
	INIT_TYPE(MeshDeform);
	INIT_TYPE(Ocean);
	INIT_TYPE(ParticleSystem);
	INIT_TYPE(ParticleInstance);
	INIT_TYPE(Explode);
	INIT_TYPE(Shrinkwrap);
	INIT_TYPE(Fluidsim);
	INIT_TYPE(Mask);
	INIT_TYPE(SimpleDeform);
	INIT_TYPE(Multires);
	INIT_TYPE(Surface);
	INIT_TYPE(Smoke);
	INIT_TYPE(ShapeKey);
	INIT_TYPE(Solidify);
	INIT_TYPE(Screw);
	INIT_TYPE(Warp);
	INIT_TYPE(WeightVGEdit);
	INIT_TYPE(WeightVGMix);
	INIT_TYPE(WeightVGProximity);
	INIT_TYPE(DynamicPaint);
	INIT_TYPE(Remesh);
	INIT_TYPE(Skin);
	INIT_TYPE(LaplacianSmooth);
	INIT_TYPE(Triangulate);
	INIT_TYPE(UVWarp);
	INIT_TYPE(MeshCache);
	INIT_TYPE(LaplacianDeform);
	INIT_TYPE(Wireframe);
	INIT_TYPE(DataTransfer);
	INIT_TYPE(NormalEdit);
	INIT_TYPE(CorrectiveSmooth);
	INIT_TYPE(MeshSequenceCache);
	INIT_TYPE(SurfaceDeform);
	INIT_TYPE(WeightedNormal);
#undef INIT_TYPE
}
