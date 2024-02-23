/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::string_search {

struct SearchItem {
  void *user_data;
  Span<StringRef> normalized_words;
  /**
   * When using menu-search, the search item is often split into multiple groups of words, each of
   * which corresponds to a menu entry. This id is the same for words in the same group and
   * different otherwise.
   */
  Span<int> word_group_ids;
  /**
   * The id of the group that is highlighted in the UI. In some places, the words in this group are
   * given higher weight.
   */
  int main_group_id;
  int main_group_length;
  int total_length;
  int weight;
  /**
   * This is a logical time stamp, i.e. the greater it is, the more recent the item was used. The
   * number is not based on an actual clock.
   */
  int recent_time;
  /**
   * Deprecated items can still be found via search, but are at the bottom of the list.
   */
  bool is_deprecated;
};

struct RecentCache {
  /**
   * Stores a logical time stamp for each previously chosen search item. The higher the time
   * stamp, the more recently the item has been selected.
   */
  Map<std::string, int> logical_time_by_str;
};

/**
 * Sometimes every search item has multiple parts. For example, when using menu search, each nested
 * menu is a separate part. Usually, one of those parts is highlighted in the UI and should be
 * prioritized in the search.
 */
enum class MainWordsHeuristic {
  FirstGroup,
  LastGroup,
  All,
};

/**
 * Non templated base class so that its methods can be implemented outside of this header.
 */
class StringSearchBase {
 protected:
  LinearAllocator<> allocator_;
  Vector<SearchItem> items_;
  const RecentCache *recent_cache_ = nullptr;
  MainWordsHeuristic main_words_heuristic_;

 protected:
  void add_impl(StringRef str, void *user_data, int weight);
  Vector<void *> query_impl(StringRef query) const;
};

/**
 * #StringSearch filters and sorts search items based on a string query. Every search item has data
 * of type T attached that is used to identify it.
 *
 * When querying, the a match score is computed between the query string and each item. Items that
 * don't match are filtered out, the rest is sorted by the score. Elements with the same score are
 * further sorted based on the optionally provided weight and other heuristics.
 *
 * The usage is simple. First #add all the search items and then use the #query method.
 */
template<typename T> class StringSearch : private StringSearchBase {
 public:
  StringSearch(const RecentCache *recent_cache, const MainWordsHeuristic main_words_heuristic)
  {
    recent_cache_ = recent_cache;
    main_words_heuristic_ = main_words_heuristic;
  }

  /**
   * Add a new possible result to the search.
   *
   * \param weight: Can be used to customize the order when multiple items have the same match
   * score.
   */
  void add(const StringRef str, T *user_data, const int weight = 0)
  {
    this->add_impl(str, (void *)user_data, weight);
  }

  /**
   * Filter and sort all previously added search items.
   * Returns an array containing the filtered user data.
   */
  Vector<T *> query(const StringRef query) const
  {
    Vector<void *> result = this->query_impl(query);
    Vector<T *> result_typed = result.as_span().cast<T *>();
    return result_typed;
  }
};

/**
 * Computes the cost of transforming string a into b. The cost/distance is the minimal number of
 * operations that need to be executed. Valid operations are deletion, insertion, substitution and
 * transposition.
 *
 * This function is utf8 aware in the sense that it works at the level of individual code points
 * (1-4 bytes long) instead of on individual bytes.
 */
int damerau_levenshtein_distance(StringRef a, StringRef b);
/**
 * Returns -1 when this is no reasonably good match.
 * Otherwise returns the number of errors in the match.
 */
int get_fuzzy_match_errors(StringRef query, StringRef full);
/**
 * Splits a string into words and normalizes them (currently that just means converting to lower
 * case). The returned strings are allocated in the given allocator.
 */
void extract_normalized_words(StringRef str,
                              LinearAllocator<> &allocator,
                              Vector<StringRef, 64> &r_words,
                              Vector<int, 64> &r_word_group_ids);

}  // namespace blender::string_search
