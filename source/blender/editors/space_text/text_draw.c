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
 * Contributor(s): none yet.
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

#include "DNA_text_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_suggestions.h"
#include "BKE_text.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_datafiles.h"
#include "UI_interface.h"
#include "UI_resources.h"

#include "text_intern.h"

/******************** text font drawing ******************/

static void text_font_begin(SpaceText *st)
{
	static int mono= -1; // XXX needs proper storage

	if(mono == -1)
		mono= BLF_load_mem("monospace", (unsigned char*)datatoc_bmonofont_ttf, datatoc_bmonofont_ttf_size);

	BLF_set(mono);
	BLF_aspect(1.0);

	BLF_size(st->lheight, 72);
}

static void text_font_end(SpaceText *st)
{
}

static int text_font_draw(SpaceText *st, int x, int y, char *str)
{
	BLF_position(x, y, 0);
	BLF_draw(str);

	return BLF_width(str);
}

static int text_font_draw_character(SpaceText *st, int x, int y, char c)
{
	char str[2];

	str[0]= c;
	str[1]= '\0';

	BLF_position(x, y, 0);
	BLF_draw(str);

	return st->cwidth;
}

int text_font_width(SpaceText *st, char *str)
{
	return BLF_width(str);
}

/****************** flatten string **********************/

static void flatten_string_append(FlattenString *fs, char c, int accum) 
{
	if(fs->pos>=fs->len && fs->pos>=sizeof(fs->fixedbuf)-1) {
		char *nbuf; int *naccum;
		if(fs->len) fs->len*= 2;
		else fs->len= sizeof(fs->fixedbuf) * 2;

		nbuf= MEM_callocN(sizeof(*fs->buf)*fs->len, "fs->buf");
		naccum= MEM_callocN(sizeof(*fs->accum)*fs->len, "fs->accum");

		memcpy(nbuf, fs->buf, fs->pos);
		memcpy(naccum, fs->accum, fs->pos);
		
		if(fs->buf != fs->fixedbuf) {
			MEM_freeN(fs->buf);
			MEM_freeN(fs->accum);
		}
		
		fs->buf= nbuf;
		fs->accum= naccum;
	}
	
	fs->buf[fs->pos]= c;	
	fs->accum[fs->pos]= accum;
	
	fs->pos++;
}

int flatten_string(SpaceText *st, FlattenString *fs, char *in)
{
	int r = 0, i = 0;

	memset(fs, 0, sizeof(FlattenString));
	fs->buf= fs->fixedbuf;
	fs->accum= fs->fixedaccum;
	
	for(r=0, i=0; *in; r++, in++) {
		if(*in=='\t') {
			if(fs->pos && *(in-1)=='\t')
				i= st->tabnumber;
			else if(st->tabnumber > 0)
				i= st->tabnumber - (fs->pos%st->tabnumber);

			while(i--)
				flatten_string_append(fs, ' ', r);
		}
		else
			flatten_string_append(fs, *in, r);
	}

	return fs->pos;
}

void flatten_string_free(FlattenString *fs)
{
	if(fs->buf != fs->fixedbuf)
		MEM_freeN(fs->buf);
	if(fs->accum != fs->fixedaccum)
		MEM_freeN(fs->accum);
}

/* Checks the specified source string for a Python built-in function name. This
 name must start at the beginning of the source string and must be followed by
 a non-identifier (see text_check_identifier(char)) or null character.
 
 If a built-in function is found, the length of the matching name is returned.
 Otherwise, -1 is returned. */

static int find_builtinfunc(char *string)
{
	int a, i;
	char builtinfuncs[][11] = {"and", "as", "assert", "break", "class", "continue", "def",
								"del", "elif", "else", "except", "exec", "finally",
								"for", "from", "global", "if", "import", "in",
								"is", "lambda", "not", "or", "pass", "print",
								"raise", "return", "try", "while", "yield"};
	for(a=0; a<30; a++) {
		i = 0;
		while(1) {
			/* If we hit the end of a keyword... (eg. "def") */
			if(builtinfuncs[a][i]=='\0') {
				/* If we still have identifier chars in the source (eg. "definate") */
				if(text_check_identifier(string[i]))
					i = -1; /* No match */
				break; /* Next keyword if no match, otherwise we're done */
				
			/* If chars mismatch, move on to next keyword */
			}
			else if(string[i]!=builtinfuncs[a][i]) {
				i = -1;
				break; /* Break inner loop, start next keyword */
			}
			i++;
		}
		if(i>0) break; /* If we have a match, we're done */
	}
	return i;
}

/* Checks the specified source string for a Python special name. This name must
 start at the beginning of the source string and must be followed by a non-
 identifier (see text_check_identifier(char)) or null character.
 
 If a special name is found, the length of the matching name is returned.
 Otherwise, -1 is returned. */

static int find_specialvar(char *string) 
{
	int i = 0;
	/* Check for "def" */
	if(string[0]=='d' && string[1]=='e' && string[2]=='f')
		i = 3;
	/* Check for "class" */
	else if(string[0]=='c' && string[1]=='l' && string[2]=='a' && string[3]=='s' && string[4]=='s')
		i = 5;
	/* If next source char is an identifier (eg. 'i' in "definate") no match */
	if(i==0 || text_check_identifier(string[i]))
		return -1;
	return i;
}

/* Ensures the format string for the given line is long enough, reallocating
 as needed. Allocation is done here, alone, to ensure consistency. */
