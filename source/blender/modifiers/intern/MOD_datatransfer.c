/*
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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_data_transfer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_modifier.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

/**************************************
 * Modifiers functions.               *
 **************************************/
static void initData(ModifierData *md)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  int i;

  dtmd->ob_source = NULL;
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

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;

  if (dtmd->defgrp_name[0] != '\0') {
    /* We need vertex groups! */
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  BKE_object_data_transfer_dttypes_to_cdmask(dtmd->data_types, r_cddata_masks);
}

static bool dependsOnNormals(ModifierData *md)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  int item_types = BKE_object_data_transfer_get_dttypes_item_types(dtmd->data_types);

  if ((item_types & ME_VERT) && (dtmd->vmap_mode & (MREMAP_USE_NORPROJ | MREMAP_USE_NORMAL))) {
    return true;
  }
  if ((item_types & ME_EDGE) && (dtmd->emap_mode & (MREMAP_USE_NORPROJ | MREMAP_USE_NORMAL))) {
    return true;
  }
  if ((item_types & ME_LOOP) && (dtmd->lmap_mode & (MREMAP_USE_NORPROJ | MREMAP_USE_NORMAL))) {
    return true;
  }
  if ((item_types & ME_POLY) && (dtmd->pmap_mode & (MREMAP_USE_NORPROJ | MREMAP_USE_NORMAL))) {
    return true;
  }

  return false;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  walk(userData, ob, &dtmd->ob_source, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  if (dtmd->ob_source != NULL) {
    CustomData_MeshMasks cddata_masks = {0};
    BKE_object_data_transfer_dttypes_to_cdmask(dtmd->data_types, &cddata_masks);
    BKE_mesh_remap_calc_source_cddata_masks_from_map_modes(
        dtmd->vmap_mode, dtmd->emap_mode, dtmd->lmap_mode, dtmd->pmap_mode, &cddata_masks);

    DEG_add_object_relation(
        ctx->node, dtmd->ob_source, DEG_OB_COMP_GEOMETRY, "DataTransfer Modifier");
    DEG_add_customdata_mask(ctx->node, dtmd->ob_source, &cddata_masks);

    if (dtmd->flags & MOD_DATATRANSFER_OBSRC_TRANSFORM) {
      DEG_add_object_relation(
          ctx->node, dtmd->ob_source, DEG_OB_COMP_TRANSFORM, "DataTransfer Modifier");
      DEG_add_modifier_to_transform_relation(ctx->node, "DataTransfer Modifier");
    }
  }
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
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

#define HIGH_POLY_WARNING 10000
#define DT_TYPES_AFFECT_MESH \
  (DT_TYPE_BWEIGHT_VERT | DT_TYPE_BWEIGHT_EDGE | DT_TYPE_CREASE | DT_TYPE_SHARP_EDGE | \
   DT_TYPE_LNOR | DT_TYPE_SHARP_FACE)

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *me_mod)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Mesh *result = me_mod;
  ReportList reports;

  /* Only used to check whether we are operating on org data or not... */
  Mesh *me = ctx->object->data;

  Object *ob_source = dtmd->ob_source;

  const bool invert_vgroup = (dtmd->flags & MOD_DATATRANSFER_INVERT_VGROUP) != 0;

  const float max_dist = (dtmd->flags & MOD_DATATRANSFER_MAP_MAXDIST) ? dtmd->map_max_distance :
                                                                        FLT_MAX;

  SpaceTransform space_transform_data;
  SpaceTransform *space_transform = (dtmd->flags & MOD_DATATRANSFER_OBSRC_TRANSFORM) ?
                                        &space_transform_data :
                                        NULL;

  if (space_transform) {
    BLI_SPACE_TRANSFORM_SETUP(space_transform, ctx->object, ob_source);
  }

  if (((result == me) || (me->mvert == result->mvert) || (me->medge == result->medge)) &&
      (dtmd->data_types & DT_TYPES_AFFECT_MESH)) {
    /* We need to duplicate data here, otherwise setting custom normals, edges' sharpness, etc.,
     * could modify org mesh, see T43671. */
    BKE_id_copy_ex(NULL, &me_mod->id, (ID **)&result, LIB_ID_COPY_LOCALIZE);
  }

  BKE_reports_init(&reports, RPT_STORE);

  /* Note: no islands precision for now here. */
  BKE_object_data_transfer_ex(ctx->depsgraph,
                              scene,
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
                              &reports);

  if (BKE_reports_contain(&reports, RPT_ERROR)) {
    BKE_modifier_set_error(md, "%s", BKE_reports_string(&reports, RPT_ERROR));
  }
  else if ((dtmd->data_types & DT_TYPE_LNOR) && !(me->flag & ME_AUTOSMOOTH)) {
    BKE_modifier_set_error((ModifierData *)dtmd, "Enable 'Auto Smooth' in Object Data Properties");
  }
  else if (result->totvert > HIGH_POLY_WARNING ||
           ((Mesh *)(ob_source->data))->totvert > HIGH_POLY_WARNING) {
    BKE_modifier_set_error(
        md, "Source or destination object has a high polygon count, computation might be slow");
  }

  return result;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *sub, *row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRow(layout, true);
  uiItemR(row, &ptr, "object", 0, IFACE_("Source"), ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetPropDecorate(sub, false);
  uiItemR(sub, &ptr, "use_object_transform", 0, "", ICON_ORIENTATION_GLOBAL);

  uiItemR(layout, &ptr, "mix_mode", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "mix_factor", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  uiItemO(layout, "Generate Data Layers", ICON_NONE, "OBJECT_OT_datalayout_transfer");

  modifier_panel_end(layout, &ptr);
}

static void vertex_panel_draw_header(const bContext *C, Panel *panel)
{
  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);
  uiLayout *layout = panel->layout;

  uiItemR(layout, &ptr, "use_vert_data", 0, NULL, ICON_NONE);
}

