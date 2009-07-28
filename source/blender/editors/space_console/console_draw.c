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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "DNA_userdef_types.h"

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

#include "UI_interface.h"
#include "UI_resources.h"

#include "console_intern.h"

static void console_font_begin(SpaceConsole *sc)
{
	static int mono= -1; // XXX needs proper storage

	if(mono == -1)
		mono= BLF_load_mem("monospace", (unsigned char*)datatoc_bmonofont_ttf, datatoc_bmonofont_ttf_size);

	BLF_set(mono);
	BLF_aspect(1.0);

	BLF_size(sc->lheight-2, 72);
}

static void console_line_color(unsigned char *fg, int type)
{
	switch(type) {
	case CONSOLE_LINE_OUTPUT:
		fg[0]=96; fg[1]=128; fg[2]=255;
		break;
	case CONSOLE_LINE_INPUT:
		fg[0]=255; fg[1]=255; fg[2]=255;
		break;
	case CONSOLE_LINE_INFO:
		fg[0]=0; fg[1]=170; fg[2]=0;
		break;
	case CONSOLE_LINE_ERROR:
		fg[0]=220; fg[1]=96; fg[2]=96;
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


/* return 0 if the last line is off the screen
 * should be able to use this for any string type */
static int console_draw_string(	char *str, int str_len,
									int console_width, int lheight,
									unsigned char *fg, unsigned char *bg,
									int winx,
									int ymin, int ymax,
									int *x, int *y, int draw)
{	
	int rct_ofs= lheight/4;
	int tot_lines = (str_len/console_width)+1; /* total number of lines for wrapping */
	int y_next = (str_len > console_width) ? (*y)+lheight*tot_lines : (*y)+lheight;

	/* just advance the height */
	if(draw==0) {
		*y= y_next;
		return 1;
	}
	else if (y_next-lheight < ymin) {
		/* have not reached the drawable area so don't break */
		*y= y_next;
		return 1;
	}

	if(str_len > console_width) { /* wrap? */
		char *line_stride= str + ((tot_lines-1) * console_width);	/* advance to the last line and draw it first */
		char eol;													/* baclup the end of wrapping */
		
		if(bg) {
			glColor3ub(bg[0], bg[1], bg[2]);
			glRecti(0, *y-rct_ofs, winx, (*y+(lheight*tot_lines))+rct_ofs);
		}

		glColor3ub(fg[0], fg[1], fg[2]);

		/* last part needs no clipping */
		BLF_position(*x, *y, 0); (*y) += lheight;
		BLF_draw(line_stride);
		line_stride -= console_width;
		
		for(; line_stride >= str; line_stride -= console_width) {
			eol = line_stride[console_width];
			line_stride[console_width]= '\0';
			
			BLF_position(*x, *y, 0); (*y) += lheight;
			BLF_draw(line_stride);
			
			line_stride[console_width] = eol; /* restore */
			
			/* check if were out of view bounds */
			if(*y > ymax)
				return 0;
		}
	}
	else { /* simple, no wrap */

		if(bg) {
			glColor3ub(bg[0], bg[1], bg[2]);
			glRecti(0, *y-rct_ofs, winx, *y+lheight-rct_ofs);
		}

		glColor3ub(fg[0], fg[1], fg[2]);

		BLF_position(*x, *y, 0); (*y) += lheight;
		BLF_draw(str);
		
		if(*y > ymax)
			return 0;
	}

	return 1;
}

#define CONSOLE_DRAW_MARGIN 4
#define CONSOLE_DRAW_SCROLL 16

static int console_text_main__internal(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports, int draw, int mouse_y, void **mouse_pick)
{
	View2D *v2d= &ar->v2d;

	ConsoleLine *cl= sc->history.last;
	
	int x_orig=CONSOLE_DRAW_MARGIN, y_orig=CONSOLE_DRAW_MARGIN;
	int x,y, y_prev;
	int cwidth;
	int console_width; /* number of characters that fit into the width of the console (fixed width) */
	unsigned char fg[3];
	
	console_font_begin(sc);
	cwidth = BLF_fixed_width();
	
	console_width= (ar->winx - (CONSOLE_DRAW_SCROLL + CONSOLE_DRAW_MARGIN*2) )/cwidth;
	if (console_width < 8) console_width= 8;
	
	x= x_orig; y= y_orig;
	
	if(mouse_y != INT_MAX)
		mouse_y += (v2d->cur.ymin+CONSOLE_DRAW_MARGIN);


	if(sc->type==CONSOLE_TYPE_PYTHON) {
		int prompt_len;
		
		/* text */
		if(draw) {
			prompt_len= strlen(sc->prompt);
			console_line_color(fg, CONSOLE_LINE_INPUT);
			glColor3ub(fg[0], fg[1], fg[2]);

			/* command line */
			if(prompt_len) {
				BLF_position(x, y, 0); x += cwidth * prompt_len;
				BLF_draw(sc->prompt);
			}
			BLF_position(x, y, 0);
			BLF_draw(cl->line);

			/* cursor */
			console_line_color(fg, CONSOLE_LINE_ERROR); /* lazy */
			glColor3ub(fg[0], fg[1], fg[2]);
			glRecti(x+(cwidth*cl->cursor) -1, y-2, x+(cwidth*cl->cursor) +1, y+sc->lheight-2);

			x= x_orig; /* remove prompt offset */
		}
		
		y += sc->lheight;
		
		for(cl= sc->scrollback.last; cl; cl= cl->prev) {
			y_prev= y;

			if(draw)
				console_line_color(fg, cl->type);

			if(!console_draw_string(	cl->line, cl->len,
										console_width, sc->lheight,
										fg, NULL,
										ar->winx-(CONSOLE_DRAW_MARGIN+CONSOLE_DRAW_SCROLL),
										v2d->cur.ymin, v2d->cur.ymax,
										&x, &y, draw))
			{
				/* when drawing, if we pass v2d->cur.ymax, then quit */
				if(draw) {
					break; /* past the y limits */
				}
			}

			if((mouse_y != INT_MAX) && (mouse_y >= y_prev && mouse_y <= y)) {
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
				y_prev= y;

				if(draw)
					console_report_color(fg, bg, report, bool);

				if(!console_draw_string(	report->message, report->len,
											console_width, sc->lheight,
											fg, bg,
											ar->winx-(CONSOLE_DRAW_MARGIN+CONSOLE_DRAW_SCROLL),
											v2d->cur.ymin, v2d->cur.ymax,
											&x, &y, draw))
				{
					/* when drawing, if we pass v2d->cur.ymax, then quit */
					if(draw) {
						break; /* past the y limits */
					}
				}
				if((mouse_y != INT_MAX) && (mouse_y >= y_prev && mouse_y <= y)) {
					*mouse_pick= (void *)report;
					break;
				}

				bool = !(bool);
			}
		}
	}
	y += sc->lheight*2;

	
	return y-y_orig;
}

void console_text_main(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports)
{
	console_text_main__internal(sc, ar, reports, 1,  INT_MAX, NULL);
}

int console_text_height(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports)
{
	return console_text_main__internal(sc, ar, reports, 0,  INT_MAX, NULL);
}

void *console_text_pick(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports, int mouse_y)
{
	void *mouse_pick= NULL;
	console_text_main__internal(sc, ar, reports, 0, mouse_y, &mouse_pick);
	return (void *)mouse_pick;
}
