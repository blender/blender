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
#include "UI_view2d.h"

#include "IMB_imbuf_types.h"

#include "info_intern.h"

static int pupmenu() {return 0;}
static int okee() {return 0;}
static int error() {return 0;}

/* ************************ header area region *********************** */

#define B_STOPRENDER	1
#define B_STOPCAST		2
#define B_STOPANIM		3

static void do_viewmenu(bContext *C, void *arg, int event)
{
}

static uiBlock *dummy_viewmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *curarea= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "dummy_viewmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

static int buttons_do_unpack()
{
	int how;
	char menu[2048];
	char *line = menu;
	int ret_value = 1, count = 0;
	
	count = countPackedFiles();
	
	if(!count) {
		pupmenu("No packed files. Autopack disabled");
		return ret_value;
	}
	if (count == 1)
		line += sprintf(line, "Unpack 1 file%%t");
	else
		line += sprintf(line, "Unpack %d files%%t", count);
	
	line += sprintf(line, "|Use files in current directory (create when necessary)%%x%d", PF_USE_LOCAL);
	line += sprintf(line, "|Write files to current directory (overwrite existing files)%%x%d", PF_WRITE_LOCAL);
	line += sprintf(line, "|%%l|Use files in original location (create when necessary)%%x%d", PF_USE_ORIGINAL);
	line += sprintf(line, "|Write files to original location (overwrite existing files)%%x%d", PF_WRITE_ORIGINAL);
	line += sprintf(line, "|%%l|Disable AutoPack, keep all packed files %%x%d", PF_KEEP);
	line += sprintf(line, "|Ask for each file %%x%d", PF_ASK);
	
	how = pupmenu(menu);
	
	if(how == -1)
		ret_value = 0;
	else {
		if (how != PF_KEEP) unpackAll(how);
		G.fileflags &= ~G_AUTOPACK;
 	}
 	
	return ret_value;
}

static void check_packAll()
{
	// first check for dirty images
	Image *ima;
	
	for(ima = G.main->image.first; ima; ima= ima->id.next) {
		if (ima->ibufs.first) { /* XXX FIX */
			ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
			
			if (ibuf && (ibuf->userflags &= IB_BITMAPDIRTY))
				break;
		}
	}

	if (ima == NULL || okee("Some images are painted on. These changes will be lost. Continue ?")) {
		packAll();
		G.fileflags |= G_AUTOPACK;
	}
}

static void do_info_externalfiles(bContext *C, void *arg, int event)
{
	switch (event) {
		
		case 1: /* pack data */
			check_packAll();
			break;
		case 3: /* unpack data */
			if (buttons_do_unpack() != 0) {
				/* Clear autopack bit only if user selected one of the unpack options */
				G.fileflags &= ~G_AUTOPACK;
			}
			break;
		case 10: /* make all paths relative */
			if (G.relbase_valid) {
				int tot,changed,failed,linked;
				char str[512];
				char txtname[24]; /* text block name */
				txtname[0] = '\0';
				makeFilesRelative(txtname, &tot, &changed, &failed, &linked);
				if (failed) sprintf(str, "Make Relative%%t|Total files %i|Changed %i|Failed %i, See Text \"%s\"|Linked %i", tot, changed, failed, txtname, linked);
				else		sprintf(str, "Make Relative%%t|Total files %i|Changed %i|Failed %i|Linked %i", tot, changed, failed, linked);
				pupmenu(str);
			} else {
				pupmenu("Can't set relative paths with an unsaved blend file");
			}
			break;
		case 11: /* make all paths absolute */
		{
			int tot,changed,failed,linked;
			char str[512];
			char txtname[24]; /* text block name */
			txtname[0] = '\0';
			makeFilesAbsolute(txtname, &tot, &changed, &failed, &linked);
			sprintf(str, "Make Absolute%%t|Total files %i|Changed %i|Failed %i|Linked %i", tot, changed, failed, linked);
			if (failed) sprintf(str, "Make Absolute%%t|Total files %i|Changed %i|Failed %i, See Text \"%s\"|Linked %i", tot, changed, failed, txtname, linked);
			else		sprintf(str, "Make Absolute%%t|Total files %i|Changed %i|Failed %i|Linked %i", tot, changed, failed, linked);
			
			pupmenu(str);
		}
			break;
		case 12: /* check images exist */
		{
			char txtname[24]; /* text block name */
			txtname[0] = '\0';
			
			/* run the missing file check */
			checkMissingFiles( txtname );
			
			if (txtname[0] == '\0') {
				okee("No external files missing");
			} else {
				char str[128];
				sprintf(str, "Missing files listed in Text \"%s\"", txtname );
				error(str);
			}
		}
			break;
		case 13: /* search for referenced files that are not available  */
// XXX			if(curarea->spacetype==SPACE_INFO) {
//				ScrArea *sa;
//				sa= closest_bigger_area();
//				areawinset(sa->win);
//			}
//			activate_fileselect(FILE_SPECIAL, "Find Missing Files", "", findMissingFiles);
			break;
	}
	
}


