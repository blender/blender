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
#include "BLI_utildefines.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"

#include "GHOST_C-api.h"

#include "ED_screen.h"

#include "GPU_glew.h"
#include "GPU_basic_shader.h"

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

		wm_triple_draw_textures(win, drawdata->triple, 1.0f, false);
	}

	glDrawBuffer(GL_BACK);
}

static enum eStereo3dInterlaceType interlace_prev_type = -1;
static char interlace_prev_swap = -1;

static void wm_method_draw_stereo3d_interlace(wmWindow *win)
{
	wmDrawData *drawdata;
	int view;
	bool flag;
	bool swap = (win->stereo3d_format->flag & S3D_INTERLACE_SWAP) != 0;
	enum eStereo3dInterlaceType interlace_type = win->stereo3d_format->interlace_type;

	for (view = 0; view < 2; view ++) {
		flag = swap ? !view : view;
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);
		GPU_basic_shader_bind(GPU_SHADER_STIPPLE);
		switch (interlace_type) {
			case S3D_INTERLACE_ROW:
				if (flag)
					GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_S3D_INTERLACE_ROW_SWAP);
				else
					GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_S3D_INTERLACE_ROW);
				break;
			case S3D_INTERLACE_COLUMN:
				if (flag)
					GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_S3D_INTERLACE_COLUMN_SWAP);
				else
					GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_S3D_INTERLACE_COLUMN);
				break;
			case S3D_INTERLACE_CHECKERBOARD:
			default:
				if (flag)
					GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_S3D_INTERLACE_CHECKER_SWAP);
				else
					GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_S3D_INTERLACE_CHECKER);
				break;
		}

		wm_triple_draw_textures(win, drawdata->triple, 1.0f, true);
		GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
	}
	interlace_prev_type = interlace_type;
	interlace_prev_swap = swap;
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

		wm_triple_draw_textures(win, drawdata->triple, 1.0f, false);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
}

static void wm_method_draw_stereo3d_sidebyside(wmWindow *win)
{
	wmDrawData *drawdata;
	wmDrawTriple *triple;
	float halfx, halfy, ratiox, ratioy;
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

		const int sizex = triple->x;
		const int sizey = triple->y;

		/* wmOrtho for the screen has this same offset */
		ratiox = sizex;
		ratioy = sizey;
		halfx = GLA_PIXEL_OFS;
		halfy = GLA_PIXEL_OFS;

		/* texture rectangle has unnormalized coordinates */
		if (triple->target == GL_TEXTURE_2D) {
			ratiox /= triple->x;
			ratioy /= triple->y;
			halfx /= triple->x;
			halfy /= triple->y;
		}

		glEnable(triple->target);
		glBindTexture(triple->target, triple->bind);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(halfx, halfy);
		glVertex2f(soffx, 0);

		glTexCoord2f(ratiox + halfx, halfy);
		glVertex2f(soffx + (sizex * 0.5f), 0);

		glTexCoord2f(ratiox + halfx, ratioy + halfy);
		glVertex2f(soffx + (sizex * 0.5f), sizey);

		glTexCoord2f(halfx, ratioy + halfy);
		glVertex2f(soffx, sizey);
		glEnd();

		glBindTexture(triple->target, 0);
		glDisable(triple->target);
	}
}

static void wm_method_draw_stereo3d_topbottom(wmWindow *win)
{
	wmDrawData *drawdata;
	wmDrawTriple *triple;
	float halfx, halfy, ratiox, ratioy;
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

		const int sizex = triple->x;
		const int sizey = triple->y;

		/* wmOrtho for the screen has this same offset */
		ratiox = sizex;
		ratioy = sizey;
		halfx = GLA_PIXEL_OFS;
		halfy = GLA_PIXEL_OFS;

		/* texture rectangle has unnormalized coordinates */
		if (triple->target == GL_TEXTURE_2D) {
			ratiox /= triple->x;
			ratioy /= triple->y;
			halfx /= triple->x;
			halfy /= triple->y;
		}

		glEnable(triple->target);
		glBindTexture(triple->target, triple->bind);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(halfx, halfy);
		glVertex2f(0, soffy);

		glTexCoord2f(ratiox + halfx, halfy);
		glVertex2f(sizex, soffy);

		glTexCoord2f(ratiox + halfx, ratioy + halfy);
		glVertex2f(sizex, soffy + (sizey * 0.5f));

		glTexCoord2f(halfx, ratioy + halfy);
		glVertex2f(0, soffy + (sizey * 0.5f));
		glEnd();

		glBindTexture(triple->target, 0);
		glDisable(triple->target);
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
	            S3D_DISPLAY_TOPBOTTOM);
}

