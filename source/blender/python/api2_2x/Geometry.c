/* 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert, Campbell Barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Geometry.h"

/*  - Not needed for now though other geometry functions will probably need them
#include "BLI_arithb.h"
#include "BKE_utildefines.h"
*/

/* Used for PolyFill */
#include "BKE_displist.h"
#include "MEM_guardedalloc.h" 
#include "BLI_blenlib.h"

/* needed for EXPP_ReturnPyObjError and EXPP_check_sequence_consistency */
#include "gen_utils.h"

//#include "util.h" /* MIN2 and MAX2 */ 
#include "BKE_utildefines.h"

#define SWAP_FLOAT(a,b,tmp) tmp=a; a=b; b=tmp
#define eul 0.000001

/*-- forward declarations -- */
static PyObject *M_Geometry_PolyFill( PyObject * self, PyObject * args );
static PyObject *M_Geometry_LineIntersect2D( PyObject * self, PyObject * args );
static PyObject *M_Geometry_BoxPack2D( PyObject * self, PyObject * args );

/*-------------------------DOC STRINGS ---------------------------*/
static char M_Geometry_doc[] = "The Blender Geometry module\n\n";
static char M_Geometry_PolyFill_doc[] = "(veclist_list) - takes a list of polylines (each point a vector) and returns the point indicies for a polyline filled with triangles";
static char M_Geometry_LineIntersect2D_doc[] = "(lineA_p1, lineA_p2, lineB_p1, lineB_p2) - takes 2 lines (as 4 vectors) and returns a vector for their point of intersection or None";
static char M_Geometry_BoxPack2D_doc[] = "";
/*-----------------------METHOD DEFINITIONS ----------------------*/
struct PyMethodDef M_Geometry_methods[] = {
	{"PolyFill", ( PyCFunction ) M_Geometry_PolyFill, METH_VARARGS, M_Geometry_PolyFill_doc},
	{"LineIntersect2D", ( PyCFunction ) M_Geometry_LineIntersect2D, METH_VARARGS, M_Geometry_LineIntersect2D_doc},
	{"BoxPack2D", ( PyCFunction ) M_Geometry_BoxPack2D, METH_VARARGS, M_Geometry_BoxPack2D_doc},
	{NULL, NULL, 0, NULL}
};
/*----------------------------MODULE INIT-------------------------*/
PyObject *Geometry_Init(void)
{
	PyObject *submodule;

	submodule = Py_InitModule3("Blender.Geometry",
				    M_Geometry_methods, M_Geometry_doc);
	return (submodule);
}

