/**
 * $Id$
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


#ifndef SUBSCRPT_H
#define SUBSCRPT_H


//---------------------------------------------------------------------
// This definition describes the default TNT data type used for
// indexing into TNT matrices and vectors.  The data type should
// be wide enough to index into large arrays.  It defaults to an
// "int", but can be overriden at compile time redefining TNT_SUBSCRIPT_TYPE,
// e.g.
// 
//      g++ -DTNT_SUBSCRIPT_TYPE='unsigned int'  ...
//
//---------------------------------------------------------------------
//

#ifndef TNT_SUBSCRIPT_TYPE
#define TNT_SUBSCRIPT_TYPE int
#endif

namespace TNT
{
    typedef TNT_SUBSCRIPT_TYPE Subscript;
}


// () indexing in TNT means 1-offset, i.e. x(1) and A(1,1) are the
// first elements.  This offset is left as a macro for future
// purposes, but should not be changed in the current release.
//
//
#define TNT_BASE_OFFSET (1)

#endif

