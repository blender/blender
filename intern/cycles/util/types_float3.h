/*
 * Copyright 2011-2017 Blender Foundation
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

#ifndef __UTIL_TYPES_FLOAT3_H__
#define __UTIL_TYPES_FLOAT3_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
struct ccl_try_align(16) float3
{
#  ifdef __KERNEL_SSE__
  union {
    __m128 m128;
    struct {
      float x, y, z, w;
    };
  };

  __forceinline float3();
  __forceinline float3(const float3 &a);
  __forceinline explicit float3(const __m128 &a);

  __forceinline operator const __m128 &() const;
  __forceinline operator __m128 &();

  __forceinline float3 &operator=(const float3 &a);
#  else  /* __KERNEL_SSE__ */
  float x, y, z, w;
#  endif /* __KERNEL_SSE__ */

  __forceinline float operator[](int i) const;
  __forceinline float &operator[](int i);
};

ccl_device_inline float3 make_float3(float f);
ccl_device_inline float3 make_float3(float x, float y, float z);
ccl_device_inline void print_float3(const char *label, const float3 &a);
#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_FLOAT3_H__ */
