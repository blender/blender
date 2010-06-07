/**
 * $Id$
 *
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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "object_intern.h"

/********************* 3d view operators ***********************/

static int objects_add_active_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;
	Group *group;
	int ok = 0;
	
	if(!ob) return OPERATOR_CANCELLED;
	
	/* linking to same group requires its own loop so we can avoid
	   looking up the active objects groups each time */

	for(group= G.main->group.first; group; group=group->id.next) {
		if(object_in_group(ob, group)) {
			/* Assign groups to selected objects */
			CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
				add_to_group(group, base->object, scene, base);
				ok = 1;
			}
			CTX_DATA_END;
		}
	}
	
	if(!ok) BKE_report(op->reports, RPT_ERROR, "Active Object contains no groups");
	
	DAG_scene_sort(scene);
	WM_event_add_notifier(C, NC_GROUP|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GROUP_OT_objects_add_active(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Selected To Active Group";
	ot->description = "Add the object to an object group that contains the active object";
	ot->idname= "GROUP_OT_objects_add_active";
	
	/* api callbacks */
	ot->exec= objects_add_active_exec;	
	ot->poll= ED_operator_scene_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int objects_remove_active_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;
	Group *group;
	int ok = 0;
	
	if(!ob) return OPERATOR_CANCELLED;
	
	/* linking to same group requires its own loop so we can avoid
	   looking up the active objects groups each time */

	for(group= G.main->group.first; group; group=group->id.next) {
		if(object_in_group(ob, group)) {
			/* Assign groups to selected objects */
			CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
				rem_from_group(group, base->object, scene, base);
				ok = 1;
			}
			CTX_DATA_END;
		}
	}
	
	if(!ok) BKE_report(op->reports, RPT_ERROR, "Active Object contains no groups");
	
	DAG_scene_sort(scene);
	WM_event_add_notifier(C, NC_GROUP|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GROUP_OT_objects_remove_active(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Selected From Active Group";
	ot->description = "Remove the object from an object group that contains the active object";
	ot->idname= "GROUP_OT_objects_remove_active";
	
	/* api callbacks */
	ot->exec= objects_remove_active_exec;	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int group_objects_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Group *group= NULL;

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		group = NULL;
		while((group = find_group(base->object, group)))
			rem_from_group(group, base->object, scene, base);
	}
	CTX_DATA_END;

	DAG_scene_sort(scene);
	WM_event_add_notifier(C, NC_GROUP|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GROUP_OT_objects_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove From Groups";
	ot->description = "Remove selected objects from all groups";
	ot->idname= "GROUP_OT_objects_remove";
	
	/* api callbacks */
	ot->exec= group_objects_remove_exec;	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int group_create_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Group *group= NULL;
	char name[32]; /* id name */
	
	RNA_string_get(op->ptr, "name", name);
	
	group= add_group(name);
		
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		add_to_group(group, base->object, scene, base);
	}
	CTX_DATA_END;

	DAG_scene_sort(scene);
	WM_event_add_notifier(C, NC_GROUP|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GROUP_OT_create(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Create New Group";
	ot->description = "Create an object group from selected objects";
	ot->idname= "GROUP_OT_create";
	
	/* api callbacks */
	ot->exec= group_create_exec;	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_string(ot->srna, "name", "Group", 32, "Name", "Name of the new group");
}

/****************** properties window operators *********************/

static int group_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Group *group;

	if(ob == NULL)
		return OPERATOR_CANCELLED;

    group= add_group("Group");
    add_to_group(group, ob, scene, NULL);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_group_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add to Group";
	ot->idname= "OBJECT_OT_group_add";
	ot->description = "Add an object to a new group";
	
	/* api callbacks */
	ot->exec= group_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int group_link_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
    Group *group= BLI_findlink(&CTX_data_main(C)->group, RNA_enum_get(op->ptr, "group"));

	if(ELEM(NULL, ob, group))
		return OPERATOR_CANCELLED;

    add_to_group(group, ob, scene, NULL);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_group_link(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Link to Group";
	ot->idname= "OBJECT_OT_group_link";
	ot->description = "Add an object to an existing group";
	
	/* api callbacks */
	ot->exec= group_link_exec;
	ot->invoke= WM_enum_search_invoke;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	prop= RNA_def_enum(ot->srna, "group", DummyRNA_NULL_items, 0, "Group", "");
	RNA_def_enum_funcs(prop, RNA_group_local_itemf);
	ot->prop= prop;
}

static int group_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Group *group= CTX_data_pointer_get_type(C, "group", &RNA_Group).data;

	if(!ob || !group)
		return OPERATOR_CANCELLED;

	rem_from_group(group, ob, scene, NULL); /* base will be used if found */

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_group_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Group";
	ot->idname= "OBJECT_OT_group_remove";
	
	/* api callbacks */
	ot->exec= group_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

