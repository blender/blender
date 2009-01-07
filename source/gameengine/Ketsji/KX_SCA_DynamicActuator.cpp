//
// Adjust dynamics settins for this object
//
// $Id$
//
// ***** BEGIN GPL LICENSE BLOCK *****
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
// All rights reserved.
//
// The Original Code is: all of this file.
//
// Contributor(s): none yet.
//
// ***** END GPL LICENSE BLOCK *****

//
// Previously existed as:

// \source\gameengine\GameLogic\SCA_DynamicActuator.cpp

// Please look here for revision history.

#include "KX_SCA_DynamicActuator.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */

	PyTypeObject 

KX_SCA_DynamicActuator::

Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_SCA_DynamicActuator",
	sizeof(KX_SCA_DynamicActuator),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0, 
	__repr,
	0, 
	0,
	0,
	0,
	0
};

PyParentObject KX_SCA_DynamicActuator::Parents[] = {
	&KX_SCA_DynamicActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};


PyMethodDef KX_SCA_DynamicActuator::Methods[] = {
	KX_PYMETHODTABLE(KX_SCA_DynamicActuator, setOperation),
   	KX_PYMETHODTABLE(KX_SCA_DynamicActuator, getOperation),
	{NULL,NULL} //Sentinel
};



PyObject* KX_SCA_DynamicActuator::_getattr(const STR_String& attr)
{
  _getattr_up(SCA_IActuator);
}



/* 1. setOperation */
KX_PYMETHODDEF_DOC(KX_SCA_DynamicActuator, setOperation,
"setOperation(operation?)\n"
"\t - operation? : type of dynamic operation\n"
"\t                0 = restore dynamics\n"
"\t                1 = disable dynamics\n"
"\t                2 = enable rigid body\n"
"\t                3 = disable rigid body\n"
"Change the dynamic status of the parent object.\n")
{
	int dyn_operation;
	
	if (!PyArg_ParseTuple(args, "i", &dyn_operation))
	{
		return NULL;	
	}
	if (dyn_operation <0 || dyn_operation>3) {
		PyErr_SetString(PyExc_IndexError, "Dynamic Actuator's setOperation() range must be between 0 and 3");
		return NULL;
	}
	m_dyn_operation= dyn_operation;
	Py_Return;
}

KX_PYMETHODDEF_DOC(KX_SCA_DynamicActuator, getOperation,
"getOperation() -> integer\n"
"Returns the operation type of this actuator.\n"
)
{
	return PyInt_FromLong((long)m_dyn_operation);
}


/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_SCA_DynamicActuator::KX_SCA_DynamicActuator(SCA_IObject *gameobj,
													   short dyn_operation,
													   PyTypeObject* T) : 

	SCA_IActuator(gameobj, T),
	m_dyn_operation(dyn_operation)
{
} /* End of constructor */


KX_SCA_DynamicActuator::~KX_SCA_DynamicActuator()
{ 
	// there's nothing to be done here, really....
} /* end of destructor */



bool KX_SCA_DynamicActuator::Update()
{
	// bool result = false;	/*unused*/
	KX_GameObject *obj = (KX_GameObject*) GetParent();
	bool bNegativeEvent = IsNegativeEvent();
	KX_IPhysicsController* controller;
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events
	
	if (!obj)
		return false; // object not accessible, shouldnt happen
	controller = obj->GetPhysicsController();
	if (!controller)
		return false;	// no physic object

	switch (m_dyn_operation)
	{
		case 0:			
			obj->RestoreDynamics();	
			break;
		case 1:
			obj->SuspendDynamics();
			break;
		case 2:
			controller->setRigidBody(true);	
			break;
		case 3:
			controller->setRigidBody(false);
			break;
	}

	return false;
}



CValue* KX_SCA_DynamicActuator::GetReplica()
{
	KX_SCA_DynamicActuator* replica = 
		new KX_SCA_DynamicActuator(*this);

	if (replica == NULL)
		return NULL;

	replica->ProcessReplica();

	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
};


/* eof */
