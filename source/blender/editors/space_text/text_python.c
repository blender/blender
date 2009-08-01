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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "BKE_suggestions.h"
#include "BKE_text.h"

#include "BLI_blenlib.h"

#include "WM_api.h"
#include "WM_types.h"

#include "text_intern.h"

int text_do_suggest_select(SpaceText *st, ARegion *ar)
{
	SuggItem *item, *first, *last, *sel;
	TextLine *tmp;
	int l, x, y, w, h, i;
	int tgti, *top;
	short mval[2];
	
	if(!st || !st->text) return 0;
	if(!texttool_text_is_active(st->text)) return 0;

	first = texttool_suggest_first();
	last = texttool_suggest_last();
	sel = texttool_suggest_selected();
	top = texttool_suggest_top();

	if(!last || !first)
		return 0;

	/* Count the visible lines to the cursor */
	for(tmp=st->text->curl, l=-st->top; tmp; tmp=tmp->prev, l++);
	if(l<0) return 0;

	text_update_character_width(st);
	
	if(st->showlinenrs) {
		x = st->cwidth*(st->text->curc-st->left) + TXT_OFFSET + TEXTXLOC - 4;
	}
	else {
		x = st->cwidth*(st->text->curc-st->left) + TXT_OFFSET - 4;
	}
	y = ar->winy - st->lheight*l - 2;

	w = SUGG_LIST_WIDTH*st->cwidth + 20;
	h = SUGG_LIST_SIZE*st->lheight + 8;

	// XXX getmouseco_areawin(mval);

	if(mval[0]<x || x+w<mval[0] || mval[1]<y-h || y<mval[1])
		return 0;

	/* Work out which of the items is at the top of the visible list */
	for(i=0, item=first; i<*top && item->next; i++, item=item->next);

	/* Work out the target item index in the visible list */
	tgti = (y-mval[1]-4) / st->lheight;
	if(tgti<0 || tgti>SUGG_LIST_SIZE)
		return 1;

	for(i=tgti; i>0 && item->next; i--, item=item->next);
	if(item)
		texttool_suggest_select(item);
	return 1;
}

void text_pop_suggest_list()
{
	SuggItem *item, *sel;
	int *top, i;

	item= texttool_suggest_first();
	sel= texttool_suggest_selected();
	top= texttool_suggest_top();

	i= 0;
	while(item && item != sel) {
		item= item->next;
		i++;
	}
	if(i > *top+SUGG_LIST_SIZE-1)
		*top= i-SUGG_LIST_SIZE+1;
	else if(i < *top)
		*top= i;
}

static void get_suggest_prefix(Text *text, int offset)
{
	int i, len;
	char *line, tmp[256];

	if(!text) return;
	if(!texttool_text_is_active(text)) return;

	line= text->curl->line;
	for(i=text->curc-1+offset; i>=0; i--)
		if(!text_check_identifier(line[i]))
			break;
	i++;
	len= text->curc-i+offset;
	if(len > 255) {
		printf("Suggestion prefix too long\n");
		len = 255;
	}
	BLI_strncpy(tmp, line+i, len);
	tmp[len]= '\0';
	texttool_suggest_prefix(tmp);
}

static void confirm_suggestion(Text *text, int skipleft)
{
	SuggItem *sel;
	int i, over=0;
	char *line;

	if(!text) return;
	if(!texttool_text_is_active(text)) return;

	sel = texttool_suggest_selected();
	if(!sel) return;

	line= text->curl->line;
	i=text->curc-skipleft-1;
	while(i>=0) {
		if(!text_check_identifier(line[i]))
			break;
		over++;
		i--;
	}

	for(i=0; i<skipleft; i++)
		txt_move_left(text, 0);
	for(i=0; i<over; i++)
		txt_move_left(text, 1);

	txt_insert_buf(text, sel->name);
	
	for(i=0; i<skipleft; i++)
		txt_move_right(text, 0);

	texttool_text_clear();
}