int text_check_format_len(TextLine *line, unsigned int len)
{
	if(line->format) {
		if(strlen(line->format) < len) {
			MEM_freeN(line->format);
			line->format = MEM_mallocN(len+2, "SyntaxFormat");
			if(!line->format) return 0;
		}
	}
	else {
		line->format = MEM_mallocN(len+2, "SyntaxFormat");
		if(!line->format) return 0;
	}

	return 1;
}

/* Formats the specified line. If do_next is set, the process will move on to
 the succeeding line if it is affected (eg. multiline strings). Format strings
 may contain any of the following characters:
 	'_'		Whitespace
 	'#'		Comment text
 	'!'		Punctuation and other symbols
 	'n'		Numerals
 	'l'		String letters
 	'v'		Special variables (class, def)
 	'b'		Built-in names (print, for, etc.)
 	'q'		Other text (identifiers, etc.)
 It is terminated with a null-terminator '\0' followed by a continuation
 flag indicating whether the line is part of a multi-line string. */

static void txt_format_line(SpaceText *st, TextLine *line, int do_next)
{
	FlattenString fs;
	char *str, *fmt, orig, cont, find, prev = ' ';
	int len, i;

	/* Get continuation from previous line */
	if(line->prev && line->prev->format != NULL) {
		fmt= line->prev->format;
		cont = fmt[strlen(fmt)+1]; /* Just after the null-terminator */
	}
	else cont = 0;

	/* Get original continuation from this line */
	if(line->format != NULL) {
		fmt= line->format;
		orig = fmt[strlen(fmt)+1]; /* Just after the null-terminator */
	}
	else orig = 0xFF;

	flatten_string(st, &fs, line->line);
	str = fs.buf;
	len = strlen(str);
	if(!text_check_format_len(line, len)) {
		flatten_string_free(&fs);
		return;
	}
	fmt = line->format;

	while(*str) {
		/* Handle escape sequences by skipping both \ and next char */
		if(*str == '\\') {
			*fmt = prev; fmt++; str++;
			if(*str == '\0') break;
			*fmt = prev; fmt++; str++;
			continue;
		}
		/* Handle continuations */
		else if(cont) {
			/* Triple strings ("""...""" or '''...''') */
			if(cont & TXT_TRISTR) {
				find = (cont & TXT_DBLQUOTSTR) ? '"' : '\'';
				if(*str==find && *(str+1)==find && *(str+2)==find) {
					*fmt = 'l'; fmt++; str++;
					*fmt = 'l'; fmt++; str++;
					cont = 0;
				}
			/* Handle other strings */
			}
			else {
				find = (cont & TXT_DBLQUOTSTR) ? '"' : '\'';
				if(*str == find) cont = 0;
			}

			*fmt = 'l';
		}
		/* Not in a string... */
		else {
			/* Deal with comments first */
			if(prev == '#' || *str == '#')
				*fmt = '#';
			/* Strings */
			else if(*str == '"' || *str == '\'') {
				find = *str;
				cont = (*str== '"') ? TXT_DBLQUOTSTR : TXT_SNGQUOTSTR;
				if(*(str+1) == find && *(str+2) == find) {
					*fmt = 'l'; fmt++; str++;
					*fmt = 'l'; fmt++; str++;
					cont |= TXT_TRISTR;
				}
				*fmt = 'l';
			}
			/* Whitespace (all ws. has been converted to spaces) */
			else if(*str == ' ')
				*fmt = '_';
			/* Numbers (digits not part of an identifier and periods followed by digits) */
			else if((prev != 'q' && text_check_digit(*str)) || (*str == '.' && text_check_digit(*(str+1))))
				*fmt = 'n';
			/* Punctuation */
			else if(text_check_delim(*str))
				*fmt = '!';
			/* Identifiers and other text (no previous ws. or delims. so text continues) */
			else if(prev == 'q')
				*fmt = 'q';
			/* Not ws, a digit, punct, or continuing text. Must be new, check for special words */
			else {
				/* Special vars(v) or built-in keywords(b) */
				if((i=find_specialvar(str)) != -1)
					prev = 'v';
				else if((i=find_builtinfunc(str)) != -1)
					prev = 'b';
				if(i>0) {
					while(i>1) {
						*fmt = prev; fmt++; str++;
						i--;
					}
					*fmt = prev;
				}
				else
					*fmt = 'q';
			}
		}
		prev = *fmt;
		fmt++;
		str++;
	}

	/* Terminate and add continuation char */
	*fmt = '\0'; fmt++;
	*fmt = cont;

	/* Debugging */
	//print_format(st, line);

	/* If continuation has changed and we're allowed, process the next line */
	if(cont!=orig && do_next && line->next) {
		txt_format_line(st, line->next, do_next);
	}

	flatten_string_free(&fs);
}

#if 0
/* Formats every line of the current text */
static void txt_format_text(SpaceText *st) 
{
	TextLine *linep;

	if(!st->text) return;

	for(linep=st->text->lines.first; linep; linep=linep->next)
		txt_format_line(st, linep, 0);
}
#endif

