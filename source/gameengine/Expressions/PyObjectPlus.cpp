/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef NO_EXP_PYTHON_EMBEDDING

/*------------------------------
 * PyObjectPlus cpp
 *
 * C++ library routines for Crawl 3.2
 *
 * Derived from work by
 * David Redish
 * graduate student
 * Computer Science Department 
 * Carnegie Mellon University (CMU)
 * Center for the Neural Basis of Cognition (CNBC) 
 * http://www.python.org/doc/PyCPP.html
 *
------------------------------*/
#include <MT_assert.h>
#include "stdlib.h"
#include "PyObjectPlus.h"
#include "STR_String.h"
/*------------------------------
 * PyObjectPlus Type		-- Every class, even the abstract one should have a Type
------------------------------*/

PyTypeObject PyObjectPlus::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/*ob_size*/
	"PyObjectPlus",			/*tp_name*/
	sizeof(PyObjectPlus),		/*tp_basicsize*/
	0,				/*tp_itemsize*/
	/* methods */
	PyDestructor,	  		/*tp_dealloc*/
	0,			 	/*tp_print*/
	__getattr, 			/*tp_getattr*/
	__setattr, 			/*tp_setattr*/
	0,			        /*tp_compare*/
	__repr,			        /*tp_repr*/
	0,			        /*tp_as_number*/
	0,		 	        /*tp_as_sequence*/
	0,			        /*tp_as_mapping*/
	0,			        /*tp_hash*/
	0,				/*tp_call */
};

PyObjectPlus::~PyObjectPlus()
{
	if (ob_refcnt)
	{
		_Py_ForgetReference(this);
	}
//	assert(ob_refcnt==0);
}

PyObjectPlus::PyObjectPlus(PyTypeObject *T) 				// constructor
{
	MT_assert(T != NULL);
	this->ob_type = T; 
	_Py_NewReference(this);
};
  
/*------------------------------
 * PyObjectPlus Methods 	-- Every class, even the abstract one should have a Methods
------------------------------*/
PyMethodDef PyObjectPlus::Methods[] = {
  {"isA",		 (PyCFunction) sPy_isA,			METH_VARARGS},
  {NULL, NULL}		/* Sentinel */
};

/*------------------------------
 * PyObjectPlus Parents		-- Every class, even the abstract one should have parents
------------------------------*/
PyParentObject PyObjectPlus::Parents[] = {&PyObjectPlus::Type, NULL};

/*------------------------------
 * PyObjectPlus attributes	-- attributes
------------------------------*/
PyObject *PyObjectPlus::_getattr(const STR_String& attr)
{
	if (attr == "__doc__" && GetType()->tp_doc)
		return PyString_FromString(GetType()->tp_doc);

  //if (streq(attr, "type"))
  //  return Py_BuildValue("s", (*(GetParents()))->tp_name);

  return Py_FindMethod(Methods, this, const_cast<char *>(attr.ReadPtr()));
}

int PyObjectPlus::_delattr(const STR_String& attr)
{
	PyErr_SetString(PyExc_AttributeError, "attribute cant be deleted");
	return 1;
}

int PyObjectPlus::_setattr(const STR_String& attr, PyObject *value)
{
	//return PyObject::_setattr(attr,value);
	//cerr << "Unknown attribute" << endl;
	PyErr_SetString(PyExc_AttributeError, "attribute cant be set");
	return 1;
}

/*------------------------------
 * PyObjectPlus repr		-- representations
------------------------------*/
PyObject *PyObjectPlus::_repr(void)
{
	PyErr_SetString(PyExc_SystemError, "Representation not overridden by object.");  
	return NULL;
}

/*------------------------------
 * PyObjectPlus isA		-- the isA functions
------------------------------*/
bool PyObjectPlus::isA(PyTypeObject *T)		// if called with a Type, use "typename"
{
  return isA(T->tp_name);
}


bool PyObjectPlus::isA(const char *mytypename)		// check typename of each parent
{
  int i;
  PyParentObject  P;
  PyParentObject *Ps = GetParents();
  
  for (P = Ps[i=0]; P != NULL; P = Ps[i++])
  {
      if (STR_String(P->tp_name) == STR_String(mytypename)	)
		  return true;
  }
	
  return false;
}

PyObject *PyObjectPlus::Py_isA(PyObject *args)		// Python wrapper for isA
{
  char *mytypename;
  if (!PyArg_ParseTuple(args, "s", &mytypename))
    return NULL;
  if(isA(mytypename))
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}

#endif //NO_EXP_PYTHON_EMBEDDING