// XXX
#define L_MOUSE 0
#define M_MOUSE 0
#define R_MOUSE 0
#define LR_SHIFTKEY 0
#define LR_ALTKEY 0
#define LR_CTRLKEY 0
#define LR_COMMANDKEY 0

// XXX
static int doc_scroll= 0;

short do_texttools(SpaceText *st, char ascii, unsigned short evnt, short val)
{
	ARegion *ar= NULL; // XXX
	int qual= 0; // XXX
	int draw=0, tools=0, swallow=0, scroll=1;
	if(!texttool_text_is_active(st->text)) return 0;
	if(!st->text || st->text->id.lib) return 0;

	if(st->doplugins && texttool_text_is_active(st->text)) {
		if(texttool_suggest_first()) tools |= TOOL_SUGG_LIST;
		if(texttool_docs_get()) tools |= TOOL_DOCUMENT;
	}

	if(ascii) {
		if(tools & TOOL_SUGG_LIST) {
			if((ascii != '_' && ascii != '*' && ispunct(ascii)) || text_check_whitespace(ascii)) {
				confirm_suggestion(st->text, 0);
				text_update_line_edited(st->text, st->text->curl);
			}
			else if((st->overwrite && txt_replace_char(st->text, ascii)) || txt_add_char(st->text, ascii)) {
				get_suggest_prefix(st->text, 0);
				text_pop_suggest_list();
				swallow= 1;
				draw= 1;
			}
		}
		if(tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0, draw= 1;

	}
	else if(val==1 && evnt) {
		switch (evnt) {
			case LEFTMOUSE:
				if(text_do_suggest_select(st, ar))
					swallow= 1;
				else {
					if(tools & TOOL_SUGG_LIST) texttool_suggest_clear();
					if(tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				}
				draw= 1;
				break;
			case MIDDLEMOUSE:
				if(text_do_suggest_select(st, ar)) {
					confirm_suggestion(st->text, 0);
					text_update_line_edited(st->text, st->text->curl);
					swallow= 1;
				}
				else {
					if(tools & TOOL_SUGG_LIST) texttool_suggest_clear();
					if(tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				}
				draw= 1;
				break;
			case ESCKEY:
				draw= swallow= 1;
				if(tools & TOOL_SUGG_LIST) texttool_suggest_clear();
				else if(tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				else draw= swallow= 0;
				break;
			case RETKEY:
				if(tools & TOOL_SUGG_LIST) {
					confirm_suggestion(st->text, 0);
					text_update_line_edited(st->text, st->text->curl);
					swallow= 1;
					draw= 1;
				}
				if(tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0, draw= 1;
				break;
			case LEFTARROWKEY:
			case BACKSPACEKEY:
				if(tools & TOOL_SUGG_LIST) {
					if(qual)
						texttool_suggest_clear();
					else {
						/* Work out which char we are about to delete/pass */
						if(st->text->curl && st->text->curc > 0) {
							char ch= st->text->curl->line[st->text->curc-1];
							if((ch=='_' || !ispunct(ch)) && !text_check_whitespace(ch)) {
								get_suggest_prefix(st->text, -1);
								text_pop_suggest_list();
							}
							else
								texttool_suggest_clear();
						}
						else
							texttool_suggest_clear();
					}
				}
				if(tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				break;
			case RIGHTARROWKEY:
				if(tools & TOOL_SUGG_LIST) {
					if(qual)
						texttool_suggest_clear();
					else {
						/* Work out which char we are about to pass */
						if(st->text->curl && st->text->curc < st->text->curl->len) {
							char ch= st->text->curl->line[st->text->curc+1];
							if((ch=='_' || !ispunct(ch)) && !text_check_whitespace(ch)) {
								get_suggest_prefix(st->text, 1);
								text_pop_suggest_list();
							}
							else
								texttool_suggest_clear();
						}
						else
							texttool_suggest_clear();
					}
				}
				if(tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0;
				break;
			case PAGEDOWNKEY:
				scroll= SUGG_LIST_SIZE-1;
			case WHEELDOWNMOUSE:
			case DOWNARROWKEY:
				if(tools & TOOL_DOCUMENT) {
					doc_scroll++;
					swallow= 1;
					draw= 1;
					break;
				}
				else if(tools & TOOL_SUGG_LIST) {
					SuggItem *sel = texttool_suggest_selected();
					if(!sel) {
						texttool_suggest_select(texttool_suggest_first());
					}
					else while(sel && sel!=texttool_suggest_last() && sel->next && scroll--) {
						texttool_suggest_select(sel->next);
						sel= sel->next;
					}
					text_pop_suggest_list();
					swallow= 1;
					draw= 1;
					break;
				}
			case PAGEUPKEY:
				scroll= SUGG_LIST_SIZE-1;
			case WHEELUPMOUSE:
			case UPARROWKEY:
				if(tools & TOOL_DOCUMENT) {
					if(doc_scroll>0) doc_scroll--;
					swallow= 1;
					draw= 1;
					break;
				}
				else if(tools & TOOL_SUGG_LIST) {
					SuggItem *sel = texttool_suggest_selected();
					while(sel && sel!=texttool_suggest_first() && sel->prev && scroll--) {
						texttool_suggest_select(sel->prev);
						sel= sel->prev;
					}
					text_pop_suggest_list();
					swallow= 1;
					draw= 1;
					break;
				}
			case RIGHTSHIFTKEY:
			case LEFTSHIFTKEY:
				break;
			default:
				if(tools & TOOL_SUGG_LIST) texttool_suggest_clear(), draw= 1;
				if(tools & TOOL_DOCUMENT) texttool_docs_clear(), doc_scroll= 0, draw= 1;
		}
	}

	if(draw)
		{}; // XXX redraw_alltext();
	
	return swallow;
}

#if 0
#ifndef DISABLE_PYTHON	
	/* Run text plugin scripts if enabled */
	if(st->doplugins && event && val) {
		if(BPY_menu_do_shortcut(PYMENU_TEXTPLUGIN, event, qual)) {
			do_draw= 1;
		}
	}
#endif
	if(do_draw)
		; // XXX redraw_alltext();
#endif

short do_textmarkers(SpaceText *st, char ascii, unsigned short evnt, short val)
{
	Text *text;
	TextMarker *marker, *mrk, *nxt;
	int c, s, draw=0, swallow=0;
	int qual= 0; // XXX

	text= st->text;
	if(!text || text->id.lib || text->curl != text->sell) return 0;

	marker= txt_find_marker(text, text->sell, text->selc, 0, 0);
	if(marker && (marker->start > text->curc || marker->end < text->curc))
		marker= NULL;

	if(!marker) {
		/* Find the next temporary marker */
		if(evnt==TABKEY) {
			int lineno= txt_get_span(text->lines.first, text->curl);
			TextMarker *mrk= text->markers.first;
			while(mrk) {
				if(!marker && (mrk->flags & TMARK_TEMP)) marker= mrk;
				if((mrk->flags & TMARK_TEMP) && (mrk->lineno > lineno || (mrk->lineno==lineno && mrk->end > text->curc))) {
					marker= mrk;
					break;
				}
				mrk= mrk->next;
			}
			if(marker) {
				txt_move_to(text, marker->lineno, marker->start, 0);
				txt_move_to(text, marker->lineno, marker->end, 1);
				// XXX WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
				evnt= ascii= val= 0;
				draw= 1;
				swallow= 1;
			}
		}
		else if(evnt==ESCKEY) {
			if(txt_clear_markers(text, 0, TMARK_TEMP)) swallow= 1;
			else if(txt_clear_markers(text, 0, 0)) swallow= 1;
			else return 0;
			evnt= ascii= val= 0;
			draw= 1;
		}
		if(!swallow) return 0;
	}

	if(ascii) {
		if(marker->flags & TMARK_EDITALL) {
			c= text->curc-marker->start;
			s= text->selc-marker->start;
			if(s<0 || s>marker->end-marker->start) return 0;

			mrk= txt_next_marker(text, marker);
			while(mrk) {
				nxt=txt_next_marker(text, mrk); /* mrk may become invalid */
				txt_move_to(text, mrk->lineno, mrk->start+c, 0);
				if(s!=c) txt_move_to(text, mrk->lineno, mrk->start+s, 1);
				if(st->overwrite) {
					if(txt_replace_char(text, ascii))
						text_update_line_edited(st->text, st->text->curl);
				}
				else {
					if(txt_add_char(text, ascii)) {
						text_update_line_edited(st->text, st->text->curl);
					}
				}

				if(mrk==marker || mrk==nxt) break;
				mrk=nxt;
			}
			swallow= 1;
			draw= 1;
		}
	}
	else if(val) {
		switch(evnt) {
			case BACKSPACEKEY:
				if(marker->flags & TMARK_EDITALL) {
					c= text->curc-marker->start;
					s= text->selc-marker->start;
					if(s<0 || s>marker->end-marker->start) return 0;
					
					mrk= txt_next_marker(text, marker);
					while(mrk) {
						nxt= txt_next_marker(text, mrk); /* mrk may become invalid */
						txt_move_to(text, mrk->lineno, mrk->start+c, 0);
						if(s!=c) txt_move_to(text, mrk->lineno, mrk->start+s, 1);
						txt_backspace_char(text);
						text_update_line_edited(st->text, st->text->curl);
						if(mrk==marker || mrk==nxt) break;
						mrk= nxt;
					}
					swallow= 1;
					draw= 1;
				}
				break;
			case DELKEY:
				if(marker->flags & TMARK_EDITALL) {
					c= text->curc-marker->start;
					s= text->selc-marker->start;
					if(s<0 || s>marker->end-marker->start) return 0;
					
					mrk= txt_next_marker(text, marker);
					while(mrk) {
						nxt= txt_next_marker(text, mrk); /* mrk may become invalid */
						txt_move_to(text, mrk->lineno, mrk->start+c, 0);
						if(s!=c) txt_move_to(text, mrk->lineno, mrk->start+s, 1);
						txt_delete_char(text);
						text_update_line_edited(st->text, st->text->curl);
						if(mrk==marker || mrk==nxt) break;
						mrk= nxt;
					}
					swallow= 1;
					draw= 1;
				}
				break;
			case TABKEY:
				if(qual & LR_SHIFTKEY) {
					nxt= marker->prev;
					if(!nxt) nxt= text->markers.last;
				}
				else {
					nxt= marker->next;
					if(!nxt) nxt= text->markers.first;
				}
				if(marker->flags & TMARK_TEMP) {
					if(nxt==marker) nxt= NULL;
					BLI_freelinkN(&text->markers, marker);
				}
				mrk= nxt;
				if(mrk) {
					txt_move_to(text, mrk->lineno, mrk->start, 0);
					txt_move_to(text, mrk->lineno, mrk->end, 1);
					// XXX WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
				}
				swallow= 1;
				draw= 1;
				break;

			/* Events that should clear markers */
			case UKEY: if(!(qual & LR_ALTKEY)) break;
			case ZKEY: if(evnt==ZKEY && !(qual & LR_CTRLKEY)) break;
			case RETKEY:
			case ESCKEY:
				if(marker->flags & (TMARK_EDITALL | TMARK_TEMP))
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
				if(qual & LR_SHIFTKEY) swallow= 1;
				break;

			default:
				if(qual!=0 && qual!=LR_SHIFTKEY)
					swallow= 1; /* Swallow all other shortcut events */
		}
	}
	
	if(draw)
		{}; // XXX redraw_alltext();
	
	return swallow;
}

