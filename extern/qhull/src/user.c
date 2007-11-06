/*<html><pre>  -<a                             href="qh-user.htm"
  >-------------------------------</a><a name="TOP">-</a>

   user.c 
   user redefinable functions

   see README.txt  see COPYING.txt for copyright information.

   see qhull.h for data structures, macros, and user-callable functions.

   see user_eg.c, unix.c, and qhull_interface.cpp for examples.

   see user.h for user-definable constants

      use qh_NOmem in mem.h to turn off memory management
      use qh_NOmerge in user.h to turn off facet merging
      set qh_KEEPstatistics in user.h to 0 to turn off statistics

   This is unsupported software.  You're welcome to make changes,
   but you're on your own if something goes wrong.  Use 'Tc' to
   check frequently.  Usually qhull will report an error if 
   a data structure becomes inconsistent.  If so, it also reports
   the last point added to the hull, e.g., 102.  You can then trace
   the execution of qhull with "T4P102".  

   Please report any errors that you fix to qhull@geom.umn.edu

   call_qhull is a template for calling qhull from within your application

   if you recompile and load this module, then user.o will not be loaded
   from qhull.a

   you can add additional quick allocation sizes in qh_user_memsizes

   if the other functions here are redefined to not use qh_print...,
   then io.o will not be loaded from qhull.a.  See user_eg.c for an
   example.  We recommend keeping io.o for the extra debugging 
   information it supplies.
*/

#include "qhull_a.h" 

/*-<a                             href="qh-user.htm#TOC"
  >-------------------------------</a><a name="call_qhull">-</a>

  qh_call_qhull( void )
    template for calling qhull from inside your program
    remove #if 0, #endif to compile

  returns: 
    exit code (see qh_ERR... in qhull.h)
    all memory freed

  notes:
    This can be called any number of times.  

  see:
    qh_call_qhull_once()
    
*/
#if 0
{
  int dim;	            /* dimension of points */
  int numpoints;            /* number of points */
  coordT *points;           /* array of coordinates for each point */
  boolT ismalloc;           /* True if qhull should free points in qh_freeqhull() or reallocation */
  char flags[]= "qhull Tv"; /* option flags for qhull, see qh_opt.htm */
  FILE *outfile= stdout;    /* output from qh_produce_output()
			       use NULL to skip qh_produce_output() */
  FILE *errfile= stderr;    /* error messages from qhull code */
  int exitcode;             /* 0 if no error from qhull */
  facetT *facet;	    /* set by FORALLfacets */
  int curlong, totlong;	    /* memory remaining after qh_memfreeshort */

  /* initialize dim, numpoints, points[], ismalloc here */
  exitcode= qh_new_qhull (dim, numpoints, points, ismalloc,
                      flags, outfile, errfile); 
  if (!exitcode) {                  /* if no error */
    /* 'qh facet_list' contains the convex hull */
    FORALLfacets {
       /* ... your code ... */
    }
  }
  qh_freeqhull(!qh_ALL);
  qh_memfreeshort (&curlong, &totlong);
  if (curlong || totlong) 
    fprintf (errfile, "qhull internal warning (main): did not free %d bytes of long memory (%d pieces)\n", totlong, curlong);
}
#endif

