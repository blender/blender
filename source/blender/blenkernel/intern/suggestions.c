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

static SuggList suggestions = {NULL, NULL, NULL, NULL, NULL};
static Text *suggText = NULL;
static SuggItem *lastInsert = NULL;
static char *documentation = NULL;
static int doc_lines = 0;

static int suggest_cmp(const char *first, const char *second, int len) {	
	int cmp, i;
	for (cmp=0, i=0; i<len; i++) {
		if (cmp= toupper(first[i])-toupper(second[i])) {
			break;
		}
	}
	return cmp;
}

static void sugg_free() {
	SuggItem *item, *prev;
	for (item = suggestions.last; item; item=prev) {
		prev = item->prev;
		MEM_freeN(item);
	}
	suggestions.first = suggestions.last = NULL;
	suggestions.firstmatch = suggestions.lastmatch = NULL;
	suggestions.selected = NULL;
}

static void docs_free() {
	if (documentation) {
		MEM_freeN(documentation);
		documentation = NULL;
	}
}

void free_suggestions() {
	sugg_free();
	docs_free();
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
	int cmp, len = strlen(prefix);

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
		suggestions.firstmatch = first;
		suggestions.lastmatch = last;
		suggestions.selected = first;
	} else {
		suggestions.firstmatch = NULL;
		suggestions.lastmatch = NULL;
		suggestions.selected = NULL;
	}
}

SuggItem *suggest_first() {
	return suggestions.firstmatch;
}

SuggItem *suggest_last() {
	return suggestions.lastmatch;
}

void suggest_set_active(Text *text) {
	if (suggText == text) return;
	suggest_clear_active();
	suggText = text;
}

void suggest_clear_active() {
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

/* Documentation methods */

void suggest_documentation(const char *docs) {
	int len;

	if (!docs) return;

	len = strlen(docs);

	if (documentation) {
		MEM_freeN(documentation);
		documentation = NULL;
	}

	/* Ensure documentation ends with a '\n' */
	if (docs[len-1] != '\n') {
		documentation = MEM_mallocN(len+2, "Documentation");
		strncpy(documentation, docs, len);
		documentation[len++] = '\n';
	} else {
		documentation = MEM_mallocN(len+1, "Documentation");
		strncpy(documentation, docs, len);
	}
	documentation[len] = '\0';
}

char *suggest_get_docs() {
	return documentation;
}

void suggest_clear_docs() {
	docs_free();
}
