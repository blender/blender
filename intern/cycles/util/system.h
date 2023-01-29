/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_SYSTEM_H__
#define __UTIL_SYSTEM_H__

#include <stdint.h>
#include <stdlib.h>

#include <string>

CCL_NAMESPACE_BEGIN

/* Get width in characters of the current console output. */
int system_console_width();

std::string system_cpu_brand_string();
int system_cpu_bits();
bool system_cpu_support_sse2();
bool system_cpu_support_sse41();
bool system_cpu_support_avx2();

size_t system_physical_ram();

/* Get identifier of the currently running process. */
uint64_t system_self_process_id();

CCL_NAMESPACE_END

#endif /* __UTIL_SYSTEM_H__ */
