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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_text/text_draw.c
 *  \ingroup sptext
 */



#include "MEM_guardedalloc.h"

#include "BLF_api.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_suggestions.h"
#include "BKE_text.h"

#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "text_intern.h"
#include "text_format.h"

/******************** text font drawing ******************/
// XXX, fixme
#define mono blf_mono_font

static void text_font_begin(SpaceText *st)
{
	BLF_size(mono, st->lheight_dpi, 72);
}

static void text_font_end(SpaceText *UNUSED(st))
{
}

static int text_font_draw(SpaceText *st, int x, int y, const char *str)
{
	int columns;

	BLF_position(mono, x, y, 0);
	columns = BLF_draw_mono(mono, str, BLF_DRAW_STR_DUMMY_MAX, st->cwidth);

	return st->cwidth * columns;
}

static int text_font_draw_character(SpaceText *st, int x, int y, char c)
{
	BLF_position(mono, x, y, 0);
	BLF_draw(mono, &c, 1);

	return st->cwidth;
}

static int text_font_draw_character_utf8(SpaceText *st, int x, int y, const char *c)
{
	int columns;

	const size_t len = BLI_str_utf8_size_safe(c);
	BLF_position(mono, x, y, 0);
	columns = BLF_draw_mono(mono, c, len, st->cwidth);

	return st->cwidth * columns;
}

#if 0
/* Formats every line of the current text */
static void txt_format_text(SpaceText *st) 
{
	TextLine *linep;

	if (!st->text) return;

	for (linep = st->text->lines.first; linep; linep = linep->next)
		txt_format_line(st, linep, 0);
}
#endif

/* Sets the current drawing color based on the format character specified */
static void format_draw_color(char formatchar)
{
	switch (formatchar) {
		case FMT_TYPE_WHITESPACE:
			break;
		case FMT_TYPE_SYMBOL:
			UI_ThemeColor(TH_SYNTAX_S);
			break;
		case FMT_TYPE_COMMENT:
			UI_ThemeColor(TH_SYNTAX_C);
			break;
		case FMT_TYPE_NUMERAL:
			UI_ThemeColor(TH_SYNTAX_N);
			break;
		case FMT_TYPE_STRING:
			UI_ThemeColor(TH_SYNTAX_L);
			break;
		case FMT_TYPE_DIRECTIVE:
			UI_ThemeColor(TH_SYNTAX_D);
			break;
		case FMT_TYPE_SPECIAL:
			UI_ThemeColor(TH_SYNTAX_V);
			break;
		case FMT_TYPE_RESERVED:
			UI_ThemeColor(TH_SYNTAX_R);
			break;
		case FMT_TYPE_KEYWORD:
			UI_ThemeColor(TH_SYNTAX_B);
			break;
		case FMT_TYPE_DEFAULT:
		default:
			UI_ThemeColor(TH_TEXT);
			break;
	}
}

/************************** draw text *****************************/

/* Notes on word-wrap
 * --
 * All word-wrap functions follow the algorithm below to maintain consistency.
 *     line        The line to wrap (tabs converted to spaces)
 *     view_width    The maximum number of characters displayable in the region
 *                 This equals region_width/font_width for the region
 *     wrap_chars    Characters that allow wrapping. This equals [' ', '\t', '-']
 * 
 * def wrap(line, view_width, wrap_chars):
 *     draw_start = 0
 *     draw_end = view_width
 *     pos = 0
 *     for c in line:
 *         if pos-draw_start >= view_width:
 *             print line[draw_start:draw_end]
 *             draw_start = draw_end
 *             draw_end += view_width
 *         elif c in wrap_chars:
 *             draw_end = pos+1
 *         pos += 1
 *     print line[draw_start:]
 * 
 */

int wrap_width(SpaceText *st, ARegion *ar)
{
	int winx = ar->winx - TXT_SCROLL_WIDTH;
	int x, max;
	
	x = st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
	max = st->cwidth ? (winx - x) / st->cwidth : 0;
	return max > 8 ? max : 8;
}

/* Sets (offl, offc) for transforming (line, curs) to its wrapped position */
void wrap_offset(SpaceText *st, ARegion *ar, TextLine *linein, int cursin, int *offl, int *offc)
{
	Text *text;
	TextLine *linep;
	int i, j, start, end, max, chop;
	char ch;

	*offl = *offc = 0;

	if (!st->text) return;
	if (!st->wordwrap) return;

	text = st->text;

	/* Move pointer to first visible line (top) */
	linep = text->lines.first;
	i = st->top;
	while (i > 0 && linep) {
		int lines = text_get_visible_lines(st, ar, linep->line);

		/* Line before top */
		if (linep == linein) {
			if (lines <= i)
				/* no visible part of line */
				return;
		}

		if (i - lines < 0) {
			break;
		}
		else {
			linep = linep->next;
			(*offl) += lines - 1;
			i -= lines;
		}
	}

	max = wrap_width(st, ar);
	cursin = txt_utf8_offset_to_column(linein->line, cursin);

	while (linep) {
		start = 0;
		end = max;
		chop = 1;
		*offc = 0;
		for (i = 0, j = 0; linep->line[j]; j += BLI_str_utf8_size_safe(linep->line + j)) {
			int chars;
			int columns = BLI_str_utf8_char_width_safe(linep->line + j); /* = 1 for tab */

			/* Mimic replacement of tabs */
			ch = linep->line[j];
			if (ch == '\t') {
				chars = st->tabnumber - i % st->tabnumber;
				if (linep == linein && i < cursin) cursin += chars - 1;
				ch = ' ';
			}
			else {
				chars = 1;
			}

			while (chars--) {
				if (i + columns - start > max) {
					end = MIN2(end, i);

					if (chop && linep == linein && i >= cursin) {
						if (i == cursin) {
							(*offl)++;
							*offc -= end - start;
						}

						return;
					}

					(*offl)++;
					*offc -= end - start;

					start = end;
					end += max;
					chop = 1;
				}
				else if (ch == ' ' || ch == '-') {
					end = i + 1;
					chop = 0;
					if (linep == linein && i >= cursin)
						return;
				}
				i += columns;
			}
		}
		if (linep == linein) break;
		linep = linep->next;
	}
}

