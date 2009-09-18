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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_packedFile_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_bpath.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_imbuf_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"

#include "info_intern.h"

/********************* pack all operator *********************/

static int pack_all_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);

	packAll(bmain, op->reports);
	G.fileflags |= G_AUTOPACK;

	return OPERATOR_FINISHED;
}

static int pack_all_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Main *bmain= CTX_data_main(C);
	Image *ima;
	ImBuf *ibuf;

	// first check for dirty images
	for(ima=bmain->image.first; ima; ima=ima->id.next) {
		if(ima->ibufs.first) { /* XXX FIX */
			ibuf= BKE_image_get_ibuf(ima, NULL);
			
			if(ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
				break;
		}
	}

	if(ima) {
		uiPupMenuOkee(C, "FILE_OT_pack_all", "Some images are painted on. These changes will be lost. Continue?");
		return OPERATOR_CANCELLED;
	}

	return pack_all_exec(C, op);
}

void FILE_OT_pack_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Pack All";
	ot->idname= "FILE_OT_pack_all";
	
	/* api callbacks */
	ot->exec= pack_all_exec;
	ot->invoke= pack_all_invoke;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************* unpack all operator *********************/

static const EnumPropertyItem unpack_all_method_items[] = {
	{PF_USE_LOCAL, "USE_LOCAL", 0, "Use files in current directory (create when necessary)", ""},
	{PF_WRITE_LOCAL, "WRITE_LOCAL", 0, "Write files to current directory (overwrite existing files)", ""},
	{PF_USE_ORIGINAL, "USE_ORIGINAL", 0, "Use files in original location (create when necessary)", ""},
	{PF_WRITE_ORIGINAL, "WRITE_ORIGINAL", 0, "Write files to original location (overwrite existing files)", ""},
	{PF_KEEP, "KEEP", 0, "Disable AutoPack, keep all packed files", ""},
	{PF_ASK, "ASK", 0, "Ask for each file", ""},
	{0, NULL, 0, NULL, NULL}};

static int unpack_all_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	int method= RNA_enum_get(op->ptr, "method");

	if(method != PF_KEEP) unpackAll(bmain, op->reports, method); /* XXX PF_ASK can't work here */
	G.fileflags &= ~G_AUTOPACK;

	return OPERATOR_FINISHED;
}

static int unpack_all_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Main *bmain= CTX_data_main(C);
	uiPopupMenu *pup;
	uiLayout *layout;
	char title[128];
	int count = 0;
	
	count = countPackedFiles(bmain);
	
	if(!count) {
		BKE_report(op->reports, RPT_WARNING, "No packed files. Autopack disabled.");
		G.fileflags &= ~G_AUTOPACK;
		return OPERATOR_CANCELLED;
	}

	if(count == 1)
		sprintf(title, "Unpack 1 file");
	else
		sprintf(title, "Unpack %d files", count);
	
	pup= uiPupMenuBegin(C, title, 0);
	layout= uiPupMenuLayout(pup);

	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemsEnumO(layout, "FILE_OT_unpack_all", "method");

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void FILE_OT_unpack_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Unpack All";
	ot->idname= "FILE_OT_unpack_all";
	
	/* api callbacks */
	ot->exec= unpack_all_exec;
	ot->invoke= unpack_all_invoke;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "method", unpack_all_method_items, PF_USE_LOCAL, "Method", "How to unpack.");
}

/********************* make paths relative operator *********************/

static int make_paths_relative_exec(bContext *C, wmOperator *op)
{
	char txtname[24]; /* text block name */
	int tot, changed, failed, linked;

	if(!G.relbase_valid) {
		BKE_report(op->reports, RPT_WARNING, "Can't set relative paths with an unsaved blend file.");
		return OPERATOR_CANCELLED;
	}

	txtname[0] = '\0';
	makeFilesRelative(txtname, &tot, &changed, &failed, &linked);

	if(failed)
		BKE_reportf(op->reports, RPT_ERROR, "Total files %i|Changed %i|Failed %i, See Text \"%s\"|Linked %i", tot, changed, failed, txtname, linked); 
	else
		BKE_reportf(op->reports, RPT_INFO, "Total files %i|Changed %i|Failed %i|Linked %i", tot, changed, failed, linked);

	return OPERATOR_FINISHED;
}

