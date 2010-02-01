/**
 * Abstract class for sensor logic bricks
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

#include "SCA_ISensor.h"
#include "SCA_EventManager.h"
#include "SCA_LogicManager.h"
// needed for IsTriggered()
#include "SCA_PythonController.h"

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Native functions */
void	SCA_ISensor::ReParent(SCA_IObject* parent)
{
	SCA_ILogicBrick::ReParent(parent);
	// will be done when the sensor is activated
	//m_eventmgr->RegisterSensor(this);
	//this->SetActive(false);
}


SCA_ISensor::SCA_ISensor(SCA_IObject* gameobj,
						 class SCA_EventManager* eventmgr) :
	SCA_ILogicBrick(gameobj)
{
	m_links = 0;
	m_suspended = false;
	m_invert = false;
	m_level = false;
	m_tap = false;
	m_reset = false;
	m_pos_ticks = 0;
	m_neg_ticks = 0;
	m_pos_pulsemode = false;
	m_neg_pulsemode = false;
	m_pulse_frequency = 0;
	m_state = false;
	m_prev_state = false;
	
	m_eventmgr = eventmgr;
}


SCA_ISensor::~SCA_ISensor()  
{
	// intentionally empty
}

void SCA_ISensor::ProcessReplica()
{
	SCA_ILogicBrick::ProcessReplica();
	m_linkedcontrollers.clear();
}

bool SCA_ISensor::IsPositiveTrigger() { 
	bool result = false;
	
	if (m_eventval) {
		result = (m_eventval->GetNumber() != 0.0);
	}
	if (m_invert) {
		result = !result;
	}
	
	return result;
}

void SCA_ISensor::SetPulseMode(bool posmode, 
							   bool negmode,
							   int freq) {
	m_pos_pulsemode = posmode;
	m_neg_pulsemode = negmode;
	m_pulse_frequency = freq;
}

void SCA_ISensor::SetInvert(bool inv) {
	m_invert = inv;
}

void SCA_ISensor::SetLevel(bool lvl) {
	m_level = lvl;
}

void SCA_ISensor::SetTap(bool tap) {
	m_tap = tap;
}


double SCA_ISensor::GetNumber() {
	return GetState();
}

void SCA_ISensor::Suspend() {
	m_suspended = true;
}

bool SCA_ISensor::IsSuspended() {
	return m_suspended;
}

void SCA_ISensor::Resume() {
	m_suspended = false;
}

void SCA_ISensor::Init() {
	printf("Sensor %s has no init function, please report this bug to Blender.org\n", m_name.Ptr());
}

void SCA_ISensor::DecLink() {
	m_links--;
	if (m_links < 0) 
	{
		printf("Warning: sensor %s has negative m_links: %d\n", m_name.Ptr(), m_links);
		m_links = 0;
	}
	if (!m_links)
	{
		// sensor is detached from all controllers, remove it from manager
		UnregisterToManager();
	}
}

void SCA_ISensor::RegisterToManager()
{
	// sensor is just activated, initialize it
	Init();
	m_state = false;
	m_eventmgr->RegisterSensor(this);
}

void SCA_ISensor::Replace_EventManager(class SCA_LogicManager* logicmgr)
{
	if(m_links) { /* true if we're used currently */

		m_eventmgr->RemoveSensor(this);
		m_eventmgr= logicmgr->FindEventManager(m_eventmgr->GetType());
		m_eventmgr->RegisterSensor(this);
	}
	else {
		m_eventmgr= logicmgr->FindEventManager(m_eventmgr->GetType());
	}
}

void SCA_ISensor::LinkToController(SCA_IController* controller)
{
	m_linkedcontrollers.push_back(controller);
}

void SCA_ISensor::UnlinkController(SCA_IController* controller)
{
	std::vector<class SCA_IController*>::iterator contit;
	for (contit = m_linkedcontrollers.begin();!(contit==m_linkedcontrollers.end());++contit)
	{
		if ((*contit) == controller)
		{
			*contit = m_linkedcontrollers.back();
			m_linkedcontrollers.pop_back();
			return;
		}
	}
	printf("Missing link from sensor %s:%s to controller %s:%s\n", 
		m_gameobj->GetName().ReadPtr(), GetName().ReadPtr(), 
		controller->GetParent()->GetName().ReadPtr(), controller->GetName().ReadPtr());
}

void SCA_ISensor::UnlinkAllControllers()
{
	std::vector<class SCA_IController*>::iterator contit;
	for (contit = m_linkedcontrollers.begin();!(contit==m_linkedcontrollers.end());++contit)
	{
		(*contit)->UnlinkSensor(this);
	}
	m_linkedcontrollers.clear();
}

void SCA_ISensor::UnregisterToManager()
{
	m_eventmgr->RemoveSensor(this);
	m_links = 0;
}

void SCA_ISensor::ActivateControllers(class SCA_LogicManager* logicmgr)
{
    for(vector<SCA_IController*>::const_iterator c= m_linkedcontrollers.begin();
		c!=m_linkedcontrollers.end();++c)
	{
		SCA_IController* contr = *c;
		if (contr->IsActive())
			logicmgr->AddTriggeredController(contr, this);
	}
}