/* cursin - mem, offc - view */
void wrap_offset_in_line(SpaceText *st, ARegion *ar, TextLine *linein, int cursin, int *offl, int *offc)
{
	int i, j, start, end, chars, max, chop;
	char ch;

	*offl = *offc = 0;

	if (!st->text) return;
	if (!st->wordwrap) return;

	max = wrap_width(st, ar);

	start = 0;
	end = max;
	chop = 1;
	*offc = 0;
	cursin = txt_utf8_offset_to_column(linein->line, cursin);

	for (i = 0, j = 0; linein->line[j]; j += BLI_str_utf8_size_safe(linein->line + j)) {
		int columns = BLI_str_utf8_char_width_safe(linein->line + j); /* = 1 for tab */

		/* Mimic replacement of tabs */
		ch = linein->line[j];
		if (ch == '\t') {
			chars = st->tabnumber - i % st->tabnumber;
			if (i < cursin) cursin += chars - 1;
			ch = ' ';
		}
		else
			chars = 1;

		while (chars--) {
			if (i + columns - start > max) {
				end = MIN2(end, i);

				if (chop && i >= cursin) {
					if (i == cursin) {
						(*offl)++;
						*offc -= end - start;
					}

					return;
				}

				(*offl)++;
				*offc -= end - start;

				start = end;
				end += max;
				chop = 1;
			}
			else if (ch == ' ' || ch == '-') {
				end = i + 1;
				chop = 0;
				if (i >= cursin)
					return;
			}
			i += columns;
		}
	}
}

int text_get_char_pos(SpaceText *st, const char *line, int cur)
{
	int a = 0, i;
	
	for (i = 0; i < cur && line[i]; i += BLI_str_utf8_size_safe(line + i)) {
		if (line[i] == '\t')
			a += st->tabnumber - a % st->tabnumber;
		else
			a += BLI_str_utf8_char_width_safe(line + i);
	}
	return a;
}

static const char *txt_utf8_forward_columns(const char *str, int columns, int *padding)
{
	int col;
	const char *p = str;
	while (*p) {
		col = BLI_str_utf8_char_width(p);
		if (columns - col < 0)
			break;
		columns -= col;
		p += BLI_str_utf8_size_safe(p);
		if (columns == 0)
			break;
	}
	if (padding)
		*padding = *p ? columns : 0;
	return p;
}

static int text_draw_wrapped(SpaceText *st, const char *str, int x, int y, int w, const char *format, int skip)
{
	FlattenString fs;
	int basex, lines;
	int i, wrap, end, max, columns, padding; /* column */
	int a, fstart, fpos;                     /* utf8 chars */
	int mi, ma, mstart, mend;                /* mem */
	char fmt_prev = 0xff;
	
	flatten_string(st, &fs, str);
	str = fs.buf;
	max = w / st->cwidth;
	if (max < 8) max = 8;
	basex = x;
	lines = 1;
	
	fpos = fstart = 0; mstart = 0;
	mend = txt_utf8_forward_columns(str, max, &padding) - str;
	end = wrap = max - padding;
	
	for (i = 0, mi = 0; str[mi]; i += columns, mi += BLI_str_utf8_size_safe(str + mi)) {
		columns = BLI_str_utf8_char_width_safe(str + mi);
		if (i + columns > end) {
			/* skip hidden part of line */
			if (skip) {
				skip--;
				fstart = fpos; mstart = mend;
				mend = txt_utf8_forward_columns(str + mend, max, &padding) - str;
				end = (wrap += max - padding);
				continue;
			}

			/* Draw the visible portion of text on the overshot line */
			for (a = fstart, ma = mstart; ma < mend; a++, ma += BLI_str_utf8_size_safe(str + ma)) {
				if (st->showsyntax && format) {
					if (fmt_prev != format[a]) format_draw_color(fmt_prev = format[a]);
				}
				x += text_font_draw_character_utf8(st, x, y, str + ma);
				fpos++;
			}
			y -= st->lheight_dpi + TXT_LINE_SPACING;
			x = basex;
			lines++;
			fstart = fpos; mstart = mend;
			mend = txt_utf8_forward_columns(str + mend, max, &padding) - str;
			end = (wrap += max - padding);

			if (y <= 0) break;
		}
		else if (str[mi] == ' ' || str[mi] == '-') {
			wrap = i + 1; mend = mi + 1;
		}
	}

	/* Draw the remaining text */
	for (a = fstart, ma = mstart; str[ma] && y > 0; a++, ma += BLI_str_utf8_size_safe(str + ma)) {
		if (st->showsyntax && format) {
			if (fmt_prev != format[a]) format_draw_color(fmt_prev = format[a]);
		}

		x += text_font_draw_character_utf8(st, x, y, str + ma);
	}

	flatten_string_free(&fs);

	return lines;
}

static void text_draw(SpaceText *st, char *str, int cshift, int maxwidth, int x, int y, const char *format)
{
	FlattenString fs;
	int columns, size, n, w = 0, padding, amount = 0;
	const char *in = NULL;

	for (n = flatten_string(st, &fs, str), str = fs.buf; n > 0; n--) {
		columns = BLI_str_utf8_char_width_safe(str);
		size = BLI_str_utf8_size_safe(str);

		if (!in) {
			if (w >= cshift) {
				padding = w - cshift;
				in = str;
			}
			else if (format)
				format++;
		}
		if (in) {
			if (maxwidth && w + columns > cshift + maxwidth)
				break;
			amount++;
		}

		w += columns;
		str += size;
	}
	if (!in) {
		flatten_string_free(&fs);
		return; /* String is shorter than shift or ends with a padding */
	}

	x += st->cwidth * padding;

	if (st->showsyntax && format) {
		int a, str_shift = 0;
		char fmt_prev = 0xff;

		for (a = 0; a < amount; a++) {
			if (format[a] != fmt_prev) format_draw_color(fmt_prev = format[a]);
			x += text_font_draw_character_utf8(st, x, y, in + str_shift);
			str_shift += BLI_str_utf8_size_safe(in + str_shift);
		}
	}
	else {
		text_font_draw(st, x, y, in);
	}

	flatten_string_free(&fs);
}

