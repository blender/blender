/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_grease_pencil_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"

#include "rna_internal.hh" /* own include */

const EnumPropertyItem rna_enum_tree_node_move_type_items[] = {
    {-1, "DOWN", 0, "Down", ""},
    {1, "UP", 0, "Up", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "BKE_attribute.hh"
#  include "BKE_context.hh"
#  include "BKE_curves.hh"
#  include "BKE_grease_pencil.hh"
#  include "BKE_report.hh"

#  include "DEG_depsgraph.hh"

#  include "rna_curves_utils.hh"

static void rna_GreasePencilDrawing_add_curves(ID *grease_pencil_id,
                                               GreasePencilDrawing *drawing_ptr,
                                               ReportList *reports,
                                               const int *sizes,
                                               const int sizes_num)
{
  using namespace blender;
  bke::greasepencil::Drawing &drawing = drawing_ptr->wrap();
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  if (!rna_CurvesGeometry_add_curves(curves, reports, sizes, sizes_num)) {
    return;
  }

  /* Default to `POLY` curves for the newly added ones. */
  drawing.strokes_for_write().curve_types_for_write().take_back(sizes_num).fill(CURVE_TYPE_POLY);
  drawing.strokes_for_write().update_curve_types();

  drawing.tag_topology_changed();

  /* Avoid updates for importers. */
  if (grease_pencil_id->us > 0) {
    DEG_id_tag_update(grease_pencil_id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, grease_pencil_id);
  }
}

static void rna_GreasePencilDrawing_remove_curves(ID *grease_pencil_id,
                                                  GreasePencilDrawing *drawing_ptr,
                                                  ReportList *reports,
                                                  const int *indices_ptr,
                                                  const int indices_num)
{
  using namespace blender;
  bke::greasepencil::Drawing &drawing = drawing_ptr->wrap();
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  if (!rna_CurvesGeometry_remove_curves(curves, reports, indices_ptr, indices_num)) {
    return;
  }

  drawing.tag_topology_changed();

  /* Avoid updates for importers. */
  if (grease_pencil_id->us > 0) {
    DEG_id_tag_update(grease_pencil_id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, grease_pencil_id);
  }
}

static void rna_GreasePencilDrawing_resize_curves(ID *grease_pencil_id,
                                                  GreasePencilDrawing *drawing_ptr,
                                                  ReportList *reports,
                                                  const int *sizes_ptr,
                                                  const int sizes_num,
                                                  const int *indices_ptr,
                                                  const int indices_num)
{
  using namespace blender;
  bke::greasepencil::Drawing &drawing = drawing_ptr->wrap();
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  if (!rna_CurvesGeometry_resize_curves(
          curves, reports, sizes_ptr, sizes_num, indices_ptr, indices_num))
  {
    return;
  }

  drawing.tag_topology_changed();

  /* Avoid updates for importers. */
  if (grease_pencil_id->us > 0) {
    DEG_id_tag_update(grease_pencil_id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, grease_pencil_id);
  }
}

static void rna_GreasePencilDrawing_set_types(ID *grease_pencil_id,
                                              GreasePencilDrawing *drawing_ptr,
                                              ReportList *reports,
                                              const int type,
                                              const int *indices_ptr,
                                              const int indices_num)
{
  using namespace blender;
  bke::greasepencil::Drawing &drawing = drawing_ptr->wrap();
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  if (!rna_CurvesGeometry_set_types(curves, reports, type, indices_ptr, indices_num)) {
    return;
  }
  /* Avoid updates for importers. */
  if (grease_pencil_id->us > 0) {
    DEG_id_tag_update(grease_pencil_id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, grease_pencil_id);
  }
}

static void rna_GreasePencilDrawing_tag_positions_changed(GreasePencilDrawing *drawing_ptr)
{
  drawing_ptr->wrap().tag_positions_changed();
}

static GreasePencilFrame *rna_Frames_frame_new(ID *id,
                                               GreasePencilLayer *layer_in,
                                               ReportList *reports,
                                               int frame_number)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *reinterpret_cast<GreasePencil *>(id);
  Layer &layer = static_cast<GreasePencilLayer *>(layer_in)->wrap();

  if (layer.frames().contains(frame_number)) {
    BKE_reportf(reports, RPT_ERROR, "Frame already exists on frame number %d", frame_number);
    return nullptr;
  }

  grease_pencil.insert_frame(layer, frame_number, 0, BEZT_KEYTYPE_KEYFRAME);
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, &grease_pencil);

  return layer.frame_at(frame_number);
}

