/**
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
#include <Python.h>
#include "KX_ConstraintWrapper.h"
#include "PHY_IPhysicsEnvironment.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_ConstraintWrapper::KX_ConstraintWrapper(
						PHY_ConstraintType ctype,
						int constraintId,
						PHY_IPhysicsEnvironment* physenv,PyTypeObject *T) :
		PyObjectPlus(T),
		m_constraintId(constraintId),
		m_constraintType(ctype),
		m_physenv(physenv)
{
}
KX_ConstraintWrapper::~KX_ConstraintWrapper()
{
}
//python integration methods
PyObject* KX_ConstraintWrapper::PyTestMethod(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	
	Py_RETURN_NONE;
}

PyObject* KX_ConstraintWrapper::PyGetConstraintId(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	return PyInt_FromLong(m_constraintId);
}




//python specific stuff
PyTypeObject KX_ConstraintWrapper::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_ConstraintWrapper",
		sizeof(KX_ConstraintWrapper),
		0,
		PyDestructor,
		0,
		__getattr,
		__setattr,
		0, //&MyPyCompare,
		__repr,
		0, //&cvalue_as_number,
		0,
		0,
		0,
		0
};

PyParentObject KX_ConstraintWrapper::Parents[] = {
	&KX_ConstraintWrapper::Type,
	NULL
};

PyObject*	KX_ConstraintWrapper::_getattr(const STR_String& attr)
{
	//here you can search for existing data members (like mass,friction etc.)
	_getattr_up(PyObjectPlus);
}

int	KX_ConstraintWrapper::_setattr(const STR_String& attr,PyObject* pyobj)
{
	
	PyTypeObject* type = pyobj->ob_type;
	int result = 1;

	if (type == &PyList_Type)
	{
		result = 0;
	}
	if (type == &PyFloat_Type)
	{
		result = 0;

	}
	if (type == &PyInt_Type)
	{
		result = 0;
	}
	if (type == &PyString_Type)
	{
		result = 0;
	}
	if (result)
		result = PyObjectPlus::_setattr(attr,pyobj);
	return result;
};


PyMethodDef KX_ConstraintWrapper::Methods[] = {
	{"testMethod",(PyCFunction) KX_ConstraintWrapper::sPyTestMethod, METH_VARARGS},
	{"getConstraintId",(PyCFunction) KX_ConstraintWrapper::sPyGetConstraintId, METH_VARARGS},
	{NULL,NULL} //Sentinel
};
