/**
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

