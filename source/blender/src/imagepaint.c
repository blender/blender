/**
 * $Id$
 * imagepaint.c
 *
 * Functions to paint images in 2D and 3D.
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jens Ole Wund (bjornmose)
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"
#include "BSE_node.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"

#include "BDR_imagepaint.h"
#include "BDR_vpaint.h"

#include "GPU_draw.h"

#include "GHOST_Types.h"

#include "blendef.h"
#include "butspace.h"
#include "mydevice.h"

/* Defines and Structs */

#define IMAPAINT_FLOAT_TO_CHAR(f) ((char)(f*255))
#define IMAPAINT_CHAR_TO_FLOAT(c) (c/255.0f)

#define IMAPAINT_FLOAT_RGB_TO_CHAR(c, f) { c[0]=IMAPAINT_FLOAT_TO_CHAR(f[0]); \
	c[1]=IMAPAINT_FLOAT_TO_CHAR(f[1]); c[2]=IMAPAINT_FLOAT_TO_CHAR(f[2]); }
#define IMAPAINT_CHAR_RGB_TO_FLOAT(f, c) { f[0]=IMAPAINT_CHAR_TO_FLOAT(c[0]); \
	f[1]=IMAPAINT_CHAR_TO_FLOAT(c[1]); f[2]=IMAPAINT_CHAR_TO_FLOAT(c[2]); }
#define IMAPAINT_FLOAT_RGB_COPY(a, b) VECCOPY(a, b)

#define IMAPAINT_TILE_BITS			6
#define IMAPAINT_TILE_SIZE			(1 << IMAPAINT_TILE_BITS)
#define IMAPAINT_TILE_NUMBER(size)	(((size)+IMAPAINT_TILE_SIZE-1) >> IMAPAINT_TILE_BITS)

#define MAXUNDONAME	64

typedef struct ImagePaintState {
	Brush *brush;
	short tool, blend;
	Image *image;
	ImBuf *canvas;
	ImBuf *clonecanvas;
	short clonefreefloat;
	char *warnpackedfile;
	char *warnmultifile;

	/* texture paint only */
	Object *ob;
	Mesh *me;
	int faceindex;
	float uv[2];
} ImagePaintState;

typedef struct UndoTile {
	struct UndoTile *next, *prev;
	ID id;
	void *rect;
	int x, y;
} UndoTile;

typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char name[MAXUNDONAME];
	unsigned long undosize;

	ImBuf *ibuf;
	ListBase tiles;
} UndoElem;

typedef struct ImagePaintPartialRedraw {
	int x1, y1, x2, y2;
	int enabled;
} ImagePaintPartialRedraw;

static ListBase undobase = {NULL, NULL};
static UndoElem *curundo = NULL;
static ImagePaintPartialRedraw imapaintpartial = {0, 0, 0, 0, 0};

/* UNDO */

/* internal functions */

static void undo_copy_tile(UndoTile *tile, ImBuf *tmpibuf, ImBuf *ibuf, int restore)
{
	/* copy or swap contents of tile->rect and region in ibuf->rect */
	IMB_rectcpy(tmpibuf, ibuf, 0, 0, tile->x*IMAPAINT_TILE_SIZE,
		tile->y*IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);

	if(ibuf->rect_float) SWAP(void*, tmpibuf->rect_float, tile->rect)
	else SWAP(void*, tmpibuf->rect, tile->rect)
	
	if(restore)
		IMB_rectcpy(ibuf, tmpibuf, tile->x*IMAPAINT_TILE_SIZE,
			tile->y*IMAPAINT_TILE_SIZE, 0, 0, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);
}

