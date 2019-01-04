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
 * Contributors: Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_icons.c
 *  \ingroup edinterface
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "GPU_extensions.h"
#include "GPU_basic_shader.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_appdir.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_datafiles.h"
#include "ED_keyframes_draw.h"
#include "ED_render.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "interface_intern.h"

#ifndef WITH_HEADLESS
#define ICON_GRID_COLS      26
#define ICON_GRID_ROWS      30

#define ICON_GRID_MARGIN    10
#define ICON_GRID_W         32
#define ICON_GRID_H         32
#endif  /* WITH_HEADLESS */

typedef struct IconImage {
	int w;
	int h;
	uint *rect;
	uchar *datatoc_rect;
	int datatoc_size;
} IconImage;

typedef void (*VectorDrawFunc)(int x, int y, int w, int h, float alpha);

#define ICON_TYPE_PREVIEW   0
#define ICON_TYPE_TEXTURE   1
#define ICON_TYPE_BUFFER    2
#define ICON_TYPE_VECTOR    3

typedef struct DrawInfo {
	int type;

	union {
		/* type specific data */
		struct {
			VectorDrawFunc func;
		} vector;
		struct {
			IconImage *image;
		} buffer;
		struct {
			int x, y, w, h;
		} texture;
	} data;
} DrawInfo;

typedef struct IconTexture {
	GLuint id;
	int w;
	int h;
	float invw;
	float invh;
} IconTexture;

/* ******************* STATIC LOCAL VARS ******************* */
/* static here to cache results of icon directory scan, so it's not
 * scanning the filesystem each time the menu is drawn */
static struct ListBase iconfilelist = {NULL, NULL};
static IconTexture icongltex = {0, 0, 0, 0.0f, 0.0f};

/* **************************************************** */

#ifndef WITH_HEADLESS

static DrawInfo *def_internal_icon(ImBuf *bbuf, int icon_id, int xofs, int yofs, int size, int type)
{
	Icon *new_icon = NULL;
	IconImage *iimg = NULL;
	DrawInfo *di;

	new_icon = MEM_callocN(sizeof(Icon), "texicon");

	new_icon->obj = NULL; /* icon is not for library object */
	new_icon->id_type = 0;

	di = MEM_callocN(sizeof(DrawInfo), "drawinfo");
	di->type = type;

	if (type == ICON_TYPE_TEXTURE) {
		di->data.texture.x = xofs;
		di->data.texture.y = yofs;
		di->data.texture.w = size;
		di->data.texture.h = size;
	}
	else if (type == ICON_TYPE_BUFFER) {
		iimg = MEM_callocN(sizeof(IconImage), "icon_img");
		iimg->w = size;
		iimg->h = size;

		/* icon buffers can get initialized runtime now, via datatoc */
		if (bbuf) {
			int y, imgsize;

			iimg->rect = MEM_mallocN(size * size * sizeof(uint), "icon_rect");

			/* Here we store the rect in the icon - same as before */
			if (size == bbuf->x && size == bbuf->y && xofs == 0 && yofs == 0)
				memcpy(iimg->rect, bbuf->rect, size * size * sizeof(int));
			else {
				/* this code assumes square images */
				imgsize = bbuf->x;
				for (y = 0; y < size; y++) {
					memcpy(&iimg->rect[y * size], &bbuf->rect[(y + yofs) * imgsize + xofs], size * sizeof(int));
				}
			}
		}
		di->data.buffer.image = iimg;
	}

	new_icon->drawinfo_free = UI_icons_free_drawinfo;
	new_icon->drawinfo = di;

	BKE_icon_set(icon_id, new_icon);

	return di;
}

static void def_internal_vicon(int icon_id, VectorDrawFunc drawFunc)
{
	Icon *new_icon = NULL;
	DrawInfo *di;

	new_icon = MEM_callocN(sizeof(Icon), "texicon");

	new_icon->obj = NULL; /* icon is not for library object */
	new_icon->id_type = 0;

	di = MEM_callocN(sizeof(DrawInfo), "drawinfo");
	di->type = ICON_TYPE_VECTOR;
	di->data.vector.func = drawFunc;

	new_icon->drawinfo_free = NULL;
	new_icon->drawinfo = di;

	BKE_icon_set(icon_id, new_icon);
}

/* Vector Icon Drawing Routines */

/* Utilities */

static void viconutil_set_point(GLint pt[2], int x, int y)
{
	pt[0] = x;
	pt[1] = y;
}

static void vicon_small_tri_right_draw(int x, int y, int w, int UNUSED(h), float alpha)
{
	GLint pts[3][2];
	int cx = x + w / 2 - 4;
	int cy = y + w / 2;
	int d = w / 5, d2 = w / 7;

	viconutil_set_point(pts[0], cx - d2, cy + d);
	viconutil_set_point(pts[1], cx - d2, cy - d);
	viconutil_set_point(pts[2], cx + d2, cy);

	glColor4f(0.2f, 0.2f, 0.2f, alpha);

	glBegin(GL_TRIANGLES);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glVertex2iv(pts[2]);
	glEnd();
}

static void vicon_keytype_draw_wrapper(int x, int y, int w, int h, float alpha, short key_type)
{
	/* init dummy theme state for Action Editor - where these colors are defined
	 * (since we're doing this offscreen, free from any particular space_id)
	 */
	struct bThemeState theme_state;
	int xco, yco;

	UI_Theme_Store(&theme_state);
	UI_SetTheme(SPACE_ACTION, RGN_TYPE_WINDOW);

	/* the "x" and "y" given are the bottom-left coordinates of the icon,
	 * while the draw_keyframe_shape() function needs the midpoint for
	 * the keyframe
	 */
	xco = x + w / 2;
	yco = y + h / 2;

	/* draw keyframe
	 * - xscale: 1.0 (since there's no timeline scaling to compensate for)
	 * - yscale: 0.3 * h (found out experimentally... dunno why!)
	 * - sel: true (so that "keyframe" state shows the iconic yellow icon)
	 */
	draw_keyframe_shape(xco, yco, 1.0f, 0.3f * h, true, key_type, KEYFRAME_SHAPE_BOTH, alpha);

	UI_Theme_Restore(&theme_state);
}

