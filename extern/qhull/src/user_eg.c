/*<html><pre>  -<a                             href="qh-user.htm"
  >-------------------------------</a><a name="TOP">-</a>

  user_eg.c
  sample code for calling qhull() from an application
  
  call with:

     user_eg "cube/diamond options" "delaunay options" "halfspace options"

  for example:

     user_eg                             # return summaries

     user_eg "n" "o" "Fp"                # return normals, OFF, points

     user_eg "n Qt" "o" "Fp"             # triangulated cube

     user_eg "QR0 p" "QR0 v p" "QR0 Fp"  # rotate input and return points
                                         # 'v' returns Voronoi
					 # transform is rotated for halfspaces

   main() makes three runs of qhull.

     1) compute the convex hull of a cube

     2a) compute the Delaunay triangulation of random points

     2b) find the Delaunay triangle closest to a point.

     3) compute the halfspace intersection of a diamond

 notes:
 
   For another example, see main() in unix.c and user_eg2.c.
   These examples, call qh_qhull() directly.  They allow
   tighter control on the code loaded with Qhull.

   For a simple C++ example, see qhull_interface.cpp

   Summaries are sent to stderr if other output formats are used

   compiled by 'make user_eg'

   see qhull.h for data structures, macros, and user-callable functions.
*/

#include "qhull_a.h"

/*-------------------------------------------------
-internal function prototypes
*/
void print_summary (void);
void makecube (coordT *points, int numpoints, int dim);
void makeDelaunay (coordT *points, int numpoints, int dim, int seed);
void findDelaunay (int dim);
void makehalf (coordT *points, int numpoints, int dim);

/*-------------------------------------------------
-print_summary()
*/
void print_summary (void) {
  facetT *facet;
  int k;

  printf ("\n%d vertices and %d facets with normals:\n", 
                 qh num_vertices, qh num_facets);
  FORALLfacets {
    for (k=0; k < qh hull_dim; k++) 
      printf ("%6.2g ", facet->normal[k]);
    printf ("\n");
  }
}

/*--------------------------------------------------
-makecube- set points to vertices of cube
  points is numpoints X dim
*/
void makecube (coordT *points, int numpoints, int dim) {
  int j,k;
  coordT *point;

  for (j=0; j<numpoints; j++) {
    point= points + j*dim;
    for (k=dim; k--; ) {
      if (j & ( 1 << k))
	point[k]= 1.0;
      else
	point[k]= -1.0;
    }
  }
} /*.makecube.*/

/*--------------------------------------------------
-makeDelaunay- set points for dim Delaunay triangulation of random points
  points is numpoints X dim.
notes:
  makeDelaunay() in user_eg2.c uses qh_setdelaunay() to project points in place.
*/
void makeDelaunay (coordT *points, int numpoints, int dim, int seed) {
  int j,k;
  coordT *point, realr;


  printf ("seed: %d\n", seed);
  qh_RANDOMseed_( seed);
  for (j=0; j<numpoints; j++) {
    point= points + j*dim;
    for (k= 0; k < dim; k++) {
      realr= qh_RANDOMint;
      point[k]= 2.0 * realr/(qh_RANDOMmax+1) - 1.0;
    }
  }
} /*.makeDelaunay.*/

/*--------------------------------------------------
-findDelaunay- find Delaunay triangle for [0.5,0.5,...]
  assumes dim < 100
notes:
  calls qh_setdelaunay() to project the point to a parabaloid
*/
void findDelaunay (int dim) {
  int k;
  coordT point[ 100];
  boolT isoutside;
  realT bestdist;
  facetT *facet;
  vertexT *vertex, **vertexp;

  for (k= 0; k < dim; k++) 
    point[k]= 0.5;
  qh_setdelaunay (dim+1, 1, point);
  facet= qh_findbestfacet (point, qh_ALL, &bestdist, &isoutside);
  FOREACHvertex_(facet->vertices) {
    for (k=0; k < dim; k++)
      printf ("%5.2f ", vertex->point[k]);
    printf ("\n");
  }
} /*.findDelaunay.*/

/*--------------------------------------------------
-makehalf- set points to halfspaces for a (dim)-dimensional diamond
  points is numpoints X dim+1

  each halfspace consists of dim coefficients followed by an offset
*/
void makehalf (coordT *points, int numpoints, int dim) {
  int j,k;
  coordT *point;

  for (j=0; j<numpoints; j++) {
    point= points + j*(dim+1);
    point[dim]= -1.0; /* offset */
    for (k=dim; k--; ) {
      if (j & ( 1 << k))
	point[k]= 1.0;
      else
	point[k]= -1.0;
    }
  }
} /*.makehalf.*/

#define DIM 3     /* dimension of points, must be < 31 for SIZEcube */
#define SIZEcube (1<<DIM)
#define SIZEdiamond (2*DIM)
#define TOTpoints (SIZEcube + SIZEdiamond)