/* Sets the current drawing color based on the format character specified */
static void format_draw_color(char formatchar)
{
	switch (formatchar) {
		case '_': /* Whitespace */
			break;
		case '!': /* Symbols */
			UI_ThemeColorBlend(TH_TEXT, TH_BACK, 0.5f);
			break;
		case '#': /* Comments */
			UI_ThemeColor(TH_SYNTAX_C);
			break;
		case 'n': /* Numerals */
			UI_ThemeColor(TH_SYNTAX_N);
			break;
		case 'l': /* Strings */
			UI_ThemeColor(TH_SYNTAX_L);
			break;
		case 'v': /* Specials: class, def */
			UI_ThemeColor(TH_SYNTAX_V);
			break;
		case 'b': /* Keywords: for, print, etc. */
			UI_ThemeColor(TH_SYNTAX_B);
			break;
		case 'q': /* Other text (identifiers) */
		default:
			UI_ThemeColor(TH_TEXT);
			break;
	}
}

/*********************** utilities ************************/

int text_check_bracket(char ch)
{
	int a;
	char opens[] = "([{";
	char close[] = ")]}";
	
	for(a=0; a<3; a++) {
		if(ch==opens[a])
			return a+1;
		else if(ch==close[a])
			return -(a+1);
	}
	return 0;
}

int text_check_delim(char ch) 
{
	int a;
	char delims[] = "():\"\' ~!%^&*-+=[]{};/<>|.#\t,";
	
	for(a=0; a<28; a++) {
		if(ch==delims[a])
			return 1;
	}
	return 0;
}

int text_check_digit(char ch)
{
	if(ch < '0') return 0;
	if(ch <= '9') return 1;
	return 0;
}

int text_check_identifier(char ch)
{
	if(ch < '0') return 0;
	if(ch <= '9') return 1;
	if(ch < 'A') return 0;
	if(ch <= 'Z' || ch == '_') return 1;
	if(ch < 'a') return 0;
	if(ch <= 'z') return 1;
	return 0;
}

int text_check_whitespace(char ch)
{
	if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
		return 1;
	return 0;
}

/************************** draw text *****************************/

/***********************/ /*

Notes on word-wrap
--
All word-wrap functions follow the algorithm below to maintain consistency.
	line		The line to wrap (tabs converted to spaces)
	view_width	The maximum number of characters displayable in the region
				This equals region_width/font_width for the region
	wrap_chars	Characters that allow wrapping. This equals [' ', '\t', '-']

def wrap(line, view_width, wrap_chars):
	draw_start = 0
	draw_end = view_width
	pos = 0
	for c in line:
		if pos-draw_start >= view_width:
			print line[draw_start:draw_end]
			draw_start = draw_end
			draw_end += view_width
		elif c in wrap_chars:
			draw_end = pos+1
		pos += 1
	print line[draw_start:]

*/ /***********************/

int wrap_width(SpaceText *st, ARegion *ar)
{
	int x, max;
	
	x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
	max= (ar->winx-x)/st->cwidth;
	return max>8 ? max : 8;
}

/* Sets (offl, offc) for transforming (line, curs) to its wrapped position */
void wrap_offset(SpaceText *st, ARegion *ar, TextLine *linein, int cursin, int *offl, int *offc)
{
	Text *text;
	TextLine *linep;
	int i, j, start, end, chars, max, chop;
	char ch;

	*offl= *offc= 0;

	if(!st->text) return;
	if(!st->wordwrap) return;

	text= st->text;

	/* Move pointer to first visible line (top) */
	linep= text->lines.first;
	i= st->top;
	while(i>0 && linep) {
		if(linep == linein) return; /* Line before top */
		linep= linep->next;
		i--;
	}

	max= wrap_width(st, ar);

	while(linep) {
		start= 0;
		end= max;
		chop= 1;
		chars= 0;
		*offc= 0;
		for(i=0, j=0; linep->line[j]!='\0'; j++) {

			/* Mimic replacement of tabs */
			ch= linep->line[j];
			if(ch=='\t') {
				chars= st->tabnumber-i%st->tabnumber;
				if(linep==linein && i<cursin) cursin += chars-1;
				ch= ' ';
			}
			else
				chars= 1;

			while(chars--) {
				if(i-start>=max) {
					if(chop && linep==linein && i >= cursin)
						return;
					(*offl)++;
					*offc -= end-start;
					start= end;
					end += max;
					chop= 1;
				}
				else if(ch==' ' || ch=='-') {
					end = i+1;
					chop= 0;
					if(linep==linein && i >= cursin)
						return;
				}
				i++;
			}
		}
		if(linep==linein) break;
		linep= linep->next;
	}
}

static int get_char_pos(SpaceText *st, char *line, int cur)
{
	int a=0, i;
	
	for(i=0; i<cur && line[i]; i++) {
		if(line[i]=='\t')
			a += st->tabnumber-a%st->tabnumber;
		else
			a++;
	}
	return a;
}

static int text_draw_wrapped(SpaceText *st, char *str, int x, int y, int w, char *format)
{
	FlattenString fs;
	int basex, i, a, len, start, end, max, lines;
	
	len= flatten_string(st, &fs, str);
	str= fs.buf;
	max= w/st->cwidth;
	if(max<8) max= 8;
	basex= x;

	lines= 1;
	start= 0;
	end= max;
	for(i=0; i<len; i++) {
		if(i-start >= max) {
			/* Draw the visible portion of text on the overshot line */
			for(a=start; a<end; a++) {
				if(st->showsyntax && format) format_draw_color(format[a]);
				x += text_font_draw_character(st, x, y, str[a]);
			}
			y -= st->lheight;
			x= basex;
			lines++;
			start= end;
			end += max;
		}
		else if(str[i]==' ' || str[i]=='-') {
			end = i+1;
		}
	}

	/* Draw the remaining text */
	for(a=start; a<len; a++) {
		if(st->showsyntax && format)
			format_draw_color(format[a]);

		x += text_font_draw_character(st, x, y, str[a]);
	}

	flatten_string_free(&fs);

	return lines;
}

