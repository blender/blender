/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "gpu_py_buffer.h"

#include "gpu_py_batch.h"
#include "gpu_py_element.h"
#include "gpu_py_framebuffer.h"
#include "gpu_py_offscreen.h"
#include "gpu_py_shader.h"
#include "gpu_py_texture.h"
#include "gpu_py_uniformbuffer.h"
#include "gpu_py_vertex_buffer.h"
#include "gpu_py_vertex_format.h"

#ifdef __cplusplus
extern "C" {
#endif

PyObject *bpygpu_types_init(void);

#ifdef __cplusplus
}
#endif