/*-<a                             href="qh-user.htm#TOC"
  >-------------------------------</a><a name="new_qhull">-</a>

  qh_new_qhull( dim, numpoints, points, ismalloc, qhull_cmd, outfile, errfile )
    build new qhull data structure and return exitcode (0 if no errors)

  notes:
    do not modify points until finished with results.
      The qhull data structure contains pointers into the points array.
    do not call qhull functions before qh_new_qhull().
      The qhull data structure is not initialized until qh_new_qhull().

    outfile may be null
    qhull_cmd must start with "qhull "
    projects points to a new point array for Delaunay triangulations ('d' and 'v')
    transforms points into a new point array for halfspace intersection ('H')
       

  To allow multiple, concurrent calls to qhull() 
    - set qh_QHpointer in user.h
    - use qh_save_qhull and qh_restore_qhull to swap the global data structure between calls.
    - use qh_freeqhull(qh_ALL) to free intermediate convex hulls

  see:
    user_eg.c for an example
*/
int qh_new_qhull (int dim, int numpoints, coordT *points, boolT ismalloc, 
		char *qhull_cmd, FILE *outfile, FILE *errfile) {
  int exitcode, hulldim;
  boolT new_ismalloc;
  static boolT firstcall = True;
  coordT *new_points;

  if (firstcall) {
    qh_meminit (errfile);
    firstcall= False;
  }
  if (strncmp (qhull_cmd,"qhull ", 6)) {
    fprintf (errfile, "qh_new_qhull: start qhull_cmd argument with \"qhull \"\n");
    exit(1);
  }
  qh_initqhull_start (NULL, outfile, errfile);
  trace1(( qh ferr, "qh_new_qhull: build new Qhull for %d %d-d points with %s\n", numpoints, dim, qhull_cmd));
  exitcode = setjmp (qh errexit);
  if (!exitcode)
  {
    qh NOerrexit = False;
    qh_initflags (qhull_cmd);
    if (qh DELAUNAY)
      qh PROJECTdelaunay= True;
    if (qh HALFspace) {
      /* points is an array of halfspaces, 
         the last coordinate of each halfspace is its offset */
      hulldim= dim-1;
      qh_setfeasible (hulldim); 
      new_points= qh_sethalfspace_all (dim, numpoints, points, qh feasible_point);
      new_ismalloc= True;
      if (ismalloc)
	free (points);
    }else {
      hulldim= dim;
      new_points= points;
      new_ismalloc= ismalloc;
    }
    qh_init_B (new_points, numpoints, hulldim, new_ismalloc);
    qh_qhull();
    qh_check_output();
    if (outfile)
      qh_produce_output(); 
    if (qh VERIFYoutput && !qh STOPpoint && !qh STOPcone)
      qh_check_points();
  }
  qh NOerrexit = True;
  return exitcode;
} /* new_qhull */

/*-<a                             href="qh-user.htm#TOC"
  >-------------------------------</a><a name="errexit">-</a>
  
  qh_errexit( exitcode, facet, ridge )
    report and exit from an error
    report facet and ridge if non-NULL
    reports useful information such as last point processed
    set qh.FORCEoutput to print neighborhood of facet

  see: 
    qh_errexit2() in qhull.c for printing 2 facets

  design:
    check for error within error processing
    compute qh.hulltime
    print facet and ridge (if any)
    report commandString, options, qh.furthest_id
    print summary and statistics (including precision statistics)
    if qh_ERRsingular
      print help text for singular data set
    exit program via long jump (if defined) or exit()      
*/
void qh_errexit(int exitcode, facetT *facet, ridgeT *ridge) {

  if (qh ERREXITcalled) {
    fprintf (qh ferr, "\nqhull error while processing previous error.  Exit program\n");
    exit(1);
  }
  qh ERREXITcalled= True;
  if (!qh QHULLfinished)
    qh hulltime= qh_CPUclock - qh hulltime;
  qh_errprint("ERRONEOUS", facet, NULL, ridge, NULL);
  fprintf (qh ferr, "\nWhile executing: %s | %s\n", qh rbox_command, qh qhull_command);
  fprintf(qh ferr, "Options selected for Qhull %s:\n%s\n", qh_VERSION, qh qhull_options);
  if (qh furthest_id >= 0) {
    fprintf(qh ferr, "Last point added to hull was p%d.", qh furthest_id);
    if (zzval_(Ztotmerge))
      fprintf(qh ferr, "  Last merge was #%d.", zzval_(Ztotmerge));
    if (qh QHULLfinished)
      fprintf(qh ferr, "\nQhull has finished constructing the hull.");
    else if (qh POSTmerging)
      fprintf(qh ferr, "\nQhull has started post-merging.");
    fprintf (qh ferr, "\n");
  }
  if (qh FORCEoutput && (qh QHULLfinished || (!facet && !ridge)))
    qh_produce_output();
  else {
    if (exitcode != qh_ERRsingular && zzval_(Zsetplane) > qh hull_dim+1) {
      fprintf (qh ferr, "\nAt error exit:\n");
      qh_printsummary (qh ferr);
      if (qh PRINTstatistics) {
	qh_collectstatistics();
	qh_printstatistics(qh ferr, "at error exit");
	qh_memstatistics (qh ferr);
      }
    }
    if (qh PRINTprecision)
      qh_printstats (qh ferr, qhstat precision, NULL);
  }
  if (!exitcode)
    exitcode= qh_ERRqhull;
  else if (exitcode == qh_ERRsingular)
    qh_printhelp_singular(qh ferr);
  else if (exitcode == qh_ERRprec && !qh PREmerge)
    qh_printhelp_degenerate (qh ferr);
  if (qh NOerrexit) {
    fprintf (qh ferr, "qhull error while ending program.  Exit program\n");
    exit(1);
  }
  qh NOerrexit= True;
  longjmp(qh errexit, exitcode);
} /* errexit */