static void rna_Frames_frame_remove(ID *id,
                                    GreasePencilLayer *layer_in,
                                    ReportList *reports,
                                    int frame_number)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *reinterpret_cast<GreasePencil *>(id);
  Layer &layer = static_cast<GreasePencilLayer *>(layer_in)->wrap();

  if (!layer.frames().contains(frame_number)) {
    BKE_reportf(reports, RPT_ERROR, "Frame doesn't exists on frame number %d", frame_number);
    return;
  }

  if (grease_pencil.remove_frames(layer, {frame_number})) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GPENCIL | NA_EDITED, &grease_pencil);
  }
}

static GreasePencilFrame *rna_Frames_frame_copy(ID *id,
                                                GreasePencilLayer *layer_in,
                                                ReportList *reports,
                                                int from_frame_number,
                                                int to_frame_number,
                                                bool instance_drawing)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *reinterpret_cast<GreasePencil *>(id);
  Layer &layer = static_cast<GreasePencilLayer *>(layer_in)->wrap();

  if (!layer.frames().contains(from_frame_number)) {
    BKE_reportf(reports, RPT_ERROR, "Frame doesn't exists on frame number %d", from_frame_number);
    return nullptr;
  }
  if (layer.frames().contains(to_frame_number)) {
    BKE_reportf(reports, RPT_ERROR, "Frame already exists on frame number %d", to_frame_number);
    return nullptr;
  }

  grease_pencil.insert_duplicate_frame(
      layer, from_frame_number, to_frame_number, instance_drawing);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, &grease_pencil);

  return layer.frame_at(to_frame_number);
}

static GreasePencilFrame *rna_Frames_frame_move(ID *id,
                                                GreasePencilLayer *layer_in,
                                                ReportList *reports,
                                                int from_frame_number,
                                                int to_frame_number)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *reinterpret_cast<GreasePencil *>(id);
  Layer &layer = static_cast<GreasePencilLayer *>(layer_in)->wrap();

  if (!layer.frames().contains(from_frame_number)) {
    BKE_reportf(reports, RPT_ERROR, "Frame doesn't exists on frame number %d", from_frame_number);
    return nullptr;
  }
  if (layer.frames().contains(to_frame_number)) {
    BKE_reportf(reports, RPT_ERROR, "Frame already exists on frame number %d", to_frame_number);
    return nullptr;
  }

  grease_pencil.insert_duplicate_frame(layer, from_frame_number, to_frame_number, true);
  grease_pencil.remove_frames(layer, {from_frame_number});
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, &grease_pencil);

  return layer.frame_at(to_frame_number);
}

static GreasePencilFrame *rna_GreasePencilLayer_get_frame_at(GreasePencilLayer *layer,
                                                             int frame_number)
{
  using namespace blender::bke::greasepencil;
  return static_cast<Layer *>(layer)->frame_at(frame_number);
}

static GreasePencilFrame *rna_GreasePencilLayer_current_frame(GreasePencilLayer *layer,
                                                              bContext *C)
{
  using namespace blender::bke::greasepencil;
  Scene *scene = CTX_data_scene(C);
  return static_cast<Layer *>(layer)->frame_at(scene->r.cfra);
}