/*--------------------------------------------------
-main- derived from call_qhull in user.c

  see program header

  this contains three runs of Qhull for convex hull, Delaunay
  triangulation or Voronoi vertices, and halfspace intersection

*/
int main (int argc, char *argv[]) {
  int dim= DIM;	            /* dimension of points */
  int numpoints;            /* number of points */
  coordT points[(DIM+1)*TOTpoints]; /* array of coordinates for each point */
  coordT *rows[TOTpoints];
  boolT ismalloc= False;    /* True if qhull should free points in qh_freeqhull() or reallocation */
  char flags[250];          /* option flags for qhull, see qh_opt.htm */
  FILE *outfile= stdout;    /* output from qh_produce_output()
			       use NULL to skip qh_produce_output() */
  FILE *errfile= stderr;    /* error messages from qhull code */
  int exitcode;             /* 0 if no error from qhull */
  facetT *facet;	    /* set by FORALLfacets */
  int curlong, totlong;	    /* memory remaining after qh_memfreeshort */
  int i;

  printf ("This is the output from user_eg.c\n\n\
It shows how qhull() may be called from an application.  It is not part\n\
of qhull itself.  If it appears accidently, please remove user_eg.c from\n\
your project.\n\n");

  /*
    Run 1: convex hull
  */
  printf( "\ncompute convex hull of cube after rotating input\n");
  sprintf (flags, "qhull s Tcv %s", argc >= 2 ? argv[1] : "");
  numpoints= SIZEcube;
  makecube (points, numpoints, DIM);
  for (i=numpoints; i--; )
    rows[i]= points+dim*i;
  qh_printmatrix (outfile, "input", rows, numpoints, dim);
  exitcode= qh_new_qhull (dim, numpoints, points, ismalloc,
                      flags, outfile, errfile); 
  if (!exitcode) {                  /* if no error */
    /* 'qh facet_list' contains the convex hull */
    print_summary();
    FORALLfacets {
       /* ... your code ... */
    }
  }
  qh_freeqhull(!qh_ALL);                   /* free long memory  */
  qh_memfreeshort (&curlong, &totlong);    /* free short memory and memory allocator */
  if (curlong || totlong) 
    fprintf (errfile, "qhull internal warning (user_eg, #1): did not free %d bytes of long memory (%d pieces)\n", totlong, curlong);

  /*
    Run 2: Delaunay triangulation
  */

  printf( "\ncompute 3-d Delaunay triangulation\n");
  sprintf (flags, "qhull s d Tcv %s", argc >= 3 ? argv[2] : "");
  numpoints= SIZEcube;
  makeDelaunay (points, numpoints, dim, time(NULL));
  for (i=numpoints; i--; )
    rows[i]= points+dim*i;
  qh_printmatrix (outfile, "input", rows, numpoints, dim);
  exitcode= qh_new_qhull (dim, numpoints, points, ismalloc,
                      flags, outfile, errfile); 
  if (!exitcode) {                  /* if no error */
    /* 'qh facet_list' contains the convex hull */
    /* If you want a Voronoi diagram ('v') and do not request output (i.e., outfile=NULL), 
       call qh_setvoronoi_all() after qh_new_qhull(). */
    print_summary();
    FORALLfacets {
       /* ... your code ... */
    }
    printf( "\nfind 3-d Delaunay triangle closest to [0.5, 0.5, ...]\n");
    exitcode= setjmp (qh errexit);  
    if (!exitcode) {
      /* Trap Qhull errors in findDelaunay().  Without the setjmp(), Qhull
         will exit() after reporting an error */
      qh NOerrexit= False;
      findDelaunay (DIM);
    }
    qh NOerrexit= True;
  }
#if qh_QHpointer  /* see user.h */
  {
    qhT *oldqhA, *oldqhB;
    coordT pointsB[DIM*TOTpoints]; /* array of coordinates for each point */


    printf( "\nsave first triangulation and compute a new triangulation\n");
    oldqhA= qh_save_qhull();
    sprintf (flags, "qhull s d Tcv %s", argc >= 3 ? argv[2] : "");
    numpoints= SIZEcube;
    makeDelaunay (pointsB, numpoints, dim, time(NULL)+1);
    for (i=numpoints; i--; )
      rows[i]= pointsB+dim*i;
    qh_printmatrix (outfile, "input", rows, numpoints, dim);
    exitcode= qh_new_qhull (dim, numpoints, pointsB, ismalloc,
                      flags, outfile, errfile); 
    if (!exitcode)
      print_summary();
    printf( "\nsave second triangulation and restore first one\n");
    oldqhB= qh_save_qhull();
    qh_restore_qhull (&oldqhA);
    print_summary();
    printf( "\nfree first triangulation and restore second one.\n");
    qh_freeqhull (qh_ALL);               /* free short and long memory used by first call */
			                 /* do not use qh_memfreeshort */
    qh_restore_qhull (&oldqhB);
    print_summary();
  }
#endif
  qh_freeqhull(!qh_ALL);                 /* free long memory */
  qh_memfreeshort (&curlong, &totlong);  /* free short memory and memory allocator */
  if (curlong || totlong) 
    fprintf (errfile, "qhull internal warning (user_eg, #2): did not free %d bytes of long memory (%d pieces)\n", totlong, curlong);

  /*
    Run 3: halfspace intersection about the origin
  */
  printf( "\ncompute halfspace intersection about the origin for a diamond\n");
  sprintf (flags, "qhull H0 s Tcv %s", argc >= 4 ? argv[3] : "Fp");
  numpoints= SIZEcube;
  makehalf (points, numpoints, dim);
  for (i=numpoints; i--; )
    rows[i]= points+(dim+1)*i;
  qh_printmatrix (outfile, "input as halfspace coefficients + offsets", rows, numpoints, dim+1);
  /* use qh_sethalfspace_all to transform the halfspaces yourself.  
     If so, set 'qh feasible_point and do not use option 'Hn,...' [it would retransform the halfspaces]
  */
  exitcode= qh_new_qhull (dim+1, numpoints, points, ismalloc,
                      flags, outfile, errfile); 
  if (!exitcode) 
    print_summary();
  qh_freeqhull (!qh_ALL);
  qh_memfreeshort (&curlong, &totlong);
  if (curlong || totlong)  /* could also check previous runs */
    fprintf (stderr, "qhull internal warning (user_eg, #3): did not free %d bytes of long memory (%d pieces)\n",
       totlong, curlong);
  return exitcode;
} /* main */