/*----------------------------------Geometry.PolyFill() -------------------*/
/* PolyFill function, uses Blenders scanfill to fill multiple poly lines */
static PyObject *M_Geometry_PolyFill( PyObject * self, PyObject * args )
{
	PyObject *tri_list; /*return this list of tri's */
	PyObject *polyLineSeq, *polyLine, *polyVec;
	int i, len_polylines, len_polypoints;
	
	/* display listbase */
	ListBase dispbase={NULL, NULL};
	DispList *dl;
	float *fp; /*pointer to the array of malloced dl->verts to set the points from the vectors */
	int index, *dl_face, totpoints=0;
	
	
	dispbase.first= dispbase.last= NULL;
	
	
	if(!PyArg_ParseTuple ( args, "O", &polyLineSeq) || !PySequence_Check(polyLineSeq)) {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a sequence of poly lines" );
	}
	
	len_polylines = PySequence_Size( polyLineSeq );
	
	for( i = 0; i < len_polylines; ++i ) {
		polyLine= PySequence_GetItem( polyLineSeq, i );
		if (!PySequence_Check(polyLine)) {
			freedisplist(&dispbase);
			Py_XDECREF(polyLine); /* may be null so use Py_XDECREF*/
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				  "One or more of the polylines is not a sequence of Mathutils.Vector's" );
		}
		
		len_polypoints= PySequence_Size( polyLine );
		if (len_polypoints>0) { /* dont bother adding edges as polylines */
			if (EXPP_check_sequence_consistency( polyLine, &vector_Type ) != 1) {
				freedisplist(&dispbase);
				Py_DECREF(polyLine);
				return EXPP_ReturnPyObjError( PyExc_TypeError,
					  "A point in one of the polylines is not a Mathutils.Vector type" );
			}
			
			dl= MEM_callocN(sizeof(DispList), "poly disp");
			BLI_addtail(&dispbase, dl);
			dl->type= DL_INDEX3;
			dl->nr= len_polypoints;
			dl->type= DL_POLY;
			dl->parts= 1; /* no faces, 1 edge loop */
			dl->col= 0; /* no material */
			dl->verts= fp= MEM_callocN( sizeof(float)*3*len_polypoints, "dl verts");
			dl->index= MEM_callocN(sizeof(int)*3*len_polypoints, "dl index");
			
			for( index = 0; index<len_polypoints; ++index, fp+=3) {
				polyVec= PySequence_GetItem( polyLine, index );
				
				fp[0] = ((VectorObject *)polyVec)->vec[0];
				fp[1] = ((VectorObject *)polyVec)->vec[1];
				if( ((VectorObject *)polyVec)->size > 2 )
					fp[2] = ((VectorObject *)polyVec)->vec[2];
				else
					fp[2]= 0.0f; /* if its a 2d vector then set the z to be zero */
				
				totpoints++;
				Py_DECREF(polyVec);
			}
		}
		Py_DECREF(polyLine);
	}
	
	if (totpoints) {
		/* now make the list to return */
		filldisplist(&dispbase, &dispbase);
		
		/* The faces are stored in a new DisplayList
		thats added to the head of the listbase */
		dl= dispbase.first; 
		
		tri_list= PyList_New(dl->parts);
		if( !tri_list ) {
			freedisplist(&dispbase);
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"Geometry.PolyFill failed to make a new list" );
		}
		
		index= 0;
		dl_face= dl->index;
		while(index < dl->parts) {
			PyList_SetItem(tri_list, index, Py_BuildValue("iii", dl_face[0], dl_face[1], dl_face[2]) );
			dl_face+= 3;
			index++;
		}
		freedisplist(&dispbase);
	} else {
		/* no points, do this so scripts dont barf */
		tri_list= PyList_New(0);
	}
	
	return tri_list;
}


