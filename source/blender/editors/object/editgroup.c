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

/* XXX */
static void BIF_undo_push() {}
static void error() {}
static int pupmenu() {return 0;}
static int pupmenu_col() {return 0;}

void add_selected_to_act_ob_groups(Scene *scene, View3D *v3d)
{
	Object *ob= OBACT, *obt;
	Base *base;
	Group *group;
	
	if (!ob) return;
	
	/* linking to same group requires its own loop so we can avoid
	   looking up the active objects groups each time */

	group= G.main->group.first;
	while(group) {
		if(object_in_group(ob, group)) {
			/* Assign groups to selected objects */
			base= FIRSTBASE;
			while(base) {
				if(TESTBASE(v3d, base)) {
					obt= base->object;
					add_to_group(group, obt);
					obt->flag |= OB_FROMGROUP;
					base->flag |= OB_FROMGROUP;
				}
				base= base->next;
			}
		}
		group= group->id.next;
	}
	DAG_scene_sort(scene);
	BIF_undo_push("Add to Active Objects Group");
}

static int group_remove_exec(bContext *C, wmOperator *op)
{
	Group *group= NULL;
	Group *group_array[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int gid,i=0; //group id

	gid = RNA_int_get(op->ptr, "GID");
	
	/*remove from all groups*/
	if (gid == 26) {
		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			group = NULL;
			while( (group = find_group(base->object, group)) ) {
				rem_from_group(group, base->object);
			}
			base->object->flag &= ~OB_FROMGROUP;
			base->flag &= ~OB_FROMGROUP;
		}
		CTX_DATA_END;
	}
	else {		
		/* build array of the groups that are in menu*/
		for(group= G.main->group.first; group && i<24; group= group->id.next) {
			if(group->id.lib==NULL) {
				GroupObject *go;
				for(go= group->gobject.first; go; go= go->next) {
					if(go->ob->id.flag & LIB_DOIT) {
						group_array[i] = group;
						i++;
						break; /* Only want to know if this group should go in the list*/
					}
				}
			}
		}
	
		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			/* if we are removed and are not in any group, set our flag */
			if(rem_from_group(group, base->object) && find_group(base->object, NULL)==NULL) {
				base->object->flag &= ~OB_FROMGROUP;
				base->flag &= ~OB_FROMGROUP;
			}
		}
		CTX_DATA_END;
	}

	DAG_scene_sort(CTX_data_scene(C));
	ED_undo_push(C,"Remove From Group");

	WM_event_add_notifier(C, NC_SCENE, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;

}
static int group_remove_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Group *group= NULL;
	char *menutext= MEM_callocN(30+(22*22), "group rem menu"), *menupt;
	int i = 0;
	
	/* UnSet Tags for Objects and Groups */
	for(group= G.main->group.first; group; group= group->id.next) {
		if(group->id.lib==NULL) {
			group->id.flag &= ~LIB_DOIT;
		}
	}
	CTX_DATA_BEGIN(C, Object*, ob, visible_objects) {
		ob->id.flag &= ~LIB_DOIT;
	}
	CTX_DATA_END;
	
	/* Not tag selected objects */
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			base->object->id.flag |= LIB_DOIT;
	}
	CTX_DATA_END;
	
	menupt = menutext;
	
	menupt += sprintf(menupt, "Remove From %%t|");
	
	/* Build a list of groups that contain selected objects */
	for(group= G.main->group.first; group && i<24; group= group->id.next) {
		if(group->id.lib==NULL) {
			GroupObject *go;
			for(go= group->gobject.first; go; go= go->next) {
				if(go->ob->id.flag & LIB_DOIT) {
					menupt += sprintf(menupt, "|%s", group->id.name+2);
					i++;
					break; /* Only want to know if this group should go in the list*/
				}
			}
		}
	}
	menupt += sprintf(menupt, "|%s %%x%d", "ALL", 26);
	/* do we have any groups? */
	if (i = 0) error("Object selection contains no groups");
	else	uiPupmenuOperator(C, 0, op, "GID", menutext);
		
	MEM_freeN(menutext);
	return OPERATOR_RUNNING_MODAL;
}
void GROUP_OT_group_remove(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "remove Selected from group";
	ot->idname= "GROUP_OT_group_remove";
	
	/* api callbacks */
	ot->invoke = group_remove_invoke;
	ot->exec= group_remove_exec;	
	ot->poll= ED_operator_scene_editable;
	
	RNA_def_int(ot->srna, "GID", 0, INT_MIN, INT_MAX, "Group", "", INT_MIN, INT_MAX);
}
static int group_create_exec(bContext *C, wmOperator *op)
{
	Group *group= NULL;
	int gid; //group id
	
	gid = RNA_int_get(op->ptr, "GID");
	
	if(gid>0) group= BLI_findlink(&G.main->group, gid-1);
	else if (gid == 0 ) group= add_group( "Group" );
	else return OPERATOR_CANCELLED;
	
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		add_to_group(group, base->object);
		base->object->flag |= OB_FROMGROUP;
		base->flag |= OB_FROMGROUP;
		base->object->recalc= OB_RECALC_OB;
	}
	CTX_DATA_END;

	DAG_scene_sort(CTX_data_scene(C));
	ED_undo_push(C,"Add to Group");

	WM_event_add_notifier(C, NC_SCENE, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;

}
static int group_create_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Group *group= NULL;
	int tot= BLI_countlist(&G.main->group);
	char *strp= MEM_callocN(tot*32 + 32, "group menu"), *strp1;

	/* are there existing groups? */
	for(group= G.main->group.first; group; group= group->id.next)
		if(group->id.lib==NULL)
			break;
	strp1= strp;
	strp1 += sprintf(strp1, "Add To %%t|");
	
	strp1 += sprintf(strp1, "%s %%x%d|", "ADD NEW", 0);
	
	for(tot=1, group= G.main->group.first; group; group= group->id.next, tot++) {
		if(group->id.lib==NULL) {
			strp1 += sprintf(strp1, "%s %%x%d|", group->id.name+2, tot);
		}
	}
	uiPupmenuOperator(C, 0, op, "GID", strp);
	MEM_freeN(strp);

	return OPERATOR_RUNNING_MODAL;
}
void GROUP_OT_group_create(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Add Selected to group";
	ot->idname= "GROUP_OT_group_create";
	
	/* api callbacks */
	ot->invoke = group_create_invoke;
	ot->exec= group_create_exec;	
	ot->poll= ED_operator_scene_editable;
	
	RNA_def_int(ot->srna, "GID", 0, INT_MIN, INT_MAX, "Group", "", INT_MIN, INT_MAX);
}

