/*
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
 * Sensor for keyboard input
 */

/** \file gameengine/GameLogic/SCA_KeyboardSensor.cpp
 *  \ingroup gamelogic
 */


#include <stddef.h>

#include "SCA_KeyboardSensor.h"
#include "SCA_KeyboardManager.h"
#include "SCA_LogicManager.h"
#include "StringValue.h"
#include "SCA_IInputDevice.h"

extern "C" {
	#include "BLI_string_cursor_utf8.h"
}

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_KeyboardSensor::SCA_KeyboardSensor(SCA_KeyboardManager* keybdmgr,
									   short int hotkey,
									   short int qual,
									   short int qual2,
									   bool bAllKeys,
									   const STR_String& targetProp,
									   const STR_String& toggleProp,
									   SCA_IObject* gameobj)
	:SCA_ISensor(gameobj,keybdmgr),
	 m_hotkey(hotkey),
	 m_qual(qual),
	 m_qual2(qual2),
	 m_bAllKeys(bAllKeys),
	 m_targetprop(targetProp),
	 m_toggleprop(toggleProp)
{
	if (hotkey == SCA_IInputDevice::KX_ESCKEY)
		keybdmgr->GetInputDevice()->HookEscape();
//	SetDrawColor(0xff0000ff);
	Init();
}



SCA_KeyboardSensor::~SCA_KeyboardSensor()
{
}

void SCA_KeyboardSensor::Init()
{
	// this function is used when the sensor is disconnected from all controllers
	// by the state engine. It reinitializes the sensor as if it was just created.
	// However, if the target key is pressed when the sensor is reactivated, it
	// will not generated an event (see remark in Evaluate()).
	m_val = (m_invert)?1:0;
	m_reset = true;
}

CValue* SCA_KeyboardSensor::GetReplica()
{
	SCA_KeyboardSensor* replica = new SCA_KeyboardSensor(*this);
	// this will copy properties and so on...
	replica->ProcessReplica();
	replica->Init();
	return replica;
}



short int SCA_KeyboardSensor::GetHotkey()
{
	return m_hotkey;
}



bool SCA_KeyboardSensor::IsPositiveTrigger()
{ 
	bool result = (m_val != 0);

	if (m_invert)
		result = !result;
		
	return result;
}



bool SCA_KeyboardSensor::TriggerOnAllKeys()
{ 
	return m_bAllKeys;
}



