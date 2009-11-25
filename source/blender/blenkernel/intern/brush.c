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

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_paint.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_render_ext.h" /* externtex */
#include "RE_shader_ext.h"

/* Datablock add/copy/free/make_local */

Brush *add_brush(const char *name)
{
	Brush *brush;

	brush= alloc_libblock(&G.main->brush, ID_BR, name);

	brush->rgb[0]= 1.0f;
	brush->rgb[1]= 1.0f;
	brush->rgb[2]= 1.0f;
	brush->alpha= 0.2f;
	brush->size= 25;
	brush->spacing= 7.5f;
	brush->smooth_stroke_radius= 75;
	brush->smooth_stroke_factor= 0.9;
	brush->rate= 0.1f;
	brush->jitter= 0.0f;
	brush->clone.alpha= 0.5;
	brush->sculpt_tool = SCULPT_TOOL_DRAW;
	brush->flag |= BRUSH_SPACE;

	brush_curve_preset(brush, BRUSH_PRESET_SMOOTH);

	/* enable fake user by default */
	brush->id.flag |= LIB_FAKEUSER;
	brush_toggled_fake_user(brush);
	
	return brush;	
}

Brush *copy_brush(Brush *brush)
{
	Brush *brushn;
	MTex *mtex;
	int a;
	
	brushn= copy_libblock(brush);

	for(a=0; a<MAX_MTEX; a++) {
		mtex= brush->mtex[a];
		if(mtex) {
			brushn->mtex[a]= MEM_dupallocN(mtex);
			if(mtex->tex) id_us_plus((ID*)mtex->tex);
		}
	}

	brushn->curve= curvemapping_copy(brush->curve);

	/* enable fake user by default */
	if (!(brushn->id.flag & LIB_FAKEUSER)) {
		brushn->id.flag |= LIB_FAKEUSER;
		brush_toggled_fake_user(brushn);
	}
	
	return brushn;
}

/* not brush itself */
void free_brush(Brush *brush)
{
	MTex *mtex;
	int a;

	for(a=0; a<MAX_MTEX; a++) {
		mtex= brush->mtex[a];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
		}
	}

	curvemapping_free(brush->curve);
}

void make_local_brush(Brush *brush)
{
	/* - only lib users: do nothing
		* - only local users: set flag
		* - mixed: make copy
		*/
	
	Brush *brushn;
	Scene *scene;
	int local= 0, lib= 0;

	if(brush->id.lib==0) return;

	if(brush->clone.image) {
		/* special case: ima always local immediately */
		brush->clone.image->id.lib= 0;
		brush->clone.image->id.flag= LIB_LOCAL;
		new_id(0, (ID *)brush->clone.image, 0);
	}

	for(scene= G.main->scene.first; scene; scene=scene->id.next)
		if(paint_brush(&scene->toolsettings->imapaint.paint)==brush) {
			if(scene->id.lib) lib= 1;
			else local= 1;
		}
	
	if(local && lib==0) {
		brush->id.lib= 0;
		brush->id.flag= LIB_LOCAL;
		new_id(0, (ID *)brush, 0);

		/* enable fake user by default */
		if (!(brush->id.flag & LIB_FAKEUSER)) {
			brush->id.flag |= LIB_FAKEUSER;
			brush_toggled_fake_user(brush);
		}
	}
	else if(local && lib) {
		brushn= copy_brush(brush);
		brushn->id.us= 1; /* only keep fake user */
		brushn->id.flag |= LIB_FAKEUSER;
		
		for(scene= G.main->scene.first; scene; scene=scene->id.next)
			if(paint_brush(&scene->toolsettings->imapaint.paint)==brush)
				if(scene->id.lib==0) {
					paint_brush_set(&scene->toolsettings->imapaint.paint, brushn);
					brushn->id.us++;
					brush->id.us--;
				}
	}
}

/* Library Operations */

int brush_set_nr(Brush **current_brush, int nr, const char *name)
{
	ID *idtest, *id;
	
	id= (ID*)(*current_brush);
	idtest= (ID*)BLI_findlink(&G.main->brush, nr-1);
	
	if(idtest==0) { /* new brush */
		if(id) idtest= (ID *)copy_brush((Brush *)id);
		else idtest= (ID *)add_brush(name);
		idtest->us--;
	}
	if(idtest!=id) {
		brush_delete(current_brush);
		*current_brush= (Brush *)idtest;
		id_us_plus(idtest);

		return 1;
	}

	return 0;
}

