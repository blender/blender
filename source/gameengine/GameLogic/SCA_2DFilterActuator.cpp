/**
 * SCA_2DFilterActuator.cpp
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "SCA_IActuator.h"
#include "SCA_2DFilterActuator.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <iostream>

SCA_2DFilterActuator::~SCA_2DFilterActuator()
{
}

SCA_2DFilterActuator::SCA_2DFilterActuator(
        SCA_IObject *gameobj, 
        RAS_2DFilterManager::RAS_2DFILTER_MODE type,
		short flag,
		float float_arg,
		int int_arg,
		RAS_IRasterizer* rasterizer,
		RAS_IRenderTools* rendertools)
    : SCA_IActuator(gameobj, KX_ACT_2DFILTER),
     m_type(type),
	 m_disableMotionBlur(flag),
	 m_float_arg(float_arg),
	 m_int_arg(int_arg),
	 m_rasterizer(rasterizer),
	 m_rendertools(rendertools)
{
	m_gameObj = NULL;
	if(gameobj){
		m_propNames = gameobj->GetPropertyNames();
		m_gameObj = gameobj;
	}
}


CValue* SCA_2DFilterActuator::GetReplica()
{
    SCA_2DFilterActuator* replica = new SCA_2DFilterActuator(*this);
    replica->ProcessReplica();
    return replica;
}


bool SCA_2DFilterActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();


	if (bNegativeEvent)
		return false; // do nothing on negative events

	if( m_type == RAS_2DFilterManager::RAS_2DFILTER_MOTIONBLUR )
	{
		if(!m_disableMotionBlur)
			m_rasterizer->EnableMotionBlur(m_float_arg);
		else
			m_rasterizer->DisableMotionBlur();

		return false;
	}
	else if(m_type < RAS_2DFilterManager::RAS_2DFILTER_NUMBER_OF_FILTERS)
	{
		m_rendertools->Update2DFilter(m_propNames, m_gameObj, m_type, m_int_arg, m_shaderText);
	}
	// once the filter is in place, no need to update it again => disable the actuator
    return false;
}


void SCA_2DFilterActuator::SetShaderText(const char *text)
{
	m_shaderText = text;
}

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_2DFilterActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_2DFilterActuator",
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

PyMethodDef SCA_2DFilterActuator::Methods[] = {
	/* add python functions to deal with m_msg... */
    {NULL,NULL}
};

PyAttributeDef SCA_2DFilterActuator::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW("shaderText", 0, 64000, false, SCA_2DFilterActuator, m_shaderText),
	KX_PYATTRIBUTE_SHORT_RW("disableMotionBlur", 0, 1, true, SCA_2DFilterActuator, m_disableMotionBlur),
	KX_PYATTRIBUTE_ENUM_RW("mode",RAS_2DFilterManager::RAS_2DFILTER_ENABLED,RAS_2DFilterManager::RAS_2DFILTER_NUMBER_OF_FILTERS,false,SCA_2DFilterActuator,m_type),
	KX_PYATTRIBUTE_INT_RW("passNumber", 0, 100, true, SCA_2DFilterActuator, m_int_arg),
	KX_PYATTRIBUTE_FLOAT_RW("value", 0.0, 100.0, SCA_2DFilterActuator, m_float_arg),
	{ NULL }	//Sentinel
};

#endif
