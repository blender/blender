/* SPDX-FileCopyrightText: 2018-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdint>

CCL_NAMESPACE_BEGIN

uint32_t util_murmur_hash3(const void *key, const int len, const uint32_t seed);
float util_hash_to_float(const uint32_t hash);

CCL_NAMESPACE_END
