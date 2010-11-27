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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>


#include "BLF_api.h"

#include "BLI_blenlib.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

// #include "BKE_suggestions.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

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
	switch(type) {
	case CONSOLE_LINE_OUTPUT:
		UI_GetThemeColor3ubv(TH_CONSOLE_OUTPUT, (char *)fg);
		break;
	case CONSOLE_LINE_INPUT:
		UI_GetThemeColor3ubv(TH_CONSOLE_INPUT, (char *)fg);
		break;
	case CONSOLE_LINE_INFO:
		UI_GetThemeColor3ubv(TH_CONSOLE_INFO, (char *)fg);
		break;
	case CONSOLE_LINE_ERROR:
		UI_GetThemeColor3ubv(TH_CONSOLE_ERROR, (char *)fg);
		break;
	}
}

typedef struct ConsoleDrawContext {
	int cwidth;
	int lheight;
	int console_width;
	int winx;
	int ymin, ymax;
	int *xy; // [2]
	int *sel; // [2]
	int *pos_pick; // bottom of view == 0, top of file == combine chars, end of line is lower then start. 
	int *mval; // [2]
	int draw;
} ConsoleDrawContext;

void console_scrollback_prompt_begin(struct SpaceConsole *sc, ConsoleLine *cl_dummy)
{
	/* fake the edit line being in the scroll buffer */
	ConsoleLine *cl= sc->history.last;
	cl_dummy->type= CONSOLE_LINE_INPUT;
	cl_dummy->len= cl_dummy->len_alloc= strlen(sc->prompt) + cl->len;
	cl_dummy->len_alloc= cl_dummy->len + 1;
	cl_dummy->line= MEM_mallocN(cl_dummy->len_alloc, "cl_dummy");
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
	SpaceConsole *sc= (SpaceConsole *)tvc->arg1;
	tvc->lheight= sc->lheight;
	tvc->sel_start= sc->sel_start;
	tvc->sel_end= sc->sel_end;
	
	/* iterator */
	tvc->iter= sc->scrollback.last;
	
	return (tvc->iter != NULL);
}

static void console_textview_end(TextViewContext *tvc)
{
	SpaceConsole *sc= (SpaceConsole *)tvc->arg1;
	(void)sc;
	
}

static int console_textview_step(TextViewContext *tvc)
{
	return ((tvc->iter= (void *)((Link *)tvc->iter)->prev) != NULL);
}

static int console_textview_line_get(struct TextViewContext *tvc, const char **line, int *len)
{
	ConsoleLine *cl= (ConsoleLine *)tvc->iter;
	*line= cl->line;
	*len= cl->len;

	return 1;
}

static int console_textview_line_color(struct TextViewContext *tvc, unsigned char fg[3], unsigned char UNUSED(bg[3]))
{
	ConsoleLine *cl= (ConsoleLine *)tvc->iter;

	/* annoying hack, to draw the prompt */
	if(tvc->iter_index == 0) {
		SpaceConsole *sc= (SpaceConsole *)tvc->arg1;
		int prompt_len= strlen(sc->prompt);
		int xy[2] = {CONSOLE_DRAW_MARGIN, CONSOLE_DRAW_MARGIN};
		const int cursor = ((ConsoleLine *)sc->history.last)->cursor;
		xy[1] += tvc->lheight/6;
		
		/* cursor */
		UI_GetThemeColor3ubv(TH_CONSOLE_CURSOR, (char *)fg);
		glColor3ubv(fg);

		glRecti(xy[0]+(tvc->cwidth*(cursor+prompt_len)) -1, xy[1]-2, xy[0]+(tvc->cwidth*(cursor+prompt_len)) +1, xy[1]+tvc->lheight-2);
	}

	console_line_color(fg, cl->type);

	return TVC_LINE_FG;
}


static int console_textview_main__internal(struct SpaceConsole *sc, struct ARegion *ar, ReportList *UNUSED(reports), int draw, int mval[2], void **mouse_pick, int *pos_pick)
{
	ConsoleLine cl_dummy= {0};
	int ret= 0;
	
	View2D *v2d= &ar->v2d;

	TextViewContext tvc= {0};
	tvc.begin= console_textview_begin;
	tvc.end= console_textview_end;

	tvc.step= console_textview_step;
	tvc.line_get= console_textview_line_get;
	tvc.line_color= console_textview_line_color;

	tvc.arg1= sc;
	tvc.arg2= NULL;

	/* view */
	tvc.sel_start= sc->sel_start;
	tvc.sel_end= sc->sel_end;
	tvc.lheight= sc->lheight;
	tvc.ymin= v2d->cur.ymin;
	tvc.ymax= v2d->cur.ymax;
	tvc.winx= ar->winx;

	console_scrollback_prompt_begin(sc, &cl_dummy);
	ret= textview_draw(&tvc, draw, mval, mouse_pick, pos_pick);
	console_scrollback_prompt_end(sc, &cl_dummy);

	return ret;
}


void console_textview_main(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports)
{
	int mval[2] = {INT_MAX, INT_MAX};
	console_textview_main__internal(sc, ar, reports, 1,  mval, NULL, NULL);
}

int console_textview_height(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports)
{
	int mval[2] = {INT_MAX, INT_MAX};
	return console_textview_main__internal(sc, ar, reports, 0,  mval, NULL, NULL);
}

void *console_text_pick(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports, int mouse_y)
{
	void *mouse_pick= NULL;
	int mval[2];

	mval[0]= 0;
	mval[1]= mouse_y;

	console_textview_main__internal(sc, ar, reports, 0, mval, &mouse_pick, NULL);
	return (void *)mouse_pick;
}

int console_char_pick(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports, int mval[2])
{
	int pos_pick= 0;
	void *mouse_pick= NULL;
	int mval_clamp[2];

	mval_clamp[0]= CLAMPIS(mval[0], CONSOLE_DRAW_MARGIN, ar->winx-(CONSOLE_DRAW_SCROLL + CONSOLE_DRAW_MARGIN));
	mval_clamp[1]= CLAMPIS(mval[1], CONSOLE_DRAW_MARGIN, ar->winy-CONSOLE_DRAW_MARGIN);

	console_textview_main__internal(sc, ar, reports, 0, mval_clamp, &mouse_pick, &pos_pick);
	return pos_pick;
}
