/**
 * $Id$
 * imagepaint.c
 *
 * Functions to edit the "2D UV/Image " 
 * and handle user events sent to it.
 * 
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
 * Contributor(s): Jens Ole Wund (bjornmose)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "PIL_time.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif


#ifdef INTERNATIONAL
#include "BIF_language.h"
#endif

#include "IMB_imbuf_types.h"

#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_global.h"

#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"

#include "BDR_vpaint.h"
#include "BDR_drawmesh.h"

#include "mydevice.h"

#include "TPT_DependKludge.h"
#include "BSE_trans_types.h"
#include "IMG_Api.h"

#include "SYS_System.h" /* for the user def menu ... should move elsewhere. */



/* this data should be a new datablock, used by G.sima .. G.sima->painttoolbox.yadayada */
/* so get the rest working and then care for this */
/* using UVTEXTTOOL_ as prefix so a grep will find 'em all later*/
Image* UVTEXTTOOL_cloneimage = NULL; 
short UVTEXTTOOL_imanr= -2;
float UVTEXTTOOL_cloneoffx = 0.0;
float UVTEXTTOOL_cloneoffy = 0.0;
float UVTEXTTOOL_clonealpha = 0.5;

short UVTEXTTOOL_POS[2];
float UVTEXTTOOL_RAD[2];
short UVTEXTTOOL_SHAPE;
short UVTEXTTOOL_INDEX = 0;
short UVTEXTTOOL_uiflags = 0;
BrushUIdata UVTEXTTOOL_DATA[7] = {
	/* r,g,b,a,size,softradius,timing*/
	{ 1.0f , 1.0f , 1.0f ,0.2f , 25.0f, 0.5f,100.0f}, /* brush */
	{ 1.0f , 1.0f , 1.0f ,0.1f , 25.0f, 0.1f,100.0f},  /* air brush */
	{ 0.5f , 0.5f , 0.5f ,1.0f , 25.0f, 0.5f,100.0f}, /* soften */
	{ 1.0f , 1.0f , 1.0f ,0.1f , 25.0f, 0.1f,100.0f},
	{ 0.0f , 0.0f , 0.0f ,0.1f , 25.0f, 0.1f,100.0f},
	{ 1.0f , 0.0f , 1.0f ,0.5f , 25.0f, 0.1f, 20.0f},
	{ 1.0f , 0.0f , 1.0f ,0.5f , 25.0f, 0.1f, 20.0f}};



void texturepaintoff()
	{
		UVTEXTTOOL_SHAPE = 0;
	}

int uv_paint_panel_but(short val)
{
    /* but still i don't know if i like that crowded floating panel */
	switch(val){
	case PAINTPANELMESSAGEEATER:
		force_draw(0);/* tool changes so redraw settings */
	}
	return 0;
}

int UVtimedaction(int action)
{
	if (( action== 1.0) 
		|| (action == 2.0)
		|| (action == 3.0)
		|| (action == 4.0)) return 1;
	return 0;
}

void UVTexturePaintToolAt(short* where)
/* keep drawimage informed on actual tool position/setting */
{
	SpaceImage *sima= curarea->spacedata.first;
	BrushUIdata *data = NULL;

	data = &UVTEXTTOOL_DATA[UVTEXTTOOL_INDEX];
	if(!data) return;
	UVTEXTTOOL_POS[0] = where[0];
	UVTEXTTOOL_POS[1] = where[1];
	UVTEXTTOOL_RAD[0] = data->size*sima->zoom/2;
	UVTEXTTOOL_RAD[1] = data->softradius*data->size*sima->zoom/2;
}

