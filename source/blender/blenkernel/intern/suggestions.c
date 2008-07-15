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
 * Contributor(s): Ian Thompson.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "DNA_text_types.h"
#include "BKE_text.h"
#include "BKE_suggestions.h"

static SuggList suggestions= {NULL, NULL, NULL, NULL, NULL};
static Text *suggText = NULL;
static SuggItem *lastInsert= NULL;

static suggest_cmp(const char *first, const char *second, int len) {	
	int cmp, i;
	for (cmp=0, i=0; i<len; i++) {
		if (cmp= toupper(first[i]) - toupper(second[i])) {
			break;
		}
	}
	return cmp;
}
void free_suggestions() {
	SuggItem *item, *prev;
	for (item = suggestions.last; item; item=prev) {
		prev = item->prev;
		MEM_freeN(item);
	}
	suggestions.first = suggestions.last = NULL;
	suggestions.firstmatch = suggestions.lastmatch = NULL;
	suggestions.selected = NULL;
}

void suggest_add(const char *name, char type) {
	SuggItem *newitem;

	newitem = MEM_mallocN(sizeof(SuggItem) + strlen(name) + 1, "SuggestionItem");
	if (!newitem) {
		printf("Failed to allocate memory for suggestion.\n");
		return;
	}

	newitem->name = (char *) (newitem + 1);
	strcpy(newitem->name, name);
	newitem->type = type;
	newitem->prev = newitem->next = NULL;

	if (!suggestions.first) {
		suggestions.first = suggestions.last = newitem;
	} else {
		newitem->prev = suggestions.last;
		suggestions.last->next = newitem;
		suggestions.last = newitem;
	}
	suggestions.selected = NULL;
}

void suggest_prefix(const char *prefix) {
	SuggItem *match, *first, *last;
	int cmp, len = strlen(prefix), i;

	if (!suggestions.first) return;
	if (len==0) {
		suggestions.selected = suggestions.firstmatch = suggestions.first;
		suggestions.lastmatch = suggestions.last;
		return;
	}
	
	first = last = NULL;
	for (match=suggestions.first; match; match=match->next) {
		cmp = suggest_cmp(prefix, match->name, len);
		if (cmp==0) {
			if (!first)
				first = match;
		} else if (cmp<0) {
			if (!last) {
				last = match->prev;
				break;
			}
		}
	}
	if (first) {
		if (!last) last = suggestions.last;
		suggestions.selected = suggestions.firstmatch = first;
		suggestions.lastmatch = last;
	} else {
		suggestions.firstmatch = suggestions.lastmatch = NULL;
	}
}

SuggItem *suggest_first() {
	return suggestions.firstmatch;
}

SuggItem *suggest_last() {
	return suggestions.lastmatch;
}

void suggest_set_text(Text *text) {
	suggText = text;
}

void suggest_clear_text() {
	free_suggestions();
	suggText = NULL;
}

short suggest_is_active(Text *text) {
	return suggText==text ? 1 : 0;
}

void suggest_set_selected(SuggItem *sel) {
	suggestions.selected = sel;
}

SuggItem *suggest_get_selected() {
	return suggestions.selected;
}
