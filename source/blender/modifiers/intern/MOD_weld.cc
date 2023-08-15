/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 *
 * Weld modifier: Remove doubles.
 */

/* TODOs:
 * - Review weight and vertex color interpolation.;
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "DEG_depsgraph.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "GEO_mesh_merge_by_distance.hh"

using blender::Array;
using blender::IndexMask;
using blender::IndexMaskMemory;
using blender::Span;
using blender::Vector;

static Span<MDeformVert> get_vertex_group(const Mesh &mesh, const int defgrp_index)
{
  if (defgrp_index == -1) {
    return {};
  }
  const MDeformVert *vertex_group = static_cast<const MDeformVert *>(
      CustomData_get_layer(&mesh.vert_data, CD_MDEFORMVERT));
  if (!vertex_group) {
    return {};
  }
  return {vertex_group, mesh.totvert};
}

static IndexMask selected_indices_from_vertex_group(Span<MDeformVert> vertex_group,
                                                    const int index,
                                                    const bool invert,
                                                    IndexMaskMemory &memory)
{
  return IndexMask::from_predicate(
      vertex_group.index_range(), blender::GrainSize(512), memory, [&](const int i) {
        return (BKE_defvert_find_weight(&vertex_group[i], index) > 0.0f) != invert;
      });
}

static Array<bool> selection_array_from_vertex_group(Span<MDeformVert> vertex_group,
                                                     const int index,
                                                     const bool invert)
{
  Array<bool> selection(vertex_group.size());
  for (const int i : vertex_group.index_range()) {
    const bool found = BKE_defvert_find_weight(&vertex_group[i], index) > 0.0f;
    selection[i] = (found != invert);
  }
  return selection;
}

static std::optional<Mesh *> calculate_weld(const Mesh &mesh, const WeldModifierData &wmd)
{
  const int defgrp_index = BKE_id_defgroup_name_index(&mesh.id, wmd.defgrp_name);
  Span<MDeformVert> vertex_group = get_vertex_group(mesh, defgrp_index);
  const bool invert = (wmd.flag & MOD_WELD_INVERT_VGROUP) != 0;

  if (wmd.mode == MOD_WELD_MODE_ALL) {
    if (!vertex_group.is_empty()) {
      IndexMaskMemory memory;
      const IndexMask selected_indices = selected_indices_from_vertex_group(
          vertex_group, defgrp_index, invert, memory);
      return blender::geometry::mesh_merge_by_distance_all(
          mesh, IndexMask(selected_indices), wmd.merge_dist);
    }
    return blender::geometry::mesh_merge_by_distance_all(
        mesh, IndexMask(mesh.totvert), wmd.merge_dist);
  }
  if (wmd.mode == MOD_WELD_MODE_CONNECTED) {
    const bool only_loose_edges = (wmd.flag & MOD_WELD_LOOSE_EDGES) != 0;
    if (!vertex_group.is_empty()) {
      Array<bool> selection = selection_array_from_vertex_group(
          vertex_group, defgrp_index, invert);
      return blender::geometry::mesh_merge_by_distance_connected(
          mesh, selection, wmd.merge_dist, only_loose_edges);
    }
    Array<bool> selection(mesh.totvert, true);
    return blender::geometry::mesh_merge_by_distance_connected(
        mesh, selection, wmd.merge_dist, only_loose_edges);
  }

  BLI_assert_unreachable();
  return nullptr;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext * /*ctx*/, Mesh *mesh)
{
  const WeldModifierData &wmd = reinterpret_cast<WeldModifierData &>(*md);

  std::optional<Mesh *> result = calculate_weld(*mesh, wmd);
  if (!result) {
    return mesh;
  }
  return *result;
}

static void init_data(ModifierData *md)
{
  WeldModifierData *wmd = (WeldModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wmd, modifier));

  MEMCPY_STRUCT_AFTER(wmd, DNA_struct_default_get(WeldModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  WeldModifierData *wmd = (WeldModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (wmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  int weld_mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "merge_threshold", UI_ITEM_NONE, IFACE_("Distance"), ICON_NONE);
  if (weld_mode == MOD_WELD_MODE_CONNECTED) {
    uiItemR(layout, ptr, "loose_edges", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Weld, panel_draw);
}

ModifierTypeInfo modifierType_Weld = {
    /*idname*/ "Weld",
    /*name*/ N_("Weld"),
    /*struct_name*/ "WeldModifierData",
    /*struct_size*/ sizeof(WeldModifierData),
    /*srna*/ &RNA_WeldModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/
    (ModifierTypeFlag)(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
                       eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
                       eModifierTypeFlag_AcceptsCVs),
    /*icon*/ ICON_AUTOMERGE_OFF, /* TODO: Use correct icon. */

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};
