/**
 * 'Xor' together all inputs
 *
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

#include "SCA_XORController.h"
#include "SCA_ISensor.h"
#include "SCA_LogicManager.h"
#include "BoolValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_XORController::SCA_XORController(SCA_IObject* gameobj,
									 PyTypeObject* T)
	:
	SCA_IController(gameobj,T)
{
}



SCA_XORController::~SCA_XORController()
{
}



void SCA_XORController::Trigger(SCA_LogicManager* logicmgr)
{

	bool sensorresult = false;

	for (vector<SCA_ISensor*>::const_iterator is=m_linkedsensors.begin();
	!(is==m_linkedsensors.end());is++)
	{
		SCA_ISensor* sensor = *is;
		if (sensor->IsPositiveTrigger())
		{
			if (sensorresult == true)
			{
                sensorresult = false;
				break;
			}
            sensorresult = true;
		}
	}
	
	CValue* newevent = new CBoolValue(sensorresult);

	for (vector<SCA_IActuator*>::const_iterator i=m_linkedactuators.begin();
	!(i==m_linkedactuators.end());i++)
	{
		SCA_IActuator* actua = *i;//m_linkedactuators.at(i);
		logicmgr->AddActiveActuator(actua,newevent);
	}

	// every actuator that needs the event, has a it's own reference to it now so
	// release it (so to be clear: if there is no actuator, it's deleted right now)
	newevent->Release();

}



CValue* SCA_XORController::GetReplica()
{
	CValue* replica = new SCA_XORController(*this);
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_XORController::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_XORController",
	sizeof(SCA_XORController),
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

PyParentObject SCA_XORController::Parents[] = {
	&SCA_XORController::Type,
	&SCA_IController::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef SCA_XORController::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyObject* SCA_XORController::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IController);
}

/* eof */
