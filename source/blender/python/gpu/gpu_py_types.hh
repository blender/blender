/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "gpu_py_buffer.hh"  // IWYU pragma: export

#include "gpu_py_batch.hh"          // IWYU pragma: export
#include "gpu_py_compute.hh"        // IWYU pragma: export
#include "gpu_py_element.hh"        // IWYU pragma: export
#include "gpu_py_framebuffer.hh"    // IWYU pragma: export
#include "gpu_py_offscreen.hh"      // IWYU pragma: export
#include "gpu_py_shader.hh"         // IWYU pragma: export
#include "gpu_py_texture.hh"        // IWYU pragma: export
#include "gpu_py_uniformbuffer.hh"  // IWYU pragma: export
#include "gpu_py_vertex_buffer.hh"  // IWYU pragma: export
#include "gpu_py_vertex_format.hh"  // IWYU pragma: export

[[nodiscard]] PyObject *bpygpu_types_init();
