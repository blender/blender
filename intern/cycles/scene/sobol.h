/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __SOBOL_H__
#define __SOBOL_H__

#include "util/types.h"

CCL_NAMESPACE_BEGIN

#define SOBOL_BITS 32
#define SOBOL_MAX_DIMENSIONS 21201

void sobol_generate_direction_vectors(uint vectors[][SOBOL_BITS], int dimensions);

CCL_NAMESPACE_END

#endif /* __SOBOL_H__ */
