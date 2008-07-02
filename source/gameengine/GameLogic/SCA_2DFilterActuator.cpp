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
		short texture_flag,
		RAS_IRasterizer* rasterizer,
		RAS_IRenderTools* rendertools,
        PyTypeObject* T)
    : SCA_IActuator(gameobj, T),
     m_type(type),
	 m_flag(flag),
	 m_int_arg(int_arg),
	 m_texture_flag(texture_flag),
	 m_float_arg(float_arg),
	 m_rasterizer(rasterizer),
	 m_rendertools(rendertools)
{
}

void SCA_2DFilterActuator::SetShaderText(STR_String text)
{
	m_shaderText = text;
}



CValue* SCA_2DFilterActuator::GetReplica()
{
    SCA_2DFilterActuator* replica = new SCA_2DFilterActuator(*this);
    replica->ProcessReplica();
    CValue::AddDataToReplica(replica);

    return replica;
}


bool SCA_2DFilterActuator::Update()
{
	bool result = false;

	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();


	if (bNegativeEvent)
		return false; // do nothing on negative events

	if( m_type == RAS_2DFilterManager::RAS_2DFILTER_MOTIONBLUR )
	{
		if(!m_flag)
		{
			m_rasterizer->EnableMotionBlur(m_float_arg);
		}
		else
		{
			m_rasterizer->DisableMotionBlur();
		}
	}
	else if(m_type < RAS_2DFilterManager::RAS_2DFILTER_NUMBER_OF_FILTERS)
	{
		m_rendertools->Update2DFilter(m_type, m_int_arg, m_shaderText, m_texture_flag);
	}
    return true;
}


PyTypeObject SCA_2DFilterActuator::Type = {
        PyObject_HEAD_INIT(&PyType_Type)
        0,
        "SCA_2DFilterActuator",
        sizeof(SCA_2DFilterActuator),
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


PyParentObject SCA_2DFilterActuator::Parents[] = {
        &SCA_2DFilterActuator::Type,
        &SCA_IActuator::Type,
        &SCA_ILogicBrick::Type,
        &CValue::Type,
        NULL
};


PyMethodDef SCA_2DFilterActuator::Methods[] = {
    /* add python functions to deal with m_msg... */
    {NULL,NULL}
};


PyObject* SCA_2DFilterActuator::_getattr(const STR_String& attr) {
    _getattr_up(SCA_IActuator);
}
