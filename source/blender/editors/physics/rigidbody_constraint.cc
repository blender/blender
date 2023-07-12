/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor_physics
 * \brief Rigid Body constraint editing operators
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_rigidbody.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_physics.h"
#include "ED_screen.h"

#include "physics_intern.h"

/* ********************************************** */
/* Helper API's for RigidBody Constraint Editing */

static bool operator_rigidbody_constraints_editable_poll(Scene *scene)
{
  if (scene == nullptr || ID_IS_LINKED(scene) || ID_IS_OVERRIDE_LIBRARY(scene) ||
      (scene->rigidbody_world != nullptr && scene->rigidbody_world->constraints != nullptr &&
       (ID_IS_LINKED(scene->rigidbody_world->constraints) ||
        ID_IS_OVERRIDE_LIBRARY(scene->rigidbody_world->constraints))))
  {
    return false;
  }
  return true;
}

static bool ED_operator_rigidbody_con_active_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (!operator_rigidbody_constraints_editable_poll(scene)) {
    return false;
  }

  if (ED_operator_object_active_editable(C)) {
    Object *ob = ED_object_active_context(C);
    return (ob && ob->rigidbody_constraint);
  }
  return false;
}

static bool ED_operator_rigidbody_con_add_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (!operator_rigidbody_constraints_editable_poll(scene)) {
    return false;
  }
  return ED_operator_object_active_editable(C);
}

bool ED_rigidbody_constraint_add(
    Main *bmain, Scene *scene, Object *ob, int type, ReportList *reports)
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
  DEG_id_tag_update(&rbw->constraints->id, ID_RECALC_COPY_ON_WRITE);

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

static int rigidbody_con_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  RigidBodyWorld *rbw = BKE_rigidbody_get_world(scene);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  int type = RNA_enum_get(op->ptr, "type");
  bool changed;

  /* sanity checks */
  if (ELEM(nullptr, scene, rbw)) {
    BKE_report(op->reports, RPT_ERROR, "No Rigid Body World to add Rigid Body Constraint to");
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
  ot->poll = ED_operator_rigidbody_con_add_poll;

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

static int rigidbody_con_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  /* apply to active object */
  if (ELEM(nullptr, ob, ob->rigidbody_constraint)) {
    BKE_report(op->reports, RPT_ERROR, "Object has no Rigid Body Constraint to remove");
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
  ot->poll = ED_operator_rigidbody_con_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
