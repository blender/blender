/*<html><pre>  -<a                             href="qh-io.htm"
  >-------------------------------</a><a name="TOP">-</a>

   io.h 
   declarations of Input/Output functions

   see README, qhull.h and io.c

   copyright (c) 1993-2002, The Geometry Center
*/

#ifndef qhDEFio
#define qhDEFio 1

/*============ constants and flags ==================*/

/*-<a                             href="qh-io.htm#TOC"
  >--------------------------------</a><a name="qh_MAXfirst">-</a>
  
  qh_MAXfirst
    maximum length of first two lines of stdin
*/
#define qh_MAXfirst  200

/*-<a                             href="qh-io.htm#TOC"
  >--------------------------------</a><a name="qh_MINradius">-</a>
  
  qh_MINradius
    min radius for Gp and Gv, fraction of maxcoord
*/
#define qh_MINradius 0.02

/*-<a                             href="qh-io.htm#TOC"
  >--------------------------------</a><a name="qh_GEOMepsilon">-</a>
  
  qh_GEOMepsilon
    adjust outer planes for 'lines closer' and geomview roundoff.  
    This prevents bleed through.
*/
#define qh_GEOMepsilon 2e-3

/*-<a                             href="qh-io.htm#TOC"
  >--------------------------------</a><a name="qh_WHITESPACE">-</a>
  
  qh_WHITESPACE
    possible values of white space
*/
#define qh_WHITESPACE " \n\t\v\r\f"


/*-<a                             href="qh-io.htm#TOC"
  >--------------------------------</a><a name="RIDGE">-</a>
  
  qh_RIDGE
    to select which ridges to print in qh_eachvoronoi
*/
typedef enum
{
    qh_RIDGEall = 0, qh_RIDGEinner, qh_RIDGEouter
}
qh_RIDGE;

/*-<a                             href="qh-io.htm#TOC"
  >--------------------------------</a><a name="printvridgeT">-</a>
  
  printvridgeT
    prints results of qh_printvdiagram

  see:
    <a href="io.c#printvridge">qh_printvridge</a> for an example
*/
typedef void (*printvridgeT)(FILE *fp, vertexT *vertex, vertexT *vertexA, setT *centers, boolT unbounded);

/*============== -prototypes in alphabetical order =========*/

void    dfacet( unsigned id);
void    dvertex( unsigned id);
void    qh_countfacets (facetT *facetlist, setT *facets, boolT printall, 
              int *numfacetsp, int *numsimplicialp, int *totneighborsp, 
              int *numridgesp, int *numcoplanarsp, int *numnumtricoplanarsp);
pointT *qh_detvnorm (vertexT *vertex, vertexT *vertexA, setT *centers, realT *offsetp);
setT   *qh_detvridge (vertexT *vertex);
setT   *qh_detvridge3 (vertexT *atvertex, vertexT *vertex);
int     qh_eachvoronoi (FILE *fp, printvridgeT printvridge, vertexT *atvertex, boolT visitall, qh_RIDGE innerouter, boolT inorder);
int     qh_eachvoronoi_all (FILE *fp, printvridgeT printvridge, boolT isupper, qh_RIDGE innerouter, boolT inorder);
void	qh_facet2point(facetT *facet, pointT **point0, pointT **point1, realT *mindist);
setT   *qh_facetvertices (facetT *facetlist, setT *facets, boolT allfacets);
void    qh_geomplanes (facetT *facet, realT *outerplane, realT *innerplane);
void    qh_markkeep (facetT *facetlist);
setT   *qh_markvoronoi (facetT *facetlist, setT *facets, boolT printall, boolT *islowerp, int *numcentersp);
void    qh_order_vertexneighbors(vertexT *vertex);
void	qh_printafacet(FILE *fp, int format, facetT *facet, boolT printall);
void    qh_printbegin (FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall);
void 	qh_printcenter (FILE *fp, int format, char *string, facetT *facet);
void    qh_printcentrum (FILE *fp, facetT *facet, realT radius);
void    qh_printend (FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall);
void    qh_printend4geom (FILE *fp, facetT *facet, int *num, boolT printall);
void    qh_printextremes (FILE *fp, facetT *facetlist, setT *facets, int printall);
void    qh_printextremes_2d (FILE *fp, facetT *facetlist, setT *facets, int printall);
void    qh_printextremes_d (FILE *fp, facetT *facetlist, setT *facets, int printall);
void	qh_printfacet(FILE *fp, facetT *facet);
void	qh_printfacet2math(FILE *fp, facetT *facet, int notfirst);
void	qh_printfacet2geom(FILE *fp, facetT *facet, realT color[3]);
void    qh_printfacet2geom_points(FILE *fp, pointT *point1, pointT *point2,
			       facetT *facet, realT offset, realT color[3]);
