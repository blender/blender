/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2008, Frances Y. Kuo and Stephen Joe
 * All rights reserved. */

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