int brush_delete(Brush **current_brush)
{
	if (*current_brush) {
		(*current_brush)->id.us--;
		*current_brush= NULL;

		return 1;
	}

	return 0;
}

void brush_toggled_fake_user(Brush *brush)
{
	ID *id= (ID*)brush;
	if(id) {
		if(id->flag & LIB_FAKEUSER) {
			id_us_plus(id);
		} else {
			id->us--;
		}
	}
}

void brush_curve_preset(Brush *b, BrushCurvePreset preset)
{
	CurveMap *cm = NULL;

	if(!b->curve)
		b->curve = curvemapping_add(1, 0, 0, 1, 1);

	cm = b->curve->cm;

	if(cm->curve)
		MEM_freeN(cm->curve);

	if(preset == BRUSH_PRESET_SHARP)
		cm->totpoint= 3;
	if(preset == BRUSH_PRESET_SMOOTH)
		cm->totpoint= 4;
	if(preset == BRUSH_PRESET_MAX)
		cm->totpoint= 2;


	cm->curve= MEM_callocN(cm->totpoint*sizeof(CurveMapPoint), "curve points");
	cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;

	if(preset == BRUSH_PRESET_SHARP) {
		cm->curve[0].x= 0;
		cm->curve[0].y= 1;
		cm->curve[1].x= 0.33;
		cm->curve[1].y= 0.33;
		cm->curve[2].x= 1;
		cm->curve[2].y= 0;
	}
	else if(preset == BRUSH_PRESET_SMOOTH) {
		cm->curve[0].x= 0;
		cm->curve[0].y= 1;
		cm->curve[1].x= 0.25;
		cm->curve[1].y= 0.92;
		cm->curve[2].x= 0.75;
		cm->curve[2].y= 0.08;
		cm->curve[3].x= 1;
		cm->curve[3].y= 0;
	}
	else if(preset == BRUSH_PRESET_MAX) {
		cm->curve[0].x= 0;
		cm->curve[0].y= 1;
		cm->curve[1].x= 1;
		cm->curve[1].y= 1;
	}

	curvemapping_changed(b->curve, 0);
}

static MTex *brush_active_texture(Brush *brush)
{
	if(brush && brush->texact >= 0)
		return brush->mtex[brush->texact];
	return NULL;
}

int brush_texture_set_nr(Brush *brush, int nr)
{
	ID *idtest, *id=NULL;

	if(brush->mtex[brush->texact])
		id= (ID *)brush->mtex[brush->texact]->tex;

	idtest= (ID*)BLI_findlink(&G.main->tex, nr-1);
	if(idtest==0) { /* new tex */
		if(id) idtest= (ID *)copy_texture((Tex *)id);
		else idtest= (ID *)add_texture("Tex");
		idtest->us--;
	}
	if(idtest!=id) {
		brush_texture_delete(brush);

		if(brush->mtex[brush->texact]==NULL) {
			brush->mtex[brush->texact]= add_mtex();
			brush->mtex[brush->texact]->r = 1.0f;
			brush->mtex[brush->texact]->g = 1.0f;
			brush->mtex[brush->texact]->b = 1.0f;
		}
		brush->mtex[brush->texact]->tex= (Tex*)idtest;
		id_us_plus(idtest);

		return 1;
	}

	return 0;
}

int brush_texture_delete(Brush *brush)
{
	if(brush->mtex[brush->texact]) {
		if(brush->mtex[brush->texact]->tex)
			brush->mtex[brush->texact]->tex->id.us--;
		MEM_freeN(brush->mtex[brush->texact]);
		brush->mtex[brush->texact]= NULL;

		return 1;
	}

	return 0;
}

int brush_clone_image_set_nr(Brush *brush, int nr)
{
	if(brush && nr > 0) {
		Image *ima= (Image*)BLI_findlink(&G.main->image, nr-1);

		if(ima) {
			brush_clone_image_delete(brush);
			brush->clone.image= ima;
			id_us_plus(&ima->id);
			brush->clone.offset[0]= brush->clone.offset[1]= 0.0f;

			return 1;
		}
	}

	return 0;
}

int brush_clone_image_delete(Brush *brush)
{
	if (brush && brush->clone.image) {
		brush->clone.image->id.us--;
		brush->clone.image= NULL;
		return 1;
	}

	return 0;
}

