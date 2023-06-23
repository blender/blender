/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <vector>

#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_volume.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_volume_types.h"

#include "DEG_depsgraph.h"

#include "GEO_mesh_to_volume.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLO_read_write.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "BLI_index_range.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_span.hh"

#include "RNA_access.h"
#include "RNA_prototypes.h"

static void initData(ModifierData *md)
{
  MeshToVolumeModifierData *mvmd = reinterpret_cast<MeshToVolumeModifierData *>(md);
  mvmd->object = nullptr;
  mvmd->resolution_mode = MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT;
  mvmd->voxel_size = 0.1f;
  mvmd->voxel_amount = 32;
  mvmd->fill_volume = true;
  mvmd->interior_band_width = 0.1f;
  mvmd->exterior_band_width = 0.1f;
  mvmd->density = 1.0f;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MeshToVolumeModifierData *mvmd = reinterpret_cast<MeshToVolumeModifierData *>(md);
  DEG_add_depends_on_transform_relation(ctx->node, "Mesh to Volume Modifier");
  if (mvmd->object) {
    DEG_add_object_relation(
        ctx->node, mvmd->object, DEG_OB_COMP_GEOMETRY, "Mesh to Volume Modifier");
    DEG_add_object_relation(
        ctx->node, mvmd->object, DEG_OB_COMP_TRANSFORM, "Mesh to Volume Modifier");
  }
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  MeshToVolumeModifierData *mvmd = reinterpret_cast<MeshToVolumeModifierData *>(md);
  walk(userData, ob, (ID **)&mvmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  MeshToVolumeModifierData *mvmd = static_cast<MeshToVolumeModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "object", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "density", 0, nullptr, ICON_NONE);

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "use_fill_volume", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "exterior_band_width", 0, nullptr, ICON_NONE);

    uiLayout *subcol = uiLayoutColumn(col, false);
    uiLayoutSetActive(subcol, !mvmd->fill_volume);
    uiItemR(subcol, ptr, "interior_band_width", 0, nullptr, ICON_NONE);
  }
  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "resolution_mode", 0, nullptr, ICON_NONE);
    if (mvmd->resolution_mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT) {
      uiItemR(col, ptr, "voxel_amount", 0, nullptr, ICON_NONE);
    }
    else {
      uiItemR(col, ptr, "voxel_size", 0, nullptr, ICON_NONE);
    }
  }

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_MeshToVolume, panel_draw);
}

static Volume *mesh_to_volume(ModifierData *md,
                              const ModifierEvalContext *ctx,
                              Volume *input_volume)
{
#ifdef WITH_OPENVDB
  using namespace blender;

  MeshToVolumeModifierData *mvmd = reinterpret_cast<MeshToVolumeModifierData *>(md);
  Object *object_to_convert = mvmd->object;

  if (object_to_convert == nullptr) {
    return input_volume;
  }
  Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(object_to_convert);
  if (mesh == nullptr) {
    return input_volume;
  }
  BKE_mesh_wrapper_ensure_mdata(mesh);

  const float4x4 mesh_to_own_object_space_transform = float4x4(ctx->object->world_to_object) *
                                                      float4x4(object_to_convert->object_to_world);
  geometry::MeshToVolumeResolution resolution;
  resolution.mode = (MeshToVolumeModifierResolutionMode)mvmd->resolution_mode;
  if (resolution.mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT) {
    resolution.settings.voxel_amount = mvmd->voxel_amount;
    if (resolution.settings.voxel_amount <= 0.0f) {
      return input_volume;
    }
  }
  else if (resolution.mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE) {
    resolution.settings.voxel_size = mvmd->voxel_size;
    if (resolution.settings.voxel_size <= 0.0f) {
      return input_volume;
    }
  }

  auto bounds_fn = [&](float3 &r_min, float3 &r_max) {
    const BoundBox *bb = BKE_object_boundbox_get(mvmd->object);
    r_min = bb->vec[0];
    r_max = bb->vec[6];
  };

  const float voxel_size = geometry::volume_compute_voxel_size(ctx->depsgraph,
                                                               bounds_fn,
                                                               resolution,
                                                               mvmd->exterior_band_width,
                                                               mesh_to_own_object_space_transform);

  /* Create a new volume. */
  Volume *volume;
  if (input_volume == nullptr) {
    volume = static_cast<Volume *>(BKE_id_new_nomain(ID_VO, "Volume"));
  }
  else {
    volume = BKE_volume_new_for_eval(input_volume);
  }

  /* Convert mesh to grid and add to volume. */
  geometry::fog_volume_grid_add_from_mesh(volume,
                                          "density",
                                          mesh,
                                          mesh_to_own_object_space_transform,
                                          voxel_size,
                                          mvmd->fill_volume,
                                          mvmd->exterior_band_width,
                                          mvmd->interior_band_width,
                                          mvmd->density);

  return volume;

#else
  UNUSED_VARS(md);
  BKE_modifier_set_error(ctx->object, md, "Compiled without OpenVDB");
  return input_volume;
#endif
}

static void modifyGeometrySet(ModifierData *md,
                              const ModifierEvalContext *ctx,
                              blender::bke::GeometrySet *geometry_set)
{
  Volume *input_volume = geometry_set->get_volume_for_write();
  Volume *result_volume = mesh_to_volume(md, ctx, input_volume);
  if (result_volume != input_volume) {
    geometry_set->replace_volume(result_volume);
  }
}

ModifierTypeInfo modifierType_MeshToVolume = {
    /*name*/ N_("Mesh to Volume"),
    /*structName*/ "MeshToVolumeModifierData",
    /*structSize*/ sizeof(MeshToVolumeModifierData),
    /*srna*/ &RNA_MeshToVolumeModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/ static_cast<ModifierTypeFlag>(0),
    /*icon*/ ICON_VOLUME_DATA, /* TODO: Use correct icon. */

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ modifyGeometrySet,

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
