/**
 * header_script.c nov-2003
 *
 * Functions to draw the "Script Window" window header
 * and handle user events sent to it.
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "BSE_headerbuttons.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_script_types.h"

#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_sca.h"
#include "BPY_extern.h"
#include "BSE_filesel.h"

#include "blendef.h"
#include "mydevice.h"

/* ********************** SCRIPT ****************************** */
void do_script_buttons(unsigned short event)
{	
	SpaceScript *sc= curarea->spacedata.first;
	ID *id, *idtest;
	int nr= 1;
	Script *script;

	if (!sc) return;
	if (sc->spacetype != SPACE_SCRIPT) return;

	switch (event) {
	case B_SCRIPTBROWSE:
		if (sc->menunr==-2) {
			activate_databrowse((ID *)sc->script, ID_SCR, 0, B_SCRIPTBROWSE,
											&sc->menunr, do_script_buttons);
			break;
		}
		if(sc->menunr < 0) break;

		script = sc->script;

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
	}

	return;
}

void script_buttons(void)
{
	uiBlock *block;
	SpaceScript *sc= curarea->spacedata.first;
	short xco = 8;
	char naam[256];
	
	if (!sc || sc->spacetype != SPACE_SCRIPT) return;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_SCRIPT;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D,
			windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0,
			SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu"
			" of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full)
		uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0,
			  0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else
		uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0,
				0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");

	/* STD SCRIPT BUTTONS */
	xco+= 2*XIC;
	xco= std_libbuttons(block, xco, 0, 0, NULL, B_SCRIPTBROWSE, (ID*)sc->script, 0, &(sc->menunr), 0, 0, 0, 0, 0);

	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}

/* ********************** SCRIPT ****************************** */

