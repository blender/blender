/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

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

#include "BPY_extern.h"

#include "BIF_gl.h"
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

#include "BSE_filesel.h"

#include "mydevice.h"
#include "blendef.h" 

#define TEXTXLOC	38

/* locals */
void drawtextspace(ScrArea *sa, void *spacedata);
void winqreadtextspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

static void *last_txt_find_string= NULL;

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
	
	if (last_txt_find_string) MEM_freeN(last_txt_find_string);
	if (temp_char_buf) MEM_freeN(temp_char_buf);
	if (temp_char_accum) MEM_freeN(temp_char_accum);	
}

static int render_string (char *in) {
	int r= 0, i;
	
	while(*in) {
		if (*in=='\t') {
			if (temp_char_pos && *(in-1)=='\t') i= TXT_TABSIZE;
			else i= TXT_TABSIZE - (temp_char_pos%TXT_TABSIZE);

			while(i--) temp_char_write(' ', r);
		} else temp_char_write(*in, r);

		r++;
		in++;
	}
	r= temp_char_pos;
	temp_char_write(0, 0);
		
	return r;
}

static int text_draw(SpaceText *st, char *str, int cshift, int maxwidth, int draw, int x, int y) {
	int r=0, w= 0;
	char *in;
	int *acc;

	w= render_string(str);
	if(w<cshift ) return 0; /* String is shorter than shift */
	
	in= temp_char_buf+cshift;
	acc= temp_char_accum+cshift;
	w= w-cshift;
	
	if (draw) {
		glRasterPos2i(x, y);
		BMF_DrawString(spacetext_get_font(st), in);
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
	
	y-= txt_get_span(text->lines.first, *linep) - st->top;
	
	if (y>0) {
		while (y-- != 0) if((*linep)->next) *linep= (*linep)->next;
	} else if (y<0) {
		while (y++ != 0) if((*linep)->prev) *linep= (*linep)->prev;
	}

	if(st->showlinenrs)
		x-= TXT_OFFSET+TEXTXLOC;
	else
		x-= TXT_OFFSET;

	if (x<0) x= 0;
	x = (x/spacetext_get_fontwidth(st)) + st->left;
	
	w= render_string((*linep)->line);
	if(x<w) *charp= temp_char_accum[x];
	else *charp= (*linep)->len;
	
	if(!sel) txt_pop_sel(text);
}

static void draw_cursor(SpaceText *st) {
	int h, x, i;
	Text *text= st->text;
	TextLine *linef, *linel;
	int charf, charl;
	
	if (text->curl==text->sell && text->curc==text->selc) {
		x= text_draw(st, text->curl->line, st->left, text->curc, 0, 0, 0);

		if (x) {
			h= txt_get_span(text->lines.first, text->curl) - st->top;

			BIF_ThemeColor(TH_HILITE);
			
			glRecti(x-1, curarea->winy-st->lheight*(h)-2, x+1, curarea->winy-st->lheight*(h+1)-2);
		}
	} else {
		int span= txt_get_span(text->curl, text->sell);
		
		if (span<0) {
			linef= text->sell;
			charf= text->selc;
			
			linel= text->curl;
			charl= text->curc;
		} else if (span>0) {
			linef= text->curl;
			charf= text->curc;
	
			linel= text->sell;		
			charl= text->selc;
		} else {
			linef= linel= text->curl;
			
			if (text->curc<text->selc) {
				charf= text->curc;
				charl= text->selc;
			} else {
				charf= text->selc;
				charl= text->curc;
			}
		}
	
			/* Walk to the beginning of visible text */
		h= txt_get_span(text->lines.first, linef) - st->top;
		while (h++<-1 && linef!=linel) linef= linef->next;
	
		x= text_draw(st, linef->line, st->left, charf, 0, 0, 0);

		BIF_ThemeColor(TH_SHADE2);

		if(st->showlinenrs) {
			if (!x) x= TXT_OFFSET + TEXTXLOC -4;
		}
		else {
			if (!x) x= TXT_OFFSET - 4;
		}
		
		if (!x) x= TXT_OFFSET-10;
		while (linef && linef != linel) {
			h= txt_get_span(text->lines.first, linef) - st->top;
			if (h>st->viewlines) break;
			
			glRecti(x, curarea->winy-st->lheight*(h)-2, curarea->winx, curarea->winy-st->lheight*(h+1)-2);
			if(st->showlinenrs)
				glRecti(TXT_OFFSET+TEXTXLOC-4, curarea->winy-st->lheight*(h+1)-2, TXT_OFFSET+TEXTXLOC, curarea->winy-st->lheight*(h+2)-2);
			else
				glRecti(TXT_OFFSET-4, curarea->winy-st->lheight*(h+1)-2, TXT_OFFSET, curarea->winy-st->lheight*(h+2)-2);

			if(st->showlinenrs)
				x= TXT_OFFSET + TEXTXLOC;
			else
				x= TXT_OFFSET;
			
			linef= linef->next;
		}
		
		h= txt_get_span(text->lines.first, linef) - st->top;

		i= text_draw(st, linel->line, st->left, charl, 0, 0, 0);
		if(i) glRecti(x, curarea->winy-st->lheight*(h)-2, i, curarea->winy-st->lheight*(h+1)-2);

	}
	
	BIF_ThemeColor(TH_TEXT);
}

static void calc_text_rcts(SpaceText *st)
{
	short barheight, barstart;
	int lbarstart, lbarh, ltexth;

	lbarstart= st->top;
	lbarh= 	st->viewlines;
	ltexth= txt_get_span(st->text->lines.first, st->text->lines.last)+1;

	barheight= (lbarh*(curarea->winy-4))/ltexth;
	if (barheight<20) barheight=20;
	
	barstart= (lbarstart*(curarea->winy-4))/ltexth + 8;

	st->txtbar.xmin= 5;
	st->txtbar.xmax= 17;
	st->txtbar.ymax= curarea->winy - barstart;
	st->txtbar.ymin= st->txtbar.ymax - barheight;

	CLAMP(st->txtbar.ymin, 2, curarea->winy-2);
	CLAMP(st->txtbar.ymax, 2, curarea->winy-2);

	st->pix_per_line= (float) ltexth/curarea->winy;
	if (st->pix_per_line<.1) st->pix_per_line=.1;

	lbarstart= MIN2(txt_get_span(st->text->lines.first, st->text->curl), 
				txt_get_span(st->text->lines.first, st->text->sell));
	lbarh= abs(txt_get_span(st->text->lines.first, st->text->curl)-txt_get_span(st->text->lines.first, st->text->sell));
	
	barheight= (lbarh*(curarea->winy-4))/ltexth;
	if (barheight<2) barheight=2; 
	
	barstart= (lbarstart*(curarea->winy-4))/ltexth + 8;
	
	st->txtscroll.xmin= 5;
	st->txtscroll.xmax= 17;
	st->txtscroll.ymax= curarea->winy-barstart;
	st->txtscroll.ymin= st->txtscroll.ymax - barheight;

	CLAMP(st->txtscroll.ymin, 2, curarea->winy-2);
	CLAMP(st->txtscroll.ymax, 2, curarea->winy-2);
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

	glDrawBuffer(GL_FRONT);
	uiEmboss(st->txtbar.xmin, st->txtbar.ymin, st->txtbar.xmax, st->txtbar.ymax, st->flags & ST_SCROLL_SELECT);
	glDrawBuffer(GL_BACK);

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
				st->left+= delta[0];
				if (st->left<0) st->left= 0;
				
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

	glDrawBuffer(GL_FRONT);
	uiEmboss(st->txtbar.xmin, st->txtbar.ymin, st->txtbar.xmax, st->txtbar.ymax, st->flags & ST_SCROLL_SELECT);
	glDrawBuffer(GL_BACK);
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
		} else if (mval[0]<0 || mval[0]>curarea->winx) {
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
}

void drawtextspace(ScrArea *sa, void *spacedata)
{
	SpaceText *st= curarea->spacedata.first;
	Text *text;
	int i;
	TextLine *tmp;
	char linenr[12];
	float col[3];
	int linecount = 0;

	if (BPY_spacetext_is_pywin(st)) {
		BPY_spacetext_do_pywin_draw(st);
		return;
	}
	
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

	BIF_ThemeColor(TH_TEXT);

	draw_cursor(st);

	tmp= text->lines.first;
	for (i= 0; i<st->top && tmp; i++) {
		tmp= tmp->next;
		linecount++;
	}
	for (i=0; i<st->viewlines && tmp; i++, tmp= tmp->next) {
		if(st->showlinenrs) {
			if(((float)(i + linecount + 1)/10000.0) < 1.0) {
				sprintf(linenr, "%4d", i + linecount + 1);
				glRasterPos2i(TXT_OFFSET - 7, curarea->winy-st->lheight*(i+1));
			} else {
				sprintf(linenr, "%5d", i + linecount + 1);
				glRasterPos2i(TXT_OFFSET - 11, curarea->winy-st->lheight*(i+1));
			}
			BMF_DrawString(spacetext_get_font(st), linenr);
			text_draw(st, tmp->line, st->left, 0, 1, TXT_OFFSET + TEXTXLOC, curarea->winy-st->lheight*(i+1));
		} else
			text_draw(st, tmp->line, st->left, 0, 1, TXT_OFFSET, curarea->winy-st->lheight*(i+1));
	}

	draw_textscroll(st);

	curarea->win_swap= WIN_BACK_OK;
}

void pop_space_text (SpaceText *st)
{
	int i, x;

	if(!st) return;
	if(!st->text) return;
	if(!st->text->curl) return;
		
	i= txt_get_span(st->text->lines.first, st->text->curl);
	if (st->top+st->viewlines <= i || st->top > i) {
		st->top= i - st->viewlines/2;
	}
	
	x= text_draw(st, st->text->curl->line, st->left, st->text->curc, 0, 0, 0);

	if (x==0 || x>curarea->winx) {
		st->left= st->text->curc-0.5*(curarea->winx)/spacetext_get_fontwidth(st);
	}

	if (st->top < 0) st->top= 0;
	if (st->left <0) st->left= 0;
}

void add_text_fs(char *file) 
{
	SpaceText *st= curarea->spacedata.first;
	Text *text;

	if (!st) return;
	if (st->spacetype != SPACE_TEXT) return;

	text= add_text(file);

	st->text= text;

	st->top= 0;
			
	allqueue(REDRAWTEXT, 0);
	allqueue(REDRAWHEADERS, 0);	
}

void free_textspace(SpaceText *st)
{
	if (!st) return;

	st->text= NULL;
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
	
	/* Do we need to get a filename? */
	if (text->flags & TXT_ISMEM) {
		activate_fileselect(FILE_SPECIAL, "SAVE TEXT FILE", G.sce, save_mem_text);
		return;	
	}
	
	/* Should we ask to save over? */
	if (text->flags & TXT_ISTMP) {
		if (BLI_exists(text->name)) {
			if (!okee("Save over?")) return;
		} else if (!okee("Create new file?")) return;

		text->flags ^= TXT_ISTMP;
	}
		
	fp= fopen(text->name, "w");
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
	
	if (text->flags & TXT_ISDIRTY) text->flags ^= TXT_ISDIRTY;
}

void unlink_text(Text *text)
{
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;
	
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


#ifdef _WIN32
char *unixNewLine(char *buffer)
{
	char *p, *p2, *output;
	
	/* we can afford the few extra bytes */
	output= MEM_callocN(strlen(buffer)+1, "unixnewline");
	for (p= buffer, p2= output; *p; p++)
		if (*p != '\r') *(p2++)= *p;
	
	*p2= 0;
	return(output);
}

char *winNewLine(char *buffer)
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
#endif


void txt_paste_clipboard(Text *text) {
#ifdef _WIN32
	char * buffer = NULL;

	if ( OpenClipboard(NULL) ) {
		HANDLE hData = GetClipboardData( CF_TEXT );
		buffer = (char*)GlobalLock( hData );
		buffer = unixNewLine(buffer);
		txt_insert_buf(text, buffer);
		GlobalUnlock( hData );
		CloseClipboard();
	}
#endif
}

void txt_copy_clipboard(Text *text) {
#ifdef _WIN32
	txt_copy_selectbuffer(text);

	if (OpenClipboard(NULL)) {
		HLOCAL clipbuffer;
		char* buffer;

		copybuffer = winNewLine(copybuffer);

		EmptyClipboard();
		clipbuffer = LocalAlloc(LMEM_FIXED,((bufferlength+1)));
		buffer = (char *) LocalLock(clipbuffer);

		strncpy(buffer, copybuffer, bufferlength);
		buffer[bufferlength] =  '\0';
		LocalUnlock(clipbuffer);
		SetClipboardData(CF_TEXT,clipbuffer);
		CloseClipboard();
	}

	if (copybuffer) {
		MEM_freeN(copybuffer);
		copybuffer= NULL;
	}
#endif
}

/*
 * again==0 show find panel or find
 * again==1 find text again */
void txt_find_panel(SpaceText *st, int again)
{
	Text *text=st->text;
	char *findstr= last_txt_find_string;
			
	if (again==0) {
		findstr= txt_sel_to_buf(text);
	} else if (again==1) {
		char buf[256];

	if (findstr && strlen(findstr)<(sizeof(buf)-1))
		strcpy(buf, findstr);
	else
		buf[0]= 0;
		
	if (sbutton(buf, 0, sizeof(buf)-1, "Find: ") && buf[0])
		findstr= BLI_strdup(buf);
	else
		findstr= NULL;
	}

	if (findstr!=last_txt_find_string) {
		if (last_txt_find_string)
			MEM_freeN(last_txt_find_string);
		last_txt_find_string= findstr;
	}
				
	if (findstr) {
		if (txt_find_string(text, findstr))
			pop_space_text(st);
		else
			error("Not found: %s", findstr);
	}
}

void run_python_script(SpaceText *st)
{
	char *py_filename;
	Text *text=st->text;
	if (!BPY_txt_do_python(st)) {
		int lineno = BPY_Err_getLinenumber();
		// jump to error if happened in current text:
		py_filename = (char*) BPY_Err_getFilename();
		if (!strcmp(py_filename, st->text->id.name+2)) {
			error("Python script error, check console");
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


void winqreadtextspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	char ascii= evt->ascii;
	SpaceText *st= curarea->spacedata.first;
	Text *text= st->text;
	int do_draw=0, p;

	if (BPY_spacetext_is_pywin(st)) {
		BPY_spacetext_do_pywin_event(st, event, val);
		return;
	}

	/* smartass code to prevent the events below from not working! */
	if (!isprint(ascii) || (G.qual & ~LR_SHIFTKEY)) ascii= 0;

	text= st->text;
	
	if (!text) {
			if (event==RIGHTMOUSE) {
				switch (pupmenu("File %t|New %x0|Open... %x1")) {
				case 0:
					st->text= add_empty_text();
					st->top= 0;
				
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);
					break;
				case 1:
					activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs);
					break;
				}
			}
			if (val && !ELEM(G.qual, 0, LR_SHIFTKEY)) {
			if (event==FKEY && (G.qual & LR_ALTKEY) && (G.qual & LR_SHIFTKEY)) {
				switch (pupmenu("File %t|New %x0|Open... %x1")) {
				case 0:
					st->text= add_empty_text();
					st->top= 0;
				
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);
					break;
				case 1:
					activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs);
					break;
			}
		} else if (event==QKEY) {
				if(okee("QUIT BLENDER")) exit_usiblender();
			}
		}
		
		return;
	}
	
	if (event==LEFTMOUSE) {
		if (val) {
			short mval[2];

			getmouseco_areawin(mval);

			if (mval[0]>2 && mval[0]<20 && mval[1]>2 && mval[1]<curarea->winy-2) {
				do_textscroll(st, 2);
			} else {			
				do_selection(st, G.qual&LR_SHIFTKEY);
				do_draw= 1;
			}
		}
	} else if (event==MIDDLEMOUSE) {
		if (val) {
			do_textscroll(st, 1);
		}
	} else if (event==RIGHTMOUSE) {
		if (val) {
			p= pupmenu("File %t|New %x0|Open... %x1|Save %x2|Save As...%x3");

			switch(p) {
				case 0:
					st->text= add_empty_text();
					st->top= 0;
					
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);
					break;

				case 1:
					activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs);
					break;
					
				case 3:
					text->flags |= TXT_ISMEM;
					
				case 2:
					txt_write_file(text);
					do_draw= 1;
					break;

				default:
					break;
			}
		}
	} else if (ascii) {
		if (txt_add_char(text, ascii)) {
			pop_space_text(st);
			do_draw= 1;
		}
	} else if (val) {
		switch (event) {
		case FKEY:
			if ((G.qual & LR_ALTKEY) && (G.qual & LR_SHIFTKEY)) {
				p= pupmenu("File %t|New %x0|Open... %x1|Save %x2|Save As...%x3");

				switch(p) {
				case 0:
					st->text= add_empty_text();
					st->top= 0;
					
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);
					break;

				case 1:
					activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs);
					break;

				case 3:
					text->flags |= TXT_ISMEM;
					
				case 2:
					txt_write_file(text);
					do_draw= 1;
					break;

				default:
					break;
				}
			} else if (G.qual & LR_ALTKEY) {
					if (txt_has_sel(text) && !(G.qual & LR_CTRLKEY)) {
					txt_find_panel(st,0);
				} else if (!last_txt_find_string || (G.qual & LR_CTRLKEY)) {
					txt_find_panel(st,1);
				}
	
				do_draw= 1;
			}
			
			break;

		case EKEY:
			if (G.qual & LR_ALTKEY && G.qual & LR_SHIFTKEY) {
				p= pupmenu("Edit %t|"
							"Cut %x0|"
							"Copy %x1|"
							"Paste %x2|"
							"Print Cut Buffer %x3");
				switch(p) {
				case 0:
					txt_cut_sel(text);
					do_draw= 1;
					break;
				case 1:
					txt_copy_sel(text);
					do_draw= 1;
					break;
				case 2:
					txt_paste(text);
					do_draw= 1;
					break;
				case 3:
					txt_print_cutbuffer();
					break;
				}
			}
			break;

		case VKEY:
			if (G.qual & LR_ALTKEY && G.qual & LR_SHIFTKEY) {
				p= pupmenu("View %t|"
							"Top of File %x0|"
							"Bottom of File %x1|"
							"Page Up %x2|"
							"Page Down %x3");
				switch(p) {
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
			break;

		case SKEY:
			if (G.qual & LR_ALTKEY && G.qual & LR_SHIFTKEY) {
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
			break;
			
		case QKEY:
			if(okee("QUIT BLENDER")) exit_usiblender();
			break;
		}

		switch(event) {
		case AKEY:
			if (G.qual & LR_ALTKEY) {
				txt_move_bol(text, G.qual & LR_SHIFTKEY);
				do_draw= 1;
				pop_space_text(st);
			} else if (G.qual & LR_CTRLKEY) {
				txt_sel_all(text);
				do_draw= 1;
			}
			break;

		case CKEY:
			if (G.qual & LR_ALTKEY || G.qual & LR_CTRLKEY) {
				if(G.qual & LR_SHIFTKEY)
					txt_copy_clipboard(text);
				else
					txt_copy_sel(text);

				do_draw= 1;	
			}
			break;

		case DKEY:
			if (G.qual & LR_CTRLKEY) {
				txt_delete_char(text);
				do_draw= 1;
				pop_space_text(st);
			}
			break;

		case EKEY:
			if (G.qual & LR_CTRLKEY) {
				txt_move_eol(text, G.qual & LR_SHIFTKEY);
				do_draw= 1;
				pop_space_text(st);
			}
			break;

		case JKEY:
			if (G.qual & LR_ALTKEY) {
				do_draw= jumptoline_interactive(st);
			}
			break;

		case OKEY:
			if (G.qual & LR_ALTKEY) {
				activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs);
			}
			break;
			
		case MKEY:
			if (G.qual & LR_ALTKEY) {
				txt_export_to_object(text);
				do_draw= 1;	
			}
			break;

		case NKEY:
			if (G.qual & LR_ALTKEY) {
					st->text= add_empty_text();
					st->top= 0;
				
					allqueue(REDRAWTEXT, 0);
					allqueue(REDRAWHEADERS, 0);

			}
			break;

		case PKEY:
			if (G.qual & LR_ALTKEY) {
				run_python_script(st);
				do_draw= 1;
			}
			break;
			
		case RKEY:
			if (G.qual & LR_ALTKEY) {
				txt_do_redo(text);
				do_draw= 1;	
			}
			if (G.qual & LR_CTRLKEY) {
			        if (text->compiled) BPY_free_compiled_text(text);
			        text->compiled = NULL;
				if (okee("Reopen Text")) {
					if (!reopen_text(text)) {
						error("Could not reopen file");
					}
				}
				do_draw= 1;	
			}
			break;
		
		case SKEY:
			if (G.qual & LR_ALTKEY) {
				if (G.qual & LR_SHIFTKEY) 
					if (text) text->flags |= TXT_ISMEM;
					
				txt_write_file(text);
				do_draw= 1;
			}
			break;
			
		case UKEY:
			if (G.qual & LR_ALTKEY) {
				if (G.qual & LR_SHIFTKEY) txt_do_redo(text);
					//txt_print_undo(text); //debug buffer in console
				else {
					txt_do_undo(text);
					do_draw= 1;
				}
			}
			break;

		case VKEY:
			if (G.qual & LR_ALTKEY || G.qual & LR_CTRLKEY) {
				if(G.qual & LR_SHIFTKEY)
					txt_paste_clipboard(text);
				else
					txt_paste(text);

				do_draw= 1;	
				pop_space_text(st);
			}
			break;

		case XKEY:
			if (G.qual & LR_ALTKEY || G.qual & LR_CTRLKEY) {
				txt_cut_sel(text);
				do_draw= 1;	
				pop_space_text(st);
			}
			break;
		
		case ZKEY:
			if (G.qual & LR_ALTKEY || G.qual & LR_CTRLKEY) {
				if (G.qual & LR_SHIFTKEY) {
					txt_do_redo(text);
					do_draw= 1;
				} else {
					txt_do_undo(text);
					do_draw= 1;
				}
			}
			break;

		case TABKEY:
			txt_add_char(text, '\t');
			pop_space_text(st);
			do_draw= 1;
			break;

		case RETKEY:
			txt_split_curline(text);
			do_draw= 1;
			pop_space_text(st);
			break;

		case BACKSPACEKEY:
			txt_backspace_char(text);
			do_draw= 1;
			pop_space_text(st);
			break;

		case DELKEY:
			txt_delete_char(text);
			do_draw= 1;
			pop_space_text(st);
			break;

		case DOWNARROWKEY:
			txt_move_down(text, G.qual & LR_SHIFTKEY);
			do_draw= 1;
			pop_space_text(st);
			break;

		case LEFTARROWKEY:
			txt_move_left(text, G.qual & LR_SHIFTKEY);
			do_draw= 1;
			pop_space_text(st);
			break;

		case RIGHTARROWKEY:
			txt_move_right(text, G.qual & LR_SHIFTKEY);
			do_draw= 1;
			pop_space_text(st);
			break;

		case UPARROWKEY:
			txt_move_up(text, G.qual & LR_SHIFTKEY);
			do_draw= 1;
			pop_space_text(st);
			break;

		case PAGEDOWNKEY:
			screen_skip(st, st->viewlines);
			do_draw= 1;
			break;

		case PAGEUPKEY:
			screen_skip(st, -st->viewlines);
			do_draw= 1;
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
