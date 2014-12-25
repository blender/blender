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

#include "util_vector.h"

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
void string_split(vector<string>& tokens, const string& str, const string& separators = "\t ");
bool string_endswith(const string& s, const char *end);
string string_strip(const string& s);

CCL_NAMESPACE_END

#endif /* __UTIL_STRING_H__ */

