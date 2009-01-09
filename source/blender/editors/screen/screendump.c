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
 * Contributor(s): Blender Foundation
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

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "WM_types.h"
#include "WM_api.h"

static unsigned int *dumprect= NULL;
static int dumpsx, dumpsy;

/* XXX */
static int saveover() {return 0;}

/* Callback */
void write_screendump(bContext *C, char *name)
{
	Scene *scene= CTX_data_scene(C);
	ImBuf *ibuf;
	
	if(dumprect) {

		strcpy(G.ima, name);
		BLI_convertstringcode(name, G.sce);
		BLI_convertstringframe(name, scene->r.cfra); /* TODO - is this ever used? */
		
		/* BKE_add_image_extension() checks for if extension was already set */
		if(scene->r.scemode & R_EXTENSION) 
			if(strlen(name)<FILE_MAXDIR+FILE_MAXFILE-5)
				BKE_add_image_extension(scene, name, scene->r.imtype);
		
		if(saveover(name)) {
//			waitcursor(1);
			
			ibuf= IMB_allocImBuf(dumpsx, dumpsy, 24, 0, 0);
			ibuf->rect= dumprect;
			
			if(scene->r.planes == 8) IMB_cspace(ibuf, rgb_to_bw);
			
			BKE_write_ibuf(scene, ibuf, name, scene->r.imtype, scene->r.subimtype, scene->r.quality);

			IMB_freeImBuf(ibuf);
		
//			waitcursor(0);
		}
		MEM_freeN(dumprect);
		dumprect= NULL;
	}
}

/* get dump from frontbuffer */
void ED_screendump(bContext *C, int fscreen)
{
	wmWindow *win= CTX_wm_window(C);
	ScrArea *curarea= CTX_wm_area(C);
	int x=0, y=0;
//	char imstr[64];

	if(dumprect) MEM_freeN(dumprect);
	dumprect= NULL;
	
	if(fscreen) {	/* full screen */
		x= 0;
		y= 0;
		dumpsx= win->sizex;
		dumpsy= win->sizey;
		
	} 
	else {
		x= curarea->totrct.xmin;
		y= curarea->totrct.ymin;
		dumpsx= curarea->totrct.xmax-x;
		dumpsy= curarea->totrct.ymax-y;
	}
	
	if (dumpsx && dumpsy) {
		
		dumprect= MEM_mallocN(sizeof(int)*dumpsx*dumpsy, "dumprect");
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, dumpsx, dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
		glFinish();
		glReadBuffer(GL_BACK);

		//			save_image_filesel_str(imstr);
		//			activate_fileselect(FILE_SPECIAL, imstr, G.ima, write_screendump);
	}

}
