/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

namespace blender {

struct Text;

struct SuggItem {
  struct SuggItem *prev, *next;
  char type;
  char name[0];
};

/**
 * Suggestions should be added in sorted order although a linear sorting method is implemented.
 * The list is then divided up based on the prefix provided by update_suggestions:
 *
 * Example:
 *   Prefix: `ab`
 *   `aaa` <- #SuggList::first
 *   `aab`
 *   `aba` <- #SuggList::firstmatch
 *   `abb` <- #SuggList::lastmatch
 *   `baa`
 *   `bab` <- #SuggList::last
 */
struct SuggList {
  SuggItem *first, *last;
  SuggItem *firstmatch, *lastmatch;
  SuggItem *selected;
  int top;
};

/* Free all text tool memory */
void free_texttools();

/* Used to identify which Text object the current tools should appear against */
void texttool_text_set_active(struct Text *text);
void texttool_text_clear();
short texttool_text_is_active(struct Text *text);

/* Suggestions */
void texttool_suggest_add(const char *name, char type);
void texttool_suggest_prefix(const char *prefix, int prefix_len);
void texttool_suggest_clear();
SuggItem *texttool_suggest_first();
SuggItem *texttool_suggest_last();
void texttool_suggest_select(SuggItem *sel);
SuggItem *texttool_suggest_selected();
int *texttool_suggest_top();

}  // namespace blender
