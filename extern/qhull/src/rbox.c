/*<html><pre>  -<a                             href="index.htm#TOC"
  >-------------------------------</a><a name="TOP">-</a>

   rbox.c
     Generate input points for qhull.
   
   notes:
     50 points generated for 'rbox D4'

     This code needs a full rewrite.  It needs separate procedures for each 
     distribution with common, helper procedures.
   
   WARNING: 
     incorrect range if qh_RANDOMmax is defined wrong (user.h)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#include "user.h"
#if __MWERKS__ && __POWERPC__
#include <SIOUX.h>
#include <Files.h>
#include <console.h>
#include <Desk.h>
#endif

#ifdef _MSC_VER  /* Microsoft Visual C++ */
#pragma warning( disable : 4244)  /* conversion from double to int */
#endif

#define MINVALUE 0.8
#define MAXdim 200
#define PI 3.1415926535897932384
#define DEFAULTzbox 1e6

char prompt[]= "\n\
-rbox- generate various point distributions.  Default is random in cube.\n\
\n\
args (any order, space separated):                    Version: 2001/06/24\n\
  3000    number of random points in cube, lens, spiral, sphere or grid\n\
  D3      dimension 3-d\n\
  c       add a unit cube to the output ('c G2.0' sets size)\n\
  d       add a unit diamond to the output ('d G2.0' sets size)\n\
  l       generate a regular 3-d spiral\n\
  r       generate a regular polygon, ('r s Z1 G0.1' makes a cone)\n\
  s       generate cospherical points\n\
  x       generate random points in simplex, may use 'r' or 'Wn'\n\
  y       same as 'x', plus simplex\n\
  Pn,m,r  add point [n,m,r] first, pads with 0\n\
\n\
  Ln      lens distribution of radius n.  Also 's', 'r', 'G', 'W'.\n\
  Mn,m,r  lattice (Mesh) rotated by [n,-m,0], [m,n,0], [0,0,r], ...\n\
          '27 M1,0,1' is {0,1,2} x {0,1,2} x {0,1,2}.  Try 'M3,4 z'.\n\
  W0.1    random distribution within 0.1 of the cube's or sphere's surface\n\
  Z0.5 s  random points in a 0.5 disk projected to a sphere\n\
  Z0.5 s G0.6 same as Z0.5 within a 0.6 gap\n\
\n\
  Bn      bounding box coordinates, default %2.2g\n\
  h       output as homogeneous coordinates for cdd\n\
  n       remove command line from the first line of output\n\
  On      offset coordinates by n\n\
  t       use time as the random number seed (default is command line)\n\
  tn      use n as the random number seed\n\
  z       print integer coordinates, default 'Bn' is %2.2g\n\
";

/* ------------------------------ prototypes ----------------*/
int roundi( double a);
void out1( double a);
void out2n( double a, double b);
void out3n( double a, double b, double c);
int     qh_rand( void);
void    qh_srand( int seed);


/* ------------------------------ globals -------------------*/

    FILE *fp;
    int isinteger= 0;
    double out_offset= 0.0;


