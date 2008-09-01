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

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif   
#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_utildefines.h"
#include "BKE_text.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_suggestions.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_keyval.h"
#include "BIF_interface.h"
#include "BIF_drawtext.h"
#include "BIF_editfont.h"
#include "BIF_spacetypes.h"
#include "BIF_usiblender.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_mainqueue.h"

#include "BSE_filesel.h"

#include "BPY_extern.h"
#include "BPY_menus.h"

#include "mydevice.h"
#include "blendef.h" 
#include "winlay.h"

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

#define TEXTXLOC		38

#define SUGG_LIST_SIZE	7
#define SUGG_LIST_WIDTH	20
#define DOC_WIDTH		40
#define DOC_HEIGHT		10

#define TOOL_SUGG_LIST	0x01
#define TOOL_DOCUMENT	0x02

#define TMARK_GRP_CUSTOM	0x00010000	/* Lower 2 bytes used for Python groups */
#define TMARK_GRP_FINDALL	0x00020000

/* forward declarations */

void drawtextspace(ScrArea *sa, void *spacedata);
void winqreadtextspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);
void txt_copy_selectbuffer (Text *text);
void draw_brackets(SpaceText *st);

static void get_selection_buffer(Text *text);
static int check_bracket(char ch);
static int check_delim(char ch);
static int check_digit(char ch);
static int check_builtinfuncs(char *string);
static int check_specialvars(char *string);
static int check_identifier(char ch);
static int check_whitespace(char ch);

static int get_wrap_width(SpaceText *st);
static int get_wrap_points(SpaceText *st, char *line);
static void get_suggest_prefix(Text *text, int offset);
static void confirm_suggestion(Text *text, int skipleft);

#define TXT_MAXFINDSTR 255
static int g_find_flags= TXT_FIND_WRAP;
static char *g_find_str= NULL;
static char *g_replace_str= NULL;

static int doc_scroll= 0;
static double last_check_time= 0;
static int jump_to= 0;
static double last_jump= 0;

static BMF_Font *spacetext_get_font(SpaceText *st) {
	static BMF_Font *scr12= NULL;
	static BMF_Font *scr15= NULL;
	
	switch (st->font_id) {
	default:
	case 0:
		if (!scr12)
			scr12= BMF_GetFont(BMF_kScreen12);
		return scr12;
	case 1:
		if (!scr15)
			scr15= BMF_GetFont(BMF_kScreen15);
		return scr15;
	}
}

static int spacetext_get_fontwidth(SpaceText *st) {
	return BMF_GetCharacterWidth(spacetext_get_font(st), ' ');
}

static char *temp_char_buf= NULL;
static int *temp_char_accum= NULL;
static int temp_char_len= 0;
static int temp_char_pos= 0;

static void temp_char_write(char c, int accum) {
	if (temp_char_len==0 || temp_char_pos>=temp_char_len) {
		char *nbuf; int *naccum;
		int olen= temp_char_len;
		
		if (olen) temp_char_len*= 2;
		else temp_char_len= 256;
		
		nbuf= MEM_mallocN(sizeof(*temp_char_buf)*temp_char_len, "temp_char_buf");
		naccum= MEM_mallocN(sizeof(*temp_char_accum)*temp_char_len, "temp_char_accum");
		
		if (olen) {
			memcpy(nbuf, temp_char_buf, olen);
			memcpy(naccum, temp_char_accum, olen);
			
			MEM_freeN(temp_char_buf);
			MEM_freeN(temp_char_accum);
		}
		
		temp_char_buf= nbuf;
		temp_char_accum= naccum;
	}
	
	temp_char_buf[temp_char_pos]= c;	
	temp_char_accum[temp_char_pos]= accum;
	
	if (c==0) temp_char_pos= 0;
	else temp_char_pos++;
}

void free_txt_data(void) {
	txt_free_cut_buffer();
	
	if (g_find_str) MEM_freeN(g_find_str);
	if (g_replace_str) MEM_freeN(g_replace_str);
	if (temp_char_buf) MEM_freeN(temp_char_buf);
	if (temp_char_accum) MEM_freeN(temp_char_accum);	
}

static int render_string (SpaceText *st, char *in) {
	int r = 0, i = 0;
	
	while(*in) {
		if (*in=='\t') {
			if (temp_char_pos && *(in-1)=='\t') i= st->tabnumber;
			else if (st->tabnumber > 0) i= st->tabnumber - (temp_char_pos%st->tabnumber);
			while(i--) temp_char_write(' ', r);
		} else temp_char_write(*in, r);

		r++;
		in++;
	}
	r= temp_char_pos;
	temp_char_write(0, 0);
		
	return r;
}

static int find_builtinfunc(char *string)
{
	int a, i;
	char builtinfuncs[][11] = {"and", "as", "assert", "break", "class", "continue", "def",
								"del", "elif", "else", "except", "exec", "finally",
								"for", "from", "global", "if", "import", "in",
								"is", "lambda", "not", "or", "pass", "print",
								"raise", "return", "try", "while", "yield"};
	for (a=0; a<30; a++) {
		i = 0;
		while (1) {
			if (builtinfuncs[a][i]=='\0') {
				if (check_identifier(string[i]))
					i = -1;
				break;
			} else if (string[i]!=builtinfuncs[a][i]) {
				i = -1;
				break;
			}
			i++;
		}
		if (i>0) break;
	}
	return i;
}

static int find_specialvar(char *string) 
{
	int i = 0;
	if (string[0]=='d' && string[1]=='e' && string[2]=='f')
		i = 3;
	else if (string[0]=='c' && string[1]=='l' && string[2]=='a' && string[3]=='s' && string[4]=='s')
		i = 5;
	if (i==0 || check_identifier(string[i]))
		return -1;
	return i;
}

static void print_format(SpaceText *st, TextLine *line) {
	int i, a;
	char *s, *f;
	s = line->line;
	f = line->format;
	for (a=0; *s; s++) {
		if (*s == '\t') {
			for (i=st->tabnumber-(a%st->tabnumber); i>0; i--)
				printf(" "), f++, a++;
		} else
			printf("%c", *s), f++, a++;
	}
	printf("\n%s [%#x]\n", line->format, (int) (f[strlen(f)+1]));
}

/* Ensures the format string for the given line is long enough, reallocating as needed */
static int check_format_len(TextLine *line, unsigned int len) {
	if (line->format) {
		if (strlen(line->format) < len) {
			MEM_freeN(line->format);
			line->format = MEM_mallocN(len+2, "SyntaxFormat");
			if (!line->format) return 0;
		}
	} else {
		line->format = MEM_mallocN(len+2, "SyntaxFormat");
		if (!line->format) return 0;
	}
	return 1;
}

/* Formats the specified line and if allowed and needed will move on to the
 * next line. The format string contains the following characters:
 *		'_'		Whitespace
 *		'#'		Comment text
 *		'!'		Punctuation and other symbols
 *		'n'		Numerals
 *		'l'		String letters
 *		'v'		Special variables (class, def)
 *		'b'		Built-in names (print, for, etc.)
 *		'q'		Other text (identifiers, etc.)
 * It is terminated with a null-terminator '\0' followed by a continuation
 * flag indicating whether the line is part of a multi-line string.
 */
