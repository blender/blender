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

#include "MEM_guardedalloc.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"

#include "BDR_drawmesh.h"
#include "BDR_imagepaint.h"
#include "BDR_vpaint.h"

#include "blendef.h"
#include "mydevice.h"

/* ImagePaintPixmap */

#define IMAPAINT_FLOAT_TO_CHAR(f) ((char)(f*255))
#define IMAPAINT_CHAR_TO_FLOAT(c) (c/255.0f)
#define IMAPAINT_FLOAT_CLAMP(f) ((f < 0.0)? 0.0: (f > 1.0)? 1.0: f)

#define IMAPAINT_FLOAT_RGB_TO_CHAR(c, f) { c[0]=IMAPAINT_FLOAT_TO_CHAR(f[0]); \
	c[1]=IMAPAINT_FLOAT_TO_CHAR(f[1]); c[2]=IMAPAINT_FLOAT_TO_CHAR(f[2]); }
#define IMAPAINT_FLOAT_RGBA_TO_CHAR(c, f) { \
	c[0]=IMAPAINT_FLOAT_TO_CHAR(f[0]); c[1]=IMAPAINT_FLOAT_TO_CHAR(f[1]); \
	c[2]=IMAPAINT_FLOAT_TO_CHAR(f[2]); c[2]=IMAPAINT_FLOAT_TO_CHAR(f[3]);}
#define IMAPAINT_CHAR_RGB_TO_FLOAT(f, c) { f[0]=IMAPAINT_CHAR_TO_FLOAT(c[0]); \
	f[1]=IMAPAINT_CHAR_TO_FLOAT(c[1]); f[2]=IMAPAINT_CHAR_TO_FLOAT(c[2]); }
#define IMAPAINT_CHAR_RGBA_TO_FLOAT(f, c) { \
	f[0]=IMAPAINT_CHAR_TO_FLOAT(c[0]); f[1]=IMAPAINT_CHAR_TO_FLOAT(c[1]); \
	f[2]=IMAPAINT_CHAR_TO_FLOAT(c[2]); f[3]=IMAPAINT_CHAR_TO_FLOAT(c[3]); }

#define IMAPAINT_FLOAT_RGB_COPY(a, b) VECCOPY(a, b)
#define IMAPAINT_FLOAT_RGB_ADD(a, b) VECADD(a, a, b)

#define IMAPAINT_RGB_COPY(a, b) { a[0]=b[0]; a[1]=b[1]; a[2]=b[2]; }
#define IMAPAINT_RGBA_COPY(a, b) { *((int*)a)=*((int*)b); }

typedef struct ImagePaintPixmap {
	unsigned int width, height, rowbytes, shared;
	char *rect;
} ImagePaintPixmap;

static ImagePaintPixmap *imapaint_pixmap_new(unsigned int w, unsigned int h, char *rect)
{
	ImagePaintPixmap *pm = MEM_callocN(sizeof(ImagePaintPixmap), "ImagePaintPixmap");

	pm->width = w;
	pm->height = h;
	pm->rowbytes = sizeof(char)*w*4;

	if (rect) {
		pm->rect = rect;
		pm->shared = 1;
	}
	else
		pm->rect = MEM_mallocN(pm->rowbytes*h, "ImagePaintPixmapRect");
	
	return pm;
}

static void imapaint_pixmap_free(ImagePaintPixmap *pm)
{
	if (!pm->shared)
		MEM_freeN(pm->rect);
	MEM_freeN(pm);
}

/* ImagePaintBrush */

typedef struct ImagePaintBrush {
	ImagePaintPixmap *pixmap;
	float rgb[3], alpha;
	unsigned int inner_radius, outer_radius;
	short torus, blend;
} ImagePaintBrush;

