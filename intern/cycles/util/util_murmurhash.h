/*
 * Copyright 2018 Blender Foundation
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


#ifndef __UTIL_MURMURHASH_H__
#define __UTIL_MURMURHASH_H__

#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

uint32_t util_murmur_hash3(const void *key, int len, uint32_t seed);
float util_hash_to_float(uint32_t hash);

CCL_NAMESPACE_END

#endif /* __UTIL_MURMURHASH_H__ */