void brush_check_exists(Brush **brush, const char *name)
{
	if(*brush==NULL)
		brush_set_nr(brush, 1, name);
}

/* Brush Sampling */
void brush_sample_tex(Brush *brush, float *xy, float *rgba)
{
	MTex *mtex= brush->mtex[brush->texact];

	if (mtex && mtex->tex) {
		float co[3], tin, tr, tg, tb, ta;
		int hasrgb;
		
		co[0]= xy[0]/(brush->size >> 1);
		co[1]= xy[1]/(brush->size >> 1);
		co[2]= 0.0f;

		hasrgb= externtex(mtex, co, &tin, &tr, &tg, &tb, &ta);

		if (hasrgb) {
			rgba[0]= tr;
			rgba[1]= tg;
			rgba[2]= tb;
			rgba[3]= ta;
		}
		else {
			rgba[0]= tin;
			rgba[1]= tin;
			rgba[2]= tin;
			rgba[3]= 1.0f;
		}
	}
	else if (rgba)
		rgba[0]= rgba[1]= rgba[2]= rgba[3]= 1.0f;
}


void brush_imbuf_new(Brush *brush, short flt, short texfall, int size, ImBuf **outbuf)
{
	ImBuf *ibuf;
	float xy[2], dist, rgba[4], *dstf;
	int x, y, rowbytes, xoff, yoff, imbflag;
	int maxsize = brush->size >> 1;
	char *dst, crgb[3];

	imbflag= (flt)? IB_rectfloat: IB_rect;
	xoff = -size/2.0f + 0.5f;
	yoff = -size/2.0f + 0.5f;
	rowbytes= size*4;

	if (*outbuf)
		ibuf= *outbuf;
	else
		ibuf= IMB_allocImBuf(size, size, 32, imbflag, 0);

	if (flt) {
		for (y=0; y < ibuf->y; y++) {
			dstf = ibuf->rect_float + y*rowbytes;

			for (x=0; x < ibuf->x; x++, dstf+=4) {
				xy[0] = x + xoff;
				xy[1] = y + yoff;

				if (texfall == 0) {
					dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

					VECCOPY(dstf, brush->rgb);
					dstf[3]= brush->alpha*brush_curve_strength_clamp(brush, dist, maxsize);
				}
				else if (texfall == 1) {
					brush_sample_tex(brush, xy, dstf);
				}
				else {
					dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

					brush_sample_tex(brush, xy, rgba);

					dstf[0] = rgba[0]*brush->rgb[0];
					dstf[1] = rgba[1]*brush->rgb[1];
					dstf[2] = rgba[2]*brush->rgb[2];
					dstf[3] = rgba[3]*brush->alpha*brush_curve_strength_clamp(brush, dist, maxsize);
				}
			}
		}
	}
	else {
		crgb[0]= FTOCHAR(brush->rgb[0]);
		crgb[1]= FTOCHAR(brush->rgb[1]);
		crgb[2]= FTOCHAR(brush->rgb[2]);

		for (y=0; y < ibuf->y; y++) {
			dst = (char*)ibuf->rect + y*rowbytes;

			for (x=0; x < ibuf->x; x++, dst+=4) {
				xy[0] = x + xoff;
				xy[1] = y + yoff;

				if (texfall == 0) {
					dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

					dst[0]= crgb[0];
					dst[1]= crgb[1];
					dst[2]= crgb[2];
					dst[3]= FTOCHAR(brush->alpha*brush_curve_strength(brush, dist, maxsize));
				}
				else if (texfall == 1) {
					brush_sample_tex(brush, xy, rgba);
					dst[0]= FTOCHAR(rgba[0]);
					dst[1]= FTOCHAR(rgba[1]);
					dst[2]= FTOCHAR(rgba[2]);
					dst[3]= FTOCHAR(rgba[3]);
				}
				else {
					dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

					brush_sample_tex(brush, xy, rgba);
					dst[0] = FTOCHAR(rgba[0]*brush->rgb[0]);
					dst[1] = FTOCHAR(rgba[1]*brush->rgb[1]);
					dst[2] = FTOCHAR(rgba[2]*brush->rgb[2]);
					dst[3] = FTOCHAR(rgba[3]*brush->alpha*brush_curve_strength_clamp(brush, dist, maxsize));
				}
			}
		}
	}

	*outbuf= ibuf;
}

