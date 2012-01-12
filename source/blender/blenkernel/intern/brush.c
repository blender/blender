/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/blenkernel/intern/brush.c
 *  \ingroup bke
 */


#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

#include "WM_types.h"

#include "RNA_access.h"

#include "BLI_bpath.h"
#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_paint.h"
#include "BKE_texture.h"
#include "BKE_icons.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_render_ext.h" /* externtex */
#include "RE_shader_ext.h"

static void brush_set_defaults(Brush *brush)
{
	brush->blend = 0;
	brush->flag = 0;

	brush->ob_mode = OB_MODE_ALL_PAINT;

	/* BRUSH SCULPT TOOL SETTINGS */
	brush->size= 35; /* radius of the brush in pixels */
	brush->alpha= 0.5f; /* brush strength/intensity probably variable should be renamed? */
	brush->autosmooth_factor= 0.0f;
	brush->crease_pinch_factor= 0.5f;
	brush->sculpt_plane = SCULPT_DISP_DIR_AREA;
	brush->plane_offset= 0.0f; /* how far above or below the plane that is found by averaging the faces */
	brush->plane_trim= 0.5f;
	brush->clone.alpha= 0.5f;
	brush->normal_weight= 0.0f;
	brush->flag |= BRUSH_ALPHA_PRESSURE;

	/* BRUSH PAINT TOOL SETTINGS */
	brush->rgb[0]= 1.0f; /* default rgb color of the brush when painting - white */
	brush->rgb[1]= 1.0f;
	brush->rgb[2]= 1.0f;

	/* BRUSH STROKE SETTINGS */
	brush->flag |= (BRUSH_SPACE|BRUSH_SPACE_ATTEN);
	brush->spacing= 10; /* how far each brush dot should be spaced as a percentage of brush diameter */

	brush->smooth_stroke_radius= 75;
	brush->smooth_stroke_factor= 0.9f;

	brush->rate= 0.1f; /* time delay between dots of paint or sculpting when doing airbrush mode */

	brush->jitter= 0.0f;

	/* BRUSH TEXTURE SETTINGS */
	default_mtex(&brush->mtex);

	brush->texture_sample_bias= 0; /* value to added to texture samples */
	brush->texture_overlay_alpha= 33;

	/* brush appearance  */

	brush->add_col[0]= 1.00; /* add mode color is light red */
	brush->add_col[1]= 0.39;
	brush->add_col[2]= 0.39;

	brush->sub_col[0]= 0.39; /* subtract mode color is light blue */
	brush->sub_col[1]= 0.39;
	brush->sub_col[2]= 1.00;
}

/* Datablock add/copy/free/make_local */

Brush *add_brush(const char *name)
{
	Brush *brush;

	brush= alloc_libblock(&G.main->brush, ID_BR, name);

	/* enable fake user by default */
	brush->id.flag |= LIB_FAKEUSER;

	brush_set_defaults(brush);

	brush->sculpt_tool = SCULPT_TOOL_DRAW; /* sculpting defaults to the draw tool for new brushes */

	 /* the default alpha falloff curve */
	brush_curve_preset(brush, CURVE_PRESET_SMOOTH);

	return brush;
}

Brush *copy_brush(Brush *brush)
{
	Brush *brushn;
	
	brushn= copy_libblock(&brush->id);

	if (brush->mtex.tex)
		id_us_plus((ID*)brush->mtex.tex);

	if (brush->icon_imbuf)
		brushn->icon_imbuf= IMB_dupImBuf(brush->icon_imbuf);

	brushn->preview = NULL;

	brushn->curve= curvemapping_copy(brush->curve);

	/* enable fake user by default */
	if (!(brushn->id.flag & LIB_FAKEUSER)) {
		brushn->id.flag |= LIB_FAKEUSER;
		brushn->id.us++;
	}
	
	return brushn;
}

/* not brush itself */
void free_brush(Brush *brush)
{
	if (brush->mtex.tex)
		brush->mtex.tex->id.us--;

	if (brush->icon_imbuf)
		IMB_freeImBuf(brush->icon_imbuf);

	BKE_previewimg_free(&(brush->preview));

	curvemapping_free(brush->curve);
}

static void extern_local_brush(Brush *brush)
{
	id_lib_extern((ID *)brush->mtex.tex);
	id_lib_extern((ID *)brush->clone.image);
}

