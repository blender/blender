/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "gpu_shader_index_range_lib.glsl"

/**
 * See `OffsetIndices` C++ definition for formal definition.
 *
 * OffsetIndices cannot be implemented on GPU because of the lack of operator overloading and
 * buffer reference in GLSL. So we simply interpret a given integer buffer as a `OffsetIndices`
 * buffer and load a specific item as a range.
 */
namespace offset_indices {

#ifdef GLSL_CPP_STUBS
/* Equivalent of `IndexRange OffsetIndices<int>operator[]`.
 * Implementation for C++ compilation. */
inline static IndexRange load_range_from_buffer(const int (&buf)[], int i)
{
  return IndexRange::from_begin_end(buf[i], buf[i + 1]);
}
#endif

}  // namespace offset_indices

/* Shader implementation because of missing buffer reference as argument in GLSL. */
#define offset_indices_load_range_from_buffer(buf_, i_) \
  IndexRange::from_begin_end(buf_[i_], buf_[i_ + 1])