static void imapaint_brush_pixmap_refresh(ImagePaintBrush *brush)
{
	ImagePaintPixmap *pm = brush->pixmap;
	char *dst, src[4], src_alpha[4];
	unsigned int y, x, outer, inner;
	float w_2, h_2, dX, dY, d, a;

	w_2 = pm->width/2.0f;
	h_2 = pm->height/2.0f;

	outer = brush->outer_radius;
	inner = brush->inner_radius;

	IMAPAINT_FLOAT_RGB_TO_CHAR(src, brush->rgb);
	src[3] = 0;
	IMAPAINT_RGB_COPY(src_alpha, src);
	src_alpha[3] = IMAPAINT_FLOAT_TO_CHAR(brush->alpha);

	for (y=0; y < pm->height; y++) {
		dst = pm->rect + y*pm->rowbytes;

		for (x=0; x < pm->width; x++, dst+=4) {
			dX = x + 0.5f - w_2;
			dY = y + 0.5f - h_2;
			d = sqrt(dX*dX + dY*dY);

			if (d <= inner) {
				IMAPAINT_RGBA_COPY(dst, src_alpha);
			}
			else if ((d < outer) && (inner < outer)) {
				a = sqrt((d - inner)/(outer - inner));
				a = (1 - a)*brush->alpha;

				IMAPAINT_RGB_COPY(dst, src);
				dst[3] = IMAPAINT_FLOAT_TO_CHAR(a);
			}
			else {
				IMAPAINT_RGBA_COPY(dst, src);
			}
		}
	}
}

static void imapaint_brush_set_radius_ratio(ImagePaintBrush *brush, float ratio)
{
	ImagePaintPixmap *pm = brush->pixmap;
	unsigned int si, w_2 = pm->width/2, h_2 = pm->height/2;

	si = (pm->width < pm->height)? pm->width: pm->height;
	brush->inner_radius = (int)((ratio*si)/2);
	brush->outer_radius = si/2;

	if (brush->outer_radius > w_2)
		brush->outer_radius = w_2;
	if (brush->outer_radius > h_2)
		brush->outer_radius = h_2;
	if (brush->inner_radius > brush->outer_radius)
		brush->inner_radius = brush->outer_radius;
}

static ImagePaintBrush *imapaint_brush_new(unsigned int w, unsigned int h, float *rgb, float alpha, float radius_ratio)
{
	ImagePaintBrush *brush = MEM_callocN(sizeof(ImagePaintBrush), "ImagePaintBrush");

	IMAPAINT_FLOAT_RGB_COPY(brush->rgb, rgb);
	brush->alpha = alpha;
	brush->pixmap = imapaint_pixmap_new(w, h, NULL);

	imapaint_brush_set_radius_ratio(brush, radius_ratio);
	imapaint_brush_pixmap_refresh(brush);

	return brush;
}

static void imapaint_brush_free(ImagePaintBrush *brush)
{
	imapaint_pixmap_free(brush->pixmap);
	MEM_freeN(brush);
}

/* ImagePaintPixmap Utilities */

static char *imapaint_pixmap_get_rgba(ImagePaintPixmap *pm, unsigned int x, unsigned int y)
{
	return &pm->rect[pm->rowbytes*y + x*4];
}

static char *imapaint_pixmap_get_rgba_torus(ImagePaintPixmap *pm, unsigned int x, unsigned int y)
{
	x %= pm->width;
	y %= pm->height;

	return &pm->rect[pm->rowbytes*y + x*4];
}