void txt_format_line(SpaceText *st, TextLine *line, int do_next) {
	char *str, *fmt, orig, cont, find, prev = ' ';
	int len, i;

	/* Get continuation from previous line */
	if (line->prev && (fmt=line->prev->format)) {
		cont = fmt[strlen(fmt)+1]; /* Just after the null-terminator */
	} else cont = 0;

	/* Get original continuation from this line */
	if (fmt=line->format) {
		orig = fmt[strlen(fmt)+1]; /* Just after the null-terminator */
	} else orig = 0xFF;

	render_string(st, line->line);
	str = temp_char_buf;
	len = strlen(str);
	if (!check_format_len(line, len)) return;
	fmt = line->format;

	while (*str) {
		/* Handle escape sequences by skipping both \ and next char */
		if (*str == '\\') {
			*fmt = prev; fmt++; str++;
			if (*str == '\0') break;
			*fmt = prev; fmt++; str++;
			continue;
		}
		/* Handle continuations */
		else if (cont) {
			/* Triple strings ("""...""" or '''...''') */
			if (cont & TXT_TRISTR) {
				find = (cont & TXT_DBLQUOTSTR) ? '"' : '\'';
				if (*str==find && *(str+1)==find && *(str+2)==find) {
					*fmt = 'l'; fmt++; str++;
					*fmt = 'l'; fmt++; str++;
					cont = 0;
				}
			/* Handle other strings */
			} else {
				find = (cont & TXT_DBLQUOTSTR) ? '"' : '\'';
				if (*str == find) cont = 0;
			}
			*fmt = 'l';
		}
		/* Not in a string... */
		else {
			/* Deal with comments first */
			if (prev == '#' || *str == '#')
				*fmt = '#';
			/* Strings */
			else if (*str == '"' || *str == '\'') {
				find = *str;
				cont = (*str== '"') ? TXT_DBLQUOTSTR : TXT_SNGQUOTSTR;
				if (*(str+1) == find && *(str+2) == find) {
					*fmt = 'l'; fmt++; str++;
					*fmt = 'l'; fmt++; str++;
					cont |= TXT_TRISTR;
				}
				*fmt = 'l';
			}
			/* Whitespace (all ws. has been converted to spaces) */
			else if (*str == ' ')
				*fmt = '_';
			/* Numbers (digits not part of an identifier and periods followed by digits) */
			else if ((prev != 'q' && check_digit(*str)) || (*str == '.' && check_digit(*(str+1))))
				*fmt = 'n';
			/* Punctuation */
			else if (check_delim(*str))
				*fmt = '!';
			/* Identifiers and other text (no previous ws. or delims. so text continues) */
			else if (prev == 'q')
				*fmt = 'q';
			/* Not ws, a digit, punct, or continuing text. Must be new, check for special words */
			else {
				/* Special vars(v) or built-in keywords(b) */
				if ((i=find_specialvar(str)) != -1)
					prev = 'v';
				else if ((i=find_builtinfunc(str)) != -1)
					prev = 'b';
				if (i>0) {
					while (i>1) {
						*fmt = prev; *fmt++; *str++;
						i--;
					}
					*fmt = prev;
				} else
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
	if (cont!=orig && do_next && line->next) {
		txt_format_line(st, line->next, do_next);
	}
}

void txt_format_text(SpaceText *st) 
{
	TextLine *linep;

	if (!st->text) return;

	for (linep=st->text->lines.first; linep; linep=linep->next)
		txt_format_line(st, linep, 0);
}

static void format_draw_color(char formatchar) {
	switch (formatchar) {
		case '_': /* Whitespace */
			break;
		case '!': /* Symbols */
			BIF_ThemeColorBlend(TH_TEXT, TH_BACK, 0.5f);
			break;
		case '#': /* Comments */
			BIF_ThemeColor(TH_SYNTAX_C);
			break;
		case 'n': /* Numerals */
			BIF_ThemeColor(TH_SYNTAX_N);
			break;
		case 'l': /* Strings */
			BIF_ThemeColor(TH_SYNTAX_L);
			break;
		case 'v': /* Specials: class, def */
			BIF_ThemeColor(TH_SYNTAX_V);
			break;
		case 'b': /* Keywords: for, print, etc. */
			BIF_ThemeColor(TH_SYNTAX_B);
			break;
		case 'q': /* Other text (identifiers) */
		default:
			BIF_ThemeColor(TH_TEXT);
			break;
	}
}

static int text_draw_wrapped(SpaceText *st, char *str, int x, int y, int w, char *format)
{
	int basex, i, a, len, start, end, max, lines;
	
	len= render_string(st, str);
	str= temp_char_buf;
	max= w/spacetext_get_fontwidth(st);
	if (max<8) max= 8;
	basex= x;

	lines= 1;
	start= 0;
	end= max;
	for (i=0; i<len; i++) {
		if (i-start >= max) {
			/* Draw the visible portion of text on the overshot line */
			for (a=start; a<end; a++) {
				if (st->showsyntax && format) format_draw_color(format[a]);
				glRasterPos2i(x, y);
				BMF_DrawCharacter(spacetext_get_font(st), str[a]);
				x += BMF_GetCharacterWidth(spacetext_get_font(st), str[a]);
			}
			y -= st->lheight;
			x= basex;
			lines++;
			start= end;
			end += max;
		} else if (str[i]==' ' || str[i]=='-') {
			end = i+1;
		}
	}
	/* Draw the remaining text */
	for (a=start; a<len; a++) {
		if (st->showsyntax && format) format_draw_color(format[a]);
		glRasterPos2i(x, y);
		BMF_DrawCharacter(spacetext_get_font(st), str[a]);
		x += BMF_GetCharacterWidth(spacetext_get_font(st), str[a]);
	}
	return lines;
}

static int text_draw(SpaceText *st, char *str, int cshift, int maxwidth, int draw, int x, int y, char *format)
{
	int r=0, w= 0;
	char *in;
	int *acc;

	w= render_string(st, str);
	if(w<cshift ) return 0; /* String is shorter than shift */
	
	in= temp_char_buf+cshift;
	acc= temp_char_accum+cshift;
	w= w-cshift;

	if (draw) {
		if(st->showsyntax && format) {
			int amount, a;
			format = format+cshift;
		
			amount = strlen(in);
			
			for(a = 0; a < amount; a++) {
				format_draw_color(format[a]);
				glRasterPos2i(x, y);
				BMF_DrawCharacter(spacetext_get_font(st), in[a]);
				x = x+BMF_GetCharacterWidth(spacetext_get_font(st), in[a]);
			}
		} else {
			glRasterPos2i(x, y);
			BMF_DrawString(spacetext_get_font(st), in);
		}
	} else {
		while (w-- && *acc++ < maxwidth) {
			r+= spacetext_get_fontwidth(st);
		}
	}

	if (cshift && r==0) return 0;
	else if (st->showlinenrs)
		return r+TXT_OFFSET+TEXTXLOC;
	else
		return r+TXT_OFFSET;
}

static void set_cursor_to_pos (SpaceText *st, int x, int y, int sel) 
{
	Text *text;
	TextLine **linep;
	int *charp;
	int w;
	
	text= st->text;

	if(sel) { linep= &text->sell; charp= &text->selc; } 
	else { linep= &text->curl; charp= &text->curc; }
	
	y= (curarea->winy - y)/st->lheight;

	if(st->showlinenrs)
		x-= TXT_OFFSET+TEXTXLOC;
	else
		x-= TXT_OFFSET;

	if (x<0) x= 0;
	x = (x/spacetext_get_fontwidth(st)) + st->left;
	
	if (st->wordwrap) {
		int i, j, endj, curs, max, chop, start, end, chars, loop;
		char ch;

		/* Point to first visible line */
		*linep= text->lines.first;
		for (i=0; i<st->top && (*linep)->next; i++) *linep= (*linep)->next;

		max= get_wrap_width(st);

		loop= 1;
		while (loop && *linep) {
			start= 0;
			end= max;
			chop= 1;
			chars= 0;
			curs= 0;
			for (i=0, j=0; loop; j++) {

				/* Mimic replacement of tabs */
				ch= (*linep)->line[j];
				if (ch=='\t') {
					chars= st->tabnumber-i%st->tabnumber;
					ch= ' ';
				} else
					chars= 1;

				while (chars--) {
					/* Gone too far, go back to last wrap point */
					if (y<0) {
						*charp= endj;
						loop= 0;
						break;
					/* Exactly at the cursor, done */
					} else if (y==0 && i-start==x) {
						*charp= curs= j;
						loop= 0;
						break;
					/* Prepare curs for next wrap */
					} else if (i-end==x) {
						curs= j;
					}
					if (i-start>=max) {
						if (chop) endj= j;
						y--;
						start= end;
						end += max;
						chop= 1;
						if (y==0 && i-start>=x) {
							*charp= curs;
							loop= 0;
							break;
						}
					} else if (ch==' ' || ch=='-' || ch=='\0') {
						if (y==0 && i-start>=x) {
							*charp= curs;
							loop= 0;
							break;
						}
						end = i+1;
						endj = j;
						chop= 0;
					}
					i++;
				}
				if (ch=='\0') break;
			}
			if (!loop || y<0) break;

			if (!(*linep)->next) {
				*charp= (*linep)->len;
				break;
			}
			
			/* On correct line but didn't meet cursor, must be at end */
			if (y==0) {
				*charp= (*linep)->len;
				break;
			}
			*linep= (*linep)->next;
			y--;
		}

	} else {
		y-= txt_get_span(text->lines.first, *linep) - st->top;
		
		if (y>0) {
			while (y-- != 0) if((*linep)->next) *linep= (*linep)->next;
		} else if (y<0) {
			while (y++ != 0) if((*linep)->prev) *linep= (*linep)->prev;
		}

		
		w= render_string(st, (*linep)->line);
		if(x<w) *charp= temp_char_accum[x];
		else *charp= (*linep)->len;
	}
	if(!sel) txt_pop_sel(text);
}

static int get_wrap_width(SpaceText *st) {
	int x, max;
	x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
	max= (curarea->winx-x)/spacetext_get_fontwidth(st);
	return max>8 ? max : 8;
}

/* Returns the number of wrap points (or additional lines) in the given string */
static int get_wrap_points(SpaceText *st, char *line) {
	int start, end, taboffs, i, max, count;
	
	if (!st->wordwrap) return 0;

	end= max= get_wrap_width(st);
	count= taboffs= start= 0;

	for (i=0; line[i]!='\0'; i++) {
		if (i-start+taboffs>=max) {
			count++;
			start= end;
			end += max;
			taboffs= 0;
		} else if (line[i]==' ' || line[i]=='\t' || line[i]=='-') {
			end = i+1;
			if (line[i]=='\t')
				taboffs += st->tabnumber-(i-start)%st->tabnumber;
		}
	}
	return count;
}

/* Sets (offl, offc) for transforming (line, curs) to its wrapped position */
static void wrap_offset(SpaceText *st, TextLine *linein, int cursin, int *offl, int *offc) {
	Text *text;
	TextLine *linep;
	int i, j, start, end, chars, max, chop;
	char ch;

	*offl= *offc= 0;

	if (!st->text) return;
	if (!st->wordwrap) return;

	text= st->text;

	/* Move pointer to first visible line (top) */
	linep= text->lines.first;
	i= st->top;
	while (i>0 && linep) {
		if (linep == linein) return; /* Line before top */
		linep= linep->next;
		i--;
	}

	max= get_wrap_width(st);

	while (linep) {
		start= 0;
		end= max;
		chop= 1;
		chars= 0;
		*offc= 0;
		for (i=0, j=0; linep->line[j]!='\0'; j++) {

			/* Mimic replacement of tabs */
			ch= linep->line[j];
			if (ch=='\t') {
				chars= st->tabnumber-i%st->tabnumber;
				if (linep==linein && i<cursin) cursin += chars-1;
				ch= ' ';
			} else
				chars= 1;

			while (chars--) {
				if (i-start>=max) {
					if (chop && linep==linein && i >= cursin)
						return;
					(*offl)++;
					*offc -= end-start;
					start= end;
					end += max;
					chop= 1;
				} else if (ch==' ' || ch=='-') {
					end = i+1;
					chop= 0;
					if (linep==linein && i >= cursin)
						return;
				}
				i++;
			}
		}
		if (linep==linein) break;
		linep= linep->next;
	}
}

static int get_char_pos(SpaceText *st, char *line, int cur) {
	int a=0, i;
	for (i=0; i<cur && line[i]; i++) {
		if (line[i]=='\t')
			a += st->tabnumber-a%st->tabnumber;
		else
			a++;
	}
	return a;
}

static void draw_markers(SpaceText *st) {
	Text *text= st->text;
	TextMarker *marker, *next;
	TextLine *top, *bottom, *line;
	int offl, offc, i, cy, x1, x2, y1, y2, x, y;

	for (i=st->top, top= text->lines.first; top->next && i>0; i--) top= top->next;
	for (i=st->viewlines-1, bottom=top; bottom->next && i>0; i--) bottom= bottom->next;
	
	for (marker= text->markers.first; marker; marker= next) {
		next= marker->next;
		for (cy= 0, line= top; line; cy++, line= line->next) {
			if (cy+st->top==marker->lineno) {
				/* Remove broken markers */
				if (marker->end>line->len || marker->start>marker->end) {
					BLI_freelinkN(&text->markers, marker);
					break;
				}

				wrap_offset(st, line, marker->start, &offl, &offc);
				x1= get_char_pos(st, line->line, marker->start) - st->left + offc;
				y1= cy + offl;
				wrap_offset(st, line, marker->end, &offl, &offc);
				x2= get_char_pos(st, line->line, marker->end) - st->left + offc;
				y2= cy + offl;

				glColor3ub(marker->color[0], marker->color[1], marker->color[2]);
				x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
				y= curarea->winy-3;

				if (y1==y2) {
					y -= y1*st->lheight;
					glBegin(GL_LINE_LOOP);
					glVertex2i(x+x2*spacetext_get_fontwidth(st)+1, y);
					glVertex2i(x+x1*spacetext_get_fontwidth(st)-2, y);
					glVertex2i(x+x1*spacetext_get_fontwidth(st)-2, y-st->lheight);
					glVertex2i(x+x2*spacetext_get_fontwidth(st)+1, y-st->lheight);
					glEnd();
				} else {
					y -= y1*st->lheight;
					glBegin(GL_LINE_STRIP);
					glVertex2i(curarea->winx, y);
					glVertex2i(x+x1*spacetext_get_fontwidth(st)-2, y);
					glVertex2i(x+x1*spacetext_get_fontwidth(st)-2, y-st->lheight);
					glVertex2i(curarea->winx, y-st->lheight);
					glEnd();
					y-=st->lheight;
					for (i=y1+1; i<y2; i++) {
						glBegin(GL_LINES);
						glVertex2i(x, y);
						glVertex2i(curarea->winx, y);
						glVertex2i(x, y-st->lheight);
						glVertex2i(curarea->winx, y-st->lheight);
						glEnd();
						y-=st->lheight;
					}
					glBegin(GL_LINE_STRIP);
					glVertex2i(x, y);
					glVertex2i(x+x2*spacetext_get_fontwidth(st)+1, y);
					glVertex2i(x+x2*spacetext_get_fontwidth(st)+1, y-st->lheight);
					glVertex2i(x, y-st->lheight);
					glEnd();
				}

				break;
			}
			if (line==bottom) break;
		}
	}
}

static void draw_cursor(SpaceText *st) {
	Text *text= st->text;
	int vcurl, vcurc, vsell, vselc, hidden=0;
	int offl, offc, x, y, w, i;
	
	/* Draw the selection */
	if (text->curl!=text->sell || text->curc!=text->selc) {

		/* Convert all to view space character coordinates */
		wrap_offset(st, text->curl, text->curc, &offl, &offc);
		vcurl = txt_get_span(text->lines.first, text->curl) - st->top + offl;
		vcurc = get_char_pos(st, text->curl->line, text->curc) - st->left + offc;
		wrap_offset(st, text->sell, text->selc, &offl, &offc);
		vsell = txt_get_span(text->lines.first, text->sell) - st->top + offl;
		vselc = get_char_pos(st, text->sell->line, text->selc) - st->left + offc;

		if (vcurc<0) vcurc=0;
		if (vselc<0) vselc=0, hidden=1;
		
		BIF_ThemeColor(TH_SHADE2);
		x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
		y= curarea->winy-2;

		if (vcurl==vsell) {
			y -= vcurl*st->lheight;
			if (vcurc < vselc)
				glRecti(x+vcurc*spacetext_get_fontwidth(st)-1, y, x+vselc*spacetext_get_fontwidth(st), y-st->lheight);
			else
				glRecti(x+vselc*spacetext_get_fontwidth(st)-1, y, x+vcurc*spacetext_get_fontwidth(st), y-st->lheight);
		} else {
			int froml, fromc, tol, toc;
			if (vcurl < vsell) {
				froml= vcurl; tol= vsell;
				fromc= vcurc; toc= vselc;
			} else {
				froml= vsell; tol= vcurl;
				fromc= vselc; toc= vcurc;
			}
			y -= froml*st->lheight;
			glRecti(x+fromc*spacetext_get_fontwidth(st)-1, y, curarea->winx, y-st->lheight); y-=st->lheight;
			for (i=froml+1; i<tol; i++)
				glRecti(x-4, y, curarea->winx, y-st->lheight),  y-=st->lheight;
			glRecti(x-4, y, x+toc*spacetext_get_fontwidth(st), y-st->lheight);  y-=st->lheight;
		}
	} else {
		wrap_offset(st, text->sell, text->selc, &offl, &offc);
		vsell = txt_get_span(text->lines.first, text->sell) - st->top + offl;
		vselc = get_char_pos(st, text->sell->line, text->selc) - st->left + offc;
		if (vselc<0) vselc=0, hidden=1;
	}

	if (!hidden) {
		/* Draw the cursor itself (we draw the sel. cursor as this is the leading edge) */
		x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
		x += vselc*spacetext_get_fontwidth(st);
		y= curarea->winy-2 - vsell*st->lheight;
		
		if (st->overwrite) {
			char ch= text->sell->line[text->selc];
			if (!ch) ch= ' ';
			w= BMF_GetCharacterWidth(spacetext_get_font(st), ch);
			BIF_ThemeColor(TH_HILITE);
			glRecti(x, y-st->lheight-1, x+w, y-st->lheight+1);
		} else {
			BIF_ThemeColor(TH_HILITE);
			glRecti(x-1, y, x+1, y-st->lheight);
		}
	}
}

static void calc_text_rcts(SpaceText *st)
{
	int lhlstart, lhlend, ltexth;
	short barheight, barstart, hlstart, hlend, blank_lines;
	short pix_available, pix_top_margin, pix_bottom_margin, pix_bardiff;

	pix_top_margin = 8;
	pix_bottom_margin = 4;
	pix_available = curarea->winy - pix_top_margin - pix_bottom_margin;
	ltexth= txt_get_span(st->text->lines.first, st->text->lines.last);
	blank_lines = st->viewlines / 2;
	
	/* when resizing a vieport with the bar at the bottom to a greater height more blank lines will be added */
	if (ltexth + blank_lines < st->top + st->viewlines) {
		blank_lines = st->top + st->viewlines - ltexth;
	}
	
	ltexth += blank_lines;

	barheight = (ltexth > 0)? (st->viewlines*pix_available)/ltexth: 0;
	pix_bardiff = 0;
	if (barheight < 20) {
		pix_bardiff = 20 - barheight; /* take into account the now non-linear sizing of the bar */	
		barheight = 20;
	}
	barstart = (ltexth > 0)? ((pix_available - pix_bardiff) * st->top)/ltexth: 0;

	st->txtbar.xmin = 5;
	st->txtbar.xmax = 17;
	st->txtbar.ymax = curarea->winy - pix_top_margin - barstart;
	st->txtbar.ymin = st->txtbar.ymax - barheight;

	CLAMP(st->txtbar.ymin, pix_bottom_margin, curarea->winy - pix_top_margin);
	CLAMP(st->txtbar.ymax, pix_bottom_margin, curarea->winy - pix_top_margin);

	st->pix_per_line= (pix_available > 0)? (float) ltexth/pix_available: 0;
	if (st->pix_per_line<.1) st->pix_per_line=.1f;

	lhlstart = MIN2(txt_get_span(st->text->lines.first, st->text->curl), 
				txt_get_span(st->text->lines.first, st->text->sell));
	lhlend = MAX2(txt_get_span(st->text->lines.first, st->text->curl), 
				txt_get_span(st->text->lines.first, st->text->sell));

	if(ltexth > 0) {
		hlstart = (lhlstart * pix_available)/ltexth;
		hlend = (lhlend * pix_available)/ltexth;

		/* the scrollbar is non-linear sized */
		if (pix_bardiff > 0) {
			/* the start of the highlight is in the current viewport */
			if (ltexth && st->viewlines && lhlstart >= st->top && lhlstart <= st->top + st->viewlines) { 
				/* speed the progresion of the start of the highlight through the scrollbar */
				hlstart = ( ( (pix_available - pix_bardiff) * lhlstart) / ltexth) + (pix_bardiff * (lhlstart - st->top) / st->viewlines); 	
			}
			else if (lhlstart > st->top + st->viewlines && hlstart < barstart + barheight && hlstart > barstart) {
				/* push hl start down */
				hlstart = barstart + barheight;
			}
			else if (lhlend > st->top  && lhlstart < st->top && hlstart > barstart) {
				/*fill out start */
				hlstart = barstart;
			}

			if (hlend <= hlstart) { 
				hlend = hlstart + 2;
			}

			/* the end of the highlight is in the current viewport */
			if (ltexth && st->viewlines && lhlend >= st->top && lhlend <= st->top + st->viewlines) { 
				/* speed the progresion of the end of the highlight through the scrollbar */
				hlend = (((pix_available - pix_bardiff )*lhlend)/ltexth) + (pix_bardiff * (lhlend - st->top)/st->viewlines); 	
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
	
	st->txtscroll.xmin= 5;
	st->txtscroll.xmax= 17;
	st->txtscroll.ymax= curarea->winy - pix_top_margin - hlstart;
	st->txtscroll.ymin= curarea->winy - pix_top_margin - hlend;

	CLAMP(st->txtscroll.ymin, pix_bottom_margin, curarea->winy - pix_top_margin);
	CLAMP(st->txtscroll.ymax, pix_bottom_margin, curarea->winy - pix_top_margin);
}

static void draw_textscroll(SpaceText *st)
{
	if (!st->text) return;

	calc_text_rcts(st);
	
	BIF_ThemeColorShade(TH_SHADE1, -20);
	glRecti(2, 2, 20, curarea->winy-6);
	uiEmboss(2, 2, 20, curarea->winy-6, 1);

	BIF_ThemeColor(TH_SHADE1);
	glRecti(st->txtbar.xmin, st->txtbar.ymin, st->txtbar.xmax, st->txtbar.ymax);

	BIF_ThemeColor(TH_SHADE2);
	glRecti(st->txtscroll.xmin, st->txtscroll.ymin, st->txtscroll.xmax, st->txtscroll.ymax);

	uiEmboss(st->txtbar.xmin, st->txtbar.ymin, st->txtbar.xmax, st->txtbar.ymax, st->flags & ST_SCROLL_SELECT);
}

static void screen_skip(SpaceText *st, int lines)
{
	int last;
	
	if (!st) return;
	if (st->spacetype != SPACE_TEXT) return;
	if (!st->text) return;

 	st->top += lines;

	last= txt_get_span(st->text->lines.first, st->text->lines.last);
	last= last - (st->viewlines/2);
	
	if (st->top>last) st->top= last;
	if (st->top<0) st->top= 0;
}

static void cursor_skip(SpaceText *st, int lines, int sel)
{
	Text *text;
	TextLine **linep;
	int oldl, oldc, *charp;
	
	if (!st) return;
	if (st->spacetype != SPACE_TEXT) return;
	if (!st->text) return;

	text= st->text;

	if (sel) linep= &text->sell, charp= &text->selc;
	else linep= &text->curl, charp= &text->curc;
	oldl= txt_get_span(text->lines.first, *linep);
	oldc= *charp;

	while (lines>0 && (*linep)->next) {
		*linep= (*linep)->next;
		lines--;
	}
	while (lines<0 && (*linep)->prev) {
		*linep= (*linep)->prev;
		lines++;
	}

	if (*charp > (*linep)->len) *charp= (*linep)->len;

	if (!sel) txt_pop_sel(st->text);
	txt_undo_add_toop(st->text, sel?UNDO_STO:UNDO_CTO, oldl, oldc, txt_get_span(text->lines.first, *linep), *charp);
}

/* 
 * mode 1 == view scroll
 * mode 2 == scrollbar
 */
static void do_textscroll(SpaceText *st, int mode)
{
	short delta[2]= {0, 0};
	short mval[2], hold[2], old[2];
	
	if (!st->text) return;
	
	calc_text_rcts(st);

	st->flags|= ST_SCROLL_SELECT;

	scrarea_do_windraw(curarea);
	screen_swapbuffers();

	getmouseco_areawin(mval);
	old[0]= hold[0]= mval[0];
	old[1]= hold[1]= mval[1];

	while(get_mbut()&(L_MOUSE|M_MOUSE)) {
		getmouseco_areawin(mval);

		if(old[0]!=mval[0] || old[1]!=mval[1]) {
			if (mode==1) {
				delta[0]= (hold[0]-mval[0])/spacetext_get_fontwidth(st);
				delta[1]= (mval[1]-hold[1])/st->lheight;
			}
			else delta[1]= (hold[1]-mval[1])*st->pix_per_line;
			
			if (delta[0] || delta[1]) {
				screen_skip(st, delta[1]);
				if (st->wordwrap) {
					st->left= 0;
				} else {
					st->left+= delta[0];
					if (st->left<0) st->left= 0;
				}
				scrarea_do_windraw(curarea);
				screen_swapbuffers();
				
				hold[0]=mval[0];
				hold[1]=mval[1];
			}
			old[0]=mval[0];
			old[1]=mval[1];
		} else {
			BIF_wait_for_statechange();
		}
	}
	st->flags^= ST_SCROLL_SELECT;

	scrarea_do_windraw(curarea);
	screen_swapbuffers();
}

static void do_selection(SpaceText *st, int selecting)
{
	short mval[2], old[2];
	int sell, selc;
	int linep2, charp2;
	int first= 1;

	getmouseco_areawin(mval);
	old[0]= mval[0];
	old[1]= mval[1];

	if (!selecting) {
		int curl= txt_get_span(st->text->lines.first, st->text->curl);
		int curc= st->text->curc;			
		int linep2, charp2;
					
		set_cursor_to_pos(st, mval[0], mval[1], 0);

		linep2= txt_get_span(st->text->lines.first, st->text->curl);
		charp2= st->text->selc;
				
		if (curl!=linep2 || curc!=charp2)
			txt_undo_add_toop(st->text, UNDO_CTO, curl, curc, linep2, charp2);
	}

	sell= txt_get_span(st->text->lines.first, st->text->sell);
	selc= st->text->selc;

	while(get_mbut()&L_MOUSE) {
		getmouseco_areawin(mval);

		if (mval[1]<0 || mval[1]>curarea->winy) {
			int d= (old[1]-mval[1])*st->pix_per_line;
			if (d) screen_skip(st, d);

			set_cursor_to_pos(st, mval[0], mval[1]<0?0:curarea->winy, 1);

			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		} else if (!st->wordwrap && (mval[0]<0 || mval[0]>curarea->winx)) {
			if (mval[0]>curarea->winx) st->left++;
			else if (mval[0]<0 && st->left>0) st->left--;
			
			set_cursor_to_pos(st, mval[0], mval[1], 1);
			
			scrarea_do_windraw(curarea);
			screen_swapbuffers();
			
			PIL_sleep_ms(10);
		} else if (first || old[0]!=mval[0] || old[1]!=mval[1]) {
			set_cursor_to_pos(st, mval[0], mval[1], 1);

			scrarea_do_windraw(curarea);
			screen_swapbuffers();

			old[0]= mval[0];
			old[1]= mval[1];
			first= 1;
		} else {
			BIF_wait_for_statechange();
		}
	}

	linep2= txt_get_span(st->text->lines.first, st->text->sell);
	charp2= st->text->selc;
		
	if (sell!=linep2 || selc!=charp2)
		txt_undo_add_toop(st->text, UNDO_STO, sell, selc, linep2, charp2);

	pop_space_text(st);
}

static int do_suggest_select(SpaceText *st)
{
	SuggItem *item, *first, *last, *sel;
	short mval[2];
	TextLine *tmp;
	int l, x, y, w, h, i;
	int tgti, *top;
	
	if (!st || !st->text) return 0;
	if (!texttool_text_is_active(st->text)) return 0;

	first = texttool_suggest_first();
	last = texttool_suggest_last();
	sel = texttool_suggest_selected();
	top = texttool_suggest_top();

	if (!last || !first)
		return 0;

	/* Count the visible lines to the cursor */
	for (tmp=st->text->curl, l=-st->top; tmp; tmp=tmp->prev, l++);
	if (l<0) return 0;
	
	if(st->showlinenrs) {
		x = spacetext_get_fontwidth(st)*(st->text->curc-st->left) + TXT_OFFSET + TEXTXLOC - 4;
	} else {
		x = spacetext_get_fontwidth(st)*(st->text->curc-st->left) + TXT_OFFSET - 4;
	}
	y = curarea->winy - st->lheight*l - 2;

	w = SUGG_LIST_WIDTH*spacetext_get_fontwidth(st) + 20;
	h = SUGG_LIST_SIZE*st->lheight + 8;

	getmouseco_areawin(mval);

	if (mval[0]<x || x+w<mval[0] || mval[1]<y-h || y<mval[1])
		return 0;

	/* Work out which of the items is at the top of the visible list */
	for (i=0, item=first; i<*top && item->next; i++, item=item->next);

	/* Work out the target item index in the visible list */
	tgti = (y-mval[1]-4) / st->lheight;
	if (tgti<0 || tgti>SUGG_LIST_SIZE)
		return 1;

	for (i=tgti; i>0 && item->next; i--, item=item->next);
	if (item)
		texttool_suggest_select(item);
	return 1;
}

static void pop_suggest_list() {
	SuggItem *item, *sel;
	int *top, i;

	item= texttool_suggest_first();
	sel= texttool_suggest_selected();
	top= texttool_suggest_top();

	i= 0;
	while (item && item != sel) {
		item= item->next;
		i++;
	}
	if (i > *top+SUGG_LIST_SIZE-1)
		*top= i-SUGG_LIST_SIZE+1;
	else if (i < *top)
		*top= i;
}

void draw_documentation(SpaceText *st)
{
	TextLine *tmp;
	char *docs, buf[DOC_WIDTH+1], *p;
	int len, i, br, lines;
	int boxw, boxh, l, x, y, top;
	
	if (!st || !st->text) return;
	if (!texttool_text_is_active(st->text)) return;
	
	docs = texttool_docs_get();

	if (!docs) return;

	/* Count the visible lines to the cursor */
	for (tmp=st->text->curl, l=-st->top; tmp; tmp=tmp->prev, l++);
	if (l<0) return;
	
	if(st->showlinenrs) {
		x= spacetext_get_fontwidth(st)*(st->text->curc-st->left) + TXT_OFFSET + TEXTXLOC - 4;
	} else {
		x= spacetext_get_fontwidth(st)*(st->text->curc-st->left) + TXT_OFFSET - 4;
	}
	if (texttool_suggest_first()) {
		x += SUGG_LIST_WIDTH*spacetext_get_fontwidth(st) + 50;
	}

	top= y= curarea->winy - st->lheight*l - 2;
	len= strlen(docs);
	boxw= DOC_WIDTH*spacetext_get_fontwidth(st) + 20;
	boxh= (DOC_HEIGHT+1)*st->lheight;

	/* Draw panel */
	BIF_ThemeColor(TH_BACK);
	glRecti(x, y, x+boxw, y-boxh);
	BIF_ThemeColor(TH_SHADE1);
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
	BIF_ThemeColor(TH_TEXT);

	i= 0; br= DOC_WIDTH; lines= -doc_scroll;
	for (p=docs; *p; p++) {
		if (*p == '\r' && *(++p) != '\n') *(--p)= '\n'; /* Fix line endings */
		if (*p == ' ' || *p == '\t')
			br= i;
		else if (*p == '\n') {
			buf[i]= '\0';
			if (lines>=0) {
				y -= st->lheight;
				text_draw(st, buf, 0, 0, 1, x+4, y-3, NULL);
			}
			i= 0; br= DOC_WIDTH; lines++;
		}
		buf[i++]= *p;
		if (i == DOC_WIDTH) { /* Reached the width, go to last break and wrap there */
			buf[br]= '\0';
			if (lines>=0) {
				y -= st->lheight;
				text_draw(st, buf, 0, 0, 1, x+4, y-3, NULL);
			}
			p -= i-br-1; /* Rewind pointer to last break */
			i= 0; br= DOC_WIDTH; lines++;
		}
		if (lines >= DOC_HEIGHT) break;
	}
	if (doc_scroll > 0 && lines < DOC_HEIGHT) {
		doc_scroll--;
		draw_documentation(st);
	}
}

void draw_suggestion_list(SpaceText *st)
{
	SuggItem *item, *first, *last, *sel;
	TextLine *tmp;
	char str[SUGG_LIST_WIDTH+1];
	int w, boxw=0, boxh, i, l, x, y, b, *top;
	
	if (!st || !st->text) return;
	if (!texttool_text_is_active(st->text)) return;

	first = texttool_suggest_first();
	last = texttool_suggest_last();

	if (!first || !last) return;

	pop_suggest_list();
	sel = texttool_suggest_selected();
	top = texttool_suggest_top();

	/* Count the visible lines to the cursor */
	for (tmp=st->text->curl, l=-st->top; tmp; tmp=tmp->prev, l++);
	if (l<0) return;
	
	if(st->showlinenrs) {
		x = spacetext_get_fontwidth(st)*(st->text->curc-st->left) + TXT_OFFSET + TEXTXLOC - 4;
	} else {
		x = spacetext_get_fontwidth(st)*(st->text->curc-st->left) + TXT_OFFSET - 4;
	}
	y = curarea->winy - st->lheight*l - 2;

	boxw = SUGG_LIST_WIDTH*spacetext_get_fontwidth(st) + 20;
	boxh = SUGG_LIST_SIZE*st->lheight + 8;
	
	BIF_ThemeColor(TH_SHADE1);
	glRecti(x-1, y+1, x+boxw+1, y-boxh-1);
	BIF_ThemeColor(TH_BACK);
	glRecti(x, y, x+boxw, y-boxh);

	/* Set the top 'item' of the visible list */
	for (i=0, item=first; i<*top && item->next; i++, item=item->next);

	for (i=0; i<SUGG_LIST_SIZE && item; i++, item=item->next) {

		y -= st->lheight;

		strncpy(str, item->name, SUGG_LIST_WIDTH);
		str[SUGG_LIST_WIDTH] = '\0';

		w = BMF_GetStringWidth(spacetext_get_font(st), str);
		
		if (item == sel) {
			BIF_ThemeColor(TH_SHADE2);
			glRecti(x+16, y-3, x+16+w, y+st->lheight-3);
		}
		b=1; /* b=1 colour block, text is default. b=0 no block, colour text */
		switch (item->type) {
			case 'k': BIF_ThemeColor(TH_SYNTAX_B); b=0; break;
			case 'm': BIF_ThemeColor(TH_TEXT); break;
			case 'f': BIF_ThemeColor(TH_SYNTAX_L); break;
			case 'v': BIF_ThemeColor(TH_SYNTAX_N); break;
			case '?': BIF_ThemeColor(TH_TEXT); b=0; break;
		}
		if (b) {
			glRecti(x+8, y+2, x+11, y+5);
			BIF_ThemeColor(TH_TEXT);
		}
		text_draw(st, str, 0, 0, 1, x+16, y-1, NULL);

		if (item == last) break;
	}
}

static short check_blockhandler(SpaceText *st, short handler) {
	short a;
	for(a=0; a<SPACE_MAXHANDLER; a+=2)
		if (st->blockhandler[a]==handler) return 1;
	return 0;
}

static void text_panel_find(short cntrl)	// TEXT_HANDLER_FIND
{
	uiBlock *block;

	if (!g_find_str || !g_replace_str) {
		g_find_str= MEM_mallocN(TXT_MAXFINDSTR+1, "find_string");
		g_replace_str= MEM_mallocN(TXT_MAXFINDSTR+1, "replace_string");
		g_find_str[0]= g_replace_str[0]= '\0';
	}
	
	block= uiNewBlock(&curarea->uiblocks, "text_panel_find", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(TEXT_HANDLER_FIND);  // for close and esc
	if(uiNewPanel(curarea, block, "Find & Replace", "Text", curarea->winx-230, curarea->winy-130, 260, 120)==0) return;
	
	uiBlockBeginAlign(block);
	uiDefButC(block, TEX, 0, "Find: ", 0,80,220,20, g_find_str, 0,(float)TXT_MAXFINDSTR, 0,0, "");
	uiDefIconBut(block, BUT, B_PASTEFIND, ICON_TEXT, 220,80,20,20, NULL, 0,0,0,0, "Copy from selection");
	uiDefButC(block, TEX, 0, "Replace: ", 0,60,220,20, g_replace_str, 0,(float)TXT_MAXFINDSTR, 0,0, "");
	uiDefIconBut(block, BUT, B_PASTEREPLACE, ICON_TEXT, 220,60,20,20, NULL, 0,0,0,0, "Copy from selection");
	uiBlockEndAlign(block);
	uiDefButBitI(block, TOG, TXT_FIND_WRAP,    0,"Wrap Around", 0,30,110,20,&g_find_flags,0,0,0,0,"Wrap search around current text");
	uiDefButBitI(block, TOG, TXT_FIND_ALLTEXTS,0,"Search All Texts",  110,30,130,20,&g_find_flags,0,0,0,0,"Search in each text");
	uiDefBut(block, BUT, B_TEXTFIND,    "Find",       0,0,50,20, NULL, 0,0,0,0, "Find next");
	uiDefBut(block, BUT, B_TEXTREPLACE, "Replace/Find", 50,0,110,20, NULL, 0,0,0,0, "Replace then find next");
	uiDefBut(block, BUT, B_TEXTMARKALL, "Mark All",   160,0,80,20, NULL, 0,0,0,0, "Mark each occurrence to edit all from one");
}

/* mode: 0 find only, 1 replace/find, 2 mark all occurrences */
void find_and_replace(SpaceText *st, short mode) {
	char *tmp;
	Text *start= NULL, *text= st->text;
	int flags, first= 1;

	if (!check_blockhandler(st, TEXT_HANDLER_FIND)) {
		toggle_blockhandler(st->area, TEXT_HANDLER_FIND, UI_PNL_TO_MOUSE);
		return;
	}

	if (!g_find_str || !g_replace_str) return;
	if (g_find_str[0] == '\0') return;
	flags= g_find_flags;
	if (flags & TXT_FIND_ALLTEXTS) flags ^= TXT_FIND_WRAP;

	do {
		if (first)
			txt_clear_markers(text, TMARK_GRP_FINDALL, 0);
		first= 0;
		
		/* Replace current */
		if (mode && txt_has_sel(text)) {
			tmp= txt_sel_to_buf(text);
			if (strcmp(g_find_str, tmp)==0) {
				if (mode==1) {
					txt_insert_buf(text, g_replace_str);
					if (st->showsyntax) txt_format_line(st, text->curl, 1);
				} else if (mode==2) {
					char color[4];
					BIF_GetThemeColor4ubv(TH_SHADE2, color);
					if (txt_find_marker(text, text->curl, text->selc, TMARK_GRP_FINDALL, 0)) {
						if (tmp) MEM_freeN(tmp), tmp=NULL;
						break;
					}
					txt_add_marker(text, text->curl, text->curc, text->selc, color, TMARK_GRP_FINDALL, TMARK_EDITALL);
				}
			}
			MEM_freeN(tmp);
			tmp= NULL;
		}

		/* Find next */
		if (txt_find_string(text, g_find_str, flags & TXT_FIND_WRAP)) {
			pop_space_text(st);
		} else if (flags & TXT_FIND_ALLTEXTS) {
			if (text==start) break;
			if (!start) start= text;
			if (text->id.next)
				text= st->text= text->id.next;
			else
				text= st->text= G.main->text.first;
			txt_move_toline(text, 0, 0);
			pop_space_text(st);
			first= 1;
		} else {
			okee("Text not found: %s", g_find_str);
			break;
		}
	} while (mode==2);
}

static void do_find_buttons(val) {
	Text *text;
	SpaceText *st;
	int do_draw= 0;
	char *tmp;

	st= curarea->spacedata.first;
	if (!st || st->spacetype != SPACE_TEXT) return;
	text= st->text;
	if (!text) return;

	switch (val) {
		case B_PASTEFIND:
			if (!g_find_str) break;
			tmp= txt_sel_to_buf(text);
			strncpy(g_find_str, tmp, TXT_MAXFINDSTR);
			MEM_freeN(tmp);
			do_draw= 1;
			break;
		case B_PASTEREPLACE:
			if (!g_replace_str) break;
			tmp= txt_sel_to_buf(text);
			strncpy(g_replace_str, tmp, TXT_MAXFINDSTR);
			MEM_freeN(tmp);
			do_draw= 1;
			break;
		case B_TEXTFIND:
			find_and_replace(st, 0);
			do_draw= 1;
			break;
		case B_TEXTREPLACE:
			find_and_replace(st, 1);
			do_draw= 1;
			break;
		case B_TEXTMARKALL:
			find_and_replace(st, 2);
			do_draw= 1;
			break;
	}
}

static void text_blockhandlers(ScrArea *sa)
{
	SpaceText *st= sa->spacedata.first;
	short a;

	/* warning; blocks need to be freed each time, handlers dont remove */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);
	
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		/* clear action value for event */
		switch(st->blockhandler[a]) {
			case TEXT_HANDLER_FIND:
				text_panel_find(st->blockhandler[a+1]);
				break;
		}
	}
	uiDrawBlocksPanels(sa, 0);
}

void drawtextspace(ScrArea *sa, void *spacedata)
{
	SpaceText *st= curarea->spacedata.first;
	Text *text;
	int i, x, y;
	TextLine *tmp;
	char linenr[12];
	float col[3];
	int linecount = 0;

	if (st==NULL || st->spacetype != SPACE_TEXT) return;

	bwin_clear_viewmat(sa->win);	/* clear buttons view */
	glLoadIdentity();
	
	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	myortho2(-0.375, (float)(sa->winx)-0.375, -0.375, (float)(sa->winy)-0.375);

	draw_area_emboss(sa);

	text= st->text;
	if(!text) return;
	
	/* Make sure all the positional pointers exist */
	if (!text->curl || !text->sell || !text->lines.first || !text->lines.last)
		txt_clean_text(text);
	
	if(st->lheight) st->viewlines= (int) curarea->winy/st->lheight;
	else st->viewlines= 0;
	
	if(st->showlinenrs) {
		cpack(0x8c787c);
		glRecti(23,  0, (st->lheight==15)?63:59,  curarea->winy - 2);
	}

	draw_cursor(st);

	tmp= text->lines.first;
	for (i= 0; i<st->top && tmp; i++) {
		if (st->showsyntax && !tmp->format) txt_format_line(st, tmp, 0);
		tmp= tmp->next;
		linecount++;
	}

	y= curarea->winy-st->lheight;
	x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;

	BIF_ThemeColor(TH_TEXT);
	for (i=0; y>0 && i<st->viewlines && tmp; i++, tmp= tmp->next) {
		if (st->showsyntax && !tmp->format) {
			txt_format_line(st, tmp, 0);
		}
		if(st->showlinenrs) {
			/*Change the color of the current line the cursor is on*/
			if(tmp == text->curl) { 
				BIF_ThemeColor(TH_HILITE);
			} else {
				BIF_ThemeColor(TH_TEXT);
			}
			if(((float)(i + linecount + 1)/10000.0) < 1.0) {
				sprintf(linenr, "%4d", i + linecount + 1);
				glRasterPos2i(TXT_OFFSET - 7, y);
			} else {
				sprintf(linenr, "%5d", i + linecount + 1);
				glRasterPos2i(TXT_OFFSET - 11, y);
			}
			BIF_ThemeColor(TH_TEXT);
			BMF_DrawString(spacetext_get_font(st), linenr);
		}
		if (st->wordwrap) {
			int lines = text_draw_wrapped(st, tmp->line, x, y, curarea->winx-x, tmp->format);
			y -= lines*st->lheight;
		} else {
			text_draw(st, tmp->line, st->left, 0, 1, x, y, tmp->format);
			y -= st->lheight;
		}
	}
	
	draw_brackets(st);
	draw_markers(st);

	draw_textscroll(st);
	draw_documentation(st);
	draw_suggestion_list(st);
	
	bwin_scalematrix(sa->win, st->blockscale, st->blockscale, st->blockscale);
	text_blockhandlers(sa);
	
	curarea->win_swap= WIN_BACK_OK;
}

/* Moves the view to the cursor location,
  also used to make sure the view isnt outside the file */
void pop_space_text (SpaceText *st)
{
	int i, x;

	if(!st) return;
	if(!st->text) return;
	if(!st->text->curl) return;
		
	i= txt_get_span(st->text->lines.first, st->text->sell);
	if (st->top+st->viewlines <= i || st->top > i) {
		st->top= i - st->viewlines/2;
	}
	
	if (st->wordwrap) {
		st->left= 0;
	} else {
		x= text_draw(st, st->text->sell->line, st->left, st->text->selc, 0, 0, 0, NULL);

		if (x==0 || x>curarea->winx) {
			st->left= st->text->curc-0.5*(curarea->winx)/spacetext_get_fontwidth(st);
		}
	}

	if (st->top < 0) st->top= 0;
	if (st->left <0) st->left= 0;
}

void add_text_fs(char *file) /* bad but cant pass an as arg here */
{
	SpaceText *st= curarea->spacedata.first;
	Text *text;

	if (st==NULL || st->spacetype != SPACE_TEXT) return;

	text= add_text(file);

	st->text= text;

	st->top= 0;

	if (st->showsyntax) txt_format_text(st);
	allqueue(REDRAWTEXT, 0);
	allqueue(REDRAWHEADERS, 0);	
}

void free_textspace(SpaceText *st)
{
	if (!st) return;

	st->text= NULL;
}

/* returns 0 if file on disk is the same or Text is in memory only
   returns 1 if file has been modified on disk since last local edit
   returns 2 if file on disk has been deleted
   -1 is returned if an error occurs
*/
int txt_file_modified(Text *text)
{
	struct stat st;
	int result;
	char file[FILE_MAXDIR+FILE_MAXFILE];

	if (!text || !text->name)
		return 0;

	BLI_strncpy(file, text->name, FILE_MAXDIR+FILE_MAXFILE);
	BLI_convertstringcode(file, G.sce);

	if (!BLI_exists(file))
		return 2;

	result = stat(file, &st);
	
	if(result == -1)
		return -1;

	if((st.st_mode & S_IFMT) != S_IFREG)
		return -1;

	if (st.st_mtime > text->mtime)
		return 1;

	return 0;
}

void txt_ignore_modified(Text *text) {
	struct stat st;
	int result;
	char file[FILE_MAXDIR+FILE_MAXFILE];

	if (!text || !text->name) return;

	BLI_strncpy(file, text->name, FILE_MAXDIR+FILE_MAXFILE);
	BLI_convertstringcode(file, G.sce);

	if (!BLI_exists(file)) return;

	result = stat(file, &st);
	
	if(result == -1 || (st.st_mode & S_IFMT) != S_IFREG)
		return;

	text->mtime= st.st_mtime;
}

static void save_mem_text(char *str)
{
	SpaceText *st= curarea->spacedata.first;
	Text *text;
	
	if (!str) return;
	
	if (!st) return;
	if (st->spacetype != SPACE_TEXT) return;

	text= st->text;
	if(!text) return;
	
	if (text->name) MEM_freeN(text->name);
	text->name= MEM_mallocN(strlen(str)+1, "textname");
	strcpy(text->name, str);

	text->flags ^= TXT_ISMEM;
		
	txt_write_file(text);
}

void txt_write_file(Text *text) 
{
	FILE *fp;
	TextLine *tmp;
	int res;
	struct stat st;
	char file[FILE_MAXDIR+FILE_MAXFILE];
	
	/* Do we need to get a filename? */
	if (text->flags & TXT_ISMEM) {
		if (text->name)
			activate_fileselect(FILE_SPECIAL, "SAVE TEXT FILE", text->name, save_mem_text);
		else
			activate_fileselect(FILE_SPECIAL, "SAVE TEXT FILE", text->id.name+2, save_mem_text);
		return;
	}

	BLI_strncpy(file, text->name, FILE_MAXDIR+FILE_MAXFILE);
	BLI_convertstringcode(file, G.sce);
	
	/* Should we ask to save over? */
	if (text->flags & TXT_ISTMP) {
		if (BLI_exists(file)) {
			if (!okee("Save over")) return;
		} else if (!okee("Create new file")) return;

		text->flags ^= TXT_ISTMP;
	}
		
	fp= fopen(file, "w");
	if (fp==NULL) {
		error("Unable to save file");
		return;
	}

	tmp= text->lines.first;
	while (tmp) {
		if (tmp->next) fprintf(fp, "%s\n", tmp->line);
		else fprintf(fp, "%s", tmp->line);
		
		tmp= tmp->next;
	}
	
	fclose (fp);

	res= stat(file, &st);
	text->mtime= st.st_mtime;
	
	if (text->flags & TXT_ISDIRTY) text->flags ^= TXT_ISDIRTY;
}

void unlink_text(Text *text)
{
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;
	
	/* check if this text was used as script link:
	 * this check function unsets the pointers and returns how many
	 * script links used this Text */
	if (BPY_check_all_scriptlinks (text)) {
		allqueue(REDRAWBUTSSCRIPT, 0);
	}
	/* equivalently for pynodes: */
	if (nodeDynamicUnlinkText ((ID*)text)) {
		allqueue(REDRAWNODE, 0);
	}

	for (scr= G.main->screen.first; scr; scr= scr->id.next) {
		for (area= scr->areabase.first; area; area= area->next) {
			for (sl= area->spacedata.first; sl; sl= sl->next) {
				if (sl->spacetype==SPACE_TEXT) {
					SpaceText *st= (SpaceText*) sl;
					
					if (st->text==text) {
						st->text= NULL;
						st->top= 0;
						
						if (st==area->spacedata.first) {
							scrarea_queue_redraw(area);
						}
					}
				}
			}
		}
	}
}

int jumptoline_interactive(SpaceText *st) {
	short nlines= txt_get_span(st->text->lines.first, st->text->lines.last)+1;
	short tmp= txt_get_span(st->text->lines.first, st->text->curl)+1;

	if (button(&tmp, 1, nlines, "Jump to line:")) {
		txt_move_toline(st->text, tmp-1, 0);
		pop_space_text(st);
		return 1;
	} else {
		return 0;
	}
}


int bufferlength;
static char *copybuffer = NULL;

void txt_copy_selectbuffer (Text *text)
{
	int length=0;
	TextLine *tmp, *linef, *linel;
	int charf, charl;
	
	if (!text) return;
	if (!text->curl) return;
	if (!text->sell) return;

	if (!txt_has_sel(text)) return;
	
	if (copybuffer) {
		MEM_freeN(copybuffer);
		copybuffer= NULL;
	}

	if (text->curl==text->sell) {
		linef= linel= text->curl;
		
		if (text->curc < text->selc) {
			charf= text->curc;
			charl= text->selc;
		} else{
			charf= text->selc;
			charl= text->curc;
		}
	} else if (txt_get_span(text->curl, text->sell)<0) {
		linef= text->sell;
		linel= text->curl;

		charf= text->selc;		
		charl= text->curc;
	} else {
		linef= text->curl;
		linel= text->sell;
		
		charf= text->curc;
		charl= text->selc;
	}

	if (linef == linel) {
		length= charl-charf;

		copybuffer= MEM_mallocN(length+1, "cut buffera");
		
		BLI_strncpy(copybuffer, linef->line + charf, length+1);
	} else {
		length+= linef->len - charf;
		length+= charl;
		length++; /* For the '\n' */
		
		tmp= linef->next;
		while (tmp && tmp!= linel) {
			length+= tmp->len+1;
			tmp= tmp->next;
		}
		
		copybuffer= MEM_mallocN(length+1, "cut bufferb");
		
		strncpy(copybuffer, linef->line+ charf, linef->len-charf);
		length= linef->len-charf;
		
		copybuffer[length++]='\n';
		
		tmp= linef->next;
		while (tmp && tmp!=linel) {
			strncpy(copybuffer+length, tmp->line, tmp->len);
			length+= tmp->len;
			
			copybuffer[length++]='\n';			
			
			tmp= tmp->next;
		}
		strncpy(copybuffer+length, linel->line, charl);
		length+= charl;
		
		copybuffer[length]=0;
	}

	bufferlength = length;
}

static char *unixNewLine(char *buffer)
{
	char *p, *p2, *output;
	
	/* we can afford the few extra bytes */
	output= MEM_callocN(strlen(buffer)+1, "unixnewline");
	for (p= buffer, p2= output; *p; p++)
		if (*p != '\r') *(p2++)= *p;
	
	*p2= 0;
	return(output);
}

static char *winNewLine(char *buffer)
{
	char *p, *p2, *output;
	int add= 0;
	
	for (p= buffer; *p; p++)
		if (*p == '\n') add++;
		
	bufferlength= p-buffer+add+1;
	output= MEM_callocN(bufferlength, "winnewline");
	for (p= buffer, p2= output; *p; p++, p2++) {
		if (*p == '\n') { 
			*(p2++)= '\r'; *p2= '\n';
		} else *p2= *p;
	}
	*p2= 0;
	
	return(output);
}

void txt_paste_clipboard(Text *text) {

	char * buff;
	char *temp_buff;
	
	buff = (char*)getClipboard(0);
	if(buff) {
		temp_buff = unixNewLine(buff);
		
		txt_insert_buf(text, temp_buff);
		if(buff){free((void*)buff);}
		if(temp_buff){MEM_freeN(temp_buff);}
	}
}

void get_selection_buffer(Text *text)
{
	char *buff = getClipboard(1);
	txt_insert_buf(text, buff);
}

void txt_copy_clipboard(Text *text) {
	char *temp;

	txt_copy_selectbuffer(text);

	if (copybuffer) {
		copybuffer[bufferlength] = '\0';
		temp = winNewLine(copybuffer);
		
		putClipboard(temp, 0);
		MEM_freeN(temp);
		MEM_freeN(copybuffer);
		copybuffer= NULL;
	}
}

void run_python_script(SpaceText *st)
{
	char *py_filename;
	Text *text=st->text;

	if (!BPY_txt_do_python_Text(text)) {
		int lineno = BPY_Err_getLinenumber();
		// jump to error if happened in current text:
		py_filename = (char*) BPY_Err_getFilename();

		/* st->text can become NULL: user called Blender.Load(blendfile)
		 * before the end of the script. */
		if (!st->text) return;

		if (!strcmp(py_filename, st->text->id.name+2)) {
			error_pyscript(  );
			if (lineno >= 0) {
				txt_move_toline(text, lineno-1, 0);
				txt_sel_line(text);
				pop_space_text(st);
			}	
		} else {
			error("Error in other (possibly external) file, "\
				"check console");
		}	
	}
}

static void set_tabs(Text *text)
{
	SpaceText *st = curarea->spacedata.first;
	st->currtab_set = setcurr_tab(text);
}

static void wrap_move_bol(SpaceText *st, short sel) {
	int offl, offc, lin;
	Text *text= st->text;

	lin= txt_get_span(text->lines.first, text->sell);
	wrap_offset(st, text->sell, text->selc, &offl, &offc);

	if (sel) {
		txt_undo_add_toop(text, UNDO_STO, lin, text->selc, lin, -offc);
		text->selc= -offc;
	} else {
		txt_undo_add_toop(text, UNDO_CTO, lin, text->curc, lin, -offc);
		text->curc= -offc;
		txt_pop_sel(text);
	}
}

static void wrap_move_eol(SpaceText *st, short sel) {
	int offl, offc, lin, startl, c;
	Text *text= st->text;

	lin= txt_get_span(text->lines.first, text->sell);
	wrap_offset(st, text->sell, text->selc, &offl, &offc);
	startl= offl;
	c= text->selc;
	while (offl==startl && text->sell->line[c]!='\0') {
		c++;
		wrap_offset(st, text->sell, c, &offl, &offc);
	} if (offl!=startl) c--;

	if (sel) {
		txt_undo_add_toop(text, UNDO_STO, lin, text->selc, lin, c);
		text->selc= c;
	} else {
		txt_undo_add_toop(text, UNDO_CTO, lin, text->curc, lin, c);
		text->curc= c;
		txt_pop_sel(text);
	}
}

static void wrap_move_up(SpaceText *st, short sel) {
	int offl, offl_1, offc, fromline, toline, c, target;
	Text *text= st->text;

	wrap_offset(st, text->sell, 0, &offl_1, &offc);
	wrap_offset(st, text->sell, text->selc, &offl, &offc);
	fromline= toline= txt_get_span(text->lines.first, text->sell);
	target= text->selc + offc;

	if (offl==offl_1) {
		if (!text->sell->prev) {
			txt_move_bol(text, sel);
			return;
		}
		toline--;
		c= text->sell->prev->len; /* End of prev. line */
		wrap_offset(st, text->sell->prev, c, &offl, &offc);
		c= -offc+target;
	} else {
		c= -offc-1; /* End of prev. line */
		wrap_offset(st, text->sell, c, &offl, &offc);
		c= -offc+target;
	}
	if (c<0) c=0;

	if (sel) {
		txt_undo_add_toop(text, UNDO_STO, fromline, text->selc, toline, c);
		if (toline<fromline) text->sell= text->sell->prev;
		if (c>text->sell->len) c= text->sell->len;
		text->selc= c;
	} else {
		txt_undo_add_toop(text, UNDO_CTO, fromline, text->curc, toline, c);
		if (toline<fromline) text->curl= text->curl->prev;
		if (c>text->curl->len) c= text->curl->len;
		text->curc= c;
		txt_pop_sel(text);
	}
}

static void wrap_move_down(SpaceText *st, short sel) {
	int offl, startoff, offc, fromline, toline, c, target;
	Text *text= st->text;

	wrap_offset(st, text->sell, text->selc, &offl, &offc);
	fromline= toline= txt_get_span(text->lines.first, text->sell);
	target= text->selc + offc;
	startoff= offl;
	c= text->selc;
	while (offl==startoff && text->sell->line[c]!='\0') {
		c++;
		wrap_offset(st, text->sell, c, &offl, &offc);
	}

	if (text->sell->line[c]=='\0') {
		if (!text->sell->next) {
			txt_move_eol(text, sel);
			return;
		}
		toline++;
		c= target;
	} else {
		c += target;
		if (c > text->sell->len) c= text->sell->len;
	}
	if (c<0) c=0;

	if (sel) {
		txt_undo_add_toop(text, UNDO_STO, fromline, text->selc, toline, c);
		if (toline>fromline) text->sell= text->sell->next;
		if (c>text->sell->len) c= text->sell->len;
		text->selc= c;
	} else {
		txt_undo_add_toop(text, UNDO_CTO, fromline, text->curc, toline, c);
		if (toline>fromline) text->curl= text->curl->next;
		if (c>text->curl->len) c= text->curl->len;
		text->curc= c;
		txt_pop_sel(text);
	}
}

static void get_suggest_prefix(Text *text, int offset) {
	int i, len;
	char *line, tmp[256];

	if (!text) return;
	if (!texttool_text_is_active(text)) return;

	line= text->curl->line;
	for (i=text->curc-1+offset; i>=0; i--)
		if (!check_identifier(line[i]))
			break;
	i++;
	len= text->curc-i+offset;
	if (len > 255) {
		printf("Suggestion prefix too long\n");
		len = 255;
	}
	strncpy(tmp, line+i, len);
	tmp[len]= '\0';
	texttool_suggest_prefix(tmp);
}

static void confirm_suggestion(Text *text, int skipleft) {
	int i, over=0;
	char *line;
	SuggItem *sel;

	if (!text) return;
	if (!texttool_text_is_active(text)) return;

	sel = texttool_suggest_selected();
	if (!sel) return;

	line= text->curl->line;
	i=text->curc-skipleft-1;
	while (i>=0) {
		if (!check_identifier(line[i]))
			break;
		over++;
		i--;
	}

	for (i=0; i<skipleft; i++)
		txt_move_left(text, 0);
	for (i=0; i<over; i++)
		txt_move_left(text, 1);

	txt_insert_buf(text, sel->name);
	
	for (i=0; i<skipleft; i++)
		txt_move_right(text, 0);

	texttool_text_clear();
}

static short do_texttools(SpaceText *st, char ascii, unsigned short evnt, short val) {
	int draw=0, tools=0, swallow=0, scroll=1;
	if (!texttool_text_is_active(st->text)) return 0;
	if (!st->text || st->text->id.lib) return 0;

	if (st->doplugins && texttool_text_is_active(st->text)) {
		if (texttool_suggest_first()) tools |= TOOL_SUGG_LIST;
		if (texttool_docs_get()) tools |= TOOL_DOCUMENT;
	}

	if (ascii) {
		if (tools & TOOL_SUGG_LIST) {
			if ((ascii != '_' && ascii != '*' && ispunct(ascii)) || check_whitespace(ascii)) {
				confirm_suggestion(st->text, 0);
				if (st->showsyntax) txt_format_line(st, st->text->curl, 1);
			} else if ((st->overwrite && txt_replace_char(st->text, ascii)) || txt_add_char(st->text, ascii)) {
				get_suggest_prefix(st->text, 0);
				pop_suggest_list();
				swallow= 1;
				draw= 1;
			}
		}
		if (tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0, draw= 1;

	} else if (val==1 && evnt) {
		switch (evnt) {
			case LEFTMOUSE:
				if (do_suggest_select(st))
					swallow= 1;
				else {
					if (tools & TOOL_SUGG_LIST) texttool_suggest_clear();
					if (tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				}
				draw= 1;
				break;
			case MIDDLEMOUSE:
				if (do_suggest_select(st)) {
					confirm_suggestion(st->text, 0);
					if (st->showsyntax) txt_format_line(st, st->text->curl, 1);
					swallow= 1;
				} else {
					if (tools & TOOL_SUGG_LIST) texttool_suggest_clear();
					if (tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				}
				draw= 1;
				break;
			case ESCKEY:
				draw= swallow= 1;
				if (tools & TOOL_SUGG_LIST) texttool_suggest_clear();
				else if (tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				else draw= swallow= 0;
				break;
			case RETKEY:
				if (tools & TOOL_SUGG_LIST) {
					confirm_suggestion(st->text, 0);
					if (st->showsyntax) txt_format_line(st, st->text->curl, 1);
					swallow= 1;
					draw= 1;
				}
				if (tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0, draw= 1;
				break;
			case LEFTARROWKEY:
			case BACKSPACEKEY:
				if (tools & TOOL_SUGG_LIST) {
					if (G.qual)
						texttool_suggest_clear();
					else {
						/* Work out which char we are about to delete/pass */
						if (st->text->curl && st->text->curc > 0) {
							char ch= st->text->curl->line[st->text->curc-1];
							if ((ch=='_' || !ispunct(ch)) && !check_whitespace(ch)) {
								get_suggest_prefix(st->text, -1);
								pop_suggest_list();
							}
							else
								texttool_suggest_clear();
						} else
							texttool_suggest_clear();
					}
				}
				if (tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				break;
			case RIGHTARROWKEY:
				if (tools & TOOL_SUGG_LIST) {
					if (G.qual)
						texttool_suggest_clear();
					else {
						/* Work out which char we are about to pass */
						if (st->text->curl && st->text->curc < st->text->curl->len) {
							char ch= st->text->curl->line[st->text->curc+1];
							if ((ch=='_' || !ispunct(ch)) && !check_whitespace(ch)) {
								get_suggest_prefix(st->text, 1);
								pop_suggest_list();
							}
							else
								texttool_suggest_clear();
						} else
							texttool_suggest_clear();
					}
				}
				if (tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				break;
			case PAGEDOWNKEY:
				scroll= SUGG_LIST_SIZE-1;
			case WHEELDOWNMOUSE:
			case DOWNARROWKEY:
				if (tools & TOOL_DOCUMENT) {
					doc_scroll++;
					swallow= 1;
					draw= 1;
					break;
				} else if (tools & TOOL_SUGG_LIST) {
					SuggItem *sel = texttool_suggest_selected();
					if (!sel) {
						texttool_suggest_select(texttool_suggest_first());
					} else while (sel && sel!=texttool_suggest_last() && sel->next && scroll--) {
						texttool_suggest_select(sel->next);
						sel= sel->next;
					}
					pop_suggest_list();
					swallow= 1;
					draw= 1;
					break;
				}
			case PAGEUPKEY:
				scroll= SUGG_LIST_SIZE-1;
			case WHEELUPMOUSE:
			case UPARROWKEY:
				if (tools & TOOL_DOCUMENT) {
					if (doc_scroll>0) doc_scroll--;
					swallow= 1;
					draw= 1;
					break;
				} else if (tools & TOOL_SUGG_LIST) {
					SuggItem *sel = texttool_suggest_selected();
					while (sel && sel!=texttool_suggest_first() && sel->prev && scroll--) {
						texttool_suggest_select(sel->prev);
						sel= sel->prev;
					}
					pop_suggest_list();
					swallow= 1;
					draw= 1;
					break;
				}
			case RIGHTSHIFTKEY:
			case LEFTSHIFTKEY:
				break;
			default:
				if (tools & TOOL_SUGG_LIST) texttool_suggest_clear(), draw= 1;
				if (tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0, draw= 1;
		}
	}

	if (draw) {
		ScrArea *sa;
		
		for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
			SpaceText *st= sa->spacedata.first;
			
			if (st && st->spacetype==SPACE_TEXT) {
				scrarea_queue_redraw(sa);
			}
		}
	}

	return swallow;
}

static short do_markers(SpaceText *st, char ascii, unsigned short evnt, short val) {
	Text *text;
	TextMarker *marker, *mrk, *nxt;
	int c, s, draw=0, swallow=0;

	text= st->text;
	if (!text || text->id.lib || text->curl != text->sell) return 0;

	marker= txt_find_marker(text, text->sell, text->selc, 0, 0);
	if (marker && (marker->start > text->curc || marker->end < text->curc))
		marker= NULL;

	if (!marker) {
		/* Find the next temporary marker */
		if (evnt==TABKEY) {
			int lineno= txt_get_span(text->lines.first, text->curl);
			TextMarker *mrk= text->markers.first;
			while (mrk) {
				if (!marker && (mrk->flags & TMARK_TEMP)) marker= mrk;
				if ((mrk->flags & TMARK_TEMP) && (mrk->lineno > lineno || (mrk->lineno==lineno && mrk->end > text->curc))) {
					marker= mrk;
					break;
				}
				mrk= mrk->next;
			}
			if (marker) {
				txt_move_to(text, marker->lineno, marker->start, 0);
				txt_move_to(text, marker->lineno, marker->end, 1);
				pop_space_text(st);
				evnt= ascii= val= 0;
				draw= 1;
				swallow= 1;
			}
		} else if (evnt==ESCKEY) {
			if (txt_clear_markers(text, 0, TMARK_TEMP)) swallow= 1;
			else if (txt_clear_markers(text, 0, 0)) swallow= 1;
			else return 0;
			evnt= ascii= val= 0;
			draw= 1;
		}
		if (!swallow) return 0;
	}

	if (ascii) {
		if (marker->flags & TMARK_EDITALL) {
			c= text->curc-marker->start;
			s= text->selc-marker->start;
			if (s<0 || s>marker->end-marker->start) return 0;

			mrk= txt_next_marker(text, marker);
			while (mrk) {
				nxt=txt_next_marker(text, mrk); /* mrk may become invalid */
				txt_move_to(text, mrk->lineno, mrk->start+c, 0);
				if (s!=c) txt_move_to(text, mrk->lineno, mrk->start+s, 1);
				if (st->overwrite) {
					if (txt_replace_char(text, ascii))
						if (st->showsyntax) txt_format_line(st, text->curl, 1);
				} else {
					if (txt_add_char(text, ascii)) {
						if (st->showsyntax) txt_format_line(st, text->curl, 1);
					}
				}

				if (mrk==marker || mrk==nxt) break;
				mrk=nxt;
			}
			swallow= 1;
			draw= 1;
		}
	} else if (val) {
		switch(evnt) {
			case BACKSPACEKEY:
				if (marker->flags & TMARK_EDITALL) {
					c= text->curc-marker->start;
					s= text->selc-marker->start;
					if (s<0 || s>marker->end-marker->start) return 0;
					
					mrk= txt_next_marker(text, marker);
					while (mrk) {
						nxt= txt_next_marker(text, mrk); /* mrk may become invalid */
						txt_move_to(text, mrk->lineno, mrk->start+c, 0);
						if (s!=c) txt_move_to(text, mrk->lineno, mrk->start+s, 1);
						txt_backspace_char(text);
						if (st->showsyntax) txt_format_line(st, text->curl, 1);
						if (mrk==marker || mrk==nxt) break;
						mrk= nxt;
					}
					swallow= 1;
					draw= 1;
				}
				break;
			case DELKEY:
				if (marker->flags & TMARK_EDITALL) {
					c= text->curc-marker->start;
					s= text->selc-marker->start;
					if (s<0 || s>marker->end-marker->start) return 0;
					
					mrk= txt_next_marker(text, marker);
					while (mrk) {
						nxt= txt_next_marker(text, mrk); /* mrk may become invalid */
						txt_move_to(text, mrk->lineno, mrk->start+c, 0);
						if (s!=c) txt_move_to(text, mrk->lineno, mrk->start+s, 1);
						txt_delete_char(text);
						if (st->showsyntax) txt_format_line(st, text->curl, 1);
						if (mrk==marker || mrk==nxt) break;
						mrk= nxt;
					}
					swallow= 1;
					draw= 1;
				}
				break;
			case TABKEY:
				if (G.qual & LR_SHIFTKEY) {
					nxt= marker->prev;
					if (!nxt) nxt= text->markers.last;
				} else {
					nxt= marker->next;
					if (!nxt) nxt= text->markers.first;
				}
				if (marker->flags & TMARK_TEMP) {
					if (nxt==marker) nxt= NULL;
					BLI_freelinkN(&text->markers, marker);
				}
				mrk= nxt;
				if (mrk) {
					txt_move_to(text, mrk->lineno, mrk->start, 0);
					txt_move_to(text, mrk->lineno, mrk->end, 1);
					pop_space_text(st);
				}
				swallow= 1;
				draw= 1;
				break;

			/* Events that should clear markers */
			case UKEY: if (!(G.qual & LR_ALTKEY)) break;
			case ZKEY: if (evnt==ZKEY && !(G.qual & LR_CTRLKEY)) break;
			case RETKEY:
			case ESCKEY:
				if (marker->flags & (TMARK_EDITALL | TMARK_TEMP))
					txt_clear_markers(text, marker->group, 0);
				else
					BLI_freelinkN(&text->markers, marker);
				swallow= 1;
				draw= 1;
				break;
			case RIGHTMOUSE: /* Marker context menu? */
			case LEFTMOUSE:
				break;
			case FKEY: /* Allow find */
				if (G.qual & LR_SHIFTKEY) swallow= 1;
				break;

			default:
				if (G.qual!=0 && G.qual!=LR_SHIFTKEY)
					swallow= 1; /* Swallow all other shortcut events */
		}
	}
	
	if (draw) {
		ScrArea *sa;
		
		for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
			SpaceText *st= sa->spacedata.first;
			
			if (st && st->spacetype==SPACE_TEXT) {
				scrarea_queue_redraw(sa);
			}
		}
	}
	return swallow;
}

static short do_modification_check(SpaceText *st) {
	Text *text= st->text;

	if (last_check_time < PIL_check_seconds_timer() - 2.0) {
		switch (txt_file_modified(text)) {
		case 1:
			/* Modified locally and externally, ahhh. Offer more possibilites. */
			if (text->flags & TXT_ISDIRTY) {
				switch (pupmenu("File Modified Outside and Inside Blender %t|Load outside changes (ignore local changes) %x0|Save local changes (ignore outside changes) %x1|Make text internal (separate copy) %x2")) {
				case 0:
					reopen_text(text);
					if (st->showsyntax) txt_format_text(st);
					return 1;
				case 1:
					txt_write_file(text);
					return 1;
				case 2:
					text->flags |= TXT_ISMEM | TXT_ISDIRTY | TXT_ISTMP;
					MEM_freeN(text->name);
					text->name= NULL;
					return 1;
				}
			} else {
				switch (pupmenu("File Modified Outside Blender %t|Reload from disk %x0|Make text internal (separate copy) %x1|Ignore %x2")) {
				case 0:
					reopen_text(text);
					if (st->showsyntax) txt_format_text(st);
					return 1;
				case 1:
					text->flags |= TXT_ISMEM | TXT_ISDIRTY | TXT_ISTMP;
					MEM_freeN(text->name);
					text->name= NULL;
					return 1;
				case 2:
					txt_ignore_modified(text);
					return 1;
				}
			}
			break;
		case 2:
			switch (pupmenu("File Deleted Outside Blender %t|Make text internal %x0|Recreate file %x1")) {
			case 0:
				text->flags |= TXT_ISMEM | TXT_ISDIRTY | TXT_ISTMP;
				MEM_freeN(text->name);
				text->name= NULL;
				return 1;
			case 1:
				txt_write_file(text);
				return 1;
			}
			break;
		default:
			break;
		}
		last_check_time = PIL_check_seconds_timer();
	}
	return 0;
}

void winqreadtextspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	char ascii= evt->ascii;
	SpaceText *st= curarea->spacedata.first;
	Text *text;
	int do_draw=0, p;
	
	if (st==NULL || st->spacetype != SPACE_TEXT) return;
	
	/* smartass code to prevent the CTRL/ALT events below from not working! */
	if(G.qual & (LR_ALTKEY|LR_CTRLKEY))
		if(!ispunct(ascii)) 
			ascii= 0;

	text= st->text;
	
	if (!text) {
		if (event==RIGHTMOUSE) {
			switch (pupmenu("File %t|New %x0|Open... %x1")) {
			case 0:
				st->text= add_empty_text("Text");
				st->top= 0;
			
				allqueue(REDRAWTEXT, 0);
				allqueue(REDRAWHEADERS, 0);
				break;
			case 1:
				activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, add_text_fs);
				break;
			}
		}
		if (val && !ELEM(G.qual, 0, LR_SHIFTKEY)) {
			if (event==FKEY && G.qual == (LR_ALTKEY|LR_SHIFTKEY)) {
				switch (pupmenu("File %t|New %x0|Open... %x1")) {
				case 0:
					st->text= add_empty_text("Text");
					st->top= 0;
				
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);
					break;
				case 1:
					activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, add_text_fs);
					break;
				}
			} 
			else if (event==QKEY) {
				if (G.qual & LR_CTRLKEY) {
					if(okee("Quit Blender")) exit_usiblender();
				}
			}
			else if (event==NKEY) {
				if (G.qual & LR_ALTKEY) {
					st->text= add_empty_text("Text");
					st->top= 0;
				
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);
				}
			}
			else if (event==OKEY) {
				if (G.qual & LR_ALTKEY) {
					activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, add_text_fs);
				}
			}
		}
		return;
	}

	if (val && uiDoBlocks(&curarea->uiblocks, event, 1)!=UI_NOTHING) event= 0;

	if (st->doplugins && do_texttools(st, ascii, event, val)) return;
	if (do_markers(st, ascii, event, val)) return;
	
	if (event==UI_BUT_EVENT) {
		do_find_buttons(val);
		do_draw= 1;
	} else if (event==LEFTMOUSE) {
		if (val) {
			short mval[2];
			char *buffer;
			set_tabs(text);
			getmouseco_areawin(mval);
			
			if (mval[0]>2 && mval[0]<20 && mval[1]>2 && mval[1]<curarea->winy-2) {
				do_textscroll(st, 2);
			} else {
				do_selection(st, G.qual&LR_SHIFTKEY);
				if (txt_has_sel(text)) {
					buffer = txt_sel_to_buf(text);
					putClipboard(buffer, 1);
					MEM_freeN(buffer);
				}
				do_draw= 1;
			}
		}
	} else if (event==MIDDLEMOUSE) {
		if (val) {
			if (U.uiflag & USER_MMB_PASTE) {
				do_selection(st, G.qual&LR_SHIFTKEY);
				get_selection_buffer(text);
				do_draw= 1;
			} else {
				do_textscroll(st, 1);
			}
		}
	} else if (event==RIGHTMOUSE) {
		if (val) {
			if (txt_has_sel(text))
				p= pupmenu("Text %t|Cut%x10|Copy%x11|Paste%x12|New %x0|Open... %x1|Save %x2|Save As...%x3|Execute Script%x4");
			else
				p= pupmenu("Text %t|Paste%x12|New %x0|Open... %x1|Save %x2|Save As...%x3|Execute Script%x4");

			switch(p) {
				case 0:
					st->text= add_empty_text("Text");
					st->top= 0;
					
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);
					break;

				case 1:
					activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, add_text_fs);
					break;
					
				case 3:
					text->flags |= TXT_ISMEM;
					
				case 2:
					txt_write_file(text);
					do_draw= 1;
					break;
				case 4:
					run_python_script(st);
					do_draw= 1;
					break;
				case 10:
					if (text && text->id.lib) {
						error_libdata();
						break;
					}
					txt_copy_clipboard(text);
					txt_cut_sel(text);
					pop_space_text(st);
					do_draw= 1;
					break;
				case 11:
					//txt_copy_sel(text);
					txt_copy_clipboard(text);
					break;
				case 12:
					if (text && text->id.lib) {
						error_libdata();
						break;
					}
					txt_paste_clipboard(text);
					if (st->showsyntax) txt_format_text(st);
					do_draw= 1;
					break;
			}
		}
	} else if (ascii) {
		if (text && text->id.lib) {
			error_libdata();
		} else {
			short mval[2];
			getmouseco_areawin(mval);
			if (st->showlinenrs && mval[0]>2 && mval[0]<60 && mval[1]>2 && mval[1]<curarea->winy-2) {
				if (ascii>='0' && ascii<='9') {
					double time = PIL_check_seconds_timer();
					if (last_jump < time-1) jump_to= 0;
					jump_to *= 10; jump_to += (int)(ascii-'0');
					txt_move_toline(text, jump_to-1, 0);
					last_jump= time;
				}
			} else if ((st->overwrite && txt_replace_char(text, ascii)) || txt_add_char(text, ascii)) {
				if (st->showsyntax) txt_format_line(st, text->curl, 1);
			}
			pop_space_text(st);
			do_draw= 1;
		}
	} else if (val) {
		switch (event) {
		case AKEY:
			if (G.qual & LR_ALTKEY) {
				txt_move_bol(text, G.qual & LR_SHIFTKEY);
				do_draw= 1;
				pop_space_text(st);
			} else if (G.qual & LR_CTRLKEY) {
				txt_sel_all(text);
				do_draw= 1;
			}
			break; /* BREAK A */
		case CKEY:
			if (G.qual & LR_ALTKEY || G.qual & LR_CTRLKEY) {
				if(G.qual & LR_SHIFTKEY)
					txt_copy_clipboard(text);
				else
					txt_copy_clipboard(text);

				do_draw= 1;	
			}
			break; /* BREAK C */
		case DKEY:
			if (text && text->id.lib) {
				error_libdata();
				break;
			}
			if (G.qual == (LR_CTRLKEY|LR_SHIFTKEY)) {
				//uncommenting
				txt_order_cursors(text);
				uncomment(text);
				do_draw = 1;
				if (st->showsyntax) txt_format_text(st);
				break;
			} else if (G.qual == LR_CTRLKEY) {
				txt_delete_char(text);
				if (st->showsyntax) txt_format_line(st, text->curl, 1);
				do_draw= 1;
				pop_space_text(st);
			}
			break; /* BREAK D */
		case EKEY:
			if (G.qual == (LR_ALTKEY|LR_SHIFTKEY)) {
				switch(pupmenu("Edit %t|Cut %x0|Copy %x1|Paste %x2|Print Cut Buffer %x3")) {
				case 0:
					if (text && text->id.lib) {
						error_libdata();
						break;
					}
					txt_copy_clipboard(text); //First copy to clipboard
					txt_cut_sel(text);
					do_draw= 1;
					break;
				case 1:
					txt_copy_clipboard(text);
					//txt_copy_sel(text);
					do_draw= 1;
					break;
				case 2:
					if (text && text->id.lib) {
						error_libdata();
						break;
					}
					//txt_paste(text);
					txt_paste_clipboard(text);
					if (st->showsyntax) txt_format_text(st);
					do_draw= 1;
					break;
				case 3:
					txt_print_cutbuffer();
					break;
				}
			}
			else if (G.qual == LR_CTRLKEY || G.qual == (LR_CTRLKEY|LR_SHIFTKEY)) {
				txt_move_eol(text, G.qual & LR_SHIFTKEY);
				do_draw= 1;
				pop_space_text(st);
			}
			break; /* BREAK E */
		case FKEY:
			if (G.qual == (LR_ALTKEY|LR_SHIFTKEY)) {
				switch(pupmenu("File %t|New %x0|Open... %x1|Save %x2|Save As...%x3")) {
				case 0:
					st->text= add_empty_text("Text");
					st->top= 0;
					
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);
					break;
				case 1:
					activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, add_text_fs);
					break;
				case 3:
					text->flags |= TXT_ISMEM;
				case 2:
					txt_write_file(text);
					do_draw= 1;
					break;
				}
			}
			else if (G.qual & (LR_ALTKEY|LR_CTRLKEY)) {
				find_and_replace(st, 0);
				do_draw= 1;
			}
			break; /* BREAK F */
		case HKEY:
			if (G.qual & (LR_ALTKEY|LR_CTRLKEY)) {
				find_and_replace(st, 1);
				do_draw= 1;
			}
			break; /* BREAK H */
		case JKEY:
			if (G.qual == LR_ALTKEY) {
				do_draw= jumptoline_interactive(st);
			}
			break; /* BREAK J */
		case MKEY:
			if (G.qual == LR_ALTKEY) {
				txt_export_to_object(text);
				do_draw= 1;
			}
			break; /* BREAK M */
		case NKEY:
			if (G.qual == LR_ALTKEY) {
				st->text= add_empty_text("Text");
				st->top= 0;
			
				allqueue(REDRAWTEXT, 0);
				allqueue(REDRAWHEADERS, 0);
			}
			break; /* BREAK N */
		case OKEY:
			if (G.qual == LR_ALTKEY) {
				activate_fileselect(FILE_SPECIAL, "Open Text File", G.sce, add_text_fs);
			}
			break; /* BREAK O */
		case PKEY:
			if (G.qual == LR_ALTKEY) {
				run_python_script(st);
				do_draw= 1;
			}
			break; /* BREAK P */
		case QKEY:
			if(okee("Quit Blender")) exit_usiblender();
			break; /* BREAK Q */
		case RKEY:
			if (G.qual == LR_ALTKEY) {
			    if (text->compiled) BPY_free_compiled_text(text);
			        text->compiled = NULL;
				if (okee("Reopen text")) {
					if (!reopen_text(text))
						error("Could not reopen file");
					if (st->showsyntax) txt_format_text(st);
				}
				do_draw= 1;	
			}
			break; /* BREAK R */
		case SKEY:
			if (G.qual == (LR_ALTKEY|LR_SHIFTKEY)) {
				p= pupmenu("Select %t|"
							"Select All %x0|"
							"Select Line %x1|"
							"Jump to Line %x3");
				switch(p) {
				case 0:
					txt_sel_all(text);
					do_draw= 1;
					break;
					
				case 1:
					txt_sel_line(text);
					do_draw= 1;
					break;
										
				case 3:
					do_draw= jumptoline_interactive(st);
					break;
				}
			}
			else if (G.qual & LR_ALTKEY) {
				/* Event treatment CANNOT enter this if
				if (G.qual & LR_SHIFTKEY) 
					if (text) text->flags |= TXT_ISMEM;
				*/
				txt_write_file(text);
				do_draw= 1;
			}
			break; /* BREAK S */
		case UKEY:
			//txt_print_undo(text); //debug buffer in console
			if (G.qual == (LR_ALTKEY|LR_SHIFTKEY)) {
				txt_do_redo(text);
				pop_space_text(st);
				do_draw= 1;
			}
			if (G.qual == LR_ALTKEY) {
				txt_do_undo(text);
				if (st->showsyntax) txt_format_text(st);
				pop_space_text(st);
				do_draw= 1;
			}
			break; /* BREAK U */
		case VKEY:
			if (G.qual == (LR_ALTKEY| LR_SHIFTKEY)) {
				switch(pupmenu("View %t|Top of File %x0|Bottom of File %x1|Page Up %x2|Page Down %x3")) {
				case 0:
					txt_move_bof(text, 0);
					do_draw= 1;
					pop_space_text(st);
					break;
				case 1:
					txt_move_eof(text, 0);
					do_draw= 1;
					pop_space_text(st);
					break;
				case 2:
					screen_skip(st, -st->viewlines);
					do_draw= 1;
					break;
				case 3:
					screen_skip(st, st->viewlines);
					do_draw= 1;
					break;
				}
			}
			/* Support for both Alt-V and Ctrl-V for Paste, for backward compatibility reasons */
			else if (G.qual & LR_ALTKEY || G.qual & LR_CTRLKEY) {
				if (text && text->id.lib) {
					error_libdata();
					break;
				}
				/* Throwing in the Shift modifier Paste from the OS clipboard */
				if (G.qual & LR_SHIFTKEY)
					txt_paste_clipboard(text);
				else
					txt_paste_clipboard(text);
				if (st->showsyntax) txt_format_text(st);
				do_draw= 1;	
				pop_space_text(st);
			}
			break; /* BREAK V */
		case XKEY:
			if (G.qual == LR_ALTKEY || G.qual == LR_CTRLKEY) {
				if (text && text->id.lib) {
					error_libdata();
					break;
				}
				txt_cut_sel(text);
				if (st->showsyntax) txt_format_text(st);
				do_draw= 1;	
				pop_space_text(st);
			}
			break;
		case ZKEY:
			if (G.qual & (LR_ALTKEY|LR_CTRLKEY|LR_COMMANDKEY)) {
				if (G.qual & LR_SHIFTKEY) {
					txt_do_redo(text);
				} else {
					txt_do_undo(text);
				}
				if (st->showsyntax) txt_format_text(st);
				pop_space_text(st);
				do_draw= 1;
			}
			break;
		case TABKEY:
			if (text && text->id.lib) {
				error_libdata();
				break;
			} else {
				if (txt_has_sel(text)) {
					if (G.qual & LR_SHIFTKEY) {
						txt_order_cursors(text);
						unindent(text);
						if (st->showsyntax) txt_format_text(st);
					} else {
						txt_order_cursors(text);
						indent(text);
						if (st->showsyntax) txt_format_text(st);
					}
				} else {
					txt_add_char(text, '\t');
					if (st->showsyntax) txt_format_line(st, text->curl, 1);
				}
			}
			pop_space_text(st);
			do_draw= 1;
			st->currtab_set = setcurr_tab(text);
			break;
		case RETKEY:
			if (text && text->id.lib) {
				error_libdata();
				break;
			}
			//double check tabs before splitting the line
			st->currtab_set = setcurr_tab(text);
			txt_split_curline(text);
			{
				int a = 0;
				if (a < st->currtab_set)
				{
					while ( a < st->currtab_set) {
						txt_add_char(text, '\t');
						a++;
					}
				}
			}
			if (st->showsyntax) {
				if (text->curl->prev) txt_format_line(st, text->curl->prev, 0);
				txt_format_line(st, text->curl, 1);
			}
			do_draw= 1;
			pop_space_text(st);
			break;
		case BACKSPACEKEY:
			if (text && text->id.lib) {
				error_libdata();
				break;
			}
			if (G.qual & (LR_ALTKEY | LR_CTRLKEY)) {
				txt_backspace_word(text);
			} else {
				txt_backspace_char(text);
			}
			set_tabs(text);
			if (st->showsyntax) txt_format_line(st, text->curl, 1);
			do_draw= 1;
			pop_space_text(st);
			break;
		case DELKEY:
			if (text && text->id.lib) {
				error_libdata();
				break;
			}
			if (G.qual & (LR_ALTKEY | LR_CTRLKEY)) {
				txt_delete_word(text);
			} else {
				txt_delete_char(text);
			}
			if (st->showsyntax) txt_format_line(st, text->curl, 1);
			do_draw= 1;
			pop_space_text(st);
			st->currtab_set = setcurr_tab(text);
			break;
		case INSERTKEY:
			st->overwrite= !st->overwrite;
			do_draw= 1;
			break;
		case LEFTARROWKEY:
			if (G.qual & LR_COMMANDKEY)
				txt_move_bol(text, G.qual & LR_SHIFTKEY);
			else if (G.qual & LR_ALTKEY)
				txt_jump_left(text, G.qual & LR_SHIFTKEY);
			else
				txt_move_left(text, G.qual & LR_SHIFTKEY);
			set_tabs(text);
			do_draw= 1;
			pop_space_text(st);
			break;
		case RIGHTARROWKEY:
			if (G.qual & LR_COMMANDKEY)
				txt_move_eol(text, G.qual & LR_SHIFTKEY);
			else if (G.qual & LR_ALTKEY)
				txt_jump_right(text, G.qual & LR_SHIFTKEY);
			else
				txt_move_right(text, G.qual & LR_SHIFTKEY);
			set_tabs(text);
			do_draw= 1;
			pop_space_text(st);
			break;
		case UPARROWKEY:
			if (st->wordwrap) wrap_move_up(st, G.qual & LR_SHIFTKEY);
			else txt_move_up(text, G.qual & LR_SHIFTKEY);
			set_tabs(text);
			do_draw= 1;
			pop_space_text(st);
			break;
		case DOWNARROWKEY:
			if (st->wordwrap) wrap_move_down(st, G.qual & LR_SHIFTKEY);
			else txt_move_down(text, G.qual & LR_SHIFTKEY);
			set_tabs(text);
			do_draw= 1;
			pop_space_text(st);
			break;
		case PAGEDOWNKEY:
			cursor_skip(st, st->viewlines, G.qual & LR_SHIFTKEY);
			pop_space_text(st);
			do_draw= 1;
			break;
		case PAGEUPKEY:
			cursor_skip(st, -st->viewlines, G.qual & LR_SHIFTKEY);
			pop_space_text(st);
			do_draw= 1;
			break;
		case HOMEKEY:
			if (st->wordwrap) wrap_move_bol(st, G.qual & LR_SHIFTKEY);
			else txt_move_bol(text, G.qual & LR_SHIFTKEY);
			do_draw= 1;
			pop_space_text(st);
			break;
		case ENDKEY:
			if (st->wordwrap) wrap_move_eol(st, G.qual & LR_SHIFTKEY);
			else txt_move_eol(text, G.qual & LR_SHIFTKEY);
			do_draw= 1;
			pop_space_text(st);
			break;
		case WHEELUPMOUSE:
			screen_skip(st, -U.wheellinescroll);
			do_draw= 1;
			break;
		case WHEELDOWNMOUSE:
			screen_skip(st, U.wheellinescroll);
			do_draw= 1;
			break;
		}
	}

	/* Run text plugin scripts if enabled */
	if (st->doplugins && event && val) {
		if (BPY_menu_do_shortcut(PYMENU_TEXTPLUGIN, event, G.qual)) {
			do_draw= 1;
		}
	}

	if (do_modification_check(st)) do_draw= 1;

	if (do_draw) {
		ScrArea *sa;
		
		for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
			SpaceText *st= sa->spacedata.first;
			
			if (st && st->spacetype==SPACE_TEXT) {
				scrarea_queue_redraw(sa);
			}
		}
	}
}