bool SCA_KeyboardSensor::Evaluate()
{
	bool result    = false;
	bool reset     = m_reset && m_level;
	bool qual	   = true;
	bool qual_change = false;
	short int m_val_orig = m_val;
	
	SCA_IInputDevice* inputdev = ((SCA_KeyboardManager *)m_eventmgr)->GetInputDevice();
	//  	cerr << "SCA_KeyboardSensor::Eval event, sensing for "<< m_hotkey << " at device " << inputdev << "\n";

	/* See if we need to do logging: togPropState exists and is
	 * different from 0 */
	CValue* myparent = GetParent();
	CValue* togPropState = myparent->GetProperty(m_toggleprop);
	if (togPropState &&
		(((int)togPropState->GetNumber()) != 0) )
	{
		LogKeystrokes();
	}

	m_reset = false;

	/* Now see whether events must be bounced. */
	if (m_bAllKeys)
	{
		bool justactivated = false;
		bool justreleased = false;
		bool active = false;

		for (int i=SCA_IInputDevice::KX_BEGINKEY ; i<= SCA_IInputDevice::KX_ENDKEY;i++)
		{
			const SCA_InputEvent & inevent = inputdev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) i);
			switch (inevent.m_status) 
			{ 
			case SCA_InputEvent::KX_JUSTACTIVATED:
				justactivated = true;
				break;
			case SCA_InputEvent::KX_JUSTRELEASED:
				justreleased = true;
				break;
			case SCA_InputEvent::KX_ACTIVE:
				active = true;
				break;
			case SCA_InputEvent::KX_NO_INPUTSTATUS:
				/* do nothing */
				break;
			}
		}

		if (justactivated)
		{
			m_val=1;
			result = true;
		} else
		{
			if (justreleased)
			{
				m_val=(active)?1:0;
				result = true;
			} else
			{
				if (active)
				{
					if (m_val == 0)
					{
						m_val = 1;
						if (m_level) {
							result = true;
						}
					}
				} else
				{
					if (m_val == 1)
					{
						m_val = 0;
						result = true;
					}
				}
			}
			if (m_tap)
				// special case for tap mode: only generate event for new activation
				result = false;
		}


	} else
	{

	//		cerr << "======= SCA_KeyboardSensor::Evaluate:: peeking at key status" << endl;
		const SCA_InputEvent & inevent = inputdev->GetEventValue(
			(SCA_IInputDevice::KX_EnumInputs) m_hotkey);
	
	//		cerr << "======= SCA_KeyboardSensor::Evaluate:: status: " << inevent.m_status << endl;
		
		
		/* Check qualifier keys
		 * - see if the qualifiers we request are pressed - 'qual' true/false
		 * - see if the qualifiers we request changed their state - 'qual_change' true/false
		 */
		if (m_qual > 0) {
			const SCA_InputEvent & qualevent = inputdev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) m_qual);
			switch(qualevent.m_status) {
			case SCA_InputEvent::KX_NO_INPUTSTATUS:
				qual = false;
				break;
			case SCA_InputEvent::KX_JUSTRELEASED:
				qual_change = true;
				qual = false;
				break;
			case SCA_InputEvent::KX_JUSTACTIVATED:
				qual_change = true;
			case SCA_InputEvent::KX_ACTIVE:
				/* do nothing */
				break;
			}
		}
		if (m_qual2 > 0 && qual==true) {
			const SCA_InputEvent & qualevent = inputdev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) m_qual2);
			/* copy of above */
			switch(qualevent.m_status) {
			case SCA_InputEvent::KX_NO_INPUTSTATUS:
				qual = false;
				break;
			case SCA_InputEvent::KX_JUSTRELEASED:
				qual_change = true;
				qual = false;
				break;
			case SCA_InputEvent::KX_JUSTACTIVATED:
				qual_change = true;
			case SCA_InputEvent::KX_ACTIVE:
				/* do nothing */
				break;
			}
		}
		/* done reading qualifiers */
		
		if (inevent.m_status == SCA_InputEvent::KX_NO_INPUTSTATUS)
		{
			if (m_val == 1)
			{
				// this situation may occur after a scene suspend: the keyboard release 
				// event was not captured, produce now the event off
				m_val = 0;
				result = true;
			}
		} else
		{
			if (inevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED)
			{
				m_val=1;
				result = true;
			} else
			{
				if (inevent.m_status == SCA_InputEvent::KX_JUSTRELEASED)
				{
					m_val = 0;
					result = true;
				} else 
				{
					if (inevent.m_status == SCA_InputEvent::KX_ACTIVE)
					{
						if (m_val == 0)
						{
							m_val = 1;
							if (m_level) 
							{
								result = true;
							}
						}
					}
				}
			}
		}
		
		/* Modify the key state based on qual(s)
		 * Tested carefully. don't touch unless your really sure.
		 * note, this will only change the results if key modifiers are set.
		 *
		 * When all modifiers and keys are positive
		 *  - pulse true
		 * 
		 * When ANY of the modifiers or main key become inactive,
		 *  - pulse false
		 */
		if (qual==false) { /* one of the qualifiers are not pressed */
			if (m_val_orig && qual_change) { /* we were originally enabled, but a qualifier changed */
				result = true;
			} else {
				result = false;
			}
			m_val = 0; /* since one of the qualifiers is not on, set the state to false */
		} else {						/* we done have any qualifiers or they are all pressed */
			if (m_val && qual_change) {	/* the main key state is true and our qualifier just changed */
				result = true;
			}
		}
		/* done with key quals */
		
	}
	
	if (reset)
		// force an event
		result = true;
	return result;

}