void SCA_ISensor::Activate(class SCA_LogicManager* logicmgr)
{
	
	// calculate if a __triggering__ is wanted
	// don't evaluate a sensor that is not connected to any controller
	if (m_links && !m_suspended) {
		bool result = this->Evaluate();
		// store the state for the rest of the logic system
		m_prev_state = m_state;
		m_state = this->IsPositiveTrigger();
		if (result) {
			// the sensor triggered this frame
			if (m_state || !m_tap) {
				ActivateControllers(logicmgr);	
				// reset these counters so that pulse are synchronized with transition
				m_pos_ticks = 0;
				m_neg_ticks = 0;
			} else
			{
				result = false;
			}
		} else
		{
			/* First, the pulsing behaviour, if pulse mode is
			 * active. It seems something goes wrong if pulse mode is
			 * not set :( */
			if (m_pos_pulsemode) {
				m_pos_ticks++;
				if (m_pos_ticks > m_pulse_frequency) {
					if ( m_state )
					{
						ActivateControllers(logicmgr);
						result = true;
					}
					m_pos_ticks = 0;
				} 
			}
			// negative pulse doesn't make sense in tap mode, skip
			if (m_neg_pulsemode && !m_tap)
			{
				m_neg_ticks++;
				if (m_neg_ticks > m_pulse_frequency) {
					if (!m_state )
					{
						ActivateControllers(logicmgr);
						result = true;
					}
					m_neg_ticks = 0;
				}
			}
		}
		if (m_tap)
		{
			// in tap mode: we send always a negative pulse immediately after a positive pulse
			if (!result)
			{
				// the sensor did not trigger on this frame
				if (m_prev_state)
				{
					// but it triggered on previous frame => send a negative pulse
					ActivateControllers(logicmgr);
					result = true;
				}
				// in any case, absence of trigger means sensor off
				m_state = false;
			}
		}
		if (!result && m_level)
		{
			// This level sensor is connected to at least one controller that was just made 
			// active but it did not generate an event yet, do it now to those controllers only 
			for(vector<SCA_IController*>::const_iterator c= m_linkedcontrollers.begin();
				c!=m_linkedcontrollers.end();++c)
			{
				SCA_IController* contr = *c;
				if (contr->IsJustActivated())
					logicmgr->AddTriggeredController(contr, this);
			}
		}
	} 
}

#ifndef DISABLE_PYTHON

/* ----------------------------------------------- */
/* Python Functions						           */
/* ----------------------------------------------- */

KX_PYMETHODDEF_DOC_NOARGS(SCA_ISensor, reset,
"reset()\n"
"\tReset sensor internal state, effect depends on the type of sensor and settings.\n"
"\tThe sensor is put in its initial state as if it was just activated.\n")
{
	Init();
	m_prev_state = false;
	Py_RETURN_NONE;
}

/* ----------------------------------------------- */
/* Python Integration Hooks					       */
/* ----------------------------------------------- */

PyTypeObject SCA_ISensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_ISensor",
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
	&SCA_ILogicBrick::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_ISensor::Methods[] = {
	KX_PYMETHODTABLE_NOARGS(SCA_ISensor, reset),
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_ISensor::Attributes[] = {
	KX_PYATTRIBUTE_BOOL_RW("usePosPulseMode",SCA_ISensor,m_pos_pulsemode),
	KX_PYATTRIBUTE_BOOL_RW("useNegPulseMode",SCA_ISensor,m_neg_pulsemode),
	KX_PYATTRIBUTE_INT_RW("frequency",0,100000,true,SCA_ISensor,m_pulse_frequency),
	KX_PYATTRIBUTE_BOOL_RW("invert",SCA_ISensor,m_invert),
	KX_PYATTRIBUTE_BOOL_RW_CHECK("level",SCA_ISensor,m_level,pyattr_check_level),
	KX_PYATTRIBUTE_BOOL_RW_CHECK("tap",SCA_ISensor,m_tap,pyattr_check_tap),
	KX_PYATTRIBUTE_RO_FUNCTION("triggered", SCA_ISensor, pyattr_get_triggered),
	KX_PYATTRIBUTE_RO_FUNCTION("positive", SCA_ISensor, pyattr_get_positive),
	KX_PYATTRIBUTE_RO_FUNCTION("status", SCA_ISensor, pyattr_get_status),
	//KX_PYATTRIBUTE_TODO("links"),
	//KX_PYATTRIBUTE_TODO("posTicks"),
	//KX_PYATTRIBUTE_TODO("negTicks"),
	{ NULL }	//Sentinel
};


PyObject* SCA_ISensor::pyattr_get_triggered(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_ISensor* self= static_cast<SCA_ISensor*>(self_v);
	int retval = 0;
	if (SCA_PythonController::m_sCurrentController)
		retval = SCA_PythonController::m_sCurrentController->IsTriggered(self);
	return PyLong_FromSsize_t(retval);
}

PyObject* SCA_ISensor::pyattr_get_positive(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_ISensor* self= static_cast<SCA_ISensor*>(self_v);
	return PyLong_FromSsize_t(self->GetState());
}

PyObject* SCA_ISensor::pyattr_get_status(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_ISensor* self= static_cast<SCA_ISensor*>(self_v);
	int status = 0;
	if (self->GetState()) 
	{
		if (self->GetState() == self->GetPrevState()) 
		{
			status = 2;
		}
		else 
		{
			status = 1;
		}
	}
	else if (self->GetState() != self->GetPrevState()) 
	{
		status = 3;
	}
	return PyLong_FromSsize_t(status);
}

int SCA_ISensor::pyattr_check_level(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_ISensor* self= static_cast<SCA_ISensor*>(self_v);
	if (self->m_level)
		self->m_tap = false;
	return 0;
}

int SCA_ISensor::pyattr_check_tap(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_ISensor* self= static_cast<SCA_ISensor*>(self_v);
	if (self->m_tap)
		self->m_level = false;
	return 0;
}
#endif // DISABLE_PYTHON

/* eof */
