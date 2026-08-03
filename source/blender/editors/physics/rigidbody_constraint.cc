/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor_physics
 * \brief Rigid Body constraint editing operators
 */

#include <cstdlib>
#include <cstring>

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_report.hh"
#include "BKE_rigidbody.h"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"
#include "ED_physics.hh"
#include "ED_screen.hh"

#include "physics_intern.hh"

namespace blender {

/* ********************************************** */
/* Helper API's for RigidBody Constraint Editing */

static bool operator_rigidbody_constraints_editable_poll(Scene *scene)
{
  if (scene == nullptr || !ID_IS_EDITABLE(scene) || ID_IS_OVERRIDE_LIBRARY(scene) ||
      (scene->rigidbody_world != nullptr && scene->rigidbody_world->constraints != nullptr &&
       (!ID_IS_EDITABLE(scene->rigidbody_world->constraints) ||
        ID_IS_OVERRIDE_LIBRARY(scene->rigidbody_world->constraints))))
  {
    return false;
  }
  return true;
}

static bool operator_rigidbody_con_active_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (!operator_rigidbody_constraints_editable_poll(scene)) {
    return false;
  }

  Object *ob = ed::object::context_active_object(C);
  return (ob && ob->rigidbody_constraint && ED_operator_object_active_editable_ex(C, ob));
}

static bool operator_rigidbody_con_add_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (!operator_rigidbody_constraints_editable_poll(scene)) {
    return false;
  }
  return ED_operator_object_active_editable(C);
}

bool ED_rigidbody_constraint_add(
    Main *bmain, Scene *scene, Object *ob, eRigidBodyCon_Type type, ReportList *reports)
{
  RigidBodyWorld *rbw = BKE_rigidbody_get_world(scene);

  /* check that object doesn't already have a constraint */
  if (ob->rigidbody_constraint) {
    BKE_reportf(
        reports, RPT_INFO, "Object '%s' already has a Rigid Body Constraint", ob->id.name + 2);
    return false;
  }
  /* create constraint group if it doesn't already exits */
  if (rbw->constraints == nullptr) {
    rbw->constraints = BKE_collection_add(bmain, nullptr, "RigidBodyConstraints");
    id_us_plus(&rbw->constraints->id);
  }
  /* make rigidbody constraint settings */
  ob->rigidbody_constraint = BKE_rigidbody_create_constraint(scene, ob, type);

  /* add constraint to rigid body constraint group */
  BKE_collection_object_add(bmain, rbw->constraints, ob);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  DEG_id_tag_update(&rbw->constraints->id, ID_RECALC_SYNC_TO_EVAL);

  return true;
}

void ED_rigidbody_constraint_remove(Main *bmain, Scene *scene, Object *ob)
{
  BKE_rigidbody_remove_constraint(bmain, scene, ob, false);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
}

/* ********************************************** */
/* Active Object Add/Remove Operators */

/* ************ Add Rigid Body Constraint ************** */

static wmOperatorStatus rigidbody_con_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  RigidBodyWorld *rbw = BKE_rigidbody_get_world(scene);
  Object *ob = ed::object::context_active_object(C);
  eRigidBodyCon_Type type = eRigidBodyCon_Type(RNA_enum_get(op->ptr, "type"));
  bool changed;
  /* Poll ensures. */
  BLI_assert(scene && ob);

  /* The rigid body world is not ensured by the poll. */
  if (rbw == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Rigid Body World to add Rigid Body Constraint to");
    return OPERATOR_CANCELLED;
  }
  /* Pinned objects could be from another scene. */
  if (!BKE_scene_object_find(*bmain, scene, ob)) {
    BKE_report(op->reports, RPT_ERROR, "No object in the scene to add Rigid Body Constraint to");
    return OPERATOR_CANCELLED;
  }
  /* apply to active object */
  changed = ED_rigidbody_constraint_add(bmain, scene, ob, type, op->reports);

  if (changed) {
    /* send updates */
    WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

    /* done */
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void RIGIDBODY_OT_constraint_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "RIGIDBODY_OT_constraint_add";
  ot->name = "Add Rigid Body Constraint";
  ot->description = "Add Rigid Body Constraint to active object";

  /* callbacks */
  ot->exec = rigidbody_con_add_exec;
  ot->poll = operator_rigidbody_con_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          rna_enum_rigidbody_constraint_type_items,
                          RBC_TYPE_FIXED,
                          "Rigid Body Constraint Type",
                          "");
}

/* ************ Remove Rigid Body Constraint ************** */

static wmOperatorStatus rigidbody_con_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = ed::object::context_active_object(C);
  /* Poll ensures. */
  BLI_assert(scene && ob && ob->rigidbody_constraint);

  /* Pinned objects could be from another scene. */
  if (!BKE_scene_object_find(*bmain, scene, ob)) {
    BKE_report(
        op->reports, RPT_ERROR, "No object in the scene to remove Rigid Body Constraint from");
    return OPERATOR_CANCELLED;
  }
  ED_rigidbody_constraint_remove(bmain, scene, ob);

  /* send updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  /* done */
  return OPERATOR_FINISHED;
}

void RIGIDBODY_OT_constraint_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "RIGIDBODY_OT_constraint_remove";
  ot->name = "Remove Rigid Body Constraint";
  ot->description = "Remove Rigid Body Constraint from Object";

  /* callbacks */
  ot->exec = rigidbody_con_remove_exec;
  ot->poll = operator_rigidbody_con_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender
