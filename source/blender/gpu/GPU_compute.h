/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"

#include "GPU_shader.h"
#include "GPU_storage_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void GPU_compute_dispatch(GPUShader *shader,
                          uint groups_x_len,
                          uint groups_y_len,
                          uint groups_z_len);

void GPU_compute_dispatch_indirect(GPUShader *shader, GPUStorageBuf *indirect_buf_);

#ifdef __cplusplus
}
#endif
