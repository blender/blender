/*<html><pre>  -<a                             href="qh-qhull.htm"
  >-------------------------------</a><a name="TOP">-</a>

   unix.c
     command line interface to qhull
	 includes SIOUX interface for Macintoshes

   see qh-qhull.htm

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
    
  see:
    concise prompt below
*/  
char qh_prompta[]= "\n\
qhull- compute convex hulls and related structures.\n\
    http://www.geom.umn.edu/software/qhull  %s\n\
\n\
input (stdin):\n\
    first lines: dimension and number of points (or vice-versa).\n\
    other lines: point coordinates, best if one point per line\n\
    comments:    start with a non-numeric character\n\
    halfspaces:  use dim plus one and put offset after coefficients.\n\
                 May be preceeded by a single interior point ('H').\n\
\n\
options:\n\
    d    - Delaunay triangulation by lifting points to a paraboloid\n\
    d Qu - furthest-site Delaunay triangulation (upper convex hull)\n\
    v    - Voronoi diagram (dual of the Delaunay triangulation)\n\
    v Qu - furthest-site Voronoi diagram\n\
    Hn,n,... - halfspace intersection about point [n,n,0,...]\n\
    Qt   - triangulated output\n\
    QJ   - joggled input instead of merged facets\n\
    Qc   - keep coplanar points with nearest facet\n\
    Qi   - keep interior points with nearest facet\n\
\n\
Qhull control options:\n\
    Qbk:n   - scale coord k so that low bound is n\n\
      QBk:n - scale coord k so that upper bound is n (QBk is %2.2g)\n\
    QbB  - scale input to unit cube centered at the origin\n\
    Qbb  - scale last coordinate to [0,m] for Delaunay triangulations\n\
    Qbk:0Bk:0 - remove k-th coordinate from input\n\
    QJn  - randomly joggle input in range [-n,n]\n\
    QRn  - random rotation (n=seed, n=0 time, n=-1 time/no rotate)\n\
%s%s%s%s";  /* split up qh_prompt for Visual C++ */
char qh_promptb[]= "\
    Qf   - partition point to furthest outside facet\n\
    Qg   - only build good facets (needs 'QGn', 'QVn', or 'PdD')\n\
    Qm   - only process points that would increase max_outside\n\
    Qr   - process random outside points instead of furthest ones\n\
    Qs   - search all points for the initial simplex\n\
    Qu   - for 'd' or 'v', compute upper hull without point at-infinity\n\
              returns furthest-site Delaunay triangulation\n\
    Qv   - test vertex neighbors for convexity\n\
    Qx   - exact pre-merges (skips coplanar and angle-coplanar facets)\n\
    Qz   - add point-at-infinity to Delaunay triangulation\n\
    QGn  - good facet if visible from point n, -n for not visible\n\
    QVn  - good facet if it includes point n, -n if not\n\
    Q0   - turn off default premerge with 'C-0'/'Qx'\n\
    Q1	   - sort merges by type instead of angle\n\
    Q2   - merge all non-convex at once instead of independent sets\n\
    Q3   - do not merge redundant vertices\n\
    Q4   - avoid old->new merges\n\
    Q5   - do not correct outer planes at end of qhull\n\
    Q6   - do not pre-merge concave or coplanar facets\n\
    Q7   - depth-first processing instead of breadth-first\n\
    Q8   - do not process near-inside points\n\
    Q9   - process furthest of furthest points\n\
    Q10  - no special processing for narrow distributions\n\
    Q11  - copy normals and recompute centrums for tricoplanar facets\n\
\n\
";
char qh_promptc[]= "\
Topts- Trace options:\n\
    T4   - trace at level n, 4=all, 5=mem/gauss, -1= events\n\
    Tc   - check frequently during execution\n\
    Ts   - print statistics\n\
    Tv   - verify result: structure, convexity, and point inclusion\n\
    Tz   - send all output to stdout\n\
    TFn  - report summary when n or more facets created\n\
    TI file - input data from file, no spaces or single quotes\n\
    TO file - output results to file, may be enclosed in single quotes\n\
    TPn  - turn on tracing when point n added to hull\n\
     TMn - turn on tracing at merge n\n\
     TWn - trace merge facets when width > n\n\
    TRn  - rerun qhull n times.  Use with 'QJn'\n\
    TVn  - stop qhull after adding point n, -n for before (see TCn)\n\
     TCn - stop qhull after building cone for point n (see TVn)\n\
\n\
Precision options:\n\
    Cn   - radius of centrum (roundoff added).  Merge facets if non-convex\n\
     An  - cosine of maximum angle.  Merge facets if cosine > n or non-convex\n\
           C-0 roundoff, A-0.99/C-0.01 pre-merge, A0.99/C0.01 post-merge\n\
    En   - max roundoff error for distance computation\n\
    Rn   - randomly perturb computations by a factor of [1-n,1+n]\n\
    Vn   - min distance above plane for a visible facet (default 3C-n or En)\n\
    Un   - max distance below plane for a new, coplanar point (default Vn)\n\
    Wn   - min facet width for outside point (before roundoff, default 2Vn)\n\
\n\
Output formats (may be combined; if none, produces a summary to stdout):\n\
    f    - facet dump\n\
    G    - Geomview output (see below)\n\
    i    - vertices incident to each facet\n\
    m    - Mathematica output (2-d and 3-d)\n\
    o    - OFF format (dim, points and facets; Voronoi regions)\n\
    n    - normals with offsets\n\
    p    - vertex coordinates or Voronoi vertices (coplanar points if 'Qc')\n\
    s    - summary (stderr)\n\
\n\
";
char qh_promptd[]= "\
More formats:\n\
    Fa   - area for each facet\n\
    FA   - compute total area and volume for option 's'\n\
    Fc   - count plus coplanar points for each facet\n\
           use 'Qc' (default) for coplanar and 'Qi' for interior\n\
    FC   - centrum or Voronoi center for each facet\n\
    Fd   - use cdd format for input (homogeneous with offset first)\n\
    FD   - use cdd format for numeric output (offset first)\n\
    FF   - facet dump without ridges\n\
    Fi   - inner plane for each facet\n\
           for 'v', separating hyperplanes for bounded Voronoi regions\n\
    FI   - ID of each facet\n\
    Fm   - merge count for each facet (511 max)\n\
    Fn   - count plus neighboring facets for each facet\n\
    FN   - count plus neighboring facets for each point\n\
    Fo   - outer plane (or max_outside) for each facet\n\
           for 'v', separating hyperplanes for unbounded Voronoi regions\n\
    FO   - options and precision constants\n\
    Fp   - dim, count, and intersection coordinates (halfspace only)\n\
    FP   - nearest vertex and distance for each coplanar point\n\
    FQ   - command used for qhull\n\
    Fs   - summary: #int (8), dimension, #points, tot vertices, tot facets,\n\
                      output: #vertices, #facets, #coplanars, #nonsimplicial\n\
                    #real (2), max outer plane, min vertex\n\
    FS   - sizes:   #int (0)\n\
                    #real(2) tot area, tot volume\n\
    Ft   - triangulation with centrums for non-simplicial facets (OFF format)\n\
    Fv   - count plus vertices for each facet\n\
           for 'v', Voronoi diagram as Voronoi vertices for pairs of sites\n\
    FV   - average of vertices (a feasible point for 'H')\n\
    Fx   - extreme points (in order for 2-d)\n\
\n\
";
char qh_prompte[]= "\
Geomview options (2-d, 3-d, and 4-d; 2-d Voronoi)\n\
    Ga   - all points as dots\n\
     Gp  -  coplanar points and vertices as radii\n\
     Gv  -  vertices as spheres\n\
    Gi   - inner planes only\n\
     Gn  -  no planes\n\
     Go  -  outer planes only\n\
    Gc   - centrums\n\
    Gh   - hyperplane intersections\n\
    Gr   - ridges\n\
    GDn  - drop dimension n in 3-d and 4-d output\n\
    Gt   - for 3-d 'd', transparent outer ridges\n\
\n\
Print options:\n\
    PAn  - keep n largest facets by area\n\
    Pdk:n - drop facet if normal[k] <= n (default 0.0)\n\
    PDk:n - drop facet if normal[k] >= n\n\
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
qhull- compute convex hulls and related structures.  %s\n\
    input (stdin): dimension, n, point coordinates\n\
    comments start with a non-numeric character\n\
    halfspace: use dim+1 and put offsets after coefficients\n\
