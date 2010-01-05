/* 
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Geometry.h"

/*  - Not needed for now though other geometry functions will probably need them
#include "BLI_math.h"
#include "BKE_utildefines.h"
*/

/* Used for PolyFill */
#include "BKE_displist.h"
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
 
#include "BKE_utildefines.h"
#include "BKE_curve.h"
#include "BLI_boxpack2d.h"
#include "BLI_math.h"

#define SWAP_FLOAT(a,b,tmp) tmp=a; a=b; b=tmp
#define eps 0.000001

/*-- forward declarations -- */
static PyObject *M_Geometry_PolyFill( PyObject * self, PyObject * polyLineSeq );
static PyObject *M_Geometry_LineIntersect2D( PyObject * self, PyObject * args );
static PyObject *M_Geometry_ClosestPointOnLine( PyObject * self, PyObject * args );
static PyObject *M_Geometry_PointInTriangle2D( PyObject * self, PyObject * args );
static PyObject *M_Geometry_PointInQuad2D( PyObject * self, PyObject * args );
static PyObject *M_Geometry_BoxPack2D( PyObject * self, PyObject * args );
static PyObject *M_Geometry_BezierInterp( PyObject * self, PyObject * args );
static PyObject *M_Geometry_BarycentricTransform( PyObject * self, PyObject * args );


