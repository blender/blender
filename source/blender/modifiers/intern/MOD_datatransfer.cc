/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cfloat>

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_customdata.hh"
#include "BKE_data_transfer.h"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_remap.hh"
#include "BKE_modifier.hh"
#include "BKE_report.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"

/**************************************
 * Modifiers functions.               *
 **************************************/
static void init_data(ModifierData *md)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  int i;

  dtmd->ob_source = nullptr;
  dtmd->data_types = 0;

  dtmd->vmap_mode = MREMAP_MODE_VERT_NEAREST;
  dtmd->emap_mode = MREMAP_MODE_EDGE_NEAREST;
  dtmd->lmap_mode = MREMAP_MODE_LOOP_NEAREST_POLYNOR;
  dtmd->pmap_mode = MREMAP_MODE_POLY_NEAREST;

  dtmd->map_max_distance = 1.0f;
  dtmd->map_ray_radius = 0.0f;

  for (i = 0; i < DT_MULTILAYER_INDEX_MAX; i++) {
    dtmd->layers_select_src[i] = DT_LAYERS_ALL_SRC;
    dtmd->layers_select_dst[i] = DT_LAYERS_NAME_DST;
  }

  dtmd->mix_mode = CDT_MIX_TRANSFER;
  dtmd->mix_factor = 1.0f;
  dtmd->defgrp_name[0] = '\0';

  dtmd->flags = MOD_DATATRANSFER_OBSRC_TRANSFORM;
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;

  if (dtmd->defgrp_name[0] != '\0') {
    /* We need vertex groups! */
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  BKE_object_data_transfer_dttypes_to_cdmask(dtmd->data_types, r_cddata_masks);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  walk(user_data, ob, (ID **)&dtmd->ob_source, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  if (dtmd->ob_source != nullptr) {
    CustomData_MeshMasks cddata_masks = {0};
    BKE_object_data_transfer_dttypes_to_cdmask(dtmd->data_types, &cddata_masks);

    DEG_add_object_relation(
        ctx->node, dtmd->ob_source, DEG_OB_COMP_GEOMETRY, "DataTransfer Modifier");
    DEG_add_customdata_mask(ctx->node, dtmd->ob_source, &cddata_masks);

    if (dtmd->flags & MOD_DATATRANSFER_OBSRC_TRANSFORM) {
      DEG_add_object_relation(
          ctx->node, dtmd->ob_source, DEG_OB_COMP_TRANSFORM, "DataTransfer Modifier");
      DEG_add_depends_on_transform_relation(ctx->node, "DataTransfer Modifier");
    }
  }
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  /* If no source object, bypass. */
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !dtmd->ob_source || dtmd->ob_source->type != OB_MESH;
}

#define DT_TYPES_AFFECT_MESH \
  (DT_TYPE_BWEIGHT_VERT | DT_TYPE_BWEIGHT_EDGE | DT_TYPE_CREASE | DT_TYPE_SHARP_EDGE | \
   DT_TYPE_LNOR | DT_TYPE_SHARP_FACE)

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *me_mod)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  Mesh *result = me_mod;
  ReportList reports;

  /* Only used to check whether we are operating on org data or not... */
  const Mesh *mesh = static_cast<const Mesh *>(ctx->object->data);

  Object *ob_source = dtmd->ob_source;

  const bool invert_vgroup = (dtmd->flags & MOD_DATATRANSFER_INVERT_VGROUP) != 0;

  const float max_dist = (dtmd->flags & MOD_DATATRANSFER_MAP_MAXDIST) ? dtmd->map_max_distance :
                                                                        FLT_MAX;

  SpaceTransform space_transform_data;
  SpaceTransform *space_transform = (dtmd->flags & MOD_DATATRANSFER_OBSRC_TRANSFORM) ?
                                        &space_transform_data :
                                        nullptr;

  if (space_transform) {
    BLI_SPACE_TRANSFORM_SETUP(space_transform, ctx->object, ob_source);
  }

  const blender::Span<blender::float3> me_positions = mesh->vert_positions();
  const blender::Span<blender::int2> me_edges = mesh->edges();
  const blender::Span<blender::float3> result_positions = result->vert_positions();

  const blender::Span<blender::int2> result_edges = result->edges();

  if (((result == mesh) || (me_positions.data() == result_positions.data()) ||
       (me_edges.data() == result_edges.data())) &&
      (dtmd->data_types & DT_TYPES_AFFECT_MESH))
  {
    /* We need to duplicate data here, otherwise setting custom normals, edges' sharpness, etc.,
     * could modify org mesh, see #43671. */
    result = (Mesh *)BKE_id_copy_ex(nullptr, &me_mod->id, nullptr, LIB_ID_COPY_LOCALIZE);
  }

  BKE_reports_init(&reports, RPT_STORE);

  /* NOTE: no islands precision for now here. */
  if (BKE_object_data_transfer_ex(ctx->depsgraph,
                                  ob_source,
                                  ctx->object,
                                  result,
                                  dtmd->data_types,
                                  false,
                                  dtmd->vmap_mode,
                                  dtmd->emap_mode,
                                  dtmd->lmap_mode,
                                  dtmd->pmap_mode,
                                  space_transform,
                                  false,
                                  max_dist,
                                  dtmd->map_ray_radius,
                                  0.0f,
                                  dtmd->layers_select_src,
                                  dtmd->layers_select_dst,
                                  dtmd->mix_mode,
                                  dtmd->mix_factor,
                                  dtmd->defgrp_name,
                                  invert_vgroup,
                                  &reports))
  {
    result->runtime->is_original_bmesh = false;
  }

  if (BKE_reports_contain(&reports, RPT_ERROR)) {
    const char *report_str = BKE_reports_string(&reports, RPT_ERROR);
    BKE_modifier_set_error(ctx->object, md, "%s", report_str);
    MEM_freeN(report_str);
  }

  BKE_reports_free(&reports);

  return result;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  row = &layout->row(true);
  row->prop(ptr, "object", UI_ITEM_NONE, IFACE_("Source"), ICON_NONE);
  sub = &row->row(true);
  sub->use_property_decorate_set(false);
  sub->prop(ptr, "use_object_transform", UI_ITEM_NONE, "", ICON_ORIENTATION_GLOBAL);

  layout->prop(ptr, "mix_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(false);
  row->active_set(!ELEM(RNA_enum_get(ptr, "mix_mode"),
                        CDT_MIX_NOMIX,
                        CDT_MIX_REPLACE_ABOVE_THRESHOLD,
                        CDT_MIX_REPLACE_BELOW_THRESHOLD));
  row->prop(ptr, "mix_factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  layout->op("OBJECT_OT_datalayout_transfer", IFACE_("Generate Data Layers"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void vertex_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  uiLayout *layout = panel->layout;

  layout->prop(ptr, "use_vert_data", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void vertex_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool use_vert_data = RNA_boolean_get(ptr, "use_vert_data");
  layout->active_set(use_vert_data);

  layout->prop(ptr, "data_types_verts", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  layout->prop(ptr, "vert_mapping", UI_ITEM_NONE, IFACE_("Mapping"), ICON_NONE);
}

static void vertex_vgroup_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->active_set(RNA_enum_get(ptr, "data_types_verts") & DT_TYPE_MDEFORMVERT);

  layout->use_property_split_set(true);

  layout->prop(
      ptr, "layers_vgroup_select_src", UI_ITEM_NONE, IFACE_("Layer Selection"), ICON_NONE);
  layout->prop(ptr, "layers_vgroup_select_dst", UI_ITEM_NONE, IFACE_("Layer Mapping"), ICON_NONE);
}

static void edge_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "use_edge_data", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void edge_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->active_set(RNA_boolean_get(ptr, "use_edge_data"));

  layout->prop(ptr, "data_types_edges", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  layout->prop(ptr, "edge_mapping", UI_ITEM_NONE, IFACE_("Mapping"), ICON_NONE);
}

static void face_corner_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "use_loop_data", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void face_corner_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->active_set(RNA_boolean_get(ptr, "use_loop_data"));

  layout->prop(ptr, "data_types_loops", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  layout->prop(ptr, "loop_mapping", UI_ITEM_NONE, IFACE_("Mapping"), ICON_NONE);
}

static void vert_vcol_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->active_set(RNA_enum_get(ptr, "data_types_verts") &
                     (DT_TYPE_MPROPCOL_VERT | DT_TYPE_MLOOPCOL_VERT));

  layout->prop(
      ptr, "layers_vcol_vert_select_src", UI_ITEM_NONE, IFACE_("Layer Selection"), ICON_NONE);
  layout->prop(
      ptr, "layers_vcol_vert_select_dst", UI_ITEM_NONE, IFACE_("Layer Mapping"), ICON_NONE);
}

static void face_corner_vcol_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->active_set(RNA_enum_get(ptr, "data_types_loops") &
                     (DT_TYPE_MPROPCOL_LOOP | DT_TYPE_MLOOPCOL_LOOP));

  layout->prop(
      ptr, "layers_vcol_loop_select_src", UI_ITEM_NONE, IFACE_("Layer Selection"), ICON_NONE);
  layout->prop(
      ptr, "layers_vcol_loop_select_dst", UI_ITEM_NONE, IFACE_("Layer Mapping"), ICON_NONE);
}