void draw_brackets(SpaceText *st)
{
	char ch;
	int b, c, startc, endc, find, stack;
	int viewc, viewl, offl, offc, x, y;
	TextLine *startl, *endl, *linep;
	Text *text = st->text;

	if (!text || !text->curl) return;

	startl= text->curl;
	startc= text->curc;
	b= check_bracket(startl->line[startc]);
	if (b==0 && startc>0) b = check_bracket(startl->line[--startc]);
	if (b==0) return;
	
	linep= startl;
	c= startc;
	endl= NULL;
	endc= -1;
	find= -b;
	stack= 0;

	/* Opening bracket, search forward for close */
	if (b>0) {
		c++;
		while (linep) {
			while (c<linep->len) {
				b= check_bracket(linep->line[c]);
				if (b==find) {
					if (stack==0) {
						endl= linep;
						endc= c;
						break;
					}
					stack--;
				} else if (b==-find) {
					stack++;
				}
				c++;
			}
			if (endl) break;
			linep= linep->next;
			c= 0;
		}
	}
	/* Closing bracket, search backward for open */
	else {
		c--;
		while (linep) {
			while (c>=0) {
				b= check_bracket(linep->line[c]);
				if (b==find) {
					if (stack==0) {
						endl= linep;
						endc= c;
						break;
					}
					stack--;
				} else if (b==-find) {
					stack++;
				}
				c--;
			}
			if (endl) break;
			linep= linep->prev;
			if (linep) c= linep->len-1;
		}
	}

	if (!endl || endc==-1) return;

	BIF_ThemeColor(TH_HILITE);	
	x= st->showlinenrs ? TXT_OFFSET + TEXTXLOC : TXT_OFFSET;
	y= curarea->winy - st->lheight;

	ch= startl->line[startc];
	wrap_offset(st, startl, startc, &offl, &offc);
	viewc= get_char_pos(st, startl->line, startc) - st->left + offc;
	if (viewc >= 0){
		viewl= txt_get_span(text->lines.first, startl) - st->top + offl;
		glRasterPos2i(x+viewc*spacetext_get_fontwidth(st), y-viewl*st->lheight);
		BMF_DrawCharacter(spacetext_get_font(st), ch);
		glRasterPos2i(x+viewc*spacetext_get_fontwidth(st)+1, y-viewl*st->lheight);
		BMF_DrawCharacter(spacetext_get_font(st), ch);
	}
	ch= endl->line[endc];
	wrap_offset(st, endl, endc, &offl, &offc);
	viewc= get_char_pos(st, endl->line, endc) - st->left + offc;
	if (viewc >= 0) {
		viewl= txt_get_span(text->lines.first, endl) - st->top + offl;
		glRasterPos2i(x+viewc*spacetext_get_fontwidth(st), y-viewl*st->lheight);
		BMF_DrawCharacter(spacetext_get_font(st), ch);
		glRasterPos2i(x+viewc*spacetext_get_fontwidth(st)+1, y-viewl*st->lheight);
		BMF_DrawCharacter(spacetext_get_font(st), ch);
	}
}