static void vicon_keytype_keyframe_draw(int x, int y, int w, int h, float alpha)
{
	vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME);
}

static void vicon_keytype_breakdown_draw(int x, int y, int w, int h, float alpha)
{
	vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_BREAKDOWN);
}

static void vicon_keytype_extreme_draw(int x, int y, int w, int h, float alpha)
{
	vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_EXTREME);
}

static void vicon_keytype_jitter_draw(int x, int y, int w, int h, float alpha)
{
	vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_JITTER);
}

static void vicon_keytype_moving_hold_draw(int x, int y, int w, int h, float alpha)
{
	vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_MOVEHOLD);
}

static void vicon_colorset_draw(int index, int x, int y, int w, int h, float UNUSED(alpha))
{
	bTheme *btheme = UI_GetTheme();
	ThemeWireColor *cs = &btheme->tarm[index];

	/* Draw three bands of color: One per color
	 *    x-----a-----b-----c
	 *    |  N  |  S  |  A  |
	 *    x-----a-----b-----c
	 */
	const int a = x + w / 3;
	const int b = x + w / 3 * 2;
	const int c = x + w;

	/* XXX: Include alpha into this... */
	/* normal */
	glColor3ubv((uchar *)cs->solid);
	glRecti(x, y, a, y + h);

	/* selected */
	glColor3ubv((uchar *)cs->select);
	glRecti(a, y, b, y + h);

	/* active */
	glColor3ubv((uchar *)cs->active);
	glRecti(b, y, c, y + h);
}

#define DEF_VICON_COLORSET_DRAW_NTH(prefix, index)                                    \
	static void vicon_colorset_draw_##prefix(int x, int y, int w, int h, float alpha) \
	{                                                                                 \
		vicon_colorset_draw(index, x, y, w, h, alpha);                                \
	}

DEF_VICON_COLORSET_DRAW_NTH(01, 0)
DEF_VICON_COLORSET_DRAW_NTH(02, 1)
DEF_VICON_COLORSET_DRAW_NTH(03, 2)
DEF_VICON_COLORSET_DRAW_NTH(04, 3)
DEF_VICON_COLORSET_DRAW_NTH(05, 4)
DEF_VICON_COLORSET_DRAW_NTH(06, 5)
DEF_VICON_COLORSET_DRAW_NTH(07, 6)
DEF_VICON_COLORSET_DRAW_NTH(08, 7)
DEF_VICON_COLORSET_DRAW_NTH(09, 8)
DEF_VICON_COLORSET_DRAW_NTH(10, 9)
DEF_VICON_COLORSET_DRAW_NTH(11, 10)
DEF_VICON_COLORSET_DRAW_NTH(12, 11)
DEF_VICON_COLORSET_DRAW_NTH(13, 12)
DEF_VICON_COLORSET_DRAW_NTH(14, 13)
DEF_VICON_COLORSET_DRAW_NTH(15, 14)
DEF_VICON_COLORSET_DRAW_NTH(16, 15)
DEF_VICON_COLORSET_DRAW_NTH(17, 16)
DEF_VICON_COLORSET_DRAW_NTH(18, 17)
DEF_VICON_COLORSET_DRAW_NTH(19, 18)
DEF_VICON_COLORSET_DRAW_NTH(20, 19)

#undef DEF_VICON_COLORSET_DRAW_NTH

#ifndef WITH_HEADLESS

static void init_brush_icons(void)
{

#define INIT_BRUSH_ICON(icon_id, name)                                          \
	{                                                                           \
		uchar *rect = (uchar *)datatoc_ ##name## _png;          \
		int size = datatoc_ ##name## _png_size;                                 \
		DrawInfo *di;                                                           \
		\
		di = def_internal_icon(NULL, icon_id, 0, 0, w, ICON_TYPE_BUFFER);       \
		di->data.buffer.image->datatoc_rect = rect;                             \
		di->data.buffer.image->datatoc_size = size;                             \
	}
	/* end INIT_BRUSH_ICON */

	const int w = 96; /* warning, brush size hardcoded in C, but it gets scaled */

	INIT_BRUSH_ICON(ICON_BRUSH_ADD, add);
	INIT_BRUSH_ICON(ICON_BRUSH_BLOB, blob);
	INIT_BRUSH_ICON(ICON_BRUSH_BLUR, blur);
	INIT_BRUSH_ICON(ICON_BRUSH_CLAY, clay);
	INIT_BRUSH_ICON(ICON_BRUSH_CLAY_STRIPS, claystrips);
	INIT_BRUSH_ICON(ICON_BRUSH_CLONE, clone);
	INIT_BRUSH_ICON(ICON_BRUSH_CREASE, crease);
	INIT_BRUSH_ICON(ICON_BRUSH_DARKEN, darken);
	INIT_BRUSH_ICON(ICON_BRUSH_SCULPT_DRAW, draw);
	INIT_BRUSH_ICON(ICON_BRUSH_FILL, fill);
	INIT_BRUSH_ICON(ICON_BRUSH_FLATTEN, flatten);
	INIT_BRUSH_ICON(ICON_BRUSH_GRAB, grab);
	INIT_BRUSH_ICON(ICON_BRUSH_INFLATE, inflate);
	INIT_BRUSH_ICON(ICON_BRUSH_LAYER, layer);
	INIT_BRUSH_ICON(ICON_BRUSH_LIGHTEN, lighten);
	INIT_BRUSH_ICON(ICON_BRUSH_MASK, mask);
	INIT_BRUSH_ICON(ICON_BRUSH_MIX, mix);
	INIT_BRUSH_ICON(ICON_BRUSH_MULTIPLY, multiply);
	INIT_BRUSH_ICON(ICON_BRUSH_NUDGE, nudge);
	INIT_BRUSH_ICON(ICON_BRUSH_PINCH, pinch);
	INIT_BRUSH_ICON(ICON_BRUSH_SCRAPE, scrape);
	INIT_BRUSH_ICON(ICON_BRUSH_SMEAR, smear);
	INIT_BRUSH_ICON(ICON_BRUSH_SMOOTH, smooth);
	INIT_BRUSH_ICON(ICON_BRUSH_SNAKE_HOOK, snake_hook);
	INIT_BRUSH_ICON(ICON_BRUSH_SOFTEN, soften);
	INIT_BRUSH_ICON(ICON_BRUSH_SUBTRACT, subtract);
	INIT_BRUSH_ICON(ICON_BRUSH_TEXDRAW, texdraw);
	INIT_BRUSH_ICON(ICON_BRUSH_TEXFILL, texfill);
	INIT_BRUSH_ICON(ICON_BRUSH_TEXMASK, texmask);
	INIT_BRUSH_ICON(ICON_BRUSH_THUMB, thumb);
	INIT_BRUSH_ICON(ICON_BRUSH_ROTATE, twist);
	INIT_BRUSH_ICON(ICON_BRUSH_VERTEXDRAW, vertexdraw);

#undef INIT_BRUSH_ICON
}