/*-------------------------DOC STRINGS ---------------------------*/
static char M_Geometry_doc[] = "The Blender Geometry module\n\n";
static char M_Geometry_PolyFill_doc[] = "(veclist_list) - takes a list of polylines (each point a vector) and returns the point indicies for a polyline filled with triangles";
static char M_Geometry_LineIntersect2D_doc[] = "(lineA_p1, lineA_p2, lineB_p1, lineB_p2) - takes 2 lines (as 4 vectors) and returns a vector for their point of intersection or None";
static char M_Geometry_ClosestPointOnLine_doc[] = "(pt, line_p1, line_p2) - takes a point and a line and returns a (Vector, float) for the point on the line, and the bool so you can know if the point was between the 2 points";
static char M_Geometry_PointInTriangle2D_doc[] = "(pt, tri_p1, tri_p2, tri_p3) - takes 4 vectors, one is the point and the next 3 define the triangle, only the x and y are used from the vectors";
static char M_Geometry_PointInQuad2D_doc[] = "(pt, quad_p1, quad_p2, quad_p3, quad_p4) - takes 5 vectors, one is the point and the next 4 define the quad, only the x and y are used from the vectors";
static char M_Geometry_BoxPack2D_doc[] = "";
static char M_Geometry_BezierInterp_doc[] = "";
/*-----------------------METHOD DEFINITIONS ----------------------*/
struct PyMethodDef M_Geometry_methods[] = {
	{"PolyFill", ( PyCFunction ) M_Geometry_PolyFill, METH_O, M_Geometry_PolyFill_doc},
	{"LineIntersect2D", ( PyCFunction ) M_Geometry_LineIntersect2D, METH_VARARGS, M_Geometry_LineIntersect2D_doc},
	{"ClosestPointOnLine", ( PyCFunction ) M_Geometry_ClosestPointOnLine, METH_VARARGS, M_Geometry_ClosestPointOnLine_doc},
	{"PointInTriangle2D", ( PyCFunction ) M_Geometry_PointInTriangle2D, METH_VARARGS, M_Geometry_PointInTriangle2D_doc},
	{"PointInQuad2D", ( PyCFunction ) M_Geometry_PointInQuad2D, METH_VARARGS, M_Geometry_PointInQuad2D_doc},
	{"BoxPack2D", ( PyCFunction ) M_Geometry_BoxPack2D, METH_O, M_Geometry_BoxPack2D_doc},
	{"BezierInterp", ( PyCFunction ) M_Geometry_BezierInterp, METH_VARARGS, M_Geometry_BezierInterp_doc},
	{"BarycentricTransform", ( PyCFunction ) M_Geometry_BarycentricTransform, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef M_Geometry_module_def = {
	PyModuleDef_HEAD_INIT,
	"Geometry",  /* m_name */
	M_Geometry_doc,  /* m_doc */
	0,  /* m_size */
	M_Geometry_methods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

/*----------------------------MODULE INIT-------------------------*/
PyObject *Geometry_Init(void)
{
	PyObject *submodule;
	
	submodule = PyModule_Create(&M_Geometry_module_def);
	PyDict_SetItemString(PySys_GetObject("modules"), M_Geometry_module_def.m_name, submodule);
	
	return (submodule);
}

/*----------------------------------Geometry.PolyFill() -------------------*/
/* PolyFill function, uses Blenders scanfill to fill multiple poly lines */
static PyObject *M_Geometry_PolyFill( PyObject * self, PyObject * polyLineSeq )
{
	PyObject *tri_list; /*return this list of tri's */
	PyObject *polyLine, *polyVec;
	int i, len_polylines, len_polypoints, ls_error = 0;
	
	/* display listbase */
	ListBase dispbase={NULL, NULL};
	DispList *dl;
	float *fp; /*pointer to the array of malloced dl->verts to set the points from the vectors */
	int index, *dl_face, totpoints=0;
	
	
	dispbase.first= dispbase.last= NULL;
	
	
	if(!PySequence_Check(polyLineSeq)) {
		PyErr_SetString( PyExc_TypeError, "expected a sequence of poly lines" );
		return NULL;
	}
	
	len_polylines = PySequence_Size( polyLineSeq );
	
	for( i = 0; i < len_polylines; ++i ) {
		polyLine= PySequence_GetItem( polyLineSeq, i );
		if (!PySequence_Check(polyLine)) {
			freedisplist(&dispbase);
			Py_XDECREF(polyLine); /* may be null so use Py_XDECREF*/
			PyErr_SetString( PyExc_TypeError, "One or more of the polylines is not a sequence of Mathutils.Vector's" );
			return NULL;
		}
		
		len_polypoints= PySequence_Size( polyLine );
		if (len_polypoints>0) { /* dont bother adding edges as polylines */
#if 0
			if (EXPP_check_sequence_consistency( polyLine, &vector_Type ) != 1) {
				freedisplist(&dispbase);
				Py_DECREF(polyLine);
				PyErr_SetString( PyExc_TypeError, "A point in one of the polylines is not a Mathutils.Vector type" );
				return NULL;
			}
#endif
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
				if(VectorObject_Check(polyVec)) {
					
					if(!BaseMath_ReadCallback((VectorObject *)polyVec))
						ls_error= 1;
					
					fp[0] = ((VectorObject *)polyVec)->vec[0];
					fp[1] = ((VectorObject *)polyVec)->vec[1];
					if( ((VectorObject *)polyVec)->size > 2 )
						fp[2] = ((VectorObject *)polyVec)->vec[2];
					else
						fp[2]= 0.0f; /* if its a 2d vector then set the z to be zero */
				}
				else {
					ls_error= 1;
				}
				
				totpoints++;
				Py_DECREF(polyVec);
			}
		}
		Py_DECREF(polyLine);
	}
	
	if(ls_error) {
		freedisplist(&dispbase); /* possible some dl was allocated */
		PyErr_SetString( PyExc_TypeError, "A point in one of the polylines is not a Mathutils.Vector type" );
		return NULL;
	}
	else if (totpoints) {
		/* now make the list to return */
		filldisplist(&dispbase, &dispbase);
		
		/* The faces are stored in a new DisplayList
		thats added to the head of the listbase */
		dl= dispbase.first; 
		
		tri_list= PyList_New(dl->parts);
		if( !tri_list ) {
			freedisplist(&dispbase);
			PyErr_SetString( PyExc_RuntimeError, "Geometry.PolyFill failed to make a new list" );
			return NULL;
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
		freedisplist(&dispbase); /* possible some dl was allocated */
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
	) {
		PyErr_SetString( PyExc_TypeError, "expected 4 vector types\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(line_a1) || !BaseMath_ReadCallback(line_a2) || !BaseMath_ReadCallback(line_b1) || !BaseMath_ReadCallback(line_b2))
		return NULL;
	
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
	if (fabs(b1x - b2x) < eps || fabs(b1y - b2y) < eps) {
		SWAP_FLOAT(a1x, b1x, xi); /*abuse xi*/
		SWAP_FLOAT(a1y, b1y, xi);
		SWAP_FLOAT(a2x, b2x, xi);
		SWAP_FLOAT(a2y, b2y, xi);
	}
	
	if (fabs(a1x-a2x) < eps) { /* verticle line */
		if (fabs(b1x-b2x) < eps){ /*verticle second line */
			Py_RETURN_NONE; /* 2 verticle lines dont intersect. */
		}
		else if (fabs(b1y-b2y) < eps) {
			/*X of vert, Y of hoz. no calculation needed */
			newvec[0]= a1x;
			newvec[1]= b1y;
			return newVectorObject(newvec, 2, Py_NEW, NULL);
		}
		
		yi = (float)(((b1y / fabs(b1x - b2x)) * fabs(b2x - a1x)) + ((b2y / fabs(b1x - b2x)) * fabs(b1x - a1x)));
		
		if (yi > MAX2(a1y, a2y)) {/* New point above seg1's vert line */
			Py_RETURN_NONE;
		} else if (yi < MIN2(a1y, a2y)) { /* New point below seg1's vert line */
			Py_RETURN_NONE;
		}
		newvec[0]= a1x;
		newvec[1]= yi;
		return newVectorObject(newvec, 2, Py_NEW, NULL);
	} else if (fabs(a2y-a1y) < eps) {  /* hoz line1 */
		if (fabs(b2y-b1y) < eps) { /*hoz line2*/
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
		return newVectorObject(newvec, 2, Py_NEW, NULL);
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
		return newVectorObject(newvec, 2, Py_NEW, NULL);
	}
	Py_RETURN_NONE;
}

static PyObject *M_Geometry_ClosestPointOnLine( PyObject * self, PyObject * args )
{
	VectorObject *pt, *line_1, *line_2;
	float pt_in[3], pt_out[3], l1[3], l2[3];
	float lambda;
	PyObject *ret;
	
	if( !PyArg_ParseTuple ( args, "O!O!O!",
	&vector_Type, &pt,
	&vector_Type, &line_1,
	&vector_Type, &line_2)
	  ) {
		PyErr_SetString( PyExc_TypeError, "expected 3 vector types\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(pt) || !BaseMath_ReadCallback(line_1) || !BaseMath_ReadCallback(line_2))
		return NULL;
	
	/* accept 2d verts */
	if (pt->size==3) { VECCOPY(pt_in, pt->vec);}
	else { pt_in[2]=0.0;	VECCOPY2D(pt_in, pt->vec) }
	
	if (line_1->size==3) { VECCOPY(l1, line_1->vec);}
	else { l1[2]=0.0;	VECCOPY2D(l1, line_1->vec) }
	
	if (line_2->size==3) { VECCOPY(l2, line_2->vec);}
	else { l2[2]=0.0;	VECCOPY2D(l2, line_2->vec) }
	
	/* do the calculation */
	lambda = closest_to_line_v3( pt_out,pt_in, l1, l2);
	
	ret = PyTuple_New(2);
	PyTuple_SET_ITEM( ret, 0, newVectorObject(pt_out, 3, Py_NEW, NULL) );
	PyTuple_SET_ITEM( ret, 1, PyFloat_FromDouble(lambda) );
	return ret;
}

static PyObject *M_Geometry_PointInTriangle2D( PyObject * self, PyObject * args )
{
	VectorObject *pt_vec, *tri_p1, *tri_p2, *tri_p3;
	
	if( !PyArg_ParseTuple ( args, "O!O!O!O!",
	  &vector_Type, &pt_vec,
	  &vector_Type, &tri_p1,
	  &vector_Type, &tri_p2,
	  &vector_Type, &tri_p3)
	) {
		PyErr_SetString( PyExc_TypeError, "expected 4 vector types\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(pt_vec) || !BaseMath_ReadCallback(tri_p1) || !BaseMath_ReadCallback(tri_p2) || !BaseMath_ReadCallback(tri_p3))
		return NULL;
	
	return PyLong_FromLong(isect_point_tri_v2(pt_vec->vec, tri_p1->vec, tri_p2->vec, tri_p3->vec));
}

static PyObject *M_Geometry_PointInQuad2D( PyObject * self, PyObject * args )
{
	VectorObject *pt_vec, *quad_p1, *quad_p2, *quad_p3, *quad_p4;
	
	if( !PyArg_ParseTuple ( args, "O!O!O!O!O!",
	  &vector_Type, &pt_vec,
	  &vector_Type, &quad_p1,
	  &vector_Type, &quad_p2,
	  &vector_Type, &quad_p3,
	  &vector_Type, &quad_p4)
	) {
		PyErr_SetString( PyExc_TypeError, "expected 5 vector types\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(pt_vec) || !BaseMath_ReadCallback(quad_p1) || !BaseMath_ReadCallback(quad_p2) || !BaseMath_ReadCallback(quad_p3) || !BaseMath_ReadCallback(quad_p4))
		return NULL;
	
	return PyLong_FromLong(isect_point_quad_v2(pt_vec->vec, quad_p1->vec, quad_p2->vec, quad_p3->vec, quad_p4->vec));
}

static int boxPack_FromPyObject(PyObject * value, boxPack **boxarray )
{
	int len, i;
	PyObject *list_item, *item_1, *item_2;
	boxPack *box;
	
	
	/* Error checking must alredy be done */
	if( !PyList_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "can only back a list of [x,y,x,w]" );
		return -1;
	}
	
	len = PyList_Size( value );
	
	(*boxarray) = MEM_mallocN( len*sizeof(boxPack), "boxPack box");
	
	
	for( i = 0; i < len; i++ ) {
		list_item = PyList_GET_ITEM( value, i );
		if( !PyList_Check( list_item ) || PyList_Size( list_item ) < 4 ) {
			MEM_freeN(*boxarray);
			PyErr_SetString( PyExc_TypeError, "can only back a list of [x,y,x,w]" );
			return -1;
		}
		
		box = (*boxarray)+i;
		
		item_1 = PyList_GET_ITEM(list_item, 2);
		item_2 = PyList_GET_ITEM(list_item, 3);
		
		if (!PyNumber_Check(item_1) || !PyNumber_Check(item_2)) {
			MEM_freeN(*boxarray);
			PyErr_SetString( PyExc_TypeError, "can only back a list of 2d boxes [x,y,x,w]" );
			return -1;
		}
		
		box->w =  (float)PyFloat_AsDouble( item_1 );
		box->h =  (float)PyFloat_AsDouble( item_2 );
		box->index = i;
		/* verts will be added later */
	}
	return 0;
}

static void boxPack_ToPyObject(PyObject * value, boxPack **boxarray)
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


static PyObject *M_Geometry_BoxPack2D( PyObject * self, PyObject * boxlist )
{
	boxPack *boxarray = NULL;
	float tot_width, tot_height;
	int len;
	int error;
	
	if(!PyList_Check(boxlist)) {
		PyErr_SetString( PyExc_TypeError, "expected a sequence of boxes [[x,y,w,h], ... ]" );
		return NULL;
	}
	
	len = PyList_Size( boxlist );
	
	if (!len)
		return Py_BuildValue( "ff", 0.0, 0.0);
	
	error = boxPack_FromPyObject(boxlist, &boxarray);
	if (error!=0)	return NULL;
	
	/* Non Python function */
	boxPack2D(boxarray, len, &tot_width, &tot_height);
	
	boxPack_ToPyObject(boxlist, &boxarray);
	
	return Py_BuildValue( "ff", tot_width, tot_height);
}

static PyObject *M_Geometry_BezierInterp( PyObject * self, PyObject * args )
{
	VectorObject *vec_k1, *vec_h1, *vec_k2, *vec_h2;
	int resolu;
	int dims;
	int i;
	float *coord_array, *fp;
	PyObject *list;
	
	float k1[4] = {0.0, 0.0, 0.0, 0.0};
	float h1[4] = {0.0, 0.0, 0.0, 0.0};
	float k2[4] = {0.0, 0.0, 0.0, 0.0};
	float h2[4] = {0.0, 0.0, 0.0, 0.0};
	
	
	if( !PyArg_ParseTuple ( args, "O!O!O!O!i",
	  &vector_Type, &vec_k1,
	  &vector_Type, &vec_h1,
	  &vector_Type, &vec_h2,
	  &vector_Type, &vec_k2, &resolu) || (resolu<=1)
	) {
		PyErr_SetString( PyExc_TypeError, "expected 4 vector types and an int greater then 1\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec_k1) || !BaseMath_ReadCallback(vec_h1) || !BaseMath_ReadCallback(vec_k2) || !BaseMath_ReadCallback(vec_h2))
		return NULL;
	
	dims= MAX4(vec_k1->size, vec_h1->size, vec_h2->size, vec_k2->size);
	
	for(i=0; i < vec_k1->size; i++) k1[i]= vec_k1->vec[i];
	for(i=0; i < vec_h1->size; i++) h1[i]= vec_h1->vec[i];
	for(i=0; i < vec_k2->size; i++) k2[i]= vec_k2->vec[i];
	for(i=0; i < vec_h2->size; i++) h2[i]= vec_h2->vec[i];
	
	coord_array = MEM_callocN(dims * (resolu) * sizeof(float), "BezierInterp");
	for(i=0; i<dims; i++) {
		forward_diff_bezier(k1[i], h1[i], h2[i], k2[i], coord_array+i, resolu-1, sizeof(float)*dims);
	}
	
	list= PyList_New(resolu);
	fp= coord_array;
	for(i=0; i<resolu; i++, fp= fp+dims) {
		PyList_SET_ITEM(list, i, newVectorObject(fp, dims, Py_NEW, NULL));
	}
	MEM_freeN(coord_array);
	return list;
}

static PyObject *M_Geometry_BarycentricTransform(PyObject * self, PyObject * args)
{
	VectorObject *vec_pt;
	VectorObject *vec_t1_tar, *vec_t2_tar, *vec_t3_tar;
	VectorObject *vec_t1_src, *vec_t2_src, *vec_t3_src;
	float vec[3];

	if( !PyArg_ParseTuple ( args, "O!O!O!O!O!O!O!",
	  &vector_Type, &vec_pt,
	  &vector_Type, &vec_t1_src,
	  &vector_Type, &vec_t2_src,
	  &vector_Type, &vec_t3_src,
	  &vector_Type, &vec_t1_tar,
	  &vector_Type, &vec_t2_tar,
	  &vector_Type, &vec_t3_tar) || (	vec_pt->size != 3 ||
										vec_t1_src->size != 3 ||
										vec_t2_src->size != 3 ||
										vec_t3_src->size != 3 ||
										vec_t1_tar->size != 3 ||
										vec_t2_tar->size != 3 ||
										vec_t3_tar->size != 3)
	) {
		PyErr_SetString( PyExc_TypeError, "expected 7, 3D vector types\n" );
		return NULL;
	}

	barycentric_transform(vec, vec_pt->vec,
			vec_t1_tar->vec, vec_t2_tar->vec, vec_t3_tar->vec,
			vec_t1_src->vec, vec_t2_src->vec, vec_t3_src->vec);

	return newVectorObject(vec, 3, Py_NEW, NULL);
}
