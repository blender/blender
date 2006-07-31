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

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_brush.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_render_ext.h" /* externtex */

/* Datablock add/copy/free/make_local */

Brush *add_brush(char *name)
{
	Brush *brush;

	brush= alloc_libblock(&G.main->brush, ID_BR, name);

	brush->rgb[0]= 1.0f;
	brush->rgb[1]= 1.0f;
	brush->rgb[2]= 1.0f;
	brush->alpha= 0.2f;
	brush->size= 25;
	brush->spacing= 10.0f;
	brush->rate= 0.1f;
	brush->innerradius= 0.5f;
	brush->clone.alpha= 0.5;

	/* enable fake user by default */
	brush_toggle_fake_user(brush);
	
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

	/* enable fake user by default */
	if (!(brushn->id.flag & LIB_FAKEUSER))
		brush_toggle_fake_user(brushn);
	
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
		if(scene->toolsettings->imapaint.brush==brush) {
			if(scene->id.lib) lib= 1;
			else local= 1;
		}
	
	if(local && lib==0) {
		brush->id.lib= 0;
		brush->id.flag= LIB_LOCAL;
		new_id(0, (ID *)brush, 0);

		/* enable fake user by default */
		if (!(brush->id.flag & LIB_FAKEUSER))
			brush_toggle_fake_user(brush);
	}
	else if(local && lib) {
		brushn= copy_brush(brush);
		brushn->id.us= 1; /* only keep fake user */
		brushn->id.flag |= LIB_FAKEUSER;
		
		for(scene= G.main->scene.first; scene; scene=scene->id.next)
			if(scene->toolsettings->imapaint.brush==brush)
				if(scene->id.lib==0) {
					scene->toolsettings->imapaint.brush= brushn;
					brushn->id.us++;
					brush->id.us--;
				}
	}
}

/* Library Operations */