static GreasePencilLayer *rna_GreasePencil_layer_new(GreasePencil *grease_pencil,
                                                     const char *name,
                                                     const bool set_active,
                                                     PointerRNA *layer_group_ptr)
{
  using namespace blender::bke::greasepencil;
  LayerGroup *layer_group = nullptr;
  if (layer_group_ptr && layer_group_ptr->data) {
    layer_group = static_cast<LayerGroup *>(layer_group_ptr->data);
  }
  Layer *layer;
  if (layer_group) {
    layer = &grease_pencil->add_layer(*layer_group, name);
  }
  else {
    layer = &grease_pencil->add_layer(name);
  }
  if (set_active) {
    grease_pencil->set_active_layer(layer);
  }

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);

  return layer;
}

static void rna_GreasePencil_layer_remove(GreasePencil *grease_pencil, PointerRNA *layer_ptr)
{
  blender::bke::greasepencil::Layer &layer = *static_cast<blender::bke::greasepencil::Layer *>(
      layer_ptr->data);
  grease_pencil->remove_layer(layer);

  RNA_POINTER_INVALIDATE(layer_ptr);
  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_SELECTED, grease_pencil);
}

static void rna_GreasePencil_layer_move(GreasePencil *grease_pencil,
                                        PointerRNA *layer_ptr,
                                        const int direction)
{
  if (direction == 0) {
    return;
  }

  blender::bke::greasepencil::TreeNode &layer_node =
      static_cast<blender::bke::greasepencil::Layer *>(layer_ptr->data)->as_node();
  switch (direction) {
    case -1:
      grease_pencil->move_node_down(layer_node, 1);
      break;
    case 1:
      grease_pencil->move_node_up(layer_node, 1);
      break;
  }

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);
}

static void rna_GreasePencil_layer_move_top(GreasePencil *grease_pencil, PointerRNA *layer_ptr)
{
  blender::bke::greasepencil::TreeNode &layer_node =
      static_cast<blender::bke::greasepencil::Layer *>(layer_ptr->data)->as_node();
  grease_pencil->move_node_top(layer_node);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);
}

static void rna_GreasePencil_layer_move_bottom(GreasePencil *grease_pencil, PointerRNA *layer_ptr)
{
  blender::bke::greasepencil::TreeNode &layer_node =
      static_cast<blender::bke::greasepencil::Layer *>(layer_ptr->data)->as_node();
  grease_pencil->move_node_bottom(layer_node);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);
}

static void rna_GreasePencil_layer_move_to_layer_group(GreasePencil *grease_pencil,
                                                       PointerRNA *layer_ptr,
                                                       PointerRNA *layer_group_ptr)
{
  using namespace blender::bke::greasepencil;
  TreeNode &layer_node = static_cast<Layer *>(layer_ptr->data)->as_node();
  LayerGroup *layer_group;
  if (layer_group_ptr && layer_group_ptr->data) {
    layer_group = static_cast<LayerGroup *>(layer_group_ptr->data);
  }
  else {
    layer_group = &grease_pencil->root_group();
  }
  if (layer_group == nullptr) {
    return;
  }
  grease_pencil->move_node_into(layer_node, *layer_group);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);
}

static PointerRNA rna_GreasePencil_layer_group_new(GreasePencil *grease_pencil,
                                                   const char *name,
                                                   PointerRNA *parent_group_ptr)
{
  using namespace blender::bke::greasepencil;
  LayerGroup *parent_group;
  if (parent_group_ptr && parent_group_ptr->data) {
    parent_group = static_cast<LayerGroup *>(parent_group_ptr->data);
  }
  else {
    parent_group = &grease_pencil->root_group();
  }
  LayerGroup *new_layer_group = &grease_pencil->add_layer_group(*parent_group, name);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);

  PointerRNA ptr = RNA_pointer_create(
      &grease_pencil->id, &RNA_GreasePencilLayerGroup, new_layer_group);
  return ptr;
}

static void rna_GreasePencil_layer_group_remove(GreasePencil *grease_pencil,
                                                PointerRNA *layer_group_ptr,
                                                bool keep_children)
{
  using namespace blender::bke::greasepencil;
  LayerGroup &layer_group = *static_cast<LayerGroup *>(layer_group_ptr->data);
  grease_pencil->remove_group(layer_group, keep_children);

  RNA_POINTER_INVALIDATE(layer_group_ptr);
  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_SELECTED, grease_pencil);
}

