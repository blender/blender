/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_structure_of_array.hh"

namespace blender {

/** Optimized conversion functions for types like float3 and int3. */
void aos_to_soa_3x4byte(int64_t size, const void *src, void *a, void *b, void *c);
void soa_to_aos_3x4byte(int64_t size, const void *a, const void *b, const void *c, void *dst);

/** Optimized conversion functions for types like float4 and ColorGeometry4f. */
void aos_to_soa_4x4byte(int64_t size, const void *src, void *a, void *b, void *c, void *d);
void soa_to_aos_4x4byte(
    int64_t size, const void *a, const void *b, const void *c, const void *d, void *dst);

template<typename T>
concept Is3x4Bytes = soa_component_num<T> == 3 && sizeof(T) == 12 &&
                     sizeof(soa_component_t<T, 0>) == 4 && sizeof(soa_component_t<T, 1>) == 4 &&
                     sizeof(soa_component_t<T, 2>) == 4;

template<typename T>
concept Is4x4Bytes = soa_component_num<T> == 4 && sizeof(T) == 16 &&
                     sizeof(soa_component_t<T, 0>) == 4 && sizeof(soa_component_t<T, 1>) == 4 &&
                     sizeof(soa_component_t<T, 2>) == 4 && sizeof(soa_component_t<T, 3>) == 4;

template<Is3x4Bytes T>
inline void aos_to_soa(const int64_t size,
                       const T *src,
                       soa_component_t<T, 0> *a,
                       soa_component_t<T, 1> *b,
                       soa_component_t<T, 2> *c)
{
  aos_to_soa_3x4byte(size, src, a, b, c);
}

template<Is3x4Bytes T>
inline void soa_to_aos(const int64_t size,
                       const soa_component_t<T, 0> *a,
                       const soa_component_t<T, 1> *b,
                       const soa_component_t<T, 2> *c,
                       T *dst)
{
  soa_to_aos_3x4byte(size, a, b, c, dst);
}

template<Is4x4Bytes T>
inline void aos_to_soa(const int64_t size,
                       const T *src,
                       soa_component_t<T, 0> *a,
                       soa_component_t<T, 1> *b,
                       soa_component_t<T, 2> *c,
                       soa_component_t<T, 3> *d)
{
  aos_to_soa_4x4byte(size, src, a, b, c, d);
}

template<Is4x4Bytes T>
inline void soa_to_aos(const int64_t size,
                       const soa_component_t<T, 0> *a,
                       const soa_component_t<T, 1> *b,
                       const soa_component_t<T, 2> *c,
                       const soa_component_t<T, 3> *d,
                       T *dst)
{
  soa_to_aos_4x4byte(size, a, b, c, d, dst);
}

}  // namespace blender