static PyObject *M_Geometry_LineIntersect2D( PyObject * self, PyObject * args )
{
	VectorObject *line_a1, *line_a2, *line_b1, *line_b2;
	float a1x, a1y, a2x, a2y,  b1x, b1y, b2x, b2y, xi, yi, a1,a2,b1,b2, newvec[2];
	if( !PyArg_ParseTuple ( args, "O!O!O!O!",
	  &vector_Type, &line_a1,
	  &vector_Type, &line_a2,
	  &vector_Type, &line_b1,
	  &vector_Type, &line_b2)
	)
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected 4 vector types\n" ) );
	
	a1x= line_a1->vec[0];
	a1y= line_a1->vec[1];
	a2x= line_a2->vec[0];
	a2y= line_a2->vec[1];

	b1x= line_b1->vec[0];
	b1y= line_b1->vec[1];
	b2x= line_b2->vec[0];
	b2y= line_b2->vec[1];
	
	if((MIN2(a1x, a2x) > MAX2(b1x, b2x)) ||
	   (MAX2(a1x, a2x) < MIN2(b1x, b2x)) ||
	   (MIN2(a1y, a2y) > MAX2(b1y, b2y)) ||
	   (MAX2(a1y, a2y) < MIN2(b1y, b2y))  ) {
		Py_RETURN_NONE;
	}
	/* Make sure the hoz/vert line comes first. */
	if (fabs(b1x - b2x) < eul || fabs(b1y - b2y) < eul) {
		SWAP_FLOAT(a1x, b1x, xi); /*abuse xi*/
		SWAP_FLOAT(a1y, b1y, xi);
		SWAP_FLOAT(a2x, b2x, xi);
		SWAP_FLOAT(a2y, b2y, xi);
	}
	
	if (fabs(a1x-a2x) < eul) { /* verticle line */
		if (fabs(b1x-b2x) < eul){ /*verticle second line */
			Py_RETURN_NONE; /* 2 verticle lines dont intersect. */
		}
		else if (fabs(b1y-b2y) < eul) {
			/*X of vert, Y of hoz. no calculation needed */
			newvec[0]= a1x;
			newvec[1]= b1y;
			return newVectorObject(newvec, 2, Py_NEW);
		}
		
		yi = (float)(((b1y / fabs(b1x - b2x)) * fabs(b2x - a1x)) + ((b2y / fabs(b1x - b2x)) * fabs(b1x - a1x)));
		
		if (yi > MAX2(a1y, a2y)) {/* New point above seg1's vert line */
			Py_RETURN_NONE;
		} else if (yi < MIN2(a1y, a2y)) { /* New point below seg1's vert line */
			Py_RETURN_NONE;
		}
		newvec[0]= a1x;
		newvec[1]= yi;
		return newVectorObject(newvec, 2, Py_NEW);
	} else if (fabs(a2y-a1y) < eul) {  /* hoz line1 */
		if (fabs(b2y-b1y) < eul) { /*hoz line2*/
			Py_RETURN_NONE; /*2 hoz lines dont intersect*/
		}
		
		/* Can skip vert line check for seg 2 since its covered above. */
		xi = (float)(((b1x / fabs(b1y - b2y)) * fabs(b2y - a1y)) + ((b2x / fabs(b1y - b2y)) * fabs(b1y - a1y)));
		if (xi > MAX2(a1x, a2x)) { /* New point right of hoz line1's */
			Py_RETURN_NONE;
		} else if (xi < MIN2(a1x, a2x)) { /*New point left of seg1's hoz line */
			Py_RETURN_NONE;
		}
		newvec[0]= xi;
		newvec[1]= a1y;
		return newVectorObject(newvec, 2, Py_NEW);
	}
	
	b1 = (a2y-a1y)/(a2x-a1x);
	b2 = (b2y-b1y)/(b2x-b1x);
	a1 = a1y-b1*a1x;
	a2 = b1y-b2*b1x;
	
	if (b1 - b2 == 0.0) {
		Py_RETURN_NONE;
	}
	
	xi = - (a1-a2)/(b1-b2);
	yi = a1+b1*xi;
	if ((a1x-xi)*(xi-a2x) >= 0 && (b1x-xi)*(xi-b2x) >= 0 && (a1y-yi)*(yi-a2y) >= 0 && (b1y-yi)*(yi-b2y)>=0) {
		newvec[0]= xi;
		newvec[1]= yi;
		return newVectorObject(newvec, 2, Py_NEW);
	}
	Py_RETURN_NONE;
}



/* Campbells BoxPacker ported from Python */
/* free vert flags */
#define EUL 0.0000001
#define BLF 1
#define TRF 2
#define TLF 4
#define BRF 8
#define BL 0
#define TR 1
#define TL 2
#define BR 3

#define BOXLEFT(b)		b->v[BL]->x
#define BOXRIGHT(b)		b->v[TR]->x
#define BOXBOTTOM(b)	b->v[BL]->y
#define BOXTOP(b)		b->v[TR]->y
#define BOXAREA(b)		(b->w * b->h)

#define UPDATE_V34X(b) b->v[TL]->x = b->v[BL]->x; b->v[BR]->x = b->v[TR]->x
#define UPDATE_V34Y(b) b->v[TL]->y = b->v[TR]->y; b->v[BR]->y = b->v[BL]->y
   	