static void undo_restore(UndoElem *undo)
{
	Image *ima = NULL;
	ImBuf *ibuf, *tmpibuf;
	UndoTile *tile;

	if(!undo)
		return;

	tmpibuf= IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32,
	                        IB_rectfloat|IB_rect, 0);
	
	for(tile=undo->tiles.first; tile; tile=tile->next) {
		/* find image based on name, pointer becomes invalid with global undo */
		if(ima && strcmp(tile->id.name, ima->id.name)==0);
		else {
			for(ima=G.main->image.first; ima; ima=ima->id.next)
				if(strcmp(tile->id.name, ima->id.name)==0)
					break;
		}

		ibuf= BKE_image_get_ibuf(ima, NULL);

		if (!ima || !ibuf || !(ibuf->rect || ibuf->rect_float))
			continue;

		undo_copy_tile(tile, tmpibuf, ibuf, 1);

		GPU_free_image(ima); /* force OpenGL reload */
		if(ibuf->rect_float)
			imb_freerectImBuf(ibuf); /* force recreate of char rect */
	}

	IMB_freeImBuf(tmpibuf);
}

static void undo_free(UndoElem *undo)
{
	UndoTile *tile;

	for(tile=undo->tiles.first; tile; tile=tile->next)
		MEM_freeN(tile->rect);
	BLI_freelistN(&undo->tiles);
}

static void undo_imagepaint_push_begin(char *name)
{
	UndoElem *uel;
	int nr;
	
	/* Undo push is split up in begin and end, the reason is that as painting
	 * happens more tiles are added to the list, and at the very end we know
	 * how much memory the undo used to remove old undo elements */

	/* remove all undos after (also when curundo==NULL) */
	while(undobase.last != curundo) {
		uel= undobase.last;
		undo_free(uel);
		BLI_freelinkN(&undobase, uel);
	}
	
	/* make new */
	curundo= uel= MEM_callocN(sizeof(UndoElem), "undo file");
	BLI_addtail(&undobase, uel);

	/* name can be a dynamic string */
	strncpy(uel->name, name, MAXUNDONAME-1);
	
	/* limit amount to the maximum amount*/
	nr= 0;
	uel= undobase.last;
	while(uel) {
		nr++;
		if(nr==U.undosteps) break;
		uel= uel->prev;
	}
	if(uel) {
		while(undobase.first!=uel) {
			UndoElem *first= undobase.first;
			undo_free(first);
			BLI_freelinkN(&undobase, first);
		}
	}
}

static void undo_imagepaint_push_end()
{
	UndoElem *uel;
	unsigned long totmem, maxmem;

	if(U.undomemory != 0) {
		/* limit to maximum memory (afterwards, we can't know in advance) */
		totmem= 0;
		maxmem= ((unsigned long)U.undomemory)*1024*1024;

		uel= undobase.last;
		while(uel) {
			totmem+= uel->undosize;
			if(totmem>maxmem) break;
			uel= uel->prev;
		}

		if(uel) {
			while(undobase.first!=uel) {
				UndoElem *first= undobase.first;
				undo_free(first);
				BLI_freelinkN(&undobase, first);
			}
		}
	}
}

/* external functions */

