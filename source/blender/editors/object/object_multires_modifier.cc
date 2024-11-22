/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "object_intern.hh"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_main.hh"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "DEG_depsgraph.hh"

#include "ED_object.hh"
#include "ED_sculpt.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender::ed::object {

/* ------------------------------------------------------------------- */
/** \name Multires Delete Higher Levels Operator
 * \{ */

static bool multires_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_MultiresModifier, (1 << OB_MESH), true, false);
}

static int multires_higher_levels_delete_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = context_active_object(C);
  MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_Multires);

  if (!mmd) {
    return OPERATOR_CANCELLED;
  }

  multiresModifier_del_levels(mmd, scene, ob, 1);

  iter_other(CTX_data_main(C), ob, true, multires_update_totlevels, &mmd->totlvl);

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int multires_higher_levels_delete_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return multires_higher_levels_delete_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_higher_levels_delete(wmOperatorType *ot)
{
  ot->name = "Delete Higher Levels";
  ot->description = "Deletes the higher resolution mesh, potential loss of detail";
  ot->idname = "OBJECT_OT_multires_higher_levels_delete";

  ot->poll = multires_poll;
  ot->invoke = multires_higher_levels_delete_invoke;
  ot->exec = multires_higher_levels_delete_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Multires Subdivide Operator
 * \{ */

static EnumPropertyItem prop_multires_subdivide_mode_type[] = {
    {int8_t(MultiresSubdivideModeType::CatmullClark),
     "CATMULL_CLARK",
     0,
     "Catmull-Clark",
     "Create a new level using Catmull-Clark subdivisions"},
    {int8_t(MultiresSubdivideModeType::Simple),
     "SIMPLE",
     0,
     "Simple",
     "Create a new level using simple subdivisions"},
    {int8_t(MultiresSubdivideModeType::Linear),
     "LINEAR",
     0,
     "Linear",
     "Create a new level using linear interpolation of the sculpted displacement"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int multires_subdivide_exec(bContext *C, wmOperator *op)
{
  Object *object = context_active_object(C);
  MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(
      op, object, eModifierType_Multires);

  if (!mmd) {
    return OPERATOR_CANCELLED;
  }

  const MultiresSubdivideModeType subdivide_mode = (MultiresSubdivideModeType)RNA_enum_get(op->ptr,
                                                                                           "mode");
  multiresModifier_subdivide(object, mmd, subdivide_mode);

  iter_other(CTX_data_main(C), object, true, multires_update_totlevels, &mmd->totlvl);

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, object);

  if (object->mode & OB_MODE_SCULPT) {
    /* ensure that grid paint mask layer is created */
    BKE_sculpt_mask_layers_ensure(
        CTX_data_ensure_evaluated_depsgraph(C), CTX_data_main(C), object, mmd);
  }

  return OPERATOR_FINISHED;
}

static int multires_subdivide_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return multires_subdivide_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_subdivide(wmOperatorType *ot)
{
  ot->name = "Multires Subdivide";
  ot->description = "Add a new level of subdivision";
  ot->idname = "OBJECT_OT_multires_subdivide";

  ot->poll = multires_poll;
  ot->invoke = multires_subdivide_invoke;
  ot->exec = multires_subdivide_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
  RNA_def_enum(ot->srna,
               "mode",
               prop_multires_subdivide_mode_type,
               int8_t(MultiresSubdivideModeType::CatmullClark),
               "Subdivision Mode",
               "How the mesh is going to be subdivided to create a new level");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Multires Reshape Operator
 * \{ */

static int multires_reshape_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = context_active_object(C), *secondob = nullptr;
  MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_Multires);

  if (!mmd) {
    return OPERATOR_CANCELLED;
  }

  if (mmd->lvl == 0) {
    BKE_report(op->reports, RPT_ERROR, "Reshape can work only with higher levels of subdivisions");
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, Object *, selob, selected_editable_objects) {
    if (selob->type == OB_MESH && selob != ob) {
      secondob = selob;
      break;
    }
  }
  CTX_DATA_END;

  if (!secondob) {
    BKE_report(op->reports, RPT_ERROR, "Second selected mesh object required to copy shape from");
    return OPERATOR_CANCELLED;
  }

  if (!multiresModifier_reshapeFromObject(depsgraph, mmd, ob, secondob)) {
    BKE_report(op->reports, RPT_ERROR, "Objects do not have the same number of vertices");
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static int multires_reshape_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return multires_reshape_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_reshape(wmOperatorType *ot)
{
  ot->name = "Multires Reshape";
  ot->description = "Copy vertex coordinates from other object";
  ot->idname = "OBJECT_OT_multires_reshape";

  ot->poll = multires_poll;
  ot->invoke = multires_reshape_invoke;
  ot->exec = multires_reshape_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Multires Save External Operator
 * \{ */

static int multires_external_save_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  Mesh *mesh = (ob) ? static_cast<Mesh *>(ob->data) : static_cast<Mesh *>(op->customdata);
  char filepath[FILE_MAX];
  const bool relative = RNA_boolean_get(op->ptr, "relative_path");

  if (!mesh) {
    return OPERATOR_CANCELLED;
  }

  if (CustomData_external_test(&mesh->corner_data, CD_MDISPS)) {
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filepath", filepath);

  if (relative) {
    BLI_path_rel(filepath, BKE_main_blendfile_path(bmain));
  }

  CustomData_external_add(&mesh->corner_data, &mesh->id, CD_MDISPS, mesh->corners_num, filepath);
  CustomData_external_write(
      &mesh->corner_data, &mesh->id, CD_MASK_MESH.lmask, mesh->corners_num, 0);

  return OPERATOR_FINISHED;
}

static int multires_external_save_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Object *ob = context_active_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  char filepath[FILE_MAX];

  if (!edit_modifier_invoke_properties(C, op)) {
    return OPERATOR_CANCELLED;
  }

  MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_Multires);

  if (!mmd) {
    return OPERATOR_CANCELLED;
  }

  if (CustomData_external_test(&mesh->corner_data, CD_MDISPS)) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return multires_external_save_exec(C, op);
  }

  op->customdata = mesh;

  SNPRINTF(filepath, "//%s.btx", mesh->id.name + 2);
  RNA_string_set(op->ptr, "filepath", filepath);

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void OBJECT_OT_multires_external_save(wmOperatorType *ot)
{
  ot->name = "Multires Save External";
  ot->description = "Save displacements to an external file";
  ot->idname = "OBJECT_OT_multires_external_save";

  /* XXX modifier no longer in context after file browser: `ot->poll = multires_poll;`. */
  ot->exec = multires_external_save_exec;
  ot->invoke = multires_external_save_invoke;
  ot->poll = multires_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BTX,
                                 FILE_SPECIAL,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Multires Pack Operator
 * \{ */

static int multires_external_pack_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_active_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  if (!CustomData_external_test(&mesh->corner_data, CD_MDISPS)) {
    return OPERATOR_CANCELLED;
  }

  /* XXX don't remove. */
  CustomData_external_remove(&mesh->corner_data, &mesh->id, CD_MDISPS, mesh->corners_num);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_multires_external_pack(wmOperatorType *ot)
{
  ot->name = "Multires Pack External";
  ot->description = "Pack displacements from an external file";
  ot->idname = "OBJECT_OT_multires_external_pack";

  ot->poll = multires_poll;
  ot->exec = multires_external_pack_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Multires Apply Base
 * \{ */

static int multires_base_apply_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *object = context_active_object(C);
  MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(
      op, object, eModifierType_Multires);

  if (!mmd) {
    return OPERATOR_CANCELLED;
  }

  ed::sculpt_paint::undo::push_multires_mesh_begin(C, op->type->name);

  multiresModifier_base_apply(depsgraph, object, mmd);

  ed::sculpt_paint::undo::push_multires_mesh_end(C, op->type->name);

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, object);

  return OPERATOR_FINISHED;
}

static int multires_base_apply_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return multires_base_apply_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_base_apply(wmOperatorType *ot)
{
  ot->name = "Multires Apply Base";
  ot->description = "Modify the base mesh to conform to the displaced mesh";
  ot->idname = "OBJECT_OT_multires_base_apply";

  ot->poll = multires_poll;
  ot->invoke = multires_base_apply_invoke;
  ot->exec = multires_base_apply_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Multires Unsubdivide
 * \{ */

static int multires_unsubdivide_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *object = context_active_object(C);
  MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(
      op, object, eModifierType_Multires);

  if (!mmd) {
    return OPERATOR_CANCELLED;
  }

  int new_levels = multiresModifier_rebuild_subdiv(depsgraph, object, mmd, 1, true);
  if (new_levels == 0) {
    BKE_report(op->reports, RPT_ERROR, "No valid subdivisions found to rebuild a lower level");
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, object);

  return OPERATOR_FINISHED;
}

static int multires_unsubdivide_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return multires_unsubdivide_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_unsubdivide(wmOperatorType *ot)
{
  ot->name = "Unsubdivide";
  ot->description = "Rebuild a lower subdivision level of the current base mesh";
  ot->idname = "OBJECT_OT_multires_unsubdivide";

  ot->poll = multires_poll;
  ot->invoke = multires_unsubdivide_invoke;
  ot->exec = multires_unsubdivide_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Multires Rebuild Subdivisions
 * \{ */

static int multires_rebuild_subdiv_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *object = context_active_object(C);
  MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(
      op, object, eModifierType_Multires);

  if (!mmd) {
    return OPERATOR_CANCELLED;
  }

  int new_levels = multiresModifier_rebuild_subdiv(depsgraph, object, mmd, INT_MAX, false);
  if (new_levels == 0) {
    BKE_report(op->reports, RPT_ERROR, "No valid subdivisions found to rebuild lower levels");
    return OPERATOR_CANCELLED;
  }

  BKE_reportf(op->reports, RPT_INFO, "%d new levels rebuilt", new_levels);

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, object);

  return OPERATOR_FINISHED;
}

static int multires_rebuild_subdiv_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return multires_rebuild_subdiv_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_rebuild_subdiv(wmOperatorType *ot)
{
  ot->name = "Rebuild Lower Subdivisions";
  ot->description =
      "Rebuilds all possible subdivisions levels to generate a lower resolution base mesh";
  ot->idname = "OBJECT_OT_multires_rebuild_subdiv";

  ot->poll = multires_poll;
  ot->invoke = multires_rebuild_subdiv_invoke;
  ot->exec = multires_rebuild_subdiv_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

}  // namespace blender::ed::object
