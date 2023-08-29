/* SPDX-FileCopyrightText: 2018-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_MURMURHASH_H__
#define __UTIL_MURMURHASH_H__

#include "util/types.h"

CCL_NAMESPACE_BEGIN

uint32_t util_murmur_hash3(const void *key, int len, uint32_t seed);
float util_hash_to_float(uint32_t hash);

CCL_NAMESPACE_END

#endif /* __UTIL_MURMURHASH_H__ */