/************************ cache utilities *****************************/

typedef struct DrawCache {
	int *line_height;
	int total_lines, nlines;

	/* this is needed to check cache relevance */
	int winx, wordwrap, showlinenrs, tabnumber;
	short lheight;
	char cwidth;
	char text_id[MAX_ID_NAME];

	/* for partial lines recalculation */
	short update_flag;
	int valid_head, valid_tail; /* amount of unchanged lines */
} DrawCache;

static void text_drawcache_init(SpaceText *st)
{
	DrawCache *drawcache = MEM_callocN(sizeof(DrawCache), "text draw cache");

	drawcache->winx = -1;
	drawcache->nlines = BLI_countlist(&st->text->lines);
	drawcache->text_id[0] = '\0';

	st->drawcache = drawcache;
}

static void text_update_drawcache(SpaceText *st, ARegion *ar)
{
	DrawCache *drawcache;
	int full_update = 0, nlines = 0;
	Text *txt = st->text;

	if (!st->drawcache) text_drawcache_init(st);

	text_update_character_width(st);

	drawcache = (DrawCache *)st->drawcache;
	nlines = drawcache->nlines;

	/* check if full cache update is needed */
	full_update |= drawcache->winx != ar->winx;               /* area was resized */
	full_update |= drawcache->wordwrap != st->wordwrap;       /* word-wrapping option was toggled */
	full_update |= drawcache->showlinenrs != st->showlinenrs; /* word-wrapping option was toggled */
	full_update |= drawcache->tabnumber != st->tabnumber;     /* word-wrapping option was toggled */
	full_update |= drawcache->lheight != st->lheight_dpi;         /* word-wrapping option was toggled */
	full_update |= drawcache->cwidth != st->cwidth;           /* word-wrapping option was toggled */
	full_update |= strncmp(drawcache->text_id, txt->id.name, MAX_ID_NAME); /* text datablock was changed */

	if (st->wordwrap) {
		/* update line heights */
		if (full_update || !drawcache->line_height) {
			drawcache->valid_head  = 0;
			drawcache->valid_tail  = 0;
			drawcache->update_flag = 1;
		}

		if (drawcache->update_flag) {
			TextLine *line = st->text->lines.first;
			int lineno = 0, size, lines_count;
			int *fp = drawcache->line_height, *new_tail, *old_tail;

			nlines = BLI_countlist(&txt->lines);
			size = sizeof(int) * nlines;

			if (fp) fp = MEM_reallocN(fp, size);
			else fp = MEM_callocN(size, "text drawcache line_height");

			drawcache->valid_tail = drawcache->valid_head = 0;
			old_tail = fp + drawcache->nlines - drawcache->valid_tail;
			new_tail = fp + nlines - drawcache->valid_tail;
			memmove(new_tail, old_tail, drawcache->valid_tail);

			drawcache->total_lines = 0;

			if (st->showlinenrs)
				st->linenrs_tot = (int)floor(log10((float)nlines)) + 1;

			while (line) {
				if (drawcache->valid_head) { /* we're inside valid head lines */
					lines_count = fp[lineno];
					drawcache->valid_head--;
				}
				else if (lineno > new_tail - fp) {  /* we-re inside valid tail lines */
					lines_count = fp[lineno];
				}
				else {
					lines_count = text_get_visible_lines(st, ar, line->line);
				}

				fp[lineno] = lines_count;

				line = line->next;
				lineno++;
				drawcache->total_lines += lines_count;
			}

			drawcache->line_height = fp;
		}
	}
	else {
		if (drawcache->line_height) {
			MEM_freeN(drawcache->line_height);
			drawcache->line_height = NULL;
		}

		if (full_update || drawcache->update_flag) {
			nlines = BLI_countlist(&txt->lines);

			if (st->showlinenrs)
				st->linenrs_tot = (int)floor(log10((float)nlines)) + 1;
		}

		drawcache->total_lines = nlines;
	}

	drawcache->nlines = nlines;

	/* store settings */
	drawcache->winx        = ar->winx;
	drawcache->wordwrap    = st->wordwrap;
	drawcache->lheight     = st->lheight_dpi;
	drawcache->cwidth      = st->cwidth;
	drawcache->showlinenrs = st->showlinenrs;
	drawcache->tabnumber   = st->tabnumber;

	strncpy(drawcache->text_id, txt->id.name, MAX_ID_NAME);

	/* clear update flag */
	drawcache->update_flag = 0;
	drawcache->valid_head  = 0;
	drawcache->valid_tail  = 0;
}

void text_drawcache_tag_update(SpaceText *st, int full)
{
	/* this happens if text editor ops are caled from python */
	if (st == NULL)
		return;
		
	if (st->drawcache) {
		DrawCache *drawcache = (DrawCache *)st->drawcache;
		Text *txt = st->text;

		if (drawcache->update_flag) {
			/* happens when tagging update from space listener */
			/* should do nothing to prevent locally tagged cache be fully recalculated */
			return;
		}

		if (!full) {
			int sellno = BLI_findindex(&txt->lines, txt->sell);
			int curlno = BLI_findindex(&txt->lines, txt->curl);

			if (curlno < sellno) {
				drawcache->valid_head = curlno;
				drawcache->valid_tail = drawcache->nlines - sellno - 1;
			}
			else {
				drawcache->valid_head = sellno;
				drawcache->valid_tail = drawcache->nlines - curlno - 1;
			}

			/* quick cache recalculation is also used in delete operator,
			 * which could merge lines which are adjacent to current selection lines
			 * expand recalculate area to this lines */
			if (drawcache->valid_head > 0) drawcache->valid_head--;
			if (drawcache->valid_tail > 0) drawcache->valid_tail--;
		}
		else {
			drawcache->valid_head = 0;
			drawcache->valid_tail = 0;
		}

		drawcache->update_flag = 1;
	}
}

