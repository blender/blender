/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_vector.hh"

#ifdef WITH_GMP

#  include "BLI_math_mpq.hh"

namespace blender {

using mpq2 = VecBase<mpq_class, 2>;
using mpq3 = VecBase<mpq_class, 3>;

namespace math {

uint64_t hash_mpq_class(const mpq_class &value);

template<> inline uint64_t vector_hash(const mpq2 &vec)
{
  return hash_mpq_class(vec.x) ^ (hash_mpq_class(vec.y) * 33);
}

template<> inline uint64_t vector_hash(const mpq3 &vec)
{
  return hash_mpq_class(vec.x) ^ (hash_mpq_class(vec.y) * 33) ^ (hash_mpq_class(vec.z) * 33 * 37);
}

/**
 * Cannot do this exactly in rational arithmetic!
 * Approximate by going in and out of doubles.
 */
template<> inline mpq_class length(const mpq2 &a)
{
  return mpq_class(sqrt(length_squared(a).get_d()));
}

/**
 * Cannot do this exactly in rational arithmetic!
 * Approximate by going in and out of doubles.
 */
template<> inline mpq_class length(const mpq3 &a)
{
  return mpq_class(sqrt(length_squared(a).get_d()));
}

/**
 * The buffer avoids allocating a temporary variable.
 */
inline mpq_class distance_squared_with_buffer(const mpq3 &a, const mpq3 &b, mpq3 &buffer)
{
  buffer = a;
  buffer -= b;
  return dot(buffer, buffer);
}

/**
 * The buffer avoids allocating a temporary variable.
 */
inline mpq_class dot_with_buffer(const mpq3 &a, const mpq3 &b, mpq3 &buffer)
{
  buffer = a;
  buffer *= b;
  buffer.x += buffer.y;
  buffer.x += buffer.z;
  return buffer.x;
}

}  // namespace math

}  // namespace blender

#endif /* WITH_GMP */
