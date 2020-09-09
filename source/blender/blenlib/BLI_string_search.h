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
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct StringSearch StringSearch;

StringSearch *BLI_string_search_new(void);
void BLI_string_search_add(StringSearch *search, const char *str, void *user_data);
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

int damerau_levenshtein_distance(StringRef a, StringRef b);
int get_fuzzy_match_errors(StringRef query, StringRef full);
void extract_normalized_words(StringRef str,
                              LinearAllocator<> &allocator,
                              Vector<StringRef, 64> &r_words);

}  // namespace blender::string_search

#endif
