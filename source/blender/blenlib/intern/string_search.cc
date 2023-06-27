/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_search.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utf8_symbols.h"
#include "BLI_timeit.hh"

/* Right arrow, keep in sync with #UI_MENU_ARROW_SEP in `UI_interface.h`. */
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

    const uint32_t unicode_a = BLI_str_utf8_as_unicode_step(a.data(), a.size(), &offset_a);

    uint32_t prev_unicode_b;
    size_t offset_b = 0;
    for (const int j : IndexRange(size_b)) {
      const uint32_t unicode_b = BLI_str_utf8_as_unicode_step(b.data(), b.size(), &offset_b);

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

  const uint32_t query_first_unicode = BLI_str_utf8_as_unicode(query.data());
  const uint32_t query_second_unicode = BLI_str_utf8_as_unicode(query.data() +
                                                                BLI_str_utf8_size(query.data()));

  const char *full_begin = full.begin();
  const char *full_end = full.end();

  const char *window_begin = full_begin;
  const char *window_end = window_begin;
  const int window_size = std::min(query_size + max_errors, full_size);
  const int extra_chars = window_size - query_size;
  const int max_acceptable_distance = max_errors + extra_chars;

  for (int i = 0; i < window_size; i++) {
    window_end += BLI_str_utf8_size(window_end);
  }

  while (true) {
    StringRef window{window_begin, window_end};
    const uint32_t window_begin_unicode = BLI_str_utf8_as_unicode(window_begin);
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
      window_begin += BLI_str_utf8_size(window_begin);
      window_end += BLI_str_utf8_size(window_end);
    }
  }
}

static constexpr int unused_word = -1;

/**
 * Takes a query and tries to match it with the first characters of some words. For example, "msfv"
 * matches "Mark Sharp from Vertices". Multiple letters of the beginning of a word can be matched
 * as well. For example, "seboulo" matches "select boundary loop". The order of words is important.
 * So "bose" does not match "select boundary". However, individual words can be skipped. For
 * example, "rocc" matches "rotate edge ccw".
 *
 * \return true when the match was successful.
 * If it was successful, the used words are tagged in \a r_word_is_matched.
 */
