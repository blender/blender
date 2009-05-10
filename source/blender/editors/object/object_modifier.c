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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_modifier.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/********************* add modifier operator ********************/

static int modifier_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
    Object *ob = CTX_data_active_object(C);
	ModifierData *md;
	int type= RNA_enum_get(op->ptr, "type");
	ModifierTypeInfo *mti = modifierType_getInfo(type);

	if(mti->flags&eModifierTypeFlag_RequiresOriginalData) {
		md = ob->modifiers.first;

		while(md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform)
			md = md->next;

		BLI_insertlinkbefore(&ob->modifiers, md, modifier_new(type));
	}
	else
		BLI_addtail(&ob->modifiers, modifier_new(type));

	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_modifier_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Modifier";
	ot->description = "Add a modifier to the active object.";
	ot->idname= "OBJECT_OT_modifier_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= modifier_add_exec;
	
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* XXX only some types should be here */
	RNA_def_enum(ot->srna, "type", modifier_type_items, 0, "Type", "");
}

