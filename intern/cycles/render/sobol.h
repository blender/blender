/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __SOBOL_H__
#define __SOBOL_H__

#include "util_types.h"

CCL_NAMESPACE_BEGIN

#define SOBOL_BITS 32
#define SOBOL_MAX_DIMENSIONS 21201

void sobol_generate_direction_vectors(uint vectors[][SOBOL_BITS], int dimensions);

CCL_NAMESPACE_END

#endif /* __SOBOL_H__ */

