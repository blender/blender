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

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"

#include "DNA_brush_types.h"
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

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_datafiles.h"
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
	unsigned int *rect;
	unsigned char *datatoc_rect;
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
	new_icon->type = 0;

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
			
			iimg->rect = MEM_mallocN(size * size * sizeof(unsigned int), "icon_rect");
			
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
	new_icon->type = 0;

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

static void viconutil_draw_tri(GLint(*pts)[2])
{
	glBegin(GL_TRIANGLES);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glVertex2iv(pts[2]);
	glEnd();
}

static void viconutil_draw_lineloop(GLint(*pts)[2], int numPoints)
{
	int i;

	glBegin(GL_LINE_LOOP);
	for (i = 0; i < numPoints; i++) {
		glVertex2iv(pts[i]);
	}
	glEnd();
}

static void viconutil_draw_lineloop_smooth(GLint(*pts)[2], int numPoints)
{
	glEnable(GL_LINE_SMOOTH);
	viconutil_draw_lineloop(pts, numPoints);
	glDisable(GL_LINE_SMOOTH);
}

static void viconutil_draw_points(GLint(*pts)[2], int numPoints, int pointSize)
{
	int i;

	glBegin(GL_QUADS);
	for (i = 0; i < numPoints; i++) {
		int x = pts[i][0], y = pts[i][1];

		glVertex2i(x - pointSize, y - pointSize);
		glVertex2i(x + pointSize, y - pointSize);
		glVertex2i(x + pointSize, y + pointSize);
		glVertex2i(x - pointSize, y + pointSize);
	}
	glEnd();
}

/* Drawing functions */

static void vicon_x_draw(int x, int y, int w, int h, float alpha)
{
	x += 3;
	y += 3;
	w -= 6;
	h -= 6;

	glEnable(GL_LINE_SMOOTH);

	glLineWidth(2.5);
	
	glColor4f(0.0, 0.0, 0.0, alpha);
	glBegin(GL_LINES);
	glVertex2i(x, y);
	glVertex2i(x + w, y + h);
	glVertex2i(x + w, y);
	glVertex2i(x, y + h);
	glEnd();

	glLineWidth(1.0);
	
	glDisable(GL_LINE_SMOOTH);
}

static void vicon_view3d_draw(int x, int y, int w, int h, float alpha)
{
	int cx = x + w / 2;
	int cy = y + h / 2;
	int d = MAX2(2, h / 3);

	glColor4f(0.5, 0.5, 0.5, alpha);
	glBegin(GL_LINES);
	glVertex2i(x, cy - d);
	glVertex2i(x + w, cy - d);
	glVertex2i(x, cy + d);
	glVertex2i(x + w, cy + d);

	glVertex2i(cx - d, y);
	glVertex2i(cx - d, y + h);
	glVertex2i(cx + d, y);
	glVertex2i(cx + d, y + h);
	glEnd();
	
	glColor4f(0.0, 0.0, 0.0, alpha);
	glBegin(GL_LINES);
	glVertex2i(x, cy);
	glVertex2i(x + w, cy);
	glVertex2i(cx, y);
	glVertex2i(cx, y + h);
	glEnd();
}

static void vicon_edit_draw(int x, int y, int w, int h, float alpha)
{
	GLint pts[4][2];

	viconutil_set_point(pts[0], x + 3,     y + 3);
	viconutil_set_point(pts[1], x + w - 3, y + 3);
	viconutil_set_point(pts[2], x + w - 3, y + h - 3);
	viconutil_set_point(pts[3], x + 3,     y + h - 3);

	glColor4f(0.0, 0.0, 0.0, alpha);
	viconutil_draw_lineloop(pts, 4);

	glColor3f(1, 1, 0.0);
	viconutil_draw_points(pts, 4, 1);
}

static void vicon_editmode_hlt_draw(int x, int y, int w, int h, float alpha)
{
	GLint pts[3][2];

	viconutil_set_point(pts[0], x + w / 2, y + h - 2);
	viconutil_set_point(pts[1], x + 3,     y + 4);
	viconutil_set_point(pts[2], x + w - 3, y + 4);

	glColor4f(0.5, 0.5, 0.5, alpha);
	viconutil_draw_tri(pts);

	glColor4f(0.0, 0.0, 0.0, 1);
	viconutil_draw_lineloop_smooth(pts, 3);

	glColor3f(1, 1, 0.0);
	viconutil_draw_points(pts, 3, 1);
}

