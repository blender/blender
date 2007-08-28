/*<html><pre>  -<a                             href="qh-qhull.htm"
  >-------------------------------</a><a name="TOP">-</a>

   qhull_a.h 
   all header files for compiling qhull

   see qh-qhull.htm

   see qhull.h for user-level definitions
   
   see user.h for user-defineable constants
   
   defines internal functions for qhull.c global.c

   copyright (c) 1993-2002, The Geometry Center

   Notes:  grep for ((" and (" to catch fprintf("lkasdjf");
           full parens around (x?y:z)
	   use '#include qhull/qhull_a.h' to avoid name clashes
*/

#ifndef qhDEFqhulla
#define qhDEFqhulla

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <float.h>    /* some compilers will not need float.h */
#include <limits.h>
#include <time.h>
#include <ctype.h>
/*** uncomment here and qset.c
     if string.h does not define memcpy()
#include <memory.h>
*/
#include "qhull.h"
#include "mem.h"
#include "qset.h"
#include "geom.h"
#include "merge.h"
#include "poly.h"
#include "io.h"
#include "stat.h"

#if qh_CLOCKtype == 2  /* defined in user.h from qhull.h */
#include <sys/types.h>
#include <sys/times.h>
#include <unistd.h>
#endif

#ifdef _MSC_VER  /* Microsoft Visual C++ */
#pragma warning( disable : 4056)  /* float constant expression.  Looks like a compiler bug */
#pragma warning( disable : 4146)  /* unary minus applied to unsigned type */
#pragma warning( disable : 4244)  /* conversion from 'unsigned long' to 'real' */
#pragma warning( disable : 4305)  /* conversion from 'const double' to 'float' */
#endif

/* ======= -macros- =========== */

/*-<a                             href="qh-qhull.htm#TOC"
  >--------------------------------</a><a name="traceN">-</a>
  
  traceN((fp.ferr, "format\n", vars));  
    calls fprintf if qh.IStracing >= N
  
  notes:
    removing tracing reduces code size but doesn't change execution speed
*/
#ifndef qh_NOtrace
#define trace0(args) {if (qh IStracing) fprintf args;}
#define trace1(args) {if (qh IStracing >= 1) fprintf args;}
#define trace2(args) {if (qh IStracing >= 2) fprintf args;}
#define trace3(args) {if (qh IStracing >= 3) fprintf args;}
#define trace4(args) {if (qh IStracing >= 4) fprintf args;}
#define trace5(args) {if (qh IStracing >= 5) fprintf args;}
#else /* qh_NOtrace */
#define trace0(args) {}
#define trace1(args) {}
#define trace2(args) {}
#define trace3(args) {}
#define trace4(args) {}
#define trace5(args) {}
#endif /* qh_NOtrace */

/***** -qhull.c prototypes (alphabetical after qhull) ********************/

void 	qh_qhull (void);
boolT   qh_addpoint (pointT *furthest, facetT *facet, boolT checkdist);
void 	qh_buildhull(void);
void    qh_buildtracing (pointT *furthest, facetT *facet);
void    qh_build_withrestart (void);
void 	qh_errexit2(int exitcode, facetT *facet, facetT *otherfacet);
void    qh_findhorizon(pointT *point, facetT *facet, int *goodvisible,int *goodhorizon);
pointT *qh_nextfurthest (facetT **visible);
void 	qh_partitionall(setT *vertices, pointT *points,int npoints);
void    qh_partitioncoplanar (pointT *point, facetT *facet, realT *dist);
void    qh_partitionpoint (pointT *point, facetT *facet);
void 	qh_partitionvisible(boolT allpoints, int *numpoints);
void    qh_precision (char *reason);
void	qh_printsummary(FILE *fp);

/***** -global.c internal prototypes (alphabetical) ***********************/

void    qh_appendprint (qh_PRINT format);
void 	qh_freebuild (boolT allmem);
void 	qh_freebuffers (void);
void    qh_initbuffers (coordT *points, int numpoints, int dim, boolT ismalloc);
int     qh_strtol (const char *s, char **endp);
double  qh_strtod (const char *s, char **endp);

/***** -stat.c internal prototypes (alphabetical) ***********************/

void	qh_allstatA (void);
void	qh_allstatB (void);
void	qh_allstatC (void);
void	qh_allstatD (void);
void	qh_allstatE (void);
void	qh_allstatE2 (void);
void	qh_allstatF (void);
void	qh_allstatG (void);
void	qh_allstatH (void);
void 	qh_freebuffers (void);
void    qh_initbuffers (coordT *points, int numpoints, int dim, boolT ismalloc);

#endif /* qhDEFqhulla */
