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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"  // ImBuf{}

#include "BLI_blenlib.h"

#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h" // EnvMap{}
#include "DNA_image_types.h" // Image{}

#include "BKE_global.h"		// struct G
#include "BKE_image.h"
#include "BKE_utildefines.h" // ELEM

#include "BIF_screen.h"		// waitcursor
#include "BIF_toolbox.h"
#include "BIF_writeimage.h"

#include "BSE_filesel.h"

#include "RE_pipeline.h"

/* ------------------------------------------------------------------------- */


void BIF_save_envmap(EnvMap *env, char *str)
{
	ImBuf *ibuf;
/*  	extern rectcpy(); */
	int dx;
	
	/* all interactive stuff is handled in buttons.c */
	if(env->cube[0]->ibuf==NULL) return;
		
	dx= env->cube[0]->ibuf->x;
	ibuf= IMB_allocImBuf(3*dx, 2*dx, 24, IB_rect, 0);
	
	IMB_rectcpy(ibuf, env->cube[0]->ibuf, 0, 0, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[1]->ibuf, dx, 0, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[2]->ibuf, 2*dx, 0, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[3]->ibuf, 0, dx, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[4]->ibuf, dx, dx, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[5]->ibuf, 2*dx, dx, 0, 0, dx, dx);
	
	BKE_write_ibuf(ibuf, str, G.scene->r.imtype, G.scene->r.subimtype, G.scene->r.quality);
	IMB_freeImBuf(ibuf);
}


/* callback for fileselect to save rendered image, renderresult was checked to exist */
static void save_rendered_image_cb(char *name)
{
	char str[FILE_MAXDIR+FILE_MAXFILE];
	
	if(BLI_testextensie(name,".blend")) {
		error("Wrong filename");
		return;
	}
	
	/* BKE_add_image_extension() checks for if extension was already set */
	if(G.scene->r.scemode & R_EXTENSION) 
		if(strlen(name)<FILE_MAXDIR+FILE_MAXFILE-5)
			BKE_add_image_extension(name, G.scene->r.imtype);

	strcpy(str, name);
	BLI_convertstringcode(str, G.sce, G.scene->r.cfra);
	
	if(saveover(str)) {
		RenderResult rres;
		ImBuf *ibuf;
		
		RE_GetResultImage(RE_GetRender("Render"), &rres);

		waitcursor(1); /* from screen.c */

		ibuf= IMB_allocImBuf(rres.rectx, rres.recty, G.scene->r.planes, 0, 0);
		ibuf->rect= rres.rect32;
		ibuf->rect_float= rres.rectf;
		ibuf->zbuf_float= rres.rectz;
		
		BKE_write_ibuf(ibuf, str, G.scene->r.imtype, G.scene->r.subimtype, G.scene->r.quality);
		IMB_freeImBuf(ibuf);	/* imbuf knows rects are not part of ibuf */
		
		strcpy(G.ima, name);
		
		waitcursor(0);
	}
}

void save_image_filesel_str(char *str)
{
	switch(G.scene->r.imtype) {
		case R_RADHDR:
			strcpy(str, "Save Radiance HDR");
			break;
		case R_PNG:
			strcpy(str, "Save PNG");
			break;
		case R_BMP:
			strcpy(str, "Save BMP");
			break;
		case R_TIFF:
			if (G.have_libtiff)
				strcpy(str, "Save TIFF");
			break;
#ifdef WITH_OPENEXR
		case R_OPENEXR:
			strcpy(str, "Save OpenEXR");
			break;
#endif
		case R_RAWTGA:
			strcpy(str, "Save Raw Targa");
			break;
		case R_IRIS:
			strcpy(str, "Save IRIS");
			break;
		case R_IRIZ:
			strcpy(str, "Save IRIS");
			break;
		case R_HAMX:
			strcpy(str, "Save HAMX");
			break;
		case R_JPEG90:
		case R_MOVIE:
		case R_AVICODEC:
		case R_AVIRAW:
		case R_AVIJPEG:
			strcpy(str, "Save JPEG");
			break;
			
		case R_TARGA:	/* default targa */
		default:
			strcpy(str, "Save Targa");
			break;
	}	
}

/* calls fileselect */
void BIF_save_rendered_image_fs(void)
{
	RenderResult *rr= RE_GetResult(RE_GetRender("Render"));
	
	if(!rr) {
		error("No image rendered");
	} else {
		char dir[FILE_MAXDIR * 2], str[FILE_MAXFILE * 2];
		
		if(G.ima[0]==0) {
			strcpy(dir, G.sce);
			BLI_splitdirstring(dir, str);
			strcpy(G.ima, dir);
		}
		
		save_image_filesel_str(str);
		activate_fileselect(FILE_SPECIAL, str, G.ima, save_rendered_image_cb);
	}
}