void	qh_printfacet3math (FILE *fp, facetT *facet, int notfirst);
void	qh_printfacet3geom_nonsimplicial(FILE *fp, facetT *facet, realT color[3]);
void	qh_printfacet3geom_points(FILE *fp, setT *points, facetT *facet, realT offset, realT color[3]);
void	qh_printfacet3geom_simplicial(FILE *fp, facetT *facet, realT color[3]);
void	qh_printfacet3vertex(FILE *fp, facetT *facet, int format);
void	qh_printfacet4geom_nonsimplicial(FILE *fp, facetT *facet, realT color[3]);
void	qh_printfacet4geom_simplicial(FILE *fp, facetT *facet, realT color[3]);
void	qh_printfacetNvertex_nonsimplicial(FILE *fp, facetT *facet, int id, int format);
void	qh_printfacetNvertex_simplicial(FILE *fp, facetT *facet, int format);
void    qh_printfacetheader(FILE *fp, facetT *facet);
void    qh_printfacetridges(FILE *fp, facetT *facet);
void	qh_printfacets(FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall);
void	qh_printhelp_degenerate(FILE *fp);
void	qh_printhelp_singular(FILE *fp);
void	qh_printhyperplaneintersection(FILE *fp, facetT *facet1, facetT *facet2,
  		   setT *vertices, realT color[3]);
void	qh_printneighborhood (FILE *fp, int format, facetT *facetA, facetT *facetB, boolT printall);
void    qh_printline3geom (FILE *fp, pointT *pointA, pointT *pointB, realT color[3]);
void	qh_printpoint(FILE *fp, char *string, pointT *point);
void	qh_printpointid(FILE *fp, char *string, int dim, pointT *point, int id);
void    qh_printpoint3 (FILE *fp, pointT *point);
void    qh_printpoints_out (FILE *fp, facetT *facetlist, setT *facets, int printall);
void    qh_printpointvect (FILE *fp, pointT *point, coordT *normal, pointT *center, realT radius, realT color[3]);
void    qh_printpointvect2 (FILE *fp, pointT *point, coordT *normal, pointT *center, realT radius);
void	qh_printridge(FILE *fp, ridgeT *ridge);
void    qh_printspheres(FILE *fp, setT *vertices, realT radius);
void    qh_printvdiagram (FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall);
int     qh_printvdiagram2 (FILE *fp, printvridgeT printvridge, setT *vertices, qh_RIDGE innerouter, boolT inorder);
void	qh_printvertex(FILE *fp, vertexT *vertex);
void	qh_printvertexlist (FILE *fp, char* string, facetT *facetlist,
                         setT *facets, boolT printall);
void	qh_printvertices (FILE *fp, char* string, setT *vertices);
void    qh_printvneighbors (FILE *fp, facetT* facetlist, setT *facets, boolT printall);
void    qh_printvoronoi (FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall);
void    qh_printvnorm (FILE *fp, vertexT *vertex, vertexT *vertexA, setT *centers, boolT unbounded);
void    qh_printvridge (FILE *fp, vertexT *vertex, vertexT *vertexA, setT *centers, boolT unbounded);
void	qh_produce_output(void);
void    qh_projectdim3 (pointT *source, pointT *destination);
int     qh_readfeasible (int dim, char *remainder);
coordT *qh_readpoints(int *numpoints, int *dimension, boolT *ismalloc);
void    qh_setfeasible (int dim);
boolT	qh_skipfacet(facetT *facet);

#endif /* qhDEFio */