void text_free_caches(SpaceText *st)
{
	DrawCache *drawcache = (DrawCache *)st->drawcache;

	if (drawcache) {
		if (drawcache->line_height)
			MEM_freeN(drawcache->line_height);

		MEM_freeN(drawcache);
	}
}

/************************ word-wrap utilities *****************************/

/* cache should be updated in caller */
static int text_get_visible_lines_no(SpaceText *st, int lineno)
{
	DrawCache *drawcache = (DrawCache *)st->drawcache;

	return drawcache->line_height[lineno];
}

int text_get_visible_lines(SpaceText *st, ARegion *ar, const char *str)
{
	int i, j, start, end, max, lines, chars;
	char ch;

	max = wrap_width(st, ar);
	lines = 1;
	start = 0;
	end = max;
	for (i = 0, j = 0; str[j]; j += BLI_str_utf8_size_safe(str + j)) {
		int columns = BLI_str_utf8_char_width_safe(str + j); /* = 1 for tab */

		/* Mimic replacement of tabs */
		ch = str[j];
		if (ch == '\t') {
			chars = st->tabnumber - i % st->tabnumber;
			ch = ' ';
		}
		else {
			chars = 1;
		}

		while (chars--) {
			if (i + columns - start > max) {
				lines++;
				start = MIN2(end, i);
				end += max;
			}
			else if (ch == ' ' || ch == '-') {
				end = i + 1;
			}

			i += columns;
		}
	}

	return lines;
}

int text_get_span_wrap(SpaceText *st, ARegion *ar, TextLine *from, TextLine *to)
{
	if (st->wordwrap) {
		int ret = 0;
		TextLine *tmp = from;

		/* Look forwards */
		while (tmp) {
			if (tmp == to) return ret;
			ret += text_get_visible_lines(st, ar, tmp->line);
			tmp = tmp->next;
		}

		return ret;
	}
	else {
		return txt_get_span(from, to);
	}
}

int text_get_total_lines(SpaceText *st, ARegion *ar)
{
	DrawCache *drawcache;

	text_update_drawcache(st, ar);
	drawcache = (DrawCache *)st->drawcache;

	return drawcache->total_lines;
}

/************************ draw scrollbar *****************************/

static void calc_text_rcts(SpaceText *st, ARegion *ar, rcti *scroll, rcti *back)
{
	int lhlstart, lhlend, ltexth, sell_off, curl_off;
	short barheight, barstart, hlstart, hlend, blank_lines;
	short pix_available, pix_top_margin, pix_bottom_margin, pix_bardiff;

	pix_top_margin = 8;
	pix_bottom_margin = 4;
	pix_available = ar->winy - pix_top_margin - pix_bottom_margin;
	ltexth = text_get_total_lines(st, ar);
	blank_lines = st->viewlines / 2;
	
	/* nicer code: use scroll rect for entire bar */
	back->xmin = ar->winx - (V2D_SCROLL_WIDTH + 1);
	back->xmax = ar->winx;
	back->ymin = 0;
	back->ymax = ar->winy;
	
	scroll->xmin = ar->winx - V2D_SCROLL_WIDTH;
	scroll->xmax = ar->winx - 5;
	scroll->ymin = 4;
	scroll->ymax = 4 + pix_available;
	
	/* when re-sizing a view-port with the bar at the bottom to a greater height more blank lines will be added */
	if (ltexth + blank_lines < st->top + st->viewlines) {
		blank_lines = st->top + st->viewlines - ltexth;
	}
	
	ltexth += blank_lines;

	barheight = (ltexth > 0) ? (st->viewlines * pix_available) / ltexth : 0;
	pix_bardiff = 0;
	if (barheight < 20) {
		pix_bardiff = 20 - barheight; /* take into account the now non-linear sizing of the bar */
		barheight = 20;
	}
	barstart = (ltexth > 0) ? ((pix_available - pix_bardiff) * st->top) / ltexth : 0;

	st->txtbar = *scroll;
	st->txtbar.ymax -= barstart;
	st->txtbar.ymin = st->txtbar.ymax - barheight;

	CLAMP(st->txtbar.ymin, pix_bottom_margin, ar->winy - pix_top_margin);
	CLAMP(st->txtbar.ymax, pix_bottom_margin, ar->winy - pix_top_margin);

	st->pix_per_line = (pix_available > 0) ? (float) ltexth / pix_available : 0;
	if (st->pix_per_line < 0.1f) st->pix_per_line = 0.1f;

	curl_off = text_get_span_wrap(st, ar, st->text->lines.first, st->text->curl);
	sell_off = text_get_span_wrap(st, ar, st->text->lines.first, st->text->sell);
	lhlstart = MIN2(curl_off, sell_off);
	lhlend = MAX2(curl_off, sell_off);

	if (ltexth > 0) {
		hlstart = (lhlstart * pix_available) / ltexth;
		hlend = (lhlend * pix_available) / ltexth;

		/* the scrollbar is non-linear sized */
		if (pix_bardiff > 0) {
			/* the start of the highlight is in the current viewport */
			if (st->viewlines && lhlstart >= st->top && lhlstart <= st->top + st->viewlines) {
				/* speed the progresion of the start of the highlight through the scrollbar */
				hlstart = ( ( (pix_available - pix_bardiff) * lhlstart) / ltexth) + (pix_bardiff * (lhlstart - st->top) / st->viewlines);
			}
			else if (lhlstart > st->top + st->viewlines && hlstart < barstart + barheight && hlstart > barstart) {
				/* push hl start down */
				hlstart = barstart + barheight;
			}
			else if (lhlend > st->top && lhlstart < st->top && hlstart > barstart) {
				/*fill out start */
				hlstart = barstart;
			}

			if (hlend <= hlstart) {
				hlend = hlstart + 2;
			}

			/* the end of the highlight is in the current viewport */
			if (st->viewlines && lhlend >= st->top && lhlend <= st->top + st->viewlines) {
				/* speed the progresion of the end of the highlight through the scrollbar */
				hlend = (((pix_available - pix_bardiff) * lhlend) / ltexth) + (pix_bardiff * (lhlend - st->top) / st->viewlines);
			}
			else if (lhlend < st->top && hlend >= barstart - 2 && hlend < barstart + barheight) {
				/* push hl end up */
				hlend = barstart;
			}
			else if (lhlend > st->top + st->viewlines && lhlstart < st->top + st->viewlines && hlend < barstart + barheight) {
				/* fill out end */
				hlend = barstart + barheight;
			}

			if (hlend <= hlstart) {
				hlstart = hlend - 2;
			}
		}
	}
	else {
		hlstart = 0;
		hlend = 0;
	}

	if (hlend - hlstart < 2) {
		hlend = hlstart + 2;
	}
	
	st->txtscroll = *scroll;
	st->txtscroll.ymax = ar->winy - pix_top_margin - hlstart;
	st->txtscroll.ymin = ar->winy - pix_top_margin - hlend;

	CLAMP(st->txtscroll.ymin, pix_bottom_margin, ar->winy - pix_top_margin);
	CLAMP(st->txtscroll.ymax, pix_bottom_margin, ar->winy - pix_top_margin);
}