void make_local_brush(Brush *brush)
{

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	Main *bmain= G.main;
	Scene *scene;
	int is_local= FALSE, is_lib= FALSE;

	if(brush->id.lib==NULL) return;

	if(brush->clone.image) {
		/* special case: ima always local immediately. Clone image should only
		   have one user anyway. */
		id_clear_lib_data(bmain, &brush->clone.image->id);
		extern_local_brush(brush);
	}

	for(scene= bmain->scene.first; scene && ELEM(0, is_lib, is_local); scene=scene->id.next) {
		if(paint_brush(&scene->toolsettings->imapaint.paint)==brush) {
			if(scene->id.lib) is_lib= TRUE;
			else is_local= TRUE;
		}
	}

	if(is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &brush->id);
		extern_local_brush(brush);

		/* enable fake user by default */
		if (!(brush->id.flag & LIB_FAKEUSER)) {
			brush->id.flag |= LIB_FAKEUSER;
			brush->id.us++;
		}
	}
	else if(is_local && is_lib) {
		Brush *brush_new= copy_brush(brush);
		brush_new->id.us= 1; /* only keep fake user */
		brush_new->id.flag |= LIB_FAKEUSER;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, brush->id.lib, &brush_new->id);
		
		for(scene= bmain->scene.first; scene; scene=scene->id.next) {
			if(paint_brush(&scene->toolsettings->imapaint.paint)==brush) {
				if(scene->id.lib==NULL) {
					paint_brush_set(&scene->toolsettings->imapaint.paint, brush_new);
				}
			}
		}
	}
}

