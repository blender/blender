/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_search.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utf8_symbols.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

/* Right arrow, keep in sync with #UI_MENU_ARROW_SEP in `UI_interface.hh`. */
#define UI_MENU_ARROW_SEP BLI_STR_UTF8_BLACK_RIGHT_POINTING_SMALL_TRIANGLE
#define UI_MENU_ARROW_SEP_UNICODE 0x25b8

namespace blender::string_search {

static int64_t count_utf8_code_points(StringRef str)
{
  return int64_t(BLI_strnlen_utf8(str.data(), size_t(str.size())));
}

int damerau_levenshtein_distance(StringRef a, StringRef b)
{
  constexpr int deletion_cost = 1;
  constexpr int insertion_cost = 1;
  constexpr int substitution_cost = 1;
  constexpr int transposition_cost = 1;

  const int size_a = count_utf8_code_points(a);
  const int size_b = count_utf8_code_points(b);

  /* Instead of keeping the entire table in memory, only keep three rows. The algorithm only
   * accesses these rows and nothing older.
   * All three rows are usually allocated on the stack. At most a single heap allocation is done,
   * if the reserved stack space is too small. */
  const int row_length = size_b + 1;
  Array<int, 64> rows(row_length * 3);

  /* Store rows as spans so that it is cheap to swap them. */
  MutableSpan v0{rows.data() + row_length * 0, row_length};
  MutableSpan v1{rows.data() + row_length * 1, row_length};
  MutableSpan v2{rows.data() + row_length * 2, row_length};

  /* Only v1 needs to be initialized. */
  for (const int i : IndexRange(row_length)) {
    v1[i] = i * insertion_cost;
  }

  uint32_t prev_unicode_a;
  size_t offset_a = 0;
  for (const int i : IndexRange(size_a)) {
    v2[0] = (i + 1) * deletion_cost;

    const uint32_t unicode_a = BLI_str_utf8_as_unicode_step_safe(a.data(), a.size(), &offset_a);

    uint32_t prev_unicode_b;
    size_t offset_b = 0;
    for (const int j : IndexRange(size_b)) {
      const uint32_t unicode_b = BLI_str_utf8_as_unicode_step_safe(b.data(), b.size(), &offset_b);

      /* Check how costly the different operations would be and pick the cheapest - the one with
       * minimal cost. */
      int new_cost = std::min({v1[j + 1] + deletion_cost,
                               v2[j] + insertion_cost,
                               v1[j] + (unicode_a != unicode_b) * substitution_cost});
      if (i > 0 && j > 0) {
        if (unicode_a == prev_unicode_b && prev_unicode_a == unicode_b) {
          new_cost = std::min(new_cost, v0[j - 1] + transposition_cost);
        }
      }

      v2[j + 1] = new_cost;
      prev_unicode_b = unicode_b;
    }

    /* Swap the three rows, so that the next row can be computed. */
    std::tie(v0, v1, v2) = std::tuple<MutableSpan<int>, MutableSpan<int>, MutableSpan<int>>(
        v1, v2, v0);
    prev_unicode_a = unicode_a;
  }

  return v1.last();
}

int get_fuzzy_match_errors(StringRef query, StringRef full)
{
  /* If it is a perfect partial match, return immediately. */
  if (full.find(query) != StringRef::not_found) {
    return 0;
  }

  const int query_size = count_utf8_code_points(query);
  const int full_size = count_utf8_code_points(full);

  /* If there is only a single character which is not in the full string, this is not a match. */
  if (query_size == 1) {
    return -1;
  }
  BLI_assert(query.size() >= 2);

  /* Allow more errors when the size grows larger. */
  const int max_errors = query_size <= 1 ? 0 : query_size / 8 + 1;

  /* If the query is too large, this cannot be a match. */
  if (query_size - full_size > max_errors) {
    return -1;
  }

  const uint32_t query_first_unicode = BLI_str_utf8_as_unicode_safe(query.data());
  const uint32_t query_second_unicode = BLI_str_utf8_as_unicode_safe(
      query.data() + BLI_str_utf8_size_safe(query.data()));

  const char *full_begin = full.begin();
  const char *full_end = full.end();

  const char *window_begin = full_begin;
  const char *window_end = window_begin;
  const int window_size = std::min(query_size + max_errors, full_size);
  const int extra_chars = window_size - query_size;
  const int max_acceptable_distance = max_errors + extra_chars;

  for (int i = 0; i < window_size; i++) {
    window_end += BLI_str_utf8_size_safe(window_end);
  }

  while (true) {
    StringRef window{window_begin, window_end};
    const uint32_t window_begin_unicode = BLI_str_utf8_as_unicode_safe(window_begin);
    int distance = 0;
    /* Expect that the first or second character of the query is correct. This helps to avoid
     * computing the more expensive distance function. */
    if (ELEM(window_begin_unicode, query_first_unicode, query_second_unicode)) {
      distance = damerau_levenshtein_distance(query, window);
      if (distance <= max_acceptable_distance) {
        return distance;
      }
    }
    if (window_end == full_end) {
      return -1;
    }

    /* When the distance is way too large, we can skip a couple of code points, because the
     * distance can't possibly become as short as required. */
    const int window_offset = std::max(1, distance / 2);
    for (int i = 0; i < window_offset && window_end < full_end; i++) {
      window_begin += BLI_str_utf8_size_safe(window_begin);
      window_end += BLI_str_utf8_size_safe(window_end);
    }
  }
}

static constexpr int unused_word = -1;

struct InitialsMatch {
  Vector<int> matched_word_indices;

