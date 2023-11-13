/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#define WITH_OSL

// clang-format off
#include "kernel/device/optix/compat.h"
#include "kernel/device/optix/globals.h"

#include "kernel/device/gpu/image.h"  /* Texture lookup uses normal CUDA intrinsics. */

#include "kernel/osl/services_gpu.h"
// clang-format on

extern "C" __device__ void __direct_callable__dummy_services()
{
}
