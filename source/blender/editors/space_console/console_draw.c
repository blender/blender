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

/** \file blender/editors/space_console/console_draw.c
 *  \ingroup spconsole
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>


#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BIF_gl.h"
#include "GPU_immediate.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "console_intern.h"


#include "../space_info/textview.h"

static void console_line_color(unsigned char fg[3], int type)
{
	switch (type) {
		case CONSOLE_LINE_OUTPUT:
			UI_GetThemeColor3ubv(TH_CONSOLE_OUTPUT, fg);
			break;
		case CONSOLE_LINE_INPUT:
			UI_GetThemeColor3ubv(TH_CONSOLE_INPUT, fg);
			break;
		case CONSOLE_LINE_INFO:
			UI_GetThemeColor3ubv(TH_CONSOLE_INFO, fg);
			break;
		case CONSOLE_LINE_ERROR:
			UI_GetThemeColor3ubv(TH_CONSOLE_ERROR, fg);
			break;
	}
}

void console_scrollback_prompt_begin(struct SpaceConsole *sc, ConsoleLine *cl_dummy)
{
	/* fake the edit line being in the scroll buffer */
	ConsoleLine *cl = sc->history.last;
	int prompt_len = strlen(sc->prompt);

	cl_dummy->type = CONSOLE_LINE_INPUT;
	cl_dummy->len = prompt_len + cl->len;
	cl_dummy->len_alloc = cl_dummy->len + 1;
	cl_dummy->line = MEM_mallocN(cl_dummy->len_alloc, "cl_dummy");
	memcpy(cl_dummy->line, sc->prompt, prompt_len);
	memcpy(cl_dummy->line + prompt_len, cl->line, cl->len + 1);
	BLI_addtail(&sc->scrollback, cl_dummy);
}
void console_scrollback_prompt_end(struct SpaceConsole *sc, ConsoleLine *cl_dummy)
{
	MEM_freeN(cl_dummy->line);
	BLI_remlink(&sc->scrollback, cl_dummy);
}

#define CONSOLE_DRAW_MARGIN 4

/* console textview callbacks */
static int console_textview_begin(TextViewContext *tvc)
{
	SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
	tvc->lheight = sc->lheight * UI_DPI_FAC;
	tvc->sel_start = sc->sel_start;
	tvc->sel_end = sc->sel_end;

	/* iterator */
	tvc->iter = sc->scrollback.last;

	return (tvc->iter != NULL);
}

static void console_textview_end(TextViewContext *tvc)
{
	SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
	(void)sc;

}

static int console_textview_step(TextViewContext *tvc)
{
	return ((tvc->iter = (void *)((Link *)tvc->iter)->prev) != NULL);
}

static int console_textview_line_get(struct TextViewContext *tvc, const char **line, int *len)
{
	ConsoleLine *cl = (ConsoleLine *)tvc->iter;
	*line = cl->line;
	*len = cl->len;
	// printf("'%s' %d\n", *line, cl->len);
	BLI_assert(cl->line[cl->len] == '\0' && (cl->len == 0 || cl->line[cl->len - 1] != '\0'));
	return 1;
}

static void console_cursor_wrap_offset(const char *str, int width, int *row, int *column, const char *end)
{
	int col;

	for (; *str; str += BLI_str_utf8_size_safe(str)) {
		col = BLI_str_utf8_char_width_safe(str);

		if (*column + col > width) {
			(*row)++;
			*column = 0;
		}

		if (end && str >= end)
			break;

		*column += col;
	}
	return;
}