void FILE_OT_make_paths_relative(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make All Paths Relative";
	ot->idname= "FILE_OT_make_paths_relative";
	
	/* api callbacks */
	ot->exec= make_paths_relative_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************* make paths absolute operator *********************/

static int make_paths_absolute_exec(bContext *C, wmOperator *op)
{
	char txtname[24]; /* text block name */
	int tot, changed, failed, linked;

	if(!G.relbase_valid) {
		BKE_report(op->reports, RPT_WARNING, "Can't set absolute paths with an unsaved blend file.");
		return OPERATOR_CANCELLED;
	}

	txtname[0] = '\0';
	makeFilesAbsolute(txtname, &tot, &changed, &failed, &linked);

	if(failed)
		BKE_reportf(op->reports, RPT_ERROR, "Total files %i|Changed %i|Failed %i, See Text \"%s\"|Linked %i", tot, changed, failed, txtname, linked); 
	else
		BKE_reportf(op->reports, RPT_INFO, "Total files %i|Changed %i|Failed %i|Linked %i", tot, changed, failed, linked);

	return OPERATOR_FINISHED;
}

void FILE_OT_make_paths_absolute(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make All Paths Absolute";
	ot->idname= "FILE_OT_make_paths_absolute";
	
	/* api callbacks */
	ot->exec= make_paths_absolute_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************* report missing files operator *********************/

static int report_missing_files_exec(bContext *C, wmOperator *op)
{
	char txtname[24]; /* text block name */

	txtname[0] = '\0';
	
	/* run the missing file check */
	checkMissingFiles(txtname);
	
	if(txtname[0] == '\0')
		BKE_report(op->reports, RPT_INFO, "No external files missing.");
	else
		BKE_reportf(op->reports, RPT_ERROR, "Missing files listed in Text \"%s\"", txtname);
	
	return OPERATOR_FINISHED;
}

void FILE_OT_report_missing_files(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Report Missing Files...";
	ot->idname= "FILE_OT_report_missing_files";
	
	/* api callbacks */
	ot->exec= report_missing_files_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************* find missing files operator *********************/

static int find_missing_files_exec(bContext *C, wmOperator *op)
{
	char *path;
	
	path= RNA_string_get_alloc(op->ptr, "path", NULL, 0);
	findMissingFiles(path);
	MEM_freeN(path);

	return OPERATOR_FINISHED;
}

static int find_missing_files_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* XXX file open button text "Find Missing Files" */
	WM_event_add_fileselect(C, op); 
	return OPERATOR_RUNNING_MODAL;
}

void FILE_OT_find_missing_files(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Find Missing Files...";
	ot->idname= "FILE_OT_find_missing_files";
	
	/* api callbacks */
	ot->exec= find_missing_files_exec;
	ot->invoke= find_missing_files_invoke;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL);
}

#if 0
static void info_filemenu(bContext *C, uiLayout *layout, void *arg_unused)
{
	
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_AREA);
	uiItemO(layout, NULL, 0, "WM_OT_read_homefile"); 
	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_AREA);
	uiItemO(layout, NULL, 0, "WM_OT_open_mainfile"); 
//	uiDefIconTextBlockBut(block, info_openrecentmenu, NULL, ICON_RIGHTARROW_THIN, "Open Recent",0, yco-=20, 120, 19, "");
//	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Recover Last Session",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
	
	uiItemS(layout);
	
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_AREA);
	uiItemO(layout, NULL, 0, "WM_OT_save_mainfile"); 
	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_AREA);
	uiItemO(layout, NULL, 0, "WM_OT_save_as_mainfile"); 

#if 0
	if(U.flag & USER_FILECOMPRESS) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Compress File",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 35, "Enable file compression");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Compress File",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 35, "Enable file compression");
	}
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save Rendered Image...|F3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Screenshot Subwindow|Ctrl F3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 24, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Screenshot All|Ctrl Shift F3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 25, "");
#if GAMEBLENDER == 1
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save Game As Runtime...",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 22, "");
#endif
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save Default Settings|Ctrl U",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 31, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Load Factory Settings",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 32, "");
	
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Append or Link|Shift F1",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Append or Link (Image Browser)|Ctrl F1",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
//	uiDefIconTextBlockBut(block, info_file_importmenu, NULL, ICON_RIGHTARROW_THIN, "Import", 0, yco-=20, menuwidth, 19, "");
//	uiDefIconTextBlockBut(block, info_file_exportmenu, NULL, ICON_RIGHTARROW_THIN, "Export", 0, yco-=20, menuwidth, 19, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, info_externalfiles, NULL, ICON_RIGHTARROW_THIN, "External Data",0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Quit Blender|Ctrl Q",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
	uiBlockSetDirection(block, UI_DOWN);
	uiTextBoundsBlock(block, 80);
	
	uiEndBlock(C, block);
	return block;
#endif
}
#endif

