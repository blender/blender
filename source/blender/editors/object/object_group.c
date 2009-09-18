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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "object_intern.h"

/********************* 3d view operators ***********************/

static int objects_add_active_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT, *obt;
	Group *group;
	int ok = 0;
	
	if(!ob) return OPERATOR_CANCELLED;
	
	/* linking to same group requires its own loop so we can avoid
	   looking up the active objects groups each time */

	for(group= G.main->group.first; group; group=group->id.next) {
		if(object_in_group(ob, group)) {
			/* Assign groups to selected objects */
			CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
				obt= base->object;
				add_to_group(group, obt);
				obt->flag |= OB_FROMGROUP;
				base->flag |= OB_FROMGROUP;
				base->object->recalc= OB_RECALC_OB;
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
	ot->description = "Add the object to an object group that contains the active object.";
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
	Object *ob= OBACT, *obt;
	Group *group;
	int ok = 0;
	
	if(!ob) return OPERATOR_CANCELLED;
	
	/* linking to same group requires its own loop so we can avoid
	   looking up the active objects groups each time */

	for(group= G.main->group.first; group; group=group->id.next) {
		if(object_in_group(ob, group)) {
			/* Assign groups to selected objects */
			CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
				obt= base->object;
				rem_from_group(group, obt);
				obt->flag &= ~OB_FROMGROUP;
				base->flag &= ~OB_FROMGROUP;
				base->object->recalc= OB_RECALC_OB;
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
	ot->description = "Remove the object from an object group that contains the active object.";
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
			rem_from_group(group, base->object);

		base->object->flag &= ~OB_FROMGROUP;
		base->flag &= ~OB_FROMGROUP;
		base->object->recalc= OB_RECALC_OB;
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
	ot->description = "Remove selected objects from all groups.";
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
	char gid[32]; //group id
	
	RNA_string_get(op->ptr, "GID", gid);
	
	group= add_group(gid);
		
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		add_to_group(group, base->object);
		base->object->flag |= OB_FROMGROUP;
		base->flag |= OB_FROMGROUP;
		base->object->recalc= OB_RECALC_OB;
	}
	CTX_DATA_END;

	DAG_scene_sort(scene);
	WM_event_add_notifier(C, NC_GROUP|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GROUP_OT_group_create(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Create New Group";
	ot->description = "Create an object group.";
	ot->idname= "GROUP_OT_group_create";
	
	/* api callbacks */
	ot->exec= group_create_exec;	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_string(ot->srna, "GID", "Group", 32, "Name", "Name of the new group");
}

/****************** properties window operators *********************/

static int group_add_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Base *base;
	Group *group;
	int value= RNA_enum_get(op->ptr, "group");

	if(!ob)
		return OPERATOR_CANCELLED;
	
	base= object_in_scene(ob, scene);
	if(!base)
		return OPERATOR_CANCELLED;
	
	if(value == -1)
		group= add_group( "Group" );
	else
		group= BLI_findlink(&bmain->group, value);

	if(group) {
		add_to_group(group, ob);
		ob->flag |= OB_FROMGROUP;
		base->flag |= OB_FROMGROUP;
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

static EnumPropertyItem group_items[]= {
	{-1, "ADD_NEW", 0, "Add New Group", ""},
	{0, NULL, 0, NULL, NULL}};

static EnumPropertyItem *group_itemf(bContext *C, PointerRNA *ptr, int *free)
{	
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item= NULL;
	Main *bmain;
	Group *group;
	int a, totitem= 0;
	
	if(!C) /* needed for docs */
		return group_items;
	
	RNA_enum_items_add_value(&item, &totitem, group_items, -1);

	bmain= CTX_data_main(C);
	if(bmain->group.first)
		RNA_enum_item_add_separator(&item, &totitem);

	for(a=0, group=bmain->group.first; group; group=group->id.next, a++) {
		tmp.value= a;
		tmp.identifier= group->id.name+2;
		tmp.name= group->id.name+2;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	RNA_enum_item_end(&item, &totitem);

	*free= 1;

	return item;
}

void OBJECT_OT_group_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Add Group";
	ot->idname= "OBJECT_OT_group_add";
	
	/* api callbacks */
	ot->exec= group_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	prop= RNA_def_enum(ot->srna, "group", group_items, -1, "Group", "Group to add object to.");
	RNA_def_enum_funcs(prop, group_itemf);
}

static int group_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Group *group= CTX_data_pointer_get_type(C, "group", &RNA_Group).data;
	Base *base;

	if(!ob || !group)
		return OPERATOR_CANCELLED;

	base= object_in_scene(ob, scene);
	if(!base)
		return OPERATOR_CANCELLED;

	rem_from_group(group, ob);

	if(find_group(ob, NULL) == NULL) {
		ob->flag &= ~OB_FROMGROUP;
		base->flag &= ~OB_FROMGROUP;
	}

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