static int text_draw(SpaceText *st, char *str, int cshift, int maxwidth, int draw, int x, int y, char *format)
{
	FlattenString fs;
	int r=0, w= 0;
	int *acc;
	char *in;

	w= flatten_string(st, &fs, str);
	if(w < cshift) {
		flatten_string_free(&fs);
		return 0; /* String is shorter than shift */
	}
	
	in= fs.buf+cshift;
	acc= fs.accum+cshift;
	w= w-cshift;

	if(draw) {
		if(st->showsyntax && format) {
			int amount, a;
			format = format+cshift;
		
			amount = strlen(in);
			
			for(a = 0; a < amount; a++) {
				format_draw_color(format[a]);
				x += text_font_draw_character(st, x, y, in[a]);
			}
		}
		else
			text_font_draw(st, x, y, in);
	}
	else {
		while(w-- && *acc++ < maxwidth)
			r+= st->cwidth;
	}

	flatten_string_free(&fs);

	if(cshift && r==0)
		return 0;
	else if(st->showlinenrs)
		return r+TXT_OFFSET+TEXTXLOC;
	else
		return r+TXT_OFFSET;
}

/************************ draw scrollbar *****************************/

static void calc_text_rcts(SpaceText *st, ARegion *ar, rcti *scroll)
{
	int lhlstart, lhlend, ltexth;
	short barheight, barstart, hlstart, hlend, blank_lines;
	short pix_available, pix_top_margin, pix_bottom_margin, pix_bardiff;

	pix_top_margin = 8;
	pix_bottom_margin = 4;
	pix_available = ar->winy - pix_top_margin - pix_bottom_margin;
	ltexth= txt_get_span(st->text->lines.first, st->text->lines.last);
	blank_lines = st->viewlines / 2;
	
	/* nicer code: use scroll rect for entire bar */
	scroll->xmin= 5;
	scroll->xmax= 17;
	scroll->ymin= 4;
	scroll->ymax= 4+pix_available;
	
	/* when resizing a vieport with the bar at the bottom to a greater height more blank lines will be added */
	if(ltexth + blank_lines < st->top + st->viewlines) {
		blank_lines = st->top + st->viewlines - ltexth;
	}
	
	ltexth += blank_lines;

	barheight = (ltexth > 0)? (st->viewlines*pix_available)/ltexth: 0;
	pix_bardiff = 0;
	if(barheight < 20) {
		pix_bardiff = 20 - barheight; /* take into account the now non-linear sizing of the bar */	
		barheight = 20;
	}
	barstart = (ltexth > 0)? ((pix_available - pix_bardiff) * st->top)/ltexth: 0;

	st->txtbar= *scroll;
	st->txtbar.ymax -= barstart;
	st->txtbar.ymin = st->txtbar.ymax - barheight;

	CLAMP(st->txtbar.ymin, pix_bottom_margin, ar->winy - pix_top_margin);
	CLAMP(st->txtbar.ymax, pix_bottom_margin, ar->winy - pix_top_margin);

	st->pix_per_line= (pix_available > 0)? (float) ltexth/pix_available: 0;
	if(st->pix_per_line<.1) st->pix_per_line=.1f;

	lhlstart = MIN2(txt_get_span(st->text->lines.first, st->text->curl), 
				txt_get_span(st->text->lines.first, st->text->sell));
	lhlend = MAX2(txt_get_span(st->text->lines.first, st->text->curl), 
				txt_get_span(st->text->lines.first, st->text->sell));

	if(ltexth > 0) {
		hlstart = (lhlstart * pix_available)/ltexth;
		hlend = (lhlend * pix_available)/ltexth;

		/* the scrollbar is non-linear sized */
		if(pix_bardiff > 0) {
			/* the start of the highlight is in the current viewport */
			if(ltexth && st->viewlines && lhlstart >= st->top && lhlstart <= st->top + st->viewlines) { 
				/* speed the progresion of the start of the highlight through the scrollbar */
				hlstart = ( ( (pix_available - pix_bardiff) * lhlstart) / ltexth) + (pix_bardiff * (lhlstart - st->top) / st->viewlines); 	
			}
			else if(lhlstart > st->top + st->viewlines && hlstart < barstart + barheight && hlstart > barstart) {
				/* push hl start down */
				hlstart = barstart + barheight;
			}
			else if(lhlend > st->top  && lhlstart < st->top && hlstart > barstart) {
				/*fill out start */
				hlstart = barstart;
			}

			if(hlend <= hlstart) { 
				hlend = hlstart + 2;
			}

			/* the end of the highlight is in the current viewport */
			if(ltexth && st->viewlines && lhlend >= st->top && lhlend <= st->top + st->viewlines) { 
				/* speed the progresion of the end of the highlight through the scrollbar */
				hlend = (((pix_available - pix_bardiff )*lhlend)/ltexth) + (pix_bardiff * (lhlend - st->top)/st->viewlines); 	
			}
			else if(lhlend < st->top && hlend >= barstart - 2 && hlend < barstart + barheight) {
				/* push hl end up */
				hlend = barstart;
			}					
			else if(lhlend > st->top + st->viewlines && lhlstart < st->top + st->viewlines && hlend < barstart + barheight) {
				/* fill out end */
				hlend = barstart + barheight;
			}

			if(hlend <= hlstart) { 
				hlstart = hlend - 2;
			}	
		}	
	}
	else {
		hlstart = 0;
		hlend = 0;
	}

	if(hlend - hlstart < 2) { 
		hlend = hlstart + 2;
	}
	
	st->txtscroll= *scroll;
	st->txtscroll.ymax= ar->winy - pix_top_margin - hlstart;
	st->txtscroll.ymin= ar->winy - pix_top_margin - hlend;

	CLAMP(st->txtscroll.ymin, pix_bottom_margin, ar->winy - pix_top_margin);
	CLAMP(st->txtscroll.ymax, pix_bottom_margin, ar->winy - pix_top_margin);
}