static void rna_GreasePencil_layer_group_move(GreasePencil *grease_pencil,
                                              PointerRNA *layer_group_ptr,
                                              int direction)
{
  if (direction == 0) {
    return;
  }

  blender::bke::greasepencil::TreeNode &layer_group_node =
      static_cast<blender::bke::greasepencil::LayerGroup *>(layer_group_ptr->data)->as_node();
  switch (direction) {
    case -1:
      grease_pencil->move_node_down(layer_group_node, 1);
      break;
    case 1:
      grease_pencil->move_node_up(layer_group_node, 1);
      break;
  }

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);
}

static void rna_GreasePencil_layer_group_move_top(GreasePencil *grease_pencil,
                                                  PointerRNA *layer_group_ptr)
{
  blender::bke::greasepencil::TreeNode &layer_group_node =
      static_cast<blender::bke::greasepencil::LayerGroup *>(layer_group_ptr->data)->as_node();
  grease_pencil->move_node_top(layer_group_node);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);
}

static void rna_GreasePencil_layer_group_move_bottom(GreasePencil *grease_pencil,
                                                     PointerRNA *layer_group_ptr)
{
  blender::bke::greasepencil::TreeNode &layer_group_node =
      static_cast<blender::bke::greasepencil::LayerGroup *>(layer_group_ptr->data)->as_node();
  grease_pencil->move_node_bottom(layer_group_node);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);
}

static void rna_GreasePencil_layer_group_move_to_layer_group(GreasePencil *grease_pencil,
                                                             PointerRNA *layer_group_ptr,
                                                             PointerRNA *parent_group_ptr)
{
  using namespace blender::bke::greasepencil;
  TreeNode &layer_group_node = static_cast<LayerGroup *>(layer_group_ptr->data)->as_node();
  LayerGroup *parent_group;
  if (parent_group_ptr && parent_group_ptr->data) {
    parent_group = static_cast<LayerGroup *>(parent_group_ptr->data);
  }
  else {
    parent_group = &grease_pencil->root_group();
  }
  grease_pencil->move_node_into(layer_group_node, *parent_group);

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, grease_pencil);
}

#else

void RNA_api_grease_pencil_drawing(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "add_strokes", "rna_GreasePencilDrawing_add_curves");
  RNA_def_function_ui_description(func, "Add new strokes with provided sizes at the end");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_int_array(func,
                           "sizes",
                           1,
                           nullptr,
                           1,
                           INT_MAX,
                           "Sizes",
                           "The number of points in each stroke",
                           1,
                           10000);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);

  func = RNA_def_function(srna, "remove_strokes", "rna_GreasePencilDrawing_remove_curves");
  RNA_def_function_ui_description(func,
                                  "Remove all strokes. If indices are provided, remove only the "
                                  "strokes with the given indices.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_int_array(func,
                           "indices",
                           1,
                           nullptr,
                           0,
                           INT_MAX,
                           "Indices",
                           "The indices of the strokes to remove",
                           0,
                           10000);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));

  func = RNA_def_function(srna, "resize_strokes", "rna_GreasePencilDrawing_resize_curves");
  RNA_def_function_ui_description(
      func,
      "Resize all existing strokes. If indices are provided, resize only the strokes with the "
      "given indices. If the new size for a stroke is smaller, the stroke is trimmed. If "
      "the new size for a stroke is larger, the new end values are default initialized.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_int_array(func,
                           "sizes",
                           1,
                           nullptr,
                           1,
                           INT_MAX,
                           "Sizes",
                           "The number of points in each stroke",
                           1,
                           10000);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);
  parm = RNA_def_int_array(func,
                           "indices",
                           1,
                           nullptr,
                           0,
                           INT_MAX,
                           "Indices",
                           "The indices of the stroke to resize",
                           0,
                           10000);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));

  func = RNA_def_function(srna, "set_types", "rna_GreasePencilDrawing_set_types");
  RNA_def_function_ui_description(func,
                                  "Set the curve type. If indices are provided, set only the "
                                  "types with the given curve indices.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  RNA_def_enum(func, "type", rna_enum_curves_type_items, CURVE_TYPE_CATMULL_ROM, "Type", "");
  parm = RNA_def_int_array(func,
                           "indices",
                           1,
                           nullptr,
                           0,
                           INT_MAX,
                           "Indices",
                           "The indices of the curves to resize",
                           0,
                           INT_MAX);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));

  func = RNA_def_function(
      srna, "tag_positions_changed", "rna_GreasePencilDrawing_tag_positions_changed");
  RNA_def_function_ui_description(
      func, "Indicate that the positions of points in the drawing have changed");
}

