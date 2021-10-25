/*
 * Copyright 2011-2015 Blender Foundation
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

#ifndef __UTIL_ALIGNED_MALLOC_H__
#define __UTIL_ALIGNED_MALLOC_H__

#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* Allocate block of size bytes at least aligned to a given value. */
void *util_aligned_malloc(size_t size, int alignment);

/* Free memory allocated by util_aligned_malloc. */
void util_aligned_free(void *ptr);

CCL_NAMESPACE_END

#endif  /* __UTIL_ALIGNED_MALLOC_H__ */
