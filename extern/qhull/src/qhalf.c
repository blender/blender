/*<html><pre>  -<a                             href="qh-qhull.htm"
  >-------------------------------</a><a name="TOP">-</a>

   qhalf.c
     compute the intersection of halfspaces about a point

   see unix.c for full interface

   copyright (c) 1993-2002, The Geometry Center
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "qhull.h"
#include "mem.h"
#include "qset.h"

#if __MWERKS__ && __POWERPC__
#include <SIOUX.h>
#include <Files.h>
#include <console.h>
#include <Desk.h>

#elif __cplusplus
extern "C" {
  int isatty (int);
}

#elif _MSC_VER
#include <io.h>
#define isatty _isatty

#else
int isatty (int);  /* returns 1 if stdin is a tty
		   if "Undefined symbol" this can be deleted along with call in main() */
#endif

/*-<a                             href="qh-qhull.htm#TOC"
  >-------------------------------</a><a name="prompt">-</a>

  qh_prompt 
    long prompt for qhull
    
  notes:
    restricted version of qhull.c
 
  see:
    concise prompt below
*/  

/* duplicated in qhalf.htm */
char hidden_options[]=" d n v Qbb QbB Qf Qg Qm Qr QR Qv Qx Qz TR E V Fa FA FC FD FS Ft FV Gt Q0 Q1 Q2 Q3 Q4 Q5 Q6 Q7 Q8 Q9 ";

char qh_prompta[]= "\n\
qhalf- compute the intersection of halfspaces about a point\n\
    http://www.geom.umn.edu/software/qhull  %s\n\
\n\
input (stdin):\n\
    optional interior point: dimension, 1, coordinates\n\
    first lines: dimension+1 and number of halfspaces\n\
    other lines: halfspace coefficients followed by offset\n\
    comments:    start with a non-numeric character\n\
\n\
options:\n\
    Hn,n - specify coordinates of interior point\n\
    Qt   - triangulated output\n\
    QJ   - joggled input instead of merged facets\n\
    Qc   - keep coplanar halfspaces\n\
    Qi   - keep other redundant halfspaces\n\
\n\
Qhull control options:\n\
    QJn  - randomly joggle input in range [-n,n]\n\
%s%s%s%s";  /* split up qh_prompt for Visual C++ */
char qh_promptb[]= "\
    Qbk:0Bk:0 - remove k-th coordinate from input\n\
    Qs   - search all halfspaces for the initial simplex\n\
    QGn  - print intersection if visible to halfspace n, -n for not\n\
    QVn  - print intersections for halfspace n, -n if not\n\
\n\
";
char qh_promptc[]= "\
Trace options:\n\
    T4   - trace at level n, 4=all, 5=mem/gauss, -1= events\n\
    Tc   - check frequently during execution\n\
    Ts   - print statistics\n\
    Tv   - verify result: structure, convexity, and redundancy\n\
    Tz   - send all output to stdout\n\
    TFn  - report summary when n or more facets created\n\
    TI file - input data from file, no spaces or single quotes\n\
    TO file - output results to file, may be enclosed in single quotes\n\
    TPn  - turn on tracing when halfspace n added to intersection\n\
    TMn  - turn on tracing at merge n\n\
    TWn  - trace merge facets when width > n\n\
    TVn  - stop qhull after adding halfspace n, -n for before (see TCn)\n\
    TCn  - stop qhull after building cone for halfspace n (see TVn)\n\
\n\
Precision options:\n\
    Cn   - radius of centrum (roundoff added).  Merge facets if non-convex\n\
     An  - cosine of maximum angle.  Merge facets if cosine > n or non-convex\n\
           C-0 roundoff, A-0.99/C-0.01 pre-merge, A0.99/C0.01 post-merge\n\
    Rn   - randomly perturb computations by a factor of [1-n,1+n]\n\
    Un   - max distance below plane for a new, coplanar halfspace\n\
    Wn   - min facet width for outside halfspace (before roundoff)\n\
\n\
Output formats (may be combined; if none, produces a summary to stdout):\n\
    f    - facet dump\n\
    G    - Geomview output (dual convex hull)\n\
    i    - non-redundant halfspaces incident to each intersection\n\
    m    - Mathematica output (dual convex hull)\n\
    o    - OFF format (dual convex hull: dimension, points, and facets)\n\
    p    - vertex coordinates of dual convex hull (coplanars if 'Qc' or 'Qi')\n\
    s    - summary (stderr)\n\
\n\
";
char qh_promptd[]= "\
More formats:\n\
    Fc   - count plus redundant halfspaces for each intersection\n\
         -   Qc (default) for coplanar and Qi for other redundant\n\
    Fd   - use cdd format for input (homogeneous with offset first)\n\
    FF   - facet dump without ridges\n\
    FI   - ID of each intersection\n\
    Fm   - merge count for each intersection (511 max)\n\
    Fn   - count plus neighboring intersections for each intersection\n\
    FN   - count plus intersections for each non-redundant halfspace\n\
    FO   - options and precision constants\n\
    Fp   - dim, count, and intersection coordinates\n\
    FP   - nearest halfspace and distance for each redundant halfspace\n\
    FQ   - command used for qhalf\n\
    Fs   - summary: #int (8), dim, #halfspaces, #non-redundant, #intersections\n\
                      for output: #non-redundant, #intersections, #coplanar\n\
                                  halfspaces, #non-simplicial intersections\n\
                    #real (2), max outer plane, min vertex\n\
    Fv   - count plus non-redundant halfspaces for each intersection\n\
    Fx   - non-redundant halfspaces\n\
\n\
";
char qh_prompte[]= "\
Geomview output (2-d, 3-d and 4-d; dual convex hull)\n\
    Ga   - all points (i.e., transformed halfspaces) as dots\n\
     Gp  -  coplanar points and vertices as radii\n\
     Gv  -  vertices (i.e., non-redundant halfspaces) as spheres\n\
    Gi   - inner planes (i.e., halfspace intersections) only\n\
     Gn  -  no planes\n\
     Go  -  outer planes only\n\
    Gc	 - centrums\n\
    Gh   - hyperplane intersections\n\
    Gr   - ridges\n\
    GDn  - drop dimension n in 3-d and 4-d output\n\
\n\
Print options:\n\
    PAn  - keep n largest facets (i.e., intersections) by area\n\
    Pdk:n- drop facet if normal[k] <= n (default 0.0)\n\
    PDk:n- drop facet if normal[k] >= n\n\
    Pg   - print good facets (needs 'QGn' or 'QVn')\n\
    PFn  - keep facets whose area is at least n\n\
    PG   - print neighbors of good facets\n\
    PMn  - keep n facets with most merges\n\
    Po   - force output.  If error, output neighborhood of facet\n\
    Pp   - do not report precision problems\n\
\n\
    .    - list of all options\n\
    -    - one line descriptions of all options\n\
";
/* for opts, don't assign 'e' or 'E' to a flag (already used for exponent) */

