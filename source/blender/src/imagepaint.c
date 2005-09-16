/**
 * $Id$
 * imagepaint.c
 *
 * Functions to edit the "2D UV/Image " 
 * and handle user events sent to it.
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

#include "IMB_imbuf_types.h"

#include "DNA_image_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_global.h"

#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"
#include "BSE_trans_types.h"

#include "BDR_drawmesh.h"
#include "BDR_imagepaint.h"
#include "BDR_vpaint.h"

#include "IMG_Api.h"

#include "mydevice.h"

struct ImagePaint Gip = {
	{NULL, {0.0f, 0.0f}, 0.5f},
	 {{{1.0f, 1.0f, 1.0f, 0.2f}, 25, 0.5f, 100.0f}, /* brush */
	 {{1.0f, 1.0f, 1.0f, 0.1f}, 25, 0.1f, 100.0f},  /* airbrush */
	 {{0.5f, 0.5f, 0.5f, 1.0f}, 25, 0.5f, 100.0f},  /* soften */
	 {{1.0f, 1.0f, 1.0f, 0.1f}, 25, 0.1f, 100.0f},  /* aux1 */
	 {{0.0f, 0.0f, 0.0f, 0.1f}, 25, 0.1f, 100.0f},  /* aux2 */
	 {{1.0f, 1.0f, 1.0f, 0.5f}, 25, 0.1f,  20.0f},  /* smear */
	 {{1.0f, 1.0f, 1.0f, 0.5f}, 25, 0.1f,  20.0f}}, /* clone */
	 0, IMAGEPAINT_BRUSH
};

static int imagepaint_init(IMG_BrushPtr **brush, IMG_CanvasPtr **canvas, IMG_CanvasPtr **clonecanvas)
{
	ImBuf *ibuf= NULL, *cloneibuf= NULL;
	ImagePaintTool *tool= &Gip.tool[Gip.current];

	/* verify that we can paint */
	if(!G.sima->image || !G.sima->image->ibuf)
		return 0;
	else if(G.sima->image->packedfile) {
		error("Painting in packed images not supported");
		return 0;
	}

	ibuf= G.sima->image->ibuf;

	if(Gip.current == IMAGEPAINT_CLONE) {
		if(!Gip.clone.image || !Gip.clone.image->ibuf)
			return 0;

		cloneibuf= Gip.clone.image->ibuf;
	}

	/* create brush */
	*brush= IMG_BrushCreate(tool->size, tool->size, tool->rgba);
	IMG_BrushSetInnerRaduisRatio(*brush, tool->innerradius);

	/* create canvas */
	*canvas= IMG_CanvasCreateFromPtr(ibuf->rect, ibuf->x, ibuf->y, ibuf->x*4);

	if(Gip.current == IMAGEPAINT_CLONE) {
		int w= cloneibuf->x, h= cloneibuf->y;
		*clonecanvas= IMG_CanvasCreateFromPtr(cloneibuf->rect, w, h, cloneibuf->x*4);
	}
	else
		*clonecanvas= NULL;
	
	/* initialize paint settings */
	if(Gip.current >= IMAGEPAINT_AIRBRUSH && Gip.current <= IMAGEPAINT_SOFTEN)
		Gip.flag |= IMAGEPAINT_TIMED;
	else
		Gip.flag &= ~IMAGEPAINT_TIMED;
	
	return 1;
}

static void imagepaint_free(IMG_BrushPtr *brush, IMG_CanvasPtr *canvas, IMG_CanvasPtr *clonecanvas)
{
	IMG_BrushDispose(brush);
	IMG_CanvasDispose(canvas);

	if(Gip.current == IMAGEPAINT_CLONE)
		IMG_CanvasDispose(clonecanvas);
}

void imagepaint_redraw_tool(void)
{
	if(Gip.flag & IMAGEPAINT_DRAW_TOOL_DRAWING)
		force_draw(0);
}

static void imagepaint_redraw(int final, int painted)
{
	if(!final && !painted) {
		imagepaint_redraw_tool();
		return;
	}

	if(final || painted) {
		if (final || G.sima->lock) {
			/* Make OpenGL aware of a changed texture */
			free_realtime_image(G.sima->image);
			force_draw_plus(SPACE_VIEW3D,0);
		}
		else
			force_draw(0);
	}

	if(final)
		allqueue(REDRAWHEADERS, 0);
}

static void imagepaint_compute_uvco(short *mval, float *uv)
{
	areamouseco_to_ipoco(G.v2d, mval, &uv[0], &uv[1]);
}

