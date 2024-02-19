/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"

#include "DEG_depsgraph.hh"

#include "ED_grease_pencil.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DNA_scene_types.h"

#include "WM_api.hh"

namespace blender::ed::greasepencil {

void select_layer_channel(GreasePencil &grease_pencil, bke::greasepencil::Layer *layer)
{
  using namespace blender::bke::greasepencil;

  if (layer != nullptr) {
    layer->set_selected(true);
  }

  if (grease_pencil.active_layer != layer) {
    grease_pencil.set_active_layer(layer);
    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, &grease_pencil);
  }
}

static int grease_pencil_layer_add_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  int new_layer_name_length;
  char *new_layer_name = RNA_string_get_alloc(
      op->ptr, "new_layer_name", nullptr, 0, &new_layer_name_length);
  BLI_SCOPED_DEFER([&] { MEM_SAFE_FREE(new_layer_name); });
  if (grease_pencil.has_active_layer()) {
    Layer &new_layer = grease_pencil.add_layer(new_layer_name);
    grease_pencil.move_node_after(new_layer.as_node(),
                                  grease_pencil.get_active_layer()->as_node());
    grease_pencil.set_active_layer(&new_layer);
    grease_pencil.insert_blank_frame(new_layer, scene->r.cfra, 0, BEZT_KEYTYPE_KEYFRAME);
  }
  else {
    Layer &new_layer = grease_pencil.add_layer(new_layer_name);
    grease_pencil.set_active_layer(&new_layer);
    grease_pencil.insert_blank_frame(new_layer, scene->r.cfra, 0, BEZT_KEYTYPE_KEYFRAME);
  }

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add New Layer";
  ot->idname = "GREASE_PENCIL_OT_layer_add";
  ot->description = "Add a new Grease Pencil layer in the active object";

  /* callbacks */
  ot->invoke = WM_operator_props_popup_confirm;
  ot->exec = grease_pencil_layer_add_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_string(
      ot->srna, "new_layer_name", "Layer", INT16_MAX, "Name", "Name of the new layer");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