/*--------------------------------------------
-rbox-  main procedure of rbox application
*/
int main(int argc, char **argv) {
    int i,j,k;
    int gendim;
    int cubesize, diamondsize, seed=0, count, apex;
    int dim=3 , numpoints= 0, totpoints, addpoints=0;
    int issphere=0, isaxis=0,  iscdd= 0, islens= 0, isregular=0, iswidth=0, addcube=0;
    int isgap=0, isspiral=0, NOcommand= 0, adddiamond=0, istime=0;
    int isbox=0, issimplex=0, issimplex2=0, ismesh=0;
    double width=0.0, gap=0.0, radius= 0.0;
    double coord[MAXdim], offset, meshm=3.0, meshn=4.0, meshr=5.0;
    double *simplex, *simplexp;
    int nthroot, mult[MAXdim];
    double norm, factor, randr, rangap, lensangle= 0, lensbase= 1;
    double anglediff, angle, x, y, cube= 0.0, diamond= 0.0;
    double box= qh_DEFAULTbox; /* scale all numbers before output */
    double randmax= qh_RANDOMmax;
    char command[200], *s, seedbuf[200];    
    time_t timedata;

#if __MWERKS__ && __POWERPC__
    char inBuf[BUFSIZ], outBuf[BUFSIZ], errBuf[BUFSIZ];
    SIOUXSettings.showstatusline= False;
    SIOUXSettings.tabspaces= 1;
    SIOUXSettings.rows= 40;
    if (setvbuf (stdin, inBuf, _IOFBF, sizeof(inBuf)) < 0   /* w/o, SIOUX I/O is slow*/
    || setvbuf (stdout, outBuf, _IOFBF, sizeof(outBuf)) < 0
    || (stdout != stderr && setvbuf (stderr, errBuf, _IOFBF, sizeof(errBuf)) < 0)) 
      	fprintf ( stderr, "qhull internal warning (main): could not change stdio to fully buffered.\n");
    argc= ccommand(&argv);
#endif
    if (argc == 1) {
 	printf (prompt, box, DEFAULTzbox);
    	exit(1);
    }
    if ((s = strrchr( argv[0], '\\'))) /* Borland gives full path */
      strcpy (command, s+1);
    else
      strcpy (command, argv[0]);
    if ((s= strstr (command, ".EXE"))
    ||  (s= strstr (command, ".exe")))
      *s= '\0';
    /* ============= read flags =============== */
    for (i=1; i < argc; i++) {
  	if (strlen (command) + strlen(argv[i]) + 1 < sizeof(command) ) {
	    strcat (command, " ");
	    strcat (command, argv[i]);
	}
        if (isdigit (argv[i][0])) {
      	    numpoints= atoi (argv[i]);
      	    continue;
	}
	if (argv[i][0] == '-')
	  (argv[i])++;
        switch (argv[i][0]) {
	  case 'c':
	    addcube= 1;
	    if (i+1 < argc && argv[i+1][0] == 'G')
	      cube= (double) atof (&argv[++i][1]);
	    break;
	  case 'd':
	    adddiamond= 1;
	    if (i+1 < argc && argv[i+1][0] == 'G')
	      diamond= (double) atof (&argv[++i][1]);
	    break;
	  case 'h':
	    iscdd= 1;
            break;
	  case 'l':
	    isspiral= 1;
            break;
	  case 'n':
	    NOcommand= 1;
	    break;
	  case 'r':
	    isregular= 1;
	    break;
	  case 's':
	    issphere= 1;
            break;
	  case 't':
	    istime= 1;
	    if (isdigit (argv[i][1]))
	      seed= atoi (&argv[i][1]);
	    else {
	      seed= time (&timedata);
	      sprintf (seedbuf, "%d", seed);
	      strcat (command, seedbuf);
	    }
            break;
	  case 'x':
	    issimplex= 1;
	    break;
	  case 'y':
	    issimplex2= 1;
	    break;
	  case 'z':
	    isinteger= 1;
	    break;
	  case 'B':
	    box= (double) atof (&argv[i][1]);
	    isbox= 1;
	    break;
	  case 'D':
	    dim= atoi (&argv[i][1]);
	    if (dim < 1
	    || dim > MAXdim) {
		fprintf (stderr, "rbox error: dim %d too large or too small\n", dim);
		exit (1);
	    }
            break;
	  case 'G':
	    if (argv[i][1])
	      gap= (double) atof (&argv[i][1]);
	    else
	      gap= 0.5;
	    isgap= 1;
	    break;
	  case 'L':
	    if (argv[i][1])
	      radius= (double) atof (&argv[i][1]);
	    else
	      radius= 10;
	    islens= 1;
	    break;
	  case 'M':
	    ismesh= 1;
    	    s= argv[i]+1;
	    if (*s)
	      meshn= strtod (s, &s);
	    if (*s == ',')
	      meshm= strtod (++s, &s);
	    else
	      meshm= 0.0;
	    if (*s == ',')
	      meshr= strtod (++s, &s);
	    else
	      meshr= sqrt (meshn*meshn + meshm*meshm);
	    if (*s) {
	      fprintf (stderr, "rbox warning: assuming 'M3,4,5' since mesh args are not integers or reals\n");
	      meshn= 3.0, meshm=4.0, meshr=5.0;
	    }
	    break;
	  case 'O':
	    out_offset= (double) atof (&argv[i][1]);
	    break;
	  case 'P':
	    addpoints++;
	    break;
	  case 'W':
	    width= (double) atof (&argv[i][1]);
	    iswidth= 1;
	    break;
	  case 'Z':
	    if (argv[i][1])
	      radius= (double) atof (&argv[i][1]);
	    else
	      radius= 1.0;
	    isaxis= 1;
	    break;
	  default:
            fprintf (stderr, "rbox warning: unknown flag %s.\nExecute 'rbox' without arguments for documentation.\n", argv[i]);
	}
    }
    /* ============= defaults, constants, and sizes =============== */
    if (isinteger && !isbox)
      box= DEFAULTzbox;
    if (addcube) {
      cubesize= floor(ldexp(1.0,dim)+0.5);
      if (cube == 0.0)
        cube= box;
    }else
      cubesize= 0;
    if (adddiamond) {
      diamondsize= 2*dim;
      if (diamond == 0.0)
        diamond= box;
    }else
      diamondsize= 0;
    if (islens) {
      if (isaxis) {
  	fprintf (stderr, "rbox error: can not combine 'Ln' with 'Zn'\n");
  	exit(1);
      }
      if (radius <= 1.0) {
  	fprintf (stderr, "rbox error: lens radius %.2g should be greater than 1.0\n",
  	       radius);
  	exit(1);
      }
      lensangle= asin (1.0/radius);
      lensbase= radius * cos (lensangle);
    }
    if (!numpoints) {
      if (issimplex2)
	; /* ok */
      else if (isregular + issimplex + islens + issphere + isaxis + isspiral + iswidth + ismesh) {
	fprintf (stderr, "rbox error: missing count\n");
	exit(1);
      }else if (adddiamond + addcube + addpoints)
	; /* ok */
      else { 
	numpoints= 50;  /* ./rbox D4 is the test case */
	issphere= 1;
      }
    }
    if ((issimplex + islens + isspiral + ismesh > 1) 
    || (issimplex + issphere + isspiral + ismesh > 1)) {
      fprintf (stderr, "rbox error: can only specify one of 'l', 's', 'x', 'Ln', or 'Mn,m,r' ('Ln s' is ok).\n");
      exit(1);
    }
    fp= stdout;
    /* ============= print header with total points =============== */
    if (issimplex || ismesh)
      totpoints= numpoints;
    else if (issimplex2)
      totpoints= numpoints+dim+1;
    else if (isregular) {
      totpoints= numpoints;
      if (dim == 2) {
      	if (islens)
      	  totpoints += numpoints - 2;
      }else if (dim == 3) {
      	if (islens)
      	  totpoints += 2 * numpoints;
        else if (isgap)
          totpoints += 1 + numpoints;
        else
          totpoints += 2;
      }
    }else
      totpoints= numpoints + isaxis;
    totpoints += cubesize + diamondsize + addpoints;
    if (iscdd) 
      fprintf(fp, "%s\nbegin\n        %d %d %s\n", 
            NOcommand ? "" : command, 
            totpoints, dim+1,
            isinteger ? "integer" : "real");
    else if (NOcommand)
      fprintf(fp,  "%d\n%d\n", dim, totpoints);
    else
      fprintf(fp,  "%d %s\n%d\n", dim, command, totpoints);
    /* ============= seed randoms =============== */
    if (istime == 0) {
      for (s=command; *s; s++) {
	if (issimplex2 && *s == 'y') /* make 'y' same seed as 'x' */
	  i= 'x';
	else
	  i= *s;
	seed= 11*seed + i;
      }
    } /* else, seed explicitly set to n or to time */
    qh_RANDOMseed_(seed);
    /* ============= explicit points =============== */
    for (i=1; i < argc; i++) {
      if (argv[i][0] == 'P') {
	s= argv[i]+1;
	count= 0;
	if (iscdd)
	  out1( 1.0);
	while (*s) {
	  out1( strtod (s, &s));
	  count++;
	  if (*s) { 
	    if (*s++ != ',') {
	      fprintf (stderr, "rbox error: missing comma after coordinate in %s\n\n", argv[i]);
	      exit (1);
	    }
          }
	}
	if (count < dim) {
	  for (k= dim-count; k--; )
	    out1( 0.0);
	}else if (count > dim) {
	  fprintf (stderr, "rbox error: %d coordinates instead of %d coordinates in %s\n\n", 
                 count, dim, argv[i]);
	  exit (1);
	}
	fprintf (fp, "\n");
      }
    }
    /* ============= simplex distribution =============== */
    if (issimplex+issimplex2) {
      if (!(simplex= malloc( dim * (dim+1) * sizeof(double)))) {
	fprintf (stderr, "insufficient memory for simplex\n");
	exit(0);
      }
      simplexp= simplex;
      if (isregular) {
        for (i= 0; i<dim; i++) {
          for (k= 0; k<dim; k++)
	    *(simplexp++)= i==k ? 1.0 : 0.0;
        }
        for (k= 0; k<dim; k++)
	  *(simplexp++)= -1.0;
      }else {
        for (i= 0; i<dim+1; i++) {
          for (k= 0; k<dim; k++) {
    	    randr= qh_RANDOMint;
            *(simplexp++)= 2.0 * randr/randmax - 1.0;
          }
        }
      }
      if (issimplex2) { 
	simplexp= simplex;
        for (i= 0; i<dim+1; i++) {
          if (iscdd)
            out1( 1.0);
          for (k= 0; k<dim; k++)
 	    out1( *(simplexp++) * box);
          fprintf (fp, "\n");
	}
      }
      for (j= 0; j<numpoints; j++) {
        if (iswidth) 
          apex= qh_RANDOMint % (dim+1);
        else
          apex= -1;
        for (k= 0; k<dim; k++)
	  coord[k]= 0.0;
	norm= 0.0;
        for (i= 0; i<dim+1; i++) {
    	  randr= qh_RANDOMint;
          factor= randr/randmax;
          if (i == apex)
            factor *= width;
          norm += factor;
          for (k= 0; k<dim; k++) {
	    simplexp= simplex + i*dim + k;
            coord[k] += factor * (*simplexp);
          }
        }
        for (k= 0; k<dim; k++)
          coord[k] /= norm;
        if (iscdd)
          out1( 1.0);
        for (k=0; k < dim; k++) 
	  out1( coord[k] * box);
        fprintf (fp, "\n");
      }
      isregular= 0; /* continue with isbox */
      numpoints= 0;
    }
    /* ============= mesh distribution =============== */
    if (ismesh) {
      nthroot= pow (numpoints, 1.0/dim) + 0.99999;
      for (k= dim; k--; )
	mult[k]= 0;
      for (i= 0; i < numpoints; i++) {
	for (k= 0; k < dim; k++) {
	  if (k == 0)
	    out1( mult[0] * meshn + mult[1] * (-meshm));
	  else if (k == 1)
	    out1( mult[0] * meshm + mult[1] * meshn);
	  else
	    out1( mult[k] * meshr );
	}
        fprintf (fp, "\n");
	for (k= 0; k < dim; k++) {
	  if (++mult[k] < nthroot)
	    break;
	  mult[k]= 0;
	}
      }
    }

    /* ============= regular points for 's' =============== */
    else if (isregular && !islens) {
      if (dim != 2 && dim != 3) {
	fprintf(stderr, "rbox error: regular points can be used only in 2-d and 3-d\n\n");
	exit(1);
      }
      if (!isaxis || radius == 0.0) {
	isaxis= 1;
	radius= 1.0;
      }
      if (dim == 3) {
        if (iscdd)
          out1( 1.0);
	out3n( 0.0, 0.0, -box);
	if (!isgap) {
          if (iscdd)
            out1( 1.0);
  	  out3n( 0.0, 0.0, box);
  	}
      }
      angle= 0.0;
      anglediff= 2.0 * PI/numpoints;
      for (i=0; i < numpoints; i++) {
	angle += anglediff;
	x= radius * cos (angle);
	y= radius * sin (angle);
	if (dim == 2) {
          if (iscdd)
            out1( 1.0);
	  out2n( x*box, y*box);
	}else {
	  norm= sqrt (1.0 + x*x + y*y);
          if (iscdd)
            out1( 1.0);
	  out3n( box*x/norm, box*y/norm, box/norm);
	  if (isgap) {
	    x *= 1-gap;
	    y *= 1-gap;
	    norm= sqrt (1.0 + x*x + y*y);
            if (iscdd)
              out1( 1.0);
	    out3n( box*x/norm, box*y/norm, box/norm);
	  }
	}
      }
    }
    /* ============= regular points for 'r Ln D2' =============== */
    else if (isregular && islens && dim == 2) {
      double cos_0;
      
      angle= lensangle;
      anglediff= 2 * lensangle/(numpoints - 1);
      cos_0= cos (lensangle);
      for (i=0; i < numpoints; i++, angle -= anglediff) {
	x= radius * sin (angle);
  	y= radius * (cos (angle) - cos_0);
        if (iscdd)
          out1( 1.0);
	out2n( x*box, y*box);
	if (i != 0 && i != numpoints - 1) {
          if (iscdd)
            out1( 1.0);
	  out2n( x*box, -y*box);
	}
      }
    }
    /* ============= regular points for 'r Ln D3' =============== */
    else if (isregular && islens && dim != 2) {
      if (dim != 3) {
	fprintf(stderr, "rbox error: regular points can be used only in 2-d and 3-d\n\n");
	exit(1);
      }
      angle= 0.0;
      anglediff= 2* PI/numpoints;
      if (!isgap) {
	isgap= 1;
        gap= 0.5;
      }
      offset= sqrt (radius * radius - (1-gap)*(1-gap)) - lensbase;
      for (i=0; i < numpoints; i++, angle += anglediff) {
  	x= cos (angle);
	y= sin (angle);
        if (iscdd)
          out1( 1.0);
	out3n( box*x, box*y, 0);
	x *= 1-gap;
	y *= 1-gap;
        if (iscdd)
          out1( 1.0);
	out3n( box*x, box*y, box * offset);
        if (iscdd)
          out1( 1.0);
	out3n( box*x, box*y, -box * offset);
      }
    }
    /* ============= apex of 'Zn' distribution + gendim =============== */
    else {
      if (isaxis) {
	gendim= dim-1;
	if (iscdd)
	  out1( 1.0);
	for (j=0; j < gendim; j++)
	  out1( 0.0);
	out1( -box);
	fprintf (fp, "\n");
      }else if (islens) 
	gendim= dim-1;
      else
	gendim= dim;
      /* ============= generate random point in unit cube =============== */
      for (i=0; i < numpoints; i++) {
	norm= 0.0;
	for (j=0; j < gendim; j++) {
	  randr= qh_RANDOMint;
	  coord[j]= 2.0 * randr/randmax - 1.0;
	  norm += coord[j] * coord[j];
	}
	norm= sqrt (norm);
	/* ============= dim-1 point of 'Zn' distribution ========== */
	if (isaxis) {
	  if (!isgap) {
	    isgap= 1;
	    gap= 1.0;
	  }
	  randr= qh_RANDOMint;
	  rangap= 1.0 - gap * randr/randmax;
	  factor= radius * rangap / norm;
	  for (j=0; j<gendim; j++)
	    coord[j]= factor * coord[j];
	/* ============= dim-1 point of 'Ln s' distribution =========== */
	}else if (islens && issphere) {
	  if (!isgap) {
	    isgap= 1;
	    gap= 1.0;
	  }
	  randr= qh_RANDOMint;
	  rangap= 1.0 - gap * randr/randmax;
	  factor= rangap / norm;
	  for (j=0; j<gendim; j++)
	    coord[j]= factor * coord[j];
	/* ============= dim-1 point of 'Ln' distribution ========== */
	}else if (islens && !issphere) {
	  if (!isgap) {
	    isgap= 1;
	    gap= 1.0;
	  }
	  j= qh_RANDOMint % gendim;
	  if (coord[j] < 0)
	    coord[j]= -1.0 - coord[j] * gap;
	  else
	    coord[j]= 1.0 - coord[j] * gap;
	/* ============= point of 'l' distribution =============== */
	}else if (isspiral) {
	  if (dim != 3) {
	    fprintf(stderr, "rbox error: spiral distribution is available only in 3d\n\n");
	    exit(1);
	  }
	  coord[0]= cos(2*PI*i/(numpoints - 1));
	  coord[1]= sin(2*PI*i/(numpoints - 1));
	  coord[2]= 2.0*(double)i/(double)(numpoints-1) - 1.0;
	/* ============= point of 's' distribution =============== */
	}else if (issphere) {
	  factor= 1.0/norm;
	  if (iswidth) {
  	    randr= qh_RANDOMint;
	    factor *= 1.0 - width * randr/randmax;
	  }
	  for (j=0; j<dim; j++)
	    coord[j]= factor * coord[j];
	}
	/* ============= project 'Zn s' point in to sphere =============== */
	if (isaxis && issphere) {
	  coord[dim-1]= 1.0;
	  norm= 1.0;
	  for (j=0; j<gendim; j++)
	    norm += coord[j] * coord[j];
	  norm= sqrt (norm);
	  for (j=0; j<dim; j++)
	    coord[j]= coord[j] / norm;
	  if (iswidth) {
  	    randr= qh_RANDOMint;
	    coord[dim-1] *= 1 - width * randr/randmax;
	  }
	/* ============= project 'Zn' point onto cube =============== */
	}else if (isaxis && !issphere) {  /* not very interesting */
	  randr= qh_RANDOMint;
	  coord[dim-1]= 2.0 * randr/randmax - 1.0;
	/* ============= project 'Ln' point out to sphere =============== */
	}else if (islens) {
	  coord[dim-1]= lensbase;
	  for (j=0, norm= 0; j<dim; j++)
	    norm += coord[j] * coord[j];
	  norm= sqrt (norm);
	  for (j=0; j<dim; j++)
	    coord[j]= coord[j] * radius/ norm;
	  coord[dim-1] -= lensbase;
	  if (iswidth) {
  	    randr= qh_RANDOMint;
	    coord[dim-1] *= 1 - width * randr/randmax;
	  }
	  if (qh_RANDOMint > randmax/2)
	    coord[dim-1]= -coord[dim-1];
	/* ============= project 'Wn' point toward boundary =============== */
	}else if (iswidth && !issphere) {
	  j= qh_RANDOMint % gendim;
	  if (coord[j] < 0)
	    coord[j]= -1.0 - coord[j] * width;
	  else
	    coord[j]= 1.0 - coord[j] * width;
	}
	/* ============= write point =============== */
	if (iscdd)
	  out1( 1.0);
	for (k=0; k < dim; k++) 
	  out1( coord[k] * box);
	fprintf (fp, "\n");
      }
    }
    /* ============= write cube vertices =============== */
    if (addcube) {
      for (j=0; j<cubesize; j++) {
        if (iscdd)
          out1( 1.0);
	for (k=dim-1; k>=0; k--) {
	  if (j & ( 1 << k))
	    out1( cube);
	  else
	    out1( -cube);
	}
	fprintf (fp, "\n");
      }
    }
    /* ============= write diamond vertices =============== */
    if (adddiamond) {
      for (j=0; j<diamondsize; j++) {
        if (iscdd)
          out1( 1.0);
	for (k=dim-1; k>=0; k--) {
	  if (j/2 != k)
	    out1( 0.0);
	  else if (j & 0x1)
	    out1( diamond);
	  else
	    out1( -diamond);
	}
	fprintf (fp, "\n");
      }
    }
    if (iscdd)
      fprintf (fp, "end\nhull\n");
    return 0;
  } /* rbox */

