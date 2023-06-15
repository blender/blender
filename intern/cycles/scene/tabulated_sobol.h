/* SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __TABULATED_SOBOL_H__
#define __TABULATED_SOBOL_H__

#include "util/types.h"

CCL_NAMESPACE_BEGIN

void tabulated_sobol_generate_4D(float4 points[], int size, int rng_seed);

CCL_NAMESPACE_END

#endif /* __TABULATED_SOBOL_H__ */
