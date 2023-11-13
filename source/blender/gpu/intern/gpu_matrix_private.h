/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GPUMatrixState *GPU_matrix_state_create(void);
void GPU_matrix_state_discard(struct GPUMatrixState *state);

#ifdef __cplusplus
}
#endif