static void draw_textscroll(SpaceText *st, ARegion *ar, rcti *scroll)
{
	bTheme *btheme= U.themes.first;
	uiWidgetColors wcol= btheme->tui.wcol_scroll;
	char col[3];
	float rad;
	
//	UI_ThemeColorShade(TH_SHADE1, -20);
//	glRecti(2, 2, 20, ar->winy-6);
//	uiEmboss(2, 2, 20, ar->winy-6, 1);

//	UI_ThemeColor(TH_SHADE1);
//	glRecti(st->txtbar.xmin, st->txtbar.ymin, st->txtbar.xmax, st->txtbar.ymax);

//	uiEmboss(st->txtbar.xmin, st->txtbar.ymin, st->txtbar.xmax, st->txtbar.ymax, st->flags & ST_SCROLL_SELECT);
	
	uiWidgetScrollDraw(&wcol, scroll, &st->txtbar, (st->flags & ST_SCROLL_SELECT)?UI_SCROLL_PRESSED:0);

	uiSetRoundBox(15);
	rad= 0.4f*MIN2(st->txtscroll.xmax - st->txtscroll.xmin, st->txtscroll.ymax - st->txtscroll.ymin);
	UI_GetThemeColor3ubv(TH_HILITE, col);
	glColor4ub(col[0], col[1], col[2], 48);
	glEnable(GL_BLEND);
	uiRoundBox(st->txtscroll.xmin+1, st->txtscroll.ymin, st->txtscroll.xmax-1, st->txtscroll.ymax, rad);
	glDisable(GL_BLEND);
}

/************************** draw markers **************************/

static void draw_markers(SpaceText *st, ARegion *ar)
{
	Text *text= st->text;
	TextMarker *marker, *next;
	TextLine *top, *bottom, *line;
	int offl, offc, i, cy, x1, x2, y1, y2, x, y;

	for(i=st->top, top= text->lines.first; top->next && i>0; i--)
		top= top->next;

	for(i=st->viewlines-1, bottom=top; bottom->next && i>0; i--)
		bottom= bottom->next;
	
	for(marker= text->markers.first; marker; marker= next) {
		next= marker->next;

		for(cy= 0, line= top; line; cy++, line= line->next) {
			if(cy+st->top==marker->lineno) {
				/* Remove broken markers */
				if(marker->end>line->len || marker->start>marker->end) {
					BLI_freelinkN(&text->markers, marker);
					break;
				}

				wrap_offset(st, ar, line, marker->start, &offl, &offc);
				x1= get_char_pos(st, line->line, marker->start) - st->left + offc;
				y1= cy + offl;
				wrap_offset(st, ar, line, marker->end, &offl, &offc);
				x2= get_char_pos(st, line->line, marker->end) - st->left + offc;
				y2= cy + offl;

				glColor3ub(marker->color[0], marker->color[1], marker->color[2]);
				x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
				y= ar->winy-3;

				if(y1==y2) {
					y -= y1*st->lheight;
					glBegin(GL_LINE_LOOP);
					glVertex2i(x+x2*st->cwidth+1, y);
					glVertex2i(x+x1*st->cwidth-2, y);
					glVertex2i(x+x1*st->cwidth-2, y-st->lheight);
					glVertex2i(x+x2*st->cwidth+1, y-st->lheight);
					glEnd();
				}
				else {
					y -= y1*st->lheight;
					glBegin(GL_LINE_STRIP);
					glVertex2i(ar->winx, y);
					glVertex2i(x+x1*st->cwidth-2, y);
					glVertex2i(x+x1*st->cwidth-2, y-st->lheight);
					glVertex2i(ar->winx, y-st->lheight);
					glEnd();
					y-=st->lheight;

					for(i=y1+1; i<y2; i++) {
						glBegin(GL_LINES);
						glVertex2i(x, y);
						glVertex2i(ar->winx, y);
						glVertex2i(x, y-st->lheight);
						glVertex2i(ar->winx, y-st->lheight);
						glEnd();
						y-=st->lheight;
					}

					glBegin(GL_LINE_STRIP);
					glVertex2i(x, y);
					glVertex2i(x+x2*st->cwidth+1, y);
					glVertex2i(x+x2*st->cwidth+1, y-st->lheight);
					glVertex2i(x, y-st->lheight);
					glEnd();
				}

				break;
			}

			if(line==bottom) break;
		}
	}
}

/*********************** draw documentation *******************************/

