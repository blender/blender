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

#ifndef __UTIL_TYPES_VECTOR3_IMPL_H__
#define __UTIL_TYPES_VECTOR3_IMPL_H__

#ifndef __UTIL_TYPES_H__
#  error "Do not include this file directly, include util/types.h instead."
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
template<typename T> ccl_always_inline vector3<T>::vector3()
{
}

template<typename T> ccl_always_inline vector3<T>::vector3(const T &a) : x(a), y(a), z(a)
{
}

template<typename T>
ccl_always_inline vector3<T>::vector3(const T &x, const T &y, const T &z) : x(x), y(y), z(z)
{
}
#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_VECTOR3_IMPL_H__ */
