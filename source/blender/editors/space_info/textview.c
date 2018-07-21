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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_info/textview.c
 *  \ingroup spinfo
 */


#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "BLF_api.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string_utf8.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "BIF_gl.h"

#include "BKE_text.h"

#include "textview.h"

static void console_font_begin(const int font_id, const int lheight)
{
	/* 0.875 is based on: 16 pixels lines get 14 pixel text */
	BLF_size(font_id, 0.875 * lheight, 72);
}

typedef struct ConsoleDrawContext {
	int font_id;
	int cwidth;
	int lheight;
	int lofs; /* text vertical offset */
	int console_width; /* number of characters that fit into the width of the console (fixed width) */
	int winx;
	int ymin, ymax;
	int *xy; // [2]
	int *sel; // [2]
	int *pos_pick; // bottom of view == 0, top of file == combine chars, end of line is lower then start.
	const int *mval; // [2]
	int draw;
} ConsoleDrawContext;

BLI_INLINE void console_step_sel(ConsoleDrawContext *cdc, const int step)
{
	cdc->sel[0] += step;
	cdc->sel[1] += step;
}

static void console_draw_sel(const char *str, const int sel[2], const int xy[2], const int str_len_draw,
                             int cwidth, int lheight, const unsigned char bg_sel[4])
{
	if (sel[0] <= str_len_draw && sel[1] >= 0) {
		const int sta = txt_utf8_offset_to_column(str, max_ii(sel[0], 0));
		const int end = txt_utf8_offset_to_column(str, min_ii(sel[1], str_len_draw));

		GPU_blend(true);
		GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

		GPUVertFormat *format = immVertexFormat();
		uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		immUniformColor4ubv(bg_sel);
		immRecti(pos, xy[0] + (cwidth * sta), xy[1] - 2 + lheight, xy[0] + (cwidth * end), xy[1] - 2);

		immUnbindProgram();

		GPU_blend(false);
	}
}

/* warning: allocated memory for 'offsets' must be freed by caller */
static int console_wrap_offsets(const char *str, int len, int width, int *lines, int **offsets)
{
	int i, end;  /* column */
	int j;       /* mem */

	*lines = 1;

	*offsets = MEM_callocN(sizeof(**offsets) * (len * BLI_UTF8_WIDTH_MAX / MAX2(1, width - (BLI_UTF8_WIDTH_MAX - 1)) + 1),
	                       "console_wrap_offsets");
	(*offsets)[0] = 0;

	for (i = 0, end = width, j = 0; j < len && str[j]; j += BLI_str_utf8_size_safe(str + j)) {
		int columns = BLI_str_utf8_char_width_safe(str + j);

		if (i + columns > end) {
			(*offsets)[*lines] = j;
			(*lines)++;

			end = i + width;
		}
		i += columns;
	}
	return j; /* return actual length */
}

/* return 0 if the last line is off the screen
 * should be able to use this for any string type */

