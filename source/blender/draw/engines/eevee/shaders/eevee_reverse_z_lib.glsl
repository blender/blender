/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Reverse-Z utils functions.
 *
 * We only modify the depth buffer content so we need to change the vertex Z position and modify
 * every depth buffer read. This keeps the projection matrices the same as the rest of blender.
 */
#include "gpu_glsl_cpp_stubs.hh"

namespace reverse_z {

/* Needs to be called at the end of the vertex entry point function. */
float4 transform(float4 hs_position)
{
#ifdef GPU_ARB_clip_control
  /* Remapping from -1..1 to 1..-1. The scaling to 0..1 is handled by the backend. */
  hs_position.z = -hs_position.z;
#endif
  return hs_position;
}

/* NOTE: Cannot put the ifdef inside the template because of template as macro. */
#ifdef GPU_ARB_clip_control
/* Needs to be called for every depth buffer read, but not for the HiZ.
 * The HiZ buffer is already reversed back before downsample. */
template<typename T> T read(T depth_buffer_value)
{
  /* Remapping from 0..1 to 1..0. The scaling to 0..1 is handled as normal by drw_screen_to_ndc. */
  return 1.0f - depth_buffer_value;
}
template float read<float>(float);
template float4 read<float4>(float4);
#else
template<typename T> T read(T depth_buffer_value)
{
  /* Passthrough. */
  return depth_buffer_value;
}
template float read<float>(float);
template float4 read<float4>(float4);
#endif

}  // namespace reverse_z
