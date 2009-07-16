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
#include "UI_interface.h"
#include "UI_resources.h"

//#include "console_intern.h"

static void console_font_begin(SpaceConsole *sc)
{
	static int mono= -1; // XXX needs proper storage

	if(mono == -1)
		mono= BLF_load_mem("monospace", (unsigned char*)datatoc_bmonofont_ttf, datatoc_bmonofont_ttf_size);

	BLF_set(mono);
	BLF_aspect(1.0);

	BLF_size(sc->lheight, 72);
}

static void console_line_color(int type)
{
	switch(type){
	case CONSOLE_LINE_OUTPUT:
		glColor4ub(96, 128, 255, 255);
		break;
	case CONSOLE_LINE_INPUT:
		glColor4ub(255, 255, 255, 255);
		break;
	case CONSOLE_LINE_INFO:
		glColor4ub(0, 170, 0, 255);
		break;
	case CONSOLE_LINE_ERROR:
		glColor4ub(220, 96, 96, 255);
		break;
	}
}

static void console_report_color(int type)
{
	if(type & RPT_ERROR_ALL)	return glColor4ub(220, 0, 0, 255);
	if(type & RPT_WARNING_ALL)	return glColor4ub(220, 96, 96, 255);
	if(type & RPT_OPERATOR_ALL)	return glColor4ub(96, 128, 255, 255);
	if(type & RPT_INFO_ALL)		return glColor4ub(0, 170, 0, 255); 
	if(type & RPT_DEBUG_ALL)	return glColor4ub(196, 196, 196, 255);
	return glColor4ub(196, 196, 196, 255); /* unknown */
}


/* return 0 if the last line is off the screen
 * should be able to use this for any string type */
static int console_draw_string(char *str, int str_len, int console_width, int lheight, int ymax, int *x, int *y)
{	
	if(str_len > console_width) { /* wrap? */
		int tot_lines = (str_len/console_width)+1;							/* total number of lines for wrapping */
		char *line_stride= str + ((tot_lines-1) * console_width);	/* advance to the last line and draw it first */
		char eol;															/* baclup the end of wrapping */
		
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
		BLF_position(*x, *y, 0); (*y) += lheight;
		BLF_draw(str);
		
		if(*y > ymax)
			return 0;
	}

	return 1;
}

#define CONSOLE_DRAW_MARGIN 8

void console_text_main(struct SpaceConsole *sc, struct ARegion *ar, ReportList *reports)
{
	ConsoleLine *cl= sc->history.last;
	
	int x_orig=CONSOLE_DRAW_MARGIN, y_orig=CONSOLE_DRAW_MARGIN;
	int x,y;
	int cwidth;
	int console_width; /* number of characters that fit into the width of the console (fixed width) */
	
	
	console_font_begin(sc);
	cwidth = BLF_fixed_width();
	
	console_width= (ar->winx - CONSOLE_DRAW_MARGIN*2)/cwidth;
	if (console_width < 8) console_width= 8;
	
	x= x_orig; y= y_orig;
	
	if(sc->type==CONSOLE_TYPE_PYTHON) {
		int prompt_len= strlen(sc->prompt);
		
		/* text */
		console_line_color(CONSOLE_LINE_INPUT);
		
		/* command line */
		if(prompt_len) {
			BLF_position(x, y, 0); x += cwidth * prompt_len;
			BLF_draw(sc->prompt);	
		}
		BLF_position(x, y, 0);
		BLF_draw(cl->line);	
		
		/* cursor */
		console_line_color(CONSOLE_LINE_ERROR); /* lazy */
		glRecti(x+(cwidth*cl->cursor) -1, y-2, x+(cwidth*cl->cursor) +1, y+sc->lheight-2);
		
		x= x_orig; /* remove prompt offset */
		
		y += sc->lheight;
		
		for(cl= sc->scrollback.last; cl; cl= cl->prev) {
			console_line_color(cl->type);

			if(!console_draw_string(cl->line, cl->len, console_width, sc->lheight, ar->winy + sc->lheight, &x, &y))
				break; /* past the y limits */
			
		}
	}
	else { 
		Report *report;
		int report_mask= 0;
		
		/* convert our display toggles into a flag compatible with BKE_report flags */
		if(sc->rpt_mask & CONSOLE_RPT_DEBUG)	report_mask |= RPT_DEBUG_ALL;
		if(sc->rpt_mask & CONSOLE_RPT_INFO)	report_mask |= RPT_INFO_ALL;
		if(sc->rpt_mask & CONSOLE_RPT_OP)		report_mask |= RPT_OPERATOR_ALL;
		if(sc->rpt_mask & CONSOLE_RPT_WARN)	report_mask |= RPT_WARNING_ALL;
		if(sc->rpt_mask & CONSOLE_RPT_ERR)		report_mask |= RPT_ERROR_ALL;
		
		for(report=reports->list.last; report; report=report->prev) {
			
			if(report->type & report_mask) {
				console_report_color(report->type);
				if(!console_draw_string(report->message, strlen(report->message), console_width, sc->lheight, ar->winy + sc->lheight, &x, &y))
					break; /* past the y limits */
			}
			
		}
	}
	
}