static void icon_verify_datatoc(IconImage *iimg)
{
	/* if it has own rect, things are all OK */
	if (iimg->rect)
		return;

	if (iimg->datatoc_rect) {
		ImBuf *bbuf = IMB_ibImageFromMemory(iimg->datatoc_rect,
		                                    iimg->datatoc_size, IB_rect, NULL, "<matcap icon>");
		/* w and h were set on initialize */
		if (bbuf->x != iimg->h && bbuf->y != iimg->w)
			IMB_scaleImBuf(bbuf, iimg->w, iimg->h);

		iimg->rect = bbuf->rect;
		bbuf->rect = NULL;
		IMB_freeImBuf(bbuf);
	}
}

static void init_matcap_icons(void)
{
	/* dynamic allocation now, tucking datatoc pointers in DrawInfo */
#define INIT_MATCAP_ICON(icon_id, name)                                       \
	{                                                                         \
		uchar *rect = (uchar *)datatoc_ ##name## _jpg;        \
		int size = datatoc_ ##name## _jpg_size;                               \
		DrawInfo *di;                                                         \
		                                                                      \
		di = def_internal_icon(NULL, icon_id, 0, 0, 96, ICON_TYPE_BUFFER);   \
		di->data.buffer.image->datatoc_rect = rect;                           \
		di->data.buffer.image->datatoc_size = size;                           \
	} (void)0

	INIT_MATCAP_ICON(ICON_MATCAP_01, mc01);
	INIT_MATCAP_ICON(ICON_MATCAP_02, mc02);
	INIT_MATCAP_ICON(ICON_MATCAP_03, mc03);
	INIT_MATCAP_ICON(ICON_MATCAP_04, mc04);
	INIT_MATCAP_ICON(ICON_MATCAP_05, mc05);
	INIT_MATCAP_ICON(ICON_MATCAP_06, mc06);
	INIT_MATCAP_ICON(ICON_MATCAP_07, mc07);
	INIT_MATCAP_ICON(ICON_MATCAP_08, mc08);
	INIT_MATCAP_ICON(ICON_MATCAP_09, mc09);
	INIT_MATCAP_ICON(ICON_MATCAP_10, mc10);
	INIT_MATCAP_ICON(ICON_MATCAP_11, mc11);
	INIT_MATCAP_ICON(ICON_MATCAP_12, mc12);
	INIT_MATCAP_ICON(ICON_MATCAP_13, mc13);
	INIT_MATCAP_ICON(ICON_MATCAP_14, mc14);
	INIT_MATCAP_ICON(ICON_MATCAP_15, mc15);
	INIT_MATCAP_ICON(ICON_MATCAP_16, mc16);
	INIT_MATCAP_ICON(ICON_MATCAP_17, mc17);
	INIT_MATCAP_ICON(ICON_MATCAP_18, mc18);
	INIT_MATCAP_ICON(ICON_MATCAP_19, mc19);
	INIT_MATCAP_ICON(ICON_MATCAP_20, mc20);
	INIT_MATCAP_ICON(ICON_MATCAP_21, mc21);
	INIT_MATCAP_ICON(ICON_MATCAP_22, mc22);
	INIT_MATCAP_ICON(ICON_MATCAP_23, mc23);
	INIT_MATCAP_ICON(ICON_MATCAP_24, mc24);

#undef INIT_MATCAP_ICON

}