/*-<a                             href="qh-qhull.htm#TOC"
  >-------------------------------</a><a name="prompt2">-</a>

  qh_prompt2
    synopsis for qhull 
*/  
char qh_prompt2[]= "\n\
qhalf- halfspace intersection about a point. Qhull %s\n\
    input (stdin): [dim, 1, interior point], dim+1, n, coefficients+offset\n\
    comments start with a non-numeric character\n\
\n\
options (qhalf.htm):\n\
    Hn,n - specify coordinates of interior point\n\
    Qt   - triangulated output\n\
    QJ   - joggled input instead of merged facets\n\
    Tv   - verify result: structure, convexity, and redundancy\n\
    .    - concise list of all options\n\
    -    - one-line description of all options\n\
\n\
output options (subset):\n\
    s    - summary of results (default)\n\
    Fp   - intersection coordinates\n\
    Fv   - non-redundant halfspaces incident to each intersection\n\
    Fx   - non-redundant halfspaces\n\
    o    - OFF file format (dual convex hull)\n\
    G    - Geomview output (dual convex hull)\n\
    m    - Mathematica output (dual convex hull)\n\
    QVn  - print intersections for halfspace n, -n if not\n\
    TO file - output results to file, may be enclosed in single quotes\n\
\n\
examples:\n\
    rbox d | qconvex FQ n | qhalf s H0,0,0 Fp\n\
    rbox c | qconvex FQ FV n | qhalf s i\n\
    rbox c | qconvex FQ FV n | qhalf s o\n\
\n\
";
/* for opts, don't assign 'e' or 'E' to a flag (already used for exponent) */