static int console_textview_line_color(struct TextViewContext *tvc, unsigned char fg[3], unsigned char UNUSED(bg[3]))
{
	ConsoleLine *cl_iter = (ConsoleLine *)tvc->iter;

	/* annoying hack, to draw the prompt */
	if (tvc->iter_index == 0) {
		const SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
		const ConsoleLine *cl = (ConsoleLine *)sc->history.last;
		int offl = 0, offc = 0;
		int xy[2] = {CONSOLE_DRAW_MARGIN, CONSOLE_DRAW_MARGIN};
		int pen[2];
		GPUVertFormat *format = immVertexFormat();
		uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		xy[1] += tvc->lheight / 6;

		console_cursor_wrap_offset(sc->prompt, tvc->console_width, &offl, &offc, NULL);
		console_cursor_wrap_offset(cl->line, tvc->console_width, &offl, &offc, cl->line + cl->cursor);
		pen[0] = tvc->cwidth * offc;
		pen[1] = -2 - tvc->lheight * offl;

		console_cursor_wrap_offset(cl->line + cl->cursor, tvc->console_width, &offl, &offc, NULL);
		pen[1] += tvc->lheight * offl;

		/* cursor */
		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
		immUniformThemeColor(TH_CONSOLE_CURSOR);

		immRectf(pos,
		         (xy[0] + pen[0]) - 1,
		         (xy[1] + pen[1]),
		         (xy[0] + pen[0]) + 1,
		         (xy[1] + pen[1] + tvc->lheight)
		         );

		immUnbindProgram();
	}

	console_line_color(fg, cl_iter->type);

	return TVC_LINE_FG;
}

static void console_textview_const_colors(TextViewContext *UNUSED(tvc), unsigned char bg_sel[4])
{
	UI_GetThemeColor4ubv(TH_CONSOLE_SELECT, bg_sel);
}

static int console_textview_main__internal(struct SpaceConsole *sc, ARegion *ar, int draw,
                                           int mval[2], void **mouse_pick, int *pos_pick)
{
	ConsoleLine cl_dummy = {NULL};
	int ret = 0;

	View2D *v2d = &ar->v2d;

	TextViewContext tvc = {0};

	tvc.begin = console_textview_begin;
	tvc.end = console_textview_end;

	tvc.step = console_textview_step;
	tvc.line_get = console_textview_line_get;
	tvc.line_color = console_textview_line_color;
	tvc.const_colors = console_textview_const_colors;

	tvc.arg1 = sc;
	tvc.arg2 = NULL;

	/* view */
	tvc.sel_start = sc->sel_start;
	tvc.sel_end = sc->sel_end;
	tvc.lheight = sc->lheight * UI_DPI_FAC;
	tvc.ymin = v2d->cur.ymin;
	tvc.ymax = v2d->cur.ymax;
	tvc.winx = ar->winx - V2D_SCROLL_WIDTH;

	console_scrollback_prompt_begin(sc, &cl_dummy);
	ret = textview_draw(&tvc, draw, mval, mouse_pick, pos_pick);
	console_scrollback_prompt_end(sc, &cl_dummy);

	return ret;
}


void console_textview_main(struct SpaceConsole *sc, ARegion *ar)
{
	int mval[2] = {INT_MAX, INT_MAX};
	console_textview_main__internal(sc, ar, 1,  mval, NULL, NULL);
}

int console_textview_height(struct SpaceConsole *sc, ARegion *ar)
{
	int mval[2] = {INT_MAX, INT_MAX};
	return console_textview_main__internal(sc, ar, 0,  mval, NULL, NULL);
}

int console_char_pick(struct SpaceConsole *sc, ARegion *ar, const int mval[2])
{
	int pos_pick = 0;
	void *mouse_pick = NULL;
	int mval_clamp[2];

	mval_clamp[0] = CLAMPIS(mval[0], CONSOLE_DRAW_MARGIN, ar->winx - CONSOLE_DRAW_MARGIN);
	mval_clamp[1] = CLAMPIS(mval[1], CONSOLE_DRAW_MARGIN, ar->winy - CONSOLE_DRAW_MARGIN);

	console_textview_main__internal(sc, ar, 0, mval_clamp, &mouse_pick, &pos_pick);
	return pos_pick;
}