void SCA_KeyboardSensor::AddToTargetProp(int keyIndex)
{
	if (IsPrintable(keyIndex)) {
		CValue* tprop = GetParent()->GetProperty(m_targetprop);
		
		if (tprop) {
			/* overwrite the old property */
			if (IsDelete(keyIndex)) {
				/* strip one char, if possible */
				STR_String newprop = tprop->GetText();
				int oldlength = newprop.Length();
				if (oldlength >= 1 ) {
					int newlength=oldlength;

					BLI_str_cursor_step_prev_utf8(newprop, newprop.Length(), &newlength);
					newprop.SetLength(newlength);

					CStringValue * newstringprop = new CStringValue(newprop, m_targetprop);
					GetParent()->SetProperty(m_targetprop, newstringprop);
					newstringprop->Release();
				}				
			} else {
				/* append */
				char pchar = ToCharacter(keyIndex, IsShifted());
				STR_String newprop = tprop->GetText() + pchar;
				CStringValue * newstringprop = new CStringValue(newprop, m_targetprop);			
				GetParent()->SetProperty(m_targetprop, newstringprop);
				newstringprop->Release();
			}
		} else {
			if (!IsDelete(keyIndex)) {
				/* Make a new property. Deletes can be ignored. */
				char pchar = ToCharacter(keyIndex, IsShifted());
				STR_String newprop = pchar;
				CStringValue * newstringprop = new CStringValue(newprop, m_targetprop);			
				GetParent()->SetProperty(m_targetprop, newstringprop);
				newstringprop->Release();
			}
		}
	}
	
}
	
/**
 * Tests whether shift is pressed
 */	
bool SCA_KeyboardSensor::IsShifted(void)
{
	SCA_IInputDevice* inputdev = ((SCA_KeyboardManager *)m_eventmgr)->GetInputDevice();
	
	if ( (inputdev->GetEventValue(SCA_IInputDevice::KX_RIGHTSHIFTKEY).m_status 
		  == SCA_InputEvent::KX_ACTIVE)
		 || (inputdev->GetEventValue(SCA_IInputDevice::KX_RIGHTSHIFTKEY).m_status 
			 == SCA_InputEvent::KX_JUSTACTIVATED)
		 || (inputdev->GetEventValue(SCA_IInputDevice::KX_LEFTSHIFTKEY).m_status 
			 == SCA_InputEvent::KX_ACTIVE)
		 || (inputdev->GetEventValue(SCA_IInputDevice::KX_LEFTSHIFTKEY).m_status 
			 == SCA_InputEvent::KX_JUSTACTIVATED)
		) {
		return true;
	} else {
		return false;
	}	
}

void SCA_KeyboardSensor::LogKeystrokes(void) 
{
	SCA_IInputDevice* inputdev = ((SCA_KeyboardManager *)m_eventmgr)->GetInputDevice();
	int num = inputdev->GetNumActiveEvents();

	/* weird loop, this one... */
	if (num > 0)
	{
		
		int index = 0;
		/* Check on all keys whether they were pushed. This does not
		 * untangle the ordering, so don't type too fast :) */
		for (int i=SCA_IInputDevice::KX_BEGINKEY ; i<= SCA_IInputDevice::KX_ENDKEY;i++)
		{
			const SCA_InputEvent & inevent = inputdev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) i);
			if (inevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED) //NO_INPUTSTATUS)
			{
				if (index < num)
				{
					AddToTargetProp(i);
					index++;
				}
			}
		}
	}
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python Functions						       */
/* ------------------------------------------------------------------------- */

KX_PYMETHODDEF_DOC_O(SCA_KeyboardSensor, getKeyStatus,
"getKeyStatus(keycode)\n"
"\tGet the given key's status (KX_NO_INPUTSTATUS, KX_JUSTACTIVATED, KX_ACTIVE or KX_JUSTRELEASED).\n")
{
	if (!PyLong_Check(value)) {
		PyErr_SetString(PyExc_ValueError, "sensor.getKeyStatus(int): Keyboard Sensor, expected an int");
		return NULL;
	}
	
	int keycode = PyLong_AsSsize_t(value);
	
	if ((keycode < SCA_IInputDevice::KX_BEGINKEY)
		|| (keycode > SCA_IInputDevice::KX_ENDKEY)) {
		PyErr_SetString(PyExc_AttributeError, "sensor.getKeyStatus(int): Keyboard Sensor, invalid keycode specified!");
		return NULL;
	}
	
	SCA_IInputDevice* inputdev = ((SCA_KeyboardManager *)m_eventmgr)->GetInputDevice();
	const SCA_InputEvent & inevent = inputdev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) keycode);
	return PyLong_FromSsize_t(inevent.m_status);
}

/* ------------------------------------------------------------------------- */
/* Python Integration Hooks					       */
/* ------------------------------------------------------------------------- */