static void imapaint_pixmap_clip(ImagePaintPixmap *pm, ImagePaintPixmap *bpm, float *pos, unsigned int *off, unsigned int *boff, unsigned int *dim)
{
	int x = (int)(pos[0]*pm->width - bpm->width/2);
	int y = (int)(pos[1]*pm->height - bpm->height/2);

	dim[0] = bpm->width;
	dim[1] = bpm->height;

	if (((x + (int)dim[0]) <= 0) || (x >= (int)pm->width) ||
	    ((y + (int)dim[1]) <= 0) || (y >= (int)pm->height)) {
		dim[0] = 0;
		dim[1] = 0;
		return;
	}

	if (x < 0) {
		dim[0] += x;
		off[0] = 0;
		boff[0] = -x;
	}
	else {
		off[0] = x;
		boff[0] = 0;
	}

	if (y < 0) {
		dim[1] += y;
		off[1] = 0;
		boff[1] = -y;
	}
	else {
		off[1] = y;
		boff[1] = 0;
	}

	if (off[0] + dim[0] > pm->width)
		dim[0] -= (off[0] + dim[0]) - pm->width;
	if (off[1] + dim[1] > pm->height)
		dim[1] -= (off[1] + dim[1]) - pm->height;
}

static void imapaint_pixmap_blend(ImagePaintPixmap *pm, ImagePaintPixmap *bpm, float *pos, short mode)
{
	unsigned int x, y, dim[2], out_off[2], in_off[2];
	char *out, *in;

	imapaint_pixmap_clip(pm, bpm, pos, out_off, in_off, dim);

	if ((dim[0] == 0) || (dim[1] == 0))
		return;

	for (y=0; y < dim[1]; y++) {
		out = imapaint_pixmap_get_rgba(pm, out_off[0], out_off[1]+y);
		in = imapaint_pixmap_get_rgba(bpm, in_off[0], in_off[1]+y);

		for (x=0; x < dim[0]; x++, out+=4, in+=4)
			brush_blend_rgb(out, out, in, in[3], mode);
	}
}

static void imapaint_pixmap_blend_torus(ImagePaintPixmap *pm, ImagePaintPixmap *bpm, float *pos, short mode)
{
	unsigned int x, y, out_off[2], mx, my;
	char *out, *in;

	out_off[0] = (int)(pos[0]*pm->width - bpm->width/2);
	out_off[1] = (int)(pos[1]*pm->height - bpm->height/2);

	for (y=0; y < bpm->height; y++) {
		in = imapaint_pixmap_get_rgba(bpm, 0, y);

		for (x=0; x < bpm->width; x++, out+=4, in+=4) {
			mx = (out_off[0]+x) % pm->width;
			my = (out_off[1]+y) % pm->height;
			out = imapaint_pixmap_get_rgba(pm, mx, my);

			brush_blend_rgb(out, out, in, in[3], mode);
		}
	}
}

static int imapaint_pixmap_add_if(ImagePaintPixmap *pm, unsigned int x, unsigned int y, float *outrgb, short torus)
{
	char *inrgb;
	float finrgb[3];

	if ((x >= pm->width) || (y >= pm->height)) {
		if (torus)
			inrgb = imapaint_pixmap_get_rgba_torus(pm, x, y);
		else
			return 0;
	}
	else
		inrgb = imapaint_pixmap_get_rgba(pm, x, y);

	IMAPAINT_CHAR_RGB_TO_FLOAT(finrgb, inrgb);
	IMAPAINT_FLOAT_RGB_ADD(outrgb, finrgb);

	return 1;
}

/* ImagePaintPixmap Tools */

static void imapaint_blend_line(ImagePaintPixmap *pm, ImagePaintBrush *brush, float *start, float *end)
{
	float numsteps, t, pos[2];
	int step, d[2];

	d[0] = (int)((end[0] - start[0])*pm->width);
	d[1] = (int)((end[1] - start[1])*pm->height);
	numsteps = sqrt(d[0]*d[0] + d[1]*d[1])/(brush->pixmap->width/4.0f);

	if(numsteps < 1.0)
		numsteps = 1.0f;

	for (step=0; step < numsteps; step++) {
		t = (step+1)/numsteps;
		pos[0] = start[0] + d[0]*t/pm->width;
		pos[1] = start[1] + d[1]*t/pm->height;

		if (brush->torus)
			imapaint_pixmap_blend_torus(pm, brush->pixmap, pos, brush->blend);
		else
			imapaint_pixmap_blend(pm, brush->pixmap, pos, brush->blend);
	}
}

