/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_float3.h"

CCL_NAMESPACE_BEGIN

#define SPECTRUM_CHANNELS 3

using Spectrum = float3;
using PackedSpectrum = packed_float3;

#define make_spectrum(f) make_float3(f)
#define load_spectrum(f) load_float3(f)
#define store_spectrum(s, f) store_float3(f)

#define zero_spectrum zero_float3
#define one_spectrum one_float3

#define FOREACH_SPECTRUM_CHANNEL(counter) \
  for (int counter = 0; counter < SPECTRUM_CHANNELS; counter++)

#define GET_SPECTRUM_CHANNEL(v, i) (((ccl_private float *)(&(v)))[i])

CCL_NAMESPACE_END
