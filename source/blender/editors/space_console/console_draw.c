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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
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

#include "MEM_guardedalloc.h"

#include "BLF_api.h"

#include "BLI_blenlib.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
// #include "BKE_suggestions.h"
#include "BKE_text.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_datafiles.h"
#include "ED_types.h"

#include "UI_resources.h"

#include "console_intern.h"


static int mono= -1; // XXX needs proper storage and change all the BLF_* here!

static void console_font_begin(SpaceConsole *sc)
{
	if(mono == -1)
		mono= BLF_load_mem("monospace", (unsigned char*)datatoc_bmonofont_ttf, datatoc_bmonofont_ttf_size);

	BLF_aspect(mono, 1.0);
	BLF_size(mono, sc->lheight-2, 72);
}

static void console_line_color(unsigned char *fg, int type)
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

static void console_report_color(unsigned char *fg, unsigned char *bg, Report *report, int bool)
{
	/*
	if		(type & RPT_ERROR_ALL)		{ fg[0]=220; fg[1]=0; fg[2]=0; }
	else if	(type & RPT_WARNING_ALL)	{ fg[0]=220; fg[1]=96; fg[2]=96; }
	else if	(type & RPT_OPERATOR_ALL)	{ fg[0]=96; fg[1]=128; fg[2]=255; }
	else if	(type & RPT_INFO_ALL)		{ fg[0]=0; fg[1]=170; fg[2]=0; }
	else if	(type & RPT_DEBUG_ALL)		{ fg[0]=196; fg[1]=196; fg[2]=196; }
	else								{ fg[0]=196; fg[1]=196; fg[2]=196; }
	*/
	if(report->flag & SELECT) {
		fg[0]=255; fg[1]=255; fg[2]=255;
		if(bool) {
			bg[0]=96; bg[1]=128; bg[2]=255;
		}
		else {
			bg[0]=90; bg[1]=122; bg[2]=249;
		}
	}

	else {
		fg[0]=0; fg[1]=0; fg[2]=0;

		if(bool) {
			bg[0]=120; bg[1]=120; bg[2]=120;
		}
		else {
			bg[0]=114; bg[1]=114; bg[2]=114;
		}

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
	int *pos_pick;
	int *mval; // [2]
	int draw;
} ConsoleDrawContext;

static void console_draw_sel(int sel[2], int xy[2], int str_len, int cwidth, int console_width, int lheight)
{
	if(sel[0] <= str_len && sel[1] >= 0) {
		int sta = MAX2(sel[0], 0);
		int end = MIN2(sel[1], str_len);

		/* highly confusing but draws correctly */
		if(sel[0] < 0 || sel[1] > str_len) {
			if(sel[0] > 0) {
				end= sta;
				sta= 0;
			}
			if (sel[1] <= str_len) {
				sta= end;
				end= str_len;
			}
		}
		/* end confusement */

		{
			glEnable(GL_POLYGON_STIPPLE);
			glPolygonStipple(stipple_halftone);
			glEnable( GL_BLEND );
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glColor4ub(255, 255, 255, 96);
		}
		glRecti(xy[0]+(cwidth*sta), xy[1]-2 + lheight, xy[0]+(cwidth*end), xy[1]-2);
		{
			glDisable(GL_POLYGON_STIPPLE);
			glDisable( GL_BLEND );
		}
	}

	sel[0] -= str_len + 1;
	sel[1] -= str_len + 1;
}


/* return 0 if the last line is off the screen
 * should be able to use this for any string type */

static int console_draw_string(ConsoleDrawContext *cdc, char *str, int str_len, unsigned char *fg, unsigned char *bg)
{
	int rct_ofs= cdc->lheight/4;
	int tot_lines = (str_len/cdc->console_width)+1; /* total number of lines for wrapping */
	int y_next = (str_len > cdc->console_width) ? cdc->xy[1]+cdc->lheight*tot_lines : cdc->xy[1]+cdc->lheight;

	/* just advance the height */
	if(cdc->draw==0) {
		if(cdc->pos_pick) {
			if((cdc->mval[1] != INT_MAX) && cdc->xy[1] <= cdc->mval[1]) {
				if((cdc->xy[1]+cdc->lheight >= cdc->mval[1])) {
					int ofs = (int)floor(((float)cdc->mval[0] / (float)cdc->cwidth));
					*cdc->pos_pick += MIN2(ofs, str_len);
				} else
					*cdc->pos_pick += str_len + 1;
			}

		}

		cdc->xy[1]= y_next;
		return 1;
	}
	else if (y_next-cdc->lheight < cdc->ymin) {
		/* have not reached the drawable area so don't break */
		cdc->xy[1]= y_next;

		/* adjust selection even if not drawing */
		if(cdc->sel[0] != cdc->sel[1]) {
			cdc->sel[0] -= str_len + 1;
			cdc->sel[1] -= str_len + 1;
		}

		return 1;
	}

	if(str_len > cdc->console_width) { /* wrap? */
		char *line_stride= str + ((tot_lines-1) * cdc->console_width);	/* advance to the last line and draw it first */
		char eol;													/* baclup the end of wrapping */
		
		if(bg) {
			glColor3ub(bg[0], bg[1], bg[2]);
			glRecti(0, cdc->xy[1]-rct_ofs, cdc->winx, (cdc->xy[1]+(cdc->lheight*tot_lines))+rct_ofs);
		}

		glColor3ub(fg[0], fg[1], fg[2]);

		/* last part needs no clipping */
		BLF_position(mono, cdc->xy[0], cdc->xy[1], 0);
		BLF_draw(mono, line_stride);

		if(cdc->sel[0] != cdc->sel[1]) {
			cdc->sel[0] += str_len - (cdc->console_width % str_len);
			cdc->sel[1] += str_len - (cdc->console_width % str_len);
			console_draw_sel(cdc->sel, cdc->xy, cdc->console_width % str_len, cdc->cwidth, cdc->console_width, cdc->lheight);
			glColor3ub(fg[0], fg[1], fg[2]);
		}

		cdc->xy[1] += cdc->lheight;

		line_stride -= cdc->console_width;
		
		for(; line_stride >= str; line_stride -= cdc->console_width) {
			eol = line_stride[cdc->console_width];
			line_stride[cdc->console_width]= '\0';
			
			BLF_position(mono, cdc->xy[0], cdc->xy[1], 0);
			BLF_draw(mono, line_stride);
			
			if(cdc->sel[0] != cdc->sel[1]) {
				console_draw_sel(cdc->sel, cdc->xy, cdc->console_width, cdc->cwidth, cdc->console_width, cdc->lheight);
				glColor3ub(fg[0], fg[1], fg[2]);
			}

			cdc->xy[1] += cdc->lheight;

			line_stride[cdc->console_width] = eol; /* restore */
			
			/* check if were out of view bounds */
			if(cdc->xy[1] > cdc->ymax)
				return 0;
		}
	}
	else { /* simple, no wrap */

		if(bg) {
			glColor3ub(bg[0], bg[1], bg[2]);
			glRecti(0, cdc->xy[1]-rct_ofs, cdc->winx, cdc->xy[1]+cdc->lheight-rct_ofs);
		}

		glColor3ub(fg[0], fg[1], fg[2]);

		BLF_position(mono, cdc->xy[0], cdc->xy[1], 0);
		BLF_draw(mono, str);
		
		if(cdc->sel[0] != cdc->sel[1])
			console_draw_sel(cdc->sel, cdc->xy, str_len, cdc->cwidth, cdc->console_width, cdc->lheight);

		cdc->xy[1] += cdc->lheight;

		if(cdc->xy[1] > cdc->ymax)
			return 0;
	}

	return 1;
}

#define CONSOLE_DRAW_MARGIN 4
#define CONSOLE_DRAW_SCROLL 16

static int console_text_main__internal(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports, int draw, int mval[2], void **mouse_pick, int *pos_pick)
{
	View2D *v2d= &ar->v2d;

	ConsoleLine *cl= sc->history.last;
	ConsoleDrawContext cdc;
	
	int x_orig=CONSOLE_DRAW_MARGIN, y_orig=CONSOLE_DRAW_MARGIN;
	int xy[2], y_prev;
	int cwidth;
	int console_width; /* number of characters that fit into the width of the console (fixed width) */
	int sel[2]= {-1, -1}; /* defaults disabled */
	unsigned char fg[3];

	console_font_begin(sc);
	cwidth = BLF_fixed_width(mono);
	
	console_width= (ar->winx - (CONSOLE_DRAW_SCROLL + CONSOLE_DRAW_MARGIN*2) )/cwidth;
	if (console_width < 8) console_width= 8;
	
	xy[0]= x_orig; xy[1]= y_orig;
	
	if(mval[1] != INT_MAX)
		mval[1] += (v2d->cur.ymin + CONSOLE_DRAW_MARGIN);

	if(pos_pick)
		*pos_pick = 0;

	/* constants for the sequencer context */
	cdc.cwidth= cwidth;
	cdc.lheight= sc->lheight;
	cdc.console_width= console_width;
	cdc.winx= ar->winx-(CONSOLE_DRAW_MARGIN+CONSOLE_DRAW_SCROLL);
	cdc.ymin= v2d->cur.ymin;
	cdc.ymax= v2d->cur.ymax;
	cdc.xy= xy;
	cdc.sel= sel;
	cdc.pos_pick= pos_pick;
	cdc.mval= mval;
	cdc.draw= draw;

	if(sc->type==CONSOLE_TYPE_PYTHON) {
		int prompt_len;

		if(sc->sel_start != sc->sel_end) {
			sel[0]= sc->sel_start;
			sel[1]= sc->sel_end;
			// printf("%d %d\n", sel[0], sel[1]);
		}
		
		/* text */
		if(draw) {
			prompt_len= strlen(sc->prompt);
			console_line_color(fg, CONSOLE_LINE_INPUT);
			glColor3ub(fg[0], fg[1], fg[2]);

			/* command line */
			if(prompt_len) {
				BLF_position(mono, xy[0], xy[1], 0); xy[0] += cwidth * prompt_len;
				BLF_draw(mono, sc->prompt);
			}
			BLF_position(mono, xy[0], xy[1], 0);
			BLF_draw(mono, cl->line);

			/* cursor */
			UI_GetThemeColor3ubv(TH_CONSOLE_CURSOR, (char *)fg);
			glColor3ub(fg[0], fg[1], fg[2]);
			glRecti(xy[0]+(cwidth*cl->cursor) -1, xy[1]-2, xy[0]+(cwidth*cl->cursor) +1, xy[1]+sc->lheight-2);

			xy[0]= x_orig; /* remove prompt offset */
		}
		
		xy[1] += sc->lheight;
		
		for(cl= sc->scrollback.last; cl; cl= cl->prev) {
			y_prev= xy[1];

			if(draw)
				console_line_color(fg, cl->type);

			if(!console_draw_string(&cdc, cl->line, cl->len, fg, NULL)) {
				/* when drawing, if we pass v2d->cur.ymax, then quit */
				if(draw) {
					break; /* past the y limits */
				}
			}

			if((mval[1] != INT_MAX) && (mval[1] >= y_prev && mval[1] <= xy[1])) {
				*mouse_pick= (void *)cl;
				break;
			}
		}
	}
	else { 
		Report *report;
		int report_mask= 0;
		int bool= 0;
		unsigned char bg[3];

		if(draw) {
			glClearColor(120.0/255.0, 120.0/255.0, 120.0/255.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		/* convert our display toggles into a flag compatible with BKE_report flags */
		report_mask= console_report_mask(sc);
		
		for(report=reports->list.last; report; report=report->prev) {
			
			if(report->type & report_mask) {
				y_prev= xy[1];

				if(draw)
					console_report_color(fg, bg, report, bool);

				if(!console_draw_string(&cdc, report->message, report->len, fg, bg)) {
					/* when drawing, if we pass v2d->cur.ymax, then quit */
					if(draw) {
						break; /* past the y limits */
					}
				}
				if((mval[1] != INT_MAX) && (mval[1] >= y_prev && mval[1] <= xy[1])) {
					*mouse_pick= (void *)report;
					break;
				}

				bool = !(bool);
			}
		}
	}
	xy[1] += sc->lheight*2;

	
	return xy[1]-y_orig;
}

void console_text_main(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports)
{
	int mval[2] = {INT_MAX, INT_MAX};
	console_text_main__internal(sc, ar, reports, 1,  mval, NULL, NULL);
}

int console_text_height(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports)
{
	int mval[2] = {INT_MAX, INT_MAX};
	return console_text_main__internal(sc, ar, reports, 0,  mval, NULL, NULL);
}

void *console_text_pick(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports, int mouse_y)
{
	int mval[2] = {0, mouse_y};
	void *mouse_pick= NULL;
	console_text_main__internal(sc, ar, reports, 0, mval, &mouse_pick, NULL);
	return (void *)mouse_pick;
}

// XXX - breaks with line wrap
int console_char_pick(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports, int mval[2])
{
	int pos_pick= 0;
	void *mouse_pick= NULL;
	console_text_main__internal(sc, ar, reports, 0, mval, &mouse_pick, &pos_pick);
	return pos_pick;
}