static void imapaint_soften_sharpen(ImagePaintPixmap *pm, ImagePaintBrush *brush, float *pos, short sharpen)
{
	ImagePaintPixmap *bpm = brush->pixmap;
	unsigned int x, y, count, xi, yi, xo, yo;
	unsigned int out_off[2], in_off[2], dim[2];
	float outrgb[3], finrgb[3];
	short torus= brush->torus;
	char *inrgb, *out;

	if (torus) {
		dim[0] = bpm->width;
		dim[1] = bpm->width;
		in_off[0] = (int)(pos[0]*pm->width - bpm->width/2);
		in_off[1] = (int)(pos[1]*pm->height - bpm->width/2);
		out_off[0] = out_off[1] = 0;
	}
	else {
		imapaint_pixmap_clip(pm, bpm, pos, in_off, out_off, dim);
		if ((dim[0] == 0) || (dim[1] == 0))
			return;
	}

	for (y=0; y < dim[1]; y++) {
		for (x=0; x < dim[0]; x++) {
			/* get input pixel */
			xi = in_off[0] + x;
			yi = in_off[1] + y;
			if (torus)
				inrgb = imapaint_pixmap_get_rgba_torus(pm, xi, yi);
			else
				inrgb = imapaint_pixmap_get_rgba(pm, xi, yi);

			/* sum and average surrounding pixels */
			count = 1;
			IMAPAINT_CHAR_RGB_TO_FLOAT(outrgb, inrgb);
			if (sharpen)
				IMAPAINT_FLOAT_RGB_COPY(finrgb, outrgb);

			count += imapaint_pixmap_add_if(pm, xi-1, yi-1, outrgb, torus);
			count += imapaint_pixmap_add_if(pm, xi-1, yi  , outrgb, torus);
			count += imapaint_pixmap_add_if(pm, xi-1, yi+1, outrgb, torus);

			count += imapaint_pixmap_add_if(pm, xi  , yi-1, outrgb, torus);
			count += imapaint_pixmap_add_if(pm, xi  , yi+1, outrgb, torus);

			count += imapaint_pixmap_add_if(pm, xi+1, yi-1, outrgb, torus);
			count += imapaint_pixmap_add_if(pm, xi+1, yi  , outrgb, torus);
			count += imapaint_pixmap_add_if(pm, xi+1, yi+1, outrgb, torus);

			outrgb[0] /= count;
			outrgb[1] /= count;
			outrgb[2] /= count;

			if (sharpen) {
				/* unsharp masking - creates ugly artifacts and is disabled
				   for now, needs some sort of clamping to reduce artifacts */
				outrgb[0] = 2*finrgb[0] - outrgb[0];
				outrgb[1] = 2*finrgb[1] - outrgb[1];
				outrgb[2] = 2*finrgb[2] - outrgb[2];

				outrgb[0] = IMAPAINT_FLOAT_CLAMP(outrgb[0]);
				outrgb[1] = IMAPAINT_FLOAT_CLAMP(outrgb[1]);
				outrgb[2] = IMAPAINT_FLOAT_CLAMP(outrgb[2]);
			}

			/* write into brush buffer */
			xo = out_off[0] + x;
			yo = out_off[1] + y;
			out = imapaint_pixmap_get_rgba(bpm, xo, yo);
			IMAPAINT_FLOAT_RGB_TO_CHAR(out, outrgb);
		}
	}

	if (torus)
		imapaint_pixmap_blend_torus(pm, bpm, pos, brush->blend);
	else
		imapaint_pixmap_blend(pm, bpm, pos, brush->blend);
}

