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


#ifndef TNT_H
#define TNT_H

//---------------------------------------------------------------------
//  tnt.h       TNT general header file.  Defines default types
//              and conventions.
//---------------------------------------------------------------------

//---------------------------------------------------------------------
//  Include current version 
//---------------------------------------------------------------------
#include "version.h"

//---------------------------------------------------------------------
// Define the data type used for matrix and vector Subscripts.
// This will default to "int", but it can be overriden at compile time,
// e.g.
// 
//      g++ -DTNT_SUBSCRIPT_TYPE='unsinged long' ...
//
// See subscript.h for details.
//---------------------------------------------------------------------

#include "subscript.h"



//---------------------------------------------------------------------
// Define this macro if you want  TNT to ensure all refernces
// are within the bounds of the array.  This encurs a run-time
// overhead, of course, but is recommended while developing
// code.  It can be turned off for production runs.
// 
//       #define TNT_BOUNDS_CHECK
//---------------------------------------------------------------------
//
#define TNT_BOUNDS_CHECK
#ifdef TNT_NO_BOUNDS_CHECK
#undef TNT_BOUNDS_CHECK
#endif

//---------------------------------------------------------------------
// Define this macro if you want to utilize matrix and vector
// regions.  This is typically on, but you can save some
// compilation time by turning it off.  If you do this and
// attempt to use regions you will get an error message.
//
//       #define TNT_USE_REGIONS
//---------------------------------------------------------------------
//
#define TNT_USE_REGIONS

//---------------------------------------------------------------------
//  
//---------------------------------------------------------------------
// if your system doesn't have abs() min(), and max() uncoment the following
//---------------------------------------------------------------------
//
// 
//#define __NEED_ABS_MIN_MAX_

#include "tntmath.h"

#endif // TNT_H