static void draw_textscroll(SpaceText *st, rcti *scroll, rcti *back)
{
	bTheme *btheme = UI_GetTheme();
	uiWidgetColors wcol = btheme->tui.wcol_scroll;
	unsigned char col[4];
	float rad;
	
	UI_ThemeColor(TH_BACK);
	glRecti(back->xmin, back->ymin, back->xmax, back->ymax);

	uiWidgetScrollDraw(&wcol, scroll, &st->txtbar, (st->flags & ST_SCROLL_SELECT) ? UI_SCROLL_PRESSED : 0);

	uiSetRoundBox(UI_CNR_ALL);
	rad = 0.4f * min_ii(BLI_rcti_size_x(&st->txtscroll), BLI_rcti_size_y(&st->txtscroll));
	UI_GetThemeColor3ubv(TH_HILITE, col);
	col[3] = 48;
	glColor4ubv(col);
	glEnable(GL_BLEND);
	uiRoundBox(st->txtscroll.xmin + 1, st->txtscroll.ymin, st->txtscroll.xmax - 1, st->txtscroll.ymax, rad);
	glDisable(GL_BLEND);
}

/*********************** draw documentation *******************************/

static void draw_documentation(SpaceText *st, ARegion *ar)
{
	TextLine *tmp;
	char *docs, buf[DOC_WIDTH + 1], *p;
	int i, br, lines;
	int boxw, boxh, l, x, y /* , top */ /* UNUSED */;
	
	if (!st || !st->text) return;
	if (!texttool_text_is_active(st->text)) return;
	
	docs = texttool_docs_get();

	if (!docs) return;

	/* Count the visible lines to the cursor */
	for (tmp = st->text->curl, l = -st->top; tmp; tmp = tmp->prev, l++) ;
	if (l < 0) return;
	
	if (st->showlinenrs) {
		x = st->cwidth * (st->text->curc - st->left) + TXT_OFFSET + TEXTXLOC - 4;
	}
	else {
		x = st->cwidth * (st->text->curc - st->left) + TXT_OFFSET - 4;
	}
	if (texttool_suggest_first()) {
		x += SUGG_LIST_WIDTH * st->cwidth + 50;
	}

	/* top = */ /* UNUSED */ y = ar->winy - st->lheight_dpi * l - 2;
	boxw = DOC_WIDTH * st->cwidth + 20;
	boxh = (DOC_HEIGHT + 1) * (st->lheight_dpi + TXT_LINE_SPACING);

	/* Draw panel */
	UI_ThemeColor(TH_BACK);
	glRecti(x, y, x + boxw, y - boxh);
	UI_ThemeColor(TH_SHADE1);
	glBegin(GL_LINE_LOOP);
	glVertex2i(x, y);
	glVertex2i(x + boxw, y);
	glVertex2i(x + boxw, y - boxh);
	glVertex2i(x, y - boxh);
	glEnd();
	glBegin(GL_LINE_LOOP);
	glVertex2i(x + boxw - 10, y - 7);
	glVertex2i(x + boxw - 4, y - 7);
	glVertex2i(x + boxw - 7, y - 2);
	glEnd();
	glBegin(GL_LINE_LOOP);
	glVertex2i(x + boxw - 10, y - boxh + 7);
	glVertex2i(x + boxw - 4, y - boxh + 7);
	glVertex2i(x + boxw - 7, y - boxh + 2);
	glEnd();
	UI_ThemeColor(TH_TEXT);

	i = 0; br = DOC_WIDTH; lines = 0; // XXX -doc_scroll;
	for (p = docs; *p; p++) {
		if (*p == '\r' && *(++p) != '\n') *(--p) = '\n';  /* Fix line endings */
		if (*p == ' ' || *p == '\t')
			br = i;
		else if (*p == '\n') {
			buf[i] = '\0';
			if (lines >= 0) {
				y -= st->lheight_dpi;
				text_draw(st, buf, 0, 0, x + 4, y - 3, NULL);
			}
			i = 0; br = DOC_WIDTH; lines++;
		}
		buf[i++] = *p;
		if (i == DOC_WIDTH) { /* Reached the width, go to last break and wrap there */
			buf[br] = '\0';
			if (lines >= 0) {
				y -= st->lheight_dpi;
				text_draw(st, buf, 0, 0, x + 4, y - 3, NULL);
			}
			p -= i - br - 1; /* Rewind pointer to last break */
			i = 0; br = DOC_WIDTH; lines++;
		}
		if (lines >= DOC_HEIGHT) break;
	}

	if (0 /* XXX doc_scroll*/ > 0 && lines < DOC_HEIGHT) {
		// XXX doc_scroll--;
		draw_documentation(st, ar);
	}
}