static void draw_documentation(SpaceText *st, ARegion *ar)
{
	TextLine *tmp;
	char *docs, buf[DOC_WIDTH+1], *p;
	int len, i, br, lines;
	int boxw, boxh, l, x, y, top;
	
	if(!st || !st->text) return;
	if(!texttool_text_is_active(st->text)) return;
	
	docs = texttool_docs_get();

	if(!docs) return;

	/* Count the visible lines to the cursor */
	for(tmp=st->text->curl, l=-st->top; tmp; tmp=tmp->prev, l++);
	if(l<0) return;
	
	if(st->showlinenrs) {
		x= st->cwidth*(st->text->curc-st->left) + TXT_OFFSET + TEXTXLOC - 4;
	}
	else {
		x= st->cwidth*(st->text->curc-st->left) + TXT_OFFSET - 4;
	}
	if(texttool_suggest_first()) {
		x += SUGG_LIST_WIDTH*st->cwidth + 50;
	}

	top= y= ar->winy - st->lheight*l - 2;
	len= strlen(docs);
	boxw= DOC_WIDTH*st->cwidth + 20;
	boxh= (DOC_HEIGHT+1)*st->lheight;

	/* Draw panel */
	UI_ThemeColor(TH_BACK);
	glRecti(x, y, x+boxw, y-boxh);
	UI_ThemeColor(TH_SHADE1);
	glBegin(GL_LINE_LOOP);
	glVertex2i(x, y);
	glVertex2i(x+boxw, y);
	glVertex2i(x+boxw, y-boxh);
	glVertex2i(x, y-boxh);
	glEnd();
	glBegin(GL_LINE_LOOP);
	glVertex2i(x+boxw-10, y-7);
	glVertex2i(x+boxw-4, y-7);
	glVertex2i(x+boxw-7, y-2);
	glEnd();
	glBegin(GL_LINE_LOOP);
	glVertex2i(x+boxw-10, y-boxh+7);
	glVertex2i(x+boxw-4, y-boxh+7);
	glVertex2i(x+boxw-7, y-boxh+2);
	glEnd();
	UI_ThemeColor(TH_TEXT);

	i= 0; br= DOC_WIDTH; lines= 0; // XXX -doc_scroll;
	for(p=docs; *p; p++) {
		if(*p == '\r' && *(++p) != '\n') *(--p)= '\n'; /* Fix line endings */
		if(*p == ' ' || *p == '\t')
			br= i;
		else if(*p == '\n') {
			buf[i]= '\0';
			if(lines>=0) {
				y -= st->lheight;
				text_draw(st, buf, 0, 0, 1, x+4, y-3, NULL);
			}
			i= 0; br= DOC_WIDTH; lines++;
		}
		buf[i++]= *p;
		if(i == DOC_WIDTH) { /* Reached the width, go to last break and wrap there */
			buf[br]= '\0';
			if(lines>=0) {
				y -= st->lheight;
				text_draw(st, buf, 0, 0, 1, x+4, y-3, NULL);
			}
			p -= i-br-1; /* Rewind pointer to last break */
			i= 0; br= DOC_WIDTH; lines++;
		}
		if(lines >= DOC_HEIGHT) break;
	}

	if(0 /* XXX doc_scroll*/ > 0 && lines < DOC_HEIGHT) {
		// XXX doc_scroll--;
		draw_documentation(st, ar);
	}
}

/*********************** draw suggestion list *******************************/

static void draw_suggestion_list(SpaceText *st, ARegion *ar)
{
	SuggItem *item, *first, *last, *sel;
	TextLine *tmp;
	char str[SUGG_LIST_WIDTH+1];
	int w, boxw=0, boxh, i, l, x, y, b, *top;
	
	if(!st || !st->text) return;
	if(!texttool_text_is_active(st->text)) return;

	first = texttool_suggest_first();
	last = texttool_suggest_last();

	if(!first || !last) return;

	text_pop_suggest_list();
	sel = texttool_suggest_selected();
	top = texttool_suggest_top();

	/* Count the visible lines to the cursor */
	for(tmp=st->text->curl, l=-st->top; tmp; tmp=tmp->prev, l++);
	if(l<0) return;
	
	if(st->showlinenrs) {
		x = st->cwidth*(st->text->curc-st->left) + TXT_OFFSET + TEXTXLOC - 4;
	}
	else {
		x = st->cwidth*(st->text->curc-st->left) + TXT_OFFSET - 4;
	}
	y = ar->winy - st->lheight*l - 2;

	boxw = SUGG_LIST_WIDTH*st->cwidth + 20;
	boxh = SUGG_LIST_SIZE*st->lheight + 8;
	
	UI_ThemeColor(TH_SHADE1);
	glRecti(x-1, y+1, x+boxw+1, y-boxh-1);
	UI_ThemeColor(TH_BACK);
	glRecti(x, y, x+boxw, y-boxh);

	/* Set the top 'item' of the visible list */
	for(i=0, item=first; i<*top && item->next; i++, item=item->next);

	for(i=0; i<SUGG_LIST_SIZE && item; i++, item=item->next) {

		y -= st->lheight;

		strncpy(str, item->name, SUGG_LIST_WIDTH);
		str[SUGG_LIST_WIDTH] = '\0';

		w = text_font_width(st, str);
		
		if(item == sel) {
			UI_ThemeColor(TH_SHADE2);
			glRecti(x+16, y-3, x+16+w, y+st->lheight-3);
		}
		b=1; /* b=1 colour block, text is default. b=0 no block, colour text */
		switch (item->type) {
			case 'k': UI_ThemeColor(TH_SYNTAX_B); b=0; break;
			case 'm': UI_ThemeColor(TH_TEXT); break;
			case 'f': UI_ThemeColor(TH_SYNTAX_L); break;
			case 'v': UI_ThemeColor(TH_SYNTAX_N); break;
			case '?': UI_ThemeColor(TH_TEXT); b=0; break;
		}
		if(b) {
			glRecti(x+8, y+2, x+11, y+5);
			UI_ThemeColor(TH_TEXT);
		}
		text_draw(st, str, 0, 0, 1, x+16, y-1, NULL);

		if(item == last) break;
	}
}