  int count_main_group_matches(const SearchItem &item) const
  {
    int count = 0;
    for (const int i : this->matched_word_indices) {
      if (item.word_group_ids[i] == item.main_group_id) {
        count++;
      }
    }
    return count;
  }

  bool better_than(const InitialsMatch &other, const SearchItem &item) const
  {
    return this->count_main_group_matches(item) > other.count_main_group_matches(item);
  }
};

/**
 * Takes a query and tries to match it with the first characters of some words. For example, "msfv"
 * matches "Mark Sharp from Vertices". Multiple letters of the beginning of a word can be matched
 * as well. For example, "seboulo" matches "select boundary loop". The order of words is important.
 * So "bose" does not match "select boundary". However, individual words can be skipped. For
 * example, "rocc" matches "rotate edge ccw".
 */
static std::optional<InitialsMatch> match_word_initials(StringRef query,
                                                        const SearchItem &item,
                                                        const Span<int> word_match_map,
                                                        int start = 0)
{
  const Span<StringRef> words = item.normalized_words;
  if (start >= words.size()) {
    return std::nullopt;
  }

  InitialsMatch match;

  size_t query_index = 0;
  int word_index = start;
  size_t char_index = 0;

  int first_found_word_index = -1;

  while (query_index < query.size()) {
    const uint query_unicode = BLI_str_utf8_as_unicode_step_safe(
        query.data(), query.size(), &query_index);
    while (true) {
      /* We are at the end of words, no complete match has been found yet. */
      if (word_index >= words.size()) {
        if (first_found_word_index >= 0) {
          /* Try starting to match at another word. In some cases one can still find matches this
           * way. */
          return match_word_initials(query, item, word_match_map, first_found_word_index + 1);
        }
        return std::nullopt;
      }

      /* Skip words that the caller does not want us to use. */
      if (word_match_map[word_index] != unused_word) {
        word_index++;
        BLI_assert(char_index == 0);
        continue;
      }

      StringRef word = words[word_index];
      /* Try to match the current character with the current word. */
      if (int(char_index) < word.size()) {
        const uint32_t char_unicode = BLI_str_utf8_as_unicode_step_safe(
            word.data(), word.size(), &char_index);
        if (query_unicode == char_unicode) {
          match.matched_word_indices.append(word_index);
          if (first_found_word_index == -1) {
            first_found_word_index = word_index;
          }
          break;
        }
      }

      /* Could not find a match in the current word, go to the beginning of the next word. */
      word_index += 1;
      char_index = 0;
    }
  }
  /* Check if we can find a better match that starts at a later word. */
  if (std::optional<InitialsMatch> sub_match = match_word_initials(
          query, item, word_match_map, first_found_word_index + 1))
  {
    if (sub_match->better_than(match, item)) {
      return sub_match;
    }
  }
  return match;
}

/**
 * The "best" is chosen with combination of word weights and word length.
 */
static int get_best_word_index_that_startswith(StringRef query,
                                               const SearchItem &item,
                                               Span<int> word_match_map,
                                               Span<StringRef> remaining_query_words)
{
  /* If there is another word in the remaining full query that contains the current word, we have
   * to pick the shortest word. If we pick a longer one, it can happen that the other query word
   * does not have a match anymore. This can lead to a situation where a query does not match
   * itself anymore.
   *
   * E.g. the full query `T > Test` wouldn't match itself anymore if `Test` has a higher weight.
   * That's because the `T` would be matched with the `Test`, but then `Test` can't match `Test
   * anymore because that's taken up already.
   *
   * If we don't have to pick the shortest match for correctness, pick the one with the largest
   * weight instead.
   */
  bool use_shortest_match = false;
  for (const StringRef other_word : remaining_query_words) {
    if (other_word.startswith(query)) {
      use_shortest_match = true;
      break;
    }
  }

  int best_word_size = INT32_MAX;
  int best_word_index = -1;
  bool best_word_in_main_group = false;
  for (const int i : item.normalized_words.index_range()) {
    if (word_match_map[i] != unused_word) {
      continue;
    }
    StringRef word = item.normalized_words[i];
    const bool word_in_main_group = item.word_group_ids[i] == item.main_group_id;
    if (word.startswith(query)) {
      bool found_new_best = false;
      if (use_shortest_match) {
        if (word.size() < best_word_size) {
          found_new_best = true;
        }
      }
      else {
        if (!best_word_in_main_group) {
          found_new_best = true;
        }
      }
      if (found_new_best) {
        best_word_index = i;
        best_word_size = word.size();
        best_word_in_main_group = word_in_main_group;
      }
    }
  }
  return best_word_index;
}

static int get_word_index_that_fuzzy_matches(StringRef query,
                                             Span<StringRef> words,
                                             Span<int> word_match_map,
                                             int *r_error_count)
{
  for (const int i : words.index_range()) {
    if (word_match_map[i] != unused_word) {
      continue;
    }
    StringRef word = words[i];
    const int error_count = get_fuzzy_match_errors(query, word);
    if (error_count >= 0) {
      *r_error_count = error_count;
      return i;
    }
  }
  return -1;
}

/**
 * Checks how well the query matches a result. If it does not match, -1 is returned. A positive
 * return value indicates how good the match is. The higher the value, the better the match.
 */
static std::optional<float> score_query_against_words(Span<StringRef> query_words,
                                                      const SearchItem &item)
{
  /* A mapping from #result_words to #query_words. It's mainly used to determine if a word has been
   * matched already to avoid matching it again. */
  Array<int, 64> word_match_map(item.normalized_words.size(), unused_word);

  /* Start with some high score, because otherwise the final score might become negative. */
  float total_match_score = item.is_deprecated ? 500 : 1000;

  for (const int query_word_index : query_words.index_range()) {
    const StringRef query_word = query_words[query_word_index];
    {
      /* Check if any result word begins with the query word. */
      const int word_index = get_best_word_index_that_startswith(
          query_word, item, word_match_map, query_words.drop_front(query_word_index + 1));
      if (word_index >= 0) {
        /* Give a match in a main group higher priority. */
        const bool is_main_group = item.word_group_ids[word_index] == item.main_group_id;
        total_match_score += is_main_group ? 10 : 9;
        word_match_map[word_index] = query_word_index;
        continue;
      }
    }
    {
      /* Try to match against word initials. */
      if (std::optional<InitialsMatch> match = match_word_initials(
              query_word, item, word_match_map))
      {
        /* If the all matched words are in the main group, give the match a higher priority. */
        bool all_main_group_matches = match->count_main_group_matches(item) ==
                                      match->matched_word_indices.size();
        total_match_score += all_main_group_matches ? 4 : 3;
        for (const int i : match->matched_word_indices) {
          word_match_map[i] = query_word_index;
        }
        continue;
      }
    }
    {
      /* Fuzzy match against words. */
      int error_count = 0;
      const int word_index = get_word_index_that_fuzzy_matches(
          query_word, item.normalized_words, word_match_map, &error_count);
      if (word_index >= 0) {
        total_match_score += 3 - error_count;
        word_match_map[word_index] = query_word_index;
        continue;
      }
    }

    /* Couldn't match query word with anything. */
    return std::nullopt;
  }

  {
    /* Add penalty when query words are not in the correct order. */
    Vector<int> match_indices;
    for (const int index : word_match_map) {
      if (index != unused_word) {
        match_indices.append(index);
      }
    }
    if (!match_indices.is_empty()) {
      for (const int i : IndexRange(match_indices.size() - 1)) {
        if (match_indices[i] > match_indices[i + 1]) {
          total_match_score -= 1;
        }
      }
    }
  }

  return total_match_score;
}

void extract_normalized_words(StringRef str,
                              LinearAllocator<> &allocator,
                              Vector<StringRef, 64> &r_words,
                              Vector<int, 64> &r_word_group_ids)
{
  const uint32_t unicode_space = uint32_t(' ');
  const uint32_t unicode_dash = uint32_t('-');
  const uint32_t unicode_underscore = uint32_t('_');
  const uint32_t unicode_slash = uint32_t('/');
  const uint32_t unicode_right_triangle = UI_MENU_ARROW_SEP_UNICODE;

  BLI_assert(unicode_space == BLI_str_utf8_as_unicode_safe(" "));
  BLI_assert(unicode_dash == BLI_str_utf8_as_unicode_safe("-"));
  BLI_assert(unicode_underscore == BLI_str_utf8_as_unicode_safe("_"));
  BLI_assert(unicode_slash == BLI_str_utf8_as_unicode_safe("/"));
  BLI_assert(unicode_right_triangle == BLI_str_utf8_as_unicode_safe(UI_MENU_ARROW_SEP));

  auto is_separator = [&](uint32_t unicode) {
    return ELEM(unicode,
                unicode_space,
                unicode_dash,
                unicode_underscore,
                unicode_slash,
                unicode_right_triangle);
  };

  Vector<int, 64> section_indices;

  /* Make a copy of the string so that we can edit it. */
  StringRef str_copy = allocator.copy_string(str);
  char *mutable_copy = const_cast<char *>(str_copy.data());
  const size_t str_size_in_bytes = size_t(str.size());
  BLI_str_tolower_ascii(mutable_copy, str_size_in_bytes);

  /* Iterate over all unicode code points to split individual words. */
  int group_id = 0;
  bool is_in_word = false;
  size_t word_start = 0;
  size_t offset = 0;
  while (offset < str_size_in_bytes) {
    size_t size = offset;
    uint32_t unicode = BLI_str_utf8_as_unicode_step_safe(str.data(), str.size(), &size);
    size -= offset;
    if (is_separator(unicode)) {
      if (is_in_word) {
        const StringRef word = str_copy.substr(int(word_start), int(offset - word_start));
        r_words.append(word);
        r_word_group_ids.append(group_id);
        is_in_word = false;
      }
    }
    else {
      if (!is_in_word) {
        word_start = offset;
        is_in_word = true;
      }
    }
    if (unicode == unicode_right_triangle) {
      group_id++;
    }
    offset += size;
  }
  /* If the last word is not followed by a separator, it has to be handled separately. */
  if (is_in_word) {
    const StringRef word = str_copy.drop_prefix(int(word_start));
    r_words.append(word);
    r_word_group_ids.append(group_id);
  }
}

void StringSearchBase::add_impl(const StringRef str, void *user_data, const int weight)
{
  Vector<StringRef, 64> words;
  Vector<int, 64> word_group_ids;
  string_search::extract_normalized_words(str, allocator_, words, word_group_ids);
  const int recent_time = recent_cache_ ?
                              recent_cache_->logical_time_by_str.lookup_default(str, -1) :
                              -1;
  int main_group_id = 0;
  if (!word_group_ids.is_empty()) {
    switch (main_words_heuristic_) {
      case MainWordsHeuristic::FirstGroup: {
        main_group_id = 0;
        break;
      }
      case MainWordsHeuristic::LastGroup: {
        main_group_id = word_group_ids.last();
        break;
      }
      case MainWordsHeuristic::All: {
        main_group_id = 0;
        word_group_ids.fill(0);
        break;
      }
    }
  }

  int main_group_length = 0;
  for (const int i : words.index_range()) {
    if (word_group_ids[i] == main_group_id) {
      main_group_length += int(words[i].size());
    }
  }

  /* Not checking for the "D" to avoid problems with upper/lower-case. */
  const bool is_deprecated = str.find("eprecated") != StringRef::not_found;

  items_.append({user_data,
                 allocator_.construct_array_copy(words.as_span()),
                 allocator_.construct_array_copy(word_group_ids.as_span()),
                 main_group_id,
                 main_group_length,
                 int(str.size()),
                 weight,
                 recent_time,
                 is_deprecated});
}

Vector<void *> StringSearchBase::query_impl(const StringRef query) const
{
  LinearAllocator<> allocator;
  Vector<StringRef, 64> query_words;
  /* This is just a dummy value that is not used for the query. */
  Vector<int, 64> word_group_ids;
  string_search::extract_normalized_words(query, allocator, query_words, word_group_ids);

  /* Compute score of every result. */
  Array<std::optional<float>> all_scores(items_.size());
  threading::parallel_for(items_.index_range(), 256, [&](const IndexRange range) {
    for (const int i : range) {
      const SearchItem &item = items_[i];
      const std::optional<float> score = string_search::score_query_against_words(query_words,
                                                                                  item);
      all_scores[i] = score;
    }
  });
  MultiValueMap<float, int> result_indices_by_score;
  for (const int i : items_.index_range()) {
    const std::optional<float> score = all_scores[i];
    if (score.has_value()) {
      result_indices_by_score.add(*score, i);
    }
  }

  Vector<float> found_scores;
  for (const float score : result_indices_by_score.keys()) {
    found_scores.append(score);
  }
  std::sort(found_scores.begin(), found_scores.end(), std::greater<>());

  /* Add results to output vector in correct order. First come the results with the best match
   * score. Results with the same score are in the order they have been added to the search. */
  Vector<int> sorted_result_indices;
  for (const float score : found_scores) {
    MutableSpan<int> indices = result_indices_by_score.lookup(score);
    if (score == found_scores[0]) {
      if (!query.is_empty()) {
        /* Sort items with best score by length. Shorter items are more likely the ones you are
         * looking for. This also ensures that exact matches will be at the top, even if the query
         * is a sub-string of another item. */
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
          const SearchItem &item_a = items_[a];
          const SearchItem &item_b = items_[b];
          /* The length of the main group has priority over the total length. */
          return item_a.main_group_length < item_b.main_group_length ||
                 (item_a.main_group_length == item_b.main_group_length &&
                  item_a.total_length < item_b.total_length);
        });
        /* Prefer items with larger weights. Use `stable_sort` so that if the weights are the same,
         * the order won't be changed. */
        std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) {
          return items_[a].weight > items_[b].weight;
        });
      }
      /* If the query gets longer, it's less likely that accessing recent items is desired. Better
       * always show the best match in this case. */
      if (query.size() <= 1) {
        /* Prefer items that have been selected recently. */
        std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) {
          return items_[a].recent_time > items_[b].recent_time;
        });
      }
    }
    sorted_result_indices.extend(indices);
  }

  Vector<void *> sorted_data(sorted_result_indices.size());
  for (const int i : sorted_result_indices.index_range()) {
    const int result_index = sorted_result_indices[i];
    const SearchItem &item = items_[result_index];
    sorted_data[i] = item.user_data;
  }
  return sorted_data;
}

}  // namespace blender::string_search
