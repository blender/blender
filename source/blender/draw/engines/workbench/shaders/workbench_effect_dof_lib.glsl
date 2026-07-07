/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

#pragma once

#include "gpu_shader_math_vector_lib.glsl"

#define dof_aperturesize dof_params.x
#define dof_distance dof_params.y
#define dof_invsensorsize dof_params.z

/* divide by sensor size to get the normalized size */
#define dof_calculate_coc(zdepth) \
  (dof_aperturesize * (dof_distance / zdepth - 1.0f) * dof_invsensorsize)

#define MAX_COC_SIZE 100.0f
float2 dof_encode_coc(float near, float far)
{
  return float2(near, far) / MAX_COC_SIZE;
}
float dof_decode_coc(float2 cocs)
{
  return max(cocs.x, cocs.y) * MAX_COC_SIZE;
}
float dof_decode_signed_coc(float2 cocs)
{
  return ((cocs.x > cocs.y) ? cocs.x : -cocs.y) * MAX_COC_SIZE;
}
