/* ========================================================================== */
/* === colamd_global.c ====================================================== */
/* ========================================================================== */

/* ----------------------------------------------------------------------------
 * COLAMD, Copyright (C) 2007, Timothy A. Davis.
 * See License.txt for the Version 2.1 of the GNU Lesser General Public License
 * http://www.cise.ufl.edu/research/sparse
 * -------------------------------------------------------------------------- */

/* Global variables for COLAMD */

#ifndef NPRINT
#ifdef MATLAB_MEX_FILE
#include "mex.h"
int (*colamd_printf) (const char *, ...) = mexPrintf ;
#else
#include <stdio.h>
int (*colamd_printf) (const char *, ...) = printf ;
#endif
#else
int (*colamd_printf) (const char *, ...) = ((void *) 0) ;
#endif

