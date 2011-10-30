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



// Header file to define C/Fortran conventions (Platform specific)

#ifndef FORTRAN_H
#define FORTRAN_H

// help map between C/C++ data types and Fortran types

typedef int     Fortran_integer;
typedef float   Fortran_float;
typedef double  Fortran_double;


typedef Fortran_double *fda_;        // (in/out) double precision array
typedef const Fortran_double *cfda_; // (in) double precsion array

typedef Fortran_double *fd_;        // (in/out)  single double precision
typedef const Fortran_double *cfd_; // (in) single double precision

typedef Fortran_float *ffa_;        // (in/out) float precision array
typedef const Fortran_float *cffa_; // (in) float precsion array

typedef Fortran_float *ff_;         // (in/out)  single float precision
typedef const Fortran_float *cff_;  // (in) single float precision

typedef Fortran_integer *fia_;          // (in/out)  single integer array
typedef const Fortran_integer *cfia_;   // (in) single integer array

typedef Fortran_integer *fi_;           // (in/out)  single integer
typedef const Fortran_integer *cfi_;    // (in) single integer

typedef char *fch_;                // (in/out) single character
typedef char *cfch_;               // (in) single character


#ifndef TNT_SUBSCRIPT_TYPE
#define TNT_SUBSCRIPT_TYPE TNT::Fortran_integer
#endif

#endif // FORTRAN_H