#define UPDATE_V34(b) UPDATE_V34X(b) UPDATE_V34Y(b)  

#define SET_BOXLEFT(b, f)	b->v[TR]->x = f + b->w; b->v[BL]->x = f; UPDATE_V34X(b)
#define SET_BOXRIGHT(b, f)	b->v[BL]->x = f - b->w; b->v[TR]->x = f; UPDATE_V34X(b)
#define SET_BOXBOTTOM(b, f)	b->v[TR]->y = f + b->h; b->v[BL]->y = f; UPDATE_V34Y(b)
#define SET_BOXTOP(b, f)	b->v[BL]->y = f - b->h; b->v[TR]->y = f; UPDATE_V34Y(b)
#define BOXINTERSECT(b1, b2) (!(BOXLEFT(b1)+EUL>=BOXRIGHT(b2) || BOXBOTTOM(b1)+EUL>=BOXTOP(b2) || BOXRIGHT(b1)-EUL<=BOXLEFT(b2) || BOXTOP(b1)-EUL<=BOXBOTTOM(b2) ))

#define BOXDEBUG(b) printf("\tBox Debug i %i, w:%.3f h:%.3f x:%.3f y:%.3f\n", b->index, b->w, b->h, b->x, b->y)


static int box_areasort(const void *p1, const void *p2)
{
	const boxPack *b1=p1, *b2=p2;
	float a1, a2;

	a1 = BOXAREA(b1);
	a2 = BOXAREA(b2);
	/*printf("a1 a2 %f %f\n", a1, a2);*/
	
	/* sort largest to smallest */
	if		( a1 < a2 ) return  1;
	else if	( a1 > a2 ) return -1;
	return 0;
}


static float box_width;
static float box_height;
static boxVert *vertarray;

static int vertex_sort(const void *p1, const void *p2)
{
	boxVert *v1, *v2;
	float a1, a2;
	
	v1 = vertarray + ((int *) p1)[0];
	v2 = vertarray + ((int *) p2)[0];
	
	// self.verts.sort(key = lambda b: max(b.x+w, b.y+h) ) # Reverse area sort
	
	a1 = MAX2(v1->x+box_width, v1->y+box_height);
	a2 = MAX2(v2->x+box_width, v2->y+box_height);
	
	/*printf("a1 a2 %f %f\n", a1, a2);*/
	
	/* sort largest to smallest */
	if		( a1 > a2 ) return  1;
	else if	( a1 < a2 ) return -1;
	return 0;
}