/* Brush Painting */

typedef struct BrushPainterCache {
	short enabled;

	int size;			/* size override, if 0 uses brush->size */
	short flt;			/* need float imbuf? */
	short texonly;		/* no alpha, color or fallof, only texture in imbuf */

	int lastsize;
	float lastalpha;
	float lastjitter;

	ImBuf *ibuf;
	ImBuf *texibuf;
	ImBuf *maskibuf;
} BrushPainterCache;

struct BrushPainter {
	Brush *brush;

	float lastmousepos[2];	/* mouse position of last paint call */

	float accumdistance;	/* accumulated distance of brush since last paint op */
	float lastpaintpos[2];	/* position of last paint op */
	float startpaintpos[2]; /* position of first paint */

	double accumtime;		/* accumulated time since last paint op (airbrush) */
	double lasttime;		/* time of last update */

	float lastpressure;

	short firsttouch;		/* first paint op */

	float startsize;
	float startalpha;
	float startjitter;
	float startspacing;

	BrushPainterCache cache;
};

BrushPainter *brush_painter_new(Brush *brush)
{
	BrushPainter *painter= MEM_callocN(sizeof(BrushPainter), "BrushPainter");

	painter->brush= brush;
	painter->firsttouch= 1;
	painter->cache.lastsize= -1; /* force ibuf create in refresh */

	painter->startsize = brush->size;
	painter->startalpha = brush->alpha;
	painter->startjitter = brush->jitter;
	painter->startspacing = brush->spacing;

	return painter;
}

void brush_painter_require_imbuf(BrushPainter *painter, short flt, short texonly, int size)
{
	if ((painter->cache.flt != flt) || (painter->cache.size != size) ||
		((painter->cache.texonly != texonly) && texonly)) {
		if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
		if (painter->cache.maskibuf) IMB_freeImBuf(painter->cache.maskibuf);
		painter->cache.ibuf= painter->cache.maskibuf= NULL;
		painter->cache.lastsize= -1; /* force ibuf create in refresh */
	}

	if (painter->cache.flt != flt) {
		if (painter->cache.texibuf) IMB_freeImBuf(painter->cache.texibuf);
		painter->cache.texibuf= NULL;
		painter->cache.lastsize= -1; /* force ibuf create in refresh */
	}

	painter->cache.size= size;
	painter->cache.flt= flt;
	painter->cache.texonly= texonly;
	painter->cache.enabled= 1;
}

void brush_painter_free(BrushPainter *painter)
{
	Brush *brush = painter->brush;

	brush->size = painter->startsize;
	brush->alpha = painter->startalpha;
	brush->jitter = painter->startjitter;
	brush->spacing = painter->startspacing;

	if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
	if (painter->cache.texibuf) IMB_freeImBuf(painter->cache.texibuf);
	if (painter->cache.maskibuf) IMB_freeImBuf(painter->cache.maskibuf);
	MEM_freeN(painter);
}

