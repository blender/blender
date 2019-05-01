/*
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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"

#include "DNA_text_types.h"
#include "BKE_suggestions.h"

/**********************/
/* Static definitions */
/**********************/

static Text *activeToolText = NULL;
static SuggList suggestions = {NULL, NULL, NULL, NULL, NULL};
static char *documentation = NULL;
// static int doc_lines = 0;

static void txttl_free_suggest(void)
{
  SuggItem *item, *prev;
  for (item = suggestions.last; item; item = prev) {
    prev = item->prev;
    MEM_freeN(item);
  }
  suggestions.first = suggestions.last = NULL;
  suggestions.firstmatch = suggestions.lastmatch = NULL;
  suggestions.selected = NULL;
  suggestions.top = 0;
}

static void txttl_free_docs(void)
{
  if (documentation) {
    MEM_freeN(documentation);
    documentation = NULL;
  }
}

/**************************/
/* General tool functions */
/**************************/

void free_texttools(void)
{
  txttl_free_suggest();
  txttl_free_docs();
}

void texttool_text_set_active(Text *text)
{
  if (activeToolText == text) {
    return;
  }
  texttool_text_clear();
  activeToolText = text;
}

void texttool_text_clear(void)
{
  free_texttools();
  activeToolText = NULL;
}

short texttool_text_is_active(Text *text)
{
  return activeToolText == text ? 1 : 0;
}

/***************************/
/* Suggestion list methods */
/***************************/

void texttool_suggest_add(const char *name, char type)
{
  const int len = strlen(name);
  int cmp;
  SuggItem *newitem, *item;

  newitem = MEM_mallocN(sizeof(SuggItem) + len + 1, "SuggItem");
  if (!newitem) {
    printf("Failed to allocate memory for suggestion.\n");
    return;
  }

  memcpy(newitem->name, name, len + 1);
  newitem->type = type;
  newitem->prev = newitem->next = NULL;

  /* Perform simple linear search for ordered storage */
  if (!suggestions.first || !suggestions.last) {
    suggestions.first = suggestions.last = newitem;
  }
  else {
    cmp = -1;
    for (item = suggestions.last; item; item = item->prev) {
      cmp = BLI_strncasecmp(name, item->name, len);

      /* Newitem comes after this item, insert here */
      if (cmp >= 0) {
        newitem->prev = item;
        if (item->next) {
          item->next->prev = newitem;
        }
        newitem->next = item->next;
        item->next = newitem;

        /* At last item, set last pointer here */
        if (item == suggestions.last) {
          suggestions.last = newitem;
        }
        break;
      }
    }
    /* Reached beginning of list, insert before first */
    if (cmp < 0) {
      newitem->next = suggestions.first;
      suggestions.first->prev = newitem;
      suggestions.first = newitem;
    }
  }
  suggestions.firstmatch = suggestions.lastmatch = suggestions.selected = NULL;
  suggestions.top = 0;
}

void texttool_suggest_prefix(const char *prefix, const int prefix_len)
{
  SuggItem *match, *first, *last;
  int cmp, top = 0;

  if (!suggestions.first) {
    return;
  }
  if (prefix_len == 0) {
    suggestions.selected = suggestions.firstmatch = suggestions.first;
    suggestions.lastmatch = suggestions.last;
    return;
  }

  first = last = NULL;
  for (match = suggestions.first; match; match = match->next) {
    cmp = BLI_strncasecmp(prefix, match->name, prefix_len);
    if (cmp == 0) {
      if (!first) {
        first = match;
        suggestions.top = top;
      }
    }
    else if (cmp < 0) {
      if (!last) {
        last = match->prev;
        break;
      }
    }
    top++;
  }
  if (first) {
    if (!last) {
      last = suggestions.last;
    }
    suggestions.firstmatch = first;
    suggestions.lastmatch = last;
    suggestions.selected = first;
  }
  else {
    suggestions.firstmatch = NULL;
    suggestions.lastmatch = NULL;
    suggestions.selected = NULL;
    suggestions.top = 0;
  }
}

void texttool_suggest_clear(void)
{
  txttl_free_suggest();
}

SuggItem *texttool_suggest_first(void)
{
  return suggestions.firstmatch;
}

SuggItem *texttool_suggest_last(void)
{
  return suggestions.lastmatch;
}

void texttool_suggest_select(SuggItem *sel)
{
  suggestions.selected = sel;
}

SuggItem *texttool_suggest_selected(void)
{
  return suggestions.selected;
}

int *texttool_suggest_top(void)
{
  return &suggestions.top;
}

/*************************/
/* Documentation methods */
/*************************/

void texttool_docs_show(const char *docs)
{
  int len;

  if (!docs) {
    return;
  }

  len = strlen(docs);

  if (documentation) {
    MEM_freeN(documentation);
    documentation = NULL;
  }

  /* Ensure documentation ends with a '\n' */
  if (docs[len - 1] != '\n') {
    documentation = MEM_mallocN(len + 2, "Documentation");
    memcpy(documentation, docs, len);
    documentation[len++] = '\n';
  }
  else {
    documentation = MEM_mallocN(len + 1, "Documentation");
    memcpy(documentation, docs, len);
  }
  documentation[len] = '\0';
}

char *texttool_docs_get(void)
{
  return documentation;
}

void texttool_docs_clear(void)
{
  txttl_free_docs();
}