/*-<a                             href="qh-user.htm#TOC"
  >-------------------------------</a><a name="errprint">-</a>
  
  qh_errprint( fp, string, atfacet, otherfacet, atridge, atvertex )
    prints out the information of facets and ridges to fp
    also prints neighbors and geomview output
    
  notes:
    except for string, any parameter may be NULL
*/
void qh_errprint(char *string, facetT *atfacet, facetT *otherfacet, ridgeT *atridge, vertexT *atvertex) {
  int i;

  if (atfacet) {
    fprintf(qh ferr, "%s FACET:\n", string);
    qh_printfacet(qh ferr, atfacet);
  }
  if (otherfacet) {
    fprintf(qh ferr, "%s OTHER FACET:\n", string);
    qh_printfacet(qh ferr, otherfacet);
  }
  if (atridge) {
    fprintf(qh ferr, "%s RIDGE:\n", string);
    qh_printridge(qh ferr, atridge);
    if (atridge->top && atridge->top != atfacet && atridge->top != otherfacet)
      qh_printfacet(qh ferr, atridge->top);
    if (atridge->bottom
	&& atridge->bottom != atfacet && atridge->bottom != otherfacet)
      qh_printfacet(qh ferr, atridge->bottom);
    if (!atfacet)
      atfacet= atridge->top;
    if (!otherfacet)
      otherfacet= otherfacet_(atridge, atfacet);
  }
  if (atvertex) {
    fprintf(qh ferr, "%s VERTEX:\n", string);
    qh_printvertex (qh ferr, atvertex);
  }
  if (qh fout && qh FORCEoutput && atfacet && !qh QHULLfinished && !qh IStracing) {
    fprintf(qh ferr, "ERRONEOUS and NEIGHBORING FACETS to output\n");
    for (i= 0; i < qh_PRINTEND; i++)  /* use fout for geomview output */
      qh_printneighborhood (qh fout, qh PRINTout[i], atfacet, otherfacet,
			    !qh_ALL);
  }
} /* errprint */


/*-<a                             href="qh-user.htm#TOC"
  >-------------------------------</a><a name="printfacetlist">-</a>
  
  qh_printfacetlist( fp, facetlist, facets, printall )
    print all fields for a facet list and/or set of facets to fp
    if !printall, 
      only prints good facets

  notes:
    also prints all vertices
*/
void qh_printfacetlist(facetT *facetlist, setT *facets, boolT printall) {
  facetT *facet, **facetp;

  qh_printbegin (qh ferr, qh_PRINTfacets, facetlist, facets, printall);
  FORALLfacet_(facetlist)
    qh_printafacet(qh ferr, qh_PRINTfacets, facet, printall);
  FOREACHfacet_(facets)
    qh_printafacet(qh ferr, qh_PRINTfacets, facet, printall);
  qh_printend (qh ferr, qh_PRINTfacets, facetlist, facets, printall);
} /* printfacetlist */


/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="user_memsizes">-</a>
  
  qh_user_memsizes()
    allocate up to 10 additional, quick allocation sizes

  notes:
    increase maximum number of allocations in qh_initqhull_mem()
*/
void qh_user_memsizes (void) {

  /* qh_memsize (size); */
} /* user_memsizes */

