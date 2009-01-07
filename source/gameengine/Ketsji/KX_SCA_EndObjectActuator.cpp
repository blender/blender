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

//

// Remove the actuator's parent when triggered
//
// Previously existed as:
// \source\gameengine\GameLogic\SCA_EndObjectActuator.cpp
// Please look here for revision history.

#include "SCA_IActuator.h"
#include "KX_SCA_EndObjectActuator.h"
#include "SCA_IScene.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_SCA_EndObjectActuator::KX_SCA_EndObjectActuator(SCA_IObject *gameobj,
												   SCA_IScene* scene,
												   PyTypeObject* T): 
	SCA_IActuator(gameobj, T),
	m_scene(scene)
{
    // intentionally empty 
} /* End of constructor */



KX_SCA_EndObjectActuator::~KX_SCA_EndObjectActuator()
{ 
	// there's nothing to be done here, really....
} /* end of destructor */



bool KX_SCA_EndObjectActuator::Update()
{
	// bool result = false;	/*unused*/
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events
	m_scene->DelayedRemoveObject(GetParent());
	
	return false;
}



CValue* KX_SCA_EndObjectActuator::GetReplica()
{
	KX_SCA_EndObjectActuator* replica = 
		new KX_SCA_EndObjectActuator(*this);
	if (replica == NULL) return NULL;

	replica->ProcessReplica();
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	return replica;
};



/* ------------------------------------------------------------------------- */
/* Python functions : integration hooks                                      */
/* ------------------------------------------------------------------------- */

PyTypeObject KX_SCA_EndObjectActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_SCA_EndObjectActuator",
	sizeof(KX_SCA_EndObjectActuator),
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


PyParentObject KX_SCA_EndObjectActuator::Parents[] = {
	&KX_SCA_EndObjectActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};



PyMethodDef KX_SCA_EndObjectActuator::Methods[] = {
  {NULL,NULL} //Sentinel
};


PyObject* KX_SCA_EndObjectActuator::_getattr(const STR_String& attr)
{
  _getattr_up(SCA_IActuator);
}

/* eof */
