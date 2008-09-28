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
	if(env->cube[0]==NULL) return;
		
	dx= env->cube[0]->x;
	ibuf= IMB_allocImBuf(3*dx, 2*dx, 24, IB_rect, 0);
	
	IMB_rectcpy(ibuf, env->cube[0], 0, 0, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[1], dx, 0, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[2], 2*dx, 0, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[3], 0, dx, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[4], dx, dx, 0, 0, dx, dx);
	IMB_rectcpy(ibuf, env->cube[5], 2*dx, dx, 0, 0, dx, dx);
	
	BKE_write_ibuf(ibuf, str, G.scene->r.imtype, G.scene->r.subimtype, G.scene->r.quality);
	IMB_freeImBuf(ibuf);
}



/* callback for fileselect to save rendered image, renderresult was checked to exist */
static void save_rendered_image_cb_real(char *name, int confirm)
{
	char str[FILE_MAXDIR+FILE_MAXFILE];
	int overwrite;
	
	if(BLI_testextensie(name,".blend")) {
		error("Wrong filename");
		return;
	}
	
	/* BKE_add_image_extension() checks for if extension was already set */
	if(G.scene->r.scemode & R_EXTENSION) 
		if(strlen(name)<FILE_MAXDIR+FILE_MAXFILE-5)
			BKE_add_image_extension(name, G.scene->r.imtype);

	strcpy(str, name);
	BLI_convertstringcode(str, G.sce);
	BLI_convertstringframe(str, G.scene->r.cfra); /* TODO - is this even used? */

	if (confirm)
		overwrite = saveover(str);
	else
		overwrite = 1;
	
	if(overwrite) {
		if(G.scene->r.imtype==R_MULTILAYER) {
			RenderResult *rr= RE_GetResult(RE_GetRender(G.scene->id.name));
			if(rr) 
				RE_WriteRenderResult(rr, str, G.scene->r.quality);
		}
		else {
			RenderResult rres;
			ImBuf *ibuf;
			
			RE_GetResultImage(RE_GetRender(G.scene->id.name), &rres);

			waitcursor(1); /* from screen.c */

			ibuf= IMB_allocImBuf(rres.rectx, rres.recty, G.scene->r.planes, 0, 0);
			ibuf->rect= (unsigned int *)rres.rect32;
			ibuf->rect_float= rres.rectf;
			ibuf->zbuf_float= rres.rectz;
			
			/* float factor for random dither, imbuf takes care of it */
			ibuf->dither= G.scene->r.dither_intensity;
			
			BKE_write_ibuf(ibuf, str, G.scene->r.imtype, G.scene->r.subimtype, G.scene->r.quality);
			IMB_freeImBuf(ibuf);	/* imbuf knows rects are not part of ibuf */
		}
		
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
		case R_FFMPEG:
		case R_PNG:
			strcpy(str, "Save PNG");
			break;
#ifdef WITH_DDS
		case R_DDS:
			strcpy(str, "Save DDS");
			break;
#endif
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
		case R_CINEON:
			strcpy(str, "Save Cineon");
			break;
		case R_DPX:
			strcpy(str, "Save DPX");
			break;
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
		case R_TARGA:
			strcpy(str, "Save Targa");
			break;
		case R_MULTILAYER:
			strcpy(str, "Save Multi Layer EXR");
			break;
			/* default we save jpeg, also for all movie formats */
		case R_JPEG90:
		case R_MOVIE:
		case R_AVICODEC:
		case R_AVIRAW:
		case R_AVIJPEG:
		default:
			strcpy(str, "Save JPEG");
			break;
	}	
}

static void save_rendered_image_cb(char *name) 
{
	save_rendered_image_cb_real(name, 1);
}

/* no fileselect, no confirm */
void BIF_save_rendered_image(char *name)
{
	save_rendered_image_cb_real(name, 0);
}
	
/* calls fileselect */
void BIF_save_rendered_image_fs(void)
{
	RenderResult rres;
	
	RE_GetResultImage(RE_GetRender(G.scene->id.name), &rres);

	if(!rres.rectf && !rres.rect32) {
		error("No image rendered");
	} 
	else {
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


