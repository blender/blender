/**	
 * $Id: $ 
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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_SUGGESTIONS_H
#define BKE_SUGGESTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ****************************************************************************
Suggestions should be added in sorted order although a linear sorting method is
implemented. The list is then divided up based on the prefix provided by
update_suggestions:

Example:
  Prefix: ab
  aaa <-- first
  aab
  aba <-- firstmatch
  abb <-- lastmatch
  baa
  bab <-- last
**************************************************************************** */

struct Text;

typedef struct SuggItem {
	struct SuggItem *prev, *next;
	char *name;
	char type;
} SuggItem;

typedef struct SuggList {
	SuggItem *first, *last;
	SuggItem *firstmatch, *lastmatch;
	SuggItem *selected;
	int top;
} SuggList;

/* Free all text tool memory */
void free_texttools();

/* Used to identify which Text object the current tools should appear against */
void texttool_text_set_active(Text *text);
void texttool_text_clear();
short texttool_text_is_active(Text *text);

/* Suggestions */
void texttool_suggest_add(const char *name, char type);
void texttool_suggest_prefix(const char *prefix);
void texttool_suggest_clear();
SuggItem *texttool_suggest_first();
SuggItem *texttool_suggest_last();
void texttool_suggest_select(SuggItem *sel);
SuggItem *texttool_suggest_selected();
int *texttool_suggest_top();

/* Documentation */
void texttool_docs_show(const char *docs);
char *texttool_docs_get();
void texttool_docs_clear();

#ifdef __cplusplus
}
#endif

#endif