void brush_debug_print_state(Brush *br)
{
	/* create a fake brush and set it to the defaults */
	Brush def= {{NULL}};
	brush_set_defaults(&def);
	
#define BR_TEST(field, t)					\
	if(br->field != def.field)				\
		printf("br->" #field " = %" #t ";\n", br->field)

#define BR_TEST_FLAG(_f)				\
	if((br->flag & _f) && !(def.flag & _f))		\
		printf("br->flag |= " #_f ";\n");	\
	else if(!(br->flag & _f) && (def.flag & _f))	\
		printf("br->flag &= ~" #_f ";\n")
	

	/* print out any non-default brush state */
	BR_TEST(normal_weight, f);

	BR_TEST(blend, d);
	BR_TEST(size, d);

	/* br->flag */
	BR_TEST_FLAG(BRUSH_AIRBRUSH);
	BR_TEST_FLAG(BRUSH_TORUS);
	BR_TEST_FLAG(BRUSH_ALPHA_PRESSURE);
	BR_TEST_FLAG(BRUSH_SIZE_PRESSURE);
	BR_TEST_FLAG(BRUSH_JITTER_PRESSURE);
	BR_TEST_FLAG(BRUSH_SPACING_PRESSURE);
	BR_TEST_FLAG(BRUSH_FIXED_TEX);
	BR_TEST_FLAG(BRUSH_RAKE);
	BR_TEST_FLAG(BRUSH_ANCHORED);
	BR_TEST_FLAG(BRUSH_DIR_IN);
	BR_TEST_FLAG(BRUSH_SPACE);
	BR_TEST_FLAG(BRUSH_SMOOTH_STROKE);
	BR_TEST_FLAG(BRUSH_PERSISTENT);
	BR_TEST_FLAG(BRUSH_ACCUMULATE);
	BR_TEST_FLAG(BRUSH_LOCK_ALPHA);
	BR_TEST_FLAG(BRUSH_ORIGINAL_NORMAL);
	BR_TEST_FLAG(BRUSH_OFFSET_PRESSURE);
	BR_TEST_FLAG(BRUSH_SPACE_ATTEN);
	BR_TEST_FLAG(BRUSH_ADAPTIVE_SPACE);
	BR_TEST_FLAG(BRUSH_LOCK_SIZE);
	BR_TEST_FLAG(BRUSH_TEXTURE_OVERLAY);
	BR_TEST_FLAG(BRUSH_EDGE_TO_EDGE);
	BR_TEST_FLAG(BRUSH_RESTORE_MESH);
	BR_TEST_FLAG(BRUSH_INVERSE_SMOOTH_PRESSURE);
	BR_TEST_FLAG(BRUSH_RANDOM_ROTATION);
	BR_TEST_FLAG(BRUSH_PLANE_TRIM);
	BR_TEST_FLAG(BRUSH_FRONTFACE);
	BR_TEST_FLAG(BRUSH_CUSTOM_ICON);

	BR_TEST(jitter, f);
	BR_TEST(spacing, d);
	BR_TEST(smooth_stroke_radius, d);
	BR_TEST(smooth_stroke_factor, f);
	BR_TEST(rate, f);

	BR_TEST(alpha, f);

	BR_TEST(sculpt_plane, d);

	BR_TEST(plane_offset, f);

	BR_TEST(autosmooth_factor, f);

	BR_TEST(crease_pinch_factor, f);

	BR_TEST(plane_trim, f);

	BR_TEST(texture_sample_bias, f);
	BR_TEST(texture_overlay_alpha, d);

	BR_TEST(add_col[0], f);
	BR_TEST(add_col[1], f);
	BR_TEST(add_col[2], f);
	BR_TEST(sub_col[0], f);
	BR_TEST(sub_col[1], f);
	BR_TEST(sub_col[2], f);

	printf("\n");
	
#undef BR_TEST
#undef BR_TEST_FLAG
}

void brush_reset_sculpt(Brush *br)
{
	/* enable this to see any non-default
	   settings used by a brush:

	brush_debug_print_state(br);
	*/

	brush_set_defaults(br);
	brush_curve_preset(br, CURVE_PRESET_SMOOTH);

	switch(br->sculpt_tool) {
	case SCULPT_TOOL_CLAY:
		br->flag |= BRUSH_FRONTFACE;
		break;
	case SCULPT_TOOL_CREASE:
		br->flag |= BRUSH_DIR_IN;
		br->alpha = 0.25;
		break;
	case SCULPT_TOOL_FILL:
		br->add_col[1] = 1;
		br->sub_col[0] = 0.25;
		br->sub_col[1] = 1;
		break;
	case SCULPT_TOOL_FLATTEN:
		br->add_col[1] = 1;
		br->sub_col[0] = 0.25;
		br->sub_col[1] = 1;
		break;
	case SCULPT_TOOL_INFLATE:
		br->add_col[0] = 0.750000;
		br->add_col[1] = 0.750000;
		br->add_col[2] = 0.750000;
		br->sub_col[0] = 0.250000;
		br->sub_col[1] = 0.250000;
		br->sub_col[2] = 0.250000;
		break;
	case SCULPT_TOOL_NUDGE:
		br->add_col[0] = 0.250000;
		br->add_col[1] = 1.000000;
		br->add_col[2] = 0.250000;
		break;
	case SCULPT_TOOL_PINCH:
		br->add_col[0] = 0.750000;
		br->add_col[1] = 0.750000;
		br->add_col[2] = 0.750000;
		br->sub_col[0] = 0.250000;
		br->sub_col[1] = 0.250000;
		br->sub_col[2] = 0.250000;
		break;
	case SCULPT_TOOL_SCRAPE:
		br->add_col[1] = 1.000000;
		br->sub_col[0] = 0.250000;
		br->sub_col[1] = 1.000000;
		break;
	case SCULPT_TOOL_ROTATE:
		br->alpha = 1.0;
		break;
	case SCULPT_TOOL_SMOOTH:
		br->flag &= ~BRUSH_SPACE_ATTEN;
		br->spacing = 5;
		br->add_col[0] = 0.750000;
		br->add_col[1] = 0.750000;
		br->add_col[2] = 0.750000;
		break;
	case SCULPT_TOOL_GRAB:
	case SCULPT_TOOL_SNAKE_HOOK:
	case SCULPT_TOOL_THUMB:
		br->size = 75;
		br->flag &= ~BRUSH_ALPHA_PRESSURE;
		br->flag &= ~BRUSH_SPACE;
		br->flag &= ~BRUSH_SPACE_ATTEN;
		br->add_col[0] = 0.250000;
		br->add_col[1] = 1.000000;
		br->add_col[2] = 0.250000;
		break;
	default:
		break;
	}
}

/* Library Operations */
void brush_curve_preset(Brush *b, /*CurveMappingPreset*/int preset)
{
	CurveMap *cm = NULL;

	if(!b->curve)
		b->curve = curvemapping_add(1, 0, 0, 1, 1);

	cm = b->curve->cm;
	cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;

	b->curve->preset = preset;
	curvemap_reset(cm, &b->curve->clipr, b->curve->preset, CURVEMAP_SLOPE_NEGATIVE);
	curvemapping_changed(b->curve, 0);
}

int brush_texture_set_nr(Brush *brush, int nr)
{
	ID *idtest, *id=NULL;

	id= (ID *)brush->mtex.tex;

	idtest= (ID*)BLI_findlink(&G.main->tex, nr-1);
	if(idtest==NULL) { /* new tex */
		if(id) idtest= (ID *)copy_texture((Tex *)id);
		else idtest= (ID *)add_texture("Tex");
		idtest->us--;
	}
	if(idtest!=id) {
		brush_texture_delete(brush);

		brush->mtex.tex= (Tex*)idtest;
		id_us_plus(idtest);

		return 1;
	}

	return 0;
}

int brush_texture_delete(Brush *brush)
{
	if(brush->mtex.tex)
		brush->mtex.tex->id.us--;

	return 1;
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

/* Brush Sampling */
void brush_sample_tex(Brush *brush, float *xy, float *rgba, const int thread)
{
	MTex *mtex= &brush->mtex;

	if (mtex && mtex->tex) {
		float co[3], tin, tr, tg, tb, ta;
		int hasrgb;
		const int radius= brush_size(brush);

		co[0]= xy[0]/radius;
		co[1]= xy[1]/radius;
		co[2]= 0.0f;

		hasrgb= externtex(mtex, co, &tin, &tr, &tg, &tb, &ta, thread);

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


void brush_imbuf_new(Brush *brush, short flt, short texfall, int bufsize, ImBuf **outbuf, int use_color_correction)
{
	ImBuf *ibuf;
	float xy[2], dist, rgba[4], *dstf;
	int x, y, rowbytes, xoff, yoff, imbflag;
	const int radius= brush_size(brush);
	char *dst, crgb[3];
	const float alpha= brush_alpha(brush);
	float brush_rgb[3];
    
	imbflag= (flt)? IB_rectfloat: IB_rect;
	xoff = -bufsize/2.0f + 0.5f;
	yoff = -bufsize/2.0f + 0.5f;
	rowbytes= bufsize*4;

	if (*outbuf)
		ibuf= *outbuf;
	else
		ibuf= IMB_allocImBuf(bufsize, bufsize, 32, imbflag);

	if (flt) {
		copy_v3_v3(brush_rgb, brush->rgb);
		if(use_color_correction){
			srgb_to_linearrgb_v3_v3(brush_rgb, brush_rgb);
		}

		for (y=0; y < ibuf->y; y++) {
			dstf = ibuf->rect_float + y*rowbytes;

			for (x=0; x < ibuf->x; x++, dstf+=4) {
				xy[0] = x + xoff;
				xy[1] = y + yoff;

				if (texfall == 0) {
					dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

					copy_v3_v3(dstf, brush_rgb);
					dstf[3]= alpha*brush_curve_strength_clamp(brush, dist, radius);
				}
				else if (texfall == 1) {
					brush_sample_tex(brush, xy, dstf, 0);
				}
				else {
					dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

					brush_sample_tex(brush, xy, rgba, 0);
					mul_v3_v3v3(dstf, rgba, brush_rgb);
					dstf[3] = rgba[3]*alpha*brush_curve_strength_clamp(brush, dist, radius);
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
					dst[3]= FTOCHAR(alpha*brush_curve_strength(brush, dist, radius));
				}
				else if (texfall == 1) {
					brush_sample_tex(brush, xy, rgba, 0);
					dst[0]= FTOCHAR(rgba[0]);
					dst[1]= FTOCHAR(rgba[1]);
					dst[2]= FTOCHAR(rgba[2]);
					dst[3]= FTOCHAR(rgba[3]);
				}
				else if (texfall == 2) {
					dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

					brush_sample_tex(brush, xy, rgba, 0);
					dst[0] = FTOCHAR(rgba[0]*brush->rgb[0]);
					dst[1] = FTOCHAR(rgba[1]*brush->rgb[1]);
					dst[2] = FTOCHAR(rgba[2]*brush->rgb[2]);
					dst[3] = FTOCHAR(rgba[3]*alpha*brush_curve_strength_clamp(brush, dist, radius));
				} else {
					dist = sqrt(xy[0]*xy[0] + xy[1]*xy[1]);

					brush_sample_tex(brush, xy, rgba, 0);
					dst[0]= crgb[0];
					dst[1]= crgb[1];
					dst[2]= crgb[2];
					dst[3] = FTOCHAR(rgba[3]*alpha*brush_curve_strength_clamp(brush, dist, radius));
				}
			}
		}
	}

	*outbuf= ibuf;
}

/* Brush Painting */

typedef struct BrushPainterCache {
	short enabled;

	int size;			/* size override, if 0 uses 2*brush_size(brush) */
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
	Scene *scene;
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

BrushPainter *brush_painter_new(Scene *scene, Brush *brush)
{
	BrushPainter *painter= MEM_callocN(sizeof(BrushPainter), "BrushPainter");

	painter->brush= brush;
	painter->scene= scene;
	painter->firsttouch= 1;
	painter->cache.lastsize= -1; /* force ibuf create in refresh */

	painter->startsize = brush_size(brush);
	painter->startalpha = brush_alpha(brush);
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

	brush_set_size(brush, painter->startsize);
	brush_set_alpha(brush, painter->startalpha);
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
	const int radius= brush_size(brush);

	xoff = -radius + 0.5f;
	yoff = -radius + 0.5f;
	xoff += (int)pos[0] - (int)painter->startpaintpos[0];
	yoff += (int)pos[1] - (int)painter->startpaintpos[1];

	ibuf = painter->cache.ibuf;
	texibuf = painter->cache.texibuf;
	maskibuf = painter->cache.maskibuf;

	dotexold = (oldtexibuf != NULL);

	/* not sure if it's actually needed or it's a mistake in coords/sizes
	   calculation in brush_painter_fixed_tex_partial_update(), but without this
	   limitation memory gets corrupted at fast strokes with quite big spacing (sergey) */
	w = MIN2(w, ibuf->x);
	h = MIN2(h, ibuf->y);

	if (painter->cache.flt) {
		for (; y < h; y++) {
			bf = ibuf->rect_float + (y*ibuf->x + origx)*4;
			tf = texibuf->rect_float + (y*texibuf->x + origx)*4;
			mf = maskibuf->rect_float + (y*maskibuf->x + origx)*4;

			if (dotexold)
				otf = oldtexibuf->rect_float + ((y - origy + yt)*oldtexibuf->x + xt)*4;

			for (x=origx; x < w; x++, bf+=4, mf+=4, tf+=4) {
				if (dotexold) {
					copy_v3_v3(tf, otf);
					tf[3] = otf[3];
					otf += 4;
				}
				else {
					xy[0] = x + xoff;
					xy[1] = y + yoff;

					brush_sample_tex(brush, xy, tf, 0);
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

					brush_sample_tex(brush, xy, rgba, 0);
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
	const int diameter= 2*brush_size(brush);

	imbflag= (cache->flt)? IB_rectfloat: IB_rect;
	if (!cache->ibuf)
		cache->ibuf= IMB_allocImBuf(diameter, diameter, 32, imbflag);
	ibuf= cache->ibuf;

	oldtexibuf= cache->texibuf;
	cache->texibuf= IMB_allocImBuf(diameter, diameter, 32, imbflag);

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

static void brush_painter_refresh_cache(BrushPainter *painter, float *pos, int use_color_correction)
{
	Brush *brush= painter->brush;
	BrushPainterCache *cache= &painter->cache;
	MTex *mtex= &brush->mtex;
	int size;
	short flt;
	const int diameter= 2*brush_size(brush);
	const float alpha= brush_alpha(brush);

	if (diameter != cache->lastsize ||
		alpha != cache->lastalpha ||
		brush->jitter != cache->lastjitter)
	{
		if (cache->ibuf) {
			IMB_freeImBuf(cache->ibuf);
			cache->ibuf= NULL;
		}
		if (cache->maskibuf) {
			IMB_freeImBuf(cache->maskibuf);
			cache->maskibuf= NULL;
		}

		flt= cache->flt;
		size= (cache->size)? cache->size: diameter;

		if (brush->flag & BRUSH_FIXED_TEX) {
			brush_imbuf_new(brush, flt, 3, size, &cache->maskibuf, use_color_correction);
			brush_painter_fixed_tex_partial_update(painter, pos);
		}
		else
			brush_imbuf_new(brush, flt, 2, size, &cache->ibuf, use_color_correction);

		cache->lastsize= diameter;
		cache->lastalpha= alpha;
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
	if (brush_use_alpha_pressure(painter->scene, brush)) 
		brush_set_alpha(brush, MAX2(0.0f, painter->startalpha*pressure));
	if (brush_use_size_pressure(painter->scene, brush))
		brush_set_size(brush, MAX2(1.0f, painter->startsize*pressure));
	if (brush->flag & BRUSH_JITTER_PRESSURE)
		brush->jitter = MAX2(0.0f, painter->startjitter*pressure);
	if (brush->flag & BRUSH_SPACING_PRESSURE)
		brush->spacing = MAX2(1.0f, painter->startspacing*(1.5f-pressure));
}

void brush_jitter_pos(Brush *brush, float pos[2], float jitterpos[2])
{
	int use_jitter= brush->jitter != 0;

	/* jitter-ed brush gives weird and unpredictable result for this
	   kinds of stroke, so manyally disable jitter usage (sergey) */
	use_jitter&= (brush->flag & (BRUSH_RESTORE_MESH|BRUSH_ANCHORED)) == 0;

	if(use_jitter){
		float rand_pos[2];
		const int radius= brush_size(brush);
		const int diameter= 2*radius;

		// find random position within a circle of diameter 1
		do {
			rand_pos[0] = BLI_frand()-0.5f;
			rand_pos[1] = BLI_frand()-0.5f;
		} while (len_v2(rand_pos) > 0.5f);

		jitterpos[0] = pos[0] + 2*rand_pos[0]*diameter*brush->jitter;
		jitterpos[1] = pos[1] + 2*rand_pos[1]*diameter*brush->jitter;
	}
	else {
		copy_v2_v2(jitterpos, pos);
	}
}

int brush_painter_paint(BrushPainter *painter, BrushFunc func, float *pos, double time, float pressure, void *user, int use_color_correction)
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
			brush_painter_refresh_cache(painter, pos, use_color_correction);
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
		spacing= brush->rate; //radius*brush->spacing*0.01f;

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
		const int radius= brush_size(brush);

		/* compute brush spacing adapted to brush radius, spacing may depend
		   on pressure, so update it */
		brush_apply_pressure(painter, brush, painter->lastpressure);
		spacing= MAX2(1.0f, radius)*brush->spacing*0.01f;

		/* setup starting distance, direction vector and accumulated distance */
		startdistance= painter->accumdistance;
		sub_v2_v2v2(dmousepos, pos, painter->lastmousepos);
		len= normalize_v2(dmousepos);
		painter->accumdistance += len;

		if (brush->flag & BRUSH_SPACE) {
			/* do paint op over unpainted distance */
			while ((len > 0.0f) && (painter->accumdistance >= spacing)) {
				step= spacing - startdistance;
				paintpos[0]= painter->lastmousepos[0] + dmousepos[0]*step;
				paintpos[1]= painter->lastmousepos[1] + dmousepos[1]*step;

				t = step/len;
				press= (1.0f-t)*painter->lastpressure + t*pressure;
				brush_apply_pressure(painter, brush, press);
				spacing= MAX2(1.0f, radius)*brush->spacing*0.01f;

				brush_jitter_pos(brush, paintpos, finalpos);

				if (painter->cache.enabled)
					brush_painter_refresh_cache(painter, finalpos, use_color_correction);

				totpaintops +=
					func(user, painter->cache.ibuf, painter->lastpaintpos, finalpos);

				painter->lastpaintpos[0]= paintpos[0];
				painter->lastpaintpos[1]= paintpos[1];
				painter->accumdistance -= spacing;
				startdistance -= spacing;
			}
		} else {
			brush_jitter_pos(brush, pos, finalpos);

			if (painter->cache.enabled)
				brush_painter_refresh_cache(painter, finalpos, use_color_correction);

			totpaintops += func(user, painter->cache.ibuf, pos, finalpos);

			painter->lastpaintpos[0]= pos[0];
			painter->lastpaintpos[1]= pos[1];
			painter->accumdistance= 0;
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

			while (painter->accumtime >= (double)brush->rate) {
				brush_apply_pressure(painter, brush, pressure);

				brush_jitter_pos(brush, pos, finalpos);

				if (painter->cache.enabled)
					brush_painter_refresh_cache(painter, finalpos, use_color_correction);

				totpaintops +=
					func(user, painter->cache.ibuf, painter->lastmousepos, finalpos);
				painter->accumtime -= (double)brush->rate;
			}

			painter->lasttime= curtime;
		}
	}

	painter->lastmousepos[0]= pos[0];
	painter->lastmousepos[1]= pos[1];
	painter->lastpressure= pressure;

	brush_set_alpha(brush, painter->startalpha);
	brush_set_size(brush, painter->startsize);
	brush->jitter = painter->startjitter;
	brush->spacing = painter->startspacing;

	return totpaintops;
}

/* Uses the brush curve control to find a strength value between 0 and 1 */
float brush_curve_strength_clamp(Brush *br, float p, const float len)
{
	if(p >= len)	return 0;
	else			p= p/len;

	p= curvemapping_evaluateF(br->curve, 0, p);
	if(p < 0.0f)		p= 0.0f;
	else if(p > 1.0f)	p= 1.0f;
	return p;
}
/* same as above but can return negative values if the curve enables
 * used for sculpt only */
float brush_curve_strength(Brush *br, float p, const float len)
{
	if(p >= len)
		p= 1.0f;
	else
		p= p/len;

	return curvemapping_evaluateF(br->curve, 0, p);
}

/* TODO: should probably be unified with BrushPainter stuff? */
unsigned int *brush_gen_texture_cache(Brush *br, int half_side)
{
	unsigned int *texcache = NULL;
	MTex *mtex = &br->mtex;
	TexResult texres= {0};
	int hasrgb, ix, iy;
	int side = half_side * 2;
	
	if(mtex->tex) {
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
				hasrgb = multitex_ext(mtex->tex, co, NULL, NULL, 0, &texres);
			
				/* if the texture gave an RGB value, we assume it didn't give a valid
				 * intensity, so calculate one (formula from do_material_tex).
				 * if the texture didn't give an RGB value, copy the intensity across
				 */
				if(hasrgb & TEX_RGB)
					texres.tin = (0.35f * texres.tr + 0.45f *
								  texres.tg + 0.2f * texres.tb);

				texres.tin = texres.tin * 255.0f;
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
struct ImBuf *brush_gen_radial_control_imbuf(Brush *br)
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

/* Unified Size and Strength */

/* XXX, wouldnt it be better to only pass the active scene?
 * this can return any old scene! - campbell*/

static short unified_settings(Brush *brush)
{
	Scene *sce;
	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->toolsettings && 
			ELEM4(brush,
			    paint_brush(&(sce->toolsettings->imapaint.paint)),
			    paint_brush(&(sce->toolsettings->vpaint->paint)),
			    paint_brush(&(sce->toolsettings->wpaint->paint)),
			    paint_brush(&(sce->toolsettings->sculpt->paint))))
		{
			return sce->toolsettings->unified_paint_settings.flag;
		}
	}

	return 0;
}

// XXX: be careful about setting size and unprojected radius
// because they depend on one another
// these functions do not set the other corresponding value
// this can lead to odd behavior if size and unprojected
// radius become inconsistent.
// the biggest problem is that it isn't possible to change
// unprojected radius because a view context is not
// available.  my ussual solution to this is to use the
// ratio of change of the size to change the unprojected
// radius.  Not completely convinced that is correct.
// In anycase, a better solution is needed to prevent
// inconsistency.

static void set_unified_size(Brush *brush, int value)
{
	Scene *sce;
	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->toolsettings && 
			ELEM4(brush,
			    paint_brush(&(sce->toolsettings->imapaint.paint)),
			    paint_brush(&(sce->toolsettings->vpaint->paint)),
			    paint_brush(&(sce->toolsettings->wpaint->paint)),
			    paint_brush(&(sce->toolsettings->sculpt->paint))))
		{
			sce->toolsettings->unified_paint_settings.size= value;
		}
	}
}

static int unified_size(Brush *brush)
{
	Scene *sce;
	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->toolsettings && 
			ELEM4(brush,
			    paint_brush(&(sce->toolsettings->imapaint.paint)),
			    paint_brush(&(sce->toolsettings->vpaint->paint)),
			    paint_brush(&(sce->toolsettings->wpaint->paint)),
			    paint_brush(&(sce->toolsettings->sculpt->paint))))
		{
			return sce->toolsettings->unified_paint_settings.size;
		}
	}

	return 35; // XXX magic number
}

static void set_unified_alpha(Brush *brush, float value)
{
	Scene *sce;
	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->toolsettings && 
			ELEM4(brush,
			    paint_brush(&(sce->toolsettings->imapaint.paint)),
			    paint_brush(&(sce->toolsettings->vpaint->paint)),
			    paint_brush(&(sce->toolsettings->wpaint->paint)),
			    paint_brush(&(sce->toolsettings->sculpt->paint))))
		{
			sce->toolsettings->unified_paint_settings.alpha= value;
		}
	}
}

static float unified_alpha(Brush *brush)
{
	Scene *sce;
	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->toolsettings && 
			ELEM4(brush,
			    paint_brush(&(sce->toolsettings->imapaint.paint)),
			    paint_brush(&(sce->toolsettings->vpaint->paint)),
			    paint_brush(&(sce->toolsettings->wpaint->paint)),
			    paint_brush(&(sce->toolsettings->sculpt->paint))))
		{
			return sce->toolsettings->unified_paint_settings.alpha;
		}
	}

	return 0.5f; // XXX magic number
}

static void set_unified_unprojected_radius(Brush *brush, float value)
{
	Scene *sce;
	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->toolsettings && 
			ELEM4(brush,
			    paint_brush(&(sce->toolsettings->imapaint.paint)),
			    paint_brush(&(sce->toolsettings->vpaint->paint)),
			    paint_brush(&(sce->toolsettings->wpaint->paint)),
			    paint_brush(&(sce->toolsettings->sculpt->paint))))
		{
			sce->toolsettings->unified_paint_settings.unprojected_radius= value;
		}
	}
}

static float unified_unprojected_radius(Brush *brush)
{
	Scene *sce;
	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->toolsettings && 
			ELEM4(brush,
			    paint_brush(&(sce->toolsettings->imapaint.paint)),
			    paint_brush(&(sce->toolsettings->vpaint->paint)),
			    paint_brush(&(sce->toolsettings->wpaint->paint)),
			    paint_brush(&(sce->toolsettings->sculpt->paint))))
		{
			return sce->toolsettings->unified_paint_settings.unprojected_radius;
		}
	}

	return 0.125f; // XXX magic number
}
void brush_set_size(Brush *brush, int size)
{
	const short us_flag = unified_settings(brush);

	if (us_flag & UNIFIED_PAINT_SIZE)
		set_unified_size(brush, size);
	else
		brush->size= size;

	//WM_main_add_notifier(NC_BRUSH|NA_EDITED, brush);
}