static void init_internal_icons(void)
{
//	bTheme *btheme = UI_GetTheme();
	ImBuf *b16buf = NULL, *b32buf = NULL;
	int x, y, icontype;

#if 0 // temp disabled
	if ((btheme != NULL) && btheme->tui.iconfile[0]) {
		char *icondir = BKE_appdir_folder_id(BLENDER_DATAFILES, "icons");
		char iconfilestr[FILE_MAX];

		if (icondir) {
			BLI_join_dirfile(iconfilestr, sizeof(iconfilestr), icondir, btheme->tui.iconfile);
			bbuf = IMB_loadiffname(iconfilestr, IB_rect, NULL); /* if the image is missing bbuf will just be NULL */
			if (bbuf && (bbuf->x < ICON_IMAGE_W || bbuf->y < ICON_IMAGE_H)) {
				printf("\n***WARNING***\nIcons file %s too small.\nUsing built-in Icons instead\n", iconfilestr);
				IMB_freeImBuf(bbuf);
				bbuf = NULL;
			}
		}
		else {
			printf("%s: 'icons' data path not found, continuing\n", __func__);
		}
	}
#endif
	if (b16buf == NULL)
		b16buf = IMB_ibImageFromMemory((uchar *)datatoc_blender_icons16_png,
		                               datatoc_blender_icons16_png_size, IB_rect, NULL, "<blender icons>");
	if (b16buf)
		IMB_premultiply_alpha(b16buf);

	if (b32buf == NULL)
		b32buf = IMB_ibImageFromMemory((uchar *)datatoc_blender_icons32_png,
		                               datatoc_blender_icons32_png_size, IB_rect, NULL, "<blender icons>");
	if (b32buf)
		IMB_premultiply_alpha(b32buf);

	if (b16buf && b32buf) {
		/* free existing texture if any */
		if (icongltex.id) {
			glDeleteTextures(1, &icongltex.id);
			icongltex.id = 0;
		}

		/* we only use a texture for cards with non-power of two */
		if (GPU_full_non_power_of_two_support()) {
			glGenTextures(1, &icongltex.id);

			if (icongltex.id) {
				int level = 2;

				icongltex.w = b32buf->x;
				icongltex.h = b32buf->y;
				icongltex.invw = 1.0f / b32buf->x;
				icongltex.invh = 1.0f / b32buf->y;

				glBindTexture(GL_TEXTURE_2D, icongltex.id);

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, b32buf->x, b32buf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, b32buf->rect);
				glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, b16buf->x, b16buf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, b16buf->rect);

				while (b16buf->x > 1) {
					ImBuf *nbuf = IMB_onehalf(b16buf);
					glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, nbuf->x, nbuf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nbuf->rect);
					level++;
					IMB_freeImBuf(b16buf);
					b16buf = nbuf;
				}

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				glBindTexture(GL_TEXTURE_2D, 0);

				if (glGetError() == GL_OUT_OF_MEMORY) {
					glDeleteTextures(1, &icongltex.id);
					icongltex.id = 0;
				}
			}
		}
	}

	if (icongltex.id)
		icontype = ICON_TYPE_TEXTURE;
	else
		icontype = ICON_TYPE_BUFFER;

	if (b32buf) {
		for (y = 0; y < ICON_GRID_ROWS; y++) {
			for (x = 0; x < ICON_GRID_COLS; x++) {
				def_internal_icon(b32buf, BIFICONID_FIRST + y * ICON_GRID_COLS + x,
				                  x * (ICON_GRID_W + ICON_GRID_MARGIN) + ICON_GRID_MARGIN,
				                  y * (ICON_GRID_H + ICON_GRID_MARGIN) + ICON_GRID_MARGIN, ICON_GRID_W,
				                  icontype);
			}
		}
	}

	def_internal_vicon(VICO_SMALL_TRI_RIGHT_VEC, vicon_small_tri_right_draw);

	def_internal_vicon(VICO_KEYTYPE_KEYFRAME_VEC, vicon_keytype_keyframe_draw);
	def_internal_vicon(VICO_KEYTYPE_BREAKDOWN_VEC, vicon_keytype_breakdown_draw);
	def_internal_vicon(VICO_KEYTYPE_EXTREME_VEC, vicon_keytype_extreme_draw);
	def_internal_vicon(VICO_KEYTYPE_JITTER_VEC, vicon_keytype_jitter_draw);
	def_internal_vicon(VICO_KEYTYPE_MOVING_HOLD_VEC, vicon_keytype_moving_hold_draw);

	def_internal_vicon(VICO_COLORSET_01_VEC, vicon_colorset_draw_01);
	def_internal_vicon(VICO_COLORSET_02_VEC, vicon_colorset_draw_02);
	def_internal_vicon(VICO_COLORSET_03_VEC, vicon_colorset_draw_03);
	def_internal_vicon(VICO_COLORSET_04_VEC, vicon_colorset_draw_04);
	def_internal_vicon(VICO_COLORSET_05_VEC, vicon_colorset_draw_05);
	def_internal_vicon(VICO_COLORSET_06_VEC, vicon_colorset_draw_06);
	def_internal_vicon(VICO_COLORSET_07_VEC, vicon_colorset_draw_07);
	def_internal_vicon(VICO_COLORSET_08_VEC, vicon_colorset_draw_08);
	def_internal_vicon(VICO_COLORSET_09_VEC, vicon_colorset_draw_09);
	def_internal_vicon(VICO_COLORSET_10_VEC, vicon_colorset_draw_10);
	def_internal_vicon(VICO_COLORSET_11_VEC, vicon_colorset_draw_11);
	def_internal_vicon(VICO_COLORSET_12_VEC, vicon_colorset_draw_12);
	def_internal_vicon(VICO_COLORSET_13_VEC, vicon_colorset_draw_13);
	def_internal_vicon(VICO_COLORSET_14_VEC, vicon_colorset_draw_14);
	def_internal_vicon(VICO_COLORSET_15_VEC, vicon_colorset_draw_15);
	def_internal_vicon(VICO_COLORSET_16_VEC, vicon_colorset_draw_16);
	def_internal_vicon(VICO_COLORSET_17_VEC, vicon_colorset_draw_17);
	def_internal_vicon(VICO_COLORSET_18_VEC, vicon_colorset_draw_18);
	def_internal_vicon(VICO_COLORSET_19_VEC, vicon_colorset_draw_19);
	def_internal_vicon(VICO_COLORSET_20_VEC, vicon_colorset_draw_20);

	IMB_freeImBuf(b16buf);
	IMB_freeImBuf(b32buf);

}
#endif  /* WITH_HEADLESS */

static void init_iconfile_list(struct ListBase *list)
{
	IconFile *ifile;
	struct direntry *dir;
	int totfile, i, index = 1;
	const char *icondir;

	BLI_listbase_clear(list);
	icondir = BKE_appdir_folder_id(BLENDER_DATAFILES, "icons");

	if (icondir == NULL)
		return;

	totfile = BLI_filelist_dir_contents(icondir, &dir);

	for (i = 0; i < totfile; i++) {
		if ((dir[i].type & S_IFREG)) {
			const char *filename = dir[i].relname;

			if (BLI_path_extension_check(filename, ".png")) {
				/* loading all icons on file start is overkill & slows startup
				 * its possible they change size after blender load anyway. */
#if 0
				int ifilex, ifiley;
				char iconfilestr[FILE_MAX + 16]; /* allow 256 chars for file+dir */
				ImBuf *bbuf = NULL;
				/* check to see if the image is the right size, continue if not */
				/* copying strings here should go ok, assuming that we never get back
				 * a complete path to file longer than 256 chars */
				BLI_join_dirfile(iconfilestr, sizeof(iconfilestr), icondir, filename);
				bbuf = IMB_loadiffname(iconfilestr, IB_rect);

				if (bbuf) {
					ifilex = bbuf->x;
					ifiley = bbuf->y;
					IMB_freeImBuf(bbuf);
				}
				else {
					ifilex = ifiley = 0;
				}

				/* bad size or failed to load */
				if ((ifilex != ICON_IMAGE_W) || (ifiley != ICON_IMAGE_H)) {
					printf("icon '%s' is wrong size %dx%d\n", iconfilestr, ifilex, ifiley);
					continue;
				}
#endif          /* removed */

				/* found a potential icon file, so make an entry for it in the cache list */
				ifile = MEM_callocN(sizeof(IconFile), "IconFile");

				BLI_strncpy(ifile->filename, filename, sizeof(ifile->filename));
				ifile->index = index;

				BLI_addtail(list, ifile);

				index++;
			}
		}
	}

	BLI_filelist_free(dir, totfile);
	dir = NULL;
}