\n\
options (qh-quick.htm):\n\
    d    - Delaunay triangulation by lifting points to a paraboloid\n\
    d Qu - furthest-site Delaunay triangulation (upper convex hull)\n\
    v    - Voronoi diagram as the dual of the Delaunay triangulation\n\
    v Qu - furthest-site Voronoi diagram\n\
    H1,1 - Halfspace intersection about [1,1,0,...] via polar duality\n\
    Qt   - triangulated output\n\
    QJ   - joggled input instead of merged facets\n\
    Tv   - verify result: structure, convexity, and point inclusion\n\
    .    - concise list of all options\n\
    -    - one-line description of all options\n\
\n\
Output options (subset):\n\
    s    - summary of results (default)\n\
    i    - vertices incident to each facet\n\
    n    - normals with offsets\n\
    p    - vertex coordinates (if 'Qc', includes coplanar points)\n\
           if 'v', Voronoi vertices\n\
    Fp   - halfspace intersections\n\
    Fx   - extreme points (convex hull vertices)\n\
    FA   - compute total area and volume\n\
    o    - OFF format (if 'v', outputs Voronoi regions)\n\
    G    - Geomview output (2-d, 3-d and 4-d)\n\
    m    - Mathematica output (2-d and 3-d)\n\
    QVn  - print facets that include point n, -n if not\n\
    TO file- output results to file, may be enclosed in single quotes\n\