static void boxPackAll(boxPack *boxarray, int len, float *tot_width, float *tot_height)
{
	boxVert *vert;
	int box_index, verts_pack_len, i, j, k, isect; /* what box are we up to packing */
	int quad_flags[4]= {BLF,TRF,TLF,BRF}; /* use for looping */
	boxPack *box, *box_test;
	int *vertex_pack_indicies;
	
	if (!len) {
		*tot_width =  0.0;
		*tot_height = 0.0;
		return;
	}
	
	/* Sort boxes, biggest first */
	qsort(boxarray, len, sizeof(boxPack), box_areasort);
	
	/* add verts to the boxes, these are only used internally  */
	vert = vertarray = MEM_mallocN( len*4*sizeof(boxVert), "boxPack verts");
	vertex_pack_indicies = MEM_mallocN( len*3*sizeof(int), "boxPack indicies");
	i=0;
	for (box= boxarray, box_index= 0; box_index < len; box_index++, box++) {
		 		
		vert->blb = vert->brb = vert->tlb =\
			vert->isect_cache[0] = vert->isect_cache[1] =\
			vert->isect_cache[2] = vert->isect_cache[3] = NULL;
		vert->free = 15 &~ TRF;
		vert->trb = box;
		vert->index = i; i++;
		box->v[BL] = vert; vert++;
		
		vert->trb= vert->brb = vert->tlb =\
			vert->isect_cache[0] = vert->isect_cache[1] =\
			vert->isect_cache[2] = vert->isect_cache[3] = NULL;
		vert->free = 15 &~ BLF;
		vert->blb = box;
		vert->index = i; i++;
		box->v[TR] = vert; vert++;
		
		vert->trb = vert->blb = vert->tlb =\
			vert->isect_cache[0] = vert->isect_cache[1] =\
			vert->isect_cache[2] = vert->isect_cache[3] = NULL;
		vert->free = 15 &~ BRF;
		vert->brb = box;
		vert->index = i; i++;
		box->v[TL] = vert; vert++;
		
		vert->trb = vert->blb = vert->brb =\
			vert->isect_cache[0] = vert->isect_cache[1] =\
			vert->isect_cache[2] = vert->isect_cache[3] = NULL;
		vert->free = 15 &~ TLF;
		vert->tlb = box; 
		vert->index = i; i++;
		box->v[BR] = vert; vert++;
	}
	vert = NULL;
	
	
	/* Pack the First box!
	 * then enter the main boxpacking loop */
	
	box = boxarray; /* get the first box  */
	/* First time, no boxes packed */
	box->v[BL]->free = 0; /* Cant use any if these */
	box->v[BR]->free &= ~(BLF|BRF);
	box->v[TL]->free &= ~(BLF|TLF);
	
	*tot_width = box->w;
	*tot_height = box->h; 
	
	/* This sets all the vertex locations */
	SET_BOXLEFT(box, 0.0);
	SET_BOXBOTTOM(box, 0.0);
	
	for (i=0; i<3; i++)
		vertex_pack_indicies[i] = box->v[i+1]->index; 
	verts_pack_len = 3;
	box++; /* next box, needed for the loop below */
	/* ...done packing the first box */

	/* Main boxpacking loop */
	for (box_index=1; box_index < len; box_index++, box++) {
		
		/* Sort the verts, these constants are used in sorting  */
		box_width = box->w;
		box_height = box->h;
		
		qsort(vertex_pack_indicies, verts_pack_len, sizeof(int), vertex_sort);
		
		/* Pack the box in with the others */
		/* sort the verts */
		isect = 1;
		
		for (i=0; i<verts_pack_len && isect; i++) {
			vert = vertarray + vertex_pack_indicies[i];
			/* printf("\ttesting vert %i %i %i %f %f\n", i, vert->free, verts_pack_len, vert->x, vert->y); */
			
			/* This vert has a free quaderent
			 * Test if we can place the box here
			 * vert->free & quad_flags[j] - Checks 
			 * */
						
			for (j=0; (j<4) && isect; j++) {
				if (vert->free & quad_flags[j]) {
					switch (j) {
					case BL:
					 	SET_BOXRIGHT(box, vert->x);
					 	SET_BOXTOP(box, vert->y);
					 	break;
					case TR:
					 	SET_BOXLEFT(box, vert->x);
					 	SET_BOXBOTTOM(box, vert->y);
					 	break;
					case TL:
					 	SET_BOXRIGHT(box, vert->x);
					 	SET_BOXBOTTOM(box, vert->y);
					 	break;
					case BR:
						SET_BOXLEFT(box, vert->x);
						SET_BOXTOP(box, vert->y);
						break;
					}
					
					/* Now we need to check that the box intersects
					  * with any other boxes
					  * Assume no intersection... */
					isect = 0;
					
					if (/* Constrain boxes to positive X/Y values */
						BOXLEFT(box)<0.0 || BOXBOTTOM(box)<0.0 ||
						/* check for last intersected */
						(vert->isect_cache[j] && BOXINTERSECT(box, vert->isect_cache[j]))
					   ) {
						/* Here we check that the last intersected
						 * box will intersect with this one using
						 * isect_cache that can store a pointer to a
						 * box for each quaderent
						 * big speedup */
						isect = 1;
					} else {
						/* do a full saech for colliding box
						 * this is realy slow, some spacialy divided
						 * datastructure would be better */
						for (box_test = boxarray; box_test != box; box_test++) {
							if BOXINTERSECT(box, box_test) {
								/* Store the last intersecting here
								 * as cache for faster checking next time around */
								vert->isect_cache[j] = box_test;
								isect = 1;
								break;
							}
						}
					}
					
					if (!isect) {
						
						/* maintain the total width and height */
						(*tot_width) = MAX2(BOXRIGHT(box), (*tot_width));
						(*tot_height) = MAX2(BOXTOP(box), (*tot_height));
						
						/* Place the box */
						vert->free &= ~quad_flags[j];
						
						switch (j) {
						case TR:
							box->v[BL]= vert;
							vert->trb = box;
						 	break;
						case TL:
							box->v[BR]= vert;
							vert->tlb = box;
						 	break;
						case BR:
							box->v[TL]= vert;
							vert->brb = box;
							break;
						case BL:
							box->v[TR]= vert;
							vert->blb = box;
						 	break;
						}
						
						/* Mask free flags for verts that are on the bottom or side
						 * so we dont get boxes outside the given rectangle ares
						 * 
						 * We can do an else/if here because only the first 
						 * box can be at the very bottom left corner */
						if (BOXLEFT(box) <= 0) {
							box->v[TL]->free &= ~(TLF|BLF);
							box->v[BL]->free &= ~(TLF|BLF);				
						} else if (BOXBOTTOM(box) <= 0) {
							box->v[BL]->free &= ~(BRF|BLF);
							box->v[BR]->free &= ~(BRF|BLF);
						}
						/* The following block of code does a logical
						 * check with 2 adjacent boxes, its possible to
						 * flag verts on one or both of the boxes 
						 * as being used by checking the width or
						 * height of both boxes */
						
						
						
						if (vert->tlb && vert->trb && (box == vert->tlb || box == vert->trb)) {
							if (vert->tlb->h > vert->trb->h) {
								vert->trb->v[TL]->free &= ~(TLF|BLF);
							} else if (vert->tlb->h < vert->trb->h) {
								vert->tlb->v[TR]->free &= ~(TRF|BRF);
							} else { /*same*/
								vert->tlb->v[TR]->free &= ~BLF;
								vert->trb->v[TL]->free &= ~BRF;
							}
						} else if (vert->blb && vert->brb && (box == vert->blb || box == vert->brb)) {
							if (vert->blb->h > vert->brb->h) {
								vert->brb->v[BL]->free &= ~(TLF|BLF);
							} else if (vert->blb->h < vert->brb->h) {
								vert->blb->v[BR]->free &= ~(TRF|BRF);
							} else { /*same*/
								vert->blb->v[BR]->free &= ~TRF;
								vert->brb->v[BL]->free &= ~TLF;
							}
						}
						/* Horizontal */
						if (vert->tlb && vert->blb && (box == vert->tlb || box == vert->blb)) {
							if (vert->tlb->w > vert->blb->w) {
								vert->blb->v[TL]->free &= ~(TLF|TRF);
							} else if (vert->tlb->w < vert->blb->w) {
								vert->tlb->v[BL]->free &= ~(BLF|BRF);
							} else { /*same*/
								vert->blb->v[TL]->free &= ~TRF;
								vert->tlb->v[BL]->free &= ~BRF;
							}
						} else if (vert->trb && vert->brb && (box == vert->trb || box == vert->brb)) {
							if (vert->trb->w > vert->brb->w) {
								vert->brb->v[TR]->free &= ~(TRF|TRF);
							} else if (vert->trb->w < vert->brb->w) {
								vert->trb->v[BR]->free &= ~(BLF|BRF);
							} else { /*same*/
								vert->brb->v[TR]->free &= ~TLF;
								vert->trb->v[BR]->free &= ~BLF;
							}
						}
						/* End logical check */
						
						
						for (k=0; k<4; k++) {
							if (box->v[k] != vert) {
								vertex_pack_indicies[verts_pack_len] = box->v[k]->index; 
								verts_pack_len++;
							}
						}
						/* The Box verts are only used interially
						 * Update the box x and y since thats what external
						 * functions will see */
						box->x = BOXLEFT(box);
						box->y = BOXBOTTOM(box);
					}
				}	
			}
		}
	}

	/* free all the verts, not realy needed because they shouldebt be
	 * touched anymore but accessing the pointers woud crash blender */
	for (box_index=0; box_index < len; box_index++) {
		box = boxarray+box_index; 
		box->v[0] = box->v[1] = box->v[2] = box->v[3] = NULL; 
	}
	MEM_freeN(vertex_pack_indicies);
	MEM_freeN(vertarray);
}

