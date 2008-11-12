/**
 * Always trigger
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "SCA_AlwaysSensor.h"
#include "SCA_LogicManager.h"
#include "SCA_EventManager.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_AlwaysSensor::SCA_AlwaysSensor(class SCA_EventManager* eventmgr,
								 SCA_IObject* gameobj,
								 PyTypeObject* T)
	: SCA_ISensor(gameobj,eventmgr, T)
{
	//SetDrawColor(255,0,0);
	Init();
}

void SCA_AlwaysSensor::Init()
{
	m_alwaysresult = true;
}

SCA_AlwaysSensor::~SCA_AlwaysSensor()
{
	/* intentionally empty */
}



CValue* SCA_AlwaysSensor::GetReplica()
{
	CValue* replica = new SCA_AlwaysSensor(*this);//m_float,GetName());
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



bool SCA_AlwaysSensor::IsPositiveTrigger()
{ 
	return (m_invert ? false : true);
}



bool SCA_AlwaysSensor::Evaluate(CValue* event)
{
	/* Nice! :) */
		//return true;
	/* even nicer ;) */
		//return false;
	
	/* nicest ! */
	bool result = m_alwaysresult;
	m_alwaysresult = false;
	return result;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_AlwaysSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_AlwaysSensor",
	sizeof(SCA_AlwaysSensor),
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

PyParentObject SCA_AlwaysSensor::Parents[] = {
	&SCA_AlwaysSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef SCA_AlwaysSensor::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyObject* SCA_AlwaysSensor::_getattr(const STR_String& attr) {
	_getattr_up(SCA_ISensor);
}

/* eof */