static void free_iconfile_list(struct ListBase *list)
{
	IconFile *ifile = NULL, *next_ifile = NULL;

	for (ifile = list->first; ifile; ifile = next_ifile) {
		next_ifile = ifile->next;
		BLI_freelinkN(list, ifile);
	}
}

#endif  /* WITH_HEADLESS */

int UI_iconfile_get_index(const char *filename)
{
	IconFile *ifile;
	ListBase *list = &(iconfilelist);

	for (ifile = list->first; ifile; ifile = ifile->next) {
		if (BLI_path_cmp(filename, ifile->filename) == 0) {
			return ifile->index;
		}
	}

	return 0;
}

ListBase *UI_iconfile_list(void)
{
	ListBase *list = &(iconfilelist);

	return list;
}


void UI_icons_free(void)
{
#ifndef WITH_HEADLESS
	if (icongltex.id) {
		glDeleteTextures(1, &icongltex.id);
		icongltex.id = 0;
	}

	free_iconfile_list(&iconfilelist);
	BKE_icons_free();
#endif
}

void UI_icons_free_drawinfo(void *drawinfo)
{
	DrawInfo *di = drawinfo;

	if (di) {
		if (di->type == ICON_TYPE_BUFFER) {
			if (di->data.buffer.image) {
				if (di->data.buffer.image->rect)
					MEM_freeN(di->data.buffer.image->rect);
				MEM_freeN(di->data.buffer.image);
			}
		}

		MEM_freeN(di);
	}
}

static DrawInfo *icon_create_drawinfo(void)
{
	DrawInfo *di = NULL;

	di = MEM_callocN(sizeof(DrawInfo), "di_icon");
	di->type = ICON_TYPE_PREVIEW;

	return di;
}

/* note!, returns unscaled by DPI */
int UI_icon_get_width(int icon_id)
{
	Icon *icon = NULL;
	DrawInfo *di = NULL;

	icon = BKE_icon_get(icon_id);

	if (icon == NULL) {
		if (G.debug & G_DEBUG)
			printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
		return 0;
	}

	di = (DrawInfo *)icon->drawinfo;
	if (!di) {
		di = icon_create_drawinfo();
		icon->drawinfo = di;
	}

	if (di)
		return ICON_DEFAULT_WIDTH;

	return 0;
}

int UI_icon_get_height(int icon_id)
{
	Icon *icon = NULL;
	DrawInfo *di = NULL;

	icon = BKE_icon_get(icon_id);

	if (icon == NULL) {
		if (G.debug & G_DEBUG)
			printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
		return 0;
	}

	di = (DrawInfo *)icon->drawinfo;

	if (!di) {
		di = icon_create_drawinfo();
		icon->drawinfo = di;
	}

	if (di)
		return ICON_DEFAULT_HEIGHT;

	return 0;
}

void UI_icons_init(int first_dyn_id)
{
	BKE_icons_init(first_dyn_id);
#ifndef WITH_HEADLESS
	init_iconfile_list(&iconfilelist);
	init_internal_icons();
	init_brush_icons();
	init_matcap_icons();
#endif
}

/* Render size for preview images and icons
 */
int UI_preview_render_size(enum eIconSizes size)
{
	switch (size) {
		case ICON_SIZE_ICON:
			return ICON_RENDER_DEFAULT_HEIGHT;
		case ICON_SIZE_PREVIEW:
			return PREVIEW_RENDER_DEFAULT_HEIGHT;
		default:
			return 0;
	}
}

/* Create rect for the icon
 */
static void icon_create_rect(struct PreviewImage *prv_img, enum eIconSizes size)
{
	uint render_size = UI_preview_render_size(size);

	if (!prv_img) {
		if (G.debug & G_DEBUG)
			printf("%s, error: requested preview image does not exist", __func__);
	}
	else if (!prv_img->rect[size]) {
		prv_img->w[size] = render_size;
		prv_img->h[size] = render_size;
		prv_img->flag[size] |= PRV_CHANGED;
		prv_img->changed_timestamp[size] = 0;
		prv_img->rect[size] = MEM_callocN(render_size * render_size * sizeof(uint), "prv_rect");
	}
}

static void ui_id_preview_image_render_size(
        const bContext *C, Scene *scene, ID *id, PreviewImage *pi, int size, const bool use_job);

void ui_icon_ensure_deferred(const bContext *C, const int icon_id, const bool big)
{
	Icon *icon = BKE_icon_get(icon_id);

	if (icon) {
		DrawInfo *di = (DrawInfo *)icon->drawinfo;

		if (!di) {
			di = icon_create_drawinfo();

			icon->drawinfo = di;
			icon->drawinfo_free = UI_icons_free_drawinfo;
		}

		if (di) {
			switch (di->type) {
				case ICON_TYPE_PREVIEW:
				{
					ID *id = (icon->id_type != 0) ? icon->obj : NULL;
					PreviewImage *prv = id ? BKE_previewimg_id_ensure(id) : icon->obj;

					if (prv) {
						const int size = big ? ICON_SIZE_PREVIEW : ICON_SIZE_ICON;

						if (id || (prv->tag & PRV_TAG_DEFFERED) != 0) {
							ui_id_preview_image_render_size(C, NULL, id, prv, size, true);
						}
					}
					break;
				}
			}
		}
	}
}

