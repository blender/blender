/*
 * Copyright 2014 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_ATOMIC_H__
#define __UTIL_ATOMIC_H__

/* Using atomic ops header from Blender. */
#include "atomic_ops.h"

ATOMIC_INLINE void atomic_update_max_z(size_t *maximum_value, size_t value)
{
	size_t prev_value = *maximum_value;
	while (prev_value < value) {
		if (atomic_cas_z(maximum_value, prev_value, value) != prev_value) {
			break;
		}
	}
}

#endif /* __UTIL_ATOMIC_H__ */