static int check_bracket(char ch)
{
	int a;
	char opens[] = "([{";
	char close[] = ")]}";
	
	for (a=0; a<3; a++) {
		if(ch==opens[a])
			return a+1;
		else if (ch==close[a])
			return -(a+1);
	}
	return 0;
}

static int check_delim(char ch) 
{
	int a;
	char delims[] = "():\"\' ~!%^&*-+=[]{};/<>|.#\t,";
	
	for (a=0; a<28; a++) {
		if (ch==delims[a])
			return 1;
	}
	return 0;
}

static int check_digit(char ch) {
	if (ch < '0') return 0;
	if (ch <= '9') return 1;
	return 0;
}

static int check_identifier(char ch) {
	if (ch < '0') return 0;
	if (ch <= '9') return 1;
	if (ch < 'A') return 0;
	if (ch <= 'Z' || ch == '_') return 1;
	if (ch < 'a') return 0;
	if (ch <= 'z') return 1;
	return 0;
}

static int check_whitespace(char ch) {
	if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
		return 1;
	return 0;
}

void convert_tabs (struct SpaceText *st, int tab)
{
	Text *text = st->text;
	TextLine *tmp;
	char *check_line, *new_line;
	int extra, number; //unknown for now
	size_t a, j;
	
	if (!text) return;
	
	tmp = text->lines.first;
	
	//first convert to all space, this make it alot easier to convert to tabs because there is no mixtures of ' ' && '\t'
	while(tmp) {
		check_line = tmp->line;
		new_line = MEM_mallocN(render_string(st, check_line)+1, "Converted_Line");
		j = 0;
		for (a=0; a < strlen(check_line); a++) { //foreach char in line
			if(check_line[a] == '\t') { //checking for tabs
				//get the number of spaces this tabs is showing
				//i dont like doing it this way but will look into it later
				new_line[j] = '\0';
				number = render_string(st, new_line);
				new_line[j] = '\t';
				new_line[j+1] = '\0';
				number = render_string(st, new_line)-number;
				for(extra = 0; extra < number; extra++) {
					new_line[j] = ' ';
					j++;
				}
			} else {
				new_line[j] = check_line[a];
				++j;
			}
		}
		new_line[j] = '\0';
		// put new_line in the tmp->line spot still need to try and set the curc correctly
		if (tmp->line) MEM_freeN(tmp->line);
		if(tmp->format) MEM_freeN(tmp->format);
		
		tmp->line = new_line;
		tmp->len = strlen(new_line);
		tmp->format = NULL;
		tmp = tmp->next;
	}
	
	if (tab) // Converting to tabs
	{	//start over from the begining
		tmp = text->lines.first;
		
		while(tmp) {
			check_line = tmp->line;
			extra = 0;
			for (a = 0; a < strlen(check_line); a++) {
				number = 0;
				for (j = 0; j < (size_t)st->tabnumber; j++) {
					if ((a+j) <= strlen(check_line)) { //check to make sure we are not pass the end of the line
						if(check_line[a+j] != ' ') {
							number = 1;
						}
					}
				}
				if (!number) { //found all number of space to equal a tab
					a = a+(st->tabnumber-1);
					extra = extra+1;
				}
			}
			
			if ( extra > 0 ) { //got tabs make malloc and do what you have to do
				new_line = MEM_mallocN(strlen(check_line)-(((st->tabnumber*extra)-extra)-1), "Converted_Line");
				extra = 0; //reuse vars
				for (a = 0; a < strlen(check_line); a++) {
					number = 0;
					for (j = 0; j < (size_t)st->tabnumber; j++) {
						if ((a+j) <= strlen(check_line)) { //check to make sure we are not pass the end of the line
							if(check_line[a+j] != ' ') {
								number = 1;
							}
						}
					}
					if (!number) { //found all number of space to equal a tab
						new_line[extra] = '\t';
						a = a+(st->tabnumber-1);
						++extra;
						
					} else { //not adding a tab
						new_line[extra] = check_line[a];
						++extra;
					}
				}
				new_line[extra] = '\0';
				// put new_line in the tmp->line spot still need to try and set the curc correctly
				if (tmp->line) MEM_freeN(tmp->line);
				if(tmp->format) MEM_freeN(tmp->format);
				
				tmp->line = new_line;
				tmp->len = strlen(new_line);
				tmp->format = NULL;
			}
			tmp = tmp->next;
		}
	}

	if (st->showsyntax) txt_format_text(st);
}
