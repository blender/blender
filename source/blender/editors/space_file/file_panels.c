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

static void file_panel_category(const bContext *C, Panel *pa, FSMenuCategory category, int icon)
{
	uiBlock *block;
	uiStyle *style= U.uistyles.first;
	int i;
	int fontsize = file_font_pointsize();
	struct FSMenu* fsmenu = fsmenu_get();
	int nentries = fsmenu_get_nentries(fsmenu, category);

	uiLayoutSetAlignment(pa->layout, UI_LAYOUT_ALIGN_LEFT);
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_file_panel_events, NULL);
	uiBlockSetEmboss(block, UI_EMBOSSP);
	uiBlockBeginAlign(block);
	for (i=0; i< nentries;++i) {
		char *fname = fsmenu_get_entry(fsmenu, category, i);
		uiItemStringO(pa->layout, fname, icon, "FILE_OT_select_bookmark", "dir", fname);
	}
	uiBlockEndAlign(block);
}

static void file_panel_system(const bContext *C, Panel *pa)
{
	file_panel_category(C, pa, FS_CATEGORY_SYSTEM, ICON_DISK_DRIVE);
}

static void file_panel_bookmarks(const bContext *C, Panel *pa)
{
	file_panel_category(C, pa, FS_CATEGORY_BOOKMARKS, ICON_BOOKMARKS);
}


static void file_panel_recent(const bContext *C, Panel *pa)
{
	file_panel_category(C, pa, FS_CATEGORY_RECENT, ICON_FILE_FOLDER);
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