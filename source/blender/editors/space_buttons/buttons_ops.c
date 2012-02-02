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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_buttons/buttons_ops.c
 *  \ingroup spbuttons
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_util.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "buttons_intern.h"	// own include

/********************** toolbox operator *********************/

static int toolbox_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *UNUSED(event))
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	PointerRNA ptr;
	uiPopupMenu *pup;
	uiLayout *layout;

	RNA_pointer_create(&sc->id, &RNA_SpaceProperties, sbuts, &ptr);

	pup= uiPupMenuBegin(C, "Align", ICON_NONE);
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
	ot->poll= ED_operator_buttons_active;
}

/********************** filebrowse operator *********************/

typedef struct FileBrowseOp {
	PointerRNA ptr;
	PropertyRNA *prop;
} FileBrowseOp;

static int file_browse_exec(bContext *C, wmOperator *op)
{
	FileBrowseOp *fbo= op->customdata;
	ID *id;
	char *str, path[FILE_MAX];
	const char *path_prop= RNA_struct_find_property(op->ptr, "directory") ? "directory" : "filepath";
	
	if (RNA_struct_property_is_set(op->ptr, path_prop)==0 || fbo==NULL)
		return OPERATOR_CANCELLED;
	
	str= RNA_string_get_alloc(op->ptr, path_prop, NULL, 0);

	/* add slash for directories, important for some properties */
	if(RNA_property_subtype(fbo->prop) == PROP_DIRPATH) {
		char name[FILE_MAX];
		
		id = fbo->ptr.id.data;

		BLI_strncpy(path, str, FILE_MAX);
		BLI_path_abs(path, id ? ID_BLEND_PATH(G.main, id) : G.main->name);
		
		if(BLI_is_dir(path)) {
			str = MEM_reallocN(str, strlen(str)+2);
			BLI_add_slash(str);
		}
		else
			BLI_splitdirstring(str, name);
	}

	RNA_property_string_set(&fbo->ptr, fbo->prop, str);
	RNA_property_update(C, &fbo->ptr, fbo->prop);
	MEM_freeN(str);


	/* special, annoying exception, filesel on redo panel [#26618] */
	{
		wmOperator *redo_op= WM_operator_last_redo(C);
		if(redo_op) {
			if(fbo->ptr.data == redo_op->ptr->data) {
				ED_undo_operator_repeat(C, redo_op);
			}
		}
	}

	MEM_freeN(op->customdata);

	return OPERATOR_FINISHED;
}

static int file_browse_cancel(bContext *UNUSED(C), wmOperator *op)
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

	if (CTX_wm_space_file(C)) {
		BKE_report(op->reports, RPT_ERROR, "Can't activate a file selector, one already open");
		return OPERATOR_CANCELLED;
	}

	uiFileBrowseContextProperty(C, &ptr, &prop);

	if(!prop)
		return OPERATOR_CANCELLED;

	str= RNA_property_string_get_alloc(&ptr, prop, NULL, 0, NULL);

	/* useful yet irritating feature, Shift+Click to open the file
	 * Alt+Click to browse a folder in the OS's browser */
	if(event->shift || event->alt) {
		PointerRNA props_ptr;

		if(event->alt) {
			char *lslash= BLI_last_slash(str);
			if(lslash)
				*lslash= '\0';
		}


		WM_operator_properties_create(&props_ptr, "WM_OT_path_open");
		RNA_string_set(&props_ptr, "filepath", str);
		WM_operator_name_call(C, "WM_OT_path_open", WM_OP_EXEC_DEFAULT, &props_ptr);
		WM_operator_properties_free(&props_ptr);

		MEM_freeN(str);
		return OPERATOR_CANCELLED;
	}
	else {
		const char *path_prop= RNA_struct_find_property(op->ptr, "directory") ? "directory" : "filepath";
		fbo= MEM_callocN(sizeof(FileBrowseOp), "FileBrowseOp");
		fbo->ptr= ptr;
		fbo->prop= prop;
		op->customdata= fbo;

		RNA_string_set(op->ptr, path_prop, str);
		MEM_freeN(str);

		/* normally ED_fileselect_get_params would handle this but we need to because of stupid
		 * user-prefs exception - campbell */
		if(RNA_struct_find_property(op->ptr, "relative_path")) {
			if(!RNA_struct_property_is_set(op->ptr, "relative_path")) {
				/* annoying exception!, if were dealign with the user prefs, default relative to be off */
				RNA_boolean_set(op->ptr, "relative_path", U.flag & USER_RELPATHS && (ptr.data != &U));
			}
		}
		WM_event_add_fileselect(C, op);

		return OPERATOR_RUNNING_MODAL;
	}
}

void BUTTONS_OT_file_browse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Accept";
	ot->description="Open a file browser, Hold Shift to open the file, Alt to browse containing directory";
	ot->idname= "BUTTONS_OT_file_browse";
	
	/* api callbacks */
	ot->invoke= file_browse_invoke;
	ot->exec= file_browse_exec;
	ot->cancel= file_browse_cancel;

	/* properties */
	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH|WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
}

/* second operator, only difference from BUTTONS_OT_file_browse is WM_FILESEL_DIRECTORY */
void BUTTONS_OT_directory_browse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Accept";
	ot->description="Open a directory browser, Hold Shift to open the file, Alt to browse containing directory";
	ot->idname= "BUTTONS_OT_directory_browse";

	/* api callbacks */
	ot->invoke= file_browse_invoke;
	ot->exec= file_browse_exec;
	ot->cancel= file_browse_cancel;

	/* properties */
	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_DIRECTORY|WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
}