static int console_draw_string(ConsoleDrawContext *cdc, const char *str, int str_len,
                               const unsigned char fg[3], const unsigned char bg[3], const unsigned char bg_sel[4])
{
	int tot_lines;            /* total number of lines for wrapping */
	int *offsets;             /* offsets of line beginnings for wrapping */
	int y_next;

	str_len = console_wrap_offsets(str, str_len, cdc->console_width, &tot_lines, &offsets);
	y_next = cdc->xy[1] + cdc->lheight * tot_lines;

	/* just advance the height */
	if (cdc->draw == 0) {
		if (cdc->pos_pick && cdc->mval[1] != INT_MAX && cdc->xy[1] <= cdc->mval[1]) {
			if (y_next >= cdc->mval[1]) {
				int ofs = 0;

				/* wrap */
				if (tot_lines > 1) {
					int iofs = (int)((float)(y_next - cdc->mval[1]) / cdc->lheight);
					ofs += offsets[MIN2(iofs, tot_lines - 1)];
				}

				/* last part */
				ofs += txt_utf8_column_to_offset(str + ofs,
				                                 (int)floor((float)cdc->mval[0] / cdc->cwidth));

				CLAMP(ofs, 0, str_len);
				*cdc->pos_pick += str_len - ofs;
			}
			else
				*cdc->pos_pick += str_len + 1;
		}

		cdc->xy[1] = y_next;
		MEM_freeN(offsets);
		return 1;
	}
	else if (y_next < cdc->ymin) {
		/* have not reached the drawable area so don't break */
		cdc->xy[1] = y_next;

		/* adjust selection even if not drawing */
		if (cdc->sel[0] != cdc->sel[1]) {
			console_step_sel(cdc, -(str_len + 1));
		}

		MEM_freeN(offsets);
		return 1;
	}

	if (tot_lines > 1) { /* wrap? */
		const int initial_offset = offsets[tot_lines - 1];
		size_t len = str_len - initial_offset;
		const char *s = str + initial_offset;
		int i;

		int sel_orig[2];
		copy_v2_v2_int(sel_orig, cdc->sel);

		/* invert and swap for wrapping */
		cdc->sel[0] = str_len - sel_orig[1];
		cdc->sel[1] = str_len - sel_orig[0];

		if (bg) {
			GPUVertFormat *format = immVertexFormat();
			uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

			immUniformColor3ubv(bg);
			immRecti(pos, 0, cdc->xy[1], cdc->winx, (cdc->xy[1] + (cdc->lheight * tot_lines)));

			immUnbindProgram();
		}

		/* last part needs no clipping */
		BLF_position(cdc->font_id, cdc->xy[0], cdc->lofs + cdc->xy[1], 0);
		BLF_color3ubv(cdc->font_id, fg);
		BLF_draw_mono(cdc->font_id, s, len, cdc->cwidth);

		if (cdc->sel[0] != cdc->sel[1]) {
			console_step_sel(cdc, -initial_offset);
			/* BLF_color3ub(cdc->font_id, 255, 0, 0); // debug */
			console_draw_sel(s, cdc->sel, cdc->xy, len, cdc->cwidth, cdc->lheight, bg_sel);
		}

		cdc->xy[1] += cdc->lheight;

		for (i = tot_lines - 1; i > 0; i--) {
			len = offsets[i] - offsets[i - 1];
			s = str + offsets[i - 1];

			BLF_position(cdc->font_id, cdc->xy[0], cdc->lofs + cdc->xy[1], 0);
			BLF_draw_mono(cdc->font_id, s, len, cdc->cwidth);

			if (cdc->sel[0] != cdc->sel[1]) {
				console_step_sel(cdc, len);
				/* BLF_color3ub(cdc->font_id, 0, 255, 0); // debug */
				console_draw_sel(s, cdc->sel, cdc->xy, len, cdc->cwidth, cdc->lheight, bg_sel);
			}

			cdc->xy[1] += cdc->lheight;

			/* check if were out of view bounds */
			if (cdc->xy[1] > cdc->ymax) {
				MEM_freeN(offsets);
				return 0;
			}
		}

		copy_v2_v2_int(cdc->sel, sel_orig);
		console_step_sel(cdc, -(str_len + 1));
	}
	else { /* simple, no wrap */

		if (bg) {
			GPUVertFormat *format = immVertexFormat();
			uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

			immUniformColor3ubv(bg);
			immRecti(pos, 0, cdc->xy[1], cdc->winx, cdc->xy[1] + cdc->lheight);

			immUnbindProgram();
		}

		BLF_color3ubv(cdc->font_id, fg);
		BLF_position(cdc->font_id, cdc->xy[0], cdc->lofs + cdc->xy[1], 0);
		BLF_draw_mono(cdc->font_id, str, str_len, cdc->cwidth);

		if (cdc->sel[0] != cdc->sel[1]) {
			int isel[2];

			isel[0] = str_len - cdc->sel[1];
			isel[1] = str_len - cdc->sel[0];

			/* BLF_color3ub(cdc->font_id, 255, 255, 0); // debug */
			console_draw_sel(str, isel, cdc->xy, str_len, cdc->cwidth, cdc->lheight, bg_sel);
			console_step_sel(cdc, -(str_len + 1));
		}

		cdc->xy[1] += cdc->lheight;

		if (cdc->xy[1] > cdc->ymax) {
			MEM_freeN(offsets);
			return 0;
		}
	}

	MEM_freeN(offsets);
	return 1;
}

