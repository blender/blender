/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <vector>

#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_to_mesh.hh"

#include "BLT_translation.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_volume_types.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.h"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_timeit.hh"

#include "DEG_depsgraph_query.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/VolumeToMesh.h>
#endif

using blender::float3;
using blender::float4x4;
using blender::Span;

static void init_data(ModifierData *md)
{
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  vmmd->object = nullptr;
  vmmd->threshold = 0.1f;
  STRNCPY(vmmd->grid_name, "density");
  vmmd->adaptivity = 0.0f;
  vmmd->resolution_mode = VOLUME_TO_MESH_RESOLUTION_MODE_GRID;
  vmmd->voxel_amount = 32;
  vmmd->voxel_size = 0.1f;
  vmmd->flag = 0;
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  DEG_add_depends_on_transform_relation(ctx->node, "Volume to Mesh Modifier");
  if (vmmd->object) {
    DEG_add_object_relation(
        ctx->node, vmmd->object, DEG_OB_COMP_GEOMETRY, "Volume to Mesh Modifier");
    DEG_add_object_relation(
        ctx->node, vmmd->object, DEG_OB_COMP_TRANSFORM, "Volume to Mesh Modifier");
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  walk(user_data, ob, (ID **)&vmmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  VolumeToMeshModifierData *vmmd = static_cast<VolumeToMeshModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "grid_name", UI_ITEM_NONE, nullptr, ICON_NONE);
  }

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "resolution_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
    if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT) {
      uiItemR(col, ptr, "voxel_amount", UI_ITEM_NONE, nullptr, ICON_NONE);
    }
    else if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
      uiItemR(col, ptr, "voxel_size", UI_ITEM_NONE, nullptr, ICON_NONE);
    }
  }

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "threshold", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "adaptivity", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "use_smooth_shade", UI_ITEM_NONE, nullptr, ICON_NONE);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_VolumeToMesh, panel_draw);
}

static Mesh *create_empty_mesh(const Mesh *input_mesh)
{
  Mesh *new_mesh = BKE_mesh_new_nomain(0, 0, 0, 0);
  BKE_mesh_copy_parameters_for_eval(new_mesh, input_mesh);
  return new_mesh;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *input_mesh)
{
  using namespace blender;
#ifdef WITH_OPENVDB
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  if (vmmd->object == nullptr) {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->object->type != OB_VOLUME) {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE &&
      vmmd->voxel_size == 0.0f)
  {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT &&
      vmmd->voxel_amount == 0)
  {
    return create_empty_mesh(input_mesh);
  }

  const Volume *volume = static_cast<Volume *>(vmmd->object->data);

  BKE_volume_load(volume, DEG_get_bmain(ctx->depsgraph));
  const blender::bke::VolumeGridData *volume_grid = BKE_volume_grid_find(volume, vmmd->grid_name);
  if (volume_grid == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Cannot find '%s' grid", vmmd->grid_name);
    return create_empty_mesh(input_mesh);
  }

  blender::bke::VolumeTreeAccessToken tree_token;
  const openvdb::GridBase &local_grid = volume_grid->grid(tree_token);

  openvdb::math::Transform::Ptr transform = local_grid.transform().copy();
  transform->postMult(openvdb::Mat4d((float *)vmmd->object->object_to_world));
  openvdb::Mat4d imat = openvdb::Mat4d((float *)ctx->object->world_to_object);
  /* `imat` had floating point issues and wasn't affine. */
  imat.setCol(3, openvdb::Vec4d(0, 0, 0, 1));
  transform->postMult(imat);

  /* Create a temporary transformed grid. The underlying tree is shared. */
  openvdb::GridBase::ConstPtr transformed_grid = local_grid.copyGridReplacingTransform(transform);

  bke::VolumeToMeshResolution resolution;
  resolution.mode = (VolumeToMeshResolutionMode)vmmd->resolution_mode;
  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT) {
    resolution.settings.voxel_amount = vmmd->voxel_amount;
  }
  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
    resolution.settings.voxel_size = vmmd->voxel_size;
  }

  Mesh *mesh = bke::volume_to_mesh(
      *transformed_grid, resolution, vmmd->threshold, vmmd->adaptivity);
  if (mesh == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Could not generate mesh from grid");
    return create_empty_mesh(input_mesh);
  }

  BKE_mesh_copy_parameters_for_eval(mesh, input_mesh);
  bke::mesh_smooth_set(*mesh, vmmd->flag & VOLUME_TO_MESH_USE_SMOOTH_SHADE);
  return mesh;
#else
  UNUSED_VARS(md);
  BKE_modifier_set_error(ctx->object, md, "Compiled without OpenVDB");
  return create_empty_mesh(input_mesh);
#endif
}

ModifierTypeInfo modifierType_VolumeToMesh = {
    /*idname*/ "Volume to Mesh",
    /*name*/ N_("Volume to Mesh"),
    /*struct_name*/ "VolumeToMeshModifierData",
    /*struct_size*/ sizeof(VolumeToMeshModifierData),
    /*srna*/ &RNA_VolumeToMeshModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh,
    /*icon*/ ICON_VOLUME_DATA, /* TODO: Use correct icon. */

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
};