PyTypeObject SCA_KeyboardSensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_KeyboardSensor",
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

PyMethodDef SCA_KeyboardSensor::Methods[] = {
	KX_PYMETHODTABLE_O(SCA_KeyboardSensor, getKeyStatus),
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_KeyboardSensor::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("events", SCA_KeyboardSensor, pyattr_get_events),
	KX_PYATTRIBUTE_BOOL_RW("useAllKeys",SCA_KeyboardSensor,m_bAllKeys),
	KX_PYATTRIBUTE_INT_RW("key",0,SCA_IInputDevice::KX_ENDKEY,true,SCA_KeyboardSensor,m_hotkey),
	KX_PYATTRIBUTE_SHORT_RW("hold1",0,SCA_IInputDevice::KX_ENDKEY,true,SCA_KeyboardSensor,m_qual),
	KX_PYATTRIBUTE_SHORT_RW("hold2",0,SCA_IInputDevice::KX_ENDKEY,true,SCA_KeyboardSensor,m_qual2),
	KX_PYATTRIBUTE_STRING_RW("toggleProperty",0,MAX_PROP_NAME,false,SCA_KeyboardSensor,m_toggleprop),
	KX_PYATTRIBUTE_STRING_RW("targetProperty",0,MAX_PROP_NAME,false,SCA_KeyboardSensor,m_targetprop),
	{ NULL }	//Sentinel
};


PyObject* SCA_KeyboardSensor::pyattr_get_events(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_KeyboardSensor* self= static_cast<SCA_KeyboardSensor*>(self_v);
	
	SCA_IInputDevice* inputdev = ((SCA_KeyboardManager *)self->m_eventmgr)->GetInputDevice();

	PyObject* resultlist = PyList_New(0);
	
	for (int i=SCA_IInputDevice::KX_BEGINKEY ; i<= SCA_IInputDevice::KX_ENDKEY;i++)
	{
		const SCA_InputEvent & inevent = inputdev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) i);
		if (inevent.m_status != SCA_InputEvent::KX_NO_INPUTSTATUS)
		{
			PyObject* keypair = PyList_New(2);
			PyList_SET_ITEM(keypair,0,PyLong_FromSsize_t(i));
			PyList_SET_ITEM(keypair,1,PyLong_FromSsize_t(inevent.m_status));
			PyList_Append(resultlist,keypair);
		}
	}	
	return resultlist;
}

#endif // WITH_PYTHON

/* Accessed from python */

// this code looks ugly, please use an ordinary hashtable

char ToCharacter(int keyIndex, bool shifted)
{
	/* numerals */
	if ( (keyIndex >= SCA_IInputDevice::KX_ZEROKEY) 
		 && (keyIndex <= SCA_IInputDevice::KX_NINEKEY) ) {
		if (shifted) {
			char numshift[] = ")!@#$%^&*(";
			return numshift[keyIndex - '0']; 
		} else {
			return keyIndex - SCA_IInputDevice::KX_ZEROKEY + '0'; 
		}
	}
	
	/* letters... always lowercase... is that desirable? */
	if ( (keyIndex >= SCA_IInputDevice::KX_AKEY) 
		 && (keyIndex <= SCA_IInputDevice::KX_ZKEY) ) {
		if (shifted) {
			return keyIndex - SCA_IInputDevice::KX_AKEY + 'A'; 
		} else {
			return keyIndex - SCA_IInputDevice::KX_AKEY + 'a'; 
		}
	}
	
	if (keyIndex == SCA_IInputDevice::KX_SPACEKEY) {
		return ' ';
	}
	if (keyIndex == SCA_IInputDevice::KX_RETKEY || keyIndex == SCA_IInputDevice::KX_PADENTER) {
		return '\n';
	}
	
	
	if (keyIndex == SCA_IInputDevice::KX_PADASTERKEY) {
		return '*';
	}
	
	if (keyIndex == SCA_IInputDevice::KX_TABKEY) {
		return '\t';
	}
	
	/* comma to period */
	char commatoperiod[] = ",-.";
	char commatoperiodshifted[] = "<_>";
	if (keyIndex == SCA_IInputDevice::KX_COMMAKEY) {
		if (shifted) {
			return commatoperiodshifted[0];
		} else {
			return commatoperiod[0];
		}
	}
	if (keyIndex == SCA_IInputDevice::KX_MINUSKEY) {
		if (shifted) {
			return commatoperiodshifted[1];
		} else {
			return commatoperiod[1];
		}
	}
	if (keyIndex == SCA_IInputDevice::KX_PERIODKEY) {
		if (shifted) {
			return commatoperiodshifted[2];
		} else {
			return commatoperiod[2];
		}
	}
	
	/* semicolon to rightbracket */
	char semicolontorightbracket[] = ";\'`/\\=[]";
	char semicolontorightbracketshifted[] = ":\"~\?|+{}";
	if ((keyIndex >= SCA_IInputDevice::KX_SEMICOLONKEY) 
		&& (keyIndex <= SCA_IInputDevice::KX_RIGHTBRACKETKEY)) {
		if (shifted) {
			return semicolontorightbracketshifted[keyIndex - SCA_IInputDevice::KX_SEMICOLONKEY];
		} else {
			return semicolontorightbracket[keyIndex - SCA_IInputDevice::KX_SEMICOLONKEY];
		}
	}
	
	/* keypad2 to padplus */
	char pad2topadplus[] = "246813579. 0- +";
	if ((keyIndex >= SCA_IInputDevice::KX_PAD2) 
		&& (keyIndex <= SCA_IInputDevice::KX_PADPLUSKEY)) { 
		return pad2topadplus[keyIndex - SCA_IInputDevice::KX_PAD2];
	}

	return '!';
}



