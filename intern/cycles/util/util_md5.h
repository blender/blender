/*
 * Copyright (C) 1999, 2002 Aladdin Enterprises.  All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * L. Peter Deutsch
 * ghost@aladdin.com
 */

/* MD5
 *
 * Simply MD5 hash computation, used by disk cache. Adapted from external
 * code, with minor code modifications done to remove some unused code and
 * change code style. */

#ifndef __UTIL_MD5_H__
#define __UTIL_MD5_H__

#include "util_string.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class MD5Hash {
public:
	MD5Hash();
	~MD5Hash();

	void append(const uint8_t *data, int size);
	bool append_file(const string& filepath);
	string get_hex();

protected:
	void process(const uint8_t *data);
	void finish(uint8_t digest[16]);

	uint32_t count[2]; /* message length in bits, lsw first */
	uint32_t abcd[4]; /* digest buffer */
	uint8_t buf[64]; /* accumulate block */
};

CCL_NAMESPACE_END

#endif /* __UTIL_MD5_H__ */

