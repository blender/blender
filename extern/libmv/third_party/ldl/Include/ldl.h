/* ========================================================================== */
/* === ldl.h:  include file for the LDL package ============================= */
/* ========================================================================== */

/* LDL Copyright (c) Timothy A Davis,
 * University of Florida.  All Rights Reserved.  See README for the License.
 */

#include "UFconfig.h"

#ifdef LDL_LONG
#define LDL_int UF_long
#define LDL_ID UF_long_id

#define LDL_symbolic ldl_l_symbolic
#define LDL_numeric ldl_l_numeric
#define LDL_lsolve ldl_l_lsolve
#define LDL_dsolve ldl_l_dsolve
#define LDL_ltsolve ldl_l_ltsolve
#define LDL_perm ldl_l_perm
#define LDL_permt ldl_l_permt
#define LDL_valid_perm ldl_l_valid_perm
#define LDL_valid_matrix ldl_l_valid_matrix

#else
#define LDL_int int
#define LDL_ID "%d"

#define LDL_symbolic ldl_symbolic
#define LDL_numeric ldl_numeric
#define LDL_lsolve ldl_lsolve
#define LDL_dsolve ldl_dsolve
#define LDL_ltsolve ldl_ltsolve
#define LDL_perm ldl_perm
#define LDL_permt ldl_permt
#define LDL_valid_perm ldl_valid_perm
#define LDL_valid_matrix ldl_valid_matrix

#endif

/* ========================================================================== */
/* === int version ========================================================== */
/* ========================================================================== */

void ldl_symbolic (int n, int Ap [ ], int Ai [ ], int Lp [ ],
    int Parent [ ], int Lnz [ ], int Flag [ ], int P [ ], int Pinv [ ]) ;

int ldl_numeric (int n, int Ap [ ], int Ai [ ], double Ax [ ],
    int Lp [ ], int Parent [ ], int Lnz [ ], int Li [ ], double Lx [ ],
    double D [ ], double Y [ ], int Pattern [ ], int Flag [ ],
    int P [ ], int Pinv [ ]) ;

void ldl_lsolve (int n, double X [ ], int Lp [ ], int Li [ ],
    double Lx [ ]) ;

void ldl_dsolve (int n, double X [ ], double D [ ]) ;

void ldl_ltsolve (int n, double X [ ], int Lp [ ], int Li [ ],
    double Lx [ ]) ;

void ldl_perm  (int n, double X [ ], double B [ ], int P [ ]) ;
void ldl_permt (int n, double X [ ], double B [ ], int P [ ]) ;

int ldl_valid_perm (int n, int P [ ], int Flag [ ]) ;
int ldl_valid_matrix ( int n, int Ap [ ], int Ai [ ]) ;

/* ========================================================================== */
/* === long version ========================================================= */
/* ========================================================================== */

void ldl_l_symbolic (UF_long n, UF_long Ap [ ], UF_long Ai [ ], UF_long Lp [ ],
    UF_long Parent [ ], UF_long Lnz [ ], UF_long Flag [ ], UF_long P [ ],
    UF_long Pinv [ ]) ;

UF_long ldl_l_numeric (UF_long n, UF_long Ap [ ], UF_long Ai [ ], double Ax [ ],
    UF_long Lp [ ], UF_long Parent [ ], UF_long Lnz [ ], UF_long Li [ ],
    double Lx [ ], double D [ ], double Y [ ], UF_long Pattern [ ],
    UF_long Flag [ ], UF_long P [ ], UF_long Pinv [ ]) ;

void ldl_l_lsolve (UF_long n, double X [ ], UF_long Lp [ ], UF_long Li [ ],
    double Lx [ ]) ;

void ldl_l_dsolve (UF_long n, double X [ ], double D [ ]) ;

void ldl_l_ltsolve (UF_long n, double X [ ], UF_long Lp [ ], UF_long Li [ ],
    double Lx [ ]) ;

void ldl_l_perm  (UF_long n, double X [ ], double B [ ], UF_long P [ ]) ;
void ldl_l_permt (UF_long n, double X [ ], double B [ ], UF_long P [ ]) ;

UF_long ldl_l_valid_perm (UF_long n, UF_long P [ ], UF_long Flag [ ]) ;
UF_long ldl_l_valid_matrix ( UF_long n, UF_long Ap [ ], UF_long Ai [ ]) ;

/* ========================================================================== */
/* === LDL version ========================================================== */
/* ========================================================================== */

#define LDL_DATE "Nov 1, 2007"
#define LDL_VERSION_CODE(main,sub) ((main) * 1000 + (sub))
#define LDL_MAIN_VERSION 2
#define LDL_SUB_VERSION 0
#define LDL_SUBSUB_VERSION 1
#define LDL_VERSION LDL_VERSION_CODE(LDL_MAIN_VERSION,LDL_SUB_VERSION)