/*********************** draw cursor ************************/

static void draw_cursor(SpaceText *st, ARegion *ar)
{
	Text *text= st->text;
	int vcurl, vcurc, vsell, vselc, hidden=0;
	int offl, offc, x, y, w, i;
	
	/* Draw the selection */
	if(text->curl!=text->sell || text->curc!=text->selc) {
		/* Convert all to view space character coordinates */
		wrap_offset(st, ar, text->curl, text->curc, &offl, &offc);
		vcurl = txt_get_span(text->lines.first, text->curl) - st->top + offl;
		vcurc = get_char_pos(st, text->curl->line, text->curc) - st->left + offc;
		wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);
		vsell = txt_get_span(text->lines.first, text->sell) - st->top + offl;
		vselc = get_char_pos(st, text->sell->line, text->selc) - st->left + offc;

		if(vcurc<0) vcurc=0;
		if(vselc<0) vselc=0, hidden=1;
		
		UI_ThemeColor(TH_SHADE2);
		x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
		y= ar->winy-2;

		if(vcurl==vsell) {
			y -= vcurl*st->lheight;
			if(vcurc < vselc)
				glRecti(x+vcurc*st->cwidth-1, y, x+vselc*st->cwidth, y-st->lheight);
			else
				glRecti(x+vselc*st->cwidth-1, y, x+vcurc*st->cwidth, y-st->lheight);
		}
		else {
			int froml, fromc, tol, toc;

			if(vcurl < vsell) {
				froml= vcurl; tol= vsell;
				fromc= vcurc; toc= vselc;
			}
			else {
				froml= vsell; tol= vcurl;
				fromc= vselc; toc= vcurc;
			}

			y -= froml*st->lheight;
			glRecti(x+fromc*st->cwidth-1, y, ar->winx, y-st->lheight); y-=st->lheight;
			for(i=froml+1; i<tol; i++)
				glRecti(x-4, y, ar->winx, y-st->lheight),  y-=st->lheight;

			glRecti(x-4, y, x+toc*st->cwidth, y-st->lheight);  y-=st->lheight;
		}
	}
	else {
		wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);
		vsell = txt_get_span(text->lines.first, text->sell) - st->top + offl;
		vselc = get_char_pos(st, text->sell->line, text->selc) - st->left + offc;

		if(vselc<0) {
			vselc= 0;
			hidden= 1;
		}
	}

	if(!hidden) {
		/* Draw the cursor itself (we draw the sel. cursor as this is the leading edge) */
		x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
		x += vselc*st->cwidth;
		y= ar->winy-2 - vsell*st->lheight;
		
		if(st->overwrite) {
			char ch= text->sell->line[text->selc];
			if(!ch) ch= ' ';
			w= st->cwidth;
			UI_ThemeColor(TH_HILITE);
			glRecti(x, y-st->lheight-1, x+w, y-st->lheight+1);
		}
		else {
			UI_ThemeColor(TH_HILITE);
			glRecti(x-1, y, x+1, y-st->lheight);
		}
	}
}

/******************* draw matching brackets *********************/

static void draw_brackets(SpaceText *st, ARegion *ar)
{
	TextLine *startl, *endl, *linep;
	Text *text = st->text;
	int b, c, startc, endc, find, stack;
	int viewc, viewl, offl, offc, x, y;
	char ch;

	if(!text->curl) return;

	startl= text->curl;
	startc= text->curc;
	b= text_check_bracket(startl->line[startc]);
	if(b==0 && startc>0) b = text_check_bracket(startl->line[--startc]);
	if(b==0) return;
	
	linep= startl;
	c= startc;
	endl= NULL;
	endc= -1;
	find= -b;
	stack= 0;

	if(b>0) {
		/* opening bracket, search forward for close */
		c++;
		while(linep) {
			while(c<linep->len) {
				b= text_check_bracket(linep->line[c]);
				if(b==find) {
					if(stack==0) {
						endl= linep;
						endc= c;
						break;
					}
					stack--;
				}
				else if(b==-find) {
					stack++;
				}
				c++;
			}
			if(endl) break;
			linep= linep->next;
			c= 0;
		}
	}
	else {
		/* closing bracket, search backward for open */
		c--;
		while(linep) {
			while(c>=0) {
				b= text_check_bracket(linep->line[c]);
				if(b==find) {
					if(stack==0) {
						endl= linep;
						endc= c;
						break;
					}
					stack--;
				}
				else if(b==-find) {
					stack++;
				}
				c--;
			}
			if(endl) break;
			linep= linep->prev;
			if(linep) c= linep->len-1;
		}
	}

	if(!endl || endc==-1)
		return;

	UI_ThemeColor(TH_HILITE);	
	x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
	y= ar->winy - st->lheight;

	/* draw opening bracket */
	ch= startl->line[startc];
	wrap_offset(st, ar, startl, startc, &offl, &offc);
	viewc= get_char_pos(st, startl->line, startc) - st->left + offc;

	if(viewc >= 0){
		viewl= txt_get_span(text->lines.first, startl) - st->top + offl;

		text_font_draw_character(st, x+viewc*st->cwidth, y-viewl*st->lheight, ch);
		text_font_draw_character(st, x+viewc*st->cwidth+1, y-viewl*st->lheight, ch);
	}

	/* draw closing bracket */
	ch= endl->line[endc];
	wrap_offset(st, ar, endl, endc, &offl, &offc);
	viewc= get_char_pos(st, endl->line, endc) - st->left + offc;

	if(viewc >= 0) {
		viewl= txt_get_span(text->lines.first, endl) - st->top + offl;

		text_font_draw_character(st, x+viewc*st->cwidth, y-viewl*st->lheight, ch);
		text_font_draw_character(st, x+viewc*st->cwidth+1, y-viewl*st->lheight, ch);
	}
}