/*********************** draw suggestion list *******************************/

static void draw_suggestion_list(SpaceText *st, ARegion *ar)
{
	SuggItem *item, *first, *last, *sel;
	char str[SUGG_LIST_WIDTH * BLI_UTF8_MAX + 1];
	int offl, offc, vcurl, vcurc;
	int w, boxw = 0, boxh, i, x, y, *top;
	const int lheight = st->lheight_dpi + TXT_LINE_SPACING;
	const int margin_x = 2;
	
	if (!st->text) return;
	if (!texttool_text_is_active(st->text)) return;

	first = texttool_suggest_first();
	last = texttool_suggest_last();

	if (!first || !last) return;

	text_pop_suggest_list();
	sel = texttool_suggest_selected();
	top = texttool_suggest_top();

	wrap_offset(st, ar, st->text->curl, st->text->curc, &offl, &offc);
	vcurl = txt_get_span(st->text->lines.first, st->text->curl) - st->top + offl;
	vcurc = text_get_char_pos(st, st->text->curl->line, st->text->curc) - st->left + offc;

	x = st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
	x += vcurc * st->cwidth - 4;
	y = ar->winy - (vcurl + 1) * lheight - 2;

	/* offset back so the start of the text lines up with the suggestions,
	 * not essential but makes suggestions easier to follow */
	x -= st->cwidth * (st->text->curc - text_find_identifier_start(st->text->curl->line, st->text->curc));

	boxw = SUGG_LIST_WIDTH * st->cwidth + 20;
	boxh = SUGG_LIST_SIZE * lheight + 8;
	
	if (x + boxw > ar->winx)
		x = MAX2(0, ar->winx - boxw);

	/* not needed but stands out nicer */
	uiDrawBoxShadow(220, x, y - boxh, x + boxw, y);

	UI_ThemeColor(TH_SHADE1);
	glRecti(x - 1, y + 1, x + boxw + 1, y - boxh - 1);
	UI_ThemeColorShade(TH_BACK, 16);
	glRecti(x, y, x + boxw, y - boxh);

	/* Set the top 'item' of the visible list */
	for (i = 0, item = first; i < *top && item->next; i++, item = item->next) ;

	for (i = 0; i < SUGG_LIST_SIZE && item; i++, item = item->next) {
		int len = txt_utf8_forward_columns(item->name, SUGG_LIST_WIDTH, NULL) - item->name;

		y -= lheight;

		BLI_strncpy(str, item->name, len + 1);

		w = st->cwidth * text_get_char_pos(st, str, len);
		
		if (item == sel) {
			UI_ThemeColor(TH_SHADE2);
			glRecti(x + margin_x, y - 3, x + margin_x + w, y + lheight - 3);
		}

		format_draw_color(item->type);
		text_draw(st, str, 0, 0, x + margin_x, y - 1, NULL);

		if (item == last) break;
	}
}

/*********************** draw cursor ************************/

static void draw_cursor(SpaceText *st, ARegion *ar)
{
	Text *text = st->text;
	int vcurl, vcurc, vsell, vselc, hidden = 0;
	int x, y, w, i;
	const int lheight = st->lheight_dpi + TXT_LINE_SPACING;

	/* Draw the selection */
	if (text->curl != text->sell || text->curc != text->selc) {
		int offl, offc;
		/* Convert all to view space character coordinates */
		wrap_offset(st, ar, text->curl, text->curc, &offl, &offc);
		vcurl = txt_get_span(text->lines.first, text->curl) - st->top + offl;
		vcurc = text_get_char_pos(st, text->curl->line, text->curc) - st->left + offc;
		wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);
		vsell = txt_get_span(text->lines.first, text->sell) - st->top + offl;
		vselc = text_get_char_pos(st, text->sell->line, text->selc) - st->left + offc;

		if (vcurc < 0) vcurc = 0;
		if (vselc < 0) vselc = 0, hidden = 1;
		
		UI_ThemeColor(TH_SHADE2);
		x = st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
		y = ar->winy;

		if (vcurl == vsell) {
			y -= vcurl * lheight;
			if (vcurc < vselc)
				glRecti(x + vcurc * st->cwidth - 1, y, x + vselc * st->cwidth, y - lheight);
			else
				glRecti(x + vselc * st->cwidth - 1, y, x + vcurc * st->cwidth, y - lheight);
		}
		else {
			int froml, fromc, tol, toc;

			if (vcurl < vsell) {
				froml = vcurl; tol = vsell;
				fromc = vcurc; toc = vselc;
			}
			else {
				froml = vsell; tol = vcurl;
				fromc = vselc; toc = vcurc;
			}

			y -= froml * lheight;
			glRecti(x + fromc * st->cwidth - 1, y, ar->winx, y - lheight); y -= lheight;
			for (i = froml + 1; i < tol; i++)
				glRecti(x - 4, y, ar->winx, y - lheight),  y -= lheight;

			glRecti(x - 4, y, x + toc * st->cwidth, y - lheight);  y -= lheight;
		}
	}
	else {
		int offl, offc;
		wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);
		vsell = txt_get_span(text->lines.first, text->sell) - st->top + offl;
		vselc = text_get_char_pos(st, text->sell->line, text->selc) - st->left + offc;

		if (vselc < 0) {
			vselc = 0;
			hidden = 1;
		}
	}

	if (st->line_hlight) {
		int x1, x2, y1, y2;

		if (st->wordwrap) {
			int visible_lines = text_get_visible_lines(st, ar, text->sell->line);
			int offl, offc;

			wrap_offset_in_line(st, ar, text->sell, text->selc, &offl, &offc);

			y1 = ar->winy - (vsell - offl) * lheight;
			y2 = y1 - (lheight * visible_lines);
		}
		else {
			y1 = ar->winy - vsell * lheight;
			y2 = y1 - (lheight);
		}

		if (!(y1 < 0 || y2 > ar->winy)) { /* check we need to draw */
			x1 = 0; // st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
			x2 = x1 + ar->winx;

			glColor4ub(255, 255, 255, 32);
			
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			glRecti(x1 - 4, y1, x2, y2);
			glDisable(GL_BLEND);
		}
	}
	
	if (!hidden) {
		/* Draw the cursor itself (we draw the sel. cursor as this is the leading edge) */
		x = st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
		x += vselc * st->cwidth;
		y = ar->winy - vsell * lheight;
		
		if (st->overwrite) {
			char ch = text->sell->line[text->selc];
			
			y += TXT_LINE_SPACING;
			w = st->cwidth;
			if (ch == '\t') w *= st->tabnumber - (vselc + st->left) % st->tabnumber;
			
			UI_ThemeColor(TH_HILITE);
			glRecti(x, y - lheight - 1, x + w, y - lheight + 1);
		}
		else {
			UI_ThemeColor(TH_HILITE);
			glRecti(x - 1, y, x + 1, y - lheight);
		}
	}
}

