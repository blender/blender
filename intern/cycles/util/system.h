/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdint>
#include <cstdlib>

#include <string>

CCL_NAMESPACE_BEGIN

/* Get width in characters of the current console output. */
int system_console_width();

std::string system_cpu_brand_string();
int system_cpu_bits();
bool system_cpu_support_sse42();
bool system_cpu_support_avx2();

size_t system_physical_ram();

/* Get identifier of the currently running process. */
uint64_t system_self_process_id();

size_t system_max_open_files();

CCL_NAMESPACE_END
