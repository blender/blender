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

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "DNA_text_types.h"
#include "BKE_text.h"
#include "BKE_suggestions.h"

static SuggList suggestions= {NULL, NULL, NULL, NULL};
static Text *suggText = NULL;

void free_suggestions() {
	SuggItem *item;
	for (item = suggestions.last; item; item=item->prev)
		MEM_freeN(item);
	suggestions.first = suggestions.last = NULL;
	suggestions.firstmatch = suggestions.lastmatch = NULL;
}

void add_suggestion(const char *name, char type) {
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
}

void update_suggestions(const char *prefix) {
	SuggItem *match, *first, *last;
	int cmp, len = strlen(prefix);

	if (!suggestions.first) return;
	if (len==0) {
		suggestions.firstmatch = suggestions.first;
		suggestions.lastmatch = suggestions.last;
		return;
	}
	
	first = last = NULL;
	for (match=suggestions.first; match; match=match->next) {
		cmp = strncmp(prefix, match->name, len);
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
		suggestions.firstmatch = first;
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

void set_suggest_text(Text *text) {
	suggText = text;
}

void clear_suggest_text() {
	free_suggestions();
	suggText = NULL;
}

short is_suggest_active(Text *text) {
	return suggText==text ? 1 : 0;
}
