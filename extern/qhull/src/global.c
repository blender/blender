/*<html><pre>  -<a                             href="qh-globa.htm"
  >-------------------------------</a><a name="TOP">-</a>

   global.c
   initializes all the globals of the qhull application

   see README

   see qhull.h for qh.globals and function prototypes

   see qhull_a.h for internal functions

   copyright (c) 1993-2002, The Geometry Center
 */

#include "qhull_a.h"

/*========= qh definition =======================*/

#if qh_QHpointer
qhT *qh_qh= NULL;	/* pointer to all global variables */
#else
qhT qh_qh;     		/* all global variables.
			   Add "= {0}" if this causes a compiler error.
			   Also qh_qhstat in stat.c and qhmem in mem.c.  */
#endif

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="appendprint">-</a>

  qh_appendprint( printFormat )
    append printFormat to qh.PRINTout unless already defined
*/
void qh_appendprint (qh_PRINT format) {
  int i;

  for (i=0; i < qh_PRINTEND; i++) {
    if (qh PRINTout[i] == format && format != qh_PRINTqhull)
      break;
    if (!qh PRINTout[i]) {
      qh PRINTout[i]= format;
      break;
    }
  }
} /* appendprint */
     
/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="checkflags">-</a>
  
  qh_checkflags( commandStr, hiddenFlags )
    errors if commandStr contains hiddenFlags
    hiddenFlags starts and ends with a space and is space deliminated (checked)

  notes:
    ignores first word (e.g., "qconvex i")
    use qh_strtol/strtod since strtol/strtod may or may not skip trailing spaces
  
  see:
    qh_initflags() initializes Qhull according to commandStr
*/
void qh_checkflags(char *command, char *hiddenflags) {
  char *s= command, *t, *chkerr, key, opt, prevopt;
  char chkkey[]= "   ";
  char chkopt[]=  "    ";
  char chkopt2[]= "     ";

  if (*hiddenflags != ' ' || hiddenflags[strlen(hiddenflags)-1] != ' ') {
    fprintf(qh ferr, "qhull error (qh_checkflags): hiddenflags must start and end with a space: \"%s\"", hiddenflags);
    qh_errexit(qh_ERRinput, NULL, NULL);
  }
  if (strpbrk(hiddenflags, ",\n\r\t")) { 
    fprintf(qh ferr, "qhull error (qh_checkflags): hiddenflags contains commas, newlines, or tabs: \"%s\"", hiddenflags);
    qh_errexit(qh_ERRinput, NULL, NULL);
  }
  while (*s && !isspace(*s))  /* skip program name */
    s++;
  while (*s) {
    while (*s && isspace(*s))
      s++;
    if (*s == '-')
      s++;
    if (!*s)
      break;
    key = *s++;
    chkerr = NULL;
    if (key == '\'') {         /* TO 'file name' */
      t= strchr(s, '\'');
      if (!t) {
	fprintf(qh ferr, "qhull error (qh_checkflags): missing the 2nd single-quote for:\n%s\n", s-1);
	qh_errexit(qh_ERRinput, NULL, NULL);
      }
      s= t+1;
      continue;
    }
    chkkey[1]= key;
    if (strstr(hiddenflags, chkkey)) {
      chkerr= chkkey;
    }else if (isupper(key)) {
      opt= ' ';
      prevopt= ' ';
      chkopt[1]= key;
      chkopt2[1]= key;
      while (!chkerr && *s && !isspace(*s)) {
	opt= *s++;
	if (isalpha(opt)) {
	  chkopt[2]= opt;
	  if (strstr(hiddenflags, chkopt))
	    chkerr= chkopt;
	  if (prevopt != ' ') {
 	    chkopt2[2]= prevopt;
 	    chkopt2[3]= opt;
	    if (strstr(hiddenflags, chkopt2))
	      chkerr= chkopt2;
	  }
	}else if (key == 'Q' && isdigit(opt) && prevopt != 'b' 
	      && (prevopt == ' ' || islower(prevopt))) {
  	    chkopt[2]= opt;
	    if (strstr(hiddenflags, chkopt))
	      chkerr= chkopt;
	}else {
	  qh_strtod (s-1, &t);
	  if (s < t)
	    s= t;
	}
        prevopt= opt;
      }
    }
    if (chkerr) {
      *chkerr= '\'';
      chkerr[strlen(chkerr)-1]=  '\'';
      fprintf(qh ferr, "qhull error: option %s is not used with this program.\n             It may be used with qhull.\n", chkerr);
      qh_errexit(qh_ERRinput, NULL, NULL);
    }
  }
} /* checkflags */
    
/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="clock">-</a>
  
  qh_clock()
    return user CPU time in 100ths (qh_SECtick)
    only defined for qh_CLOCKtype == 2

  notes:
    use first value to determine time 0
    from Stevens '92 8.15
