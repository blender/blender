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
 * Making screendumps.
 */

#include <string.h>

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_sca.h"

#include "BIF_gl.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_toets.h"

#include "BSE_filesel.h"

#include "render.h"
#include "mydevice.h"

static unsigned int *dumprect=0;
static int dumpsx, dumpsy;

void write_screendump(char *name);

void write_screendump(char *name)
{
	ImBuf *ibuf;
	
	if(dumprect) {

		strcpy(G.ima, name);
		BLI_convertstringcode(name, G.sce, G.scene->r.cfra);
		
		if(saveover(name)) {
			waitcursor(1);
			
			ibuf= IMB_allocImBuf(dumpsx, dumpsy, 24, 0, 0);
			ibuf->rect= dumprect;
			
			if(G.scene->r.imtype== R_IRIS) ibuf->ftype= IMAGIC;
			else if(G.scene->r.imtype==R_IRIZ) ibuf->ftype= IMAGIC;
			else if(G.scene->r.imtype==R_TARGA) ibuf->ftype= TGA;
			else if(G.scene->r.imtype==R_RAWTGA) ibuf->ftype= RAWTGA;
			else if(G.scene->r.imtype==R_PNG) ibuf->ftype= PNG;
			else if(G.scene->r.imtype==R_HAMX) ibuf->ftype= AN_hamx;
			else if(ELEM5(G.scene->r.imtype, R_MOVIE, R_AVICODEC, R_AVIRAW, R_AVIJPEG, R_JPEG90)) {
				ibuf->ftype= JPG|G.scene->r.quality;
			}
			else ibuf->ftype= TGA;	

			IMB_saveiff(ibuf, name, IB_rect);
			IMB_freeImBuf(ibuf);
			
			waitcursor(0);
		}
		MEM_freeN(dumprect);
		dumprect= 0;
	}
}


void BIF_screendump(void)
{
	/* dump pakken van frontbuffer */
	int x=0, y=0;
	char imstr[32];
	
	dumpsx= 0;
	dumpsy= 0;
	
	if(dumprect) MEM_freeN(dumprect);
	dumprect= 0;
	
	if (G.qual & LR_SHIFTKEY) {
		x= 0;
		y= 0;
		
		dumpsx= G.curscreen->sizex;
		dumpsy= G.curscreen->sizey;
		
	} 
	else {
		if(mywin_inmenu()) {
			mywin_getmenu_rect(&x, &y, &dumpsx, &dumpsy);
		} else {
			int win= mywinget();

			bwin_getsuborigin(win, &x, &y);
			bwin_getsize(win, &dumpsx, &dumpsy);
		}
	}
	
	if (dumpsx && dumpsy) {
		save_image_filesel_str(imstr);
		
		dumprect= MEM_mallocN(sizeof(int)*dumpsx*dumpsy, "dumprect");
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, dumpsx, dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
	
		/* filesel openen */
		
		activate_fileselect(FILE_SPECIAL, imstr, G.ima, write_screendump);
	}	
}
