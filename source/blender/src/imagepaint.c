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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "BLI_arithb.h"
#include "PIL_time.h"

#include "IMB_imbuf.h"
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

/* ImagePaint Utilities */

#define IMAPAINT_FLOAT_TO_CHAR(f) ((char)(f*255))
#define IMAPAINT_CHAR_TO_FLOAT(c) (c/255.0f)

#define IMAPAINT_FLOAT_RGB_TO_CHAR(c, f) { c[0]=IMAPAINT_FLOAT_TO_CHAR(f[0]); \
	c[1]=IMAPAINT_FLOAT_TO_CHAR(f[1]); c[2]=IMAPAINT_FLOAT_TO_CHAR(f[2]); }
#define IMAPAINT_CHAR_RGB_TO_FLOAT(f, c) { f[0]=IMAPAINT_CHAR_TO_FLOAT(c[0]); \
	f[1]=IMAPAINT_CHAR_TO_FLOAT(c[1]); f[2]=IMAPAINT_CHAR_TO_FLOAT(c[2]); }
#define IMAPAINT_FLOAT_RGB_COPY(a, b) VECCOPY(a, b)

static void imapaint_blend_line(ImBuf *ibuf, ImBuf *ibufb, float *start, float *end)
{
	float numsteps, t, pos[2];
	int step, d[2], ipos[2];

	d[0] = (int)(end[0] - start[0]);
	d[1] = (int)(end[1] - start[1]);
	numsteps = sqrt(d[0]*d[0] + d[1]*d[1])/(ibufb->x/4.0f);

	if(numsteps < 1.0)
		numsteps = 1.0f;

	for (step=0; step < numsteps; step++) {
		t = (step+1)/numsteps;
		pos[0] = start[0] + d[0]*t;
		pos[1] = start[1] + d[1]*t;

		ipos[0]= (int)(pos[0] - ibufb->x/2);
		ipos[1]= (int)(pos[1] - ibufb->y/2);
		IMB_rectblend(ibuf, ibufb, ipos[0], ipos[1], 0, 0,
			ibufb->x, ibufb->y, IMB_BLEND_MIX);
	}
}

static void imapaint_ibuf_get_set_rgb(ImBuf *ibuf, int x, int y, short torus, short set, float *rgb)
{
	if (torus) {
		x %= ibuf->x;
		if (x < 0) x += ibuf->x;
		y %= ibuf->y;
		if (y < 0) y += ibuf->y;
	}

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + (ibuf->x*y + x)*4;

		if (set) IMAPAINT_FLOAT_RGB_COPY(rrgbf, rgb)
		else IMAPAINT_FLOAT_RGB_COPY(rgb, rrgbf)
	}
	else {
		char *rrgb = (char*)ibuf->rect + (ibuf->x*y + x)*4;

		if (set) IMAPAINT_FLOAT_RGB_TO_CHAR(rrgb, rgb)
		else IMAPAINT_CHAR_RGB_TO_FLOAT(rgb, rrgb)
	}
}

static int imapaint_ibuf_add_if(ImBuf *ibuf, unsigned int x, unsigned int y, float *outrgb, short torus)
{
	float inrgb[3];

	if ((x >= ibuf->x) || (y >= ibuf->y)) {
		if (torus) imapaint_ibuf_get_set_rgb(ibuf, x, y, 1, 0, inrgb);
		else return 0;
	}
	else imapaint_ibuf_get_set_rgb(ibuf, x, y, 0, 0, inrgb);

	outrgb[0] += inrgb[0];
	outrgb[1] += inrgb[1];
	outrgb[2] += inrgb[2];

	return 1;
}

/* ImagePaint Tools */

static void imapaint_lift_soften(ImBuf *ibuf, ImBuf *ibufb, int *pos, short torus)
{
	int x, y, count, xi, yi, xo, yo;
	int out_off[2], in_off[2], dim[2];
	float outrgb[3];

	dim[0] = ibufb->x;
	dim[1] = ibufb->y;
	in_off[0] = pos[0];
	in_off[1] = pos[1];
	out_off[0] = out_off[1] = 0;

	if (!torus) {
		IMB_rectclip(ibuf, ibufb, &in_off[0], &in_off[1], &out_off[0],
			&out_off[1], &dim[0], &dim[1]);

		if ((dim[0] == 0) || (dim[1] == 0))
			return;
	}

	for (y=0; y < dim[1]; y++) {
		for (x=0; x < dim[0]; x++) {
			/* get input pixel */
			xi = in_off[0] + x;
			yi = in_off[1] + y;

			count = 1;
			imapaint_ibuf_get_set_rgb(ibuf, xi, yi, torus, 0, outrgb);

			count += imapaint_ibuf_add_if(ibuf, xi-1, yi-1, outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi-1, yi  , outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi-1, yi+1, outrgb, torus);

			count += imapaint_ibuf_add_if(ibuf, xi  , yi-1, outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi  , yi+1, outrgb, torus);

			count += imapaint_ibuf_add_if(ibuf, xi+1, yi-1, outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi+1, yi  , outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi+1, yi+1, outrgb, torus);

			outrgb[0] /= count;
			outrgb[1] /= count;
			outrgb[2] /= count;

			/* write into brush buffer */
			xo = out_off[0] + x;
			yo = out_off[1] + y;
			imapaint_ibuf_get_set_rgb(ibufb, xo, yo, 0, 1, outrgb);
		}
	}
}