#define CONSOLE_DRAW_MARGIN 4

int textview_draw(TextViewContext *tvc, const int draw, int mval[2], void **mouse_pick, int *pos_pick)
{
	ConsoleDrawContext cdc = {0};

	int x_orig = CONSOLE_DRAW_MARGIN, y_orig = CONSOLE_DRAW_MARGIN + tvc->lheight / 6;
	int xy[2], y_prev;
	int sel[2] = {-1, -1}; /* defaults disabled */
	unsigned char fg[3], bg[3];
	const int font_id = blf_mono_font;

	console_font_begin(font_id, tvc->lheight);

	xy[0] = x_orig; xy[1] = y_orig;

	if (mval[1] != INT_MAX)
		mval[1] += (tvc->ymin + CONSOLE_DRAW_MARGIN);

	if (pos_pick)
		*pos_pick = 0;

	/* constants for the sequencer context */
	cdc.font_id = font_id;
	cdc.cwidth = (int)BLF_fixed_width(font_id);
	assert(cdc.cwidth > 0);
	cdc.lheight = tvc->lheight;
	cdc.lofs = -BLF_descender(font_id);
	/* note, scroll bar must be already subtracted () */
	cdc.console_width = (tvc->winx - (CONSOLE_DRAW_MARGIN * 2)) / cdc.cwidth;
	/* avoid divide by zero on small windows */
	if (cdc.console_width < 1)
		cdc.console_width = 1;
	cdc.winx = tvc->winx - CONSOLE_DRAW_MARGIN;
	cdc.ymin = tvc->ymin;
	cdc.ymax = tvc->ymax;
	cdc.xy = xy;
	cdc.sel = sel;
	cdc.pos_pick = pos_pick;
	cdc.mval = mval;
	cdc.draw = draw;

	/* shouldnt be needed */
	tvc->cwidth = cdc.cwidth;
	tvc->console_width = cdc.console_width;
	tvc->iter_index = 0;

	if (tvc->sel_start != tvc->sel_end) {
		sel[0] = tvc->sel_start;
		sel[1] = tvc->sel_end;
	}

	if (tvc->begin(tvc)) {
		unsigned char bg_sel[4] = {0};

		if (draw && tvc->const_colors) {
			tvc->const_colors(tvc, bg_sel);
		}

		do {
			const char *ext_line;
			int ext_len;
			int color_flag = 0;

			y_prev = xy[1];

			if (draw)
				color_flag = tvc->line_color(tvc, fg, bg);

			tvc->line_get(tvc, &ext_line, &ext_len);

			if (!console_draw_string(&cdc, ext_line, ext_len,
			                         (color_flag & TVC_LINE_FG) ? fg : NULL,
			                         (color_flag & TVC_LINE_BG) ? bg : NULL,
			                         bg_sel))
			{
				/* when drawing, if we pass v2d->cur.ymax, then quit */
				if (draw) {
					break; /* past the y limits */
				}
			}

			if ((mval[1] != INT_MAX) && (mval[1] >= y_prev && mval[1] <= xy[1])) {
				*mouse_pick = (void *)tvc->iter;
				break;
			}

			tvc->iter_index++;

		} while (tvc->step(tvc));
	}

	tvc->end(tvc);

	xy[1] += tvc->lheight * 2;

	return xy[1] - y_orig;
}
