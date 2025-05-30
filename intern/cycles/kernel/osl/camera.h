/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include "kernel/globals.h"

#include "kernel/osl/types.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void cameradata_to_shaderglobals(const packed_float3 sensor,
                                                   const packed_float3 dSdx,
                                                   const packed_float3 dSdy,
                                                   const float2 rand_lens,
                                                   ccl_private ShaderGlobals *globals)
{
  memset(globals, 0, sizeof(ShaderGlobals));

  globals->P = sensor;
  globals->dPdx = dSdx;
  globals->dPdy = dSdy;
  globals->N = make_float3(rand_lens);
}

#ifndef __KERNEL_GPU__

packed_float3 osl_eval_camera(KernelGlobals kg,
                              const packed_float3 sensor,
                              const packed_float3 dSdx,
                              const packed_float3 dSdy,
                              const float2 rand_lens,
                              packed_float3 &P,
                              packed_float3 &dPdx,
                              packed_float3 &dPdy,
                              packed_float3 &D,
                              packed_float3 &dDdx,
                              packed_float3 &dDdy);

#else

ccl_device_inline packed_float3 osl_eval_camera(KernelGlobals kg,
                                                const packed_float3 sensor,
                                                const packed_float3 dSdx,
                                                const packed_float3 dSdy,
                                                const float2 rand_lens,
                                                packed_float3 &P,
                                                packed_float3 &dPdx,
                                                packed_float3 &dPdy,
                                                packed_float3 &D,
                                                packed_float3 &dDdx,
                                                packed_float3 &dDdy)
{
  ShaderGlobals globals;
  cameradata_to_shaderglobals(sensor, dSdx, dSdy, rand_lens, &globals);

  float output[21] = {0.0f};
#  ifdef __KERNEL_OPTIX__
  optixDirectCall<void>(/*NUM_CALLABLE_PROGRAM_GROUPS*/ 2,
                        /*shaderglobals_ptr*/ &globals,
                        /*groupdata_ptr*/ (void *)nullptr,
                        /*userdata_base_ptr*/ (void *)nullptr,
                        /*output_base_ptr*/ (void *)output,
                        /*shadeindex*/ 0,
                        /*interactive_params_ptr*/ (void *)nullptr);
#  endif

  P = make_float3(output[0], output[1], output[2]);
  dPdx = make_float3(output[3], output[4], output[5]);
  dPdy = make_float3(output[6], output[7], output[8]);
  D = make_float3(output[9], output[10], output[11]);
  dDdx = make_float3(output[12], output[13], output[14]);
  dDdy = make_float3(output[15], output[16], output[17]);
  return make_float3(output[18], output[19], output[20]);
}

#endif

CCL_NAMESPACE_END
