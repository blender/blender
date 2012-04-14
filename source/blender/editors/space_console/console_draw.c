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

#include "BLF_api.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BKE_report.h"


#include "MEM_guardedalloc.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_datafiles.h"
#include "ED_types.h"

#include "UI_resources.h"

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

typedef struct ConsoleDrawContext {
	int cwidth;
	int lheight;
	int console_width;
	int winx;
	int ymin, ymax;
#if 0 /* used by textview, may use later */
	int *xy; // [2]
	int *sel; // [2]
	int *pos_pick; // bottom of view == 0, top of file == combine chars, end of line is lower then start. 
	int *mval; // [2]
	int draw;
#endif
} ConsoleDrawContext;

void console_scrollback_prompt_begin(struct SpaceConsole *sc, ConsoleLine *cl_dummy)
{
	/* fake the edit line being in the scroll buffer */
	ConsoleLine *cl = sc->history.last;
	cl_dummy->type = CONSOLE_LINE_INPUT;
	cl_dummy->len = cl_dummy->len_alloc = strlen(sc->prompt) + cl->len;
	cl_dummy->len_alloc = cl_dummy->len + 1;
	cl_dummy->line = MEM_mallocN(cl_dummy->len_alloc, "cl_dummy");
	memcpy(cl_dummy->line, sc->prompt, (cl_dummy->len_alloc - cl->len));
	memcpy(cl_dummy->line + ((cl_dummy->len_alloc - cl->len)) - 1, cl->line, cl->len + 1);
	BLI_addtail(&sc->scrollback, cl_dummy);
}
void console_scrollback_prompt_end(struct SpaceConsole *sc, ConsoleLine *cl_dummy) 
{
	MEM_freeN(cl_dummy->line);
	BLI_remlink(&sc->scrollback, cl_dummy);
}

#define CONSOLE_DRAW_MARGIN 4
#define CONSOLE_DRAW_SCROLL 16



/* console textview callbacks */
static int console_textview_begin(TextViewContext *tvc)
{
	SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
	tvc->lheight = sc->lheight;
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

	return 1;
}

static int console_textview_line_color(struct TextViewContext *tvc, unsigned char fg[3], unsigned char UNUSED(bg[3]))
{
	ConsoleLine *cl_iter = (ConsoleLine *)tvc->iter;

	/* annoying hack, to draw the prompt */
	if (tvc->iter_index == 0) {
		const SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
		const ConsoleLine *cl = (ConsoleLine *)sc->history.last;
		const int prompt_len = strlen(sc->prompt);
		const int cursor_loc = cl->cursor + prompt_len;
		int xy[2] = {CONSOLE_DRAW_MARGIN, CONSOLE_DRAW_MARGIN};
		int pen[2];
		xy[1] += tvc->lheight / 6;

		/* account for wrapping */
		if (cl->len < tvc->console_width) {
			/* simple case, no wrapping */
			pen[0] = tvc->cwidth * cursor_loc;
			pen[1] = -2;
		}
		else {
			/* wrap */
			pen[0] = tvc->cwidth * (cursor_loc % tvc->console_width);
			pen[1] = -2 + (((cl->len / tvc->console_width) - (cursor_loc / tvc->console_width)) * tvc->lheight);
		}

		/* cursor */
		UI_GetThemeColor3ubv(TH_CONSOLE_CURSOR, fg);
		glColor3ubv(fg);

		glRecti((xy[0] + pen[0]) - 1,
		        (xy[1] + pen[1]),
		        (xy[0] + pen[0]) + 1,
		        (xy[1] + pen[1] + tvc->lheight)
		        );
	}

	console_line_color(fg, cl_iter->type);

	return TVC_LINE_FG;
}


static int console_textview_main__internal(struct SpaceConsole *sc, struct ARegion *ar, int draw, int mval[2], void **mouse_pick, int *pos_pick)
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

	tvc.arg1 = sc;
	tvc.arg2 = NULL;

	/* view */
	tvc.sel_start = sc->sel_start;
	tvc.sel_end = sc->sel_end;
	tvc.lheight = sc->lheight;
	tvc.ymin = v2d->cur.ymin;
	tvc.ymax = v2d->cur.ymax;
	tvc.winx = ar->winx;

	console_scrollback_prompt_begin(sc, &cl_dummy);
	ret = textview_draw(&tvc, draw, mval, mouse_pick, pos_pick);
	console_scrollback_prompt_end(sc, &cl_dummy);

	return ret;
}


void console_textview_main(struct SpaceConsole *sc, struct ARegion *ar)
{
	int mval[2] = {INT_MAX, INT_MAX};
	console_textview_main__internal(sc, ar, 1,  mval, NULL, NULL);
}

int console_textview_height(struct SpaceConsole *sc, struct ARegion *ar)
{
	int mval[2] = {INT_MAX, INT_MAX};
	return console_textview_main__internal(sc, ar, 0,  mval, NULL, NULL);
}

int console_char_pick(struct SpaceConsole *sc, struct ARegion *ar, int mval[2])
{
	int pos_pick = 0;
	void *mouse_pick = NULL;
	int mval_clamp[2];

	mval_clamp[0] = CLAMPIS(mval[0], CONSOLE_DRAW_MARGIN, ar->winx - (CONSOLE_DRAW_SCROLL + CONSOLE_DRAW_MARGIN));
	mval_clamp[1] = CLAMPIS(mval[1], CONSOLE_DRAW_MARGIN, ar->winy - CONSOLE_DRAW_MARGIN);

	console_textview_main__internal(sc, ar, 0, mval_clamp, &mouse_pick, &pos_pick);
	return pos_pick;
}