static void imapaint_lift_smear(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	IMB_rectblend_torus(ibufb, ibuf, 0, 0, pos[0], pos[1],
		ibufb->x, ibufb->y, IMB_BLEND_COPY_RGB);
}

static ImBuf *imapaint_lift_clone(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	/* note: allocImbuf returns zero'd memory, so regions outside image will
	   have zero alpha, and hence not be blended onto the image */
	int w=ibufb->x, h=ibufb->y, destx=0, desty=0, srcx=pos[0], srcy=pos[1];
	ImBuf *clonebuf= IMB_allocImBuf(w, h, ibufb->depth, ibufb->flags, 0);

	IMB_rectclip(clonebuf, ibuf, &destx, &desty, &srcx, &srcy, &w, &h);
	IMB_rectblend(clonebuf, ibuf, destx, desty, srcx, srcy, w, h,
		IMB_BLEND_COPY_RGB);
	IMB_rectblend(clonebuf, ibufb, destx, desty, destx, desty, w, h,
		IMB_BLEND_COPY_ALPHA);

	return clonebuf;
}

/* ImagePaint state and operations */

typedef struct ImagePaintState {
	Brush *brush;
	short tool;
	ImBuf *canvas;
	ImBuf *clonecanvas;
} ImagePaintState;

static void imapaint_convert_brushco(ImBuf *ibufb, float *pos, int *ipos)
{
	ipos[0]= (int)(pos[0] - ibufb->x/2);
	ipos[1]= (int)(pos[1] - ibufb->y/2);
}

static int imapaint_paint_op(void *state, ImBuf *ibufb, float *lastpos, float *pos)
{
	ImagePaintState s= *((ImagePaintState*)state);
	ImBuf *clonebuf= NULL;
	short torus= s.brush->flag & BRUSH_TORUS;
	short blend= s.brush->blend;
	float *offset= s.brush->clone.offset;
	float liftpos[2];
	int bpos[2], blastpos[2], bliftpos[2];

	if ((s.tool == PAINT_TOOL_SMEAR) && (lastpos[0]==pos[0]) && (lastpos[1]==pos[1]))
		return 0;

	imapaint_convert_brushco(ibufb, pos, bpos);

	/* lift from canvas */
	if(s.tool == PAINT_TOOL_SOFTEN) {
		imapaint_lift_soften(s.canvas, ibufb, bpos, torus);
	}
	else if(s.tool == PAINT_TOOL_SMEAR) {
		imapaint_convert_brushco(ibufb, lastpos, blastpos);
		imapaint_lift_smear(s.canvas, ibufb, blastpos);
	}
	else if(s.tool == PAINT_TOOL_CLONE && s.clonecanvas) {
		liftpos[0]= pos[0] - offset[0]*s.canvas->x;
		liftpos[1]= pos[1] - offset[1]*s.canvas->y;

		imapaint_convert_brushco(ibufb, liftpos, bliftpos);
		clonebuf= imapaint_lift_clone(s.clonecanvas, ibufb, bliftpos);
	}

	/* blend into canvas */
	if(torus)
		IMB_rectblend_torus(s.canvas, (clonebuf)? clonebuf: ibufb,
			bpos[0], bpos[1], 0, 0, ibufb->x, ibufb->y, blend);
	else
		IMB_rectblend(s.canvas, (clonebuf)? clonebuf: ibufb,
			bpos[0], bpos[1], 0, 0, ibufb->x, ibufb->y, blend);
	
	if(clonebuf) IMB_freeImBuf(clonebuf);

	return 1;
}

/* 2D ImagePaint */

static void imapaint_compute_uvco(short *mval, float *uv)
{
	areamouseco_to_ipoco(G.v2d, mval, &uv[0], &uv[1]);
}

