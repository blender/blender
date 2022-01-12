/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bli
 * \brief Math vector functions needed specifically for mesh intersect and boolean.
 */

#include "BLI_math_vec_types.hh"

#ifdef WITH_GMP
#  include "BLI_math_mpq.hh"
#  include "BLI_math_vec_mpq_types.hh"
#endif

namespace blender {

/**
 * #orient2d gives the exact result, using multi-precision arithmetic when result
 * is close to zero. orient3d_fast just uses double arithmetic, so may be
 * wrong if the answer is very close to zero.
 * Similarly, for #incircle and #incircle_fast.
 */
int orient2d(const double2 &a, const double2 &b, const double2 &c);
int orient2d_fast(const double2 &a, const double2 &b, const double2 &c);

int incircle(const double2 &a, const double2 &b, const double2 &c, const double2 &d);
int incircle_fast(const double2 &a, const double2 &b, const double2 &c, const double2 &d);

/**
 * #orient3d gives the exact result, using multi-precision arithmetic when result
 * is close to zero. orient3d_fast just uses double arithmetic, so may be
 * wrong if the answer is very close to zero.
 * Similarly, for #insphere and #insphere_fast.
 */
int orient3d(const double3 &a, const double3 &b, const double3 &c, const double3 &d);
int orient3d_fast(const double3 &a, const double3 &b, const double3 &c, const double3 &d);

int insphere(
    const double3 &a, const double3 &b, const double3 &c, const double3 &d, const double3 &e);
int insphere_fast(
    const double3 &a, const double3 &b, const double3 &c, const double3 &d, const double3 &e);

#ifdef WITH_GMP
/**
 * Return +1 if a, b, c are in CCW order around a circle in the plane.
 * Return -1 if they are in CW order, and 0 if they are in line.
 */
int orient2d(const mpq2 &a, const mpq2 &b, const mpq2 &c);
/**
 * Return +1 if d is in the oriented circle through a, b, and c.
 * The oriented circle goes CCW through a, b, and c.
 * Return -1 if d is outside, and 0 if it is on the circle.
 */
int incircle(const mpq2 &a, const mpq2 &b, const mpq2 &c, const mpq2 &d);
/**
 * Return +1 if d is below the plane containing a, b, c (which appear
 * CCW when viewed from above the plane).
 * Return -1 if d is above the plane.
 * Return 0 if it is on the plane.
 */
int orient3d(const mpq3 &a, const mpq3 &b, const mpq3 &c, const mpq3 &d);
#endif
}  // namespace blender
