/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2013 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Sergej Reich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file rigidbody_constraint.c
 *  \ingroup editor_physics
 *  \brief Rigid Body constraint editing operators
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_rigidbody.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_physics.h"
#include "ED_screen.h"

#include "physics_intern.h"

/* ********************************************** */
/* Helper API's for RigidBody Constraint Editing */

static int ED_operator_rigidbody_con_active_poll(bContext *C)
{
	if (ED_operator_object_active_editable(C)) {
		Object *ob = CTX_data_active_object(C);
		return (ob && ob->rigidbody_constraint);
	}
	else
		return 0;
}


bool ED_rigidbody_constraint_add(Scene *scene, Object *ob, int type, ReportList *reports)
{
	RigidBodyWorld *rbw = BKE_rigidbody_get_world(scene);

	/* check that object doesn't already have a constraint */
	if (ob->rigidbody_constraint) {
		BKE_reportf(reports, RPT_INFO, "Object '%s' already has a Rigid Body Constraint", ob->id.name + 2);
		return false;
	}
	/* create constraint group if it doesn't already exits */
	if (rbw->constraints == NULL) {
		rbw->constraints = BKE_group_add(G.main, "RigidBodyConstraints");
	}
	/* make rigidbody constraint settings */
	ob->rigidbody_constraint = BKE_rigidbody_create_constraint(scene, ob, type);
	ob->rigidbody_constraint->flag |= RBC_FLAG_NEEDS_VALIDATE;

	/* add constraint to rigid body constraint group */
	BKE_group_object_add(rbw->constraints, ob, scene, NULL);

	DAG_id_tag_update(&ob->id, OB_RECALC_OB);
	return true;
}

void ED_rigidbody_constraint_remove(Scene *scene, Object *ob)
{
	RigidBodyWorld *rbw = BKE_rigidbody_get_world(scene);

	BKE_rigidbody_remove_constraint(scene, ob);
	if (rbw)
		BKE_group_object_unlink(rbw->constraints, ob, scene, NULL);

	DAG_id_tag_update(&ob->id, OB_RECALC_OB);
}

/* ********************************************** */
/* Active Object Add/Remove Operators */

/* ************ Add Rigid Body Constraint ************** */

static int rigidbody_con_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	RigidBodyWorld *rbw = BKE_rigidbody_get_world(scene);
	Object *ob = (scene) ? OBACT : NULL;
	int type = RNA_enum_get(op->ptr, "type");
	bool changed;

	/* sanity checks */
	if (ELEM(NULL, scene, rbw)) {
		BKE_report(op->reports, RPT_ERROR, "No Rigid Body World to add Rigid Body Constraint to");
		return OPERATOR_CANCELLED;
	}
	/* apply to active object */
	changed = ED_rigidbody_constraint_add(scene, ob, type, op->reports);

	if (changed) {
		/* send updates */
		WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

		/* done */
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void RIGIDBODY_OT_constraint_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "RIGIDBODY_OT_constraint_add";
	ot->name = "Add Rigid Body Constraint";
	ot->description = "Add Rigid Body Constraint to active object";

	/* callbacks */
	ot->exec = rigidbody_con_add_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", rigidbody_constraint_type_items, RBC_TYPE_FIXED, "Rigid Body Constraint Type", "");
}

/* ************ Remove Rigid Body Constraint ************** */

static int rigidbody_con_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = (scene) ? OBACT : NULL;

	/* sanity checks */
	if (scene == NULL)
		return OPERATOR_CANCELLED;

	/* apply to active object */
	if (ELEM(NULL, ob, ob->rigidbody_constraint)) {
		BKE_report(op->reports, RPT_ERROR, "Object has no Rigid Body Constraint to remove");
		return OPERATOR_CANCELLED;
	}
	else {
		ED_rigidbody_constraint_remove(scene, ob);
	}

	/* send updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

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