static void imapaint_lift_smear(ImagePaintPixmap *pm, ImagePaintBrush *brush, float *pos)
{
	ImagePaintPixmap *bpm = brush->pixmap;
	int in_off[2], x, y;
	char *out, *in;

	in_off[0] = (int)(pos[0]*pm->width - bpm->width/2);
	in_off[1] = (int)(pos[1]*pm->height - bpm->height/2);

	for (y=0; y < bpm->height; y++) {
		out = imapaint_pixmap_get_rgba(bpm, 0, y);
		for (x=0; x < bpm->width; x++, out+=4) {
			in = imapaint_pixmap_get_rgba_torus(pm, in_off[0]+x, in_off[1]+y);
			IMAPAINT_RGB_COPY(out, in);
		}
	}
}

static void imapaint_smear(ImagePaintPixmap *pm, ImagePaintBrush *brush, float *start, float *end)
{
	float pos[2];

	pos[0]= 2*start[0] - end[0];
	pos[1]= 2*start[1] - end[1];

	imapaint_lift_smear(pm, brush, pos);
	imapaint_blend_line(pm, brush, start, end);
}

static void imapaint_lift_clone(ImagePaintPixmap *pm, ImagePaintBrush *brush, float *pos)
{
	ImagePaintPixmap *bpm = brush->pixmap;
	int in_off[2], x, y, xi, yi;
	char *out, *in;

	/* we overwrite alphas for pixels outside clone, so need to reload them */
	imapaint_brush_pixmap_refresh(brush);

	in_off[0] = (int)(pos[0]*pm->width - bpm->width/2);
	in_off[1] = (int)(pos[1]*pm->height - bpm->height/2);

	for (y=0; y < bpm->height; y++) {
		out = imapaint_pixmap_get_rgba(bpm, 0, y);
		for (x=0; x < bpm->width; x++, out+=4) {
			xi = in_off[0] + x;
			yi = in_off[1] + y;

			if ((xi < 0) || (yi < 0) || (xi >= pm->width) || (yi >= pm->height)) {
				out[0] = out[1] = out[2] = out[3] = 0;
			}
			else {
				in = imapaint_pixmap_get_rgba(pm, xi, yi);
				IMAPAINT_RGB_COPY(out, in);
			}
		}
	}
}

static void imapaint_clone(ImagePaintPixmap *pm, ImagePaintPixmap *cpm, ImagePaintBrush *brush, float *start, float *off)
{
	float pos[2];

	pos[0]= start[0] - off[0];
	pos[1]= start[1] - off[1];

	imapaint_lift_clone(cpm, brush, pos);
	imapaint_pixmap_blend(pm, brush->pixmap, start, brush->blend);
}

/* 2D image paint */

#if 0
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
#endif

static ImagePaintBrush *imapaint_init_brush()
{
	ToolSettings *settings= G.scene->toolsettings;
	Brush *brush= settings->imapaint.brush;

	if (!brush)
		return NULL;

	/* initialize paint settings */
	if(brush->flag & BRUSH_AIRBRUSH)
		settings->imapaint.flag |= IMAGEPAINT_TIMED;
	else
		settings->imapaint.flag &= ~IMAGEPAINT_TIMED;

	/* create brush */
	return imapaint_brush_new(brush->size, brush->size, brush->rgb, brush->alpha, brush->innerradius);
}

static void imapaint_free_brush(ImagePaintBrush *brush)
{
	imapaint_brush_free(brush);
}

