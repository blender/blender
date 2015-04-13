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
 * The Original Code is Copyright (C) 2015 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_stereo.c
 *  \ingroup wm
 */


#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"

#include "GHOST_C-api.h"

#include "ED_screen.h"

#include "GPU_glew.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_draw.h" /* wmDrawTriple */
#include "wm_window.h"

#include "UI_interface.h"
#include "UI_resources.h"

static void wm_method_draw_stereo3d_pageflip(wmWindow *win)
{
	wmDrawData *drawdata;
	int view;

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		if (view == STEREO_LEFT_ID)
			glDrawBuffer(GL_BACK_LEFT);
		else //STEREO_RIGHT_ID
			glDrawBuffer(GL_BACK_RIGHT);

		wm_triple_draw_textures(win, drawdata->triple, 1.0f);
	}

	glDrawBuffer(GL_BACK);
}

static GLuint left_interlace_mask[32];
static GLuint right_interlace_mask[32];
static enum eStereo3dInterlaceType interlace_prev_type = -1;
static char interlace_prev_swap = -1;

static void wm_interlace_masks_create(wmWindow *win)
{
	GLuint pattern;
	char i;
	bool swap = (win->stereo3d_format->flag & S3D_INTERLACE_SWAP) != 0;
	enum eStereo3dInterlaceType interlace_type = win->stereo3d_format->interlace_type;

	if (interlace_prev_type == interlace_type && interlace_prev_swap == swap)
		return;

	switch (interlace_type) {
		case S3D_INTERLACE_ROW:
			pattern = 0x00000000;
			pattern = swap ? ~pattern : pattern;
			for (i = 0; i < 32; i += 2) {
				left_interlace_mask[i] = pattern;
				right_interlace_mask[i] = ~pattern;
			}
			for (i = 1; i < 32; i += 2) {
				left_interlace_mask[i] = ~pattern;
				right_interlace_mask[i] = pattern;
			}
			break;
		case S3D_INTERLACE_COLUMN:
			pattern = 0x55555555;
			pattern = swap ? ~pattern : pattern;
			for (i = 0; i < 32; i++) {
				left_interlace_mask[i] = pattern;
				right_interlace_mask[i] = ~pattern;
			}
			break;
		case S3D_INTERLACE_CHECKERBOARD:
		default:
			pattern = 0x55555555;
			pattern = swap ? ~pattern : pattern;
			for (i = 0; i < 32; i += 2) {
				left_interlace_mask[i] = pattern;
				right_interlace_mask[i] = ~pattern;
			}
			for (i = 1; i < 32; i += 2) {
				left_interlace_mask[i] = ~pattern;
				right_interlace_mask[i] = pattern;
			}
			break;
	}
	interlace_prev_type = interlace_type;
	interlace_prev_swap = swap;
}

static void wm_method_draw_stereo3d_interlace(wmWindow *win)
{
	wmDrawData *drawdata;
	int view;

	wm_interlace_masks_create(win);

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		glEnable(GL_POLYGON_STIPPLE);
		glPolygonStipple(view ? (GLubyte *) right_interlace_mask : (GLubyte *) left_interlace_mask);

		wm_triple_draw_textures(win, drawdata->triple, 1.0f);
		glDisable(GL_POLYGON_STIPPLE);
	}
}

static void wm_method_draw_stereo3d_anaglyph(wmWindow *win)
{
	wmDrawData *drawdata;
	int view, bit;

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		bit = view + 1;
		switch (win->stereo3d_format->anaglyph_type) {
			case S3D_ANAGLYPH_REDCYAN:
				glColorMask((1&bit) ? GL_TRUE : GL_FALSE,
				            (2&bit) ? GL_TRUE : GL_FALSE,
				            (2&bit) ? GL_TRUE : GL_FALSE,
				            GL_FALSE);
				break;
			case S3D_ANAGLYPH_GREENMAGENTA:
				glColorMask((2&bit) ? GL_TRUE : GL_FALSE,
				            (1&bit) ? GL_TRUE : GL_FALSE,
				            (2&bit) ? GL_TRUE : GL_FALSE,
				            GL_FALSE);
				break;
			case S3D_ANAGLYPH_YELLOWBLUE:
				glColorMask((1&bit) ? GL_TRUE : GL_FALSE,
				            (1&bit) ? GL_TRUE : GL_FALSE,
				            (2&bit) ? GL_TRUE : GL_FALSE,
				            GL_FALSE);
				break;
		}

		wm_triple_draw_textures(win, drawdata->triple, 1.0f);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
}