*/
unsigned long qh_clock (void) {

#if (qh_CLOCKtype == 2)
  struct tms time;
  static long clktck;  /* initialized first call */
  double ratio, cpu;
  unsigned long ticks;

  if (!clktck) {
    if ((clktck= sysconf (_SC_CLK_TCK)) < 0) {
      fprintf (qh ferr, "qhull internal error (qh_clock): sysconf() failed.  Use qh_CLOCKtype 1 in user.h\n");
      qh_errexit (qh_ERRqhull, NULL, NULL);
    }
  }
  if (times (&time) == -1) {
    fprintf (qh ferr, "qhull internal error (qh_clock): times() failed.  Use qh_CLOCKtype 1 in user.h\n");
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
  ratio= qh_SECticks / (double)clktck;
  ticks= time.tms_utime * ratio;
  return ticks;
#else
  fprintf (qh ferr, "qhull internal error (qh_clock): use qh_CLOCKtype 2 in user.h\n");
  qh_errexit (qh_ERRqhull, NULL, NULL); /* never returns */
  return 0;
#endif
} /* clock */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="freebuffers">-</a>

  qh_freebuffers()
    free up global memory buffers

  notes:
    must match qh_initbuffers()
*/
void qh_freebuffers (void) {

  trace5((qh ferr, "qh_freebuffers: freeing up global memory buffers\n"));
  /* allocated by qh_initqhull_buffers */
  qh_memfree (qh NEARzero, qh hull_dim * sizeof(realT));
  qh_memfree (qh lower_threshold, (qh input_dim+1) * sizeof(realT));
  qh_memfree (qh upper_threshold, (qh input_dim+1) * sizeof(realT));
  qh_memfree (qh lower_bound, (qh input_dim+1) * sizeof(realT));
  qh_memfree (qh upper_bound, (qh input_dim+1) * sizeof(realT));
  qh_memfree (qh gm_matrix, (qh hull_dim+1) * qh hull_dim * sizeof(coordT));
  qh_memfree (qh gm_row, (qh hull_dim+1) * sizeof(coordT *));
  qh NEARzero= qh lower_threshold= qh upper_threshold= NULL;
  qh lower_bound= qh upper_bound= NULL;
  qh gm_matrix= NULL;
  qh gm_row= NULL;
  qh_setfree (&qh other_points);
  qh_setfree (&qh del_vertices);
  qh_setfree (&qh coplanarset);
  if (qh line)                /* allocated by qh_readinput, freed if no error */
    free (qh line);
  if (qh half_space)
    free (qh half_space);
  if (qh temp_malloc)
    free (qh temp_malloc);
  if (qh feasible_point)      /* allocated by qh_readfeasible */
    free (qh feasible_point);
  if (qh feasible_string)     /* allocated by qh_initflags */
    free (qh feasible_string);
  qh line= qh feasible_string= NULL;
  qh half_space= qh feasible_point= qh temp_malloc= NULL;
  /* usually allocated by qh_readinput */
  if (qh first_point && qh POINTSmalloc) {
    free(qh first_point);
    qh first_point= NULL;
  }
  if (qh input_points && qh input_malloc) { /* set by qh_joggleinput */
    free (qh input_points);
    qh input_points= NULL;
  }
  trace5((qh ferr, "qh_freebuffers: finished\n"));
} /* freebuffers */


/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="freebuild">-</a>

  qh_freebuild( allmem )
    free global memory used by qh_initbuild and qh_buildhull
    if !allmem,
      does not free short memory (freed by qh_memfreeshort)

  design:
    free centrums
    free each vertex
    mark unattached ridges
    for each facet
      free ridges
      free outside set, coplanar set, neighbor set, ridge set, vertex set
      free facet
    free hash table
    free interior point
    free merge set
    free temporary sets
*/
void qh_freebuild (boolT allmem) {
  facetT *facet;
  vertexT *vertex;
  ridgeT *ridge, **ridgep;
  mergeT *merge, **mergep;

  trace1((qh ferr, "qh_freebuild: free memory from qh_inithull and qh_buildhull\n"));
  if (qh del_vertices)
    qh_settruncate (qh del_vertices, 0);
  if (allmem) {
    qh_clearcenters (qh_ASnone);
    while ((vertex= qh vertex_list)) {
      if (vertex->next)
        qh_delvertex (vertex);
      else {
        qh_memfree (vertex, sizeof(vertexT));
        qh newvertex_list= qh vertex_list= NULL;
      }
    }
  }else if (qh VERTEXneighbors) {
    FORALLvertices
      qh_setfreelong (&(vertex->neighbors));
  }
  qh VERTEXneighbors= False;
  qh GOODclosest= NULL;
  if (allmem) {
    FORALLfacets {
      FOREACHridge_(facet->ridges)
        ridge->seen= False;
    }
    FORALLfacets {
      if (facet->visible) {
	FOREACHridge_(facet->ridges) {
	  if (!otherfacet_(ridge, facet)->visible)
	    ridge->seen= True;  /* an unattached ridge */
	}
      }
    }
    while ((facet= qh facet_list)) {
      FOREACHridge_(facet->ridges) {
        if (ridge->seen) {
          qh_setfree(&(ridge->vertices));
          qh_memfree(ridge, sizeof(ridgeT));
        }else
          ridge->seen= True;
      }
      qh_setfree (&(facet->outsideset));
      qh_setfree (&(facet->coplanarset));
      qh_setfree (&(facet->neighbors));
      qh_setfree (&(facet->ridges));
      qh_setfree (&(facet->vertices));
      if (facet->next)
        qh_delfacet (facet);
      else {
        qh_memfree (facet, sizeof(facetT));
        qh visible_list= qh newfacet_list= qh facet_list= NULL;
      }
    }
  }else {
    FORALLfacets {
      qh_setfreelong (&(facet->outsideset));
      qh_setfreelong (&(facet->coplanarset));
      if (!facet->simplicial) {
        qh_setfreelong (&(facet->neighbors));
        qh_setfreelong (&(facet->ridges));
        qh_setfreelong (&(facet->vertices));
      }
    }
  }
  qh_setfree (&(qh hash_table));
  qh_memfree (qh interior_point, qh normal_size);
  qh interior_point= NULL;
  FOREACHmerge_(qh facet_mergeset)  /* usually empty */
    qh_memfree (merge, sizeof(mergeT));
  qh facet_mergeset= NULL;  /* temp set */
  qh degen_mergeset= NULL;  /* temp set */
  qh_settempfree_all();
} /* freebuild */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="freeqhull">-</a>

  qh_freeqhull( allmem )
    free global memory
    if !allmem,
      does not free short memory (freed by qh_memfreeshort)

  notes:
    sets qh.NOerrexit in case caller forgets to

  design:
    free global and temporary memory from qh_initbuild and qh_buildhull
    free buffers
    free statistics
*/
void qh_freeqhull (boolT allmem) {

  trace1((qh ferr, "qh_freeqhull: free global memory\n"));
  qh NOerrexit= True;  /* no more setjmp since called at exit */
  qh_freebuild (allmem);
  qh_freebuffers();
  qh_freestatistics();
#if qh_QHpointer
  free (qh_qh);
  qh_qh= NULL;
#else
  memset((char *)&qh_qh, 0, sizeof(qhT));
  qh NOerrexit= True;
#endif
} /* freeqhull */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="init_A">-</a>

  qh_init_A( infile, outfile, errfile, argc, argv )
    initialize memory and stdio files
    convert input options to option string (qh.qhull_command)

  notes:
    infile may be NULL if qh_readpoints() is not called

    errfile should always be defined.  It is used for reporting
    errors.  outfile is used for output and format options.

    argc/argv may be 0/NULL

    called before error handling initialized
    qh_errexit() may not be used
*/
void qh_init_A (FILE *infile, FILE *outfile, FILE *errfile, int argc, char *argv[]) {
  qh_meminit (errfile);
  qh_initqhull_start (infile, outfile, errfile);
  qh_init_qhull_command (argc, argv);
} /* init_A */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="init_B">-</a>

  qh_init_B( points, numpoints, dim, ismalloc )
    initialize globals for points array

    points has numpoints dim-dimensional points
      points[0] is the first coordinate of the first point
      points[1] is the second coordinate of the first point
      points[dim] is the first coordinate of the second point

    ismalloc=True
      Qhull will call free(points) on exit or input transformation
    ismalloc=False
      Qhull will allocate a new point array if needed for input transformation

    qh.qhull_command
      is the option string.
      It is defined by qh_init_B(), qh_qhull_command(), or qh_initflags

  returns:
    if qh.PROJECTinput or (qh.DELAUNAY and qh.PROJECTdelaunay)
      projects the input to a new point array

        if qh.DELAUNAY,
          qh.hull_dim is increased by one
        if qh.ATinfinity,
          qh_projectinput adds point-at-infinity for Delaunay tri.

    if qh.SCALEinput
      changes the upper and lower bounds of the input, see qh_scaleinput()

    if qh.ROTATEinput
      rotates the input by a random rotation, see qh_rotateinput()
      if qh.DELAUNAY
        rotates about the last coordinate

  notes:
    called after points are defined
    qh_errexit() may be used
*/
void qh_init_B (coordT *points, int numpoints, int dim, boolT ismalloc) {
  qh_initqhull_globals (points, numpoints, dim, ismalloc);
  if (qhmem.LASTsize == 0)
    qh_initqhull_mem();
  /* mem.c and qset.c are initialized */
  qh_initqhull_buffers();
  qh_initthresholds (qh qhull_command);
  if (qh PROJECTinput || (qh DELAUNAY && qh PROJECTdelaunay))
    qh_projectinput();
  if (qh SCALEinput)
    qh_scaleinput();
  if (qh ROTATErandom >= 0) {
    qh_randommatrix (qh gm_matrix, qh hull_dim, qh gm_row);
    if (qh DELAUNAY) {
      int k, lastk= qh hull_dim-1;
      for (k= 0; k < lastk; k++) {
        qh gm_row[k][lastk]= 0.0;
        qh gm_row[lastk][k]= 0.0;
      }
      qh gm_row[lastk][lastk]= 1.0;
    }
    qh_gram_schmidt (qh hull_dim, qh gm_row);
    qh_rotateinput (qh gm_row);
  }
} /* init_B */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="init_qhull_command">-</a>

  qh_init_qhull_command( argc, argv )
    build qh.qhull_command from argc/argv

  returns:
    a space-deliminated string of options (just as typed)

  notes:
    makes option string easy to input and output

    argc/argv may be 0/NULL
*/
void qh_init_qhull_command(int argc, char *argv[]) {
  int i;
  char *s;

  if (argc) {
    if ((s= strrchr( argv[0], '\\'))) /* Borland gives full path */
      strcpy (qh qhull_command, s+1);
    else
      strcpy (qh qhull_command, argv[0]);
    if ((s= strstr (qh qhull_command, ".EXE"))
    ||  (s= strstr (qh qhull_command, ".exe")))
      *s= '\0';
  }
  for (i=1; i < argc; i++) {
    if (strlen (qh qhull_command) + strlen(argv[i]) + 1 < sizeof(qh qhull_command)) {
      strcat (qh qhull_command, " ");
      strcat (qh qhull_command, argv[i]);
    }else {
      fprintf (qh ferr, "qhull input error: more than %d characters in command line\n",
        (int)sizeof(qh qhull_command));
      exit (1);  /* can not use qh_errexit */
    }
  }
} /* init_qhull_command */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="initflags">-</a>

  qh_initflags( commandStr )
    set flags and initialized constants from commandStr

  returns:
    sets qh.qhull_command to command if needed

  notes:
    ignores first word (e.g., "qhull d")
    use qh_strtol/strtod since strtol/strtod may or may not skip trailing spaces

  see:
    qh_initthresholds() continues processing of 'Pdn' and 'PDn'
    'prompt' in unix.c for documentation

  design:
    for each space-deliminated option group
      if top-level option
        check syntax
        append approriate option to option string
        set appropriate global variable or append printFormat to print options
      else
        for each sub-option
          check syntax
          append approriate option to option string
          set appropriate global variable or append printFormat to print options


*/
void qh_initflags(char *command) {
  int k, i, lastproject;
  char *s= command, *t, *prev_s, *start, key;
  boolT isgeom= False, wasproject;
  realT r;

  if (command != &qh qhull_command[0]) {
    *qh qhull_command= '\0';
    strncat( qh qhull_command, command, sizeof( qh qhull_command));
  }
  while (*s && !isspace(*s))  /* skip program name */
    s++;
  while (*s) {
    while (*s && isspace(*s))
      s++;
    if (*s == '-')
      s++;
    if (!*s)
      break;
    prev_s= s;
    switch (*s++) {
    case 'd':
      qh_option ("delaunay", NULL, NULL);
      qh DELAUNAY= True;
      break;
    case 'f':
      qh_option ("facets", NULL, NULL);
      qh_appendprint (qh_PRINTfacets);
      break;
    case 'i':
      qh_option ("incidence", NULL, NULL);
      qh_appendprint (qh_PRINTincidences);
      break;
    case 'm':
      qh_option ("mathematica", NULL, NULL);
      qh_appendprint (qh_PRINTmathematica);
      break;
    case 'n':
      qh_option ("normals", NULL, NULL);
      qh_appendprint (qh_PRINTnormals);
      break;
    case 'o':
      qh_option ("offFile", NULL, NULL);
      qh_appendprint (qh_PRINToff);
      break;
    case 'p':
      qh_option ("points", NULL, NULL);
      qh_appendprint (qh_PRINTpoints);
      break;
    case 's':
      qh_option ("summary", NULL, NULL);
      qh PRINTsummary= True;
      break;
    case 'v':
      qh_option ("voronoi", NULL, NULL);
      qh VORONOI= True;
      qh DELAUNAY= True;
      break;
    case 'A':
      if (!isdigit(*s) && *s != '.' && *s != '-')
	fprintf(qh ferr, "qhull warning: no maximum cosine angle given for option 'An'.  Ignored.\n");
      else {
	if (*s == '-') {
	  qh premerge_cos= -qh_strtod (s, &s);
          qh_option ("Angle-premerge-", NULL, &qh premerge_cos);
	  qh PREmerge= True;
	}else {
	  qh postmerge_cos= qh_strtod (s, &s);
          qh_option ("Angle-postmerge", NULL, &qh postmerge_cos);
	  qh POSTmerge= True;
	}
	qh MERGING= True;
      }
      break;
    case 'C':
      if (!isdigit(*s) && *s != '.' && *s != '-')
	fprintf(qh ferr, "qhull warning: no centrum radius given for option 'Cn'.  Ignored.\n");
      else {
	if (*s == '-') {
	  qh premerge_centrum= -qh_strtod (s, &s);
          qh_option ("Centrum-premerge-", NULL, &qh premerge_centrum);
	  qh PREmerge= True;
	}else {
	  qh postmerge_centrum= qh_strtod (s, &s);
          qh_option ("Centrum-postmerge", NULL, &qh postmerge_centrum);
	  qh POSTmerge= True;
	}
	qh MERGING= True;
      }
      break;
    case 'E':
      if (*s == '-')
	fprintf(qh ferr, "qhull warning: negative maximum roundoff given for option 'An'.  Ignored.\n");
      else if (!isdigit(*s))
	fprintf(qh ferr, "qhull warning: no maximum roundoff given for option 'En'.  Ignored.\n");
      else {
	qh DISTround= qh_strtod (s, &s);
        qh_option ("Distance-roundoff", NULL, &qh DISTround);
	qh SETroundoff= True;
      }
      break;
    case 'H':
      start= s;
      qh HALFspace= True;
      qh_strtod (s, &t);
      while (t > s)  {
        if (*t && !isspace (*t)) {
	  if (*t == ',')
	    t++;
	  else
	    fprintf (qh ferr, "qhull warning: origin for Halfspace intersection should be 'Hn,n,n,...'\n");
	}
        s= t;
	qh_strtod (s, &t);
      }
      if (start < t) {
        if (!(qh feasible_string= (char*)calloc (t-start+1, 1))) {
          fprintf(qh ferr, "qhull error: insufficient memory for 'Hn,n,n'\n");
          qh_errexit(qh_ERRmem, NULL, NULL);
        }
        strncpy (qh feasible_string, start, t-start);
        qh_option ("Halfspace-about", NULL, NULL);
        qh_option (qh feasible_string, NULL, NULL);
      }else
        qh_option ("Halfspace", NULL, NULL);
      break;
    case 'R':
      if (!isdigit(*s))
	fprintf(qh ferr, "qhull warning: missing random perturbation for option 'Rn'.  Ignored\n");
      else {
	qh RANDOMfactor= qh_strtod (s, &s);
        qh_option ("Random_perturb", NULL, &qh RANDOMfactor);
        qh RANDOMdist= True;
      }
      break;
    case 'V':
      if (!isdigit(*s) && *s != '-')
	fprintf(qh ferr, "qhull warning: missing visible distance for option 'Vn'.  Ignored\n");
      else {
	qh MINvisible= qh_strtod (s, &s);
        qh_option ("Visible", NULL, &qh MINvisible);
      }
      break;
    case 'U':
      if (!isdigit(*s) && *s != '-')
	fprintf(qh ferr, "qhull warning: missing coplanar distance for option 'Un'.  Ignored\n");
      else {
	qh MAXcoplanar= qh_strtod (s, &s);
        qh_option ("U-coplanar", NULL, &qh MAXcoplanar);
      }
      break;
    case 'W':
      if (*s == '-')
	fprintf(qh ferr, "qhull warning: negative outside width for option 'Wn'.  Ignored.\n");
      else if (!isdigit(*s))
	fprintf(qh ferr, "qhull warning: missing outside width for option 'Wn'.  Ignored\n");
      else {
	qh MINoutside= qh_strtod (s, &s);
        qh_option ("W-outside", NULL, &qh MINoutside);
        qh APPROXhull= True;
      }
      break;
    /************  sub menus ***************/
    case 'F':
      while (*s && !isspace(*s)) {
	switch(*s++) {
	case 'a':
	  qh_option ("Farea", NULL, NULL);
	  qh_appendprint (qh_PRINTarea);
	  qh GETarea= True;
	  break;
	case 'A':
	  qh_option ("FArea-total", NULL, NULL);
	  qh GETarea= True;
	  break;
        case 'c':
          qh_option ("Fcoplanars", NULL, NULL);
          qh_appendprint (qh_PRINTcoplanars);
          break;
        case 'C':
          qh_option ("FCentrums", NULL, NULL);
          qh_appendprint (qh_PRINTcentrums);
          break;
	case 'd':
          qh_option ("Fd-cdd-in", NULL, NULL);
	  qh CDDinput= True;
	  break;
	case 'D':
          qh_option ("FD-cdd-out", NULL, NULL);
	  qh CDDoutput= True;
	  break;
	case 'F':
	  qh_option ("FFacets-xridge", NULL, NULL);
          qh_appendprint (qh_PRINTfacets_xridge);
	  break;
        case 'i':
          qh_option ("Finner", NULL, NULL);
          qh_appendprint (qh_PRINTinner);
          break;
        case 'I':
          qh_option ("FIDs", NULL, NULL);
          qh_appendprint (qh_PRINTids);
          break;
        case 'm':
          qh_option ("Fmerges", NULL, NULL);
          qh_appendprint (qh_PRINTmerges);
          break;
        case 'n':
          qh_option ("Fneighbors", NULL, NULL);
          qh_appendprint (qh_PRINTneighbors);
          break;
        case 'N':
          qh_option ("FNeighbors-vertex", NULL, NULL);
          qh_appendprint (qh_PRINTvneighbors);
          break;
        case 'o':
          qh_option ("Fouter", NULL, NULL);
          qh_appendprint (qh_PRINTouter);
          break;
	case 'O':
	  if (qh PRINToptions1st) {
	    qh_option ("FOptions", NULL, NULL);
	    qh_appendprint (qh_PRINToptions);
	  }else
	    qh PRINToptions1st= True;
	  break;
	case 'p':
	  qh_option ("Fpoint-intersect", NULL, NULL);
	  qh_appendprint (qh_PRINTpointintersect);
	  break;
	case 'P':
	  qh_option ("FPoint-nearest", NULL, NULL);
	  qh_appendprint (qh_PRINTpointnearest);
	  break;
	case 'Q':
	  qh_option ("FQhull", NULL, NULL);
	  qh_appendprint (qh_PRINTqhull);
	  break;
        case 's':
          qh_option ("Fsummary", NULL, NULL);
          qh_appendprint (qh_PRINTsummary);
          break;
        case 'S':
          qh_option ("FSize", NULL, NULL);
          qh_appendprint (qh_PRINTsize);
          qh GETarea= True;
          break;
        case 't':
          qh_option ("Ftriangles", NULL, NULL);
          qh_appendprint (qh_PRINTtriangles);
          break;
        case 'v':
          /* option set in qh_initqhull_globals */
          qh_appendprint (qh_PRINTvertices);
          break;
        case 'V':
          qh_option ("FVertex-average", NULL, NULL);
          qh_appendprint (qh_PRINTaverage);
          break;
	case 'x':
	  qh_option ("Fxtremes", NULL, NULL);
	  qh_appendprint (qh_PRINTextremes);
	  break;
	default:
	  s--;
	  fprintf (qh ferr, "qhull warning: unknown 'F' output option %c, rest ignored\n", (int)s[0]);
	  while (*++s && !isspace(*s));
	  break;
	}
      }
      break;
    case 'G':
      isgeom= True;
      qh_appendprint (qh_PRINTgeom);
      while (*s && !isspace(*s)) {
	switch(*s++) {
        case 'a':
          qh_option ("Gall-points", NULL, NULL);
          qh PRINTdots= True;
          break;
        case 'c':
          qh_option ("Gcentrums", NULL, NULL);
          qh PRINTcentrums= True;
          break;
	case 'h':
          qh_option ("Gintersections", NULL, NULL);
	  qh DOintersections= True;
	  break;
	case 'i':
          qh_option ("Ginner", NULL, NULL);
	  qh PRINTinner= True;
	  break;
	case 'n':
          qh_option ("Gno-planes", NULL, NULL);
	  qh PRINTnoplanes= True;
	  break;
	case 'o':
          qh_option ("Gouter", NULL, NULL);
	  qh PRINTouter= True;
	  break;
	case 'p':
          qh_option ("Gpoints", NULL, NULL);
	  qh PRINTcoplanar= True;
	  break;
	case 'r':
          qh_option ("Gridges", NULL, NULL);
	  qh PRINTridges= True;
	  break;
	case 't':
          qh_option ("Gtransparent", NULL, NULL);
	  qh PRINTtransparent= True;
	  break;
	case 'v':
          qh_option ("Gvertices", NULL, NULL);
	  qh PRINTspheres= True;
	  break;
	case 'D':
	  if (!isdigit (*s))
	    fprintf (qh ferr, "qhull input error: missing dimension for option 'GDn'\n");
	  else {
	    if (qh DROPdim >= 0)
	      fprintf (qh ferr, "qhull warning: can only drop one dimension.  Previous 'GD%d' ignored\n",
	           qh DROPdim);
  	    qh DROPdim= qh_strtol (s, &s);
            qh_option ("GDrop-dim", &qh DROPdim, NULL);
          }
	  break;
	default:
	  s--;
	  fprintf (qh ferr, "qhull warning: unknown 'G' print option %c, rest ignored\n", (int)s[0]);
	  while (*++s && !isspace(*s));
	  break;
	}
      }
      break;
    case 'P':
      while (*s && !isspace(*s)) {
	switch(*s++) {
	case 'd': case 'D':  /* see qh_initthresholds() */
	  key= s[-1];
	  i= qh_strtol (s, &s);
	  r= 0;
	  if (*s == ':') {
	    s++;
	    r= qh_strtod (s, &s);
	  }
	  if (key == 'd')
  	    qh_option ("Pdrop-facets-dim-less", &i, &r);
  	  else
  	    qh_option ("PDrop-facets-dim-more", &i, &r);
	  break;
        case 'g':
          qh_option ("Pgood-facets", NULL, NULL);
          qh PRINTgood= True;
          break;
        case 'G':
          qh_option ("PGood-facet-neighbors", NULL, NULL);
          qh PRINTneighbors= True;
          break;
        case 'o':
          qh_option ("Poutput-forced", NULL, NULL);
          qh FORCEoutput= True;
          break;
        case 'p':
          qh_option ("Pprecision-ignore", NULL, NULL);
          qh PRINTprecision= False;
          break;
	case 'A':
	  if (!isdigit (*s))
	    fprintf (qh ferr, "qhull input error: missing facet count for keep area option 'PAn'\n");
	  else {
  	    qh KEEParea= qh_strtol (s, &s);
            qh_option ("PArea-keep", &qh KEEParea, NULL);
            qh GETarea= True;
          }
	  break;
	case 'F':
	  if (!isdigit (*s))
	    fprintf (qh ferr, "qhull input error: missing facet area for option 'PFn'\n");
	  else {
  	    qh KEEPminArea= qh_strtod (s, &s);
            qh_option ("PFacet-area-keep", NULL, &qh KEEPminArea);
            qh GETarea= True;
          }
	  break;
	case 'M':
	  if (!isdigit (*s))
	    fprintf (qh ferr, "qhull input error: missing merge count for option 'PMn'\n");
	  else {
  	    qh KEEPmerge= qh_strtol (s, &s);
            qh_option ("PMerge-keep", &qh KEEPmerge, NULL);
          }
	  break;
	default:
	  s--;
	  fprintf (qh ferr, "qhull warning: unknown 'P' print option %c, rest ignored\n", (int)s[0]);
	  while (*++s && !isspace(*s));
	  break;
	}
      }
      break;
    case 'Q':
      lastproject= -1;
      while (*s && !isspace(*s)) {
	switch(*s++) {
	case 'b': case 'B':  /* handled by qh_initthresholds */
	  key= s[-1];
	  if (key == 'b' && *s == 'B') {
	    s++;
	    r= qh_DEFAULTbox;
	    qh SCALEinput= True;
	    qh_option ("QbBound-unit-box", NULL, &r);
	    break;
	  }
	  if (key == 'b' && *s == 'b') {
	    s++;
	    qh SCALElast= True;
	    qh_option ("Qbbound-last", NULL, NULL);
	    break;
	  }
	  k= qh_strtol (s, &s);
	  r= 0.0;
	  wasproject= False;
	  if (*s == ':') {
	    s++;
	    if ((r= qh_strtod(s, &s)) == 0.0) {
 	      t= s;            /* need true dimension for memory allocation */
	      while (*t && !isspace(*t)) {
	        if (toupper(*t++) == 'B'
	         && k == qh_strtol (t, &t)
	         && *t++ == ':'
	         && qh_strtod(t, &t) == 0.0) {
	          qh PROJECTinput++;
	          trace2((qh ferr, "qh_initflags: project dimension %d\n", k));
	          qh_option ("Qb-project-dim", &k, NULL);
		  wasproject= True;
	          lastproject= k;
	          break;
		}
	      }
	    }
  	  }
	  if (!wasproject) {
	    if (lastproject == k && r == 0.0)
	      lastproject= -1;  /* doesn't catch all possible sequences */
	    else if (key == 'b') {
	      qh SCALEinput= True;
	      if (r == 0.0)
		r= -qh_DEFAULTbox;
	      qh_option ("Qbound-dim-low", &k, &r);
	    }else {
	      qh SCALEinput= True;
	      if (r == 0.0)
		r= qh_DEFAULTbox;
	      qh_option ("QBound-dim-high", &k, &r);
	    }
	  }
	  break;
	case 'c':
	  qh_option ("Qcoplanar-keep", NULL, NULL);
	  qh KEEPcoplanar= True;
	  break;
	case 'f':
	  qh_option ("Qfurthest-outside", NULL, NULL);
	  qh BESToutside= True;
	  break;
	case 'g':
	  qh_option ("Qgood-facets-only", NULL, NULL);
	  qh ONLYgood= True;
	  break;
	case 'i':
	  qh_option ("Qinterior-keep", NULL, NULL);
	  qh KEEPinside= True;
	  break;
	case 'm':
	  qh_option ("Qmax-outside-only", NULL, NULL);
	  qh ONLYmax= True;
	  break;
	case 'r':
	  qh_option ("Qrandom-outside", NULL, NULL);
	  qh RANDOMoutside= True;
	  break;
	case 's':
	  qh_option ("Qsearch-initial-simplex", NULL, NULL);
	  qh ALLpoints= True;
	  break;
	case 't':
	  qh_option ("Qtriangulate", NULL, NULL);
	  qh TRIangulate= True;
	  break;
	case 'T':
	  qh_option ("QTestPoints", NULL, NULL);
	  if (!isdigit (*s))
	    fprintf (qh ferr, "qhull input error: missing number of test points for option 'QTn'\n");
	  else {
  	    qh TESTpoints= qh_strtol (s, &s);
            qh_option ("QTestPoints", &qh TESTpoints, NULL);
          }
	  break;
	case 'u':
	  qh_option ("QupperDelaunay", NULL, NULL);
	  qh UPPERdelaunay= True;
	  break;
	case 'v':
	  qh_option ("Qvertex-neighbors-convex", NULL, NULL);
	  qh TESTvneighbors= True;
	  break;
	case 'x':
	  qh_option ("Qxact-merge", NULL, NULL);
	  qh MERGEexact= True;
	  break;
	case 'z':
	  qh_option ("Qz-infinity-point", NULL, NULL);
	  qh ATinfinity= True;
	  break;
	case '0':
	  qh_option ("Q0-no-premerge", NULL, NULL);
	  qh NOpremerge= True;
	  break;
	case '1':
	  if (!isdigit(*s)) {
	    qh_option ("Q1-no-angle-sort", NULL, NULL);
	    qh ANGLEmerge= False;
	    break; 
	  }
	  switch(*s++) {
  	  case '0':
	    qh_option ("Q10-no-narrow", NULL, NULL);
	    qh NOnarrow= True;
	    break; 
  	  case '1':
	    qh_option ("Q11-trinormals Qtriangulate", NULL, NULL);
	    qh TRInormals= True;
	    qh TRIangulate= True;
	    break; 
	  default:
	    s--;
	    fprintf (qh ferr, "qhull warning: unknown 'Q' qhull option 1%c, rest ignored\n", (int)s[0]);
	    while (*++s && !isspace(*s));
	    break;
	  }
	  break;
	case '2':
	  qh_option ("Q2-no-merge-independent", NULL, NULL);
	  qh MERGEindependent= False;
	  goto LABELcheckdigit;
	  break; /* no warnings */
	case '3':
	  qh_option ("Q3-no-merge-vertices", NULL, NULL);
	  qh MERGEvertices= False;
	LABELcheckdigit:
	  if (isdigit(*s))
	    fprintf (qh ferr, "qhull warning: can not follow '1', '2', or '3' with a digit.  '%c' skipped.\n",
	             *s++);
	  break;
	case '4':
	  qh_option ("Q4-avoid-old-into-new", NULL, NULL);
	  qh AVOIDold= True;
	  break;
	case '5':
	  qh_option ("Q5-no-check-outer", NULL, NULL);
	  qh SKIPcheckmax= True;
	  break;
	case '6':
	  qh_option ("Q6-no-concave-merge", NULL, NULL);
	  qh SKIPconvex= True;
	  break;
	case '7':
	  qh_option ("Q7-no-breadth-first", NULL, NULL);
	  qh VIRTUALmemory= True;
	  break;
	case '8':
	  qh_option ("Q8-no-near-inside", NULL, NULL);
	  qh NOnearinside= True;
	  break;
	case '9':
	  qh_option ("Q9-pick-furthest", NULL, NULL);
	  qh PICKfurthest= True;
	  break;
	case 'G':
	  i= qh_strtol (s, &t);
	  if (qh GOODpoint)
	    fprintf (qh ferr, "qhull warning: good point already defined for option 'QGn'.  Ignored\n");
          else if (s == t)
	    fprintf (qh ferr, "qhull warning: missing good point id for option 'QGn'.  Ignored\n");
	  else if (i < 0 || *s == '-') {
 	    qh GOODpoint= i-1;
  	    qh_option ("QGood-if-dont-see-point", &i, NULL);
	  }else {
 	    qh GOODpoint= i+1;
  	    qh_option ("QGood-if-see-point", &i, NULL);
  	  }
 	  s= t;
	  break;
	case 'J':
          if (!isdigit(*s) && *s != '-')
   	    qh JOGGLEmax= 0.0;
	  else {
 	    qh JOGGLEmax= (realT) qh_strtod (s, &s);
            qh_option ("QJoggle", NULL, &qh JOGGLEmax);
	  }
	  break;
	case 'R':
          if (!isdigit(*s) && *s != '-')
	    fprintf (qh ferr, "qhull warning: missing random seed for option 'QRn'.  Ignored\n");
	  else {
 	    qh ROTATErandom= i= qh_strtol(s, &s);
   	    if (i > 0)
   	      qh_option ("QRotate-id", &i, NULL );
	    else if (i < -1)
   	      qh_option ("QRandom-seed", &i, NULL );
          }
	  break;
	case 'V':
	  i= qh_strtol (s, &t);
	  if (qh GOODvertex)
	    fprintf (qh ferr, "qhull warning: good vertex already defined for option 'QVn'.  Ignored\n");
          else if (s == t)
	    fprintf (qh ferr, "qhull warning: no good point id given for option 'QVn'.  Ignored\n");
	  else if (i < 0) {
 	    qh GOODvertex= i - 1;
 	    qh_option ("QV-good-facets-not-point", &i, NULL);
	  }else {
  	    qh_option ("QV-good-facets-point", &i, NULL);
	    qh GOODvertex= i + 1;
          }
 	  s= t;
	  break;
	default:
	  s--;
	  fprintf (qh ferr, "qhull warning: unknown 'Q' qhull option %c, rest ignored\n", (int)s[0]);
	  while (*++s && !isspace(*s));
	  break;
	}
      }
      break;
    case 'T':
      while (*s && !isspace(*s)) {
	if (isdigit(*s) || *s == '-')
	  qh IStracing= qh_strtol(s, &s);
	else switch(*s++) {
	case 'c':
          qh_option ("Tcheck-frequently", NULL, NULL);
	  qh CHECKfrequently= True;
	  break;
	case 's':
          qh_option ("Tstatistics", NULL, NULL);
	  qh PRINTstatistics= True;
	  break;
	case 'v':
          qh_option ("Tverify", NULL, NULL);
	  qh VERIFYoutput= True;
	  break;
	case 'z':
	  if (!qh fout)
	    fprintf (qh ferr, "qhull warning: output file undefined (stdout).  Option 'Tz' ignored.\n");
	  else {
	    qh_option ("Tz-stdout", NULL, NULL);
  	    qh ferr= qh fout;
  	    qhmem.ferr= qh fout;
	  }
	  break;
	case 'C':
	  if (!isdigit(*s))
	    fprintf (qh ferr, "qhull warning: missing point id for cone for trace option 'TCn'.  Ignored\n");
	  else {
	    i= qh_strtol (s, &s);
	    qh_option ("TCone-stop", &i, NULL);
	    qh STOPcone= i + 1;
          }
	  break;
	case 'F':
	  if (!isdigit(*s))
	    fprintf (qh ferr, "qhull warning: missing frequency count for trace option 'TFn'.  Ignored\n");
	  else {
	    qh REPORTfreq= qh_strtol (s, &s);
            qh_option ("TFacet-log", &qh REPORTfreq, NULL);
	    qh REPORTfreq2= qh REPORTfreq/2;  /* for tracemerging() */
	  }
	  break;
	case 'I':
	  if (s[0] != ' ' || s[1] == '\"' || s[1] == '\'' ||isspace (s[1])) {
	    s++;
	    fprintf (qh ferr, "qhull warning: option 'TI' mistyped.\nUse 'TI', one space, file name, and space or end-of-line.\nDo not use quotes.  Option 'FI' ignored.\n");
	  }else {  /* not a procedure because of qh_option (filename, NULL, NULL); */
	    char filename[500], *t= filename;

	    s++;
	    while (*s) {
	      if (t - filename >= sizeof (filename)-2) {
		fprintf (qh ferr, "qhull error: filename for 'TI' too long.\n");
		qh_errexit (qh_ERRinput, NULL, NULL);
	      }
	      if (isspace (*s))
		break;
	      *(t++)= *s++;
	    }
	    *t= '\0';
	    if (!freopen (filename, "r", stdin)) {
	      fprintf (qh ferr, "qhull error: could not open file \"%s\".", filename);
	      qh_errexit (qh_ERRinput, NULL, NULL);
	    }else {
	      qh_option ("TInput-file", NULL, NULL);
	      qh_option (filename, NULL, NULL);
	    }
	  }
	  break;
	case 'O':
	  if (s[0] != ' ' || s[1] == '\"' || isspace (s[1])) {
	    s++;
	    fprintf (qh ferr, "qhull warning: option 'TO' mistyped.\nUse 'TO', one space, file name, and space or end-of-line.\nThe file name may be enclosed in single quotes.\nDo not use double quotes.  Option 'FO' ignored.\n");
	  }else {  /* not a procedure because of qh_option (filename, NULL, NULL); */
	    char filename[500], *t= filename;
	    boolT isquote= False;

	    s++;
	    if (*s == '\'') {
	      isquote= True;
	      s++;
	    }
	    while (*s) {
	      if (t - filename >= sizeof (filename)-2) {
		fprintf (qh ferr, "qhull error: filename for 'TO' too long.\n");
		qh_errexit (qh_ERRinput, NULL, NULL);
	      }
	      if (isquote) {
		if (*s == '\'') {
		  s++;
		  isquote= False;
		  break;
		}
	      }else if (isspace (*s))
		break;
	      *(t++)= *s++;
	    }
	    *t= '\0';
	    if (isquote)
	      fprintf (qh ferr, "qhull error: missing end quote for option 'TO'.  Rest of line ignored.\n");
	    else if (!freopen (filename, "w", stdout)) {
	      fprintf (qh ferr, "qhull error: could not open file \"%s\".", filename);
	      qh_errexit (qh_ERRinput, NULL, NULL);
	    }else {
	      qh_option ("TOutput-file", NULL, NULL);
	      qh_option (filename, NULL, NULL);
	    }
	  }
	  break;
	case 'P':
	  if (!isdigit(*s))
	    fprintf (qh ferr, "qhull warning: missing point id for trace option 'TPn'.  Ignored\n");
	  else {
	    qh TRACEpoint= qh_strtol (s, &s);
            qh_option ("Trace-point", &qh TRACEpoint, NULL);
          }
	  break;
	case 'M':
	  if (!isdigit(*s))
	    fprintf (qh ferr, "qhull warning: missing merge id for trace option 'TMn'.  Ignored\n");
	  else {
	    qh TRACEmerge= qh_strtol (s, &s);
            qh_option ("Trace-merge", &qh TRACEmerge, NULL);
          }
	  break;
	case 'R':
	  if (!isdigit(*s))
	    fprintf (qh ferr, "qhull warning: missing rerun count for trace option 'TRn'.  Ignored\n");
	  else {
	    qh RERUN= qh_strtol (s, &s);
            qh_option ("TRerun", &qh RERUN, NULL);
          }
	  break;
	case 'V':
	  i= qh_strtol (s, &t);
	  if (s == t)
	    fprintf (qh ferr, "qhull warning: missing furthest point id for trace option 'TVn'.  Ignored\n");
	  else if (i < 0) {
	    qh STOPpoint= i - 1;
            qh_option ("TV-stop-before-point", &i, NULL);
	  }else {
	    qh STOPpoint= i + 1;
            qh_option ("TV-stop-after-point", &i, NULL);
          }
          s= t;
	  break;
	case 'W':
	  if (!isdigit(*s))
	    fprintf (qh ferr, "qhull warning: missing max width for trace option 'TWn'.  Ignored\n");
	  else {
 	    qh TRACEdist= (realT) qh_strtod (s, &s);
            qh_option ("TWide-trace", NULL, &qh TRACEdist);
          }
	  break;
	default:
	  s--;
	  fprintf (qh ferr, "qhull warning: unknown 'T' trace option %c, rest ignored\n", (int)s[0]);
	  while (*++s && !isspace(*s));
	  break;
	}
      }
      break;
    default:
      fprintf (qh ferr, "qhull warning: unknown flag %c (%x)\n", (int)s[-1],
	       (int)s[-1]);
      break;
    }
    if (s-1 == prev_s && *s && !isspace(*s)) {
      fprintf (qh ferr, "qhull warning: missing space after flag %c (%x); reserved for menu. Skipped.\n",
	       (int)*prev_s, (int)*prev_s);
      while (*s && !isspace(*s))
	s++;
    }
  }
  if (isgeom && !qh FORCEoutput && qh PRINTout[1])
    fprintf (qh ferr, "qhull warning: additional output formats are not compatible with Geomview\n");
  /* set derived values in qh_initqhull_globals */
} /* initflags */


/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="initqhull_buffers">-</a>

  qh_initqhull_buffers()
    initialize global memory buffers

  notes:
    must match qh_freebuffers()
*/
void qh_initqhull_buffers (void) {
  int k;

  qh TEMPsize= (qhmem.LASTsize - sizeof (setT))/SETelemsize;
  if (qh TEMPsize <= 0 || qh TEMPsize > qhmem.LASTsize)
    qh TEMPsize= 8;  /* e.g., if qh_NOmem */
  qh other_points= qh_setnew (qh TEMPsize);
  qh del_vertices= qh_setnew (qh TEMPsize);
  qh coplanarset= qh_setnew (qh TEMPsize);
  qh NEARzero= (realT *)qh_memalloc(qh hull_dim * sizeof(realT));
  qh lower_threshold= (realT *)qh_memalloc((qh input_dim+1) * sizeof(realT));
  qh upper_threshold= (realT *)qh_memalloc((qh input_dim+1) * sizeof(realT));
  qh lower_bound= (realT *)qh_memalloc((qh input_dim+1) * sizeof(realT));
  qh upper_bound= (realT *)qh_memalloc((qh input_dim+1) * sizeof(realT));
  for(k= qh input_dim+1; k--; ) {
    qh lower_threshold[k]= -REALmax;
    qh upper_threshold[k]= REALmax;
    qh lower_bound[k]= -REALmax;
    qh upper_bound[k]= REALmax;
  }
  qh gm_matrix= (coordT *)qh_memalloc((qh hull_dim+1) * qh hull_dim * sizeof(coordT));
  qh gm_row= (coordT **)qh_memalloc((qh hull_dim+1) * sizeof(coordT *));
} /* initqhull_buffers */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="initqhull_globals">-</a>

  qh_initqhull_globals( points, numpoints, dim, ismalloc )
    initialize globals
    if ismalloc
      points were malloc'd and qhull should free at end

  returns:
    sets qh.first_point, num_points, input_dim, hull_dim and others
    seeds random number generator (seed=1 if tracing)
    modifies qh.hull_dim if ((qh.DELAUNAY and qh.PROJECTdelaunay) or qh.PROJECTinput)
    adjust user flags as needed
    also checks DIM3 dependencies and constants

  notes:
    do not use qh_point() since an input transformation may move them elsewhere

  see:
    qh_initqhull_start() sets default values for non-zero globals

  design:
    initialize points array from input arguments
    test for qh.ZEROcentrum
      (i.e., use opposite vertex instead of cetrum for convexity testing)
    test for qh.PRINTgood (i.e., only print 'good' facets)
    initialize qh.CENTERtype, qh.normal_size,
      qh.center_size, qh.TRACEpoint/level,
    initialize and test random numbers
    check for conflicting print output options
*/
void qh_initqhull_globals (coordT *points, int numpoints, int dim, boolT ismalloc) {
  int seed, pointsneeded, extra= 0, i, randi, k;
  boolT printgeom= False, printmath= False, printcoplanar= False;
  realT randr;
  realT factorial;

  time_t timedata;

  trace0((qh ferr, "qh_initqhull_globals: for %s | %s\n", qh rbox_command,
      qh qhull_command));
  qh POINTSmalloc= ismalloc;
  qh first_point= points;
  qh num_points= numpoints;
  qh hull_dim= qh input_dim= dim;
  if (!qh NOpremerge && !qh MERGEexact && !qh PREmerge && qh JOGGLEmax > REALmax/2) {
    qh MERGING= True;
    if (qh hull_dim <= 4) {
      qh PREmerge= True;
      qh_option ("_pre-merge", NULL, NULL);
    }else {
      qh MERGEexact= True;
      qh_option ("Qxact_merge", NULL, NULL);
    }
  }else if (qh MERGEexact) 
    qh MERGING= True;
  if (!qh NOpremerge && qh JOGGLEmax > REALmax/2) {
#ifdef qh_NOmerge
    qh JOGGLEmax= 0.0;
#endif
  }
  if (qh TRIangulate && qh JOGGLEmax < REALmax/2 && qh PRINTprecision)
    fprintf(qh ferr, "qhull warning: joggle ('QJ') always produces simplicial output.  Triangulated output ('Qt') does nothing.\n");
  if (qh JOGGLEmax < REALmax/2 && qh DELAUNAY && !qh SCALEinput && !qh SCALElast) {
    qh SCALElast= True;
    qh_option ("Qbbound-last-qj", NULL, NULL);
  }
  if (qh MERGING && !qh POSTmerge && qh premerge_cos > REALmax/2
  && qh premerge_centrum == 0) {
    qh ZEROcentrum= True;
    qh ZEROall_ok= True;
    qh_option ("_zero-centrum", NULL, NULL);
  }
  if (qh JOGGLEmax < REALmax/2 && REALepsilon > 2e-8 && qh PRINTprecision)
    fprintf(qh ferr, "qhull warning: real epsilon, %2.2g, is probably too large for joggle ('QJn')\nRecompile with double precision reals (see user.h).\n",
          REALepsilon);
#ifdef qh_NOmerge
  if (qh MERGING) {
    fprintf (qh ferr, "qhull input error: merging not installed (qh_NOmerge + 'Qx', 'Cn' or 'An')\n");
    qh_errexit (qh_ERRinput, NULL, NULL);
  }
#endif
  if (!(qh PRINTgood || qh PRINTneighbors)) {
    if (qh KEEParea || qh KEEPminArea < REALmax/2 || qh KEEPmerge || qh DELAUNAY
	|| (!qh ONLYgood && (qh GOODvertex || qh GOODpoint))) {
      qh PRINTgood= True;
      qh_option ("Pgood", NULL, NULL);
    }
  }
  if (qh DELAUNAY && qh KEEPcoplanar && !qh KEEPinside) {
    qh KEEPinside= True;
    qh_option ("Qinterior-keep", NULL, NULL);
  }
  if (qh DELAUNAY && qh HALFspace) {
    fprintf (qh ferr, "qhull input error: can not use Delaunay ('d') or Voronoi ('v') with halfspace intersection ('H')\n");
    qh_errexit (qh_ERRinput, NULL, NULL);
  }
  if (!qh DELAUNAY && (qh UPPERdelaunay || qh ATinfinity)) {
    fprintf (qh ferr, "qhull input error: use upper-Delaunay ('Qu') or infinity-point ('Qz') with Delaunay ('d') or Voronoi ('v')\n");
    qh_errexit (qh_ERRinput, NULL, NULL);
  }
  if (qh UPPERdelaunay && qh ATinfinity) {
    fprintf (qh ferr, "qhull input error: can not use infinity-point ('Qz') with upper-Delaunay ('Qu')\n");
    qh_errexit (qh_ERRinput, NULL, NULL);
  }
  if (qh SCALElast && !qh DELAUNAY && qh PRINTprecision)
    fprintf (qh ferr, "qhull input warning: option 'Qbb' (scale-last-coordinate) is normally used with 'd' or 'v'\n");
  qh DOcheckmax= (!qh SKIPcheckmax && qh MERGING );
  qh KEEPnearinside= (qh DOcheckmax && !(qh KEEPinside && qh KEEPcoplanar) 
                          && !qh NOnearinside);
  if (qh MERGING)
    qh CENTERtype= qh_AScentrum;
  else if (qh VORONOI)
    qh CENTERtype= qh_ASvoronoi;
  if (qh TESTvneighbors && !qh MERGING) {
    fprintf(qh ferr, "qhull input error: test vertex neighbors ('Qv') needs a merge option\n");
    qh_errexit (qh_ERRinput, NULL ,NULL);
  }
  if (qh PROJECTinput || (qh DELAUNAY && qh PROJECTdelaunay)) {
    qh hull_dim -= qh PROJECTinput;
    if (qh DELAUNAY) {
      qh hull_dim++;
      extra= 1;
    }
  }
  if (qh hull_dim <= 1) {
    fprintf(qh ferr, "qhull error: dimension %d must be > 1\n", qh hull_dim);
    qh_errexit (qh_ERRinput, NULL, NULL);
  }
  for (k= 2, factorial=1.0; k < qh hull_dim; k++)
    factorial *= k;
  qh AREAfactor= 1.0 / factorial;
  trace2((qh ferr, "qh_initqhull_globals: initialize globals.  dim %d numpoints %d malloc? %d projected %d to hull_dim %d\n",
	dim, numpoints, ismalloc, qh PROJECTinput, qh hull_dim));
  qh normal_size= qh hull_dim * sizeof(coordT);
  qh center_size= qh normal_size - sizeof(coordT);
  pointsneeded= qh hull_dim+1;
  if (qh hull_dim > qh_DIMmergeVertex) {
    qh MERGEvertices= False;
    qh_option ("Q3-no-merge-vertices-dim-high", NULL, NULL);
  }
  if (qh GOODpoint)
    pointsneeded++;
#ifdef qh_NOtrace
  if (qh IStracing) {
    fprintf (qh ferr, "qhull input error: tracing is not installed (qh_NOtrace in user.h)");
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
#endif
  if (qh RERUN > 1) {
    qh TRACElastrun= qh IStracing; /* qh_build_withrestart duplicates next conditional */
    if (qh IStracing != -1)
      qh IStracing= 0;
  }else if (qh TRACEpoint != -1 || qh TRACEdist < REALmax/2 || qh TRACEmerge) {
    qh TRACElevel= (qh IStracing? qh IStracing : 3);
    qh IStracing= 0;
  }
  if (qh ROTATErandom == 0 || qh ROTATErandom == -1) {
    seed= time (&timedata);
    if (qh ROTATErandom  == -1) {
      seed= -seed;
      qh_option ("QRandom-seed", &seed, NULL );
    }else
      qh_option ("QRotate-random", &seed, NULL);
    qh ROTATErandom= seed;
  }
  seed= qh ROTATErandom;
  if (seed == INT_MIN)    /* default value */
    seed= 1;
  else if (seed < 0)
    seed= -seed;
  qh_RANDOMseed_(seed);
  randr= 0.0;
  for (i= 1000; i--; ) {
    randi= qh_RANDOMint;
    randr += randi;
    if (randi > qh_RANDOMmax) {
      fprintf (qh ferr, "\
qhull configuration error (qh_RANDOMmax in user.h):\n\
   random integer %d > qh_RANDOMmax (%.8g)\n",
	       randi, qh_RANDOMmax);
      qh_errexit (qh_ERRinput, NULL, NULL);
    }
  }
  qh_RANDOMseed_(seed);
  randr = randr/1000;
  if (randr < qh_RANDOMmax/10
  || randr > qh_RANDOMmax * 5)
    fprintf (qh ferr, "\
qhull configuration warning (qh_RANDOMmax in user.h):\n\
   average of 1000 random integers (%.2g) is much different than expected (%.2g).\n\
   Is qh_RANDOMmax (%.2g) wrong?\n",
	     randr, qh_RANDOMmax/2.0, qh_RANDOMmax);
  qh RANDOMa= 2.0 * qh RANDOMfactor/qh_RANDOMmax;
  qh RANDOMb= 1.0 - qh RANDOMfactor;
  if (qh_HASHfactor < 1.1) {
    fprintf(qh ferr, "qhull internal error (qh_initqhull_globals): qh_HASHfactor %d must be at least 1.1.  Qhull uses linear hash probing\n",
      qh_HASHfactor);
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
  if (numpoints+extra < pointsneeded) {
    fprintf(qh ferr,"qhull input error: not enough points (%d) to construct initial simplex (need %d)\n",
	    numpoints, pointsneeded);
    qh_errexit(qh_ERRinput, NULL, NULL);
  }
  if (qh PRINTtransparent) {
    if (qh hull_dim != 4 || !qh DELAUNAY || qh VORONOI || qh DROPdim >= 0) {
      fprintf(qh ferr,"qhull input error: transparent Delaunay ('Gt') needs 3-d Delaunay ('d') w/o 'GDn'\n");
      qh_errexit(qh_ERRinput, NULL, NULL);
    }
    qh DROPdim = 3;
    qh PRINTridges = True;
  }
  for (i= qh_PRINTEND; i--; ) {
    if (qh PRINTout[i] == qh_PRINTgeom)
      printgeom= True;
    else if (qh PRINTout[i] == qh_PRINTmathematica)
      printmath= True;
    else if (qh PRINTout[i] == qh_PRINTcoplanars)
      printcoplanar= True;
    else if (qh PRINTout[i] == qh_PRINTpointnearest)
      printcoplanar= True;
    else if (qh PRINTout[i] == qh_PRINTpointintersect && !qh HALFspace) {
      fprintf (qh ferr, "qhull input error: option 'Fp' is only used for \nhalfspace intersection ('Hn,n,n').\n");
      qh_errexit (qh_ERRinput, NULL, NULL);
    }else if (qh PRINTout[i] == qh_PRINTtriangles && (qh HALFspace || qh VORONOI)) {
      fprintf (qh ferr, "qhull input error: option 'Ft' is not available for Voronoi vertices or halfspace intersection\n");
      qh_errexit (qh_ERRinput, NULL, NULL);
    }else if (qh PRINTout[i] == qh_PRINTcentrums && qh VORONOI) {
      fprintf (qh ferr, "qhull input error: option 'FC' is not available for Voronoi vertices ('v')\n");
      qh_errexit (qh_ERRinput, NULL, NULL);
    }else if (qh PRINTout[i] == qh_PRINTvertices) {
      if (qh VORONOI)
        qh_option ("Fvoronoi", NULL, NULL);
      else 
        qh_option ("Fvertices", NULL, NULL);
    }
  }
  if (printcoplanar && qh DELAUNAY && qh JOGGLEmax < REALmax/2) {
    if (qh PRINTprecision) 
      fprintf (qh ferr, "qhull input warning: 'QJ' (joggle) will usually prevent coincident input sites for options 'Fc' and 'FP'\n");
  }
  if (!qh KEEPcoplanar && !qh KEEPinside && !qh ONLYgood) {
    if ((qh PRINTcoplanar && qh PRINTspheres) || printcoplanar) {
      qh KEEPcoplanar = True;
      qh_option ("Qcoplanar", NULL, NULL);
    }
  }
  if (printmath && (qh hull_dim > 3 || qh VORONOI)) {
    fprintf (qh ferr, "qhull input error: Mathematica output is only available for 2-d and 3-d convex hulls and 2-d Delaunay triangulations\n");
    qh_errexit (qh_ERRinput, NULL, NULL);
  }
  if (printgeom) {
    if (qh hull_dim > 4) {
      fprintf (qh ferr, "qhull input error: Geomview output is only available for 2-d, 3-d and 4-d\n");
      qh_errexit (qh_ERRinput, NULL, NULL);
    }
    if (qh PRINTnoplanes && !(qh PRINTcoplanar + qh PRINTcentrums
     + qh PRINTdots + qh PRINTspheres + qh DOintersections + qh PRINTridges)) {
      fprintf (qh ferr, "qhull input error: no output specified for Geomview\n");
      qh_errexit (qh_ERRinput, NULL, NULL);
    }
    if (qh VORONOI && (qh hull_dim > 3 || qh DROPdim >= 0)) {
      fprintf (qh ferr, "qhull input error: Geomview output for Voronoi diagrams only for 2-d\n");
      qh_errexit (qh_ERRinput, NULL, NULL);
    }
    /* can not warn about furthest-site Geomview output: no lower_threshold */
    if (qh hull_dim == 4 && qh DROPdim == -1 &&
	(qh PRINTcoplanar || qh PRINTspheres || qh PRINTcentrums)) {
      fprintf (qh ferr, "qhull input warning: coplanars, vertices, and centrums output not\n\
available for 4-d output (ignored).  Could use 'GDn' instead.\n");
      qh PRINTcoplanar= qh PRINTspheres= qh PRINTcentrums= False;
    }
  }
  qh PRINTdim= qh hull_dim;
  if (qh DROPdim >=0) {    /* after Geomview checks */
    if (qh DROPdim < qh hull_dim) {
      qh PRINTdim--;
      if (!printgeom || qh hull_dim < 3)
        fprintf (qh ferr, "qhull input warning: drop dimension 'GD%d' is only available for 3-d/4-d Geomview\n", qh DROPdim);
    }else
      qh DROPdim= -1;
  }else if (qh VORONOI) {
    qh DROPdim= qh hull_dim-1;
    qh PRINTdim= qh hull_dim-1;
  }
} /* initqhull_globals */
 
/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="initqhull_mem">-</a>

  qh_initqhull_mem(  )
    initialize mem.c for qhull
    qh.hull_dim and qh.normal_size determine some of the allocation sizes
    if qh.MERGING,
      includes ridgeT
    calls qh_user_memsizes() to add up to 10 additional sizes for quick allocation
      (see numsizes below)

  returns:
    mem.c already for qh_memalloc/qh_memfree (errors if called beforehand)

  notes:
    qh_produceoutput() prints memsizes

*/
void qh_initqhull_mem (void) {
  int numsizes;
  int i;

  numsizes= 8+10;
  qh_meminitbuffers (qh IStracing, qh_MEMalign, numsizes,
                     qh_MEMbufsize,qh_MEMinitbuf);
  qh_memsize(sizeof(vertexT));
  if (qh MERGING) {
    qh_memsize(sizeof(ridgeT));
    qh_memsize(sizeof(mergeT));
  }
  qh_memsize(sizeof(facetT));
  i= sizeof(setT) + (qh hull_dim - 1) * SETelemsize;  /* ridge.vertices */
  qh_memsize(i);
  qh_memsize(qh normal_size);        /* normal */
  i += SETelemsize;                 /* facet.vertices, .ridges, .neighbors */
  qh_memsize(i);
  qh_user_memsizes();
  qh_memsetup();
} /* initqhull_mem */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="initqhull_start">-</a>

  qh_initqhull_start( infile, outfile, errfile )
    start initialization of qhull
    initialize statistics, stdio, default values for global variables

  see:
    qh_maxmin() determines the precision constants
*/
void qh_initqhull_start (FILE *infile, FILE *outfile, FILE *errfile) {

  qh_CPUclock; /* start the clock */
#if qh_QHpointer
  if (!(qh_qh= (qhT *)malloc (sizeof(qhT)))) {
    fprintf (errfile, "qhull error (qh_initqhull_globals): insufficient memory\n");
    exit (qh_ERRmem);  /* no error handler */
  }
  memset((char *)qh_qh, 0, sizeof(qhT));   /* every field is 0, FALSE, NULL */
#else
  memset((char *)&qh_qh, 0, sizeof(qhT));
#endif
  strcat (qh qhull, "qhull");
  qh_initstatistics();
  qh ANGLEmerge= True;
  qh DROPdim= -1;
  qh ferr= errfile;
  qh fin= infile;
  qh fout= outfile;
  qh furthest_id= -1;
  qh JOGGLEmax= REALmax;
  qh KEEPminArea = REALmax;
  qh last_low= REALmax;
  qh last_high= REALmax;
  qh last_newhigh= REALmax;
  qh max_outside= 0.0;
  qh max_vertex= 0.0;
  qh MAXabs_coord= 0.0;
  qh MAXsumcoord= 0.0;
  qh MAXwidth= -REALmax;
  qh MERGEindependent= True;
  qh MINdenom_1= fmax_(1.0/REALmax, REALmin); /* used by qh_scalepoints */
  qh MINoutside= 0.0;
  qh MINvisible= REALmax;
  qh MAXcoplanar= REALmax;
  qh outside_err= REALmax;
  qh premerge_centrum= 0.0;
  qh premerge_cos= REALmax;
  qh PRINTprecision= True;
  qh PRINTradius= 0.0;
  qh postmerge_cos= REALmax;
  qh postmerge_centrum= 0.0;
  qh ROTATErandom= INT_MIN;
  qh MERGEvertices= True;
  qh totarea= 0.0;
  qh totvol= 0.0;
  qh TRACEdist= REALmax;
  qh TRACEpoint= -1; /* recompile or use 'TPn' */
  qh tracefacet_id= UINT_MAX;  /* recompile to trace a facet */
  qh tracevertex_id= UINT_MAX; /* recompile to trace a vertex */
  qh_RANDOMseed_(1);
} /* initqhull_start */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="initthresholds">-</a>

  qh_initthresholds( commandString )
    set thresholds for printing and scaling from commandString

  returns:
    sets qh.GOODthreshold or qh.SPLITthreshold if 'Pd0D1' used

  see:
    qh_initflags(), 'Qbk' 'QBk' 'Pdk' and 'PDk'
    qh_inthresholds()

  design:
    for each 'Pdn' or 'PDn' option
      check syntax
      set qh.lower_threshold or qh.upper_threshold
    set qh.GOODthreshold if an unbounded threshold is used
    set qh.SPLITthreshold if a bounded threshold is used
*/
void qh_initthresholds(char *command) {
  realT value;
  int index, maxdim, k;
  char *s= command;
  char key;

  maxdim= qh input_dim;
  if (qh DELAUNAY && (qh PROJECTdelaunay || qh PROJECTinput))
    maxdim++;
  while (*s) {
    if (*s == '-')
      s++;
    if (*s == 'P') {
      s++;
      while (*s && !isspace(key= *s++)) {
	if (key == 'd' || key == 'D') {
	  if (!isdigit(*s)) {
	    fprintf(qh ferr, "qhull warning: no dimension given for Print option '%c' at: %s.  Ignored\n",
		    key, s-1);
	    continue;
	  }
	  index= qh_strtol (s, &s);
	  if (index >= qh hull_dim) {
	    fprintf(qh ferr, "qhull warning: dimension %d for Print option '%c' is >= %d.  Ignored\n",
	        index, key, qh hull_dim);
	    continue;
	  }
	  if (*s == ':') {
	    s++;
	    value= qh_strtod(s, &s);
	    if (fabs((double)value) > 1.0) {
	      fprintf(qh ferr, "qhull warning: value %2.4g for Print option %c is > +1 or < -1.  Ignored\n",
	              value, key);
	      continue;
	    }
	  }else
	    value= 0.0;
	  if (key == 'd')
	    qh lower_threshold[index]= value;
	  else
	    qh upper_threshold[index]= value;
	}
      }
    }else if (*s == 'Q') {
      s++;
      while (*s && !isspace(key= *s++)) {
	if (key == 'b' && *s == 'B') {
	  s++;
	  for (k=maxdim; k--; ) {
	    qh lower_bound[k]= -qh_DEFAULTbox;
	    qh upper_bound[k]= qh_DEFAULTbox;
	  }
	}else if (key == 'b' && *s == 'b')
	  s++;
	else if (key == 'b' || key == 'B') {
	  if (!isdigit(*s)) {
	    fprintf(qh ferr, "qhull warning: no dimension given for Qhull option %c.  Ignored\n",
		    key);
	    continue;
	  }
	  index= qh_strtol (s, &s);
	  if (index >= maxdim) {
	    fprintf(qh ferr, "qhull warning: dimension %d for Qhull option %c is >= %d.  Ignored\n",
	        index, key, maxdim);
	    continue;
	  }
	  if (*s == ':') {
	    s++;
	    value= qh_strtod(s, &s);
	  }else if (key == 'b')
	    value= -qh_DEFAULTbox;
	  else
	    value= qh_DEFAULTbox;
	  if (key == 'b')
	    qh lower_bound[index]= value;
	  else
	    qh upper_bound[index]= value;
	}
      }
    }else {
      while (*s && !isspace (*s))
        s++;
    }
    while (isspace (*s))
      s++;
  }
  for (k= qh hull_dim; k--; ) {
    if (qh lower_threshold[k] > -REALmax/2) {
      qh GOODthreshold= True;
      if (qh upper_threshold[k] < REALmax/2) {
        qh SPLITthresholds= True;
        qh GOODthreshold= False;
        break;
      }
    }else if (qh upper_threshold[k] < REALmax/2)
      qh GOODthreshold= True;
  }
} /* initthresholds */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="option">-</a>

  qh_option( option, intVal, realVal )
    add an option description to qh.qhull_options

  notes:
    will be printed with statistics ('Ts') and errors
    strlen(option) < 40
*/
void qh_option (char *option, int *i, realT *r) {
  char buf[200];
  int len, maxlen;

  sprintf (buf, "  %s", option);
  if (i)
    sprintf (buf+strlen(buf), " %d", *i);
  if (r)
    sprintf (buf+strlen(buf), " %2.2g", *r);
  len= strlen(buf);
  qh qhull_optionlen += len;
  maxlen= sizeof (qh qhull_options) - len -1;
  maximize_(maxlen, 0);
  if (qh qhull_optionlen >= 80 && maxlen > 0) {
    qh qhull_optionlen= len;
    strncat (qh qhull_options, "\n", maxlen--);
  }
  strncat (qh qhull_options, buf, maxlen);
} /* option */

#if qh_QHpointer
/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="restore_qhull">-</a>

  qh_restore_qhull( oldqh )
    restores a previously saved qhull
    also restores qh_qhstat and qhmem.tempstack

  notes:
    errors if current qhull hasn't been saved or freed
    uses qhmem for error reporting

  NOTE 1998/5/11:
    Freeing memory after qh_save_qhull and qh_restore_qhull
    is complicated.  The procedures will be redesigned.

  see:
    qh_save_qhull()
*/
void qh_restore_qhull (qhT **oldqh) {

  if (*oldqh && strcmp ((*oldqh)->qhull, "qhull")) {
    fprintf (qhmem.ferr, "qhull internal error (qh_restore_qhull): %p is not a qhull data structure\n",
                  *oldqh);
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
  if (qh_qh) {
    fprintf (qhmem.ferr, "qhull internal error (qh_restore_qhull): did not save or free existing qhull\n");
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
  if (!*oldqh || !(*oldqh)->old_qhstat) {
    fprintf (qhmem.ferr, "qhull internal error (qh_restore_qhull): did not previously save qhull %p\n",
                  *oldqh);
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
  qh_qh= *oldqh;
  *oldqh= NULL;
  qh_qhstat= qh old_qhstat;
  qhmem.tempstack= qh old_tempstack;
  trace1((qh ferr, "qh_restore_qhull: restored qhull from %p\n", *oldqh));
} /* restore_qhull */

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="save_qhull">-</a>

  qh_save_qhull(  )
    saves qhull for a later qh_restore_qhull
    also saves qh_qhstat and qhmem.tempstack

  returns:
    qh_qh=NULL

  notes:
    need to initialize qhull or call qh_restore_qhull before continuing

  NOTE 1998/5/11:
    Freeing memory after qh_save_qhull and qh_restore_qhull
    is complicated.  The procedures will be redesigned.

  see:
    qh_restore_qhull()
*/
qhT *qh_save_qhull (void) {
  qhT *oldqh;

  trace1((qhmem.ferr, "qh_save_qhull: save qhull %p\n", qh_qh));
  if (!qh_qh) {
    fprintf (qhmem.ferr, "qhull internal error (qh_save_qhull): qhull not initialized\n");
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
  qh old_qhstat= qh_qhstat;
  qh_qhstat= NULL;
  qh old_tempstack= qhmem.tempstack;
  qhmem.tempstack= NULL;
  oldqh= qh_qh;
  qh_qh= NULL;
  return oldqh;
} /* save_qhull */

#endif

/*-<a                             href="qh-globa.htm#TOC"
  >-------------------------------</a><a name="strtol">-</a>

  qh_strtol( s, endp) qh_strtod( s, endp)
    internal versions of strtol() and strtod()
    does not skip trailing spaces
  notes:
    some implementations of strtol()/strtod() skip trailing spaces
*/
double qh_strtod (const char *s, char **endp) {
  double result;

  result= strtod (s, endp);
  if (s < (*endp) && (*endp)[-1] == ' ')
    (*endp)--;
  return result;
} /* strtod */

int qh_strtol (const char *s, char **endp) {
  int result;

  result= (int) strtol (s, endp, 10);
  if (s< (*endp) && (*endp)[-1] == ' ')
    (*endp)--;
  return result;
} /* strtol */