static void imapaint_compute_imageco(ImBuf *ibuf, short *mval, float *mousepos)
{
	areamouseco_to_ipoco(G.v2d, mval, &mousepos[0], &mousepos[1]);
	mousepos[0] *= ibuf->x;
	mousepos[1] *= ibuf->y;
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

static int imapaint_canvas_init(Brush *brush, short tool, ImBuf **canvas, ImBuf **clonecanvas, short *freefloat)
{
	Image *ima= G.sima->image;

	/* verify that we can paint and create canvas */
	if(!ima || !ima->ibuf || !(ima->ibuf->rect || ima->ibuf->rect_float))
		return 0;
	else if(ima->packedfile)
		return 0;

	*canvas= ima->ibuf;

	/* create clone canvas */
	if(clonecanvas && (tool == PAINT_TOOL_CLONE)) {
		ima= brush->clone.image;
		if(!ima || !ima->ibuf || !(ima->ibuf->rect || ima->ibuf->rect_float))
			return 0;

		*clonecanvas= ima->ibuf;

		if((*canvas)->rect_float && !(*clonecanvas)->rect_float) {
			/* temporarily add float rect for cloning */
			*freefloat= 1;
			IMB_float_from_rect(*clonecanvas);
		}
		else if(!(*canvas)->rect_float && !(*clonecanvas)->rect) {
			*freefloat= 0;
			IMB_rect_from_float(*clonecanvas);
		}
		else
			*freefloat= 0;
	}
	else if(clonecanvas)
		*clonecanvas= NULL;

	return 1;
}

void imagepaint_paint(short mousebutton)
{
	ImagePaintState s;
	BrushPainter *painter;
	ToolSettings *settings= G.scene->toolsettings;
	short prevmval[2], mval[2], freefloat=0;
	float mousepos[2];
	double mousetime;

	/* initialize state */
	s.brush= settings->imapaint.brush;
	s.tool= settings->imapaint.tool;

	if(!s.brush) return;
	if(!imapaint_canvas_init(s.brush, s.tool, &s.canvas, &s.clonecanvas, &freefloat)) {
		if(G.sima->image && G.sima->image->packedfile)
			error("Painting in packed images not supported");
		return;
	}

	settings->imapaint.flag |= IMAGEPAINT_DRAWING;

	/* create painter and paint once */
	painter= brush_painter_new(s.brush);
	brush_painter_require_imbuf(painter, ((s.canvas->rect_float)? 1: 0), 0, 0);

	getmouseco_areawin(mval);
	mousetime= PIL_check_seconds_timer();
	prevmval[0]= mval[0];
	prevmval[1]= mval[1];
	imapaint_compute_imageco(s.canvas, mval, mousepos);

	if(brush_painter_paint(painter, imapaint_paint_op, mousepos, mousetime, &s)) {
		if (s.canvas->rect_float)
			imb_freerectImBuf(s.canvas); /* force recreate */
		imapaint_redraw(0, 1);
	}

	/* paint loop */
	while(get_mbut() & mousebutton) {
		getmouseco_areawin(mval);
		mousetime= PIL_check_seconds_timer();

		if((mval[0] != prevmval[0]) || (mval[1] != prevmval[1])) {
			prevmval[0]= mval[0];
			prevmval[1]= mval[1];
			imapaint_compute_imageco(s.canvas, mval, mousepos);
		}
		else if (!(s.brush->flag & BRUSH_AIRBRUSH))
			continue;

		if(brush_painter_paint(painter, imapaint_paint_op, mousepos, mousetime, &s)) {
			if (s.canvas->rect_float)
				imb_freerectImBuf(s.canvas); /* force recreate */
			imapaint_redraw(0, 1);
		}

		/* todo: check if we can wait here to not take up all cpu usage? */
	}

	/* clean up */
	settings->imapaint.flag &= ~IMAGEPAINT_DRAWING;
	s.canvas->userflags |= IB_BITMAPDIRTY;

	if (freefloat) imb_freerectfloatImBuf(s.clonecanvas);

	brush_painter_free(painter);

	imapaint_redraw(1, 0);
}

/* 3D TexturePaint */

/* these will be moved */
int facesel_face_pick(Mesh *me, short *mval, unsigned int *index, short rect);
void texpaint_pick_uv(Object *ob, Mesh *mesh, TFace *tf, short *xy, float *mousepos);

static void texpaint_compute_imageco(ImBuf *ibuf, Object *ob, Mesh *mesh, TFace *tf, short *xy, float *imageco)
{
	texpaint_pick_uv(ob, mesh, tf, xy, imageco);
	imageco[0] *= ibuf->x;
	imageco[1] *= ibuf->y;
}

void texturepaint_paint(short mousebutton)
{
	Object *ob;
	Mesh *me;
	TFace *face, *face_old = 0;
	short xy[2], xy_old[2];
	//int a, index;
	Image *img=NULL, *img_old = NULL;
	ImBuf *brush, *canvas = 0;
	unsigned int face_index;
	char *warn_packed_file = 0;
	float uv[2], uv_old[2];
	extern VPaint Gvp;
	Brush tmpbrush;

	ob = OBACT;
	if (!ob || !(ob->lay & G.vd->lay)) return;
	me = get_mesh(ob);
	if (!me) return;

	/* create a fake Brush for now - will be replaced soon */
	memset(&tmpbrush, 0, sizeof(Brush));
	tmpbrush.size= Gvp.size;
	tmpbrush.alpha= Gvp.a;
	tmpbrush.innerradius= 0.5f;
	IMAPAINT_FLOAT_RGB_COPY(tmpbrush.rgb, &Gvp.r);
	brush = brush_imbuf_new(&tmpbrush, 0, 0, tmpbrush.size);

	persp(PERSP_VIEW);

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
					canvas = (img)? img->ibuf: NULL;
				}
				else {
					img = 0;
				}

				if (img != img_old) {
					/* Faces have different textures. Finish drawing in the old face. */
					if (face_old && canvas) {
						texpaint_compute_imageco(canvas, ob, me, face_old, xy, uv);
						imapaint_blend_line(canvas, brush, uv_old, uv);
						img_old->ibuf->userflags |= IB_BITMAPDIRTY;
						canvas = 0;
					}

					/* Create new canvas and start drawing in the new face. */
					if (img) {
						if (canvas && img->packedfile == 0) {
							/* MAART: skipx is not set most of the times. Make a guess. */
							if (canvas) {
								texpaint_compute_imageco(canvas, ob, me, face, xy_old, uv_old);
								texpaint_compute_imageco(canvas, ob, me, face, xy, uv);
								imapaint_blend_line(canvas, brush, uv_old, uv);
								canvas->userflags |= IB_BITMAPDIRTY;
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
							texpaint_compute_imageco(canvas, ob, me, face_old, xy, uv);
							imapaint_blend_line(canvas, brush, uv_old, uv);
							img_old->ibuf->userflags |= IB_BITMAPDIRTY;
						}

						/* Start drawing in the new face. */
						if (face) {
							texpaint_compute_imageco(canvas, ob, me, face, xy_old, uv_old);
							texpaint_compute_imageco(canvas, ob, me, face, xy, uv);
							imapaint_blend_line(canvas, brush, uv_old, uv);
							canvas->userflags |= IB_BITMAPDIRTY;
						}
					}
				}
			}
			else {
				/* Same face, continue drawing */
				if (face && canvas) {
					/* Get the new (u,v) coordinates */
					texpaint_compute_imageco(canvas, ob, me, face, xy, uv);
					imapaint_blend_line(canvas, brush, uv_old, uv);
					canvas->userflags |= IB_BITMAPDIRTY;
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

	IMB_freeImBuf(brush);

	if (warn_packed_file)
		error("Painting in packed images is not supported: %s", warn_packed_file);

	persp(PERSP_WIN);

	BIF_undo_push("UV face draw");
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWHEADERS, 0);
}

void imagepaint_pick(short mousebutton)
{
	ToolSettings *settings= G.scene->toolsettings;
	Brush *brush= settings->imapaint.brush;

	if(brush && (settings->imapaint.tool == PAINT_TOOL_CLONE)) {
		if(brush->clone.image && brush->clone.image->ibuf) {
			short prevmval[2], mval[2];
			float lastmousepos[2], mousepos[2];
		
			getmouseco_areawin(prevmval);

			while(get_mbut() & mousebutton) {
				getmouseco_areawin(mval);

				if((prevmval[0] != mval[0]) || (prevmval[1] != mval[1]) ) {
					/* mouse moved, so move the clone image */
					imapaint_compute_uvco(prevmval, lastmousepos);
					imapaint_compute_uvco(mval, mousepos);

					brush->clone.offset[0] += mousepos[0] - lastmousepos[0];
					brush->clone.offset[1] += mousepos[1] - lastmousepos[1];

					force_draw(0);

					prevmval[0]= mval[0];
					prevmval[1]= mval[1];
				}
			}
		}
	}
	else if(brush)
		sample_vpaint();
}

