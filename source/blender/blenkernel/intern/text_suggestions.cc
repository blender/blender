/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"

#include "BKE_text_suggestions.h" /* Own include. */
#include "DNA_text_types.h"

/**********************/
/* Static definitions */
/**********************/

static Text *activeToolText = nullptr;
static SuggList suggestions = {nullptr, nullptr, nullptr, nullptr, nullptr};
static char *documentation = nullptr;
// static int doc_lines = 0;

static void txttl_free_suggest()
{
  SuggItem *item, *prev;
  for (item = suggestions.last; item; item = prev) {
    prev = item->prev;
    MEM_freeN(item);
  }
  suggestions.first = suggestions.last = nullptr;
  suggestions.firstmatch = suggestions.lastmatch = nullptr;
  suggestions.selected = nullptr;
  suggestions.top = 0;
}

static void txttl_free_docs()
{
  MEM_SAFE_FREE(documentation);
}

/**************************/
/* General tool functions */
/**************************/

void free_texttools()
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

void texttool_text_clear()
{
  free_texttools();
  activeToolText = nullptr;
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

  newitem = static_cast<SuggItem *>(MEM_mallocN(sizeof(SuggItem) + len + 1, "SuggItem"));
  if (!newitem) {
    printf("Failed to allocate memory for suggestion.\n");
    return;
  }

  memcpy(newitem->name, name, len + 1);
  newitem->type = type;
  newitem->prev = newitem->next = nullptr;

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
  suggestions.firstmatch = suggestions.lastmatch = suggestions.selected = nullptr;
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

  first = last = nullptr;
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
    suggestions.firstmatch = nullptr;
    suggestions.lastmatch = nullptr;
    suggestions.selected = nullptr;
    suggestions.top = 0;
  }
}

void texttool_suggest_clear()
{
  txttl_free_suggest();
}

SuggItem *texttool_suggest_first()
{
  return suggestions.firstmatch;
}

SuggItem *texttool_suggest_last()
{
  return suggestions.lastmatch;
}

void texttool_suggest_select(SuggItem *sel)
{
  suggestions.selected = sel;
}

SuggItem *texttool_suggest_selected()
{
  return suggestions.selected;
}

int *texttool_suggest_top()
{
  return &suggestions.top;
}
