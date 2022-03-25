/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_SYSTEM_H__
#define __UTIL_SYSTEM_H__

#include "util/string.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Get width in characters of the current console output. */
int system_console_width();

string system_cpu_brand_string();
int system_cpu_bits();
bool system_cpu_support_sse2();
bool system_cpu_support_sse3();
bool system_cpu_support_sse41();
bool system_cpu_support_avx();
bool system_cpu_support_avx2();

size_t system_physical_ram();

/* Start a new process of the current application with the given arguments. */
bool system_call_self(const vector<string> &args);

/* Get identifier of the currently running process. */
uint64_t system_self_process_id();

CCL_NAMESPACE_END

#endif /* __UTIL_SYSTEM_H__ */
