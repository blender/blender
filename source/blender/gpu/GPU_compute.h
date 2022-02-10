/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"

#include "GPU_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

void GPU_compute_dispatch(GPUShader *shader,
                          uint groups_x_len,
                          uint groups_y_len,
                          uint groups_z_len);

#ifdef __cplusplus
}
#endif