static int grease_pencil_layer_remove_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  if (!grease_pencil.has_active_layer()) {
    return OPERATOR_CANCELLED;
  }

  grease_pencil.remove_layer(*grease_pencil.get_active_layer());

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Layer";
  ot->idname = "GREASE_PENCIL_OT_layer_remove";
  ot->description = "Remove the active Grease Pencil layer";

  /* callbacks */
  ot->exec = grease_pencil_layer_remove_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static const EnumPropertyItem prop_layer_reorder_location[] = {
    {LAYER_REORDER_ABOVE, "ABOVE", 0, "Above", ""},
    {LAYER_REORDER_BELOW, "BELOW", 0, "Below", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static int grease_pencil_layer_reorder_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  if (!grease_pencil.has_active_layer()) {
    return OPERATOR_CANCELLED;
  }

  int target_layer_name_length;
  char *target_layer_name = RNA_string_get_alloc(
      op->ptr, "target_layer_name", nullptr, 0, &target_layer_name_length);
  const int reorder_location = RNA_enum_get(op->ptr, "location");

  TreeNode *target_node = grease_pencil.find_node_by_name(target_layer_name);
  if (!target_node || !target_node->is_layer()) {
    MEM_SAFE_FREE(target_layer_name);
    return OPERATOR_CANCELLED;
  }

  Layer &active_layer = *grease_pencil.get_active_layer();
  switch (reorder_location) {
    case LAYER_REORDER_ABOVE: {
      /* NOTE: The layers are stored from bottom to top, so inserting above (visually), means
       * inserting the link after the target. */
      grease_pencil.move_node_after(active_layer.as_node(), *target_node);
      break;
    }
    case LAYER_REORDER_BELOW: {
      /* NOTE: The layers are stored from bottom to top, so inserting below (visually), means
       * inserting the link before the target. */
      grease_pencil.move_node_before(active_layer.as_node(), *target_node);
      break;
    }
    default:
      BLI_assert_unreachable();
  }

  MEM_SAFE_FREE(target_layer_name);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_reorder(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reorder Layer";
  ot->idname = "GREASE_PENCIL_OT_layer_reorder";
  ot->description = "Reorder the active Grease Pencil layer";

  /* callbacks */
  ot->exec = grease_pencil_layer_reorder_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_string(ot->srna,
                                     "target_layer_name",
                                     "Layer",
                                     INT16_MAX,
                                     "Target Name",
                                     "Name of the target layer");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_enum(
      ot->srna, "location", prop_layer_reorder_location, LAYER_REORDER_ABOVE, "Location", "");
}

static int grease_pencil_layer_active_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  int layer_index = RNA_int_get(op->ptr, "layer");

  const Layer &layer = *grease_pencil.layers()[layer_index];

  if (grease_pencil.is_layer_active(&layer)) {
    return OPERATOR_CANCELLED;
  }
  grease_pencil.set_active_layer(&layer);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_active(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Active Layer";
  ot->idname = "GREASE_PENCIL_OT_layer_active";
  ot->description = "Set the active Grease Pencil layer";

  /* callbacks */
  ot->exec = grease_pencil_layer_active_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_int(
      ot->srna, "layer", 0, 0, INT_MAX, "Grease Pencil Layer", "", 0, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static int grease_pencil_layer_group_add_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  int new_layer_group_name_length;
  char *new_layer_group_name = RNA_string_get_alloc(
      op->ptr, "new_layer_group_name", nullptr, 0, &new_layer_group_name_length);

  if (grease_pencil.has_active_layer()) {
    LayerGroup &new_group = grease_pencil.add_layer_group(
        grease_pencil.get_active_layer()->parent_group(), new_layer_group_name);
    grease_pencil.move_node_after(new_group.as_node(),
                                  grease_pencil.get_active_layer()->as_node());
  }
  else {
    grease_pencil.add_layer_group(grease_pencil.root_group(), new_layer_group_name);
  }

  MEM_SAFE_FREE(new_layer_group_name);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_group_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add New Layer Group";
  ot->idname = "GREASE_PENCIL_OT_layer_group_add";
  ot->description = "Add a new Grease Pencil layer group in the active object";

  /* callbacks */
  ot->exec = grease_pencil_layer_group_add_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_string(
      ot->srna, "new_layer_group_name", nullptr, INT16_MAX, "Name", "Name of the new layer group");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

static int grease_pencil_layer_hide_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");

  if (!grease_pencil.has_active_layer()) {
    return OPERATOR_CANCELLED;
  }

  if (unselected) {
    /* hide unselected */
    for (Layer *layer : grease_pencil.layers_for_write()) {
      const bool is_active = grease_pencil.is_layer_active(layer);
      layer->set_visible(is_active);
    }
  }
  else {
    /* hide selected/active */
    Layer &active_layer = *grease_pencil.get_active_layer();
    active_layer.set_visible(false);
  }

  /* notifiers */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Layer(s)";
  ot->idname = "GREASE_PENCIL_OT_layer_hide";
  ot->description = "Hide selected/unselected Grease Pencil layers";

  /* callbacks */
  ot->exec = grease_pencil_layer_hide_exec;
  ot->poll = active_grease_pencil_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected layers");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

static int grease_pencil_layer_reveal_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  if (!grease_pencil.has_active_layer()) {
    return OPERATOR_CANCELLED;
  }

  for (Layer *layer : grease_pencil.layers_for_write()) {
    layer->set_visible(true);
  }

  /* notifiers */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show All Layers";
  ot->idname = "GREASE_PENCIL_OT_layer_reveal";
  ot->description = "Show all Grease Pencil layers";

  /* callbacks */
  ot->exec = grease_pencil_layer_reveal_exec;
  ot->poll = active_grease_pencil_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int grease_pencil_layer_isolate_exec(bContext *C, wmOperator *op)
{
  using namespace ::blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const int affect_visibility = RNA_boolean_get(op->ptr, "affect_visibility");
  bool isolate = false;

  for (const Layer *layer : grease_pencil.layers()) {
    if (grease_pencil.is_layer_active(layer)) {
      continue;
    }
    if ((affect_visibility && layer->is_visible()) || !layer->is_locked()) {
      isolate = true;
      break;
    }
  }

  for (Layer *layer : grease_pencil.layers_for_write()) {
    if (grease_pencil.is_layer_active(layer) || !isolate) {
      layer->set_locked(false);
      if (affect_visibility) {
        layer->set_visible(true);
      }
    }
    else {
      layer->set_locked(true);
      if (affect_visibility) {
        layer->set_visible(false);
      }
    }
  }

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_isolate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Isolate Layers";
  ot->idname = "GREASE_PENCIL_OT_layer_isolate";
  ot->description = "Make only active layer visible/editable";

  /* callbacks */
  ot->exec = grease_pencil_layer_isolate_exec;
  ot->poll = active_grease_pencil_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(
      ot->srna, "affect_visibility", false, "Affect Visibility", "Also affect the visibility");
}
}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_layers()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_layer_add);
  WM_operatortype_append(GREASE_PENCIL_OT_layer_remove);
  WM_operatortype_append(GREASE_PENCIL_OT_layer_reorder);
  WM_operatortype_append(GREASE_PENCIL_OT_layer_active);
  WM_operatortype_append(GREASE_PENCIL_OT_layer_hide);
  WM_operatortype_append(GREASE_PENCIL_OT_layer_reveal);
  WM_operatortype_append(GREASE_PENCIL_OT_layer_isolate);

  WM_operatortype_append(GREASE_PENCIL_OT_layer_group_add);
}
