/**
 * header_sound.c oct-2003
 *
 * Functions to draw the "Audio Timeline" window header
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
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

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BIF_editsound.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BSE_drawipo.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"

#include "blendef.h"
#include "interface.h"
#include "mydevice.h"

void do_sound_buttons(unsigned short event)
{
	ID *id, *idtest;
	int nr;
	char name[256];

	switch(event) {

	case B_SOUNDBROWSE: 
		if(G.ssound->sndnr== -2) {
			activate_databrowse((ID *)G.ssound->sound, ID_SO, 0,
											B_SOUNDBROWSE, &G.ssound->sndnr, do_sound_buttons);
			return;
		}
		if (G.ssound->sndnr < 0) break;
		if (G.ssound->sndnr == 32766) {
			if (G.ssound && G.ssound->sound) strcpy(name, G.ssound->sound->name);
			else strcpy(name, U.sounddir);
			activate_fileselect(FILE_SPECIAL, "SELECT WAV FILE", name,
											load_space_sound);
		} else {
			nr= 1;
			id= (ID *)G.ssound->sound;

			idtest= G.main->sound.first;
			while(idtest) {
				if(nr==G.ssound->sndnr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}

			if(idtest==0) { /* no new */
				return;
			}

			if(idtest!=id) {
				G.ssound->sound= (bSound *)idtest;
				if(idtest->us==0) idtest->us= 1;
				allqueue(REDRAWSOUND, 0);
			}
		}

		break;
	case B_SOUNDBROWSE2:	
		id = (ID *)G.buts->lockpoin;
		if(G.buts->texnr == -2) {
			activate_databrowse(id, ID_SO, 0, B_SOUNDBROWSE2,
											&G.buts->texnr, do_sound_buttons);
			return;
		}
		if (G.buts->texnr < 0) break;
		if (G.buts->texnr == 32766) {
			if (id) strcpy(name, ((bSound *)id)->name);
			else strcpy(name, U.sounddir);
			activate_fileselect(FILE_SPECIAL, "SELECT WAV FILE",
											name, load_sound_buttons);
		} else {
			nr= 1;

			idtest= G.main->sound.first;
			while (idtest) {
				if(nr == G.buts->texnr) {
					break;
				}
				nr++;
				idtest = idtest->next;
			}

			if (idtest == 0) {	/* geen new */
				return;
			}

			if (idtest != id) {
				G.buts->lockpoin = (bSound *)idtest;
				if(idtest->us==0) idtest->us= 1;
				//allqueue(REDRAWBUTSSOUND, 0);
				BIF_preview_changed(G.buts);
			}
		}
		break;

	case B_SOUNDHOME:
		G.v2d->cur= G.v2d->tot;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
	}
}

void sound_buttons(void)
{
	uiBlock *block;
	short xco;
	char naam[256];
	char ch[20];
	
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTYELLOW);

	curarea->butspacetype= SPACE_SOUND;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_SOUNDHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");

	xco= std_libbuttons(block, xco+40, 0, 0, NULL, B_SOUNDBROWSE, (ID *)G.ssound->sound, 0, &(G.ssound->sndnr), 1, 0, 0, 0, 0);	

	if(G.ssound->sound) {
		bSound *sound= G.ssound->sound;

		if (sound->sample && sound->sample->len)
		{
			if (sound->sample->channels == 1)
				strcpy(ch, "Mono");
			else if (sound->sample->channels == 2)
				strcpy(ch, "Stereo");
			else
				strcpy(ch, "Unknown");
			
			sprintf(naam, "Sample: %s, %d bit, %d Hz, %d samples", ch, sound->sample->bits, sound->sample->rate, sound->sample->len);
			cpack(0x0);
			glRasterPos2i(xco+10, 5);
			BMF_DrawString(uiBlockGetCurFont(block), naam);
		}
		else
		{
			sprintf(naam, "No sample info available.");
			cpack(0x0);
			glRasterPos2i(xco+10, 5);
			BMF_DrawString(uiBlockGetCurFont(block), naam);
		}
		
	}

	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}

/* the next two functions are also called from fileselect: */

void load_space_sound(char *str)
{
	bSound *sound;

	sound= sound_new_sound(str);
	if (sound) {
		if (G.ssound) {
			G.ssound->sound= sound;
		}
	} else {
		error("Not a valid sample: %s", str);
	}

	allqueue(REDRAWSOUND, 0);
	allqueue(REDRAWBUTSLOGIC, 0);
}

void load_sound_buttons(char *str)
{
	bSound *sound;

	sound= sound_new_sound(str);
	if (sound) {
		if (curarea && curarea->spacetype==SPACE_BUTS) {
//			if (G.buts->mainb == BUTS_SOUND) {
//				G.buts->lockpoin = sound;
//			}
		}
	} else {
		error("Not a valid sample: %s", str);
	}

	//allqueue(REDRAWBUTSSOUND, 0);
}