static bool match_word_initials(StringRef query,
                                Span<StringRef> words,
                                Span<int> word_match_map,
                                MutableSpan<bool> r_word_is_matched,
                                int start = 0)
{
  if (start >= words.size()) {
    return false;
  }

  r_word_is_matched.fill(false);

  size_t query_index = 0;
  int word_index = start;
  size_t char_index = 0;

  int first_found_word_index = -1;

  while (query_index < query.size()) {
    const uint query_unicode = BLI_str_utf8_as_unicode_step(
        query.data(), query.size(), &query_index);
    while (true) {
      /* We are at the end of words, no complete match has been found yet. */
      if (word_index >= words.size()) {
        if (first_found_word_index >= 0) {
          /* Try starting to match at another word. In some cases one can still find matches this
           * way. */
          return match_word_initials(
              query, words, word_match_map, r_word_is_matched, first_found_word_index + 1);
        }
        return false;
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
        const uint32_t char_unicode = BLI_str_utf8_as_unicode_step(
            word.data(), word.size(), &char_index);
        if (query_unicode == char_unicode) {
          r_word_is_matched[word_index] = true;
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
  return true;
}

static int get_shortest_word_index_that_startswith(StringRef query,
                                                   Span<StringRef> words,
                                                   Span<int> word_match_map)
{
  int best_word_size = INT32_MAX;
  int best_word_index = -1;
  for (const int i : words.index_range()) {
    if (word_match_map[i] != unused_word) {
      continue;
    }
    StringRef word = words[i];
    if (word.startswith(query)) {
      if (word.size() < best_word_size) {
        best_word_index = i;
        best_word_size = word.size();
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
static int score_query_against_words(Span<StringRef> query_words, Span<StringRef> result_words)
{
  /* A mapping from #result_words to #query_words. It's mainly used to determine if a word has been
   * matched already to avoid matching it again. */
  Array<int, 64> word_match_map(result_words.size(), unused_word);

  /* Start with some high score, because otherwise the final score might become negative. */
  int total_match_score = 1000;

  for (const int query_word_index : query_words.index_range()) {
    const StringRef query_word = query_words[query_word_index];
    {
      /* Check if any result word begins with the query word. */
      const int word_index = get_shortest_word_index_that_startswith(
          query_word, result_words, word_match_map);
      if (word_index >= 0) {
        total_match_score += 10;
        word_match_map[word_index] = query_word_index;
        continue;
      }
    }
    {
      /* Try to match against word initials. */
      Array<bool, 64> matched_words(result_words.size());
      const bool success = match_word_initials(
          query_word, result_words, word_match_map, matched_words);
      if (success) {
        total_match_score += 3;
        for (const int i : result_words.index_range()) {
          if (matched_words[i]) {
            word_match_map[i] = query_word_index;
          }
        }
        continue;
      }
    }
    {
      /* Fuzzy match against words. */
      int error_count = 0;
      const int word_index = get_word_index_that_fuzzy_matches(
          query_word, result_words, word_match_map, &error_count);
      if (word_index >= 0) {
        total_match_score += 3 - error_count;
        word_match_map[word_index] = query_word_index;
        continue;
      }
    }

    /* Couldn't match query word with anything. */
    return -1;
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
                              Vector<StringRef, 64> &r_words)
{
  const uint32_t unicode_space = uint32_t(' ');
  const uint32_t unicode_slash = uint32_t('/');
  const uint32_t unicode_right_triangle = UI_MENU_ARROW_SEP_UNICODE;

  BLI_assert(unicode_space == BLI_str_utf8_as_unicode(" "));
  BLI_assert(unicode_slash == BLI_str_utf8_as_unicode("/"));
  BLI_assert(unicode_right_triangle == BLI_str_utf8_as_unicode(UI_MENU_ARROW_SEP));

  auto is_separator = [&](uint32_t unicode) {
    return ELEM(unicode, unicode_space, unicode_slash, unicode_right_triangle);
  };

  /* Make a copy of the string so that we can edit it. */
  StringRef str_copy = allocator.copy_string(str);
  char *mutable_copy = const_cast<char *>(str_copy.data());
  const size_t str_size_in_bytes = size_t(str.size());
  BLI_str_tolower_ascii(mutable_copy, str_size_in_bytes);

  /* Iterate over all unicode code points to split individual words. */
  bool is_in_word = false;
  size_t word_start = 0;
  size_t offset = 0;
  while (offset < str_size_in_bytes) {
    size_t size = offset;
    uint32_t unicode = BLI_str_utf8_as_unicode_step(str.data(), str.size(), &size);
    size -= offset;
    if (is_separator(unicode)) {
      if (is_in_word) {
        r_words.append(str_copy.substr(int(word_start), int(offset - word_start)));
        is_in_word = false;
      }
    }
    else {
      if (!is_in_word) {
        word_start = offset;
        is_in_word = true;
      }
    }
    offset += size;
  }
  /* If the last word is not followed by a separator, it has to be handled separately. */
  if (is_in_word) {
    r_words.append(str_copy.drop_prefix(int(word_start)));
  }
}

}  // namespace blender::string_search

struct SearchItem {
  blender::Span<blender::StringRef> normalized_words;
  int length;
  void *user_data;
  int weight;
};

struct StringSearch {
  blender::LinearAllocator<> allocator;
  blender::Vector<SearchItem> items;
};

StringSearch *BLI_string_search_new()
{
  return new StringSearch();
}

void BLI_string_search_add(StringSearch *search,
                           const char *str,
                           void *user_data,
                           const int weight)
{
  using namespace blender;
  Vector<StringRef, 64> words;
  StringRef str_ref{str};
  string_search::extract_normalized_words(str_ref, search->allocator, words);
  search->items.append({search->allocator.construct_array_copy(words.as_span()),
                        int(str_ref.size()),
                        user_data,
                        weight});
}

int BLI_string_search_query(StringSearch *search, const char *query, void ***r_data)
{
  using namespace blender;

  const StringRef query_str = query;

  LinearAllocator<> allocator;
  Vector<StringRef, 64> query_words;
  string_search::extract_normalized_words(query_str, allocator, query_words);

  /* Compute score of every result. */
  MultiValueMap<int, int> result_indices_by_score;
  for (const int result_index : search->items.index_range()) {
    const int score = string_search::score_query_against_words(
        query_words, search->items[result_index].normalized_words);
    if (score >= 0) {
      result_indices_by_score.add(score, result_index);
    }
  }

  Vector<int> found_scores;
  for (const int score : result_indices_by_score.keys()) {
    found_scores.append(score);
  }
  std::sort(found_scores.begin(), found_scores.end(), std::greater<>());

  /* Add results to output vector in correct order. First come the results with the best match
   * score. Results with the same score are in the order they have been added to the search. */
  Vector<int> sorted_result_indices;
  for (const int score : found_scores) {
    MutableSpan<int> indices = result_indices_by_score.lookup(score);
    if (score == found_scores[0] && !query_str.is_empty()) {
      /* Sort items with best score by length. Shorter items are more likely the ones you are
       * looking for. This also ensures that exact matches will be at the top, even if the query is
       * a sub-string of another item. */
      std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return search->items[a].length < search->items[b].length;
      });
      /* Prefer items with larger weights. Use `stable_sort` so that if the weights are the same,
       * the order won't be changed. */
      std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) {
        return search->items[a].weight > search->items[b].weight;
      });
    }
    sorted_result_indices.extend(indices);
  }

  void **sorted_data = static_cast<void **>(
      MEM_malloc_arrayN(size_t(sorted_result_indices.size()), sizeof(void *), AT));
  for (const int i : sorted_result_indices.index_range()) {
    const int result_index = sorted_result_indices[i];
    SearchItem &item = search->items[result_index];
    sorted_data[i] = item.user_data;
  }

  *r_data = sorted_data;

  return sorted_result_indices.size();
}

void BLI_string_search_free(StringSearch *string_search)
{
  delete string_search;
}
