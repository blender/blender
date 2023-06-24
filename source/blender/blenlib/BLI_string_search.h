/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct StringSearch StringSearch;

StringSearch *BLI_string_search_new(void);
/**
 * Add a new possible result to the search.
 * The caller keeps ownership of all parameters.
 *
 * \param weight: Can be used to customize the order when multiple items have the same match score.
 */
void BLI_string_search_add(StringSearch *search, const char *str, void *user_data, int weight);

/**
 * Filter and sort all previously added search items.
 * Returns an array containing the filtered user data.
 * The caller has to free the returned array.
 */
int BLI_string_search_query(StringSearch *search, const char *query, void ***r_data);
void BLI_string_search_free(StringSearch *search);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BLI_linear_allocator.hh"
#  include "BLI_span.hh"
#  include "BLI_string_ref.hh"
#  include "BLI_vector.hh"

namespace blender::string_search {

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
                              Vector<StringRef, 64> &r_words);

}  // namespace blender::string_search

#endif