static void brush_painter_do_partial(BrushPainter *painter, ImBuf *oldtexibuf, int x, int y, int w, int h, int xt, int yt, float *pos)
{
	Brush *brush= painter->brush;
	ImBuf *ibuf, *maskibuf, *texibuf;
	float *bf, *mf, *tf, *otf=NULL, xoff, yoff, xy[2], rgba[4];
	char *b, *m, *t, *ot= NULL;
	int dotexold, origx= x, origy= y;

	xoff = -brush->size/2.0f + 0.5f;
	yoff = -brush->size/2.0f + 0.5f;
	xoff += (int)pos[0] - (int)painter->startpaintpos[0];
	yoff += (int)pos[1] - (int)painter->startpaintpos[1];

	ibuf = painter->cache.ibuf;
	texibuf = painter->cache.texibuf;
	maskibuf = painter->cache.maskibuf;

	dotexold = (oldtexibuf != NULL);

	if (painter->cache.flt) {
		for (; y < h; y++) {
			bf = ibuf->rect_float + (y*ibuf->x + origx)*4;
			tf = texibuf->rect_float + (y*texibuf->x + origx)*4;
			mf = maskibuf->rect_float + (y*maskibuf->x + origx)*4;

			if (dotexold)
				otf = oldtexibuf->rect_float + ((y - origy + yt)*oldtexibuf->x + xt)*4;

			for (x=origx; x < w; x++, bf+=4, mf+=4, tf+=4) {
				if (dotexold) {
					VECCOPY(tf, otf);
					tf[3] = otf[3];
					otf += 4;
				}
				else {
					xy[0] = x + xoff;
					xy[1] = y + yoff;

					brush_sample_tex(brush, xy, tf);
				}

				bf[0] = tf[0]*mf[0];
				bf[1] = tf[1]*mf[1];
				bf[2] = tf[2]*mf[2];
				bf[3] = tf[3]*mf[3];
			}
		}
	}
	else {
		for (; y < h; y++) {
			b = (char*)ibuf->rect + (y*ibuf->x + origx)*4;
			t = (char*)texibuf->rect + (y*texibuf->x + origx)*4;
			m = (char*)maskibuf->rect + (y*maskibuf->x + origx)*4;

			if (dotexold)
				ot = (char*)oldtexibuf->rect + ((y - origy + yt)*oldtexibuf->x + xt)*4;

			for (x=origx; x < w; x++, b+=4, m+=4, t+=4) {
				if (dotexold) {
					t[0] = ot[0];
					t[1] = ot[1];
					t[2] = ot[2];
					t[3] = ot[3];
					ot += 4;
				}
				else {
					xy[0] = x + xoff;
					xy[1] = y + yoff;

					brush_sample_tex(brush, xy, rgba);
					t[0]= FTOCHAR(rgba[0]);
					t[1]= FTOCHAR(rgba[1]);
					t[2]= FTOCHAR(rgba[2]);
					t[3]= FTOCHAR(rgba[3]);
				}

				b[0] = t[0]*m[0]/255;
				b[1] = t[1]*m[1]/255;
				b[2] = t[2]*m[2]/255;
				b[3] = t[3]*m[3]/255;
			}
		}
	}
}

static void brush_painter_fixed_tex_partial_update(BrushPainter *painter, float *pos)
{
	Brush *brush= painter->brush;
	BrushPainterCache *cache= &painter->cache;
	ImBuf *oldtexibuf, *ibuf;
	int imbflag, destx, desty, srcx, srcy, w, h, x1, y1, x2, y2;

	imbflag= (cache->flt)? IB_rectfloat: IB_rect;
	if (!cache->ibuf)
		cache->ibuf= IMB_allocImBuf(brush->size, brush->size, 32, imbflag, 0);
	ibuf= cache->ibuf;

	oldtexibuf= cache->texibuf;
	cache->texibuf= IMB_allocImBuf(brush->size, brush->size, 32, imbflag, 0);

	if (oldtexibuf) {
		srcx= srcy= 0;
		destx= (int)painter->lastpaintpos[0] - (int)pos[0];
		desty= (int)painter->lastpaintpos[1] - (int)pos[1];
		w= oldtexibuf->x;
		h= oldtexibuf->y;

		IMB_rectclip(cache->texibuf, oldtexibuf, &destx, &desty, &srcx, &srcy, &w, &h);
	}
	else {
		srcx= srcy= 0;
		destx= desty= 0;
		w= h= 0;
	}
	
	x1= destx;
	y1= desty;
	x2= destx+w;
	y2= desty+h;

	/* blend existing texture in new position */
	if ((x1 < x2) && (y1 < y2))
		brush_painter_do_partial(painter, oldtexibuf, x1, y1, x2, y2, srcx, srcy, pos);

	if (oldtexibuf)
		IMB_freeImBuf(oldtexibuf);

	/* sample texture in new areas */
	if ((0 < x1) && (0 < ibuf->y))
		brush_painter_do_partial(painter, NULL, 0, 0, x1, ibuf->y, 0, 0, pos);
	if ((x2 < ibuf->x) && (0 < ibuf->y))
		brush_painter_do_partial(painter, NULL, x2, 0, ibuf->x, ibuf->y, 0, 0, pos);
	if ((x1 < x2) && (0 < y1))
		brush_painter_do_partial(painter, NULL, x1, 0, x2, y1, 0, 0, pos);
	if ((x1 < x2) && (y2 < ibuf->y))
		brush_painter_do_partial(painter, NULL, x1, y2, x2, ibuf->y, 0, 0, pos);
}