uiBlock *info_externalfiles(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;
	
	block= uiBeginBlock(C, ar, "info_externalfiles", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_info_externalfiles, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pack into .blend file",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unpack into Files...",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make all Paths Relative",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make all Paths Absolute",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Report Missing Files...",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 12, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find Missing Files...",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 13, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}



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


static void do_info_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_STOPRENDER:
			G.afbreek= 1;
			break;
		case B_STOPCAST:
			WM_jobs_stop(CTX_wm_manager(C), CTX_wm_screen(C));
			break;
		case B_STOPANIM:
			ED_screen_animation_timer(C, 0, 0);
			break;
	}
}

static void screen_idpoin_handle(bContext *C, ID *id, int event)
{
	switch(event) {
		case UI_ID_BROWSE:
			/* exception: can't set screens inside of area/region handers */
			WM_event_add_notifier(C, NC_SCREEN|ND_SCREENBROWSE, id);
			break;
		case UI_ID_DELETE:
			ED_undo_push(C, "");
			break;
		case UI_ID_RENAME:
			break;
		case UI_ID_ADD_NEW:
			/* XXX not implemented */
			break;
		case UI_ID_OPEN:
			/* XXX not implemented */
			break;
		case UI_ID_ALONE:
			/* XXX not implemented */
			break;
		case UI_ID_PIN:
			break;
	}
}

static void scene_idpoin_handle(bContext *C, ID *id, int event)
{
	switch(event) {
		case UI_ID_BROWSE:
			/* exception: can't set screens inside of area/region handers */
			WM_event_add_notifier(C, NC_SCENE|ND_SCENEBROWSE, id);
			break;
		case UI_ID_DELETE:
			ED_undo_push(C, "");
			break;
		case UI_ID_RENAME:
			break;
		case UI_ID_ADD_NEW:
			/* XXX not implemented */
			break;
		case UI_ID_OPEN:
			/* XXX not implemented */
			break;
		case UI_ID_ALONE:
			/* XXX not implemented */
			break;
		case UI_ID_PIN:
			break;
	}
}

static void operator_search_cb(const struct bContext *C, void *arg, char *str, uiSearchItems *items)
{
	wmOperatorType *ot = WM_operatortype_first();
	
	for(; ot; ot= ot->next) {
		
		if(BLI_strcasestr(ot->name, str)) {
			if(ot->poll==NULL || ot->poll((bContext *)C)) {
				int len= strlen(ot->name);
				
				BLI_strncpy(items->names[items->totitem], ot->name, items->maxstrlen);
				
				/* check for hotkey */
				if(len < items->maxstrlen-6) {
					if(WM_key_event_operator_string(C, ot->idname, WM_OP_EXEC_DEFAULT, NULL, items->names[items->totitem]+len+1, items->maxstrlen-len-1)) {
						items->names[items->totitem][len]= '|';
					}
				}
				
				items->totitem++;
				if(items->totitem>=items->maxitem)
					break;
			}
		}
	}
}

void info_header_buttons(const bContext *C, ARegion *ar)
{
	wmWindow *win= CTX_wm_window(C);
	bScreen *screen= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	uiBlock *block;
	int xco, yco= 3;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_info_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		xmax= GetButStringLength("File");
		uiDefMenuBut(block, info_filemenu, NULL, "File", xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Add");
		uiDefPulldownBut(block, dummy_viewmenu, sa, "Add",	xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Timeline");
		uiDefPulldownBut(block, dummy_viewmenu, sa, "Timeline",	xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Game");
		uiDefPulldownBut(block, dummy_viewmenu, sa, "Game",	xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Render");
		uiDefPulldownBut(block, dummy_viewmenu, sa, "Render",	xco, yco, xmax-3, 20, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Help");
		uiDefPulldownBut(block, dummy_viewmenu, NULL, "Help",	xco, yco, xmax-3, 20, "");
		xco+= xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	if(screen->full==0) {
		xco= uiDefIDPoinButs(block, CTX_data_main(C), NULL, (ID*)win->screen, ID_SCR, NULL, xco, yco,
						 screen_idpoin_handle, UI_ID_BROWSE|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_DELETE);
		xco += 8;
		xco= uiDefIDPoinButs(block, CTX_data_main(C), NULL, (ID*)screen->scene, ID_SCE, NULL, xco, yco,
							 scene_idpoin_handle, UI_ID_BROWSE|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_DELETE);
		xco += 8;
	}	

	if(WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C))) {
		uiDefIconTextBut(block, BUT, B_STOPRENDER, ICON_REC, "Render", xco+5,yco,75,19, NULL, 0.0f, 0.0f, 0, 0, "Stop rendering");
		xco+= 80;
	}
	if(WM_jobs_test(CTX_wm_manager(C), CTX_wm_screen(C))) {
		uiDefIconTextBut(block, BUT, B_STOPCAST, ICON_REC, "Capture", xco+5,yco,85,19, NULL, 0.0f, 0.0f, 0, 0, "Stop screencast");
		xco+= 90;
	}
	if(screen->animtimer) {
		uiDefIconTextBut(block, BUT, B_STOPANIM, ICON_REC, "Anim Player", xco+5,yco,85,19, NULL, 0.0f, 0.0f, 0, 0, "Stop animation playback");
		xco+= 90;
	}
	
	{
		static char search[256]= "";
		uiBut *but= uiDefSearchBut(block, search, 0, ICON_PROP_ON, 256, xco+5, yco, 120, 19, "");
		
		uiButSetSearchFunc(but, operator_search_cb, NULL);

		xco+= 125;
	}

	
	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


