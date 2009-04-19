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
#include "PyObjectPlus.h"
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
	PyObject_HEAD_INIT(NULL)
		0,
		"KX_ConstraintWrapper",
		sizeof(PyObjectPlus_Proxy),
		0,
		py_base_dealloc,
		0,
		0,
		0,
		0,
		py_base_repr,
		0,0,0,0,0,0,
		py_base_getattro,
		py_base_setattro,
		0,0,0,0,0,0,0,0,0,
		Methods
};

PyParentObject KX_ConstraintWrapper::Parents[] = {
	&KX_ConstraintWrapper::Type,
	NULL
};

PyObject*	KX_ConstraintWrapper::py_getattro(PyObject *attr)
{
	//here you can search for existing data members (like mass,friction etc.)
	py_getattro_up(PyObjectPlus);
}

int	KX_ConstraintWrapper::py_setattro(PyObject *attr,PyObject* pyobj)
{
	int result = 1;
	/* what the heck is this supposed to do?, needs attention */
	if (PyList_Check(pyobj))
	{
		result = 0;
	}
	if (PyFloat_Check(pyobj))
	{
		result = 0;

	}
	if (PyInt_Check(pyobj))
	{
		result = 0;
	}
	if (PyString_Check(pyobj))
	{
		result = 0;
	}
	if (result)
		result = PyObjectPlus::py_setattro(attr,pyobj);
	return result;
};


PyMethodDef KX_ConstraintWrapper::Methods[] = {
	{"testMethod",(PyCFunction) KX_ConstraintWrapper::sPyTestMethod, METH_VARARGS},
	{"getConstraintId",(PyCFunction) KX_ConstraintWrapper::sPyGetConstraintId, METH_VARARGS},
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_ConstraintWrapper::Attributes[] = {
	{ NULL }	//Sentinel
};