/**
 * Determine whether this character can be printed. We cannot use
 * the library functions here, because we need to test our own
 * keycodes. */
bool IsPrintable(int keyIndex)
{
	/* only print 
	 * - numerals: KX_ZEROKEY to KX_NINEKEY
	 * - alphas:   KX_AKEY to KX_ZKEY. 
	 * - specials: KX_RETKEY, KX_PADASTERKEY, KX_PADCOMMAKEY to KX_PERIODKEY,
	 *             KX_TABKEY , KX_SEMICOLONKEY to KX_RIGHTBRACKETKEY, 
	 *             KX_PAD2 to KX_PADPLUSKEY
	 * - delete and backspace: also printable in the sense that they modify 
	 *                         the string
	 * - retkey: should this be printable?
	 * - virgule: prints a space... don't know which key that's supposed
	 *   to be...
	 */
	if ( ((keyIndex >= SCA_IInputDevice::KX_ZEROKEY) 
		  && (keyIndex <= SCA_IInputDevice::KX_NINEKEY))
		 || ((keyIndex >= SCA_IInputDevice::KX_AKEY) 
			 && (keyIndex <= SCA_IInputDevice::KX_ZKEY)) 
		 || (keyIndex == SCA_IInputDevice::KX_SPACEKEY) 
		 || (keyIndex == SCA_IInputDevice::KX_RETKEY)
		 || (keyIndex == SCA_IInputDevice::KX_PADENTER)
		 || (keyIndex == SCA_IInputDevice::KX_PADASTERKEY) 
		 || (keyIndex == SCA_IInputDevice::KX_TABKEY) 
		 || ((keyIndex >= SCA_IInputDevice::KX_COMMAKEY) 
			 && (keyIndex <= SCA_IInputDevice::KX_PERIODKEY)) 
		 || ((keyIndex >= SCA_IInputDevice::KX_SEMICOLONKEY) 
			 && (keyIndex <= SCA_IInputDevice::KX_RIGHTBRACKETKEY)) 
		 || ((keyIndex >= SCA_IInputDevice::KX_PAD2) 
			 && (keyIndex <= SCA_IInputDevice::KX_PADPLUSKEY)) 
		 || (keyIndex == SCA_IInputDevice::KX_DELKEY)
		 || (keyIndex == SCA_IInputDevice::KX_BACKSPACEKEY)
		)
	{
		return true;
	} else {
		return false;
	}
}

/**
 * Tests whether this is a delete key.
 */	
bool IsDelete(int keyIndex)
{
	if ( (keyIndex == SCA_IInputDevice::KX_DELKEY)
		 || (keyIndex == SCA_IInputDevice::KX_BACKSPACEKEY) ) {
		return true;
	} else {
		return false;
	}
}