static void vertex_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  bool use_vert_data = RNA_boolean_get(&ptr, "use_vert_data");
  uiLayoutSetActive(layout, use_vert_data);

  uiItemR(layout, &ptr, "data_types_verts", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "vert_mapping", 0, IFACE_("Mapping"), ICON_NONE);
}

static void vertex_vgroup_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetActive(layout, RNA_enum_get(&ptr, "data_types_verts") & DT_TYPE_MDEFORMVERT);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "layers_vgroup_select_src", 0, IFACE_("Layer Selection"), ICON_NONE);
  uiItemR(layout, &ptr, "layers_vgroup_select_dst", 0, IFACE_("Layer Mapping"), ICON_NONE);
}

static void edge_panel_draw_header(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_edge_data", 0, NULL, ICON_NONE);
}

static void edge_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetActive(layout, RNA_boolean_get(&ptr, "use_edge_data"));

  uiItemR(layout, &ptr, "data_types_edges", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "edge_mapping", 0, IFACE_("Mapping"), ICON_NONE);
}

static void face_corner_panel_draw_header(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_loop_data", 0, NULL, ICON_NONE);
}

static void face_corner_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetActive(layout, RNA_boolean_get(&ptr, "use_loop_data"));

  uiItemR(layout, &ptr, "data_types_loops", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "loop_mapping", 0, IFACE_("Mapping"), ICON_NONE);
}

static void face_corner_vcol_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, RNA_enum_get(&ptr, "data_types_loops") & DT_TYPE_VCOL);

  uiItemR(layout, &ptr, "layers_vcol_select_src", 0, IFACE_("Layer Selection"), ICON_NONE);
  uiItemR(layout, &ptr, "layers_vcol_select_dst", 0, IFACE_("Layer Mapping"), ICON_NONE);
}

static void face_corner_uv_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, RNA_enum_get(&ptr, "data_types_loops") & DT_TYPE_UV);

  uiItemR(layout, &ptr, "layers_uv_select_src", 0, IFACE_("Layer Selection"), ICON_NONE);
  uiItemR(layout, &ptr, "layers_uv_select_dst", 0, IFACE_("Layer Mapping"), ICON_NONE);
  uiItemR(layout, &ptr, "islands_precision", 0, NULL, ICON_NONE);
}

static void face_panel_draw_header(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_poly_data", 0, NULL, ICON_NONE);
}

static void face_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetActive(layout, RNA_boolean_get(&ptr, "use_poly_data"));

  uiItemR(layout, &ptr, "data_types_polys", 0, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "poly_mapping", 0, IFACE_("Mapping"), ICON_NONE);
}

static void advanced_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Max Distance"));
  uiItemR(row, &ptr, "use_max_distance", 0, "", ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, RNA_boolean_get(&ptr, "use_max_distance"));
  uiItemR(sub, &ptr, "max_distance", 0, "", ICON_NONE);

  uiItemR(layout, &ptr, "ray_radius", 0, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_DataTransfer, panel_draw);
  PanelType *vertex_panel = modifier_subpanel_register(
      region_type, "vertex", "", vertex_panel_draw_header, vertex_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "vertex_vgroup", "Vertex Groups", NULL, vertex_vgroup_panel_draw, vertex_panel);

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
                             "Vertex Colors",
                             NULL,
                             face_corner_vcol_panel_draw,
                             face_corner_panel);
  modifier_subpanel_register(
      region_type, "face_corner_uv", "UVs", NULL, face_corner_uv_panel_draw, face_corner_panel);

  modifier_subpanel_register(
      region_type, "face", "", face_panel_draw_header, face_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "advanced", "Topology Mapping", NULL, advanced_panel_draw, panel_type);
}

#undef HIGH_POLY_WARNING
#undef DT_TYPES_AFFECT_MESH

ModifierTypeInfo modifierType_DataTransfer = {
    /* name */ "DataTransfer",
    /* structName */ "DataTransferModifierData",
    /* structSize */ sizeof(DataTransferModifierData),
    /* type */ eModifierTypeType_NonGeometrical,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_UsesPreview,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