/* only called when icon has changed */
/* only call with valid pointer from UI_icon_draw */
static void icon_set_image(
        const bContext *C, Scene *scene, ID *id, PreviewImage *prv_img, enum eIconSizes size, const bool use_job)
{
	if (!prv_img) {
		if (G.debug & G_DEBUG)
			printf("%s: no preview image for this ID: %s\n", __func__, id->name);
		return;
	}

	if (prv_img->flag[size] & PRV_USER_EDITED) {
		/* user-edited preview, do not auto-update! */
		return;
	}

	icon_create_rect(prv_img, size);

	if (use_job) {
		/* Job (background) version */
		ED_preview_icon_job(C, prv_img, id, prv_img->rect[size], prv_img->w[size], prv_img->h[size]);
	}
	else {
		if (!scene) {
			scene = CTX_data_scene(C);
		}
		/* Immediate version */
		ED_preview_icon_render(CTX_data_main(C), scene, id, prv_img->rect[size], prv_img->w[size], prv_img->h[size]);
	}
}

PreviewImage *UI_icon_to_preview(int icon_id)
{
	Icon *icon = BKE_icon_get(icon_id);

	if (icon) {
		DrawInfo *di = (DrawInfo *)icon->drawinfo;
		if (di) {
			if (di->type == ICON_TYPE_PREVIEW) {
				PreviewImage *prv = (icon->id_type != 0) ? BKE_previewimg_id_ensure((ID *)icon->obj) : icon->obj;

				if (prv) {
					return BKE_previewimg_copy(prv);
				}
			}
			else if (di->data.buffer.image) {
				ImBuf *bbuf;

				bbuf = IMB_ibImageFromMemory(di->data.buffer.image->datatoc_rect, di->data.buffer.image->datatoc_size,
				                             IB_rect, NULL, __func__);
				if (bbuf) {
					PreviewImage *prv = BKE_previewimg_create();

					prv->rect[0] = bbuf->rect;

					prv->w[0] = bbuf->x;
					prv->h[0] = bbuf->y;

					bbuf->rect = NULL;
					IMB_freeImBuf(bbuf);

					return prv;
				}
			}
		}
	}
	return NULL;
}

static void icon_draw_rect(float x, float y, int w, int h, float UNUSED(aspect), int rw, int rh,
                           uint *rect, float alpha, const float rgb[3], const bool is_preview)
{
	ImBuf *ima = NULL;
	int draw_w = w;
	int draw_h = h;
	int draw_x = x;
	int draw_y = y;

	/* sanity check */
	if (w <= 0 || h <= 0 || w > 2000 || h > 2000) {
		printf("%s: icons are %i x %i pixels?\n", __func__, w, h);
		BLI_assert(!"invalid icon size");
		return;
	}

	/* modulate color */
	if (alpha != 1.0f)
		glPixelTransferf(GL_ALPHA_SCALE, alpha);

	if (rgb) {
		glPixelTransferf(GL_RED_SCALE, rgb[0]);
		glPixelTransferf(GL_GREEN_SCALE, rgb[1]);
		glPixelTransferf(GL_BLUE_SCALE, rgb[2]);
	}

	/* rect contains image in 'rendersize', we only scale if needed */
	if (rw != w || rh != h) {
		/* preserve aspect ratio and center */
		if (rw > rh) {
			draw_w = w;
			draw_h = (int)(((float)rh / (float)rw) * (float)w);
			draw_y += (h - draw_h) / 2;
		}
		else if (rw < rh) {
			draw_w = (int)(((float)rw / (float)rh) * (float)h);
			draw_h = h;
			draw_x += (w - draw_w) / 2;
		}
		/* if the image is squared, the draw_ initialization values are good */

		/* first allocate imbuf for scaling and copy preview into it */
		ima = IMB_allocImBuf(rw, rh, 32, IB_rect);
		memcpy(ima->rect, rect, rw * rh * sizeof(uint));
		IMB_scaleImBuf(ima, draw_w, draw_h); /* scale it */
		rect = ima->rect;
	}

	/* draw */
	if (is_preview) {
		glaDrawPixelsSafe(draw_x, draw_y, draw_w, draw_h, draw_w, GL_RGBA, GL_UNSIGNED_BYTE, rect);
	}
	else {
		int bound_options;
		GPU_BASIC_SHADER_DISABLE_AND_STORE(bound_options);

		glRasterPos2f(draw_x, draw_y);
		glDrawPixels(draw_w, draw_h, GL_RGBA, GL_UNSIGNED_BYTE, rect);

		GPU_BASIC_SHADER_ENABLE_AND_RESTORE(bound_options);
	}

	if (ima)
		IMB_freeImBuf(ima);

	/* restore color */
	if (alpha != 0.0f)
		glPixelTransferf(GL_ALPHA_SCALE, 1.0f);

	if (rgb) {
		glPixelTransferf(GL_RED_SCALE, 1.0f);
		glPixelTransferf(GL_GREEN_SCALE, 1.0f);
		glPixelTransferf(GL_BLUE_SCALE, 1.0f);
	}
}

static void icon_draw_texture(
        float x, float y, float w, float h, int ix, int iy,
        int UNUSED(iw), int ih, float alpha, const float rgb[3])
{
	float x1, x2, y1, y2;

	if (rgb) glColor4f(rgb[0], rgb[1], rgb[2], alpha);
	else     glColor4f(alpha, alpha, alpha, alpha);

	x1 = ix * icongltex.invw;
	x2 = (ix + ih) * icongltex.invw;
	y1 = iy * icongltex.invh;
	y2 = (iy + ih) * icongltex.invh;

	GPU_basic_shader_bind(GPU_SHADER_TEXTURE_2D | GPU_SHADER_USE_COLOR);
	glBindTexture(GL_TEXTURE_2D, icongltex.id);

	/* sharper downscaling, has no effect when scale matches with a mip level */
	glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -0.5f);

	glBegin(GL_QUADS);
	glTexCoord2f(x1, y1);
	glVertex2f(x, y);

	glTexCoord2f(x2, y1);
	glVertex2f(x + w, y);

	glTexCoord2f(x2, y2);
	glVertex2f(x + w, y + h);

	glTexCoord2f(x1, y2);
	glVertex2f(x, y + h);
	glEnd();

	glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 0.0f);

	glBindTexture(GL_TEXTURE_2D, 0);
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
}

