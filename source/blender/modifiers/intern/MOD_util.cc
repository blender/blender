/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h" /* BKE_pose_channel_find_name */
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_object.h"

#include "BKE_modifier.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.hh"
#include "MOD_util.hh"

#include "MEM_guardedalloc.h"

void MOD_init_texture(MappingInfoModifierData *dmd, const ModifierEvalContext *ctx)
{
  Tex *tex = dmd->texture;

  if (tex == nullptr) {
    return;
  }

  if (tex->ima && BKE_image_is_animated(tex->ima)) {
    BKE_image_user_frame_calc(tex->ima, &tex->iuser, DEG_get_ctime(ctx->depsgraph));
  }
}

void MOD_get_texture_coords(MappingInfoModifierData *dmd,
                            const ModifierEvalContext * /*ctx*/,
                            Object *ob,
                            Mesh *mesh,
                            float (*cos)[3],
                            float (*r_texco)[3])
{
  /* TODO: to be renamed to `get_texture_coords` once we are done with moving modifiers to Mesh. */

  using namespace blender;
  const int verts_num = mesh->totvert;
  int i;
  int texmapping = dmd->texmapping;
  float mapref_imat[4][4];

  if (texmapping == MOD_DISP_MAP_OBJECT) {
    if (dmd->map_object != nullptr) {
      Object *map_object = dmd->map_object;
      if (dmd->map_bone[0] != '\0') {
        bPoseChannel *pchan = BKE_pose_channel_find_name(map_object->pose, dmd->map_bone);
        if (pchan) {
          float mat_bone_world[4][4];
          mul_m4_m4m4(mat_bone_world, map_object->object_to_world, pchan->pose_mat);
          invert_m4_m4(mapref_imat, mat_bone_world);
        }
        else {
          invert_m4_m4(mapref_imat, map_object->object_to_world);
        }
      }
      else {
        invert_m4_m4(mapref_imat, map_object->object_to_world);
      }
    }
    else { /* if there is no map object, default to local */
      texmapping = MOD_DISP_MAP_LOCAL;
    }
  }

  /* UVs need special handling, since they come from faces */
  if (texmapping == MOD_DISP_MAP_UV) {
    if (CustomData_has_layer(&mesh->ldata, CD_PROP_FLOAT2)) {
      const OffsetIndices polys = mesh->polys();
      const Span<int> corner_verts = mesh->corner_verts();
      BLI_bitmap *done = BLI_BITMAP_NEW(verts_num, __func__);
      char uvname[MAX_CUSTOMDATA_LAYER_NAME];
      CustomData_validate_layer_name(&mesh->ldata, CD_PROP_FLOAT2, dmd->uvlayer_name, uvname);
      const float(*mloop_uv)[2] = static_cast<const float(*)[2]>(
          CustomData_get_layer_named(&mesh->ldata, CD_PROP_FLOAT2, uvname));

      /* verts are given the UV from the first face that uses them */
      for (const int i : polys.index_range()) {
        const IndexRange poly = polys[i];
        uint fidx = poly.size() - 1;

        do {
          uint lidx = poly.start() + fidx;
          const int vidx = corner_verts[lidx];

          if (!BLI_BITMAP_TEST(done, vidx)) {
            /* remap UVs from [0, 1] to [-1, 1] */
            r_texco[vidx][0] = (mloop_uv[lidx][0] * 2.0f) - 1.0f;
            r_texco[vidx][1] = (mloop_uv[lidx][1] * 2.0f) - 1.0f;
            BLI_BITMAP_ENABLE(done, vidx);
          }

        } while (fidx--);
      }

      MEM_freeN(done);
      return;
    }

    /* if there are no UVs, default to local */
    texmapping = MOD_DISP_MAP_LOCAL;
  }

  const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
  for (i = 0; i < verts_num; i++, r_texco++) {
    switch (texmapping) {
      case MOD_DISP_MAP_LOCAL:
        copy_v3_v3(*r_texco, cos != nullptr ? *cos : positions[i]);
        break;
      case MOD_DISP_MAP_GLOBAL:
        mul_v3_m4v3(*r_texco, ob->object_to_world, cos != nullptr ? *cos : positions[i]);
        break;
      case MOD_DISP_MAP_OBJECT:
        mul_v3_m4v3(*r_texco, ob->object_to_world, cos != nullptr ? *cos : positions[i]);
        mul_m4_v3(mapref_imat, *r_texco);
        break;
    }
    if (cos != nullptr) {
      cos++;
    }
  }
}