/*********************** main area drawing *************************/

void draw_text_main(SpaceText *st, ARegion *ar)
{
	Text *text= st->text;
	TextLine *tmp;
	rcti scroll;
	char linenr[12];
	int i, x, y, linecount= 0;

	/* if no text, nothing to do */
	if(!text)
		return;
	
	/* make sure all the positional pointers exist */
	if(!text->curl || !text->sell || !text->lines.first || !text->lines.last)
		txt_clean_text(text);
	
	if(st->lheight) st->viewlines= (int)ar->winy/st->lheight;
	else st->viewlines= 0;
	
	/* update rects for scroll */
	calc_text_rcts(st, ar, &scroll);	/* scroll will hold the entire bar size */

	/* update syntax formatting if needed */
	tmp= text->lines.first;
	for(i= 0; i<st->top && tmp; i++) {
		if(st->showsyntax && !tmp->format)
			txt_format_line(st, tmp, 0);

		tmp= tmp->next;
		linecount++;
	}

	text_font_begin(st);
	st->cwidth= BLF_fixed_width();
	st->cwidth= MAX2(st->cwidth, 1);

	/* draw line numbers background */
	if(st->showlinenrs) {
		st->linenrs_tot = (int)floor(log10((float)(linecount + st->viewlines))) + 1;
		x= TXT_OFFSET + TEXTXLOC;

		UI_ThemeColor(TH_GRID);
		glRecti((TXT_OFFSET-12), 0, (TXT_OFFSET-5) + TEXTXLOC, ar->winy - 2);
	}
	else {
		st->linenrs_tot= 0; /* not used */
		x= TXT_OFFSET;
	}
	y= ar->winy-st->lheight;

	/* draw cursor */
	draw_cursor(st, ar);

	/* draw the text */
	UI_ThemeColor(TH_TEXT);

	for(i=0; y>0 && i<st->viewlines && tmp; i++, tmp= tmp->next) {
		if(st->showsyntax && !tmp->format)
			txt_format_line(st, tmp, 0);

		if(st->showlinenrs) {
			/* draw line number */
			if(tmp == text->curl)
				UI_ThemeColor(TH_HILITE);
			else
				UI_ThemeColor(TH_TEXT);

			sprintf(linenr, "%d", i + linecount + 1);
			/* itoa(i + linecount + 1, linenr, 10); */ /* not ansi-c :/ */
			text_font_draw(st, TXT_OFFSET - 7, y, linenr);

			UI_ThemeColor(TH_TEXT);
		}

		if(st->wordwrap) {
			/* draw word wrapped text */
			int lines = text_draw_wrapped(st, tmp->line, x, y, ar->winx-x, tmp->format);
			y -= lines*st->lheight;
		}
		else {
			/* draw unwrapped text */
			text_draw(st, tmp->line, st->left, 0, 1, x, y, tmp->format);
			y -= st->lheight;
		}
	}
	
	/* draw other stuff */
	draw_brackets(st, ar);
	draw_markers(st, ar);
	glTranslatef(0.375f, 0.375f, 0.0f); /* XXX scroll requires exact pixel space */
	draw_textscroll(st, ar, &scroll);
	draw_documentation(st, ar);
	draw_suggestion_list(st, ar);
	
	text_font_end(st);
}

/************************** update ***************************/

void text_update_character_width(SpaceText *st)
{
	text_font_begin(st);
	st->cwidth= BLF_fixed_width();
	st->cwidth= MAX2(st->cwidth, 1);
	text_font_end(st);
}

/* Moves the view to the cursor location,
  also used to make sure the view isnt outside the file */
void text_update_cursor_moved(SpaceText *st, ARegion *ar)
{
	Text *text= st->text;
	int i, x;

	if(!text || !text->curl) return;

	text_update_character_width(st);

	i= txt_get_span(text->lines.first, text->sell);
	if(st->top+st->viewlines <= i || st->top > i)
		st->top= i - st->viewlines/2;
	
	if(st->wordwrap) {
		st->left= 0;
	}
	else {
		x= text_draw(st, text->sell->line, st->left, text->selc, 0, 0, 0, NULL);

		if(x==0 || x>ar->winx)
			st->left= text->curc-0.5*(ar->winx)/st->cwidth;
	}

	if(st->top < 0) st->top= 0;
	if(st->left <0) st->left= 0;
}

