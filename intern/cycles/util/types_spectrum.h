/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_TYPES_SPECTRUM_H__
#define __UTIL_TYPES_SPECTRUM_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#define SPECTRUM_CHANNELS 3
#define SPECTRUM_DATA_TYPE float3
#define PACKED_SPECTRUM_DATA_TYPE packed_float3

using Spectrum = SPECTRUM_DATA_TYPE;
using PackedSpectrum = PACKED_SPECTRUM_DATA_TYPE;

#define make_spectrum(f) CONCAT(make_, SPECTRUM_DATA_TYPE(f))
#define load_spectrum(f) CONCAT(load_, SPECTRUM_DATA_TYPE(f))
#define store_spectrum(s, f) CONCAT(store_, SPECTRUM_DATA_TYPE((s), (f)))

#define zero_spectrum CONCAT(zero_, SPECTRUM_DATA_TYPE)
#define one_spectrum CONCAT(one_, SPECTRUM_DATA_TYPE)

#define FOREACH_SPECTRUM_CHANNEL(counter) \
  for (int counter = 0; counter < SPECTRUM_CHANNELS; counter++)

#define GET_SPECTRUM_CHANNEL(v, i) (((ccl_private float *)(&(v)))[i])

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_SPECTRUM_H__ */
