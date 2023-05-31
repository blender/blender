/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * General looping helpers, use `BLI_FOREACH` prefix.
 */

/**
 * Even value distribution.
 *
 * \a src must be larger than \a dst,
 * \a dst defines the number of iterations, their values are evenly spaced.
 *
 * The following pairs represent (src, dst) arguments and the values they loop over.
 * <pre>
 * (19, 4) ->    [2, 7, 11. 16]
 * (100, 5) ->   [9, 29, 49, 69, 89]
 * (100, 3) ->   [16, 49, 83]
 * (100, 100) -> [0..99]
 * </pre>
 * \note this is mainly useful for numbers that might not divide evenly into each other.
 */
#define BLI_FOREACH_SPARSE_RANGE(src, dst, i) \
  for (int _src = (src), _src2 = _src * 2, _dst2 = (dst)*2, _error = _dst2 - _src, i = 0, _delta; \
       ((void)(_delta = divide_floor_i(_error, _dst2)), (void)(i -= _delta), (i < _src)); \
       _error -= (_delta * _dst2) + _src2)
