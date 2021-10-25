/*
 * Always trigger
 *
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

/** \file gameengine/GameLogic/SCA_AlwaysSensor.cpp
 *  \ingroup gamelogic
 */

#ifdef _MSC_VER
  /* This warning tells us about truncation of __long__ stl-generated names.
   * It can occasionally cause DevStudio to have internal compiler warnings. */
#  pragma warning( disable:4786 )
#endif

#include "SCA_AlwaysSensor.h"
#include "SCA_LogicManager.h"
#include "SCA_EventManager.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_AlwaysSensor::SCA_AlwaysSensor(class SCA_EventManager* eventmgr,
								 SCA_IObject* gameobj)
	: SCA_ISensor(gameobj,eventmgr)
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
	replica->ProcessReplica();

	return replica;
}



bool SCA_AlwaysSensor::IsPositiveTrigger()
{ 
	return (m_invert ? false : true);
}



bool SCA_AlwaysSensor::Evaluate()
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

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_AlwaysSensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_AlwaysSensor",
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
	&SCA_ISensor::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_AlwaysSensor::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_AlwaysSensor::Attributes[] = {
	{ NULL }	//Sentinel
};

#endif

/* eof */
