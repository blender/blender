/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "buttons_intern.h"	// own include

/********************** toolbox operator *********************/

static int toolbox_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	PointerRNA ptr;
	uiPopupMenu *pup;
	uiLayout *layout;

	RNA_pointer_create(&sc->id, &RNA_SpaceProperties, sbuts, &ptr);

	pup= uiPupMenuBegin(C, "Align", 0);
	layout= uiPupMenuLayout(pup);
	uiItemsEnumR(layout, &ptr, "align");
	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void BUTTONS_OT_toolbox(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toolbox";
	ot->description="Display button panel toolbox";
	ot->idname= "BUTTONS_OT_toolbox";
	
	/* api callbacks */
	ot->invoke= toolbox_invoke;
}

/********************** filebrowse operator *********************/

typedef struct FileBrowseOp {
	PointerRNA ptr;
	PropertyRNA *prop;
} FileBrowseOp;

static int file_browse_exec(bContext *C, wmOperator *op)
{
	FileBrowseOp *fbo= op->customdata;
	char *str;
	
	if (RNA_property_is_set(op->ptr, "path")==0 || fbo==NULL)
		return OPERATOR_CANCELLED;
	
	str= RNA_string_get_alloc(op->ptr, "path", 0, 0);
	RNA_property_string_set(&fbo->ptr, fbo->prop, str);
	RNA_property_update(C, &fbo->ptr, fbo->prop);
	MEM_freeN(str);

	MEM_freeN(op->customdata);
	return OPERATOR_FINISHED;
}

static int file_browse_cancel(bContext *C, wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;

	return OPERATOR_CANCELLED;
}

static int file_browse_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	FileBrowseOp *fbo;
	char *str;

	uiFileBrowseContextProperty(C, &ptr, &prop);

	if(!prop)
		return OPERATOR_CANCELLED;
	
	fbo= MEM_callocN(sizeof(FileBrowseOp), "FileBrowseOp");
	fbo->ptr= ptr;
	fbo->prop= prop;
	op->customdata= fbo;

	str= RNA_property_string_get_alloc(&ptr, prop, 0, 0);
	RNA_string_set(op->ptr, "path", str);
	MEM_freeN(str);

	WM_event_add_fileselect(C, op); 
	
	return OPERATOR_RUNNING_MODAL;
}

void BUTTONS_OT_file_browse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "File Browse";
	ot->description="Open a file browser.";
	ot->idname= "BUTTONS_OT_file_browse";
	
	/* api callbacks */
	ot->invoke= file_browse_invoke;
	ot->exec= file_browse_exec;
	ot->cancel= file_browse_cancel;

	/* properties */
	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL);
}

