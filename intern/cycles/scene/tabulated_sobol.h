/* SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types.h"

CCL_NAMESPACE_BEGIN

void tabulated_sobol_generate_4D(float4 points[], const int size, const int rng_seed);

CCL_NAMESPACE_END
