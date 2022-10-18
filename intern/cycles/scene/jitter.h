/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2019-2022 Blender Foundation */

#ifndef __JITTER_H__
#define __JITTER_H__

#include "util/types.h"

CCL_NAMESPACE_BEGIN

void progressive_multi_jitter_02_generate_2D(float2 points[], int size, int rng_seed);

CCL_NAMESPACE_END

#endif /* __JITTER_H__ */
