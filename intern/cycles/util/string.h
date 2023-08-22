/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <string.h>
#include <string>

/* Use string view implementation from OIIO.
 * Ideally, need to switch to `std::string_view`, but this first requires getting rid of using
 * namespace OIIO as it causes symbol collision. */
#include <OpenImageIO/string_view.h>

#include "util/vector.h"

CCL_NAMESPACE_BEGIN

using std::string;
using std::to_string;

using OIIO::string_view;

#ifdef __GNUC__
#  define PRINTF_ATTRIBUTE __attribute__((format(printf, 1, 2)))
#else
#  define PRINTF_ATTRIBUTE
#endif

string string_printf(const char *format, ...) PRINTF_ATTRIBUTE;

bool string_iequals(const string &a, const string &b);
void string_split(vector<string> &tokens,
                  const string &str,
                  const string &separators = "\t ",
                  bool skip_empty_tokens = true);
void string_replace(string &haystack, const string &needle, const string &other);
void string_replace_same_length(string &haystack, const string &needle, const string &other);
bool string_startswith(string_view s, string_view start);
bool string_endswith(string_view s, string_view end);
string string_strip(const string &s);
string string_remove_trademark(const string &s);
string string_from_bool(const bool var);
string to_string(const char *str);
string to_string(const float4 &v);
string string_to_lower(const string &s);

/* Wide char strings are only used on Windows to deal with non-ASCII
 * characters in file names and such. No reason to use such strings
 * for something else at this moment.
 *
 * Please note that strings are expected to be in UTF-8 codepage, and
 * if ANSI is needed then explicit conversion required.
 */
#ifdef _WIN32
using std::wstring;
wstring string_to_wstring(const string &path);
string string_from_wstring(const wstring &path);
string string_to_ansi(const string &str);
#endif

/* Make a string from a size in bytes in human readable form. */
string string_human_readable_size(size_t size);
/* Make a string from a unit-less quantity in human readable form. */
string string_human_readable_number(size_t num);

CCL_NAMESPACE_END