/* Drawing size for preview images */
static int get_draw_size(enum eIconSizes size)
{
	switch (size) {
		case ICON_SIZE_ICON:
			return ICON_DEFAULT_HEIGHT;
		case ICON_SIZE_PREVIEW:
			return PREVIEW_DEFAULT_HEIGHT;
		default:
			return 0;
	}
}



static void icon_draw_size(
        float x, float y, int icon_id, float aspect, float alpha, const float rgb[3],
        enum eIconSizes size, int draw_size, const bool UNUSED(nocreate), const bool is_preview)
{
	bTheme *btheme = UI_GetTheme();
	Icon *icon = NULL;
	DrawInfo *di = NULL;
	IconImage *iimg;
	const float fdraw_size = (float)draw_size;
	int w, h;

	icon = BKE_icon_get(icon_id);
	alpha *= btheme->tui.icon_alpha;

	if (icon == NULL) {
		if (G.debug & G_DEBUG)
			printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
		return;
	}

	di = (DrawInfo *)icon->drawinfo;

	if (!di) {
		di = icon_create_drawinfo();

		icon->drawinfo = di;
		icon->drawinfo_free = UI_icons_free_drawinfo;
	}

	/* scale width and height according to aspect */
	w = (int)(fdraw_size / aspect + 0.5f);
	h = (int)(fdraw_size / aspect + 0.5f);

	if (di->type == ICON_TYPE_VECTOR) {
		/* vector icons use the uiBlock transformation, they are not drawn
		 * with untransformed coordinates like the other icons */
		di->data.vector.func((int)x, (int)y, w, h, 1.0f);
	}
	else if (di->type == ICON_TYPE_TEXTURE) {
		/* texture image use premul alpha for correct scaling */
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		icon_draw_texture(x, y, (float)w, (float)h, di->data.texture.x, di->data.texture.y,
		                  di->data.texture.w, di->data.texture.h, alpha, rgb);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (di->type == ICON_TYPE_BUFFER) {
		/* it is a builtin icon */
		iimg = di->data.buffer.image;
#ifndef WITH_HEADLESS
		icon_verify_datatoc(iimg);
#endif
		if (!iimg->rect) return;  /* something has gone wrong! */

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		icon_draw_rect(x, y, w, h, aspect, iimg->w, iimg->h, iimg->rect, alpha, rgb, is_preview);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (di->type == ICON_TYPE_PREVIEW) {
		PreviewImage *pi = (icon->id_type != 0) ? BKE_previewimg_id_ensure((ID *)icon->obj) : icon->obj;

		if (pi) {
			/* no create icon on this level in code */
			if (!pi->rect[size]) return;  /* something has gone wrong! */

			/* preview images use premul alpha ... */
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

			icon_draw_rect(x, y, w, h, aspect, pi->w[size], pi->h[size], pi->rect[size], alpha, rgb, is_preview);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}
}

static void ui_id_preview_image_render_size(
        const bContext *C, Scene *scene, ID *id, PreviewImage *pi, int size, const bool use_job)
{
	if (((pi->flag[size] & PRV_CHANGED) || !pi->rect[size])) { /* changed only ever set by dynamic icons */
		/* create the rect if necessary */
		icon_set_image(C, scene, id, pi, size, use_job);

		pi->flag[size] &= ~PRV_CHANGED;
	}
}

void UI_id_icon_render(const bContext *C, Scene *scene, ID *id, const bool big, const bool use_job)
{
	PreviewImage *pi = BKE_previewimg_id_ensure(id);

	if (pi) {
		if (big)
			ui_id_preview_image_render_size(C, scene, id, pi, ICON_SIZE_PREVIEW, use_job);  /* bigger preview size */
		else
			ui_id_preview_image_render_size(C, scene, id, pi, ICON_SIZE_ICON, use_job);     /* icon size */
	}
}

static void ui_id_brush_render(const bContext *C, ID *id)
{
	PreviewImage *pi = BKE_previewimg_id_ensure(id);
	enum eIconSizes i;

	if (!pi)
		return;

	for (i = 0; i < NUM_ICON_SIZES; i++) {
		/* check if rect needs to be created; changed
		 * only set by dynamic icons */
		if (((pi->flag[i] & PRV_CHANGED) || !pi->rect[i])) {
			icon_set_image(C, NULL, id, pi, i, true);
			pi->flag[i] &= ~PRV_CHANGED;
		}
	}
}


static int ui_id_brush_get_icon(const bContext *C, ID *id)
{
	Brush *br = (Brush *)id;

	if (br->flag & BRUSH_CUSTOM_ICON) {
		BKE_icon_id_ensure(id);
		ui_id_brush_render(C, id);
	}
	else {
		Object *ob = CTX_data_active_object(C);
		SpaceImage *sima;
		const EnumPropertyItem *items = NULL;
		int tool = PAINT_TOOL_DRAW, mode = 0;

		/* XXX: this is not nice, should probably make brushes
		 * be strictly in one paint mode only to avoid
		 * checking various context stuff here */

		if (CTX_wm_view3d(C) && ob) {
			if (ob->mode & OB_MODE_SCULPT)
				mode = OB_MODE_SCULPT;
			else if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT))
				mode = OB_MODE_VERTEX_PAINT;
			else if (ob->mode & OB_MODE_TEXTURE_PAINT)
				mode = OB_MODE_TEXTURE_PAINT;
		}
		else if ((sima = CTX_wm_space_image(C)) &&
		         (sima->mode == SI_MODE_PAINT))
		{
			mode = OB_MODE_TEXTURE_PAINT;
		}

		/* reset the icon */
		if (mode == OB_MODE_SCULPT) {
			items = rna_enum_brush_sculpt_tool_items;
			tool = br->sculpt_tool;
		}
		else if (mode == OB_MODE_VERTEX_PAINT) {
			items = rna_enum_brush_vertex_tool_items;
			tool = br->vertexpaint_tool;
		}
		else if (mode == OB_MODE_TEXTURE_PAINT) {
			items = rna_enum_brush_image_tool_items;
			tool = br->imagepaint_tool;
		}

		if (!items || !RNA_enum_icon_from_value(items, tool, &id->icon_id))
			id->icon_id = 0;
	}

	return id->icon_id;
}

int ui_id_icon_get(const bContext *C, ID *id, const bool big)
{
	int iconid = 0;

	/* icon */
	switch (GS(id->name)) {
		case ID_BR:
			iconid = ui_id_brush_get_icon(C, id);
			break;
		case ID_MA: /* fall through */
		case ID_TE: /* fall through */
		case ID_IM: /* fall through */
		case ID_WO: /* fall through */
		case ID_LA: /* fall through */
			iconid = BKE_icon_id_ensure(id);
			/* checks if not exists, or changed */
			UI_id_icon_render(C, NULL, id, big, true);
			break;
		default:
			break;
	}

	return iconid;
}

int UI_rnaptr_icon_get(bContext *C, PointerRNA *ptr, int rnaicon, const bool big)
{
	ID *id = NULL;

	if (!ptr->data)
		return rnaicon;

	/* try ID, material, texture or dynapaint slot */
	if (RNA_struct_is_ID(ptr->type)) {
		id = ptr->id.data;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_MaterialSlot)) {
		id = RNA_pointer_get(ptr, "material").data;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_TextureSlot)) {
		id = RNA_pointer_get(ptr, "texture").data;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_DynamicPaintSurface)) {
		DynamicPaintSurface *surface = (DynamicPaintSurface *)ptr->data;

		if (surface->format == MOD_DPAINT_SURFACE_F_PTEX)
			return ICON_TEXTURE_SHADED;
		else if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX)
			return ICON_OUTLINER_DATA_MESH;
		else if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ)
			return ICON_FILE_IMAGE;
	}

	/* get icon from ID */
	if (id) {
		int icon = ui_id_icon_get(C, id, big);

		return icon ? icon : rnaicon;
	}

	return rnaicon;
}