/******************* draw matching brackets *********************/

static void draw_brackets(SpaceText *st, ARegion *ar)
{
	TextLine *startl, *endl, *linep;
	Text *text = st->text;
	int b, fc, find, stack, viewc, viewl, offl, offc, x, y;
	int startc, endc, c;
	
	char ch;

	// showsyntax must be on or else the format string will be null
	if (!text->curl || !st->showsyntax) return;

	startl = text->curl;
	startc = text->curc;
	b = text_check_bracket(startl->line[startc]);
	if (b == 0 && startc > 0) b = text_check_bracket(startl->line[--startc]);
	if (b == 0) return;

	linep = startl;
	c = startc;
	fc = txt_utf8_offset_to_index(linep->line, startc);
	endl = NULL;
	endc = -1;
	find = -b;
	stack = 0;
	
	/* Don't highlight backets if syntax HL is off or bracket in string or comment. */
	if (!linep->format || linep->format[fc] == FMT_TYPE_STRING || linep->format[fc] == FMT_TYPE_COMMENT)
		return;

	if (b > 0) {
		/* opening bracket, search forward for close */
		fc++;
		c += BLI_str_utf8_size_safe(linep->line + c);
		while (linep) {
			while (c < linep->len) {
				if (linep->format && linep->format[fc] != FMT_TYPE_STRING && linep->format[fc] != FMT_TYPE_COMMENT) {
					b = text_check_bracket(linep->line[c]);
					if (b == find) {
						if (stack == 0) {
							endl = linep;
							endc = c;
							break;
						}
						stack--;
					}
					else if (b == -find) {
						stack++;
					}
				}
				fc++;
				c += BLI_str_utf8_size_safe(linep->line + c);
			}
			if (endl) break;
			linep = linep->next;
			c = 0;
			fc = 0;
		}
	}
	else {
		/* closing bracket, search backward for open */
		fc--;
		if (c > 0) c -= linep->line + c - BLI_str_prev_char_utf8(linep->line + c);
		while (linep) {
			while (fc >= 0) {
				if (linep->format && linep->format[fc] != FMT_TYPE_STRING && linep->format[fc] != FMT_TYPE_COMMENT) {
					b = text_check_bracket(linep->line[c]);
					if (b == find) {
						if (stack == 0) {
							endl = linep;
							endc = c;
							break;
						}
						stack--;
					}
					else if (b == -find) {
						stack++;
					}
				}
				fc--;
				if (c > 0) c -= linep->line + c - BLI_str_prev_char_utf8(linep->line + c);
			}
			if (endl) break;
			linep = linep->prev;
			if (linep) {
				if (linep->format) fc = strlen(linep->format) - 1;
				else fc = -1;
				if (linep->len) c = BLI_str_prev_char_utf8(linep->line + linep->len) - linep->line;
				else fc = -1;
			}
		}
	}

	if (!endl || endc == -1)
		return;

	UI_ThemeColor(TH_HILITE);
	x = st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
	y = ar->winy - st->lheight_dpi;

	/* draw opening bracket */
	ch = startl->line[startc];
	wrap_offset(st, ar, startl, startc, &offl, &offc);
	viewc = text_get_char_pos(st, startl->line, startc) - st->left + offc;

	if (viewc >= 0) {
		viewl = txt_get_span(text->lines.first, startl) - st->top + offl;

		text_font_draw_character(st, x + viewc * st->cwidth, y - viewl * (st->lheight_dpi + TXT_LINE_SPACING), ch);
		text_font_draw_character(st, x + viewc * st->cwidth + 1, y - viewl * (st->lheight_dpi + TXT_LINE_SPACING), ch);
	}

	/* draw closing bracket */
	ch = endl->line[endc];
	wrap_offset(st, ar, endl, endc, &offl, &offc);
	viewc = text_get_char_pos(st, endl->line, endc) - st->left + offc;

	if (viewc >= 0) {
		viewl = txt_get_span(text->lines.first, endl) - st->top + offl;

		text_font_draw_character(st, x + viewc * st->cwidth, y - viewl * (st->lheight_dpi + TXT_LINE_SPACING), ch);
		text_font_draw_character(st, x + viewc * st->cwidth + 1, y - viewl * (st->lheight_dpi + TXT_LINE_SPACING), ch);
	}
}

/*********************** main area drawing *************************/

