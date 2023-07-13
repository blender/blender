/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "DEG_depsgraph.h"

#include "ED_grease_pencil.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DNA_scene_types.h"

#include "WM_api.h"

namespace blender::ed::greasepencil {

static int grease_pencil_layer_add_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  Object *object = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  int new_layer_name_length;
  char *new_layer_name = RNA_string_get_alloc(
      op->ptr, "new_layer_name", nullptr, 0, &new_layer_name_length);

  if (grease_pencil.has_active_layer()) {
    LayerGroup &active_group = grease_pencil.get_active_layer()->parent_group();
    Layer &new_layer = grease_pencil.add_layer_after(
        active_group, &grease_pencil.get_active_layer_for_write()->as_node(), new_layer_name);
    grease_pencil.set_active_layer(&new_layer);
    grease_pencil.insert_blank_frame(new_layer, scene->r.cfra, 0, BEZT_KEYTYPE_KEYFRAME);
  }
  else {
    Layer &new_layer = grease_pencil.add_layer(new_layer_name);
    grease_pencil.set_active_layer(&new_layer);
    grease_pencil.insert_blank_frame(new_layer, scene->r.cfra, 0, BEZT_KEYTYPE_KEYFRAME);
  }

  MEM_SAFE_FREE(new_layer_name);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_layer_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add New Layer";
  ot->idname = "GREASE_PENCIL_OT_layer_add";
  ot->description = "Add a new Grease Pencil layer in the active object";

  /* callbacks */
  ot->exec = grease_pencil_layer_add_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_string(
      ot->srna, "new_layer_name", "GP_Layer", INT16_MAX, "Name", "Name of the new layer");
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

  grease_pencil.remove_layer(*grease_pencil.get_active_layer_for_write());

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, &grease_pencil);

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

  Layer *target_layer = grease_pencil.find_layer_by_name(target_layer_name);
  if (!target_layer) {
    MEM_SAFE_FREE(target_layer_name);
    return OPERATOR_CANCELLED;
  }

  Layer *active_layer = grease_pencil.get_active_layer_for_write();
  active_layer->parent_group().unlink_node(&active_layer->as_node());

  switch (reorder_location) {
    case LAYER_REORDER_ABOVE: {
      /* Note: The layers are stored from bottom to top, so inserting above (visually), means
       * inserting the link after the target. */
      target_layer->parent_group().add_layer_after(active_layer, &target_layer->as_node());
      break;
    }
    case LAYER_REORDER_BELOW: {
      /* Note: The layers are stored from bottom to top, so inserting below (visually), means
       * inserting the link before the target. */
      target_layer->parent_group().add_layer_before(active_layer, &target_layer->as_node());
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
                                     "GP_Layer",
                                     INT16_MAX,
                                     "Target Name",
                                     "Name of the target layer");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_enum(
      ot->srna, "location", prop_layer_reorder_location, LAYER_REORDER_ABOVE, "Location", "");
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
    LayerGroup &active_group = grease_pencil.get_active_layer()->parent_group();
    grease_pencil.add_layer_group_after(active_group,
                                        &grease_pencil.get_active_layer_for_write()->as_node(),
                                        new_layer_group_name);
  }
  else {
    grease_pencil.add_layer_group(new_layer_group_name);
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

  PropertyRNA *prop = RNA_def_string(ot->srna,
                                     "new_layer_group_name",
                                     "GP_Group",
                                     INT16_MAX,
                                     "Name",
                                     "Name of the new layer group");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_layers()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_layer_add);
  WM_operatortype_append(GREASE_PENCIL_OT_layer_remove);
  WM_operatortype_append(GREASE_PENCIL_OT_layer_reorder);

  WM_operatortype_append(GREASE_PENCIL_OT_layer_group_add);
}
