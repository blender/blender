/*
 * Copyright 2011, Blender Foundation.
 *
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

CCL_NAMESPACE_END

#endif /* __UTIL_STRING_H__ */

