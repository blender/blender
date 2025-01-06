/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Header that can be included wherever KernelGlobals is used in the kernel,
 * to make clangd happy. For GPU devices it's defined beforehand. */

#ifndef __KERNEL_GPU__
#  include "kernel/device/cpu/compat.h"   // IWYU pragma: export
#  include "kernel/device/cpu/globals.h"  // IWYU pragma: export
#endif