int boxPack_FromPyObject(PyObject * value, boxPack **boxarray )
{
	int len, i;
	PyObject *list_item, *item_1, *item_2;
	boxPack *box;
	
	
	/* Error checking must alredy be done */
	if( !PyList_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"can only back a list of [x,y,x,w]" );
	
	len = PyList_Size( value );
	
	(*boxarray) = MEM_mallocN( len*sizeof(boxPack), "boxPack box");
	
	
	for( i = 0; i < len; i++ ) {
		list_item = PyList_GET_ITEM( value, i );
		if( !PyList_Check( list_item ) || PyList_Size( list_item ) < 4 ) {
			MEM_freeN(*boxarray);
			return EXPP_ReturnIntError( PyExc_TypeError,
					"can only back a list of [x,y,x,w]" );
		}
		
		box = (*boxarray)+i;
		
		item_1 = PyList_GET_ITEM(list_item, 2);
		item_2 = PyList_GET_ITEM(list_item, 3);
		
		if (!PyNumber_Check(item_1) || !PyNumber_Check(item_2)) {
			MEM_freeN(*boxarray);
			return EXPP_ReturnIntError( PyExc_TypeError,
					"can only back a list of 2d boxes [x,y,x,w]" );
		}
		
		box->x = box->y = 0.0f;
		box->w =  (float)PyFloat_AsDouble( item_1 );
		box->h =  (float)PyFloat_AsDouble( item_2 );
		box->index = i;
		/* verts will be added later */
	}
	return 0;
}