static ImagePaintPixmap *imapaint_init_canvas(ImagePaintPixmap **clonecanvas)
{
	ImBuf *ibuf= NULL, *cloneibuf= NULL;
	ImagePaintPixmap *canvas;
	ToolSettings *settings= G.scene->toolsettings;
	Brush *brush= settings->imapaint.brush;

	/* verify that we can paint and create canvas */
	if(!G.sima->image || !G.sima->image->ibuf || !G.sima->image->ibuf->rect)
		return NULL;
	else if(G.sima->image->packedfile)
		return NULL;

	ibuf= G.sima->image->ibuf;
	canvas= imapaint_pixmap_new(ibuf->x, ibuf->y, (char*)ibuf->rect);

	if (clonecanvas) {
		/* create clone canvas */
		if(brush && (settings->imapaint.tool == PAINT_TOOL_CLONE)) {
			int w, h;
			if(!brush->clone.image || !brush->clone.image->ibuf)
				return 0;

			cloneibuf= brush->clone.image->ibuf;
			w = cloneibuf->x;
			h = cloneibuf->y;
			*clonecanvas= imapaint_pixmap_new(w, h, (char*)cloneibuf->rect);
		}
		else
			*clonecanvas= NULL;
	}

	return canvas;
}

static void imapaint_free_canvas(ImagePaintPixmap *canvas, ImagePaintPixmap *clonecanvas)
{
	imapaint_pixmap_free(canvas);
	if(clonecanvas)
		imapaint_pixmap_free(clonecanvas);
}

void imapaint_redraw_tool(void)
{
	if(G.scene->toolsettings->imapaint.flag & IMAGEPAINT_DRAW_TOOL_DRAWING)
		force_draw(0);
}

