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

#ifndef __UTIL_TYPES_UINT2_IMPL_H__
#define __UTIL_TYPES_UINT2_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
__forceinline uint uint2::operator[](uint i) const
{
  util_assert(i < 2);
  return *(&x + i);
}

__forceinline uint &uint2::operator[](uint i)
{
  util_assert(i < 2);
  return *(&x + i);
}

ccl_device_inline uint2 make_uint2(uint x, uint y)
{
  uint2 a = {x, y};
  return a;
}
#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_UINT2_IMPL_H__ */