static void face_corner_uv_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->active_set(RNA_enum_get(ptr, "data_types_loops") & DT_TYPE_UV);

  layout->prop(ptr, "layers_uv_select_src", UI_ITEM_NONE, IFACE_("Layer Selection"), ICON_NONE);
  layout->prop(ptr, "layers_uv_select_dst", UI_ITEM_NONE, IFACE_("Layer Mapping"), ICON_NONE);
  layout->prop(ptr, "islands_precision", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void face_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "use_poly_data", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void face_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->active_set(RNA_boolean_get(ptr, "use_poly_data"));

  layout->prop(ptr, "data_types_polys", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  layout->prop(ptr, "poly_mapping", UI_ITEM_NONE, IFACE_("Mapping"), ICON_NONE);
}

static void advanced_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  row = &layout->row(true, IFACE_("Max Distance"));
  row->prop(ptr, "use_max_distance", UI_ITEM_NONE, "", ICON_NONE);
  sub = &row->row(true);
  sub->active_set(RNA_boolean_get(ptr, "use_max_distance"));
  sub->prop(ptr, "max_distance", UI_ITEM_NONE, "", ICON_NONE);

  layout->prop(ptr, "ray_radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_DataTransfer, panel_draw);
  PanelType *vertex_panel = modifier_subpanel_register(
      region_type, "vertex", "", vertex_panel_draw_header, vertex_panel_draw, panel_type);
  modifier_subpanel_register(region_type,
                             "vertex_vgroup",
                             "Vertex Groups",
                             nullptr,
                             vertex_vgroup_panel_draw,
                             vertex_panel);

  modifier_subpanel_register(
      region_type, "vert_vcol", "Colors", nullptr, vert_vcol_panel_draw, vertex_panel);

  modifier_subpanel_register(
      region_type, "edge", "", edge_panel_draw_header, edge_panel_draw, panel_type);

  PanelType *face_corner_panel = modifier_subpanel_register(region_type,
                                                            "face_corner",
                                                            "",
                                                            face_corner_panel_draw_header,
                                                            face_corner_panel_draw,
                                                            panel_type);
  modifier_subpanel_register(region_type,
                             "face_corner_vcol",
                             "Colors",
                             nullptr,
                             face_corner_vcol_panel_draw,
                             face_corner_panel);
  modifier_subpanel_register(
      region_type, "face_corner_uv", "UVs", nullptr, face_corner_uv_panel_draw, face_corner_panel);

  modifier_subpanel_register(
      region_type, "face", "", face_panel_draw_header, face_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "advanced", "Topology Mapping", nullptr, advanced_panel_draw, panel_type);
}

#undef DT_TYPES_AFFECT_MESH

ModifierTypeInfo modifierType_DataTransfer = {
    /*idname*/ "DataTransfer",
    /*name*/ N_("DataTransfer"),
    /*struct_name*/ "DataTransferModifierData",
    /*struct_size*/ sizeof(DataTransferModifierData),
    /*srna*/ &RNA_DataTransferModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_DATA_TRANSFER,

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
    /*is_disabled*/ is_disabled,
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
    /*foreach_working_space_color*/ nullptr,
};
