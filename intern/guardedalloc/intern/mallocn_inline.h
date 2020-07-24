/*
 * Adapted from jemalloc, to protect against buffer overflow vulnerabilities.
 *
 * Copyright (C) 2002-2017 Jason Evans <jasone@canonware.com>.
 * All rights reserved.
 * Copyright (C) 2007-2012 Mozilla Foundation.  All rights reserved.
 * Copyright (C) 2009-2017 Facebook, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice(s),
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice(s),
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file
 * \ingroup MEM
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