int UI_idcode_icon_get(const int idcode)
{
	switch (idcode) {
		case ID_AC:
			return ICON_ACTION;
		case ID_AR:
			return ICON_ARMATURE_DATA;
		case ID_BR:
			return ICON_BRUSH_DATA;
		case ID_CA:
			return ICON_CAMERA_DATA;
		case ID_CF:
			return ICON_FILE;
		case ID_CU:
			return ICON_CURVE_DATA;
		case ID_GD:
			return ICON_GREASEPENCIL;
		case ID_GR:
			return ICON_GROUP;
		case ID_IM:
			return ICON_IMAGE_DATA;
		case ID_LA:
			return ICON_LAMP_DATA;
		case ID_LS:
			return ICON_LINE_DATA;
		case ID_LT:
			return ICON_LATTICE_DATA;
		case ID_MA:
			return ICON_MATERIAL_DATA;
		case ID_MB:
			return ICON_META_DATA;
		case ID_MC:
			return ICON_CLIP;
		case ID_ME:
			return ICON_MESH_DATA;
		case ID_MSK:
			return ICON_MOD_MASK;  /* TODO! this would need its own icon! */
		case ID_NT:
			return ICON_NODETREE;
		case ID_OB:
			return ICON_OBJECT_DATA;
		case ID_PA:
			return ICON_PARTICLE_DATA;
		case ID_PAL:
			return ICON_COLOR;  /* TODO! this would need its own icon! */
		case ID_PC:
			return ICON_CURVE_BEZCURVE;  /* TODO! this would need its own icon! */
		case ID_SCE:
			return ICON_SCENE_DATA;
		case ID_SPK:
			return ICON_SPEAKER;
		case ID_SO:
			return ICON_SOUND;
		case ID_TE:
			return ICON_TEXTURE_DATA;
		case ID_TXT:
			return ICON_TEXT;
		case ID_VF:
			return ICON_FONT_DATA;
		case ID_WO:
			return ICON_WORLD_DATA;
		default:
			return ICON_NONE;
	}
}

static void icon_draw_at_size(
        float x, float y, int icon_id, float aspect, float alpha,
        enum eIconSizes size, const bool nocreate)
{
	int draw_size = get_draw_size(size);
	icon_draw_size(x, y, icon_id, aspect, alpha, NULL, size, draw_size, nocreate, false);
}

void UI_icon_draw_aspect(float x, float y, int icon_id, float aspect, float alpha)
{
	icon_draw_at_size(x, y, icon_id, aspect, alpha, ICON_SIZE_ICON, 0);
}

void UI_icon_draw_aspect_color(float x, float y, int icon_id, float aspect, const float rgb[3])
{
	int draw_size = get_draw_size(ICON_SIZE_ICON);
	icon_draw_size(x, y, icon_id, aspect, 1.0f, rgb, ICON_SIZE_ICON, draw_size, false, false);
}

/* draws icon with dpi scale factor */
void UI_icon_draw(float x, float y, int icon_id)
{
	UI_icon_draw_aspect(x, y, icon_id, 1.0f / UI_DPI_FAC, 1.0f);
}

void UI_icon_draw_size(float x, float y, int size, int icon_id, float alpha)
{
	icon_draw_size(x, y, icon_id, 1.0f, alpha, NULL, ICON_SIZE_ICON, size, true, false);
}

void UI_icon_draw_preview(float x, float y, int icon_id)
{
	icon_draw_at_size(x, y, icon_id, 1.0f, 1.0f, ICON_SIZE_PREVIEW, 0);
}

void UI_icon_draw_preview_aspect(float x, float y, int icon_id, float aspect)
{
	icon_draw_at_size(x, y, icon_id, aspect, 1.0f, ICON_SIZE_PREVIEW, 0);
}

void UI_icon_draw_preview_aspect_size(float x, float y, int icon_id, float aspect, float alpha, int size)
{
	icon_draw_size(x, y, icon_id, aspect, alpha, NULL, ICON_SIZE_PREVIEW, size, false, true);
}