static void imapaint_redraw(int final, int painted)
{
	if(!final && !painted) {
		imapaint_redraw_tool();
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

static void imapaint_compute_uvco(short *mval, float *uv)
{
	areamouseco_to_ipoco(G.v2d, mval, &uv[0], &uv[1]);
}

static void imapaint_paint_tool(ImagePaintBrush *brush, ImagePaintPixmap *canvas, ImagePaintPixmap *clonecanvas, float *prevuv, float *uv)
{
	ToolSettings *settings= G.scene->toolsettings;
	Brush *curbrush= settings->imapaint.brush;

	brush->torus= (settings->imapaint.flag & IMAGEPAINT_TORUS)? 1: 0;
	brush->blend= curbrush->blend;

	if(settings->imapaint.tool == PAINT_TOOL_SOFTEN)
		imapaint_soften_sharpen(canvas, brush, prevuv, 0);
	else if(settings->imapaint.tool == PAINT_TOOL_SMEAR)
		imapaint_smear(canvas, brush, prevuv, uv);
	else if(settings->imapaint.tool == PAINT_TOOL_CLONE)
		imapaint_clone(canvas, clonecanvas, brush, prevuv, curbrush->clone.offset);
	else if(curbrush->flag & BRUSH_AIRBRUSH)
		imapaint_blend_line(canvas, brush, uv, uv);
	else
		imapaint_blend_line(canvas, brush, prevuv, uv);
}

void imagepaint_paint(short mousebutton)
{
	ImagePaintBrush *brush;
	ImagePaintPixmap *canvas, *clonecanvas=NULL;
	short prevmval[2], mval[2];
	double prevtime, curtime;
	float prevuv[2], uv[2];
	int paint= 0, moved= 0, firsttouch=1 ;
	ToolSettings *settings= G.scene->toolsettings;
	Brush *curbrush= settings->imapaint.brush;

	if (!(canvas = imapaint_init_canvas(&clonecanvas))) {
		if(G.sima->image && G.sima->image->packedfile)
			error("Painting in packed images not supported");
		return;
	}
	else if (!(brush = imapaint_init_brush())) {
		imapaint_free_canvas(canvas, clonecanvas);
		return;
	}
	
	getmouseco_areawin(prevmval);
	prevtime = PIL_check_seconds_timer();

	settings->imapaint.flag |= IMAGEPAINT_DRAWING;

	while(get_mbut() & mousebutton) {
		getmouseco_areawin(mval);

		if(firsttouch)
			moved= paint= 1;
		else
			moved= paint= (prevmval[0] != mval[0]) || (prevmval[1] != mval[1]);

		if(settings->imapaint.flag & IMAGEPAINT_TIMED) {
			/* see if need to draw because of timer */
			curtime = PIL_check_seconds_timer();

			if(((curtime - prevtime) > (5.0/curbrush->timing)) || firsttouch) {
				prevtime= curtime;
				paint= 1;
			}
			else paint= 0;
		}
		else if(paint && !firsttouch) {
			/* check if we moved enough to draw */
			float dmval[2], d, dlimit;

			dmval[0]= prevmval[0] - mval[0];
			dmval[1]= prevmval[1] - mval[1];

			d= sqrt(dmval[0]*dmval[0] + dmval[1]*dmval[1]);
			dlimit= curbrush->size*G.sima->zoom*curbrush->timing/200.0;

			if (d < dlimit)
				paint= 0;
		}

		if(paint) {
			/* do the actual painting */
			imapaint_compute_uvco(prevmval, prevuv);
			imapaint_compute_uvco(mval, uv);

			imapaint_paint_tool(brush, canvas, clonecanvas, prevuv, uv);

			prevmval[0]= mval[0];
			prevmval[1]= mval[1];
		}
		firsttouch = 0;

		if(paint)
			imapaint_redraw(0, paint);
		else if(moved && (settings->imapaint.flag & IMAGEPAINT_DRAW_TOOL))
			imapaint_redraw(0, paint);
	}

	settings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

	imapaint_free_brush(brush);
	imapaint_free_canvas(canvas, clonecanvas);
	G.sima->image->ibuf->userflags |= IB_BITMAPDIRTY;

	imapaint_redraw(1, 0);
}

void imagepaint_pick(short mousebutton)
{
	ToolSettings *settings= G.scene->toolsettings;
	Brush *brush= settings->imapaint.brush;

	if(brush && (settings->imapaint.tool == PAINT_TOOL_CLONE)) {
		if(brush->clone.image && brush->clone.image->ibuf) {
			short prevmval[2], mval[2];
			float prevuv[2], uv[2];
		
			getmouseco_areawin(prevmval);

			while(get_mbut() & mousebutton) {
				getmouseco_areawin(mval);

				if((prevmval[0] != mval[0]) || (prevmval[1] != mval[1]) ) {
					/* mouse moved, so move the clone image */
					imapaint_compute_uvco(prevmval, prevuv);
					imapaint_compute_uvco(mval, uv);

					brush->clone.offset[0] += uv[0] - prevuv[0];
					brush->clone.offset[1] += uv[1] - prevuv[1];

					force_draw(0);

					prevmval[0]= mval[0];
					prevmval[1]= mval[1];
				}
			}
		}
	}
	else if(brush) {
		extern VPaint Gvp;

		sample_vpaint();
		brush->rgb[0]= Gvp.r;
		brush->rgb[1]= Gvp.g;
		brush->rgb[2]= Gvp.b;
	}
}

/* these will be moved */
int facesel_face_pick(Mesh *me, short *mval, unsigned int *index, short rect);
int face_pick_uv(Object* object, Mesh* mesh, TFace* face, short *xy, float *uv);

void texturepaint_paint()
{
	Object *ob;
	Mesh *me;
	TFace *face, *face_old = 0;
	short xy[2], xy_old[2];
	//int a, index;
	Image *img=NULL, *img_old = NULL;
	ImagePaintBrush *brush;
	ImagePaintPixmap *canvas = 0;
	unsigned int face_index, mousebutton;
	char *warn_packed_file = 0;
	float uv[2], uv_old[2];
	extern VPaint Gvp;
	ImBuf *ibuf= NULL;

	ob = OBACT;
	if (!ob) {
		error("No active object"); return;
	}
	if (!(ob->lay & G.vd->lay)) {
		error("The active object is not in this layer"); return;
	}
	me = get_mesh(ob);
	if (!me) {
		error("The active object does not have a mesh obData"); return;
	}

	brush = imapaint_brush_new(Gvp.size, Gvp.size, &Gvp.r, Gvp.a, 0.5);
	if (!brush) {
		error("Can't create brush"); return;
	}

	persp(PERSP_VIEW);

	if (U.flag & USER_LMOUSESELECT) mousebutton = R_MOUSE;
	else mousebutton = L_MOUSE;

	getmouseco_areawin(xy_old);
	while (get_mbut() & mousebutton) {
		getmouseco_areawin(xy);
		/* Check if cursor has moved */
		if ((xy[0] != xy_old[0]) || (xy[1] != xy_old[1])) {

			/* Get face to draw on */
			if (!facesel_face_pick(me, xy, &face_index, 0)) face = NULL;
			else face = (((TFace*)me->tface)+face_index);

			/* Check if this is another face. */
			if (face != face_old) {
				/* The active face changed, check the texture */
				if (face) {
					img = face->tpage;
					ibuf = (img)? img->ibuf: NULL;
				}
				else {
					img = 0;
				}

				if (img != img_old) {
					/* Faces have different textures. Finish drawing in the old face. */
					if (face_old && canvas) {
						face_pick_uv(ob, me, face_old, xy, uv);
						imapaint_blend_line(canvas, brush, uv_old, uv);
						img_old->ibuf->userflags |= IB_BITMAPDIRTY;
						/* Delete old canvas */
						imapaint_pixmap_free(canvas);
						canvas = 0;
					}

					/* Create new canvas and start drawing in the new face. */
					if (img) {
						if (ibuf && img->packedfile == 0) {
							/* MAART: skipx is not set most of the times. Make a guess. */
							canvas = imapaint_pixmap_new(ibuf->x, ibuf->y, (char*)ibuf->rect);
							if (canvas) {
								face_pick_uv(ob, me, face, xy_old, uv_old);
								face_pick_uv(ob, me, face, xy, uv);
								imapaint_blend_line(canvas, brush, uv_old, uv);
								ibuf->userflags |= IB_BITMAPDIRTY;
							}
						}
						else {
							if (img->packedfile) {
								warn_packed_file = img->id.name + 2;
								img = 0;
							}
						}
					}
				}
				else {
					/* Face changed and faces have the same texture. */
					if (canvas) {
						/* Finish drawing in the old face. */
						if (face_old) {
							face_pick_uv(ob, me, face_old, xy, uv);
							imapaint_blend_line(canvas, brush, uv_old, uv);
							img_old->ibuf->userflags |= IB_BITMAPDIRTY;
						}

						/* Start drawing in the new face. */
						if (face) {
							face_pick_uv(ob, me, face, xy_old, uv_old);
							face_pick_uv(ob, me, face, xy, uv);
							imapaint_blend_line(canvas, brush, uv_old, uv);
							ibuf->userflags |= IB_BITMAPDIRTY;
						}
					}
				}
			}
			else {
				/* Same face, continue drawing */
				if (face && canvas) {
					/* Get the new (u,v) coordinates */
					face_pick_uv(ob, me, face, xy, uv);
					imapaint_blend_line(canvas, brush, uv_old, uv);
					ibuf->userflags |= IB_BITMAPDIRTY;
				}
			}

			if (face && img) {
				/* Make OpenGL aware of a change in the texture */
				free_realtime_image(img);
				/* Redraw the view */
				scrarea_do_windraw(curarea);
				screen_swapbuffers();
			}

			xy_old[0] = xy[0];
			xy_old[1] = xy[1];
			uv_old[0] = uv[0];
			uv_old[1] = uv[1];
			face_old = face;
			img_old = img;
		}
	}

	imapaint_brush_free(brush);
	if (canvas) {
		imapaint_pixmap_free(canvas);
		canvas = 0;
	}

	if (warn_packed_file) {
		error("Painting in packed images is not supported: %s", warn_packed_file);
	}

	persp(PERSP_WIN);

	BIF_undo_push("UV face draw");
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWHEADERS, 0);
}

