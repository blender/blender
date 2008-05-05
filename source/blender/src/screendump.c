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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Making screendumps.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_sca.h"

#include "BIF_gl.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_toets.h"
#include "BIF_interface.h"
#include "BIF_writeimage.h"

#include "BSE_filesel.h"

#include "mydevice.h"

static unsigned int *dumprect=0;
static int dumpsx, dumpsy;

void write_screendump(char *name)
{
	ImBuf *ibuf;
	
	if(dumprect) {

		strcpy(G.ima, name);
		BLI_convertstringcode(name, G.sce);
		BLI_convertstringframe(name, G.scene->r.cfra); /* TODO - is this ever used? */
		
		/* BKE_add_image_extension() checks for if extension was already set */
		if(G.scene->r.scemode & R_EXTENSION) 
			if(strlen(name)<FILE_MAXDIR+FILE_MAXFILE-5)
				BKE_add_image_extension(name, G.scene->r.imtype);
		
		if(saveover(name)) {
			waitcursor(1);
			
			ibuf= IMB_allocImBuf(dumpsx, dumpsy, 24, 0, 0);
			ibuf->rect= dumprect;
			
			if(G.scene->r.planes == 8) IMB_cspace(ibuf, rgb_to_bw);
			
			BKE_write_ibuf(ibuf, name, G.scene->r.imtype, G.scene->r.subimtype, G.scene->r.quality);

			IMB_freeImBuf(ibuf);
			
			waitcursor(0);
		}
		MEM_freeN(dumprect);
		dumprect= NULL;
	}
}

/* get dump from frontbuffer */
void BIF_screendump(int fscreen)
{
	extern int uiIsMenu(int *x, int *y, int *sizex, int *sizey);
	int ismenu;
	static int wasmenu= 0;
	int x=0, y=0;
	char imstr[64];

	/* this sets dumpsx/y to zero if ismenu==0 */
	ismenu= uiIsMenu(&x, &y, &dumpsx, &dumpsy);
	
	if(wasmenu && !ismenu) {
		save_image_filesel_str(imstr);
		strcat(imstr, " (Menu)");
		activate_fileselect(FILE_SPECIAL, imstr, G.ima, write_screendump);
		wasmenu= 0;
		return;
	}
	
	if(dumprect) MEM_freeN(dumprect);
	dumprect= NULL;
	
	if((G.qual & LR_SHIFTKEY) || fscreen) {	/* full screen */
		x= 0;
		y= 0;
		
		dumpsx= G.curscreen->sizex;
		dumpsy= G.curscreen->sizey;
		
	} 
	else {
		if(ismenu==0) {	/* a window */
			//int win= mywinget();

			//bwin_getsuborigin(win, &x, &y);
			//bwin_getsize(win, &dumpsx, &dumpsy);
			x= curarea->totrct.xmin;
			y= curarea->totrct.ymin;
			dumpsx= curarea->totrct.xmax-x;
			dumpsy= curarea->totrct.ymax-y;
		}
	}
	
	if (dumpsx && dumpsy) {
		
		dumprect= MEM_mallocN(sizeof(int)*dumpsx*dumpsy, "dumprect");
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, dumpsx, dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
		glFinish();
		glReadBuffer(GL_BACK);
		
		if(ismenu==0) {
			wasmenu= 0;
			save_image_filesel_str(imstr);
			activate_fileselect(FILE_SPECIAL, imstr, G.ima, write_screendump);
		}
		else wasmenu= 1;
	}

}
