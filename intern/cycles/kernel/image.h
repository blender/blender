/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Header that can be included wherever image textures are used in the kernel,
 * to make clangd happy. For GPU devices it's defined beforehand. */

#ifndef __KERNEL_GPU__
#  include "kernel/device/cpu/image.h"  // IWYU pragma: export
#endif
