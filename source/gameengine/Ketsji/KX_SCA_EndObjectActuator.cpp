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
												   SCA_IScene* scene):
	SCA_IActuator(gameobj, KX_ACT_END_OBJECT),
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
	return replica;
};

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions : integration hooks                                      */
/* ------------------------------------------------------------------------- */

PyTypeObject KX_SCA_EndObjectActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_SCA_EndObjectActuator",
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

PyMethodDef KX_SCA_EndObjectActuator::Methods[] = {
  {NULL,NULL} //Sentinel
};

PyAttributeDef KX_SCA_EndObjectActuator::Attributes[] = {
	{ NULL }	//Sentinel
};

#endif // DISABLE_PYTHON

/* eof */
