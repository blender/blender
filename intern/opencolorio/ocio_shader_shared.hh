/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.h"
#endif

struct OCIO_GPUCurveMappingParameters {
  /* Curve mapping parameters
   *
   * See documentation for OCIO_CurveMappingSettings to get fields descriptions.
   * (this ones pretty much copies stuff from C structure.)
   */
  float4 mintable;
  float4 range;
  float4 ext_in_x;
  float4 ext_in_y;
  float4 ext_out_x;
  float4 ext_out_y;
  float4 first_x;
  float4 first_y;
  float4 last_x;
  float4 last_y;
  float4 black;
  float4 bwmul;
  int lut_size;
  int use_extend_extrapolate;
  int _pad0;
  int _pad1;
};

struct OCIO_GPUParameters {
  float dither;
  float scale;
  float exponent;
  bool1 use_predivide;
  bool1 use_overlay;
  bool1 use_hdr;
  int _pad0;
  int _pad1;
};
