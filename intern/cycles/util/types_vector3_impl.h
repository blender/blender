/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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
