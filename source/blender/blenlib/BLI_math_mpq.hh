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
 */

#ifdef WITH_GMP

/* This file uses an external file header to define the multi-precision
 * rational type, mpq_class.
 * This class keeps separate multi-precision integer numerator and
 * denominator, reduced to lowest terms after each arithmetic operation.
 * It can be used where it is important to have exact arithmetic results.
 *
 * See gmplib.org for full documentation. In particular:
 * https://gmplib.org/manual/C_002b_002b-Interface-Rationals
 */
#  include "gmpxx.h"

#endif /* WITH_GMP */
