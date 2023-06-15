/* SPDX-FileCopyrightText: 2002-2017 `Jason Evans <jasone@canonware.com>`. All rights reserved.
 * SPDX-FileCopyrightText: 2007-2012 Mozilla Foundation. All rights reserved.
 * SPDX-FileCopyrightText: 2009-2017 Facebook, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause */

/** \file
 * \ingroup intern_mem
 */

#ifndef __MALLOCN_INLINE_H__
#define __MALLOCN_INLINE_H__

#ifdef __cplusplus
extern "C" {
#endif

MEM_INLINE bool MEM_size_safe_multiply(size_t a, size_t b, size_t *result)
{
  /* A size_t with its high-half bits all set to 1. */
  const size_t high_bits = SIZE_MAX << (sizeof(size_t) * 8 / 2);
  *result = a * b;

  if (UNLIKELY(*result == 0)) {
    return (a == 0 || b == 0);
  }

  /*
   * We got a non-zero size, but we don't know if we overflowed to get
   * there.  To avoid having to do a divide, we'll be clever and note that
   * if both A and B can be represented in N/2 bits, then their product
   * can be represented in N bits (without the possibility of overflow).
   */
  return ((high_bits & (a | b)) == 0 || (*result / b == a));
}

#ifdef __cplusplus
}
#endif

#endif /* __MALLOCN_INLINE_H__ */
