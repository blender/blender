/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_STRING_H__
#define __UTIL_STRING_H__

#include <string.h>
#include <string>
#include <sstream>

#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

using std::string;
using std::stringstream;
using std::ostringstream;
using std::istringstream;

#ifdef __GNUC__
#define PRINTF_ATTRIBUTE __attribute__((format(printf, 1, 2)))
#else
#define PRINTF_ATTRIBUTE
#endif

string string_printf(const char *format, ...) PRINTF_ATTRIBUTE;

bool string_iequals(const string& a, const string& b);
void string_split(vector<string>& tokens,
                  const string& str,
                  const string& separators = "\t ",
                  bool skip_empty_tokens = true);
void string_replace(string& haystack, const string& needle, const string& other);
bool string_startswith(const string& s, const char *start);
bool string_endswith(const string& s, const char *end);
string string_strip(const string& s);
string string_remove_trademark(const string& s);
string string_from_bool(const bool var);

/* Wide char strings are only used on Windows to deal with non-ascii
 * characters in file names and such. No reason to use such strings
 * for something else at this moment.
 *
 * Please note that strings are expected to be in UTF-8 codepage, and
 * if ANSI is needed then explicit conversion required.
 *
 */
#ifdef _WIN32
using std::wstring;
wstring string_to_wstring(const string& path);
string string_from_wstring(const wstring& path);
string string_to_ansi(const string& str);
#endif

/* Make a string from a size in bytes in human readable form */
string string_human_readable_size(size_t size);
/* Make a string from a unitless quantity in human readable form */
string string_human_readable_number(size_t num);

CCL_NAMESPACE_END

#endif /* __UTIL_STRING_H__ */
