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
 * 
 * Contributor(s): Blender Foundation, Andrea Weikert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_file/file_panels.c
 *  \ingroup spfile
 */


#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "file_intern.h"
#include "fsmenu.h"

#include <string.h>

static void file_panel_cb(bContext *C, void *arg_entry, void *UNUSED(arg_v))
{
	PointerRNA ptr;
	char *entry = (char *)arg_entry;

	WM_operator_properties_create(&ptr, "FILE_OT_select_bookmark");
	RNA_string_set(&ptr, "dir", entry);
	WM_operator_name_call(C, "FILE_OT_select_bookmark", WM_OP_INVOKE_REGION_WIN, &ptr);
	WM_operator_properties_free(&ptr);
}

static void file_panel_category(const bContext *C, Panel *pa, FSMenuCategory category, short *nr, int icon, int allow_delete, int reverse)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	uiBlock *block;
	uiBut *but;
	uiLayout *box, *col;
	struct FSMenu *fsmenu = fsmenu_get();
	int i, i_iter, nentries = fsmenu_get_nentries(fsmenu, category);

	/* reset each time */
	*nr = -1;

	/* hide if no entries */
	if (nentries == 0)
		return;

	/* layout */
	uiLayoutSetAlignment(pa->layout, UI_LAYOUT_ALIGN_LEFT);
	block = uiLayoutGetBlock(pa->layout);
	box = uiLayoutBox(pa->layout);
	col = uiLayoutColumn(box, TRUE);

	for (i_iter = 0; i_iter < nentries; ++i_iter) {
		char dir[FILE_MAX];
		char temp[FILE_MAX];
		uiLayout *layout = uiLayoutRow(col, FALSE);
		char *entry;

		i = reverse ? nentries - (i_iter + 1) : i_iter;
		
		entry = fsmenu_get_entry(fsmenu, category, i);
		
		/* set this list item as active if we have a match */
		if (sfile->params) {
			if (BLI_path_cmp(sfile->params->dir, entry) == 0) {
				*nr = i;
			}
		}

		/* create nice bookmark name, shows last directory in the full path currently */
		BLI_strncpy(temp, entry, FILE_MAX);
		BLI_add_slash(temp);
		BLI_getlastdir(temp, dir, FILE_MAX);
		BLI_del_slash(dir);

		if (dir[0] == 0)
			BLI_strncpy(dir, entry, FILE_MAX);

		/* create list item */
		but = uiDefIconTextButS(block, LISTROW, 0, icon, dir, 0, 0, UI_UNIT_X * 10, UI_UNIT_Y, nr, 0, i, 0, 0, entry);
		uiButSetFunc(but, file_panel_cb, entry, NULL);
		uiButSetFlag(but, UI_ICON_LEFT | UI_TEXT_LEFT);

		/* create delete button */
		if (allow_delete && fsmenu_can_save(fsmenu, category, i)) {
			uiBlockSetEmboss(block, UI_EMBOSSN);
			uiItemIntO(layout, "", ICON_X, "FILE_OT_delete_bookmark", "index", i);
			uiBlockSetEmboss(block, UI_EMBOSS);
		}
	}
}

static void file_panel_system(const bContext *C, Panel *pa)
{
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile)
		file_panel_category(C, pa, FS_CATEGORY_SYSTEM, &sfile->systemnr, ICON_DISK_DRIVE, 0, 0);
}

static void file_panel_bookmarks(const bContext *C, Panel *pa)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	uiLayout *row;

	if (sfile) {
		row = uiLayoutRow(pa->layout, FALSE);
		uiItemO(row, IFACE_("Add"), ICON_ZOOMIN, "file.bookmark_add");
		uiItemL(row, NULL, ICON_NONE);

		file_panel_category(C, pa, FS_CATEGORY_BOOKMARKS, &sfile->bookmarknr, ICON_BOOKMARKS, 1, 0);
	}
}

static void file_panel_recent(const bContext *C, Panel *pa)
{
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile) {
		if (!(U.uiflag & USER_HIDE_RECENT) ) {
			file_panel_category(C, pa, FS_CATEGORY_RECENT, &sfile->recentnr, ICON_FILE_FOLDER, 0, 1);
		}
	}
}


static int file_panel_operator_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	return (sfile && sfile->op);
}

static void file_panel_operator_header(const bContext *C, Panel *pa)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	wmOperator *op = sfile->op;

	BLI_strncpy(pa->drawname, RNA_struct_ui_name(op->type->srna), sizeof(pa->drawname));
}

static int file_panel_check_prop(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	const char *prop_id = RNA_property_identifier(prop);
	return !(strcmp(prop_id, "filepath") == 0 ||
	         strcmp(prop_id, "directory") == 0 ||
	         strcmp(prop_id, "filename") == 0
	         );
}

static void file_panel_operator(const bContext *C, Panel *pa)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	wmOperator *op = sfile->op;
	// int empty= 1, flag;
	
	uiBlockSetFunc(uiLayoutGetBlock(pa->layout), file_draw_check_cb, NULL, NULL);

	uiLayoutOperatorButs(C, pa->layout, op, file_panel_check_prop, '\0', UI_LAYOUT_OP_SHOW_EMPTY);

	uiBlockSetFunc(uiLayoutGetBlock(pa->layout), NULL, NULL, NULL);
}

void file_panels_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype file system directories");
	strcpy(pt->idname, "FILE_PT_system");
	strcpy(pt->label, N_("System"));
	pt->draw = file_panel_system;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype file bookmarks");
	strcpy(pt->idname, "FILE_PT_bookmarks");
	strcpy(pt->label, N_("Bookmarks"));
	pt->draw = file_panel_bookmarks;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype file recent directories");
	strcpy(pt->idname, "FILE_PT_recent");
	strcpy(pt->label, N_("Recent"));
	pt->draw = file_panel_recent;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype file operator properties");
	strcpy(pt->idname, "FILE_PT_operator");
	strcpy(pt->label, N_("Operator"));
	pt->poll = file_panel_operator_poll;
	pt->draw_header = file_panel_operator_header;
	pt->draw = file_panel_operator;
	BLI_addtail(&art->paneltypes, pt);
}

