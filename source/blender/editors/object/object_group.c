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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_group.c
 *  \ingroup edobj
 */


#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_object.h"

#include "ED_screen.h"
#include "ED_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "object_intern.h"

/********************* 3d view operators ***********************/

static bool group_link_early_exit_check(Group *group, Object *object)
{
	GroupObject *group_object;

	for (group_object = group->gobject.first; group_object; group_object = group_object->next) {
		if (group_object->ob == object) {
			return true;
		}
	}

	return false;
}

static bool check_group_contains_object_recursive(Group *group, Object *object)
{
	GroupObject *group_object;

	if ((group->id.flag & LIB_DOIT) == 0) {
		/* Cycle already exists in groups, let's prevent further crappyness */
		return true;
	}

	group->id.flag &= ~LIB_DOIT;

	for (group_object = group->gobject.first; group_object; group_object = group_object->next) {
		Object *current_object = group_object->ob;

		if (current_object == object) {
			return true;
		}

		if (current_object->dup_group) {
			if (check_group_contains_object_recursive(current_object->dup_group, object)) {
				return true;
			}
		}
	}

	group->id.flag |= LIB_DOIT;

	return false;
}

/* can be called with C == NULL */
static EnumPropertyItem *group_object_active_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	Object *ob;
	EnumPropertyItem *item = NULL, item_tmp = {0};
	int totitem = 0;

	if (C == NULL) {
		return DummyRNA_NULL_items;
	}

	ob = ED_object_context(C);

	/* check that the object exists */
	if (ob) {
		Group *group;
		int i = 0, count = 0;

		/* if 2 or more groups, add option to add to all groups */
		group = NULL;
		while ((group = BKE_group_object_find(group, ob)))
			count++;

		if (count >= 2) {
			item_tmp.identifier = item_tmp.name = "All Groups";
			item_tmp.value = INT_MAX; /* this will give NULL on lookup */
			RNA_enum_item_add(&item, &totitem, &item_tmp);
			RNA_enum_item_add_separator(&item, &totitem);
		}

		/* add groups */
		group = NULL;
		while ((group = BKE_group_object_find(group, ob))) {
			item_tmp.identifier = item_tmp.name = group->id.name + 2;
			/* item_tmp.icon = ICON_ARMATURE_DATA; */
			item_tmp.value = i;
			RNA_enum_item_add(&item, &totitem, &item_tmp);
			i++;
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

/* get the group back from the enum index, quite awkward and UI specific */
static Group *group_object_active_find_index(Object *ob, const int group_object_index)
{
	Group *group = NULL;
	int i = 0;
	while ((group = BKE_group_object_find(group, ob))) {
		if (i == group_object_index) {
			break;
		}
		i++;
	}

	return group;
}

static int objects_add_active_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int single_group_index = RNA_enum_get(op->ptr, "group");
	Group *single_group = group_object_active_find_index(ob, single_group_index);
	Group *group;
	bool is_cycle = false;
	bool updated = false;

	if (ob == NULL)
		return OPERATOR_CANCELLED;

	/* now add all selected objects to the group(s) */
	for (group = bmain->group.first; group; group = group->id.next) {
		if (single_group && group != single_group)
			continue;
		if (!BKE_group_object_exists(group, ob))
			continue;

		/* for recursive check */
		BKE_main_id_tag_listbase(&bmain->group, true);

		CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
		{
			if (group_link_early_exit_check(group, base->object))
				continue;

			if (base->object->dup_group != group && !check_group_contains_object_recursive(group, base->object)) {
				BKE_group_object_add(group, base->object, scene, base);
				updated = true;
			}
			else {
				is_cycle = true;
			}
		}
		CTX_DATA_END;
	}

	if (is_cycle)
		BKE_report(op->reports, RPT_WARNING, "Skipped some groups because of cycle detected");

	if (!updated)
		return OPERATOR_CANCELLED;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GROUP_OT_objects_add_active(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Selected To Active Group";
	ot->description = "Add the object to an object group that contains the active object";
	ot->idname = "GROUP_OT_objects_add_active";
	
	/* api callbacks */
	ot->exec = objects_add_active_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "group", DummyRNA_NULL_items, 0, "Group", "The group to add other selected objects to");
	RNA_def_enum_funcs(prop, group_object_active_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

static int objects_remove_active_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;
	int single_group_index = RNA_enum_get(op->ptr, "group");
	Group *single_group = group_object_active_find_index(ob, single_group_index);
	Group *group;
	bool ok = false;
	
	if (ob == NULL)
		return OPERATOR_CANCELLED;
	
	/* linking to same group requires its own loop so we can avoid
	 * looking up the active objects groups each time */

	for (group = bmain->group.first; group; group = group->id.next) {
		if (single_group && group != single_group)
			continue;

		if (BKE_group_object_exists(group, ob)) {
			/* Remove groups from selected objects */
			CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
			{
				BKE_group_object_unlink(group, base->object, scene, base);
				ok = 1;
			}
			CTX_DATA_END;
		}
	}
	
	if (!ok)
		BKE_report(op->reports, RPT_ERROR, "Active object contains no groups");
	
	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GROUP_OT_objects_remove_active(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Remove Selected From Active Group";
	ot->description = "Remove the object from an object group that contains the active object";
	ot->idname = "GROUP_OT_objects_remove_active";
	
	/* api callbacks */
	ot->exec = objects_remove_active_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = ED_operator_objectmode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "group", DummyRNA_NULL_items, 0, "Group", "The group to remove other selected objects from");
	RNA_def_enum_funcs(prop, group_object_active_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

static int group_objects_remove_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		BKE_object_groups_clear(scene, base, base->object);
	}
	CTX_DATA_END;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GROUP_OT_objects_remove_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove From All Groups";
	ot->description = "Remove selected objects from all groups";
	ot->idname = "GROUP_OT_objects_remove_all";
	
	/* api callbacks */
	ot->exec = group_objects_remove_all_exec;
	ot->poll = ED_operator_objectmode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int group_objects_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int single_group_index = RNA_enum_get(op->ptr, "group");
	Group *single_group = group_object_active_find_index(ob, single_group_index);
	Group *group;
	bool updated = false;

	if (ob == NULL)
		return OPERATOR_CANCELLED;

	for (group = bmain->group.first; group; group = group->id.next) {
		if (single_group && group != single_group)
			continue;
		if (!BKE_group_object_exists(group, ob))
			continue;

		/* now remove all selected objects from the group */
		CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
		{
			BKE_group_object_unlink(group, base->object, scene, base);
			updated = true;
		}
		CTX_DATA_END;
	}

	if (!updated)
		return OPERATOR_CANCELLED;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GROUP_OT_objects_remove(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Remove From Group";
	ot->description = "Remove selected objects from a group";
	ot->idname = "GROUP_OT_objects_remove";

	/* api callbacks */
	ot->exec = group_objects_remove_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "group", DummyRNA_NULL_items, 0, "Group", "The group to remove this object from");
	RNA_def_enum_funcs(prop, group_object_active_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

static int group_create_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Group *group = NULL;
	char name[MAX_ID_NAME - 2]; /* id name */
	
	RNA_string_get(op->ptr, "name", name);
	
	group = BKE_group_add(bmain, name);
		
	CTX_DATA_BEGIN (C, Base *, base, selected_bases)
	{
		BKE_group_object_add(group, base->object, scene, base);
	}
	CTX_DATA_END;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GROUP_OT_create(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Create New Group";
	ot->description = "Create an object group from selected objects";
	ot->idname = "GROUP_OT_create";
	
	/* api callbacks */
	ot->exec = group_create_exec;
	ot->poll = ED_operator_objectmode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_string(ot->srna, "name", "Group", MAX_ID_NAME - 2, "Name", "Name of the new group");
}

/****************** properties window operators *********************/

static int group_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	Main *bmain = CTX_data_main(C);
	Group *group;

	if (ob == NULL)
		return OPERATOR_CANCELLED;

	group = BKE_group_add(bmain, "Group");
	BKE_group_object_add(group, ob, scene, NULL);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_group_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add to Group";
	ot->idname = "OBJECT_OT_group_add";
	ot->description = "Add an object to a new group";
	
	/* api callbacks */
	ot->exec = group_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int group_link_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	Group *group = BLI_findlink(&bmain->group, RNA_enum_get(op->ptr, "group"));

	if (ELEM(NULL, ob, group))
		return OPERATOR_CANCELLED;

	/* Early return check, if the object is already in group
	 * we could sckip all the dependency check and just consider
	 * operator is finished.
	 */
	if (group_link_early_exit_check(group, ob)) {
		return OPERATOR_FINISHED;
	}

	/* Adding object to group which is used as dupligroup for self is bad idea.
	 *
	 * It is also  bad idea to add object to group which is in group which
	 * contains our current object.
	 */
	BKE_main_id_tag_listbase(&bmain->group, true);
	if (ob->dup_group == group || check_group_contains_object_recursive(group, ob)) {
		BKE_report(op->reports, RPT_ERROR, "Could not add the group because of dependency cycle detected");
		return OPERATOR_CANCELLED;
	}

	BKE_group_object_add(group, ob, scene, NULL);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_group_link(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Link to Group";
	ot->idname = "OBJECT_OT_group_link";
	ot->description = "Add an object to an existing group";
	
	/* api callbacks */
	ot->exec = group_link_exec;
	ot->invoke = WM_enum_search_invoke;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "group", DummyRNA_NULL_items, 0, "Group", "");
	RNA_def_enum_funcs(prop, RNA_group_local_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

static int group_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	Group *group = CTX_data_pointer_get_type(C, "group", &RNA_Group).data;

	if (!ob || !group)
		return OPERATOR_CANCELLED;

	BKE_group_object_unlink(group, ob, scene, NULL); /* base will be used if found */

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_group_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Group";
	ot->idname = "OBJECT_OT_group_remove";
	ot->description = "Remove the active object from this group";
	
	/* api callbacks */
	ot->exec = group_remove_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

