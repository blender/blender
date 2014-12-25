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

#include <stdarg.h>
#include <stdio.h>

#include <boost/algorithm/string.hpp>

#include "util_foreach.h"
#include "util_string.h"

#ifdef _WIN32
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#endif

CCL_NAMESPACE_BEGIN

string string_printf(const char *format, ...)
{
	vector<char> str(128, 0);

	while(1) {
		va_list args;
		int result;

		va_start(args, format);
		result = vsnprintf(&str[0], str.size(), format, args);
		va_end(args);

		if(result == -1) {
			/* not enough space or formatting error */
			if(str.size() > 65536) {
				assert(0);
				return string("");
			}

			str.resize(str.size()*2, 0);
			continue;
		}
		else if(result >= (int)str.size()) {
			/* not enough space */
			str.resize(result + 1, 0);
			continue;
		}

		return string(&str[0]);
	}
}

bool string_iequals(const string& a, const string& b)
{
	if(a.size() == b.size()) {
		for(size_t i = 0; i < a.size(); i++)
			if(toupper(a[i]) != toupper(b[i]))
				return false;

		return true;
	}

	return false;
}

void string_split(vector<string>& tokens, const string& str, const string& separators)
{
	vector<string> split;

	boost::split(split, str, boost::is_any_of(separators), boost::token_compress_on);

	foreach(const string& token, split)
		if(token != "")
			tokens.push_back(token);
}

bool string_endswith(const string& s, const char *end)
{
	size_t len = strlen(end);

	if(len > s.size())
		return 0;
	else
		return strncmp(s.c_str() + s.size() - len, end, len) == 0;
}

string string_strip(const string& s)
{
	string result = s;
	result.erase(0, result.find_first_not_of(' '));
	result.erase(result.find_last_not_of(' ') + 1);
	return result;

}

CCL_NAMESPACE_END