\n\
examples:\n\
    rbox c d D2 | qhull Qc s f Fx | more      rbox 1000 s | qhull Tv s FA\n\
    rbox 10 D2 | qhull d QJ s i TO result     rbox 10 D2 | qhull v QJ p\n\
    rbox 10 D2 | qhull d Qu QJ m              rbox 10 D2 | qhull v Qu QJ o\n\
    rbox c | qhull n                          rbox c | qhull FV n | qhull H Fp\n\
    rbox d D12 | qhull QR0 FA                 rbox c D7 | qhull FA TF1000\n\
    rbox y 1000 W0 | qhull                    rbox 10 | qhull v QJ o Fv\n\
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
Except for 'F.' and 'PG', upper-case options take an argument.\n\
\n\
 delaunay       voronoi	       Geomview       Halfspace      facet_dump\n\
 incidences     mathematica    normals        OFF_format     points\n\
 summary\n\
\n\
 Farea          FArea-total    Fcoplanars     FCentrums      Fd-cdd-in\n\
 FD-cdd-out     FF-dump-xridge Finner         FIDs           Fmerges\n\
 Fneighbors     FNeigh-vertex  Fouter         FOptions       Fpoint-intersect\n\
 FPoint_near    FQhull         Fsummary       FSize          Ftriangles\n\
 Fvertices      Fvoronoi       FVertex-ave    Fxtremes\n\
\n\
 Gvertices      Gpoints        Gall_points    Gno_planes     Ginner\n\
 Gcentrums      Ghyperplanes   Gridges        Gouter         GDrop_dim\n\
 Gtransparent\n\
\n\
 PArea-keep     Pdrop d0:0D0   Pgood          PFacet_area_keep\n\
 PGood_neighbors PMerge-keep   Poutput_forced Pprecision_not\n\
\n\
 QbBound 0:0.5  Qbk:0Bk:0_drop QbB-scale-box  Qbb-scale-last Qcoplanar\n\
 Qfurthest      Qgood_only     QGood_point    Qinterior      Qmax_out\n\
 QJoggle        Qrandom        QRotate        Qsearch_1st    Qtriangulate\n\
 QupperDelaunay QVertex_good   Qvneighbors    Qxact_merge    Qzinfinite\n\
\n\
 Q0_no_premerge Q1_no_angle    Q2_no_independ Q3_no_redundant Q4_no_old\n\
 Q5_no_check_out Q6_no_concave Q7_depth_first Q8_no_near_in  Q9_pick_furthest\n\
 Q10_no_narrow  Q11_trinormals\n\
\n\
 T4_trace       Tcheck_often   Tstatistics    Tverify        Tz_stdout\n\
 TFacet_log     TInput_file    TPoint_trace   TMerge_trace   TOutput_file\n\
 TRerun         TWide_trace    TVertex_stop   TCone_stop\n\
\n\
 Angle_max      Centrum_size   Error_round    Random_dist    Visible_min\n\
 Ucoplanar_max  Wide_outside\n\
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
    fprintf(stdout, qh_prompta, qh_VERSION, qh_DEFAULTbox,
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
    qh_initflags (qh qhull_command);
    points= qh_readpoints (&numpoints, &dim, &ismalloc);
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

