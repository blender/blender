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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Andrea Weikert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLI_blenlib.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "file_intern.h"
#include "fsmenu.h"

#include <string.h>

static void do_file_panel_events(bContext *C, void *arg, int event)
{

}

static void file_panel_category(const bContext *C, Panel *pa, FSMenuCategory category, int icon, int allow_delete)
{
	uiBlock *block;
	struct FSMenu* fsmenu = fsmenu_get();
	int nentries = fsmenu_get_nentries(fsmenu, category);
	int i;

	uiLayoutSetAlignment(pa->layout, UI_LAYOUT_ALIGN_LEFT);
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_file_panel_events, NULL);
	uiBlockSetEmboss(block, UI_EMBOSSP);
	uiBlockBeginAlign(block);
	for (i=0; i< nentries;++i) {
		char dir[FILE_MAX];
		char temp[FILE_MAX];
		uiLayout* layout = uiLayoutRow(pa->layout, UI_LAYOUT_ALIGN_LEFT);
		char *entry = fsmenu_get_entry(fsmenu, category, i);

		/* create nice bookmark name, shows last directory in the full path currently */
		BLI_strncpy(temp, entry, FILE_MAX);
		BLI_add_slash(temp);
		BLI_getlastdir(temp, dir, FILE_MAX);
		BLI_del_slash(dir);

		/* operator shows the short bookmark name, should eventually have tooltip */
		uiItemStringO(layout, dir, icon, "FILE_OT_select_bookmark", "dir", entry);
		if (allow_delete && fsmenu_can_save(fsmenu, category, i) )
			uiItemIntO(layout, "", ICON_X, "FILE_OT_delete_bookmark", "index", i);
	}
	uiBlockEndAlign(block);
}

static void file_panel_system(const bContext *C, Panel *pa)
{
	file_panel_category(C, pa, FS_CATEGORY_SYSTEM, ICON_DISK_DRIVE, 0);
}

static void file_panel_bookmarks(const bContext *C, Panel *pa)
{
	file_panel_category(C, pa, FS_CATEGORY_BOOKMARKS, ICON_BOOKMARKS, 1);
}


static void file_panel_recent(const bContext *C, Panel *pa)
{
	file_panel_category(C, pa, FS_CATEGORY_RECENT, ICON_FILE_FOLDER, 0);
}


static void file_panel_operator(const bContext *C, Panel *pa)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	struct wmOperator *op = sfile ? sfile->op : NULL;
	uiBlock *block;
	int sy;

	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_file_panel_events, NULL);

	sy= 0;
	if (op) {
		uiBlockBeginAlign(block);
		RNA_STRUCT_BEGIN(op->ptr, prop) {
			if(strcmp(RNA_property_identifier(prop), "rna_type") == 0)
				continue;
			if(strcmp(RNA_property_identifier(prop), "filename") == 0)
				continue;
			if(strcmp(RNA_property_identifier(prop), "display") == 0)
				continue;
			if(strncmp(RNA_property_identifier(prop), "filter", 6) == 0)
				continue;
			uiItemFullR(pa->layout, NULL, 0, op->ptr, prop, -1, 0, 0, 0, 0);
		}
		RNA_STRUCT_END;
		uiBlockEndAlign(block);
	}
	uiBlockLayoutResolve(C, block, NULL, &sy);
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


void file_panels_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype file system directories");
	strcpy(pt->idname, "FILE_PT_system");
	strcpy(pt->label, "System");
	pt->draw= file_panel_system;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype file bookmarks");
	strcpy(pt->idname, "FILE_PT_bookmarks");
	strcpy(pt->label, "Bookmarks");
	pt->draw= file_panel_bookmarks;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype file recent directories");
	strcpy(pt->idname, "FILE_PT_recent");
	strcpy(pt->label, "Recent");
	pt->draw= file_panel_recent;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype file operator properties");
	strcpy(pt->idname, "FILE_PT_operator");
	strcpy(pt->label, "Operator");
	pt->draw= file_panel_operator;
	BLI_addtail(&art->paneltypes, pt);
}

