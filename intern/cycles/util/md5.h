/* SPDX-FileCopyrightText: 1999, 2002 Aladdin Enterprises. All rights reserved.
 *
 * SPDX-License-Identifier: Zlib
 *
 * By `L. Peter Deutsch <ghost@aladdin.com>`. */

/* MD5
 *
 * Simply MD5 hash computation, used by disk cache. Adapted from external
 * code, with minor code modifications done to remove some unused code and
 * change code style. */

#ifndef __UTIL_MD5_H__
#define __UTIL_MD5_H__

#include "util/string.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

class MD5Hash {
 public:
  MD5Hash();
  ~MD5Hash();

  void append(const uint8_t *data, int size);
  void append(const string &str);
  bool append_file(const string &filepath);
  string get_hex();

 protected:
  void process(const uint8_t *data);
  void finish(uint8_t digest[16]);

  uint32_t count[2]; /* message length in bits, LSW first. */
  uint32_t abcd[4];  /* digest buffer */
  uint8_t buf[64];   /* accumulate block */
};

string util_md5_string(const string &str);

CCL_NAMESPACE_END

#endif /* __UTIL_MD5_H__ */
