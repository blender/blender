/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
$Id$

This is code to emulate a readonly Dictionary/Class object 
for storage of Constants 

Inserting readonly values:

PyObject *constants = ConstObject_New();
insertConst(constants, "item", PyInt_FromInt(CONSTANT));
...


Constant values are accessed in python by either:

c = module.Const.CONSTANT   

   or

c = module.Const['CONSTANT']

*/

#include "Python.h"
#include "BPY_macros.h"

#include "BPY_constobject.h"

#define Const_Check(v)       ((v)->ob_type == &Const_Type)


/* ----------------------------------------------------- */
/* Declarations for objects of type const */


PyTypeObject Const_Type;

/* PROTOS */



static constobject *
newconstobject()
{
	constobject *self;
	Const_Type.ob_type = &PyType_Type;
	self = PyObject_NEW(constobject, &Const_Type);
	if (self == NULL)
		return NULL;
	self->dict = PyDict_New();	
	return self;
}

char ConstObject_doc[] = "Readonly dictionary type\n\n\
This is used as a container for constants, which can be accessed by two ways:\n\
\n\
    c = <ConstObject>.<attribute>\n\
\n\
or\n\
    c = <ConstObject>['<attribute>']";

PyObject *ConstObject_New(void)
{
	return (PyObject *) newconstobject();
}	

PyObject *const_repr(constobject *self) 
{
	PyObject *repr;
	repr = PyObject_Repr(self->dict);
	return repr;
}

static void const_dealloc(PyObject *self) {
	Py_DECREF(((constobject *)self)->dict);
	PyMem_DEL(self);
}
static PyObject *
const_getattr(constobject *self, char *name)
{
	PyObject *item;
	if (STREQ(name, "__doc__")) {
		return PyString_FromString(ConstObject_doc);
	}	
	if (STREQ(name, "__members__")) {
		return PyDict_Keys(self->dict);
	}	
	item = PyDict_GetItemString(self->dict, name);	/* borrowed ref ! */
	if (item)
		Py_INCREF(item);
	if (!item) {
		PyErr_SetString(PyExc_AttributeError, name);
	}
	return item;
}

/* inserts a constant with name into the dictionary self */
void insertConst(PyObject *self, char *name, PyObject *cnst)
{
	PyDict_SetItemString(((constobject *)self)->dict, name, cnst);
}


/* Code to access const objects as mappings */

static int
const_length(constobject *self)
{
	return 0;
}

static PyObject *
const_subscript(constobject *self, PyObject *key)
{
	PyObject *item;
	item =  PyDict_GetItem(self->dict, key);	
	if (item) 
		Py_INCREF(item);
	return item;
}

static int
const_ass_sub(constobject *self, PyObject *v, PyObject *w)
{
	/* no write access */
	return 0;
}

static PyMappingMethods const_as_mapping = {
	(inquiry)const_length,		/*mp_length*/
	(binaryfunc)const_subscript,		/*mp_subscript*/
	(objobjargproc)const_ass_sub,	/*mp_ass_subscript*/
};

/* -------------------------------------------------------- */

PyTypeObject Const_Type = {
	PyObject_HEAD_INIT(NULL)
	0,                              /*ob_size*/
	"const",                        /*tp_name*/
	sizeof(constobject),            /*tp_basicsize*/
	0,                              /*tp_itemsize*/
	/* methods */
	(destructor)     const_dealloc, /*tp_dealloc*/
	(printfunc)      0,   /*tp_print*/
	(getattrfunc)    const_getattr,	/*tp_getattr*/
	(setattrfunc)    0,             /*tp_setattr*/
	(cmpfunc)        0,             /*tp_compare*/
	(reprfunc)       const_repr,             /*tp_repr*/
	                 0,             /*tp_as_number*/
	                 0,             /*tp_as_sequence*/
	                 &const_as_mapping,	/*tp_as_mapping*/
	                 0,	            /*tp_hash*/
};

