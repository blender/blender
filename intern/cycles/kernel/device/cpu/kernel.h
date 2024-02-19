/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* CPU Kernel Interface */

#include "util/half.h"
#include "util/types.h"

#include "kernel/types.h"

CCL_NAMESPACE_BEGIN

#define KERNEL_NAME_JOIN(x, y, z) x##_##y##_##z
#define KERNEL_NAME_EVAL(arch, name) KERNEL_NAME_JOIN(kernel, arch, name)
#define KERNEL_FUNCTION_FULL_NAME(name) KERNEL_NAME_EVAL(KERNEL_ARCH, name)

struct IntegratorStateCPU;
struct KernelGlobalsCPU;
struct KernelData;

KernelGlobalsCPU *kernel_globals_create();
void kernel_globals_free(KernelGlobalsCPU *kg);

void *kernel_osl_memory(const KernelGlobalsCPU *kg);
bool kernel_osl_use(const KernelGlobalsCPU *kg);

void kernel_const_copy(KernelGlobalsCPU *kg, const char *name, void *host, size_t size);
void kernel_global_memory_copy(KernelGlobalsCPU *kg, const char *name, void *mem, size_t size);

#define KERNEL_ARCH cpu
#include "kernel/device/cpu/kernel_arch.h"

#define KERNEL_ARCH cpu_sse2
#include "kernel/device/cpu/kernel_arch.h"

#define KERNEL_ARCH cpu_sse42
#include "kernel/device/cpu/kernel_arch.h"

#define KERNEL_ARCH cpu_avx2
#include "kernel/device/cpu/kernel_arch.h"

CCL_NAMESPACE_END
