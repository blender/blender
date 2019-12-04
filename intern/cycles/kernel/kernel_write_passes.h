/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(__SPLIT_KERNEL__) || defined(__KERNEL_CUDA__)
#  define __ATOMIC_PASS_WRITE__
#endif

CCL_NAMESPACE_BEGIN

ccl_device_inline void kernel_write_pass_float(ccl_global float *buffer, float value)
{
  ccl_global float *buf = buffer;
#ifdef __ATOMIC_PASS_WRITE__
  atomic_add_and_fetch_float(buf, value);
#else
  *buf += value;
#endif
}

ccl_device_inline void kernel_write_pass_float3(ccl_global float *buffer, float3 value)
{
#ifdef __ATOMIC_PASS_WRITE__
  ccl_global float *buf_x = buffer + 0;
  ccl_global float *buf_y = buffer + 1;
  ccl_global float *buf_z = buffer + 2;

  atomic_add_and_fetch_float(buf_x, value.x);
  atomic_add_and_fetch_float(buf_y, value.y);
  atomic_add_and_fetch_float(buf_z, value.z);
#else
  ccl_global float3 *buf = (ccl_global float3 *)buffer;
  *buf += value;
#endif
}

ccl_device_inline void kernel_write_pass_float4(ccl_global float *buffer, float4 value)
{
#ifdef __ATOMIC_PASS_WRITE__
  ccl_global float *buf_x = buffer + 0;
  ccl_global float *buf_y = buffer + 1;
  ccl_global float *buf_z = buffer + 2;
  ccl_global float *buf_w = buffer + 3;

  atomic_add_and_fetch_float(buf_x, value.x);
  atomic_add_and_fetch_float(buf_y, value.y);
  atomic_add_and_fetch_float(buf_z, value.z);
  atomic_add_and_fetch_float(buf_w, value.w);
#else
  ccl_global float4 *buf = (ccl_global float4 *)buffer;
  *buf += value;
#endif
}

#ifdef __DENOISING_FEATURES__
ccl_device_inline void kernel_write_pass_float_variance(ccl_global float *buffer, float value)
{
  kernel_write_pass_float(buffer, value);

  /* The online one-pass variance update that's used for the megakernel can't easily be implemented
   * with atomics, so for the split kernel the E[x^2] - 1/N * (E[x])^2 fallback is used. */
  kernel_write_pass_float(buffer + 1, value * value);
}

#  ifdef __ATOMIC_PASS_WRITE__
#    define kernel_write_pass_float3_unaligned kernel_write_pass_float3
#  else
ccl_device_inline void kernel_write_pass_float3_unaligned(ccl_global float *buffer, float3 value)
{
  buffer[0] += value.x;
  buffer[1] += value.y;
  buffer[2] += value.z;
}
#  endif

ccl_device_inline void kernel_write_pass_float3_variance(ccl_global float *buffer, float3 value)
{
  kernel_write_pass_float3_unaligned(buffer, value);
  kernel_write_pass_float3_unaligned(buffer + 3, value * value);
}
#endif /* __DENOISING_FEATURES__ */

CCL_NAMESPACE_END
