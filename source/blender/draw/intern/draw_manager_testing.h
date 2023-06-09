/* SPDX-FileCopyrightText: 2016 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

/* Internal API only for test cases. */

#pragma once

#include "GPU_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_OPENGL_DRAW_TESTS
void DRW_draw_state_init_gtests(eGPUShaderConfig sh_cfg);
#endif

#ifdef __cplusplus
}
#endif