int brush_size(Brush *brush)
{
	const short us_flag = unified_settings(brush);

	return (us_flag & UNIFIED_PAINT_SIZE) ? unified_size(brush) : brush->size;
}

int brush_use_locked_size(const Scene *scene, Brush *brush)
{
	const short us_flag = scene->toolsettings->unified_paint_settings.flag;

	return (us_flag & UNIFIED_PAINT_SIZE) ?
	        (us_flag & UNIFIED_PAINT_BRUSH_LOCK_SIZE) :
	        (brush->flag & BRUSH_LOCK_SIZE);
}

int brush_use_size_pressure(const Scene *scene, Brush *brush)
{
	const short us_flag = scene->toolsettings->unified_paint_settings.flag;

	return (us_flag & UNIFIED_PAINT_SIZE) ?
	        (us_flag & UNIFIED_PAINT_BRUSH_SIZE_PRESSURE) :
	        (brush->flag & BRUSH_SIZE_PRESSURE);
}

int brush_use_alpha_pressure(const Scene *scene, Brush *brush)
{
	const short us_flag = scene->toolsettings->unified_paint_settings.flag;

	return (us_flag & UNIFIED_PAINT_ALPHA) ?
	        (us_flag & UNIFIED_PAINT_BRUSH_ALPHA_PRESSURE) :
	        (brush->flag & BRUSH_ALPHA_PRESSURE);
}

void brush_set_unprojected_radius(Brush *brush, float unprojected_radius)
{
	const short us_flag = unified_settings(brush);

	if (us_flag & UNIFIED_PAINT_SIZE)
		set_unified_unprojected_radius(brush, unprojected_radius);
	else
		brush->unprojected_radius= unprojected_radius;

	//WM_main_add_notifier(NC_BRUSH|NA_EDITED, brush);
}

float brush_unprojected_radius(Brush *brush)
{
	const short us_flag = unified_settings(brush);

	return (us_flag & UNIFIED_PAINT_SIZE) ?
	        unified_unprojected_radius(brush) :
	        brush->unprojected_radius;
}

void brush_set_alpha(Brush *brush, float alpha)
{
	const short us_flag = unified_settings(brush);

	if (us_flag & UNIFIED_PAINT_ALPHA)
		set_unified_alpha(brush, alpha);
	else
		brush->alpha= alpha;

	//WM_main_add_notifier(NC_BRUSH|NA_EDITED, brush);
}

float brush_alpha(Brush *brush)
{
	const short us_flag = unified_settings(brush);

	return (us_flag & UNIFIED_PAINT_ALPHA) ?
	        unified_alpha(brush) :
	        brush->alpha;
}
