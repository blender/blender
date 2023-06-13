/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <vector>

#include "BKE_lib_query.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_volume.h"
#include "BKE_volume_to_mesh.hh"

#include "BLT_translation.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_volume_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.h"
#include "BLI_span.hh"
#include "BLI_timeit.hh"

#include "DEG_depsgraph_query.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/VolumeToMesh.h>
#endif

using blender::float3;
using blender::float4x4;
using blender::Span;

static void initData(ModifierData *md)
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

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
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

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  walk(userData, ob, (ID **)&vmmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  VolumeToMeshModifierData *vmmd = static_cast<VolumeToMeshModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "object", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "grid_name", 0, nullptr, ICON_NONE);
  }

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "resolution_mode", 0, nullptr, ICON_NONE);
    if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT) {
      uiItemR(col, ptr, "voxel_amount", 0, nullptr, ICON_NONE);
    }
    else if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
      uiItemR(col, ptr, "voxel_size", 0, nullptr, ICON_NONE);
    }
  }

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "threshold", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "adaptivity", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "use_smooth_shade", 0, nullptr, ICON_NONE);
  }

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_VolumeToMesh, panel_draw);
}

static Mesh *create_empty_mesh(const Mesh *input_mesh)
{
  Mesh *new_mesh = BKE_mesh_new_nomain(0, 0, 0, 0);
  BKE_mesh_copy_parameters_for_eval(new_mesh, input_mesh);
  return new_mesh;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *input_mesh)
{
#ifdef WITH_OPENVDB
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  if (vmmd->object == nullptr) {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->object->type != OB_VOLUME) {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE &&
      vmmd->voxel_size == 0.0f) {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT &&
      vmmd->voxel_amount == 0)
  {
    return create_empty_mesh(input_mesh);
  }

  const Volume *volume = static_cast<Volume *>(vmmd->object->data);

  BKE_volume_load(volume, DEG_get_bmain(ctx->depsgraph));
  const VolumeGrid *volume_grid = BKE_volume_grid_find_for_read(volume, vmmd->grid_name);
  if (volume_grid == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Cannot find '%s' grid", vmmd->grid_name);
    return create_empty_mesh(input_mesh);
  }

  const openvdb::GridBase::ConstPtr local_grid = BKE_volume_grid_openvdb_for_read(volume,
                                                                                  volume_grid);

  openvdb::math::Transform::Ptr transform = local_grid->transform().copy();
  transform->postMult(openvdb::Mat4d((float *)vmmd->object->object_to_world));
  openvdb::Mat4d imat = openvdb::Mat4d((float *)ctx->object->world_to_object);
  /* `imat` had floating point issues and wasn't affine. */
  imat.setCol(3, openvdb::Vec4d(0, 0, 0, 1));
  transform->postMult(imat);

  /* Create a temporary transformed grid. The underlying tree is shared. */
  openvdb::GridBase::ConstPtr transformed_grid = local_grid->copyGridReplacingTransform(transform);

  blender::bke::VolumeToMeshResolution resolution;
  resolution.mode = (VolumeToMeshResolutionMode)vmmd->resolution_mode;
  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT) {
    resolution.settings.voxel_amount = vmmd->voxel_amount;
  }
  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
    resolution.settings.voxel_size = vmmd->voxel_size;
  }

  Mesh *mesh = blender::bke::volume_to_mesh(
      *transformed_grid, resolution, vmmd->threshold, vmmd->adaptivity);
  if (mesh == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Could not generate mesh from grid");
    return create_empty_mesh(input_mesh);
  }

  BKE_mesh_copy_parameters_for_eval(mesh, input_mesh);
  BKE_mesh_smooth_flag_set(mesh, vmmd->flag & VOLUME_TO_MESH_USE_SMOOTH_SHADE);
  return mesh;
#else
  UNUSED_VARS(md);
  BKE_modifier_set_error(ctx->object, md, "Compiled without OpenVDB");
  return create_empty_mesh(input_mesh);
#endif
}

ModifierTypeInfo modifierType_VolumeToMesh = {
    /*name*/ N_("Volume to Mesh"),
    /*structName*/ "VolumeToMeshModifierData",
    /*structSize*/ sizeof(VolumeToMeshModifierData),
    /*srna*/ &RNA_VolumeToMeshModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh,
    /*icon*/ ICON_VOLUME_DATA, /* TODO: Use correct icon. */

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ modifyMesh,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ nullptr,
    /*freeData*/ nullptr,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