static void brush_painter_refresh_cache(BrushPainter *painter, float *pos)
{
	Brush *brush= painter->brush;
	BrushPainterCache *cache= &painter->cache;
	MTex *mtex= brush->mtex[brush->texact];
	int size;
	short flt;

	if ((brush->size != cache->lastsize) || (brush->alpha != cache->lastalpha)
	    || (brush->jitter != cache->lastjitter)) {
		if (cache->ibuf) {
			IMB_freeImBuf(cache->ibuf);
			cache->ibuf= NULL;
		}
		if (cache->maskibuf) {
			IMB_freeImBuf(cache->maskibuf);
			cache->maskibuf= NULL;
		}

		flt= cache->flt;
		size= (cache->size)? cache->size: brush->size;

		if (!(mtex && mtex->tex) || (mtex->tex->type==0)) {
			brush_imbuf_new(brush, flt, 0, size, &cache->ibuf);
		}
		else if (brush->flag & BRUSH_FIXED_TEX) {
			brush_imbuf_new(brush, flt, 0, size, &cache->maskibuf);
			brush_painter_fixed_tex_partial_update(painter, pos);
		}
		else
			brush_imbuf_new(brush, flt, 2, size, &cache->ibuf);

		cache->lastsize= brush->size;
		cache->lastalpha= brush->alpha;
		cache->lastjitter= brush->jitter;
	}
	else if ((brush->flag & BRUSH_FIXED_TEX) && mtex && mtex->tex) {
		int dx = (int)painter->lastpaintpos[0] - (int)pos[0];
		int dy = (int)painter->lastpaintpos[1] - (int)pos[1];

		if ((dx != 0) || (dy != 0))
			brush_painter_fixed_tex_partial_update(painter, pos);
	}
}

void brush_painter_break_stroke(BrushPainter *painter)
{
	painter->firsttouch= 1;
}

static void brush_apply_pressure(BrushPainter *painter, Brush *brush, float pressure)
{
	if (brush->flag & BRUSH_ALPHA_PRESSURE) 
		brush->alpha = MAX2(0.0, painter->startalpha*pressure);
	if (brush->flag & BRUSH_SIZE_PRESSURE)
		brush->size = MAX2(1.0, painter->startsize*pressure);
	if (brush->flag & BRUSH_JITTER_PRESSURE)
		brush->jitter = MAX2(0.0, painter->startjitter*pressure);
	if (brush->flag & BRUSH_SPACING_PRESSURE)
		brush->spacing = MAX2(1.0, painter->startspacing*(1.5f-pressure));
}

static void brush_jitter_pos(Brush *brush, float *pos, float *jitterpos)
{
	if(brush->jitter){
		jitterpos[0] = pos[0] + ((BLI_frand()-0.5f) * brush->size * brush->jitter * 2);
		jitterpos[1] = pos[1] + ((BLI_frand()-0.5f) * brush->size * brush->jitter * 2);
	}
	else {
		VECCOPY2D(jitterpos, pos);
	}
}