static void vicon_editmode_dehlt_draw(int x, int y, int w, int h, float UNUSED(alpha))
{
	GLint pts[3][2];

	viconutil_set_point(pts[0], x + w / 2, y + h - 2);
	viconutil_set_point(pts[1], x + 3,     y + 4);
	viconutil_set_point(pts[2], x + w - 3, y + 4);

	glColor4f(0.0f, 0.0f, 0.0f, 1);
	viconutil_draw_lineloop_smooth(pts, 3);

	glColor3f(0.9f, 0.9f, 0.9f);
	viconutil_draw_points(pts, 3, 1);
}

static void vicon_disclosure_tri_right_draw(int x, int y, int w, int UNUSED(h), float alpha)
{
	GLint pts[3][2];
	int cx = x + w / 2;
	int cy = y + w / 2;
	int d = w / 3, d2 = w / 5;

	viconutil_set_point(pts[0], cx - d2, cy + d);
	viconutil_set_point(pts[1], cx - d2, cy - d);
	viconutil_set_point(pts[2], cx + d2, cy);

	glShadeModel(GL_SMOOTH);
	glBegin(GL_TRIANGLES);
	glColor4f(0.8f, 0.8f, 0.8f, alpha);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glColor4f(0.3f, 0.3f, 0.3f, alpha);
	glVertex2iv(pts[2]);
	glEnd();
	glShadeModel(GL_FLAT);

	glColor4f(0.0f, 0.0f, 0.0f, 1);
	viconutil_draw_lineloop_smooth(pts, 3);
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

	glShadeModel(GL_SMOOTH);
	glBegin(GL_TRIANGLES);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glVertex2iv(pts[2]);
	glEnd();
	glShadeModel(GL_FLAT);
}

static void vicon_disclosure_tri_down_draw(int x, int y, int w, int UNUSED(h), float alpha)
{
	GLint pts[3][2];
	int cx = x + w / 2;
	int cy = y + w / 2;
	int d = w / 3, d2 = w / 5;

	viconutil_set_point(pts[0], cx + d, cy + d2);
	viconutil_set_point(pts[1], cx - d, cy + d2);
	viconutil_set_point(pts[2], cx, cy - d2);

	glShadeModel(GL_SMOOTH);
	glBegin(GL_TRIANGLES);
	glColor4f(0.8f, 0.8f, 0.8f, alpha);
	glVertex2iv(pts[0]);
	glVertex2iv(pts[1]);
	glColor4f(0.3f, 0.3f, 0.3f, alpha);
	glVertex2iv(pts[2]);
	glEnd();
	glShadeModel(GL_FLAT);

	glColor4f(0.0f, 0.0f, 0.0f, 1);
	viconutil_draw_lineloop_smooth(pts, 3);
}

static void vicon_move_up_draw(int x, int y, int w, int h, float UNUSED(alpha))
{
	int d = -2;

	glEnable(GL_LINE_SMOOTH);
	glLineWidth(1);
	glColor3f(0.0, 0.0, 0.0);

	glBegin(GL_LINE_STRIP);
	glVertex2i(x + w / 2 - d * 2, y + h / 2 + d);
	glVertex2i(x + w / 2, y + h / 2 - d + 1);
	glVertex2i(x + w / 2 + d * 2, y + h / 2 + d);
	glEnd();

	glLineWidth(1.0);
	glDisable(GL_LINE_SMOOTH);
}

static void vicon_move_down_draw(int x, int y, int w, int h, float UNUSED(alpha))
{
	int d = 2;

	glEnable(GL_LINE_SMOOTH);
	glLineWidth(1);
	glColor3f(0.0, 0.0, 0.0);

	glBegin(GL_LINE_STRIP);
	glVertex2i(x + w / 2 - d * 2, y + h / 2 + d);
	glVertex2i(x + w / 2, y + h / 2 - d - 1);
	glVertex2i(x + w / 2 + d * 2, y + h / 2 + d);
	glEnd();

	glLineWidth(1.0);
	glDisable(GL_LINE_SMOOTH);
}

#ifndef WITH_HEADLESS

