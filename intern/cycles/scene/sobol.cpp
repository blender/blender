/*
 * Sobol sequence direction vectors.
 *
 * This file contains code to create direction vectors for generating sobol
 * sequences in high dimensions. It is adapted from code on this webpage:
 *
 * http://web.maths.unsw.edu.au/~fkuo/sobol/
 *
 * From these papers:
 *
 * S. Joe and F. Y. Kuo, Remark on Algorithm 659: Implementing Sobol's quasirandom
 * sequence generator, ACM Trans. Math. Softw. 29, 49-57 (2003)
 *
 * S. Joe and F. Y. Kuo, Constructing Sobol sequences with better two-dimensional
 * projections, SIAM J. Sci. Comput. 30, 2635-2654 (2008)
 */

/* Copyright (c) 2008, Frances Y. Kuo and Stephen Joe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the names of the copyright holders nor the names of the
 *       University of New South Wales and the University of Waikato
 *       and its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "util/types.h"

#include "scene/sobol.h"

CCL_NAMESPACE_BEGIN

#include "scene/sobol.tables"

void sobol_generate_direction_vectors(uint vectors[][SOBOL_BITS], int dimensions)
{
  assert(dimensions <= SOBOL_MAX_DIMENSIONS);

  const uint L = SOBOL_BITS;

  /* first dimension is exception */
  uint *v = vectors[0];

  for (uint i = 0; i < L; i++)
    v[i] = 1 << (31 - i);  // all m's = 1

  for (int dim = 1; dim < dimensions; dim++) {
    const SobolDirectionNumbers *numbers = &SOBOL_NUMBERS[dim - 1];
    const uint s = numbers->s;
    const uint a = numbers->a;
    const uint *m = numbers->m;

    v = vectors[dim];

    if (L <= s) {
      for (uint i = 0; i < L; i++)
        v[i] = m[i] << (31 - i);
    }
    else {
      for (uint i = 0; i < s; i++)
        v[i] = m[i] << (31 - i);

      for (uint i = s; i < L; i++) {
        v[i] = v[i - s] ^ (v[i - s] >> s);

        for (uint k = 1; k < s; k++)
          v[i] ^= (((a >> (s - 1 - k)) & 1) * v[i - k]);
      }
    }
  }
}

CCL_NAMESPACE_END
