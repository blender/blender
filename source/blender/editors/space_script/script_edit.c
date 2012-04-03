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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_script/script_edit.c
 *  \ingroup spscript
 */


#include <string.h>
#include <stdio.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_screen.h"


#include "script_intern.h"	// own include

#ifdef WITH_PYTHON
#include "BPY_extern.h" /* BPY_script_exec */
#endif

static int run_pyfile_exec(bContext *C, wmOperator *op)
{
	char path[512];
	RNA_string_get(op->ptr, "filepath", path);
#ifdef WITH_PYTHON
	if (BPY_filepath_exec(C, path, op->reports)) {
		ARegion *ar= CTX_wm_region(C);
		ED_region_tag_redraw(ar);
		return OPERATOR_FINISHED;
	}
#else
	(void)C; /* unused */
#endif
	return OPERATOR_CANCELLED; /* FAIL */
}

void SCRIPT_OT_python_file_run(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Run python file";
	ot->description = "Run Python file";
	ot->idname = "SCRIPT_OT_python_file_run";
	ot->flag = OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = run_pyfile_exec;
	ot->poll = ED_operator_areaactive;

	RNA_def_string_file_path(ot->srna, "filepath", "", FILE_MAX, "Path", "");
}


static int script_reload_exec(bContext *C, wmOperator *UNUSED(op))
{
#ifdef WITH_PYTHON
	/* TODO, this crashes on netrender and keying sets, need to look into why
	 * disable for now unless running in debug mode */
	WM_cursor_wait(1);
	BPY_string_exec(C, "__import__('bpy').utils.load_scripts(reload_scripts=True)");
	WM_cursor_wait(0);
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;
#else
	(void)C; /* unused */
	return OPERATOR_CANCELLED;
#endif
}

void SCRIPT_OT_reload(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reload Scripts";
	ot->description = "Reload Scripts";
	ot->idname = "SCRIPT_OT_reload";

	/* api callbacks */
	ot->exec = script_reload_exec;
}