/*-<a                             href="qh-qhull.htm#TOC"
  >-------------------------------</a><a name="prompt3">-</a>

  qh_prompt3
    concise prompt for qhull 
*/  
char qh_prompt3[]= "\n\
Qhull %s.\n\
Except for 'F.' and 'PG', upper_case options take an argument.\n\
\n\
 incidences     Geomview       mathematica    OFF_format     point_dual\n\
 summary        facet_dump\n\
\n\
 Fc_redundant   Fd_cdd_in      FF_dump_xridge FIDs           Fmerges\n\
 Fneighbors     FN_intersect   FOptions       Fp_coordinates FP_nearest\n\
 FQhalf         Fsummary       Fv_halfspace   Fx_non_redundant\n\
\n\
 Gvertices      Gpoints        Gall_points    Gno_planes     Ginner\n\
 Gcentrums      Ghyperplanes   Gridges        Gouter         GDrop_dim\n\
\n\
 PArea_keep     Pdrop d0:0D0   Pgood          PFacet_area_keep\n\
 PGood_neighbors PMerge_keep   Poutput_forced Pprecision_not\n\
\n\
 Qbk:0Bk:0_drop Qcoplanar      QG_half_good   Qi_redundant   QJoggle\n\
 Qsearch_1st    Qtriangulate   QVertex_good\n\
\n\
 T4_trace       Tcheck_often   Tstatistics    Tverify        Tz_stdout\n\
 TFacet_log     TInput_file    TPoint_trace   TMerge_trace   TOutput_file\n\
 TWide_trace    TVertex_stop   TCone_stop\n\
\n\
 Angle_max      Centrum_size   Random_dist    Ucoplanar_max  Wide_outside\n\
";

/*-<a                             href="qh-qhull.htm#TOC"
  >-------------------------------</a><a name="main">-</a>
  
  main( argc, argv )
    processes the command line, calls qhull() to do the work, and exits
  
  design:
    initializes data structures
    reads points
    finishes initialization
    computes convex hull and other structures
    checks the result
    writes the output
    frees memory
*/
int main(int argc, char *argv[]) {
  int curlong, totlong; /* used !qh_NOmem */
  int exitcode, numpoints, dim;
  coordT *points;
  boolT ismalloc;

#if __MWERKS__ && __POWERPC__
  char inBuf[BUFSIZ], outBuf[BUFSIZ], errBuf[BUFSIZ];
  SIOUXSettings.showstatusline= false;
  SIOUXSettings.tabspaces= 1;
  SIOUXSettings.rows= 40;
  if (setvbuf (stdin, inBuf, _IOFBF, sizeof(inBuf)) < 0   /* w/o, SIOUX I/O is slow*/
  || setvbuf (stdout, outBuf, _IOFBF, sizeof(outBuf)) < 0
  || (stdout != stderr && setvbuf (stderr, errBuf, _IOFBF, sizeof(errBuf)) < 0)) 
    fprintf (stderr, "qhull internal warning (main): could not change stdio to fully buffered.\n");
  argc= ccommand(&argv);
#endif

  if ((argc == 1) && isatty( 0 /*stdin*/)) {      
    fprintf(stdout, qh_prompt2, qh_VERSION);
    exit(qh_ERRnone);
  }
  if (argc > 1 && *argv[1] == '-' && !*(argv[1]+1)) {
    fprintf(stdout, qh_prompta, qh_VERSION, 
        qh_promptb, qh_promptc, qh_promptd, qh_prompte);
    exit(qh_ERRnone);
  }
  if (argc >1 && *argv[1] == '.' && !*(argv[1]+1)) {
    fprintf(stdout, qh_prompt3, qh_VERSION);
    exit(qh_ERRnone);
  }
  qh_init_A (stdin, stdout, stderr, argc, argv);  /* sets qh qhull_command */
  exitcode= setjmp (qh errexit); /* simple statement for CRAY J916 */
  if (!exitcode) {
    qh_option ("Halfspace", NULL, NULL);
    qh HALFspace= True;    /* 'H'   */
    qh_checkflags (qh qhull_command, hidden_options);
    qh_initflags (qh qhull_command);
    if (qh SCALEinput) {
      fprintf(qh ferr, "\
qhull error: options 'Qbk:n' and 'QBk:n' are not used with qhalf.\n\
             Use 'Qbk:0Bk:0 to drop dimension k.\n");
      qh_errexit(qh_ERRinput, NULL, NULL);
    }
    points= qh_readpoints (&numpoints, &dim, &ismalloc);
    if (dim >= 5) {
      qh_option ("Qxact_merge", NULL, NULL);
      qh MERGEexact= True; /* 'Qx' always */
    }
    qh_init_B (points, numpoints, dim, ismalloc);
    qh_qhull();
    qh_check_output();
    qh_produce_output();
    if (qh VERIFYoutput && !qh FORCEoutput && !qh STOPpoint && !qh STOPcone)
      qh_check_points();
    exitcode= qh_ERRnone;
  }
  qh NOerrexit= True;  /* no more setjmp */
#ifdef qh_NOmem
  qh_freeqhull( True);
#else
  qh_freeqhull( False);
  qh_memfreeshort (&curlong, &totlong);
  if (curlong || totlong) 
    fprintf (stderr, "qhull internal warning (main): did not free %d bytes of long memory (%d pieces)\n",
       totlong, curlong);
#endif
  return exitcode;
} /* main */