static void wm_method_draw_stereo3d_sidebyside(wmWindow *win)
{
	wmDrawData *drawdata;
	wmDrawTriple *triple;
	float halfx, halfy, ratiox, ratioy;
	int x, y, offx, offy;
	float alpha = 1.0f;
	int view;
	int soffx;
	bool cross_eyed = (win->stereo3d_format->flag & S3D_SIDEBYSIDE_CROSSEYED) != 0;

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);
		triple = drawdata->triple;

		soffx = WM_window_pixels_x(win) * 0.5f;
		if (view == STEREO_LEFT_ID) {
			if (!cross_eyed)
				soffx = 0;
		}
		else { //RIGHT_LEFT_ID
			if (cross_eyed)
				soffx = 0;
		}

		glEnable(triple->target);

		for (y = 0, offy = 0; y < triple->ny; offy += triple->y[y], y++) {
			for (x = 0, offx = 0; x < triple->nx; offx += triple->x[x], x++) {
				const int sizex = triple->x[x];
				const int sizey = triple->y[y];

				/* wmOrtho for the screen has this same offset */
				ratiox = sizex;
				ratioy = sizey;
				halfx = GLA_PIXEL_OFS;
				halfy = GLA_PIXEL_OFS;

				/* texture rectangle has unnormalized coordinates */
				if (triple->target == GL_TEXTURE_2D) {
					ratiox /= triple->x[x];
					ratioy /= triple->y[y];
					halfx /= triple->x[x];
					halfy /= triple->y[y];
				}

				glBindTexture(triple->target, triple->bind[x + y * triple->nx]);

				glColor4f(1.0f, 1.0f, 1.0f, alpha);
				glBegin(GL_QUADS);
				glTexCoord2f(halfx, halfy);
				glVertex2f(soffx + (offx * 0.5f), offy);

				glTexCoord2f(ratiox + halfx, halfy);
				glVertex2f(soffx + ((offx + sizex) * 0.5f), offy);

				glTexCoord2f(ratiox + halfx, ratioy + halfy);
				glVertex2f(soffx + ((offx + sizex) * 0.5f), offy + sizey);

				glTexCoord2f(halfx, ratioy + halfy);
				glVertex2f(soffx + (offx * 0.5f), offy + sizey);
				glEnd();
			}
		}

		glBindTexture(triple->target, 0);
		glDisable(triple->target);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
}

static void wm_method_draw_stereo3d_topbottom(wmWindow *win)
{
	wmDrawData *drawdata;
	wmDrawTriple *triple;
	float halfx, halfy, ratiox, ratioy;
	int x, y, offx, offy;
	float alpha = 1.0f;
	int view;
	int soffy;

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);
		triple = drawdata->triple;

		if (view == STEREO_LEFT_ID) {
			soffy = WM_window_pixels_y(win) * 0.5f;
		}
		else { /* STEREO_RIGHT_ID */
			soffy = 0;
		}

		glEnable(triple->target);

		for (y = 0, offy = 0; y < triple->ny; offy += triple->y[y], y++) {
			for (x = 0, offx = 0; x < triple->nx; offx += triple->x[x], x++) {
				const int sizex = triple->x[x];
				const int sizey = triple->y[y];

				/* wmOrtho for the screen has this same offset */
				ratiox = sizex;
				ratioy = sizey;
				halfx = GLA_PIXEL_OFS;
				halfy = GLA_PIXEL_OFS;

				/* texture rectangle has unnormalized coordinates */
				if (triple->target == GL_TEXTURE_2D) {
					ratiox /= triple->x[x];
					ratioy /= triple->y[y];
					halfx /= triple->x[x];
					halfy /= triple->y[y];
				}

				glBindTexture(triple->target, triple->bind[x + y * triple->nx]);

				glColor4f(1.0f, 1.0f, 1.0f, alpha);
				glBegin(GL_QUADS);
				glTexCoord2f(halfx, halfy);
				glVertex2f(offx, soffy + (offy * 0.5f));

				glTexCoord2f(ratiox + halfx, halfy);
				glVertex2f(offx + sizex, soffy + (offy * 0.5f));

				glTexCoord2f(ratiox + halfx, ratioy + halfy);
				glVertex2f(offx + sizex, soffy + ((offy + sizey) * 0.5f));

				glTexCoord2f(halfx, ratioy + halfy);
				glVertex2f(offx, soffy + ((offy + sizey) * 0.5f));
				glEnd();
			}
		}

		glBindTexture(triple->target, 0);
		glDisable(triple->target);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
}

