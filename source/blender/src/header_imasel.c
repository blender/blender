/**
 * header_imasel.c oct-2003
 *
 * Functions to draw the "Image Browser" window header
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
#include "DNA_space_types.h"

#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BSE_headerbuttons.h"

#include "interface.h"
#include "blendef.h"

void do_imasel_buttons(short event)
{
  SpaceImaSel *simasel;
  char name[256];
  
  simasel= curarea->spacedata.first;
  
  if(curarea->win==0) return;

  switch(event) {
  case B_IMASELHOME:
    break;
    
  case B_IMASELREMOVEBIP:
    
    if(bitset(simasel->fase, IMS_FOUND_BIP)){
    
      strcpy(name, simasel->dir);
      strcat(name, ".Bpib");
    
      remove(name);
    
      simasel->fase &= ~ IMS_FOUND_BIP;
    }
    break;
  }
}

void imasel_buttons(void)
{
	SpaceImaSel *simasel;
	uiBlock *block;
	short xco;
	char naam[256];
	
	simasel= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTBLUE);

	curarea->butspacetype= SPACE_IMASEL;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");
	
	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "");
	
	xco+=XIC;
	if (simasel->title){
		xco+=25;
		glRasterPos2i(xco,  4);
		BMF_DrawString(G.font, simasel->title);
		xco+=BMF_GetStringWidth(G.fonts, simasel->title);
		xco+=25;
	}
	uiDefIconBut(block, BUT, B_IMASELREMOVEBIP, ICON_BPIBFOLDER_X, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "");/* remove  */
	
	uiDefIconButS(block, TOG|BIT|0, B_REDR, ICON_BPIBFOLDERGREY, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "Toggles display of directory information");/* dir   */
	uiDefIconButS(block, TOG|BIT|1, B_REDR, ICON_INFO, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "Toggles display of selected image information");/* info  */
	uiDefIconButS(block, TOG|BIT|2, B_REDR, ICON_IMAGE_COL, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "");/* image */
	uiDefIconButS(block, TOG|BIT|3, B_REDR, ICON_MAGNIFY, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "Toggles magnified view of thumbnail of images under mouse pointer");/* magnify */
	
	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