/* 1= an undo, -1 is a redo. */
void undo_imagepaint_step(int step)
{
	UndoElem *undo;

	if(step==1) {
		if(curundo==NULL) error("No more steps to undo");
		else {
			if(G.f & G_DEBUG) printf("undo %s\n", curundo->name);
			undo_restore(curundo);
			curundo= curundo->prev;
		}
	}
	else if(step==-1) {
		if((curundo!=NULL && curundo->next==NULL) || undobase.first==NULL) error("No more steps to redo");
		else {
			undo= (curundo && curundo->next)? curundo->next: undobase.first;
			undo_restore(undo);
			curundo= undo;
			if(G.f & G_DEBUG) printf("redo %s\n", undo->name);
		}
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void undo_imagepaint_clear(void)
{
	UndoElem *uel;
	
	uel= undobase.first;
	while(uel) {
		undo_free(uel);
		uel= uel->next;
	}

	BLI_freelistN(&undobase);
	curundo= NULL;
}

/* Imagepaint Partial Redraw & Dirty Region */

static void imapaint_clear_partial_redraw()
{
	memset(&imapaintpartial, 0, sizeof(imapaintpartial));
}

static void imapaint_dirty_region(Image *ima, ImBuf *ibuf, int x, int y, int w, int h)
{
	ImBuf *tmpibuf;
	UndoTile *tile;
	int srcx= 0, srcy= 0, origx, allocsize;

	IMB_rectclip(ibuf, NULL, &x, &y, &srcx, &srcy, &w, &h);

	if (w == 0 || h == 0)
		return;
	
	if (!imapaintpartial.enabled) {
		imapaintpartial.x1 = x;
		imapaintpartial.y1 = y;
		imapaintpartial.x2 = x+w;
		imapaintpartial.y2 = y+h;
		imapaintpartial.enabled = 1;
	}
	else {
		imapaintpartial.x1 = MIN2(imapaintpartial.x1, x);
		imapaintpartial.y1 = MIN2(imapaintpartial.y1, y);
		imapaintpartial.x2 = MAX2(imapaintpartial.x2, x+w);
		imapaintpartial.y2 = MAX2(imapaintpartial.y2, y+h);
	}

	w = ((x + w - 1) >> IMAPAINT_TILE_BITS);
	h = ((y + h - 1) >> IMAPAINT_TILE_BITS);
	origx = (x >> IMAPAINT_TILE_BITS);
	y = (y >> IMAPAINT_TILE_BITS);

	tmpibuf= IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32,
	                        IB_rectfloat|IB_rect, 0);
	
	for (; y <= h; y++) {
		for (x=origx; x <= w; x++) {
			for(tile=curundo->tiles.first; tile; tile=tile->next)
				if(tile->x == x && tile->y == y && strcmp(tile->id.name, ima->id.name)==0)
					break;

			if(!tile) {
				tile= MEM_callocN(sizeof(UndoTile), "ImaUndoTile");
				tile->id= ima->id;
				tile->x= x;
				tile->y= y;

				allocsize= IMAPAINT_TILE_SIZE*IMAPAINT_TILE_SIZE*4;
				allocsize *= (ibuf->rect_float)? sizeof(float): sizeof(char);
				tile->rect= MEM_mapallocN(allocsize, "ImaUndoRect");

				undo_copy_tile(tile, tmpibuf, ibuf, 0);
				curundo->undosize += allocsize;

				BLI_addtail(&curundo->tiles, tile);
			}
		}
	}

	ibuf->userflags |= IB_BITMAPDIRTY;

	IMB_freeImBuf(tmpibuf);
}

static void imapaint_image_update(Image *image, ImBuf *ibuf, short texpaint)
{
	if(ibuf->rect_float)
		imb_freerectImBuf(ibuf); /* force recreate of char rect */
	if(ibuf->mipmap[0])
		imb_freemipmapImBuf(ibuf);

	/* todo: should set_tpage create ->rect? */
	if(texpaint || G.sima->lock) {
		int w = imapaintpartial.x2 - imapaintpartial.x1;
		int h = imapaintpartial.y2 - imapaintpartial.y1;
		GPU_paint_update_image(image, imapaintpartial.x1, imapaintpartial.y1, w, h);
	}
}

/* note; gets called for both 2d image paint and 3d texture paint. in the
   latter case image may be NULL and G.sima may not exist */
static void imapaint_redraw(int final, int texpaint, Image *image)
{
	if(final) {
		if(texpaint)
			allqueue(REDRAWIMAGE, 0);
		else if(!G.sima->lock) {
			if(image)
				GPU_free_image(image); /* force OpenGL reload */
			allqueue(REDRAWVIEW3D, 0);
		}
		allqueue(REDRAWHEADERS, 0);
		
		if(!texpaint && image) {
			/* after paint, tag Image or RenderResult nodes changed */
			if(G.scene->nodetree) {
				imagepaint_composite_tags(G.scene->nodetree, image, &G.sima->iuser);
			}
			/* signal composite (hurmf, need an allqueue?) */
			if(G.sima->lock) {
				ScrArea *sa;
				for(sa=G.curscreen->areabase.first; sa; sa= sa->next) {
					if(sa->spacetype==SPACE_NODE) {
						if(((SpaceNode *)sa->spacedata.first)->treetype==NTREE_COMPOSIT) {
							addqueue(sa->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
							break;
						}
					}
				}
			}
		}		
	}
	else if(!texpaint && G.sima->lock)
		force_draw_plus(SPACE_VIEW3D, 0);
	else
		force_draw(0);
}

/* Image Paint Operations */

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

static void imapaint_convert_brushco(ImBuf *ibufb, float *pos, int *ipos)
{
	ipos[0]= (int)(pos[0] - ibufb->x/2);
	ipos[1]= (int)(pos[1] - ibufb->y/2);
}

static int imapaint_paint_op(void *state, ImBuf *ibufb, float *lastpos, float *pos)
{
	ImagePaintState *s= ((ImagePaintState*)state);
	ImBuf *clonebuf= NULL;
	short torus= s->brush->flag & BRUSH_TORUS;
	short blend= s->blend;
	float *offset= s->brush->clone.offset;
	float liftpos[2];
	int bpos[2], blastpos[2], bliftpos[2];

	imapaint_convert_brushco(ibufb, pos, bpos);

	/* lift from canvas */
	if(s->tool == PAINT_TOOL_SOFTEN) {
		imapaint_lift_soften(s->canvas, ibufb, bpos, torus);
	}
	else if(s->tool == PAINT_TOOL_SMEAR) {
		if (lastpos[0]==pos[0] && lastpos[1]==pos[1])
			return 0;

		imapaint_convert_brushco(ibufb, lastpos, blastpos);
		imapaint_lift_smear(s->canvas, ibufb, blastpos);
	}
	else if(s->tool == PAINT_TOOL_CLONE && s->clonecanvas) {
		liftpos[0]= pos[0] - offset[0]*s->canvas->x;
		liftpos[1]= pos[1] - offset[1]*s->canvas->y;

		imapaint_convert_brushco(ibufb, liftpos, bliftpos);
		clonebuf= imapaint_lift_clone(s->clonecanvas, ibufb, bliftpos);
	}

	imapaint_dirty_region(s->image, s->canvas, bpos[0], bpos[1], ibufb->x, ibufb->y);

	/* blend into canvas */
	if(torus)
		IMB_rectblend_torus(s->canvas, (clonebuf)? clonebuf: ibufb,
			bpos[0], bpos[1], 0, 0, ibufb->x, ibufb->y, blend);
	else
		IMB_rectblend(s->canvas, (clonebuf)? clonebuf: ibufb,
			bpos[0], bpos[1], 0, 0, ibufb->x, ibufb->y, blend);
			
	if(clonebuf) IMB_freeImBuf(clonebuf);

	return 1;
}

/* 2D ImagePaint */

static void imapaint_compute_uvco(short *mval, float *uv)
{
	areamouseco_to_ipoco(G.v2d, mval, &uv[0], &uv[1]);
}

/* 3D TexturePaint */

int facesel_face_pick(Mesh *me, short *mval, unsigned int *index, short rect);
void texpaint_pick_uv(Object *ob, Mesh *mesh, unsigned int faceindex, short *xy, float *mousepos);

static int texpaint_break_stroke(float *prevuv, float *fwuv, float *bkuv, float *uv)
{
	float d1[2], d2[2];
	float mismatch = Vec2Lenf(fwuv, uv);
	float len1 = Vec2Lenf(prevuv, fwuv);
	float len2 = Vec2Lenf(bkuv, uv);

	Vec2Subf(d1, fwuv, prevuv);
	Vec2Subf(d2, uv, bkuv);

	return ((Inp2f(d1, d2) < 0.0f) || (mismatch > MAX2(len1, len2)*2));
}

/* ImagePaint Common */

static int imapaint_canvas_set(ImagePaintState *s, Image *ima)
{
	ImBuf *ibuf= BKE_image_get_ibuf(ima, G.sima?&G.sima->iuser:NULL);
	
	/* verify that we can paint and set canvas */
	if(ima->packedfile && ima->rr) {
		s->warnpackedfile = ima->id.name + 2;
		return 0;
	}	
	else if(ibuf && ibuf->channels!=4) {
		s->warnmultifile = ima->id.name + 2;
		return 0;
	}
	else if(!ima || !ibuf || !(ibuf->rect || ibuf->rect_float))
		return 0;

	s->image= ima;
	s->canvas= ibuf;

	/* set clone canvas */
	if(s->tool == PAINT_TOOL_CLONE) {
		ima= s->brush->clone.image;
		ibuf= BKE_image_get_ibuf(ima, G.sima?&G.sima->iuser:NULL);
		
		if(!ima || !ibuf || !(ibuf->rect || ibuf->rect_float))
			return 0;

		s->clonecanvas= ibuf;

		if(s->canvas->rect_float && !s->clonecanvas->rect_float) {
			/* temporarily add float rect for cloning */
			IMB_float_from_rect(s->clonecanvas);
			s->clonefreefloat= 1;
		}
		else if(!s->canvas->rect_float && !s->clonecanvas->rect)
			IMB_rect_from_float(s->clonecanvas);
	}

	return 1;
}

static void imapaint_canvas_free(ImagePaintState *s)
{
	if (s->clonefreefloat)
		imb_freerectfloatImBuf(s->clonecanvas);
}

static int imapaint_paint_sub_stroke(ImagePaintState *s, BrushPainter *painter, Image *image, short texpaint, float *uv, double time, int update, float pressure)
{
	ImBuf *ibuf= BKE_image_get_ibuf(image, G.sima?&G.sima->iuser:NULL);
	float pos[2];

	if(!ibuf)
		return 0;

	pos[0] = uv[0]*ibuf->x;
	pos[1] = uv[1]*ibuf->y;

	brush_painter_require_imbuf(painter, ((ibuf->rect_float)? 1: 0), 0, 0);

	if (brush_painter_paint(painter, imapaint_paint_op, pos, time, pressure, s)) {
		if (update)
			imapaint_image_update(image, ibuf, texpaint);
		return 1;
	}
	else return 0;
}

static void imapaint_paint_stroke(ImagePaintState *s, BrushPainter *painter, short texpaint, short *prevmval, short *mval, double time, float pressure)
{
	Image *newimage = NULL;
	float fwuv[2], bkuv[2], newuv[2];
	unsigned int newfaceindex;
	int breakstroke = 0, redraw = 0;

	if (texpaint) {
		/* pick new face and image */
		if (facesel_face_pick(s->me, mval, &newfaceindex, 0)) {
			ImBuf *ibuf;
			
			newimage = (Image*)((s->me->mtface+newfaceindex)->tpage);
			ibuf= BKE_image_get_ibuf(newimage, G.sima?&G.sima->iuser:NULL);

			if(ibuf && ibuf->rect)
				texpaint_pick_uv(s->ob, s->me, newfaceindex, mval, newuv);
			else {
				newimage = NULL;
				newuv[0] = newuv[1] = 0.0f;
			}
		}
		else
			newuv[0] = newuv[1] = 0.0f;

		/* see if stroke is broken, and if so finish painting in old position */
		if (s->image) {
			texpaint_pick_uv(s->ob, s->me, s->faceindex, mval, fwuv);
			texpaint_pick_uv(s->ob, s->me, newfaceindex, prevmval, bkuv);

			if (newimage == s->image)
				breakstroke= texpaint_break_stroke(s->uv, fwuv, bkuv, newuv);
			else
				breakstroke= 1;
		}
		else
			fwuv[0]= fwuv[1]= 0.0f;

		if (breakstroke) {
			texpaint_pick_uv(s->ob, s->me, s->faceindex, mval, fwuv);
			redraw |= imapaint_paint_sub_stroke(s, painter, s->image, texpaint,
				fwuv, time, 1, pressure);
			imapaint_clear_partial_redraw();
			brush_painter_break_stroke(painter);
		}

		/* set new canvas */
		if (newimage && (newimage != s->image))
			if (!imapaint_canvas_set(s, newimage))
				newimage = NULL;

		/* paint in new image */
		if (newimage) {
			if (breakstroke)
				redraw|= imapaint_paint_sub_stroke(s, painter, newimage,
					texpaint, bkuv, time, 0, pressure);
			redraw|= imapaint_paint_sub_stroke(s, painter, newimage, texpaint,
				newuv, time, 1, pressure);
		}

		/* update state */
		s->image = newimage;
		s->faceindex = newfaceindex;
		s->uv[0] = newuv[0];
		s->uv[1] = newuv[1];
	}
	else {
		imapaint_compute_uvco(mval, newuv);
		redraw |= imapaint_paint_sub_stroke(s, painter, s->image, texpaint, newuv,
			time, 1, pressure);
	}

	if (redraw) {
		imapaint_redraw(0, texpaint, NULL);
		imapaint_clear_partial_redraw();
	}
}

void imagepaint_paint(short mousebutton, short texpaint)
{
	ImagePaintState s;
	BrushPainter *painter;
	ToolSettings *settings= G.scene->toolsettings;
	short prevmval[2], mval[2];
	double time;
	float pressure;

	if(!settings->imapaint.brush)
		return;

	/* initialize state */
	memset(&s, 0, sizeof(s));
	s.brush = settings->imapaint.brush;
	s.tool = settings->imapaint.tool;
	if(texpaint && (s.tool == PAINT_TOOL_CLONE))
		s.tool = PAINT_TOOL_DRAW;
	s.blend = s.brush->blend;

	if(texpaint) {
		s.ob = OBACT;
		if (!s.ob || !(s.ob->lay & G.vd->lay)) return;
		s.me = get_mesh(s.ob);
		if (!s.me) return;

		persp(PERSP_VIEW);
	}
	else {
		s.image = G.sima->image;

		if(!imapaint_canvas_set(&s, G.sima->image)) {
			if(s.warnmultifile)
				error("Image requires 4 color channels to paint");
			if(s.warnpackedfile)
				error("Packed MultiLayer files cannot be painted");
			return;
		}
	}

	settings->imapaint.flag |= IMAGEPAINT_DRAWING;
	undo_imagepaint_push_begin("Image Paint");

	/* create painter and paint once */
	painter= brush_painter_new(s.brush);

	getmouseco_areawin(mval);

	pressure = get_pressure();
	s.blend = (get_activedevice() == 2)? BRUSH_BLEND_ERASE_ALPHA: s.brush->blend;
	
	time= PIL_check_seconds_timer();
	prevmval[0]= mval[0];
	prevmval[1]= mval[1];

	/* special exception here for too high pressure values on first touch in
	   windows for some tablets */
    if (!((s.brush->flag & (BRUSH_ALPHA_PRESSURE|BRUSH_SIZE_PRESSURE|
		BRUSH_SPACING_PRESSURE|BRUSH_RAD_PRESSURE)) && (get_activedevice() != 0) && (pressure >= 0.99f)))
		imapaint_paint_stroke(&s, painter, texpaint, prevmval, mval, time, pressure);

	/* paint loop */
	do {
		getmouseco_areawin(mval);

		pressure = get_pressure();
		s.blend = (get_activedevice() == 2)? BRUSH_BLEND_ERASE_ALPHA: s.brush->blend;
			
		time= PIL_check_seconds_timer();

		if((mval[0] != prevmval[0]) || (mval[1] != prevmval[1])) {
			imapaint_paint_stroke(&s, painter, texpaint, prevmval, mval, time, pressure);
			prevmval[0]= mval[0];
			prevmval[1]= mval[1];
		}
		else if (s.brush->flag & BRUSH_AIRBRUSH)
			imapaint_paint_stroke(&s, painter, texpaint, prevmval, mval, time, pressure);
		else
			BIF_wait_for_statechange();

		/* do mouse checking at the end, so don't check twice, and potentially
		   miss a short tap */
	} while(get_mbut() & mousebutton);

	/* clean up */
	settings->imapaint.flag &= ~IMAGEPAINT_DRAWING;
	imapaint_canvas_free(&s);
	brush_painter_free(painter);

	imapaint_redraw(1, texpaint, s.image);
	undo_imagepaint_push_end();
	
	if (texpaint) {
		if (s.warnmultifile)
			error("Image requires 4 color channels to paint: %s", s.warnmultifile);
		if(s.warnpackedfile)
			error("Packed MultiLayer files cannot be painted %s", s.warnpackedfile);

		persp(PERSP_WIN);
	}
}

void imagepaint_pick(short mousebutton)
{
	ToolSettings *settings= G.scene->toolsettings;
	Brush *brush= settings->imapaint.brush;

	if(brush && (settings->imapaint.tool == PAINT_TOOL_CLONE)) {
		if(brush->clone.image) {
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

