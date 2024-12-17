/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

#include "gpu_shader_math_vector_lib.glsl"

#define dof_aperturesize dofParams.x
#define dof_distance dofParams.y
#define dof_invsensorsize dofParams.z

/* divide by sensor size to get the normalized size */
#define dof_calculate_coc(zdepth) \
  (dof_aperturesize * (dof_distance / zdepth - 1.0) * dof_invsensorsize)

#define dof_linear_depth(z) \
  ((drw_view.winmat[3][3] == 0.0) ? \
       (nearFar.x * nearFar.y) / (z * (nearFar.x - nearFar.y) + nearFar.y) : \
       (z * 2.0 - 1.0) * nearFar.y)

#define MAX_COC_SIZE 100.0
vec2 dof_encode_coc(float near, float far)
{
  return vec2(near, far) / MAX_COC_SIZE;
}
float dof_decode_coc(vec2 cocs)
{
  return max(cocs.x, cocs.y) * MAX_COC_SIZE;
}
float dof_decode_signed_coc(vec2 cocs)
{
  return ((cocs.x > cocs.y) ? cocs.x : -cocs.y) * MAX_COC_SIZE;
}
