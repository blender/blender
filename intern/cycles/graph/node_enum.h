/*
 * Copyright 2011-2016 Blender Foundation
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

#pragma once

#include "util/util_map.h"
#include "util/util_param.h"

CCL_NAMESPACE_BEGIN

/* Enum
 *
 * Utility class for enum values. */

struct NodeEnum {
	bool empty() const { return left.empty(); }
	void insert(const char *x, int y) {
		left[ustring(x)] = y;
		right[y] = ustring(x);
	}

	bool exists(ustring x) const { return left.find(x) != left.end(); }
	bool exists(int y) const { return right.find(y) != right.end(); }

	int operator[](const char *x) const { return left.find(ustring(x))->second; }
	int operator[](ustring x) const { return left.find(x)->second; }
	ustring operator[](int y) const { return right.find(y)->second; }

private:
	unordered_map<ustring, int, ustringHash> left;
	unordered_map<int, ustring> right;
};

CCL_NAMESPACE_END
