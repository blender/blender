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
#include "BLI_arithb.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_depsgraph.h"
#include "BKE_group.h"
#include "BKE_global.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "ED_view3d.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "object_intern.h"

static int objects_add_active_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT, *obt;
	Group *group;
	int ok = 0;
	
	if (!ob) return OPERATOR_CANCELLED;
	
	/* linking to same group requires its own loop so we can avoid
	   looking up the active objects groups each time */

	group= G.main->group.first;
	while(group) {
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
		group= group->id.next;
	}
	
	if (!ok) BKE_report(op->reports, RPT_ERROR, "Active Object contains no groups");
	
	DAG_scene_sort(CTX_data_scene(C));

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
	
	if (!ob) return OPERATOR_CANCELLED;
	
	/* linking to same group requires its own loop so we can avoid
	   looking up the active objects groups each time */

	group= G.main->group.first;
	while(group) {
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
		group= group->id.next;
	}
	
	if (!ok) BKE_report(op->reports, RPT_ERROR, "Active Object contains no groups");
	
	DAG_scene_sort(CTX_data_scene(C));

	WM_event_add_notifier(C, NC_GROUP|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;

}

void GROUP_OT_objects_remove_active(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Remove Selected From active group";
	ot->description = "Remove the object from an object group that contains the active object.";
	ot->idname= "GROUP_OT_objects_remove_active";
	
	/* api callbacks */
	ot->exec= objects_remove_active_exec;	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int group_remove_exec(bContext *C, wmOperator *op)
{
	Group *group= NULL;

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		group = NULL;
		while( (group = find_group(base->object, group)) ) {
			rem_from_group(group, base->object);
		}
		base->object->flag &= ~OB_FROMGROUP;
		base->flag &= ~OB_FROMGROUP;
		base->object->recalc= OB_RECALC_OB;
	}
	CTX_DATA_END;

	DAG_scene_sort(CTX_data_scene(C));

	WM_event_add_notifier(C, NC_GROUP|NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;

}

void GROUP_OT_group_remove(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "remove Selected from group";
	ot->description = "remove an object from the group.";
	ot->idname= "GROUP_OT_group_remove";
	
	/* api callbacks */
	ot->exec= group_remove_exec;	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int group_create_exec(bContext *C, wmOperator *op)
{
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

	DAG_scene_sort(CTX_data_scene(C));

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