static void init_brush_icons(void)
{

#define INIT_BRUSH_ICON(icon_id, name)                                          \
	{                                                                           \
		unsigned char *rect = (unsigned char *)datatoc_ ##name## _png;          \
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
		unsigned char *rect = (unsigned char *)datatoc_ ##name## _jpg;        \
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
		b16buf = IMB_ibImageFromMemory((unsigned char *)datatoc_blender_icons16_png,
		                               datatoc_blender_icons16_png_size, IB_rect, NULL, "<blender icons>");
	if (b16buf)
		IMB_premultiply_alpha(b16buf);

	if (b32buf == NULL)
		b32buf = IMB_ibImageFromMemory((unsigned char *)datatoc_blender_icons32_png,
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
		if (GPU_non_power_of_two_support()) {
			glGenTextures(1, &icongltex.id);

			if (icongltex.id) {
				int level = 2;
				
				icongltex.w = b32buf->x;
				icongltex.h = b32buf->y;
				icongltex.invw = 1.0f / b32buf->x;
				icongltex.invh = 1.0f / b32buf->y;

				glBindTexture(GL_TEXTURE_2D, icongltex.id);
				
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, b32buf->x, b32buf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, b32buf->rect);
				glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, b16buf->x, b16buf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, b16buf->rect);
				
				while (b16buf->x > 1) {
					ImBuf *nbuf = IMB_onehalf(b16buf);
					glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, nbuf->x, nbuf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nbuf->rect);
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

	def_internal_vicon(VICO_VIEW3D_VEC, vicon_view3d_draw);
	def_internal_vicon(VICO_EDIT_VEC, vicon_edit_draw);
	def_internal_vicon(VICO_EDITMODE_VEC_DEHLT, vicon_editmode_dehlt_draw);
	def_internal_vicon(VICO_EDITMODE_VEC_HLT, vicon_editmode_hlt_draw);
	def_internal_vicon(VICO_DISCLOSURE_TRI_RIGHT_VEC, vicon_disclosure_tri_right_draw);
	def_internal_vicon(VICO_DISCLOSURE_TRI_DOWN_VEC, vicon_disclosure_tri_down_draw);
	def_internal_vicon(VICO_MOVE_UP_VEC, vicon_move_up_draw);
	def_internal_vicon(VICO_MOVE_DOWN_VEC, vicon_move_down_draw);
	def_internal_vicon(VICO_X_VEC, vicon_x_draw);
	def_internal_vicon(VICO_SMALL_TRI_RIGHT_VEC, vicon_small_tri_right_draw);

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
			
			if (BLI_testextensie(filename, ".png")) {
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

	BLI_filelist_free(dir, totfile, NULL);
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
#ifdef WITH_HEADLESS
	(void)first_dyn_id;
#else
	init_iconfile_list(&iconfilelist);
	BKE_icons_init(first_dyn_id);
	init_internal_icons();
	init_brush_icons();
	init_matcap_icons();
#endif
}

/* Render size for preview images and icons
 */
static int preview_render_size(enum eIconSizes size)
{
	switch (size) {
		case ICON_SIZE_ICON:    return 32;
		case ICON_SIZE_PREVIEW: return PREVIEW_DEFAULT_HEIGHT;
	}
	return 0;
}

/* Create rect for the icon
 */
static void icon_create_rect(struct PreviewImage *prv_img, enum eIconSizes size)
{
	unsigned int render_size = preview_render_size(size);

	if (!prv_img) {
		if (G.debug & G_DEBUG)
			printf("%s, error: requested preview image does not exist", __func__);
	}
	else if (!prv_img->rect[size]) {
		prv_img->w[size] = render_size;
		prv_img->h[size] = render_size;
		prv_img->changed[size] = 1;
		prv_img->changed_timestamp[size] = 0;
		prv_img->rect[size] = MEM_callocN(render_size * render_size * sizeof(unsigned int), "prv_rect");
	}
}

/* only called when icon has changed */
/* only call with valid pointer from UI_icon_draw */
static void icon_set_image(const bContext *C, ID *id, PreviewImage *prv_img, enum eIconSizes size, const bool use_job)
{
	if (!prv_img) {
		if (G.debug & G_DEBUG)
			printf("%s: no preview image for this ID: %s\n", __func__, id->name);
		return;
	}

	icon_create_rect(prv_img, size);

	if (use_job) {
		/* Job (background) version */
		ED_preview_icon_job(C, prv_img, id, prv_img->rect[size], prv_img->w[size], prv_img->h[size]);
	}
	else {
		/* Immediate version */
		ED_preview_icon_render(C, prv_img, id, prv_img->rect[size], prv_img->w[size], prv_img->h[size]);
	}
}

PreviewImage *UI_icon_to_preview(int icon_id)
{
	Icon *icon = BKE_icon_get(icon_id);
	
	if (icon) {
		DrawInfo *di = (DrawInfo *)icon->drawinfo;
		if (di && di->data.buffer.image) {
			ImBuf *bbuf;
			
			bbuf = IMB_ibImageFromMemory(di->data.buffer.image->datatoc_rect, di->data.buffer.image->datatoc_size, IB_rect, NULL, "<matcap buffer>");
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
	return NULL;
}

static void icon_draw_rect(float x, float y, int w, int h, float UNUSED(aspect), int rw, int rh,
                           unsigned int *rect, float alpha, const float rgb[3], const bool is_preview)
{
	ImBuf *ima = NULL;

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
	if (rw != w && rh != h) {
		/* first allocate imbuf for scaling and copy preview into it */
		ima = IMB_allocImBuf(rw, rh, 32, IB_rect);
		memcpy(ima->rect, rect, rw * rh * sizeof(unsigned int));
		IMB_scaleImBuf(ima, w, h); /* scale it */
		rect = ima->rect;
	}

	/* draw */
	if (is_preview) {
		glaDrawPixelsSafe(x, y, w, h, w, GL_RGBA, GL_UNSIGNED_BYTE, rect);
	}
	else {
		glRasterPos2f(x, y);
		glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, rect);
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

static void icon_draw_texture(float x, float y, float w, float h, int ix, int iy,
                              int UNUSED(iw), int ih, float alpha, const float rgb[3])
{
	float x1, x2, y1, y2;

	if (rgb) glColor4f(rgb[0], rgb[1], rgb[2], alpha);
	else     glColor4f(alpha, alpha, alpha, alpha);

	x1 = ix * icongltex.invw;
	x2 = (ix + ih) * icongltex.invw;
	y1 = iy * icongltex.invh;
	y2 = (iy + ih) * icongltex.invh;

	glEnable(GL_TEXTURE_2D);
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
	glDisable(GL_TEXTURE_2D);
}

/* Drawing size for preview images */
static int get_draw_size(enum eIconSizes size)
{
	switch (size) {
		case ICON_SIZE_ICON: return ICON_DEFAULT_HEIGHT;
		case ICON_SIZE_PREVIEW: return PREVIEW_DEFAULT_HEIGHT;
	}
	return 0;
}



static void icon_draw_size(float x, float y, int icon_id, float aspect, float alpha, const float rgb[3],
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
		PreviewImage *pi = BKE_previewimg_get((ID *)icon->obj);

		if (pi) {
			/* no create icon on this level in code */
			if (!pi->rect[size]) return;  /* something has gone wrong! */
			
			/* preview images use premul alpha ... */
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			icon_draw_rect(x, y, w, h, aspect, pi->w[size], pi->h[size], pi->rect[size], 1.0f, NULL, is_preview);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}
}

static void ui_id_preview_image_render_size(const bContext *C, ID *id, PreviewImage *pi, int size, const bool use_job)
{
	if ((pi->changed[size] || !pi->rect[size])) { /* changed only ever set by dynamic icons */
		/* create the rect if necessary */
		icon_set_image(C, id, pi, size, use_job);

		pi->changed[size] = 0;
	}
}

void UI_id_icon_render(const bContext *C, ID *id, const bool big, const bool use_job)
{
	PreviewImage *pi = BKE_previewimg_get(id);

	if (pi) {
		if (big)
			ui_id_preview_image_render_size(C, id, pi, ICON_SIZE_PREVIEW, use_job);  /* bigger preview size */
		else
			ui_id_preview_image_render_size(C, id, pi, ICON_SIZE_ICON, use_job);     /* icon size */
	}
}

static void ui_id_brush_render(const bContext *C, ID *id)
{
	PreviewImage *pi = BKE_previewimg_get(id); 
	enum eIconSizes i;
	
	if (!pi)
		return;
	
	for (i = 0; i < NUM_ICON_SIZES; i++) {
		/* check if rect needs to be created; changed
		 * only set by dynamic icons */
		if ((pi->changed[i] || !pi->rect[i])) {
			icon_set_image(C, id, pi, i, false);
			pi->changed[i] = 0;
		}
	}
}


static int ui_id_brush_get_icon(const bContext *C, ID *id)
{
	Brush *br = (Brush *)id;

	if (br->flag & BRUSH_CUSTOM_ICON) {
		BKE_icon_getid(id);
		ui_id_brush_render(C, id);
	}
	else {
		Object *ob = CTX_data_active_object(C);
		SpaceImage *sima;
		EnumPropertyItem *items = NULL;
		int tool, mode = 0;

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
			items = brush_sculpt_tool_items;
			tool = br->sculpt_tool;
		}
		else if (mode == OB_MODE_VERTEX_PAINT) {
			items = brush_vertex_tool_items;
			tool = br->vertexpaint_tool;
		}
		else if (mode == OB_MODE_TEXTURE_PAINT) {
			items = brush_image_tool_items;
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
			iconid = BKE_icon_getid(id);
			/* checks if not exists, or changed */
			UI_id_icon_render(C, id, big, true);
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

static void icon_draw_at_size(float x, float y, int icon_id, float aspect, float alpha,
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

void UI_icon_draw_preview_aspect_size(float x, float y, int icon_id, float aspect, int size)
{
	icon_draw_size(x, y, icon_id, aspect, 1.0f, NULL, ICON_SIZE_PREVIEW, size, false, true);
}

