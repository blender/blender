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

#pragma once

#ifdef __KERNEL_GPU__
#  define __ATOMIC_PASS_WRITE__
#endif

CCL_NAMESPACE_BEGIN

ccl_device_inline void kernel_write_pass_float(ccl_global float *ccl_restrict buffer, float value)
{
#ifdef __ATOMIC_PASS_WRITE__
  atomic_add_and_fetch_float(buffer, value);
#else
  *buffer += value;
#endif
}

ccl_device_inline void kernel_write_pass_float3(ccl_global float *ccl_restrict buffer,
                                                float3 value)
{
#ifdef __ATOMIC_PASS_WRITE__
  ccl_global float *buf_x = buffer + 0;
  ccl_global float *buf_y = buffer + 1;
  ccl_global float *buf_z = buffer + 2;

  atomic_add_and_fetch_float(buf_x, value.x);
  atomic_add_and_fetch_float(buf_y, value.y);
  atomic_add_and_fetch_float(buf_z, value.z);
#else
  buffer[0] += value.x;
  buffer[1] += value.y;
  buffer[2] += value.z;
#endif
}

ccl_device_inline void kernel_write_pass_float4(ccl_global float *ccl_restrict buffer,
                                                float4 value)
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
  buffer[0] += value.x;
  buffer[1] += value.y;
  buffer[2] += value.z;
  buffer[3] += value.w;
#endif
}

ccl_device_inline float kernel_read_pass_float(ccl_global float *ccl_restrict buffer)
{
  return *buffer;
}

ccl_device_inline float3 kernel_read_pass_float3(ccl_global float *ccl_restrict buffer)
{
  return make_float3(buffer[0], buffer[1], buffer[2]);
}

ccl_device_inline float4 kernel_read_pass_float4(ccl_global float *ccl_restrict buffer)
{
  return make_float4(buffer[0], buffer[1], buffer[2], buffer[3]);
}

CCL_NAMESPACE_END
