/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*

*
* Template Numerical Toolkit (TNT): Linear Algebra Module
*
* Mathematical and Computational Sciences Division
* National Institute of Technology,
* Gaithersburg, MD USA
*
*
* This software was developed at the National Institute of Standards and
* Technology (NIST) by employees of the Federal Government in the course
* of their official duties. Pursuant to title 17 Section 105 of the
* United States Code, this software is not subject to copyright protection
* and is in the public domain.  The Template Numerical Toolkit (TNT) is
* an experimental system.  NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*
* BETA VERSION INCOMPLETE AND SUBJECT TO CHANGE
* see http://math.nist.gov/tnt for latest updates.
*
*/



// The requirements for a bare-bones vector class:
//
//
//   o) must have 0-based [] indexing for const and
//          non-const objects  (i.e. operator[] defined)
//
//   o) must have size() method to denote the number of
//          elements
//   o) must clean up after itself when destructed
//          (i.e. no memory leaks)
//
//   -) must have begin() and end() methods  (The begin()
//          method is necessary, because relying on 
//          &v_[0] may not work on a empty vector (i.e. v_ is NULL.)
//
//   o) must be templated
//   o) must have X::value_type defined to be the types of elements
//   o) must have X::X(const &x) copy constructor (by *value*)
//   o) must have X::X(int N) constructor to N-length vector
//          (NOTE: this constructor need *NOT* initalize elements)
//
//   -) must have X::X(int N, T scalar) constructor to initalize
//          elements to value of "scalar".
//
//       ( removed, because valarray<> class uses (scalar, N)  rather
//              than (N, scalar) )
//   -) must have X::X(int N, const T* scalars) constructor to copy from
//              any C linear array
//
//         ( removed, because of same reverse order of valarray<> )
//
//   o) must have assignment A=B, by value
//
//  NOTE: this class is *NOT* meant to be derived from,
//  so its methods (particularly indexing) need not be
//  declared virtual.
//
//
//  Some things it *DOES NOT* need to do are
//
//  o) bounds checking
//  o) array referencing (e.g. reference counting)
//  o) support () indexing
//  o) I/O 
//