void RNA_api_grease_pencil_frames(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "new", "rna_Frames_frame_new");
  RNA_def_function_ui_description(func, "Add a new Grease Pencil frame");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_int(func,
                     "frame_number",
                     1,
                     MINAFRAME,
                     MAXFRAME,
                     "Frame Number",
                     "The frame on which the drawing appears",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "frame", "GreasePencilFrame", "", "The newly created frame");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Frames_frame_remove");
  RNA_def_function_ui_description(func, "Remove a Grease Pencil frame");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_int(func,
                     "frame_number",
                     1,
                     MINAFRAME,
                     MAXFRAME,
                     "Frame Number",
                     "The frame number of the frame to remove",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "copy", "rna_Frames_frame_copy");
  RNA_def_function_ui_description(func, "Copy a Grease Pencil frame");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_int(func,
                     "from_frame_number",
                     1,
                     MINAFRAME,
                     MAXFRAME,
                     "Source Frame Number",
                     "The frame number of the source frame",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "to_frame_number",
                     2,
                     MINAFRAME,
                     MAXFRAME,
                     "Frame Number of Copy",
                     "The frame number to copy the frame to",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func,
                         "instance_drawing",
                         false,
                         "Instance Drawing",
                         "Let the copied frame use the same drawing as the source");
  parm = RNA_def_pointer(func, "copy", "GreasePencilFrame", "", "The newly copied frame");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "move", "rna_Frames_frame_move");
  RNA_def_function_ui_description(func, "Move a Grease Pencil frame");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_int(func,
                     "from_frame_number",
                     1,
                     MINAFRAME,
                     MAXFRAME,
                     "Source Frame Number",
                     "The frame number of the source frame",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "to_frame_number",
                     2,
                     MINAFRAME,
                     MAXFRAME,
                     "Target Frame Number",
                     "The frame number to move the frame to",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "moved", "GreasePencilFrame", "", "The moved frame");
  RNA_def_function_return(func, parm);
}

void RNA_api_grease_pencil_layer(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "get_frame_at", "rna_GreasePencilLayer_get_frame_at");
  RNA_def_function_ui_description(func, "Get the frame at given frame number");
  parm = RNA_def_int(
      func, "frame_number", 1, MINAFRAME, MAXFRAME, "Frame Number", "", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "frame", "GreasePencilFrame", "Frame", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "current_frame", "rna_GreasePencilLayer_current_frame");
  RNA_def_function_ui_description(
      func, "The Grease Pencil frame at the current scene time on this layer");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "frame", "GreasePencilFrame", "", "");
  RNA_def_function_return(func, parm);
}

