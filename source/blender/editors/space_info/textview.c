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

#include "BLF_api.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"



#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_datafiles.h"

#include "textview.h"

static void console_font_begin(TextViewContext *sc)
{
	BLF_size(blf_mono_font, sc->lheight-2, 72);
}

typedef struct ConsoleDrawContext {
	int cwidth;
	int lheight;
	int console_width; /* number of characters that fit into the width of the console (fixed width) */
	int winx;
	int ymin, ymax;
	int *xy; // [2]
	int *sel; // [2]
	int *pos_pick; // bottom of view == 0, top of file == combine chars, end of line is lower then start. 
	int *mval; // [2]
	int draw;
} ConsoleDrawContext;

static void console_draw_sel(int sel[2], int xy[2], int str_len_draw, int cwidth, int lheight)
{
	if(sel[0] <= str_len_draw && sel[1] >= 0) {
		int sta = MAX2(sel[0], 0);
		int end = MIN2(sel[1], str_len_draw);

		glEnable(GL_POLYGON_STIPPLE);
		glPolygonStipple(stipple_halftone);
		glEnable( GL_BLEND );
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4ub(255, 255, 255, 96);

		glRecti(xy[0]+(cwidth*sta), xy[1]-2 + lheight, xy[0]+(cwidth*end), xy[1]-2);

		glDisable(GL_POLYGON_STIPPLE);
		glDisable( GL_BLEND );
	}
}


/* return 0 if the last line is off the screen
 * should be able to use this for any string type */

