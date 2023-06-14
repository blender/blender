/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef __HIP_DEVICE_COMPILE__

#  include "kernel/device/hip/compat.h"
#  include "kernel/device/hip/config.h"

#  include <hiprt/hiprt_device.h>

#  include "kernel/device/hiprt/globals.h"

#  include "kernel/device/gpu/image.h"
#  include "kernel/device/gpu/kernel.h"

#endif
