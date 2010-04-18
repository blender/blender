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
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */

PyTypeObject KX_SCA_DynamicActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_SCA_DynamicActuator",
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
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_SCA_DynamicActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_SCA_DynamicActuator::Attributes[] = {
	KX_PYATTRIBUTE_SHORT_RW("mode",0,4,false,KX_SCA_DynamicActuator,m_dyn_operation),
	KX_PYATTRIBUTE_FLOAT_RW("mass",0.0,FLT_MAX,KX_SCA_DynamicActuator,m_setmass),
	{ NULL }	//Sentinel
};

#endif // DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_SCA_DynamicActuator::KX_SCA_DynamicActuator(SCA_IObject *gameobj,
													   short dyn_operation,
													   float setmass) :

	SCA_IActuator(gameobj, KX_ACT_DYNAMIC),
	m_dyn_operation(dyn_operation),
	m_setmass(setmass)
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
		case 4:
			controller->SetMass(m_setmass);
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
	return replica;
};


/* eof */