void RNA_api_grease_pencil_layers(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "new", "rna_GreasePencil_layer_new");
  RNA_def_function_ui_description(func, "Add a new Grease Pencil layer");
  parm = RNA_def_string(func, "name", "GreasePencilLayer", MAX_NAME, "Name", "Name of the layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(
      func, "set_active", true, "Set Active", "Set the newly created layer as the active layer");
  parm = RNA_def_pointer(
      func,
      "layer_group",
      "GreasePencilLayerGroup",
      "",
      "The layer group the new layer will be created in (use None for the main stack)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  parm = RNA_def_pointer(func, "layer", "GreasePencilLayer", "", "The newly created layer");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GreasePencil_layer_remove");
  RNA_def_function_ui_description(func, "Remove a Grease Pencil layer");
  parm = RNA_def_pointer(func, "layer", "GreasePencilLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "move", "rna_GreasePencil_layer_move");
  RNA_def_function_ui_description(func,
                                  "Move a Grease Pencil layer in the layer group or main stack");
  parm = RNA_def_pointer(func, "layer", "GreasePencilLayer", "", "The layer to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  parm = RNA_def_enum(
      func, "type", rna_enum_tree_node_move_type_items, 1, "", "Direction of movement");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "move_top", "rna_GreasePencil_layer_move_top");
  RNA_def_function_ui_description(
      func, "Move a Grease Pencil layer to the top of the layer group or main stack");
  parm = RNA_def_pointer(func, "layer", "GreasePencilLayer", "", "The layer to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "move_bottom", "rna_GreasePencil_layer_move_bottom");
  RNA_def_function_ui_description(
      func, "Move a Grease Pencil layer to the bottom of the layer group or main stack");
  parm = RNA_def_pointer(func, "layer", "GreasePencilLayer", "", "The layer to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(
      srna, "move_to_layer_group", "rna_GreasePencil_layer_move_to_layer_group");
  RNA_def_function_ui_description(func, "Move a Grease Pencil layer into a layer group");
  parm = RNA_def_pointer(func, "layer", "GreasePencilLayer", "", "The layer to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  parm = RNA_def_pointer(
      func,
      "layer_group",
      "GreasePencilLayerGroup",
      "",
      "The layer group the layer will be moved into (use None for the main stack)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

void RNA_api_grease_pencil_layer_groups(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "new", "rna_GreasePencil_layer_group_new");
  RNA_def_function_ui_description(func, "Add a new Grease Pencil layer group");
  parm = RNA_def_string(
      func, "name", "GreasePencilLayerGroup", MAX_NAME, "Name", "Name of the layer group");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func,
                         "parent_group",
                         "GreasePencilLayerGroup",
                         "",
                         "The parent layer group the new group will be created in (use None "
                         "for the main stack)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  parm = RNA_def_pointer(
      func, "layer_group", "GreasePencilLayerGroup", "", "The newly created layer group");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GreasePencil_layer_group_remove");
  RNA_def_function_ui_description(func, "Remove a new Grease Pencil layer group");
  parm = RNA_def_pointer(
      func, "layer_group", "GreasePencilLayerGroup", "", "The layer group to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  parm = RNA_def_boolean(func,
                         "keep_children",
                         false,
                         "",
                         "Keep the children nodes of the group and only delete the group itself");

  func = RNA_def_function(srna, "move", "rna_GreasePencil_layer_group_move");
  RNA_def_function_ui_description(func,
                                  "Move a layer group in the parent layer group or main stack");
  parm = RNA_def_pointer(
      func, "layer_group", "GreasePencilLayerGroup", "", "The layer group to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  parm = RNA_def_enum(
      func, "type", rna_enum_tree_node_move_type_items, 1, "", "Direction of movement");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "move_top", "rna_GreasePencil_layer_group_move_top");
  RNA_def_function_ui_description(
      func, "Move a layer group to the top of the parent layer group or main stack");
  parm = RNA_def_pointer(
      func, "layer_group", "GreasePencilLayerGroup", "", "The layer group to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "move_bottom", "rna_GreasePencil_layer_group_move_bottom");
  RNA_def_function_ui_description(
      func, "Move a layer group to the bottom of the parent layer group or main stack");
  parm = RNA_def_pointer(
      func, "layer_group", "GreasePencilLayerGroup", "", "The layer group to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(
      srna, "move_to_layer_group", "rna_GreasePencil_layer_group_move_to_layer_group");
  RNA_def_function_ui_description(func, "Move a layer group into a parent layer group");
  parm = RNA_def_pointer(
      func, "layer_group", "GreasePencilLayerGroup", "", "The layer group to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  parm = RNA_def_pointer(func,
                         "parent_group",
                         "GreasePencilLayerGroup",
                         "",
                         "The parent layer group the layer group will be moved into (use "
                         "None for the main stack)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

#endif
