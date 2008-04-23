/**
 * header_script.c nov-2003
 *
 * Functions to draw the "Script Window" window header
 * and handle user events sent to it.
 * 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Willian P. Germano.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BLI_blenlib.h"

#include "BSE_headerbuttons.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_sca.h"
#include "BSE_filesel.h"

#include "BPY_extern.h"
#include "BPY_menus.h"

#include "blendef.h"
#include "mydevice.h"

/* ********************** SCRIPT ****************************** */

/* action executed after clicking in Scripts menu */
static void do_scripts_submenus(void *int_arg, int event)
{
	int menutype = (long)int_arg;

	BPY_menu_do_python (menutype, event);

	//allqueue(REDRAWSCRIPT, 0);
}

static uiBlock *script_scripts_submenus(void *int_menutype)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;
	BPyMenu *pym;
	int i = 0, menutype = (long)int_menutype;

	if ((menutype < 0) || (menutype > PYMENU_SCRIPTS_MENU_TOTAL))
		return NULL;

	block= uiNewBlock(&curarea->uiblocks, "scriptsscriptssubmenus", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_scripts_submenus, int_menutype);

	for (pym = BPyMenuTable[menutype]; pym; pym = pym->next, i++) {
		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, i, pym->tooltip?pym->tooltip:pym->filename);
	}

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_script_scriptsmenu(void *arg, int event)
{
	ScrArea *sa;
	
	if(curarea->spacetype==SPACE_INFO) {
		sa= closest_bigger_area();
		areawinset(sa->win);
	}

	/* these are no defines, easier this way, the codes are in the function below */
	switch(event) {
	case 0: /* update menus */
		if (BPY_path_update()==0) { 
			error("Invalid scripts dir: check console");
		}
		break;
	}

//	allqueue(REDRAWSCRIPT, 0);
}

/* Scripts menu */
static uiBlock *script_scriptsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 0, menuwidth = 120;
	int i;

	block= uiNewBlock(&curarea->uiblocks, "script_scriptsmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_script_scriptsmenu, NULL);

	for (i = 0; i < PYMENU_SCRIPTS_MENU_TOTAL; i++) {
		uiDefIconTextBlockBut(block, script_scripts_submenus, (void *)(long)i, ICON_RIGHTARROW_THIN, BPyMenu_group_itoa(i), 0, yco-=20, menuwidth, 19, "");
	}

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Update Menus", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "Use when a scripts folder or its contents are modified");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

void do_script_buttons(unsigned short event)
{	
	SpaceScript *sc= curarea->spacedata.first;
	ID *id, *idtest;
	int nr= 1;
	Script *script = sc->script;

	if (!sc) return;
	if (sc->spacetype != SPACE_SCRIPT) return;

	switch (event) {
	case B_SCRIPTBROWSE:
		if (sc->menunr==-2) {
			activate_databrowse((ID *)script, ID_SCR, 0, B_SCRIPTBROWSE,
											&sc->menunr, do_script_buttons);
			break;
		}

		if(sc->menunr < 0) break;

		nr = 1;
		id = (ID *)script;

		idtest= G.main->script.first;
		while(idtest) {
			if(nr==sc->menunr) {
				break;
			}
			nr++;
			idtest= idtest->next;
		}
		if(idtest!=id) {
			sc->script= (Script *)idtest;

			allqueue(REDRAWSCRIPT, 0);
			allqueue(REDRAWHEADERS, 0);
		}
		break;
	case B_SCRIPT2PREV:
		if(sc->next) {
			BLI_remlink(&curarea->spacedata, sc);
			BLI_addtail(&curarea->spacedata, sc);
			sc = curarea->spacedata.first;
			newspace(curarea, sc->spacetype);
		}
		break;
	}

	return;
}

void script_buttons(void)
{
	uiBlock *block;
	SpaceScript *sc= curarea->spacedata.first;
	short xco = 8, xmax;
	char naam[256];
	
	if (!sc || sc->spacetype != SPACE_SCRIPT) return;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_SCRIPT;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D,
			windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0,
			SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu"
			" of available types.");
	xco += XIC+14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if(curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Enables display of pulldown menus");
	} else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hides pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	/* pull down menus */
	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("Scripts");
		uiDefPulldownBut(block,script_scriptsmenu, NULL, "Scripts", xco, 0, xmax, 20, "");
		xco+=xmax;
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
	
	uiBlockBeginAlign(block);
	uiDefIconBut(block, BUT, B_SCRIPT2PREV, ICON_GO_LEFT, xco+=XIC, 0, XIC, YIC,
		0, 0, 0, 0, 0, "Returns to previous window");

	/* FULL WINDOW */
	if(curarea->full)
		uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0,
			  0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else
		uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0,
				0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	uiBlockEndAlign(block);
	
	/* STD SCRIPT BUTTONS */
	xco += 2*XIC;
	xco= std_libbuttons(block, xco, 0, 0, NULL, B_SCRIPTBROWSE, ID_SCRIPT, 0, (ID*)sc->script, 0, &(sc->menunr), 0, 0, 0, 0, 0);

	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}


/* ********************** SCRIPT ****************************** */

