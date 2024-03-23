/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "gpu_py_buffer.hh"

#include "gpu_py_batch.hh"
#include "gpu_py_compute.hh"
#include "gpu_py_element.hh"
#include "gpu_py_framebuffer.hh"
#include "gpu_py_offscreen.hh"
#include "gpu_py_shader.hh"
#include "gpu_py_texture.hh"
#include "gpu_py_uniformbuffer.hh"
#include "gpu_py_vertex_buffer.hh"
#include "gpu_py_vertex_format.hh"

PyObject *bpygpu_types_init(void);