int brush_painter_paint(BrushPainter *painter, BrushFunc func, float *pos, double time, float pressure, void *user)
{
	Brush *brush= painter->brush;
	int totpaintops= 0;

	if (pressure == 0.0f) {
		if(painter->lastpressure) // XXX - hack, operator misses
			pressure= painter->lastpressure;
		else
			pressure = 1.0f;	/* zero pressure == not using tablet */
	}
	if (painter->firsttouch) {
		/* paint exactly once on first touch */
		painter->startpaintpos[0]= pos[0];
		painter->startpaintpos[1]= pos[1];

		brush_apply_pressure(painter, brush, pressure);
		if (painter->cache.enabled)
			brush_painter_refresh_cache(painter, pos);
		totpaintops += func(user, painter->cache.ibuf, pos, pos);
		
		painter->lasttime= time;
		painter->firsttouch= 0;
		painter->lastpaintpos[0]= pos[0];
		painter->lastpaintpos[1]= pos[1];
	}
#if 0
	else if (painter->brush->flag & BRUSH_AIRBRUSH) {
		float spacing, step, paintpos[2], dmousepos[2], len;
		double starttime, curtime= time;

		/* compute brush spacing adapted to brush size */
		spacing= brush->rate; //brush->size*brush->spacing*0.01f;

		/* setup starting time, direction vector and accumulated time */
		starttime= painter->accumtime;
		sub_v2_v2v2(dmousepos, pos, painter->lastmousepos);
		len= normalize_v2(dmousepos);
		painter->accumtime += curtime - painter->lasttime;

		/* do paint op over unpainted time distance */
		while (painter->accumtime >= spacing) {
			step= (spacing - starttime)*len;
			paintpos[0]= painter->lastmousepos[0] + dmousepos[0]*step;
			paintpos[1]= painter->lastmousepos[1] + dmousepos[1]*step;

			if (painter->cache.enabled)
				brush_painter_refresh_cache(painter);
			totpaintops += func(user, painter->cache.ibuf,
				painter->lastpaintpos, paintpos);

			painter->lastpaintpos[0]= paintpos[0];
			painter->lastpaintpos[1]= paintpos[1];
			painter->accumtime -= spacing;
			starttime -= spacing;
		}
		
		painter->lasttime= curtime;
	}
#endif
	else {
		float startdistance, spacing, step, paintpos[2], dmousepos[2], finalpos[2];
		float t, len, press;

		/* compute brush spacing adapted to brush size, spacing may depend
		   on pressure, so update it */
		brush_apply_pressure(painter, brush, painter->lastpressure);
		spacing= MAX2(1.0f, brush->size)*brush->spacing*0.01f;

		/* setup starting distance, direction vector and accumulated distance */
		startdistance= painter->accumdistance;
		sub_v2_v2v2(dmousepos, pos, painter->lastmousepos);
		len= normalize_v2(dmousepos);
		painter->accumdistance += len;

		/* do paint op over unpainted distance */
		while ((len > 0.0f) && (painter->accumdistance >= spacing)) {
			step= spacing - startdistance;
			paintpos[0]= painter->lastmousepos[0] + dmousepos[0]*step;
			paintpos[1]= painter->lastmousepos[1] + dmousepos[1]*step;

			t = step/len;
			press= (1.0f-t)*painter->lastpressure + t*pressure;
			brush_apply_pressure(painter, brush, press);
			spacing= MAX2(1.0f, brush->size)*brush->spacing*0.01f;

			brush_jitter_pos(brush, paintpos, finalpos);

			if (painter->cache.enabled)
				brush_painter_refresh_cache(painter, finalpos);

			totpaintops +=
				func(user, painter->cache.ibuf, painter->lastpaintpos, finalpos);

			painter->lastpaintpos[0]= paintpos[0];
			painter->lastpaintpos[1]= paintpos[1];
			painter->accumdistance -= spacing;
			startdistance -= spacing;
		}

		/* do airbrush paint ops, based on the number of paint ops left over
		   from regular painting. this is a temporary solution until we have
		   accurate time stamps for mouse move events */
		if (brush->flag & BRUSH_AIRBRUSH) {
			double curtime= time;
			double painttime= brush->rate*totpaintops;

			painter->accumtime += curtime - painter->lasttime;
			if (painter->accumtime <= painttime)
				painter->accumtime= 0.0;
			else
				painter->accumtime -= painttime;

			while (painter->accumtime >= brush->rate) {
				brush_apply_pressure(painter, brush, pressure);

				brush_jitter_pos(brush, pos, finalpos);

				if (painter->cache.enabled)
					brush_painter_refresh_cache(painter, finalpos);

				totpaintops +=
					func(user, painter->cache.ibuf, painter->lastmousepos, finalpos);
				painter->accumtime -= brush->rate;
			}

			painter->lasttime= curtime;
		}
	}

	painter->lastmousepos[0]= pos[0];
	painter->lastmousepos[1]= pos[1];
	painter->lastpressure= pressure;

	brush->alpha = painter->startalpha;
	brush->size = painter->startsize;
	brush->jitter = painter->startjitter;
	brush->spacing = painter->startspacing;

	return totpaintops;
}

/* Uses the brush curve control to find a strength value between 0 and 1 */
float brush_curve_strength_clamp(Brush *br, float p, const float len)
{
	if(p >= len)	p= 1.0f;
	else			p= p/len;

	p= curvemapping_evaluateF(br->curve, 0, p);
	if(p < 0.0)			p= 0.0f;
	else if(p > 1.0f)	p= 1.0f;
	return p;
}
/* same as above but can return negative values if the curve enables
 * used for sculpt only */
float brush_curve_strength(Brush *br, float p, const float len)
{
	if(p >= len)	p= 1.0f;
	else			p= p/len;
	return curvemapping_evaluateF(br->curve, 0, p);
}

