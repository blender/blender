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

#include "KX_PhysicsObjectWrapper.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IPhysicsController.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_PhysicsObjectWrapper::KX_PhysicsObjectWrapper(
						PHY_IPhysicsController* ctrl,
						PHY_IPhysicsEnvironment* physenv,PyTypeObject *T) :
					PyObjectPlus(T),
					m_ctrl(ctrl),
					m_physenv(physenv)
{
}

KX_PhysicsObjectWrapper::~KX_PhysicsObjectWrapper()
{
}


PyObject* KX_PhysicsObjectWrapper::PySetPosition(PyObject* args)
{
	float x,y,z;
	if (PyArg_ParseTuple(args,"fff:setPosition",&x,&y,&z))
	{
		m_ctrl->setPosition(x,y,z);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


PyObject* KX_PhysicsObjectWrapper::PySetLinearVelocity(PyObject* args)
{
	float x,y,z;
	int local;
	if (PyArg_ParseTuple(args,"fffi:setLinearVelocity",&x,&y,&z,&local))
	{
		m_ctrl->SetLinearVelocity(x,y,z,local != 0);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

PyObject* KX_PhysicsObjectWrapper::PySetAngularVelocity(PyObject* args)
{
	float x,y,z;
	int local;
	if (PyArg_ParseTuple(args,"fffi:setAngularVelocity",&x,&y,&z,&local))
	{
		m_ctrl->SetAngularVelocity(x,y,z,local != 0);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

PyObject*	KX_PhysicsObjectWrapper::PySetActive(PyObject* args)
{
	int active;
	if (PyArg_ParseTuple(args,"i:setActive",&active))
	{
		m_ctrl->SetActive(active!=0);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


PyAttributeDef KX_PhysicsObjectWrapper::Attributes[] = {
	{ NULL }	//Sentinel
};

//python specific stuff
PyTypeObject KX_PhysicsObjectWrapper::Type = {
	PyObject_HEAD_INIT(NULL)
		0,
		"KX_PhysicsObjectWrapper",
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

PyParentObject KX_PhysicsObjectWrapper::Parents[] = {
	&KX_PhysicsObjectWrapper::Type,
	NULL
};

PyObject*	KX_PhysicsObjectWrapper::py_getattro(PyObject *attr)
{
	py_getattro_up(PyObjectPlus);
}


int	KX_PhysicsObjectWrapper::py_setattro(PyObject *attr,PyObject *pyobj)
{
	int result = 1;

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


PyMethodDef KX_PhysicsObjectWrapper::Methods[] = {
	{"setPosition",(PyCFunction) KX_PhysicsObjectWrapper::sPySetPosition, METH_VARARGS},
	{"setLinearVelocity",(PyCFunction) KX_PhysicsObjectWrapper::sPySetLinearVelocity, METH_VARARGS},
	{"setAngularVelocity",(PyCFunction) KX_PhysicsObjectWrapper::sPySetAngularVelocity, METH_VARARGS},
	{"setActive",(PyCFunction) KX_PhysicsObjectWrapper::sPySetActive, METH_VARARGS},
	{NULL,NULL} //Sentinel
};