int brush_set_nr(Brush **current_brush, int nr)
{
	ID *idtest, *id;
	
	id= (ID*)(*current_brush);
	idtest= (ID*)BLI_findlink(&G.main->brush, nr-1);
	
	if(idtest==0) { /* new brush */
		if(id) idtest= (ID *)copy_brush((Brush *)id);
		else idtest= (ID *)add_brush("Brush");
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

void brush_toggle_fake_user(Brush *brush)
{
	ID *id= (ID*)brush;
	if(id) {
		if(id->flag & LIB_FAKEUSER) {
			id->flag -= LIB_FAKEUSER;
			id->us--;
		} else {
			id->flag |= LIB_FAKEUSER;
			id_us_plus(id);
		}
	}
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

void brush_check_exists(Brush **brush)
{
	if(*brush==NULL)
		brush_set_nr(brush, 1);
}

/* Brush Sampling */

static float brush_sample_falloff(Brush *brush, float dist)
{
	float a, outer, inner;

	outer = brush->size >> 1;
	inner = outer*brush->innerradius;

	if (dist <= inner) {
		return brush->alpha;
	}
	else if ((dist < outer) && (inner < outer)) {
		/* formula used by sculpt:
		   0.5f * (cos(3*(dist - inner)/(outer - inner)) + 1); */
		a = sqrt((dist - inner)/(outer - inner));
		return (1 - a)*brush->alpha;
	}
	else 
		return 0.0f;
}

void brush_sample(Brush *brush, float *xy, float dist, float *rgb, float *alpha, short texonly)
{
	if (alpha) {
		if (texonly) *alpha= 1.0;
		else *alpha= brush_sample_falloff(brush, dist);
	}
	
	if (xy && brush->mtex[0] && brush->mtex[0]->tex) {
		float co[3], tin, tr, tg, tb, ta;
		int hasrgb;
		
		co[0]= xy[0]/(brush->size >> 1);
		co[1]= xy[1]/(brush->size >> 1);
		co[2]= 0.0f;

		hasrgb= externtex(brush->mtex[0], co, &tin, &tr, &tg, &tb, &ta);

		if (rgb) {
			if (hasrgb) {
				rgb[0]= tr*brush->rgb[0];
				rgb[1]= tg*brush->rgb[1];
				rgb[2]= tb*brush->rgb[2];
			}
			else {
				rgb[0]= tin*brush->rgb[0];
				rgb[1]= tin*brush->rgb[1];
				rgb[2]= tin*brush->rgb[2];
			}
		}
		if (alpha && hasrgb)
			*alpha *= ta;
	}
	else if (rgb)
		VECCOPY(rgb, brush->rgb)
}

#define FTOCHAR(val) val<=0.0f?0: (val>=1.0f?255: (char)(255.0f*val))

ImBuf *brush_imbuf_new(Brush *brush, short flt, short texonly, int size)
{
	ImBuf *ibuf;
	float w_2, h_2, xy[2], dist, rgba[3], *dstf;
	unsigned int x, y, rowbytes;
	char *dst;

	if (texonly && !(brush->mtex[0] && brush->mtex[0]->tex))
		return NULL;

	w_2 = size/2.0f;
	h_2 = size/2.0f;
	rowbytes= size*4;

	if (flt) {
		ibuf= IMB_allocImBuf(size, size, 32, IB_rectfloat, 0);

		for (y=0; y < ibuf->y; y++) {
			dstf = ibuf->rect_float + y*rowbytes;

			for (x=0; x < ibuf->x; x++, dstf+=4) {
				xy[0] = x + 0.5f - w_2;
				xy[1] = y + 0.5f - h_2;
				dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

				brush_sample(brush, xy, dist, dstf, dstf+3, texonly);
			}
		}
	}
	else {
		ibuf= IMB_allocImBuf(size, size, 32, IB_rect, 0);

		for (y=0; y < ibuf->y; y++) {
			dst = (char*)ibuf->rect + y*rowbytes;

			for (x=0; x < ibuf->x; x++, dst+=4) {
				xy[0] = x + 0.5f - w_2;
				xy[1] = y + 0.5f - h_2;
				dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

				brush_sample(brush, xy, dist, rgba, rgba+3, texonly);
				dst[0]= FTOCHAR(rgba[0]);
				dst[1]= FTOCHAR(rgba[1]);
				dst[2]= FTOCHAR(rgba[2]);
				dst[3]= FTOCHAR(rgba[3]);
			}
		}
	}

	return ibuf;
}

/* Brush Painting */

struct BrushPainter {
	Brush *brush;

	float lastmousepos[2];	/* mouse position of last paint call */

	float accumdistance;	/* accumulated distance of brush since last paint op */
	float lastpaintpos[2];	/* position of last paint op */

	double accumtime;		/* accumulated time since last paint op (airbrush) */
	double lasttime;		/* time of last update */

	short firsttouch;		/* first paint op */

	struct BrushPainterImbufCache {
		int size;			/* size override, if 0 uses brush->size */
		short flt;			/* need float imbuf? */
		short texonly;		/* no alpha, color or fallof, only texture in imbuf */
		short enabled;

		int lastsize;
		float lastalpha;
		float lastinnerradius;

		ImBuf *ibuf;
	} cache;
};

BrushPainter *brush_painter_new(Brush *brush)
{
	BrushPainter *painter= MEM_callocN(sizeof(BrushPainter), "BrushPainter");

	painter->brush= brush;
	painter->firsttouch= 1;

	return painter;
}

void brush_painter_require_imbuf(BrushPainter *painter, short flt, short texonly, int size)
{
	painter->cache.size = size;
	painter->cache.flt = flt;
	painter->cache.texonly = texonly;
	painter->cache.enabled = 1;
}

void brush_painter_free(BrushPainter *painter)
{
	if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
	MEM_freeN(painter);
}

static void brush_painter_refresh_cache(BrushPainter *painter)
{
	Brush *brush= painter->brush;

	if ((brush->size != painter->cache.lastsize)
	    || (brush->alpha != painter->cache.lastalpha)
	    || (brush->innerradius != painter->cache.lastinnerradius)) {

		if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);

		painter->cache.ibuf= brush_imbuf_new(brush,
			painter->cache.flt, painter->cache.texonly,
			painter->cache.size? painter->cache.size: brush->size);

		painter->cache.lastsize= brush->size;
		painter->cache.lastalpha= brush->alpha;
		painter->cache.lastinnerradius= brush->innerradius;
	}
}

int brush_painter_paint(BrushPainter *painter, BrushFunc func, float *pos, double time, void *user)
{
	Brush *brush= painter->brush;
	int totpaintops= 0;

	if (painter->firsttouch) {
		/* paint exactly once on first touch */
		if (painter->cache.enabled) brush_painter_refresh_cache(painter);
		totpaintops += func(user, painter->cache.ibuf, pos, pos);
		
		painter->lastpaintpos[0]= pos[0];
		painter->lastpaintpos[1]= pos[1];
		painter->lasttime= time;

		painter->firsttouch= 0;
	}
#if 0
	else if (painter->brush->flag & BRUSH_AIRBRUSH) {
		float spacing, step, paintpos[2], dmousepos[2], len;
		double starttime, curtime= time;

		/* compute brush spacing adapted to brush size */
		spacing= brush->rate; //brush->size*brush->spacing*0.01f;

		/* setup starting time, direction vector and accumulated time */
		starttime= painter->accumtime;
		Vec2Subf(dmousepos, pos, painter->lastmousepos);
		len= Normalise2(dmousepos);
		painter->accumtime += curtime - painter->lasttime;

		/* do paint op over unpainted time distance */
		while (painter->accumtime >= spacing) {
			step= (spacing - starttime)*len;
			paintpos[0]= painter->lastmousepos[0] + dmousepos[0]*step;
			paintpos[1]= painter->lastmousepos[1] + dmousepos[1]*step;

			if (painter->cache.enabled) brush_painter_refresh_cache(painter);
			totpaintops += func(user, painter->cache.ibuf, painter->lastpaintpos, paintpos);

			painter->lastpaintpos[0]= paintpos[0];
			painter->lastpaintpos[1]= paintpos[1];
			painter->accumtime -= spacing;
			starttime -= spacing;
		}
		
		painter->lasttime= curtime;
	}
#endif
	else {
		float startdistance, spacing, step, paintpos[2], dmousepos[2];

		/* compute brush spacing adapted to brush size */
		spacing= brush->size*brush->spacing*0.01f;

		/* setup starting distance, direction vector and accumulated distance */
		startdistance= painter->accumdistance;
		Vec2Subf(dmousepos, pos, painter->lastmousepos);
		painter->accumdistance += Normalise2(dmousepos);

		/* do paint op over unpainted distance */
		while (painter->accumdistance >= spacing) {
			step= spacing - startdistance;
			paintpos[0]= painter->lastmousepos[0] + dmousepos[0]*step;
			paintpos[1]= painter->lastmousepos[1] + dmousepos[1]*step;

			if (painter->cache.enabled) brush_painter_refresh_cache(painter);
			totpaintops += func(user, painter->cache.ibuf, painter->lastpaintpos, paintpos);

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
				if (painter->cache.enabled) brush_painter_refresh_cache(painter);
				totpaintops += func(user, painter->cache.ibuf, painter->lastmousepos, pos);
				painter->accumtime -= brush->rate;
			}

			painter->lasttime= curtime;
		}
	}

	painter->lastmousepos[0]= pos[0];
	painter->lastmousepos[1]= pos[1];

	return totpaintops;
}


