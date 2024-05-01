/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

/* Internal API only for test cases. */

#pragma once

#include "GPU_shader.hh"

#ifdef WITH_GPU_DRAW_TESTS
void DRW_draw_state_init_gtests(eGPUShaderConfig sh_cfg);
#endif