void wm_method_draw_stereo3d(const bContext *UNUSED(C), wmWindow *win)
{
	switch (win->stereo3d_format->display_mode) {
		case S3D_DISPLAY_ANAGLYPH:
			wm_method_draw_stereo3d_anaglyph(win);
			break;
		case S3D_DISPLAY_INTERLACE:
			wm_method_draw_stereo3d_interlace(win);
			break;
		case S3D_DISPLAY_PAGEFLIP:
			wm_method_draw_stereo3d_pageflip(win);
			break;
		case S3D_DISPLAY_SIDEBYSIDE:
			wm_method_draw_stereo3d_sidebyside(win);
			break;
		case S3D_DISPLAY_TOPBOTTOM:
			wm_method_draw_stereo3d_topbottom(win);
			break;
		default:
			break;
	}
}

static bool wm_stereo3d_quadbuffer_supported(void)
{
	int gl_stereo = 0;
	glGetBooleanv(GL_STEREO, (GLboolean *)&gl_stereo);
	return gl_stereo != 0;
}

static bool wm_stereo3d_is_fullscreen_required(eStereoDisplayMode stereo_display)
{
	return ELEM(stereo_display,
	            S3D_DISPLAY_SIDEBYSIDE,
	            S3D_DISPLAY_TOPBOTTOM,
	            S3D_DISPLAY_PAGEFLIP);
}

bool WM_stereo3d_enabled(wmWindow *win, bool skip_stereo3d_check)
{
	bScreen *screen = win->screen;

	if ((skip_stereo3d_check == false) && (ED_screen_stereo3d_required(screen) == false)) {
		return false;
	}

	if (wm_stereo3d_is_fullscreen_required(win->stereo3d_format->display_mode)) {
		if (GHOST_GetWindowState(win->ghostwin) != GHOST_kWindowStateFullScreen) {
			return false;
		}
	}

	return true;
}

/************************** Stereo 3D operator **********************************/
typedef struct Stereo3dData {
	Stereo3dFormat stereo3d_format;
} Stereo3dData;

static bool wm_stereo3d_set_properties(bContext *UNUSED(C), wmOperator *op)
{
	Stereo3dData *s3dd = op->customdata;
	Stereo3dFormat *s3d = &s3dd->stereo3d_format;
	PropertyRNA *prop;
	bool is_set = false;

	prop = RNA_struct_find_property(op->ptr, "display_mode");
	if (RNA_property_is_set(op->ptr, prop)) {
		s3d->display_mode = RNA_property_enum_get(op->ptr, prop);
		is_set = true;
	}

	prop = RNA_struct_find_property(op->ptr, "anaglyph_type");
	if (RNA_property_is_set(op->ptr, prop)) {
		s3d->anaglyph_type = RNA_property_enum_get(op->ptr, prop);
		is_set = true;
	}

	prop = RNA_struct_find_property(op->ptr, "interlace_type");
	if (RNA_property_is_set(op->ptr, prop)) {
		s3d->interlace_type = RNA_property_enum_get(op->ptr, prop);
		is_set = true;
	}

	prop = RNA_struct_find_property(op->ptr, "use_interlace_swap");
	if (RNA_property_is_set(op->ptr, prop)) {
		if (RNA_property_boolean_get(op->ptr, prop))
			s3d->flag |= S3D_INTERLACE_SWAP;
		else
			s3d->flag &= ~S3D_INTERLACE_SWAP;
		is_set = true;
	}

	prop = RNA_struct_find_property(op->ptr, "use_sidebyside_crosseyed");
	if (RNA_property_is_set(op->ptr, prop)) {
		if (RNA_property_boolean_get(op->ptr, prop))
			s3d->flag |= S3D_SIDEBYSIDE_CROSSEYED;
		else
			s3d->flag &= ~S3D_SIDEBYSIDE_CROSSEYED;
		is_set = true;
	}

	return is_set;
}

