/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_search.hh"

namespace blender::ui::string_search {

/**
 * Remember the string that the user chose. This allows us to put it higher up in the search items
 * later on.
 */
void add_recent_search(StringRef chosen_str);

/**
 * Depending on the user preferences, either outputs the recent cache or null.
 */
const blender::string_search::RecentCache *get_recent_cache_or_null();

void write_recent_searches_file();
void read_recent_searches_file();

/**
 * Wrapper for the lower level #StringSearch in blenlib that takes recent searches into account
 * automatically.
 */
template<typename T> class StringSearch : public blender::string_search::StringSearch<T> {
 public:
  StringSearch(const blender::string_search::MainWordsHeuristic main_word_heuristic =
                   blender::string_search::MainWordsHeuristic::LastGroup)
      : blender::string_search::StringSearch<T>(get_recent_cache_or_null(), main_word_heuristic)
  {
  }
};

}  // namespace blender::ui::string_search