bool WM_stereo3d_enabled(wmWindow *win, bool skip_stereo3d_check)
{
	bScreen *screen = win->screen;

	/* some 3d methods change the window arrangement, thus they shouldn't
	 * toggle on/off just because there is no 3d elements being drawn */
	if (wm_stereo3d_is_fullscreen_required(win->stereo3d_format->display_mode)) {
		return GHOST_GetWindowState(win->ghostwin) == GHOST_kWindowStateFullScreen;
	}

	if ((skip_stereo3d_check == false) && (ED_screen_stereo3d_required(screen) == false)) {
		return false;
	}

	/* some 3d methods change the window arrangement, thus they shouldn't
	 * toggle on/off just because there is no 3d elements being drawn */
	if (wm_stereo3d_is_fullscreen_required(win->stereo3d_format->display_mode)) {
		return GHOST_GetWindowState(win->ghostwin) == GHOST_kWindowStateFullScreen;
	}

	return true;
}

/**
 * If needed, this adjusts \a r_mouse_xy so that drawn cursor and handled mouse position are matching visually.
 */
void wm_stereo3d_mouse_offset_apply(wmWindow *win, int *r_mouse_xy)
{
	if (!WM_stereo3d_enabled(win, false))
		return;

	if (win->stereo3d_format->display_mode == S3D_DISPLAY_SIDEBYSIDE) {
		const int half_x = win->sizex / 2;
		/* right half of the screen */
		if (r_mouse_xy[0] > half_x) {
			r_mouse_xy[0] -= half_x;
		}
		r_mouse_xy[0] *= 2;
	}
	else if (win->stereo3d_format->display_mode == S3D_DISPLAY_TOPBOTTOM) {
		const int half_y = win->sizey / 2;
		/* upper half of the screen */
		if (r_mouse_xy[1] > half_y) {
			r_mouse_xy[1] -= half_y;
		}
		r_mouse_xy[1] *= 2;
	}
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
	wmWindow *win_src = CTX_wm_window(C);
	wmWindow *win_dst = NULL;
	const bool is_fullscreen = WM_window_is_fullscreen(win_src);
	char prev_display_mode = win_src->stereo3d_format->display_mode;
	Stereo3dData *s3dd;
	bool ok = true;

	if (G.background)
		return OPERATOR_CANCELLED;

	if (op->customdata == NULL) {
		/* no invoke means we need to set the operator properties here */
		wm_stereo3d_set_init(C, op);
		wm_stereo3d_set_properties(C, op);
	}

	s3dd = op->customdata;
	*win_src->stereo3d_format = s3dd->stereo3d_format;

	if (prev_display_mode == S3D_DISPLAY_PAGEFLIP &&
	    prev_display_mode != win_src->stereo3d_format->display_mode)
	{
		/* in case the hardward supports pageflip but not the display */
		if ((win_dst = wm_window_copy_test(C, win_src))) {
			/* pass */
		}
		else {
			BKE_report(op->reports, RPT_ERROR,
			           "Failed to create a window without quad-buffer support, you may experience flickering");
			ok = false;
		}
	}
	else if (win_src->stereo3d_format->display_mode == S3D_DISPLAY_PAGEFLIP) {
		/* ED_screen_duplicate() can't handle other cases yet T44688 */
		if (win_src->screen->state != SCREENNORMAL) {
			BKE_report(op->reports, RPT_ERROR,
			           "Failed to switch to Time Sequential mode when in fullscreen");
			ok = false;
		}
		/* pageflip requires a new window to be created with the proper OS flags */
		else if ((win_dst = wm_window_copy_test(C, win_src))) {
			if (wm_stereo3d_quadbuffer_supported()) {
				BKE_report(op->reports, RPT_INFO, "Quad-buffer window successfully created");
			}
			else {
				wm_window_close(C, wm, win_dst);
				win_dst = NULL;
				BKE_report(op->reports, RPT_ERROR, "Quad-buffer not supported by the system");
				ok = false;
			}
		}
		else {
			BKE_report(op->reports, RPT_ERROR,
			           "Failed to create a window compatible with the time sequential display method");
			ok = false;
		}
	}

	if (wm_stereo3d_is_fullscreen_required(s3dd->stereo3d_format.display_mode)) {
		if (!is_fullscreen) {
			BKE_report(op->reports, RPT_INFO, "Stereo 3D Mode requires the window to be fullscreen");
		}
	}

	MEM_freeN(op->customdata);

	if (ok) {
		if (win_dst) {
			wm_window_close(C, wm, win_src);
		}

		WM_event_add_notifier(C, NC_WINDOW, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		/* without this, the popup won't be freed freed properly T44688 */
		CTX_wm_window_set(C, win_src);
		win_src->stereo3d_format->display_mode = prev_display_mode;
		return OPERATOR_CANCELLED;
	}
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