void boxPack_ToPyObject(PyObject * value, boxPack **boxarray)
{
	int len, i;
	PyObject *list_item;
	boxPack *box;
	
	len = PyList_Size( value );
	
	for( i = 0; i < len; i++ ) {
		box = (*boxarray)+i;
		list_item = PyList_GET_ITEM( value, box->index );
		PyList_SET_ITEM( list_item, 0, PyFloat_FromDouble( box->x ));
		PyList_SET_ITEM( list_item, 1, PyFloat_FromDouble( box->y ));
	}
	MEM_freeN(*boxarray);
}


static PyObject *M_Geometry_BoxPack2D( PyObject * self, PyObject * args )
{
	PyObject *boxlist; /*return this list of tri's */
	boxPack *boxarray;
	float tot_width, tot_height;
	int len;
	int error;
	
	if(!PyArg_ParseTuple ( args, "O", &boxlist) || !PyList_Check(boxlist)) {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a sequence of boxes [[x,y,w,h], ... ]" );
	}
	
	len = PyList_Size( boxlist );
	
	if (!len)
		return Py_BuildValue( "ff", 0.0, 0.0);
	
	error = boxPack_FromPyObject(boxlist, &boxarray);
	if (error!=0)	return NULL;
	
	/* Non Python function */
	boxPackAll(boxarray, len, &tot_width, &tot_height);
	
	boxPack_ToPyObject(boxlist, &boxarray);
	
	return Py_BuildValue( "ff", tot_width, tot_height);
}
