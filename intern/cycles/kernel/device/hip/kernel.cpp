/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* HIP kernel entry points */

#ifdef __HIP_DEVICE_COMPILE__

#  include "kernel/device/hip/compat.h"
#  include "kernel/device/hip/config.h"
#  include "kernel/device/hip/globals.h"

#  include "kernel/device/gpu/image.h"
#  include "kernel/device/gpu/kernel.h"

#endif