static void wm_stereo3d_set_init(bContext *C, wmOperator *op)
{
	Stereo3dData *s3dd;
	wmWindow *win = CTX_wm_window(C);

	op->customdata = s3dd = MEM_callocN(sizeof(Stereo3dData), __func__);

	/* store the original win stereo 3d settings in case of cancel */
	s3dd->stereo3d_format = *win->stereo3d_format;
}

int wm_stereo3d_set_exec(bContext *C, wmOperator *op)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	const bool is_fullscreen = WM_window_is_fullscreen(win);
	char display_mode = win->stereo3d_format->display_mode;

	if (G.background)
		return OPERATOR_CANCELLED;

	if (op->customdata) {
		Stereo3dData *s3dd = op->customdata;
		*win->stereo3d_format = s3dd->stereo3d_format;
	}

	/* pageflip requires a new window to be created with the proper OS flags */
	if (win->stereo3d_format->display_mode == S3D_DISPLAY_PAGEFLIP) {
		if (wm_stereo3d_quadbuffer_supported() == false) {
			BKE_report(op->reports, RPT_ERROR, "Quad-buffer not supported by the system");
			win->stereo3d_format->display_mode = display_mode;
			return OPERATOR_CANCELLED;
		}
		if (wm_window_duplicate_exec(C, op) == OPERATOR_FINISHED) {
			wm_window_close(C, wm, win);
			win = wm->windows.last;
		}
		else {
			BKE_report(op->reports, RPT_ERROR,
			           "Fail to create a window compatible with time sequential (page-flip) display method");
			win->stereo3d_format->display_mode = display_mode;
			return OPERATOR_CANCELLED;
		}
	}

	if (wm_stereo3d_is_fullscreen_required(win->stereo3d_format->display_mode)) {
		if (!is_fullscreen) {
			BKE_report(op->reports, RPT_INFO, "Stereo 3D Mode requires the window to be fullscreen");
		}
	}

	if (op->customdata) {
		MEM_freeN(op->customdata);
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;
}

int wm_stereo3d_set_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	wm_stereo3d_set_init(C, op);

	if (wm_stereo3d_set_properties(C, op))
		return wm_stereo3d_set_exec(C, op);
	else
		return WM_operator_props_dialog_popup(C, op, 250, 100);
}

void wm_stereo3d_set_draw(bContext *UNUSED(C), wmOperator *op)
{
	Stereo3dData *s3dd = op->customdata;
	PointerRNA stereo3d_format_ptr;
	uiLayout *layout = op->layout;
	uiLayout *col;

	RNA_pointer_create(NULL, &RNA_Stereo3dDisplay, &s3dd->stereo3d_format, &stereo3d_format_ptr);

	col = uiLayoutColumn(layout, false);
	uiItemR(col, &stereo3d_format_ptr, "display_mode", 0, NULL, ICON_NONE);

	switch (s3dd->stereo3d_format.display_mode) {
		case S3D_DISPLAY_ANAGLYPH:
		{
			uiItemR(col, &stereo3d_format_ptr, "anaglyph_type", 0, NULL, ICON_NONE);
			break;
		}
		case S3D_DISPLAY_INTERLACE:
		{
			uiItemR(col, &stereo3d_format_ptr, "interlace_type", 0, NULL, ICON_NONE);
			uiItemR(col, &stereo3d_format_ptr, "use_interlace_swap", 0, NULL, ICON_NONE);
			break;
		}
		case S3D_DISPLAY_SIDEBYSIDE:
		{
			uiItemR(col, &stereo3d_format_ptr, "use_sidebyside_crosseyed", 0, NULL, ICON_NONE);
			/* fall-through */
		}
		case S3D_DISPLAY_PAGEFLIP:
		case S3D_DISPLAY_TOPBOTTOM:
		default:
		{
			break;
		}
	}
}

bool wm_stereo3d_set_check(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
	/* the check function guarantees that the menu is updated to show the
	 * sub-options when an enum change (e.g., it shows the anaglyph options
	 * when anaglyph is on, and the interlace options when this is on */
	return true;
}

void wm_stereo3d_set_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata = NULL;
}
