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
 * limitations under the License
 */

#ifndef __UTIL_HASH_H__
#define __UTIL_HASH_H__

#include "util_types.h"

CCL_NAMESPACE_BEGIN

static inline uint hash_int_2d(uint kx, uint ky)
{
	#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

	uint a, b, c;

	a = b = c = 0xdeadbeef + (2 << 2) + 13;
	a += kx;
	b += ky;

	c ^= b; c -= rot(b,14);
	a ^= c; a -= rot(c,11);
	b ^= a; b -= rot(a,25);
	c ^= b; c -= rot(b,16);
	a ^= c; a -= rot(c,4);
	b ^= a; b -= rot(a,14);
	c ^= b; c -= rot(b,24);

	return c;

	#undef rot
}

static inline uint hash_int(uint k)
{
	return hash_int_2d(k, 0);
}

static inline uint hash_string(const char *str)
{
	uint i = 0, c;

	while ((c = *str++))
		i = i * 37 + c;

	return i;
}

CCL_NAMESPACE_END

#endif /* __UTIL_HASH_H__ */