void MOD_previous_vcos_store(ModifierData *md, const float (*vert_coords)[3])
{
  while ((md = md->next) && md->type == eModifierType_Armature) {
    ArmatureModifierData *amd = (ArmatureModifierData *)md;
    if (amd->multi && amd->vert_coords_prev == nullptr) {
      amd->vert_coords_prev = static_cast<float(*)[3]>(MEM_dupallocN(vert_coords));
    }
    else {
      break;
    }
  }
  /* lattice/mesh modifier too */
}

Mesh *MOD_deform_mesh_eval_get(Object *ob,
                               struct BMEditMesh *em,
                               Mesh *mesh,
                               const float (*vertexCos)[3],
                               const int verts_num,
                               const bool use_orco)
{
  if (mesh != nullptr) {
    /* pass */
  }
  else if (ob->type == OB_MESH) {
    if (em) {
      mesh = BKE_mesh_wrapper_from_editmesh_with_coords(
          em, nullptr, vertexCos, static_cast<const Mesh *>(ob->data));
    }
    else {
      /* TODO(sybren): after modifier conversion of DM to Mesh is done, check whether
       * we really need a copy here. Maybe the CoW ob->data can be directly used. */
      Mesh *mesh_prior_modifiers = BKE_object_get_pre_modified_mesh(ob);
      mesh = (Mesh *)BKE_id_copy_ex(
          nullptr, &mesh_prior_modifiers->id, nullptr, LIB_ID_COPY_LOCALIZE);
      mesh->runtime->deformed_only = true;
    }

    if (em != nullptr) {
      /* pass */
    }
    /* TODO(sybren): after modifier conversion of DM to Mesh is done, check whether
     * we really need vertexCos here. */
    else if (vertexCos) {
      BKE_mesh_vert_coords_apply(mesh, vertexCos);
    }

    if (use_orco) {
      BKE_mesh_orco_ensure(ob, mesh);
    }
  }
  else if (ELEM(ob->type, OB_FONT, OB_CURVES_LEGACY, OB_SURF)) {
    /* TODO(sybren): get evaluated mesh from depsgraph once
     * that's properly generated for curves. */
    mesh = BKE_mesh_new_nomain_from_curve(ob);

    /* Currently, that may not be the case every time
     * (texts e.g. tend to give issues,
     * also when deforming curve points instead of generated curve geometry... ). */
    if (mesh != nullptr && mesh->totvert != verts_num) {
      BKE_id_free(nullptr, mesh);
      mesh = nullptr;
    }
  }

  if (mesh && mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_MDATA) {
    BLI_assert(mesh->totvert == verts_num);
  }

  return mesh;
}

void MOD_get_vgroup(
    Object *ob, struct Mesh *mesh, const char *name, const MDeformVert **dvert, int *defgrp_index)
{
  if (mesh) {
    *defgrp_index = BKE_id_defgroup_name_index(&mesh->id, name);
    if (*defgrp_index != -1) {
      *dvert = BKE_mesh_deform_verts(mesh);
    }
    else {
      *dvert = nullptr;
    }
  }
  else {
    *defgrp_index = BKE_object_defgroup_name_index(ob, name);
    if (*defgrp_index != -1 && ob->type == OB_LATTICE) {
      *dvert = BKE_lattice_deform_verts_get(ob);
    }
    else {
      *dvert = nullptr;
    }
  }
}

void MOD_depsgraph_update_object_bone_relation(struct DepsNodeHandle *node,
                                               Object *object,
                                               const char *bonename,
                                               const char *description)
{
  if (object == nullptr) {
    return;
  }
  if (bonename[0] != '\0' && object->type == OB_ARMATURE) {
    DEG_add_object_relation(node, object, DEG_OB_COMP_EVAL_POSE, description);
  }
  else {
    DEG_add_object_relation(node, object, DEG_OB_COMP_TRANSFORM, description);
  }
}

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
  INIT_TYPE(Mask);
  INIT_TYPE(SimpleDeform);
  INIT_TYPE(Multires);
  INIT_TYPE(Surface);
  INIT_TYPE(Fluid);
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
  INIT_TYPE(Weld);
  INIT_TYPE(DataTransfer);
  INIT_TYPE(NormalEdit);
  INIT_TYPE(CorrectiveSmooth);
  INIT_TYPE(MeshSequenceCache);
  INIT_TYPE(SurfaceDeform);
  INIT_TYPE(WeightedNormal);
  INIT_TYPE(MeshToVolume);
  INIT_TYPE(VolumeDisplace);
  INIT_TYPE(VolumeToMesh);
  INIT_TYPE(Nodes);
#undef INIT_TYPE
}