/* TODO: should probably be unified with BrushPainter stuff? */
unsigned int *brush_gen_texture_cache(Brush *br, int half_side)
{
	unsigned int *texcache = NULL;
	MTex *mtex = br->mtex[br->texact];
	TexResult texres;
	int hasrgb, ix, iy;
	int side = half_side * 2;

	memset(&texres, 0, sizeof(TexResult));
	
	if(mtex && mtex->tex) {
		float x, y, step = 2.0 / side, co[3];

		texcache = MEM_callocN(sizeof(int) * side * side, "Brush texture cache");

		BKE_image_get_ibuf(mtex->tex->ima, NULL);
		
		/*do normalized cannonical view coords for texture*/
		for (y=-1.0, iy=0; iy<side; iy++, y += step) {
			for (x=-1.0, ix=0; ix<side; ix++, x += step) {
				co[0]= x;
				co[1]= y;
				co[2]= 0.0f;
				
				/* This is copied from displace modifier code */
				hasrgb = multitex_ext(mtex->tex, co, NULL, NULL, 1, &texres);
			
				/* if the texture gave an RGB value, we assume it didn't give a valid
				 * intensity, so calculate one (formula from do_material_tex).
				 * if the texture didn't give an RGB value, copy the intensity across
				 */
				if(hasrgb & TEX_RGB)
					texres.tin = (0.35 * texres.tr + 0.45 *
					              texres.tg + 0.2 * texres.tb);

				texres.tin = texres.tin * 255.0;
				((char*)texcache)[(iy*side+ix)*4] = (char)texres.tin;
				((char*)texcache)[(iy*side+ix)*4+1] = (char)texres.tin;
				((char*)texcache)[(iy*side+ix)*4+2] = (char)texres.tin;
				((char*)texcache)[(iy*side+ix)*4+3] = (char)texres.tin;
			}
		}
	}

	return texcache;
}

/**** Radial Control ****/
static struct ImBuf *brush_gen_radial_control_imbuf(Brush *br)
{
	ImBuf *im = MEM_callocN(sizeof(ImBuf), "radial control texture");
	unsigned int *texcache;
	int side = 128;
	int half = side / 2;
	int i, j;

	texcache = brush_gen_texture_cache(br, half);
	im->rect_float = MEM_callocN(sizeof(float) * side * side, "radial control rect");
	im->x = im->y = side;

	for(i=0; i<side; ++i) {
		for(j=0; j<side; ++j) {
			float magn= sqrt(pow(i - half, 2) + pow(j - half, 2));
			im->rect_float[i*side + j]= brush_curve_strength_clamp(br, magn, half);
		}
	}

	/* Modulate curve with texture */
	if(texcache) {
		for(i=0; i<side; ++i) {
			for(j=0; j<side; ++j) {
				const int col= texcache[i*side+j];
				im->rect_float[i*side+j]*= (((char*)&col)[0]+((char*)&col)[1]+((char*)&col)[2])/3.0f/255.0f;
			}
		}

		MEM_freeN(texcache);
	}

	return im;
}

void brush_radial_control_invoke(wmOperator *op, Brush *br, float size_weight)
{
	int mode = RNA_int_get(op->ptr, "mode");
	float original_value= 0;

	if(mode == WM_RADIALCONTROL_SIZE)
		original_value = br->size * size_weight;
	else if(mode == WM_RADIALCONTROL_STRENGTH)
		original_value = br->alpha;
	else if(mode == WM_RADIALCONTROL_ANGLE) {
		MTex *mtex = brush_active_texture(br);
		if(mtex)
			original_value = mtex->rot;
	}

	RNA_float_set(op->ptr, "initial_value", original_value);
	op->customdata = brush_gen_radial_control_imbuf(br);
}

int brush_radial_control_exec(wmOperator *op, Brush *br, float size_weight)
{
	int mode = RNA_int_get(op->ptr, "mode");
	float new_value = RNA_float_get(op->ptr, "new_value");
	const float conv = 0.017453293;

	if(mode == WM_RADIALCONTROL_SIZE)
		br->size = new_value * size_weight;
	else if(mode == WM_RADIALCONTROL_STRENGTH)
		br->alpha = new_value;
	else if(mode == WM_RADIALCONTROL_ANGLE) {
		MTex *mtex = brush_active_texture(br);
		if(mtex)
			mtex->rot = new_value * conv;
	}

	return OPERATOR_FINISHED;
}