void UVTexturePaintMsg( void *spacedata, unsigned short event,short val)
/* handle events in texturepaint mode of UV-Image Editor*/
{
	SpaceImage *sima= curarea->spacedata.first;
	BrushUIdata *data=NULL;
	IMG_BrushPtr brush;
	IMG_CanvasPtr canvas,clonecanvas =NULL;
	short xy_prev[2], xy_curr[2];
	static short dtxy_prev[2], dtxy_curr[2];
	float uv_prev[2], uv_curr[2];
	int rowBytes,clonerowBytes;
	double brushtime;
	int firsttouch = 1;
	float duv[2];
	float dduv;
    char extensionmode; 
	View2D *v2d= &sima->v2d;

	data = &UVTEXTTOOL_DATA[UVTEXTTOOL_INDEX];
	if (!data) return;
	switch(event){
	case UI_BUT_EVENT:
	{
		if (uv_paint_panel_but(val)) break;  
	}
	case MOUSEX:
	case MOUSEY:
		{
	/* tool preview */
			if (UVTEXTTOOL_uiflags & 2) { 
				getmouseco_areawin(dtxy_curr);
				if ( dtxy_curr[0]!=dtxy_prev[0] || dtxy_curr[1]!=dtxy_prev[1]) {
					UVTexturePaintToolAt(dtxy_curr);
					UVTEXTTOOL_SHAPE = 1;
					force_draw(0);
				}
			}
			else {
				UVTEXTTOOL_SHAPE = 0;
				
			}
			

			dtxy_prev[0] = dtxy_curr[0];
			dtxy_prev[1] = dtxy_curr[1];
			break;
		}
		
	}
	
	
	switch(event) {
	case LEFTMOUSE:
		/* Paranoia checks */
		if (!sima) break;
		if (!sima->image) break;
		if (!sima->image->ibuf) break;
		if (sima->image->packedfile) {
			error("Painting in packed images not supported");
			break;
		}
		brush = IMG_BrushCreate((int)(data->size), (int)(data->size), data->r, data->g, data->b, data->a);
		
		IMG_BrushSetInnerRaduisRatio(brush,data->softradius);
		/* skipx is not set most of the times. Make a guess. */
		rowBytes = sima->image->ibuf->skipx ? sima->image->ibuf->skipx : sima->image->ibuf->x * 4;
		canvas = IMG_CanvasCreateFromPtr(sima->image->ibuf->rect, sima->image->ibuf->x, sima->image->ibuf->y, rowBytes);
		if (UVTEXTTOOL_cloneimage){
			if (UVTEXTTOOL_cloneimage->ibuf){
				clonerowBytes = UVTEXTTOOL_cloneimage->ibuf->skipx ? UVTEXTTOOL_cloneimage->ibuf->skipx : UVTEXTTOOL_cloneimage->ibuf->x * 4;
				clonecanvas = IMG_CanvasCreateFromPtr(UVTEXTTOOL_cloneimage->ibuf->rect, UVTEXTTOOL_cloneimage->ibuf->x, UVTEXTTOOL_cloneimage->ibuf->y, clonerowBytes);
			}
		}		
		getmouseco_areawin(xy_prev);
		brushtime = PIL_check_seconds_timer();
		while (get_mbut() & L_MOUSE) {
			UVTEXTTOOL_SHAPE = 0;
			getmouseco_areawin(xy_curr);
			/* check for timed actions */
			if (UVtimedaction(UVTEXTTOOL_INDEX)){ 
				if ((PIL_check_seconds_timer()-brushtime) > (5.0/data->brushtiming) )
				{
					brushtime=PIL_check_seconds_timer();
					firsttouch = 1;
					xy_prev[0] = xy_curr[0];
					xy_prev[1] = xy_curr[1];
				}
			}
			/* check for movement actions */
			if ((xy_prev[0] != xy_curr[0]) || (xy_prev[1] != xy_curr[1]) || firsttouch) {
				/* so now we know we did move at all */
				/* Convert mouse coordinates to u,v and draw */
				areamouseco_to_ipoco(v2d, xy_prev, &uv_prev[0], &uv_prev[1]);
				areamouseco_to_ipoco(v2d, xy_curr, &uv_curr[0], &uv_curr[1]);
				/* do some gearing down in % of brush diameter*/
				duv[0] = (float)(xy_prev[0]- xy_curr[0]);
				duv[1] = (float)(xy_prev[1]- xy_curr[1]);
				dduv = (float)sqrt(duv[0] * duv[0] + duv[1] * duv[1]);
				if ((dduv < (data->size*sima->zoom  * data->brushtiming/200.0) ) && (firsttouch == 0)){
					if (UVTEXTTOOL_uiflags & 1){ /* this spoils all efforts reducing redraw needs */
						static short m_prev[2];
						/* doing a brute force toolshape update by backbuffer drawing */
						if ((m_prev[0] != xy_curr[0]) || (m_prev[1] != xy_curr[1])) {
							UVTexturePaintToolAt(xy_curr);
							UVTEXTTOOL_SHAPE = UVTEXTTOOL_uiflags & 1;
							force_draw(0);
						}
						m_prev[0] = xy_curr[0];
						m_prev[1] = xy_curr[1];
					}
					continue;
				}
				/* respect timed actions */
				if (UVtimedaction(UVTEXTTOOL_INDEX) && (firsttouch == 0)){
					continue;
				}
				
				
				firsttouch = 0;
				if (UVTEXTTOOL_uiflags & 4) 
					extensionmode = 't'; 
				else
					extensionmode = 'c'; 
				switch(UVTEXTTOOL_INDEX) {
				case 2:
					IMG_CanvasSoftenAt(canvas,uv_prev[0],uv_prev[1],(int)(data->size),data->a,data->softradius,extensionmode);
					break;
				case 5:
					IMG_CanvasSmear(canvas,uv_prev[0], uv_prev[1], uv_curr[0], uv_curr[1],(int)(data->size),data->a,data->softradius,extensionmode);
					break;
				case 6:
					IMG_CanvasCloneAt(canvas,clonecanvas,uv_prev[0],uv_prev[1],UVTEXTTOOL_cloneoffx,UVTEXTTOOL_cloneoffy,(int)(data->size),data->a,data->softradius);
					break;
				default:			
//					IMG_CanvasDrawLineUVEX(canvas, brush, uv_prev[0], uv_prev[1], uv_curr[0], uv_curr[1],'c');
					IMG_CanvasDrawLineUVEX(canvas, brush, uv_prev[0], uv_prev[1], uv_curr[0], uv_curr[1],extensionmode);
				}
				
				if (G.sima->lock) {
					/* Make OpenGL aware of a changed texture */
					free_realtime_image(sima->image);
					/* Redraw this view and the 3D view */
					UVTexturePaintToolAt(xy_curr);
					UVTEXTTOOL_SHAPE = UVTEXTTOOL_uiflags & 1;
					force_draw_plus(SPACE_VIEW3D,0);
					
				}
				else {
					/* Redraw only this view */
					UVTexturePaintToolAt(xy_curr);
					UVTEXTTOOL_SHAPE = UVTEXTTOOL_uiflags & 1;
					force_draw(0);
				}
				
				xy_prev[0] = xy_curr[0];
				xy_prev[1] = xy_curr[1];
			}
		}
		UVTEXTTOOL_SHAPE = UVTEXTTOOL_uiflags & 2;
		/* Set the dirty bit in the image so that it is clear that it has been modified. */
		sima->image->ibuf->userflags |= IB_BITMAPDIRTY;
		if (!G.sima->lock) {
			/* Make OpenGL aware of a changed texture */
			free_realtime_image(sima->image);
			/* Redraw this view and the 3D view */
			force_draw_plus(SPACE_VIEW3D,0);
		}
		IMG_BrushDispose(brush);
		IMG_CanvasDispose(canvas);
		if (clonecanvas) IMG_CanvasDispose(clonecanvas);
		allqueue(REDRAWHEADERS, 0);

		break;
			case RIGHTMOUSE:
				{
					extern float UVTEXTTOOL_cloneoffx;
					extern float UVTEXTTOOL_cloneoffy;
					/* call the color lifter */
					if (UVTEXTTOOL_INDEX==6){
						getmouseco_areawin(xy_prev);
						while (get_mbut() & R_MOUSE) {
							getmouseco_areawin(xy_curr);
							/* check for movement actions */
							if ((xy_prev[0] != xy_curr[0]) || (xy_prev[1] != xy_curr[1]) ) {
								/* so now we know we did move at all */
								/* Convert mouse coordinates to u,v and draw */
								areamouseco_to_ipoco(v2d, xy_prev, &uv_prev[0], &uv_prev[1]);
								areamouseco_to_ipoco(v2d, xy_curr, &uv_curr[0], &uv_curr[1]);
								UVTEXTTOOL_cloneoffx += uv_curr[0] -uv_prev[0];
								UVTEXTTOOL_cloneoffy += uv_curr[1] -uv_prev[1];
								force_draw(0);
								
								
								
							}
							xy_prev[0] = xy_curr[0];
							xy_prev[1] = xy_curr[1];
							
						}
					}
					else sample_vpaint();
					break;
				}
				
		}
		
}
