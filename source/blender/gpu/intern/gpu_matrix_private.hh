/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

struct GPUMatrixState *GPU_matrix_state_create();
void GPU_matrix_state_discard(struct GPUMatrixState *state);
