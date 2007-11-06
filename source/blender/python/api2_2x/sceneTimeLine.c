/* $Id$
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
 * Contributor(s): Joilnen Leite
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BLI_blenlib.h>
#include "Scene.h"

#include <stdio.h>
#include <MEM_guardedalloc.h>	/* for MEM_callocN */

#include "gen_utils.h"
#include "sceneTimeLine.h"

// static PyObject *TimeLine_New (PyObject *self);
static PyObject *M_TimeLine_Get (PyObject *self, PyObject *args);

static char M_TimeLine_Get_doc[]= "Return the Scene.TimeLine.";

//----------------------Scene.TimeMarker subsubmodule method def----------------------------
struct PyMethodDef M_TimeLine_methods[]= {
//	{"New", (PyCFunction) M_TimeMarker_New, METH_NOVAR,
//	 M_TimeLine_New_doc},
	{"Get", (PyCFunction) M_TimeLine_Get, METH_VARARGS,
	 M_TimeLine_Get_doc},
	{NULL, NULL, 0, NULL}
};

static PyObject *TimeLine_addMarker (BPy_TimeLine *self, PyObject *args);
static PyObject *TimeLine_delMarker (BPy_TimeLine *self, PyObject *args);
static PyObject *TimeLine_setNameMarker (BPy_TimeLine *self, PyObject *args);
static PyObject *TimeLine_getNameMarker (BPy_TimeLine *self, PyObject *args);
static PyObject *TimeLine_getFramesMarked (BPy_TimeLine *self, PyObject *args);

static PyObject *TimeLine_repr (BPy_TimeLine *self) {

	return PyString_FromFormat ("[TimeLine]");
}

static PyMethodDef BPy_TimeLine_methods[] = {
	{"add", (PyCFunction) TimeLine_addMarker,
	 METH_VARARGS,
	 "() - Add timemarker"},
	{"delete", (PyCFunction) TimeLine_delMarker,
	 METH_VARARGS,
	 "() - delete timemarker"},
	{"setName", (PyCFunction) TimeLine_setNameMarker,
	 METH_VARARGS,
	 "() - Get timemarker name"},
	{"getName", (PyCFunction) TimeLine_getNameMarker,
	 METH_VARARGS,
	 "() - Set timemarker name"},
	{"getMarked", (PyCFunction) TimeLine_getFramesMarked,
	 METH_VARARGS,
	 "() - Get frames timemarked"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------dealloc----------------------------------------*/
static void TimeLine_dealloc( BPy_TimeLine * self )
{
	PyObject_DEL( self );
}

/*-----------------------getattr----------------------------------------*/
static PyObject *TimeLine_getattr (BPy_TimeLine *self, char *name) {
	return Py_FindMethod( BPy_TimeLine_methods, ( PyObject * ) self, name );
}

/*-----------------------setattr----------------------------------------*/
static int TimeLine_setattr (BPy_TimeLine *self, char *name, PyObject *value) {
	PyObject *valtuple;
	PyObject *error= NULL;

	valtuple= Py_BuildValue ("(O)", value);

	if (!valtuple)
		return EXPP_ReturnIntError( PyExc_MemoryError,
	    		"TimeLineSetAttr: couldn't create tuple" );
	if( strcmp( name, "name" ) == 0 )
		error = TimeLine_setNameMarker (self, valtuple);
	Py_DECREF (valtuple);
	if (error != Py_None)
		return -1;

	Py_DECREF (Py_None);
	return 0;	
}

//-----------------------BPy_Scene method def------------------------------
PyTypeObject TimeLine_Type = {
	PyObject_HEAD_INIT (NULL) 0,	/* ob_size */
	"TimeLine",			/* tp_name */
	sizeof (BPy_TimeLine),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) TimeLine_dealloc,	/* tp_dealloc */
	(printfunc) 0,	/* tp_print */
	(getattrfunc) TimeLine_getattr,	/* tp_getattr */
	(setattrfunc) TimeLine_setattr,	/* tp_setattr */
	0,
	(reprfunc) TimeLine_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_hash */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_hash */
	0,0,0,0,0,0,0,0,0,
	BPy_TimeLine_methods,
	0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

PyObject *TimeLine_Init (void) 
{
	PyObject *submodule;

	if (PyType_Ready (&TimeLine_Type) < 0)
		return NULL;
	submodule= Py_InitModule3 ("Blender.Scene.TimeLine", M_TimeLine_methods,
			"The Blender TimeLine subsubmodule");

	return submodule;
}

PyObject *TimeLine_CreatePyObject (BPy_TimeLine *tl) {
	BPy_TimeLine *bl_tl;

	bl_tl= (BPy_TimeLine *) PyObject_NEW (BPy_TimeLine, &TimeLine_Type);

	return (( PyObject * ) bl_tl);
}


PyObject *M_TimeLine_Get (PyObject *self, PyObject *args) {

	return EXPP_incr_ret (Py_None);
}

static PyObject *TimeLine_getFramesMarked (BPy_TimeLine *self, PyObject *args) {

	PyObject *marker_dict= NULL;
	TimeMarker *marker_it= NULL;
	PyObject *tmarker= NULL, *pyo= NULL, *tmpstr;

	if (!PyArg_ParseTuple (args, "|O", &tmarker))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
					      "expected nothing, string or int as arguments.");
	if (tmarker) {
		char s[64];
		int frm= 0;

		if (PyString_Check (tmarker) && (BLI_strncpy(s, PyString_AsString (tmarker), 64)) ) {
			for (marker_it= self->marker_list->first; marker_it; marker_it= marker_it->next)
				if (!strcmp (marker_it->name, s)) {
					frm= (int)marker_it->frame;
					break;
				}
		}
		else if (PyInt_Check (tmarker))
			frm= (int)PyInt_AS_LONG (tmarker);
		else
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
					      "expected nothing, string or int as arguments.");
		if (frm>0) {
			marker_dict= PyDict_New ();
			for (marker_it= self->marker_list->first; marker_it; marker_it= marker_it->next){
				if (marker_it->frame==frm) {
					pyo= PyDict_GetItem ((PyObject*)marker_dict, PyInt_FromLong ((long int)marker_it->frame));
					tmpstr = PyString_FromString(marker_it->name);
					if (pyo) {
						PyList_Append (pyo, tmpstr);
						Py_INCREF(pyo);
					}else{
						pyo = PyList_New(0);
						PyList_Append (pyo, tmpstr);
					}
					Py_DECREF(tmpstr);
					
					PyDict_SetItem (marker_dict, PyInt_FromLong ((long int)marker_it->frame), pyo); 
					if (pyo) { 
						Py_DECREF (pyo); 
						pyo= NULL; 
					}
				}
			}
		}

	}else {
		marker_dict= PyDict_New ();
		for (marker_it= self->marker_list->first; marker_it; marker_it= marker_it->next) {
			pyo=PyDict_GetItem ((PyObject*)marker_dict, PyInt_FromLong ((long int)marker_it->frame));
			tmpstr = PyString_FromString(marker_it->name);
			if (pyo) {
				PyList_Append (pyo, tmpstr);
				Py_INCREF (pyo);
			}else{ 
				pyo= PyList_New (0);
				PyList_Append (pyo, tmpstr);
			}
			Py_DECREF(tmpstr);
			
			PyDict_SetItem (marker_dict, PyInt_FromLong ((long int)marker_it->frame), pyo); 
			if (pyo) { 
				Py_DECREF (pyo); 
				pyo= NULL; 
			}
		}
	}

	return marker_dict;
}