static int console_draw_string(ConsoleDrawContext *cdc, const char *str, int str_len, unsigned char *fg, unsigned char *bg)
{
#define STEP_SEL(value) cdc->sel[0] += (value); cdc->sel[1] += (value)
	int rct_ofs= cdc->lheight/4;
	int tot_lines = (str_len/cdc->console_width)+1; /* total number of lines for wrapping */
	int y_next = (str_len > cdc->console_width) ? cdc->xy[1]+cdc->lheight*tot_lines : cdc->xy[1]+cdc->lheight;
	const int mono= blf_mono_font;

	/* just advance the height */
	if(cdc->draw==0) {
		if(cdc->pos_pick && (cdc->mval[1] != INT_MAX)) {
			if(cdc->xy[1] <= cdc->mval[1]) {
				if((y_next >= cdc->mval[1])) {
					int ofs = (int)floor(((float)cdc->mval[0] / (float)cdc->cwidth));

					/* wrap */
					if(str_len > cdc->console_width)
						ofs += (cdc->console_width * ((int)((((float)(y_next - cdc->mval[1]) / (float)(y_next-cdc->xy[1])) * tot_lines))));
	
					CLAMP(ofs, 0, str_len);
					*cdc->pos_pick += str_len - ofs;
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
			STEP_SEL(-(str_len + 1));
		}

		return 1;
	}

	if(str_len > cdc->console_width) { /* wrap? */
		const int initial_offset= ((tot_lines-1) * cdc->console_width);
		const char *line_stride= str + initial_offset;	/* advance to the last line and draw it first */
		
		int sel_orig[2];
		copy_v2_v2_int(sel_orig, cdc->sel);

		/* invert and swap for wrapping */
		cdc->sel[0] = str_len - sel_orig[1];
		cdc->sel[1] = str_len - sel_orig[0];
		
		if(bg) {
			glColor3ubv(bg);
			glRecti(0, cdc->xy[1]-rct_ofs, cdc->winx, (cdc->xy[1]+(cdc->lheight*tot_lines))+rct_ofs);
		}

		glColor3ubv(fg);

		/* last part needs no clipping */
		BLF_position(mono, cdc->xy[0], cdc->xy[1], 0);
		BLF_draw(mono, line_stride, str_len - initial_offset);

		if(cdc->sel[0] != cdc->sel[1]) {
			STEP_SEL(-initial_offset);
			// glColor4ub(255, 0, 0, 96); // debug
			console_draw_sel(cdc->sel, cdc->xy, str_len % cdc->console_width, cdc->cwidth, cdc->lheight);
			STEP_SEL(cdc->console_width);
			glColor3ubv(fg);
		}

		cdc->xy[1] += cdc->lheight;

		line_stride -= cdc->console_width;
		
		for(; line_stride >= str; line_stride -= cdc->console_width) {
			BLF_position(mono, cdc->xy[0], cdc->xy[1], 0);
			BLF_draw(mono, line_stride, cdc->console_width);
			
			if(cdc->sel[0] != cdc->sel[1]) {
				// glColor4ub(0, 255, 0, 96); // debug
				console_draw_sel(cdc->sel, cdc->xy, cdc->console_width, cdc->cwidth, cdc->lheight);
				STEP_SEL(cdc->console_width);
				glColor3ubv(fg);
			}

			cdc->xy[1] += cdc->lheight;
			
			/* check if were out of view bounds */
			if(cdc->xy[1] > cdc->ymax)
				return 0;
		}

		copy_v2_v2_int(cdc->sel, sel_orig);
		STEP_SEL(-(str_len + 1));
	}
	else { /* simple, no wrap */

		if(bg) {
			glColor3ubv(bg);
			glRecti(0, cdc->xy[1]-rct_ofs, cdc->winx, cdc->xy[1]+cdc->lheight-rct_ofs);
		}

		glColor3ubv(fg);

		BLF_position(mono, cdc->xy[0], cdc->xy[1], 0);
		BLF_draw(mono, str, str_len);
		
		if(cdc->sel[0] != cdc->sel[1]) {
			int isel[2];

			isel[0]= str_len - cdc->sel[1];
			isel[1]= str_len - cdc->sel[0];

			// glColor4ub(255, 255, 0, 96); // debug
			console_draw_sel(isel, cdc->xy, str_len, cdc->cwidth, cdc->lheight);
			STEP_SEL(-(str_len + 1));
		}

		cdc->xy[1] += cdc->lheight;

		if(cdc->xy[1] > cdc->ymax)
			return 0;
	}

	return 1;
#undef STEP_SEL
}

#define CONSOLE_DRAW_MARGIN 4
#define CONSOLE_DRAW_SCROLL 16

int textview_draw(TextViewContext *tvc, int draw, int mval[2], void **mouse_pick, int *pos_pick)
{
	ConsoleDrawContext cdc= {0};

	int x_orig=CONSOLE_DRAW_MARGIN, y_orig=CONSOLE_DRAW_MARGIN + tvc->lheight/6;
	int xy[2], y_prev;
	int sel[2]= {-1, -1}; /* defaults disabled */
	unsigned char fg[3], bg[3];
	const int mono= blf_mono_font;

	console_font_begin(tvc);

	xy[0]= x_orig; xy[1]= y_orig;

	if(mval[1] != INT_MAX)
		mval[1] += (tvc->ymin + CONSOLE_DRAW_MARGIN);

	if(pos_pick)
		*pos_pick = 0;

	/* constants for the sequencer context */
	cdc.cwidth= (int)BLF_fixed_width(mono);
	assert(cdc.cwidth > 0);
	cdc.lheight= tvc->lheight;
	cdc.console_width= (tvc->winx - (CONSOLE_DRAW_SCROLL + CONSOLE_DRAW_MARGIN*2) ) / cdc.cwidth;
	CLAMP(cdc.console_width, 1, INT_MAX); /* avoid divide by zero on small windows */
	cdc.winx= tvc->winx-(CONSOLE_DRAW_MARGIN+CONSOLE_DRAW_SCROLL);
	cdc.ymin = tvc->ymin;
	cdc.ymax = tvc->ymax;
	cdc.xy= xy;
	cdc.sel= sel;
	cdc.pos_pick= pos_pick;
	cdc.mval= mval;
	cdc.draw= draw;

	/* shouldnt be needed */
	tvc->cwidth= cdc.cwidth;
	tvc->console_width= cdc.console_width;
	tvc->iter_index= 0;

	if(tvc->sel_start != tvc->sel_end) {
		sel[0]= tvc->sel_start;
		sel[1]= tvc->sel_end;
	}

	if(tvc->begin(tvc)) {

		do {
			const char *ext_line;
			int ext_len;
			int color_flag= 0;

			y_prev= xy[1];

			if(draw)
				color_flag= tvc->line_color(tvc, fg, bg);

			tvc->line_get(tvc, &ext_line, &ext_len);

			if(!console_draw_string(&cdc, ext_line, ext_len, (color_flag & TVC_LINE_FG) ? fg : NULL, (color_flag & TVC_LINE_BG) ? bg : NULL)) {
				/* when drawing, if we pass v2d->cur.ymax, then quit */
				if(draw) {
					break; /* past the y limits */
				}
			}

			if((mval[1] != INT_MAX) && (mval[1] >= y_prev && mval[1] <= xy[1])) {
				*mouse_pick= (void *)tvc->iter;
				break;
			}

			tvc->iter_index++;

		} while(tvc->step(tvc));
	}

	tvc->end(tvc);

	xy[1] += tvc->lheight * 2;

	return xy[1] - y_orig;
}