static void imagepaint_paint_tool(IMG_BrushPtr *brush, IMG_CanvasPtr *canvas, IMG_CanvasPtr *clonecanvas, float *prevuv, float *uv)
{
	int torus = Gip.flag & IMAGEPAINT_TORUS;
	ImagePaintTool *tool= &Gip.tool[Gip.current];

	if(Gip.current == IMAGEPAINT_SOFTEN)
		IMG_CanvasSoftenAt(canvas, prevuv[0], prevuv[1], tool->size, tool->rgba[3], tool->innerradius, torus);
	else if(Gip.current == IMAGEPAINT_SMEAR)
		IMG_CanvasSmear(canvas, prevuv[0], prevuv[1], uv[0], uv[1], tool->size, tool->rgba[3], tool->innerradius, torus);
	else if(Gip.current == IMAGEPAINT_CLONE) {
		float offx= Gip.clone.offset[0];
		float offy= Gip.clone.offset[1];

		IMG_CanvasCloneAt(canvas, clonecanvas, prevuv[0], prevuv[1], offx, offy, tool->size, tool->rgba[3], tool->innerradius);
	}
	else
		IMG_CanvasDrawLineUVEX(canvas, brush, prevuv[0], prevuv[1], uv[0], uv[1], torus);
}

void imagepaint_paint(short mousebutton)
{
	IMG_BrushPtr *brush;
	IMG_CanvasPtr *canvas, *clonecanvas;
	short prevmval[2], mval[2];
	double prevtime, curtime;
	float prevuv[2], uv[2];
	int paint= 0, moved= 0;
	ImagePaintTool *tool= &Gip.tool[Gip.current];

	if(!imagepaint_init(&brush, &canvas, &clonecanvas))
		return;
	
	getmouseco_areawin(prevmval);
	prevtime = PIL_check_seconds_timer();

	Gip.flag |= IMAGEPAINT_DRAWING;

	while(get_mbut() & mousebutton) {
		getmouseco_areawin(mval);

		moved= paint= (prevmval[0] != mval[0]) || (prevmval[1] != mval[1]);

		if(Gip.flag & IMAGEPAINT_TIMED) {
			/* see if need to draw because of timer */
			curtime = PIL_check_seconds_timer();

			if((curtime - prevtime) > (5.0/tool->timing)) {
				prevtime= curtime;
				paint= 1;
			}
		}
		else if(paint) {
			/* check if we moved enough to draw */
			float dmval[2], d, dlimit;

			dmval[0]= prevmval[0] - mval[0];
			dmval[1]= prevmval[1] - mval[1];

			d= sqrt(dmval[0]*dmval[0] + dmval[1]*dmval[1]);
			dlimit= tool->size*G.sima->zoom*tool->timing/200.0;

			if (d < dlimit)
				paint= 0;
		}

		if(paint) {
			/* do the actual painting */
			imagepaint_compute_uvco(prevmval, prevuv);
			imagepaint_compute_uvco(mval, uv);

			imagepaint_paint_tool(brush, canvas, clonecanvas, prevuv, uv);

			prevmval[0]= mval[0];
			prevmval[1]= mval[1];
		}

		if(paint)
			imagepaint_redraw(0, paint);
		else if(moved && (Gip.flag & IMAGEPAINT_DRAW_TOOL))
			imagepaint_redraw(0, paint);
	}

	Gip.flag &= ~IMAGEPAINT_DRAWING;

	imagepaint_free(brush, canvas, clonecanvas);
	G.sima->image->ibuf->userflags |= IB_BITMAPDIRTY;

	imagepaint_redraw(1, 0);
}

void imagepaint_pick(short mousebutton)
{
	ImagePaintTool *tool= &Gip.tool[Gip.current];

	if(Gip.current == IMAGEPAINT_CLONE) {
		if(Gip.clone.image && Gip.clone.image->ibuf) {
			short prevmval[2], mval[2];
			float prevuv[2], uv[2];
		
			getmouseco_areawin(prevmval);

			while(get_mbut() & mousebutton) {
				getmouseco_areawin(mval);

				if((prevmval[0] != mval[0]) || (prevmval[1] != mval[1]) ) {
					/* mouse moved, so move the clone image */
					imagepaint_compute_uvco(prevmval, prevuv);
					imagepaint_compute_uvco(mval, uv);

					Gip.clone.offset[0] += uv[0] - prevuv[0];
					Gip.clone.offset[1] += uv[1] - prevuv[1];

					force_draw(0);

					prevmval[0]= mval[0];
					prevmval[1]= mval[1];
				}
			}
		}
	}
	else {
		extern VPaint Gvp;

		sample_vpaint();
		tool->rgba[0]= Gvp.r;
		tool->rgba[1]= Gvp.g;
		tool->rgba[2]= Gvp.b;
	}
}