/*------------------------------------------------
-outxxx - output functions
*/
int roundi( double a) {
  if (a < 0.0) {
    if (a - 0.5 < INT_MIN) {
      fprintf(stderr, "rbox input error: coordinate %2.2g is too large.  Reduce 'Bn'\n", a);
      exit (1);
    }
    return a - 0.5;
  }else {
    if (a + 0.5 > INT_MAX) {
      fprintf(stderr, "rbox input error: coordinate %2.2g is too large.  Reduce 'Bn'\n", a);
      exit (1);
    }
    return a + 0.5;
  }
} /* roundi */

void out1(double a) {

  if (isinteger) 
    fprintf(fp, "%d ", roundi( a+out_offset));
  else
    fprintf(fp, qh_REAL_1, a+out_offset);
} /* out1 */

void out2n( double a, double b) {

  if (isinteger)
    fprintf(fp, "%d %d\n", roundi(a+out_offset), roundi(b+out_offset));
  else
    fprintf(fp, qh_REAL_2n, a+out_offset, b+out_offset);
} /* out2n */

void out3n( double a, double b, double c) { 

  if (isinteger)
    fprintf(fp, "%d %d %d\n", roundi(a+out_offset), roundi(b+out_offset), roundi(c+out_offset));
  else
    fprintf(fp, qh_REAL_3n, a+out_offset, b+out_offset, c+out_offset);
} /* out3n */

/*-------------------------------------------------
-rand & srand- generate pseudo-random number between 1 and 2^31 -2
  from Park & Miller's minimimal standard random number generator
  Communications of the ACM, 31:1192-1201, 1988.
notes:
  does not use 0 or 2^31 -1
  this is silently enforced by qh_srand()
  copied from geom2.c
*/
static int seed = 1;  /* global static */

int qh_rand( void) {
#define qh_rand_a 16807
#define qh_rand_m 2147483647
#define qh_rand_q 127773  /* m div a */
#define qh_rand_r 2836    /* m mod a */
  int lo, hi, test;

  hi = seed / qh_rand_q;  /* seed div q */
  lo = seed % qh_rand_q;  /* seed mod q */
  test = qh_rand_a * lo - qh_rand_r * hi;
  if (test > 0)
    seed= test;
  else
    seed= test + qh_rand_m;
  return seed;
} /* rand */

void qh_srand( int newseed) {
  if (newseed < 1)
    seed= 1;
  else if (newseed >= qh_rand_m)
    seed= qh_rand_m - 1;
  else
    seed= newseed;
} /* qh_srand */

