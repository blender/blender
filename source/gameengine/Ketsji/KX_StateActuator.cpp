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
 * Actuator to toggle visibility/invisibility of objects
 */

#include "KX_StateActuator.h"
#include "KX_GameObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_StateActuator::KX_StateActuator(
	SCA_IObject* gameobj,
	int operation,
	unsigned int mask,
	PyTypeObject* T
	) 
	: SCA_IActuator(gameobj,T),
	  m_operation(operation),
	  m_mask(mask)
{
	// intentionally empty
}

KX_StateActuator::~KX_StateActuator(
	void
	)
{
	// intentionally empty
}

CValue*
KX_StateActuator::GetReplica(
	void
	)
{
	KX_StateActuator* replica = new KX_StateActuator(*this);
	replica->ProcessReplica();
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	return replica;
}

bool
KX_StateActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	unsigned int objMask;
	
	RemoveAllEvents();
	if (bNegativeEvent) return false;

	KX_GameObject *obj = (KX_GameObject*) GetParent();
	
	objMask = obj->GetState();
	switch (m_operation) 
	{
	case OP_CPY:
		objMask = m_mask;
		break;
	case OP_SET:
		objMask |= m_mask;
		break;
	case OP_CLR:
		objMask &= ~m_mask;
		break;
	case OP_NEG:
		objMask ^= m_mask;
		break;
	default:
		// unsupported operation, no  nothing
		return false;
	}
	obj->SetState(objMask);
	return false;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject 
KX_StateActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_StateActuator",
	sizeof(KX_StateActuator),
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

PyParentObject 
KX_StateActuator::Parents[] = {
	&KX_StateActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef 
KX_StateActuator::Methods[] = {
	{"setOperation", (PyCFunction) KX_StateActuator::sPySetOperation, 
	 METH_VARARGS, SetOperation_doc},
	{"setMask", (PyCFunction) KX_StateActuator::sPySetMask, 
	 METH_VARARGS, SetMask_doc},
	{NULL,NULL} //Sentinel
};

PyObject* 
KX_StateActuator::_getattr(
	const STR_String& attr
	) 
{
	_getattr_up(SCA_IActuator);
};



/* set operation ---------------------------------------------------------- */
char 
KX_StateActuator::SetOperation_doc[] = 
"setOperation(op)\n"
"\t - op : bit operation (0=Copy, 1=Set, 2=Clear, 3=Negate)"
"\tSet the type of bit operation to be applied on object state mask.\n"
"\tUse setMask() to specify the bits that will be modified.\n";
PyObject* 

KX_StateActuator::PySetOperation(PyObject* self, 
				    PyObject* args, 
				    PyObject* kwds) {
	int oper;

	if(!PyArg_ParseTuple(args, "i", &oper)) {
		return NULL;
	}

	m_operation = oper;

	Py_Return;
}

/* set mask ---------------------------------------------------------- */
char 
KX_StateActuator::SetMask_doc[] = 
"setMask(mask)\n"
"\t - mask : bits that will be modified"
"\tSet the value that defines the bits that will be modified by the operation.\n"
"\tThe bits that are 1 in the value will be updated in the object state,\n"
"\tthe bits that are 0 are will be left unmodified expect for the Copy operation\n"
"\twhich copies the value to the object state.\n";
PyObject* 

KX_StateActuator::PySetMask(PyObject* self, 
				    PyObject* args, 
				    PyObject* kwds) {
	int mask;

	if(!PyArg_ParseTuple(args, "i", &mask)) {
		return NULL;
	}

	m_mask = mask;

	Py_Return;
}


