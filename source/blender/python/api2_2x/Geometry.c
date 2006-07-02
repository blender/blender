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


//-------------------------DOC STRINGS ---------------------------
static char M_Geometry_doc[] = "The Blender Geometry module\n\n";
static char M_Geometry_PolyFill_doc[] = "(veclist_list) - takes a list of polylines (each point a vector) and returns the point indicies for a polyline filled with triangles";
//-----------------------METHOD DEFINITIONS ----------------------
struct PyMethodDef M_Geometry_methods[] = {
	{"PolyFill", ( PyCFunction ) M_Geometry_PolyFill, METH_VARARGS, M_Geometry_PolyFill_doc},
	{NULL, NULL, 0, NULL}
};
//----------------------------MODULE INIT-------------------------
PyObject *Geometry_Init(void)
{
	PyObject *submodule;

	submodule = Py_InitModule3("Blender.Geometry",
				    M_Geometry_methods, M_Geometry_doc);
	return (submodule);
}

//----------------------------------Geometry.PolyFill() -------------------
/* PolyFill function, uses Blenders scanfill to fill multiple poly lines */
PyObject *M_Geometry_PolyFill( PyObject * self, PyObject * args )
{
	PyObject *tri_list; /*return this list of tri's */
	PyObject *polyLineList, *polyLine, *polyVec;
	int i, len_polylines, len_polypoints;
	
	/* display listbase */
	ListBase dispbase={NULL, NULL};
	DispList *dl;
	float *fp; /*pointer to the array of malloced dl->verts to set the points from the vectors */
	int index, *dl_face, totpoints=0;
	
	
	dispbase.first= dispbase.last= NULL;
	
	
	if( !PyArg_ParseTuple ( args, "O!", &PyList_Type, &polyLineList) ) {
		freedisplist(&dispbase);
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a list of poly lines" );
	}
	
	
	if (EXPP_check_sequence_consistency( polyLineList, &PyList_Type ) != 1)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a list of lists of vectors" );
	
	len_polylines = PySequence_Size( polyLineList );
	
	for( i = 0; i < len_polylines; ++i ) {
		polyLine= PySequence_GetItem( polyLineList, i );
		
		len_polypoints= PySequence_Size( polyLine );
		if (len_polypoints>2) { /* dont bother adding edges as polylines */
			if (EXPP_check_sequence_consistency( polyLine, &vector_Type ) != 1)
				return EXPP_ReturnPyObjError( PyExc_TypeError,
					  "expected a list of poly lines" );
			
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