static PyObject *TimeLine_addMarker (BPy_TimeLine *self, PyObject *args) {
	int frame= 0;
	TimeMarker *marker= NULL, *marker_it= NULL;

	if (!PyArg_ParseTuple( args, "i", &frame ))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
		      "expected int as argument.");
	/* two markers can't be at the same place */
	for (marker_it= self->marker_list->first; marker_it; marker_it= marker_it->next) {
		if (marker_it->frame==frame)
			return EXPP_incr_ret (Py_None);
	}
	if (frame<self->sfra || frame>self->efra)
		return EXPP_ReturnPyObjError (PyExc_TypeError, "frame out of range.");
	marker= MEM_callocN (sizeof(TimeMarker), "TimeMarker");
	if (!marker) return EXPP_incr_ret (Py_None); 
	marker->frame= frame;
	BLI_addtail (self->marker_list, marker);
	return EXPP_incr_ret (Py_None);
}

static PyObject *TimeLine_delMarker (BPy_TimeLine *self, PyObject *args) {
	int frame= 0;
	TimeMarker *marker= NULL;
	
	if (!PyArg_ParseTuple (args, "|i", &frame))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"expected int as argument.");

	for (marker= self->marker_list->first; marker; marker= marker->next) {
		if (!frame)
			BLI_freelinkN (self->marker_list, marker);
		else if (marker->frame == frame) {
			BLI_freelinkN (self->marker_list, marker);
			return EXPP_incr_ret (Py_None);
		}
	}

	return EXPP_incr_ret (Py_None);
}

static PyObject *TimeLine_setNameMarker (BPy_TimeLine *self, PyObject *args) {
	char *buf;
	char name[64];
	int frame= 0;
	TimeMarker *marker= NULL;
	
	if (!PyArg_ParseTuple( args, "is", &frame, &buf))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
					      "expected int as argument.");
	PyOS_snprintf (name, sizeof (name), "%s", buf);
	for (marker= self->marker_list->first; marker; marker= marker->next) {
		if (marker->frame == frame) {
			BLI_strncpy(marker->name, name, sizeof(marker->name));
			return EXPP_incr_ret (Py_None);
		}
	}

	return EXPP_ReturnPyObjError (PyExc_TypeError, "frame not marked.");
}

static PyObject *TimeLine_getNameMarker (BPy_TimeLine *self, PyObject *args) {
	int frame= 0;
	TimeMarker *marker;

	if (!PyArg_ParseTuple (args, "i", &frame))
		return EXPP_ReturnPyObjError (PyExc_TypeError, "expected int as argument.");
	
	for (marker= self->marker_list->first; marker; marker= marker->next) {
		if (marker->frame == frame)
			return PyString_FromString (marker->name);
	}

	return EXPP_ReturnPyObjError (PyExc_TypeError, "frame not marked.");
}


