/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include <stdio.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#else
#include <sys/times.h>
#endif

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_global.h"

#include "BIF_fsmenu.h"
#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_imasel.h"

#include "BSE_filesel.h"
#include "BSE_drawimasel.h"

#include "BDR_editcurve.h"

#include "blendef.h"
#include "mydevice.h"
#include "interface.h"

/* locals */
void draw_icon_imasel(void);
void winqreadimasel(unsigned short event, short val, char ascii);

#define XIC 20
#define YIC 21

/* GLOBALS */
extern char *fsmenu;


void draw_icon_imasel(void)
{
	scrarea_queue_winredraw(curarea);
}

void winqreadimasel(unsigned short event, short val, char ascii)
{
	SpaceImaSel *simasel;
	
	short mval[2];
	short area_event;
	short queredraw = 0;
	char  name[256];
	char *selname;
	static double prevtime=0;
	
	
	if(val==0) return;
	simasel= curarea->spacedata.first;
	
	area_event = 0;
	getmouseco_areawin(mval);
	simasel->mx= mval[0];
	simasel->my= mval[1];
	
	if (simasel->desx > 0){
		if ( (mval[0] > simasel->dssx) && (mval[0] < simasel->dsex) && (mval[1] > simasel->dssy) && (mval[1] < simasel->dsey) ) area_event = IMS_INDIRSLI;
		if ( (mval[0] > simasel->desx) && (mval[0] < simasel->deex) && (mval[1] > simasel->desy) && (mval[1] < simasel->deey) ) area_event = IMS_INDIR;
	}
	if (simasel->fesx > 0){
		if ( (mval[0] > simasel->fssx) && (mval[0] < simasel->fsex) && (mval[1] > simasel->fssy) && (mval[1] < simasel->fsey) ) area_event = IMS_INFILESLI;
		if ( (mval[0] > simasel->fesx) && (mval[0] < simasel->feex) && (mval[1] > simasel->fesy) && (mval[1] < simasel->feey) ) area_event = IMS_INFILE;
	}	
	
	if( event!=RETKEY && event!=PADENTER)
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

	switch(event) {
	case AFTERPIBREAD:	
		get_pib_file(simasel);
		queredraw = 1;
		break;
			
	case AFTERIMASELIMA:
		if (bitset(simasel->fase, IMS_DOTHE_INF)){
			get_file_info(simasel);
			
			if (!bitset(simasel->fase, IMS_KNOW_INF)){
				addafterqueue(curarea->win, AFTERIMASELIMA, 1);
				
			}else{
				simasel->subfase = 0;
				simasel->imafase = 0;
				simasel->fase |= IMS_DOTHE_IMA;
				addafterqueue(curarea->win, AFTERIMASELGET, 1);
			}
		}
		break;
	case AFTERIMASELGET:
		if (bitset(simasel->fase, IMS_DOTHE_IMA)){
			get_next_image(simasel);
			if (simasel->ima_redraw > 0){
				double newtime = PIL_check_seconds_timer();
				if ((newtime - prevtime) > 0.03) { 
					simasel->ima_redraw = 0;
					queredraw = 1;
					prevtime = newtime;
				}
				
			}
			if (!bitset(simasel->fase, IMS_KNOW_IMA)){
				addafterqueue(curarea->win, AFTERIMASELGET, 1);
			}else{
				simasel->ima_redraw = 0;
				simasel->subfase    = 0;
				simasel->imafase    = 0;
				addqueue(curarea->win, AFTERIMAWRITE, 1);
				queredraw = 1;
			}
		}
		break;
	case  AFTERIMAWRITE:
		if (bitset(simasel->fase, IMS_KNOW_IMA)){
			write_new_pib(simasel);
			queredraw = 1;
		}
		break;	
	
	case RIGHTMOUSE:
		if ((area_event == IMS_INFILE) && (simasel->hilite_ima)){
			select_ima_files(simasel);
			queredraw = 1;
		}
		break;
	case UI_BUT_EVENT:
		
		/* bug: blender's interface kit also returns a '4'... what is it! */
		
		switch(val) {
		case 13:	/*  'P' */
			imadir_parent(simasel);
			queredraw = 1;
			
		case 1: /* dir entry */
			checkdir(simasel->dir);
			clear_ima_dir(simasel);
			queredraw = 1;
			break;
		
		case 3: /* fsmenu */
			selname= fsmenu_get_entry(simasel->fileselmenuitem-1);
			if (selname) {
				strcpy(simasel->dir, selname);
				checkdir(simasel->dir);
				clear_ima_dir(simasel);
			    queredraw = 1;
			}
			break;

		case 5:
			if (simasel->returnfunc) {
				char name[256];
				strcpy(name, simasel->dir);
				strcat(name, simasel->file);
				simasel->returnfunc(name);
				filesel_prevspace();
			}
			break;
		case 6:
			filesel_prevspace();
			break;
					
		}
		break;
		
	case LEFTMOUSE:
	case MIDDLEMOUSE:
	
		/* No button pressed */
		switch (area_event){
		case IMS_INDIRSLI:
			move_imadir_sli(simasel);
			queredraw = 1;
			break;
		case IMS_INFILESLI:
			move_imafile_sli(simasel);
			queredraw = 1;
			break;
		case IMS_INDIR:
			if (simasel->hilite > -1){
				change_imadir(simasel);
				queredraw = 1;
			}
			break;
		case IMS_INFILE:
			if (simasel->hilite_ima){
				strcpy(simasel->fole, simasel->hilite_ima->file_name);
				strcpy(simasel->file, simasel->hilite_ima->file_name);
				
				if (event == LEFTMOUSE) addqueue(curarea->win, IMALEFTMOUSE, 1);	
				
				if ((event == MIDDLEMOUSE)&&(simasel->returnfunc)){
					strcpy(name, simasel->dir);
					strcat(name, simasel->file);
					
					if(simasel->mode & IMS_STRINGCODE) BLI_makestringcode(G.sce, name);
					
					simasel->returnfunc(name);
					filesel_prevspace();
				}
				queredraw = 1;
			}
			break;
		}
		break;
	
	case MOUSEX:
	case MOUSEY:
		getmouseco_areawin(mval);	/* lokaal screen */
		calc_hilite(simasel);
		if (simasel->mouse_move_redraw ){
			simasel->mouse_move_redraw = 0;
			queredraw = 1;
		}
		break;
		
	case WHEELUPMOUSE:
	case WHEELDOWNMOUSE:
		switch(area_event){
		case IMS_INDIRSLI:
		case IMS_INDIR:
			if (simasel->dirsli){
				if (event == WHEELUPMOUSE)	simasel->topdir -= U.wheellinescroll;
				if (event == WHEELDOWNMOUSE)	simasel->topdir += U.wheellinescroll; 	
				queredraw = 1;
			}
			break;
		case IMS_INFILESLI:
		case IMS_INFILE:
			if(simasel->imasli){
				if (event == WHEELUPMOUSE)	simasel->image_slider -= 0.2 * simasel->slider_height;
				if (event == WHEELDOWNMOUSE)	simasel->image_slider += 0.2 * simasel->slider_height;
				
				if(simasel->image_slider < 0.0)	simasel->image_slider = 0.0;
				if(simasel->image_slider > 1.0)	simasel->image_slider = 1.0;
				queredraw = 1;
			}	
			break;
		}
		break;

	case PAGEUPKEY:
	case PAGEDOWNKEY:
		switch(area_event){
		case IMS_INDIRSLI:
		case IMS_INDIR:
			if (simasel->dirsli){
				if (event == PAGEUPKEY)   simasel->topdir -= (simasel->dirsli_lines - 1);
				if (event == PAGEDOWNKEY) simasel->topdir += (simasel->dirsli_lines - 1); 	
				queredraw = 1;
			}
			break;
		case IMS_INFILESLI:
		case IMS_INFILE:
			if(simasel->imasli){
				if (event == PAGEUPKEY)   simasel->image_slider -= simasel->slider_height;
				if (event == PAGEDOWNKEY) simasel->image_slider += simasel->slider_height;
				
				if(simasel->image_slider < 0.0)  simasel->image_slider = 0.0;
				if(simasel->image_slider > 1.0)  simasel->image_slider = 1.0;
				queredraw = 1;
			}	
			break;
		}
		break;
	
	case HOMEKEY:
		simasel->image_slider = 0.0;
		queredraw = 1;
		break;

	case ENDKEY:
		simasel->image_slider = 1.0;
		queredraw = 1;
		break;
			
	case AKEY:
		if (G.qual == 0){
			ima_select_all(simasel);
			queredraw = 1;
		}
		break;

	case PKEY:
		if(G.qual & LR_SHIFTKEY) {
			extern char bprogname[];	/* usiblender.c */
			
			sprintf(name, "%s -a \"%s%s\"", bprogname, simasel->dir, simasel->file);
			system(name);
		}
		if(G.qual & LR_CTRLKEY) {
			if(bitset(simasel->fase, IMS_KNOW_IMA)) pibplay(simasel);
		}
		if (G.qual == 0){
			imadir_parent(simasel);
			checkdir(simasel->dir);
			clear_ima_dir(simasel);
			queredraw = 1;
		}
		break;
		
	case IKEY:
		if ((G.qual == 0)&&(simasel->file)){
			sprintf(name, "$IMAGEEDITOR %s%s", simasel->dir, simasel->file);
			system(name);
			queredraw = 1;
		}

		break;
	
	case PADPLUSKEY:
	case EQUALKEY:
		BLI_newname(simasel->file, +1);
		queredraw = 1;
		break;
		
	case PADMINUS:
	case MINUSKEY:
		BLI_newname(simasel->file, -1);
		queredraw = 1;
		break;
		
	case BACKSLASHKEY:
	case SLASHKEY:
#ifdef WIN32
		strcpy(simasel->dir, "\\");
#else
		strcpy(simasel->dir, "/");
#endif
		clear_ima_dir(simasel);
		simasel->image_slider = 0.0;
		queredraw = 1;
		break;
		
	case PERIODKEY:
		clear_ima_dir(simasel);
		queredraw = 1;
		break;
	
	case ESCKEY:
		filesel_prevspace();
		break;

	case PADENTER:
	case RETKEY:
		if (simasel->returnfunc){
			strcpy(name, simasel->dir);
			strcat(name, simasel->file);
			simasel->returnfunc(name);
			filesel_prevspace();
		}
		break;
	}
	
		
	if (queredraw) scrarea_queue_winredraw(curarea);
}