void draw_text_main(SpaceText *st, ARegion *ar)
{
	Text *text = st->text;
	TextFormatType *tft;
	TextLine *tmp;
	rcti scroll, back;
	char linenr[12];
	int i, x, y, winx, linecount = 0, lineno = 0;
	int wraplinecount = 0, wrap_skip = 0;
	int margin_column_x;

	/* if no text, nothing to do */
	if (!text)
		return;

	/* dpi controlled line height and font size */
	st->lheight_dpi = (U.widget_unit * st->lheight) / 20;
	st->viewlines = (st->lheight_dpi) ? (int)ar->winy / (st->lheight_dpi + TXT_LINE_SPACING) : 0;
	
	text_update_drawcache(st, ar);

	/* make sure all the positional pointers exist */
	if (!text->curl || !text->sell || !text->lines.first || !text->lines.last)
		txt_clean_text(text);
	
	/* update rects for scroll */
	calc_text_rcts(st, ar, &scroll, &back); /* scroll will hold the entire bar size */

	/* update syntax formatting if needed */
	tft = ED_text_format_get(text);
	tmp = text->lines.first;
	lineno = 0;
	for (i = 0; i < st->top && tmp; i++) {
		if (st->showsyntax && !tmp->format)
			tft->format_line(st, tmp, false);

		if (st->wordwrap) {
			int lines = text_get_visible_lines_no(st, lineno);

			if (wraplinecount + lines > st->top) {
				wrap_skip = st->top - wraplinecount;
				break;
			}
			else {
				wraplinecount += lines;
				tmp = tmp->next;
				linecount++;
			}
		}
		else {
			tmp = tmp->next;
			linecount++;
		}

		lineno++;
	}

	text_font_begin(st);
	st->cwidth = BLF_fixed_width(mono);
	st->cwidth = MAX2(st->cwidth, (char)1);

	/* draw line numbers background */
	if (st->showlinenrs) {
		x = TXT_OFFSET + TEXTXLOC;

		UI_ThemeColor(TH_GRID);
		glRecti((TXT_OFFSET - 12), 0, (TXT_OFFSET - 5) + TEXTXLOC, ar->winy - 2);
	}
	else {
		st->linenrs_tot = 0; /* not used */
		x = TXT_OFFSET;
	}
	y = ar->winy - st->lheight_dpi;
	winx = ar->winx - TXT_SCROLL_WIDTH;
	
	/* draw cursor */
	draw_cursor(st, ar);

	/* draw the text */
	UI_ThemeColor(TH_TEXT);

	for (i = 0; y > 0 && i < st->viewlines && tmp; i++, tmp = tmp->next) {
		if (st->showsyntax && !tmp->format)
			tft->format_line(st, tmp, false);

		if (st->showlinenrs && !wrap_skip) {
			/* draw line number */
			if (tmp == text->curl)
				UI_ThemeColor(TH_HILITE);
			else
				UI_ThemeColor(TH_TEXT);

			BLI_snprintf(linenr, sizeof(linenr), "%*d", st->linenrs_tot, i + linecount + 1);
			/* itoa(i + linecount + 1, linenr, 10); */ /* not ansi-c :/ */
			text_font_draw(st, TXT_OFFSET - 7, y, linenr);

			UI_ThemeColor(TH_TEXT);
		}

		if (st->wordwrap) {
			/* draw word wrapped text */
			int lines = text_draw_wrapped(st, tmp->line, x, y, winx - x, tmp->format, wrap_skip);
			y -= lines * (st->lheight_dpi + TXT_LINE_SPACING);
		}
		else {
			/* draw unwrapped text */
			text_draw(st, tmp->line, st->left, ar->winx / st->cwidth, x, y, tmp->format);
			y -= st->lheight_dpi + TXT_LINE_SPACING;
		}
		
		wrap_skip = 0;
	}
	
	if (st->flags & ST_SHOW_MARGIN) {
		UI_ThemeColor(TH_HILITE);

		margin_column_x = x + st->cwidth * (st->margin_column - st->left);
		
		if (margin_column_x >= x) {
			glBegin(GL_LINES);
			glVertex2i(margin_column_x, 0);
			glVertex2i(margin_column_x, ar->winy - 2);
			glEnd();
		}
	}

	/* draw other stuff */
	draw_brackets(st, ar);
	glTranslatef(GLA_PIXEL_OFS, GLA_PIXEL_OFS, 0.0f); /* XXX scroll requires exact pixel space */
	draw_textscroll(st, &scroll, &back);
	draw_documentation(st, ar);
	draw_suggestion_list(st, ar);
	
	text_font_end(st);
}

/************************** update ***************************/

void text_update_character_width(SpaceText *st)
{
	text_font_begin(st);
	st->cwidth = BLF_fixed_width(mono);
	st->cwidth = MAX2(st->cwidth, (char)1);
	text_font_end(st);
}

/* Moves the view to the cursor location,
 * also used to make sure the view isn't outside the file */
void text_scroll_to_cursor(SpaceText *st, ScrArea *sa)
{
	Text *text;
	ARegion *ar = NULL;
	int i, x, winx = 0;

	if (ELEM(NULL, st, st->text, st->text->curl)) return;

	text = st->text;

	for (ar = sa->regionbase.first; ar; ar = ar->next)
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			winx = ar->winx;
			break;
		}

	text_update_character_width(st);

	i = txt_get_span(text->lines.first, text->sell);
	if (st->wordwrap) {
		int offl, offc;
		wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);
		i += offl;
	}

	if (st->top + st->viewlines <= i || st->top > i)
		st->top = i - st->viewlines / 2;
	
	if (st->wordwrap) {
		st->left = 0;
	}
	else {
		x = st->cwidth * (text_get_char_pos(st, text->sell->line, text->selc) - st->left);
		winx -= TXT_OFFSET + (st->showlinenrs ? TEXTXLOC : 0) + TXT_SCROLL_WIDTH;

		if (x <= 0 || x > winx)
			st->left += (x - winx / 2) / st->cwidth;
	}

	if (st->top < 0) st->top = 0;
	if (st->left < 0) st->left = 0;

	st->scroll_accum[0] = 0.0f;
	st->scroll_accum[1] = 0.0f;
}

void text_update_cursor_moved(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceText *st = CTX_wm_space_text(C);

	text_scroll_to_cursor(st, sa);
}
