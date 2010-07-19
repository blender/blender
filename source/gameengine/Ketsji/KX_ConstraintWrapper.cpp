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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "PyObjectPlus.h"
#include "KX_ConstraintWrapper.h"
#include "PHY_IPhysicsEnvironment.h"

KX_ConstraintWrapper::KX_ConstraintWrapper(
						PHY_ConstraintType ctype,
						int constraintId,
						PHY_IPhysicsEnvironment* physenv) :
		PyObjectPlus(),
		m_constraintId(constraintId),
		m_constraintType(ctype),
		m_physenv(physenv)
{
}
KX_ConstraintWrapper::~KX_ConstraintWrapper()
{
}

#ifndef DISABLE_PYTHON

PyObject* KX_ConstraintWrapper::PyGetConstraintId()
{
	return PyLong_FromSsize_t(m_constraintId);
}


PyObject* KX_ConstraintWrapper::PyGetParam(PyObject* args, PyObject* kwds)
{
	int dof;
	float value;
	
	if (!PyArg_ParseTuple(args,"i:getParam",&dof))
		return NULL;
	
	value = m_physenv->getConstraintParam(m_constraintId,dof);
	return PyFloat_FromDouble(value);
	
}

PyObject* KX_ConstraintWrapper::PySetParam(PyObject* args, PyObject* kwds)
{
	int dof;
	float minLimit,maxLimit;
	
	if (!PyArg_ParseTuple(args,"iff:setParam",&dof,&minLimit,&maxLimit))
		return NULL;
	
	m_physenv->setConstraintParam(m_constraintId,dof,minLimit,maxLimit);
	Py_RETURN_NONE;
}


//python specific stuff
PyTypeObject KX_ConstraintWrapper::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_ConstraintWrapper",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_ConstraintWrapper::Methods[] = {
	{"getConstraintId",(PyCFunction) KX_ConstraintWrapper::sPyGetConstraintId, METH_NOARGS},
	{"setParam",(PyCFunction) KX_ConstraintWrapper::sPySetParam, METH_VARARGS},
	{"getParam",(PyCFunction) KX_ConstraintWrapper::sPyGetParam, METH_VARARGS},
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_ConstraintWrapper::Attributes[] = {
	//KX_PYATTRIBUTE_TODO("constraintId"),
	{ NULL }	//Sentinel
};

#endif // DISABLE_PYTHON
