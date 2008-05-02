/**
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
 * Sensor for keyboard input
 */
#include "SCA_KeyboardSensor.h"
#include "SCA_KeyboardManager.h"
#include "SCA_LogicManager.h"
#include "StringValue.h"
#include "SCA_IInputDevice.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
									   SCA_IObject* gameobj,
									   PyTypeObject* T )
	:SCA_ISensor(gameobj,keybdmgr,T),
	 m_pKeyboardMgr(keybdmgr),
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
	m_val=0;
}



SCA_KeyboardSensor::~SCA_KeyboardSensor()
{
}



CValue* SCA_KeyboardSensor::GetReplica()
{
	CValue* replica = new SCA_KeyboardSensor(*this);
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

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



bool SCA_KeyboardSensor::Evaluate(CValue* eventval)
{
	bool result    = false;
	SCA_IInputDevice* inputdev = m_pKeyboardMgr->GetInputDevice();
	
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



	/* Now see whether events must be bounced. */
	if (m_bAllKeys)
	{
		bool justactivated = false;
		bool justreleased = false;
		bool active = false;

		for (int i=SCA_IInputDevice::KX_BEGINKEY ; i< SCA_IInputDevice::KX_ENDKEY;i++)
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
						//see comment below
						//m_val = 1;
						//result = true;
						;
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
		}


	} else
	{

	//		cerr << "======= SCA_KeyboardSensor::Evaluate:: peeking at key status" << endl;
		const SCA_InputEvent & inevent = inputdev->GetEventValue(
			(SCA_IInputDevice::KX_EnumInputs) m_hotkey);
	
	//		cerr << "======= SCA_KeyboardSensor::Evaluate:: status: " << inevent.m_status << endl;

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
							//hmm, this abnormal situation may occur in the following cases:
							//- the key was pressed while the scene was suspended
							//- this is a new scene and the key is active from the start
							//In the second case, it's dangerous to activate the sensor
							//(think of a key to go to next scene)
							//What we really need is a edge/level flag in the key sensor
							//m_val = 1;
							//result = true;
							;
						}
					}
				}
			}
		}
	}

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
					newprop.SetLength(oldlength - 1);
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
 * Determine whether this character can be printed. We cannot use
 * the library functions here, because we need to test our own
 * keycodes. */
bool SCA_KeyboardSensor::IsPrintable(int keyIndex)
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
/*  			 || (keyIndex == KX_RETKEY)  */
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

// this code looks ugly, please use an ordinary hashtable

char SCA_KeyboardSensor::ToCharacter(int keyIndex, bool shifted)
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
	
/*  			 || (keyIndex == SCA_IInputDevice::KX_RETKEY)  */
	
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
	char semicolontorightbracket[] = ";\'` /\\=[]";
	char semicolontorightbracketshifted[] = ":\"~ \?|+{}";
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
 * Tests whether this is a delete key.
 */	
bool SCA_KeyboardSensor::IsDelete(int keyIndex)
{
	if ( (keyIndex == SCA_IInputDevice::KX_DELKEY)
		 || (keyIndex == SCA_IInputDevice::KX_BACKSPACEKEY) ) {
		return true;
	} else {
		return false;
	}
}

/**
 * Tests whether shift is pressed
 */	
bool SCA_KeyboardSensor::IsShifted(void)
{
	SCA_IInputDevice* inputdev = m_pKeyboardMgr->GetInputDevice();
	
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
	SCA_IInputDevice* inputdev = m_pKeyboardMgr->GetInputDevice();
	int num = inputdev->GetNumActiveEvents();

	/* weird loop, this one... */
	if (num > 0)
	{
		
		int index = 0;
		/* Check on all keys whether they were pushed. This does not
         * untangle the ordering, so don't type too fast :) */
		for (int i=SCA_IInputDevice::KX_BEGINKEY ; i< SCA_IInputDevice::KX_ENDKEY;i++)
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


/* ------------------------------------------------------------------------- */
/* Python functions : specific                                               */
/* ------------------------------------------------------------------------- */


PyObject* SCA_KeyboardSensor::PySetAllMode(PyObject* self, 
			       PyObject* args, 
			       PyObject* kwds)
{
	bool allkeys;

	if (!PyArg_ParseTuple(args, "i", &allkeys))
	{
	  return NULL;
	}
	
	m_bAllKeys = allkeys;
	Py_Return
}



PyObject* SCA_KeyboardSensor::sPySetAllMode(PyObject* self, 
				       PyObject* args, 
				       PyObject* kwds)
{
//	printf("sPyIsPositive\n");
    return ((SCA_KeyboardSensor*) self)->PyIsPositive(self, args, kwds);
}


/** 1. GetKey : check which key this sensor looks at */
char SCA_KeyboardSensor::GetKey_doc[] = 
"getKey()\n"
"\tReturn the code of the key this sensor is listening to.\n" ;
PyObject* SCA_KeyboardSensor::PyGetKey(PyObject* self, PyObject* args, PyObject* kwds)
{
	return PyInt_FromLong(m_hotkey);
}

/** 2. SetKey: change the key to look at */
char SCA_KeyboardSensor::SetKey_doc[] = 
"setKey(keycode)\n"
"\t- keycode: any code from GameKeys\n"
"\tSet the key this sensor should listen to.\n" ;
PyObject* SCA_KeyboardSensor::PySetKey(PyObject* self, PyObject* args, PyObject* kwds)
{
	int keyCode;
	
	if(!PyArg_ParseTuple(args, "i", &keyCode)) {
		return NULL;
	}

	/* Since we have symbolic constants for this in Python, we don't guard   */
	/* anything. It's up to the user to provide a sensible number.           */
	m_hotkey = keyCode;

	Py_Return;
}

/** 3. GetHold1 : set the first bucky bit */
char SCA_KeyboardSensor::GetHold1_doc[] = 
"getHold1()\n"
"\tReturn the code of the first key modifier to the key this \n"
"\tsensor is listening to.\n" ;
PyObject* SCA_KeyboardSensor::PyGetHold1(PyObject* self, PyObject* args, PyObject* kwds)
{
	return PyInt_FromLong(m_qual);
}

/** 4. SetHold1: change the first bucky bit */
char SCA_KeyboardSensor::SetHold1_doc[] = 
"setHold1(keycode)\n"
"\t- keycode: any code from GameKeys\n"
"\tSet the first modifier to the key this sensor should listen to.\n" ;
PyObject* SCA_KeyboardSensor::PySetHold1(PyObject* self, PyObject* args, PyObject* kwds)
{
	int keyCode;

	if(!PyArg_ParseTuple(args, "i", &keyCode)) {
		return NULL;
	}
	
	/* Since we have symbolic constants for this in Python, we don't guard   */
	/* anything. It's up to the user to provide a sensible number.           */
	m_qual = keyCode;

	Py_Return;
}
	
/** 5. GetHold2 : get the second bucky bit */
char SCA_KeyboardSensor::GetHold2_doc[] = 
"getHold2()\n"
"\tReturn the code of the second key modifier to the key this \n"
"\tsensor is listening to.\n" ;
PyObject* SCA_KeyboardSensor::PyGetHold2(PyObject* self, PyObject* args, PyObject* kwds)
{
	return PyInt_FromLong(m_qual2);
}

/** 6. SetHold2: change the second bucky bit */
char SCA_KeyboardSensor::SetHold2_doc[] = 
"setHold2(keycode)\n"
"\t- keycode: any code from GameKeys\n"
"\tSet the first modifier to the key this sensor should listen to.\n" ;
PyObject* SCA_KeyboardSensor::PySetHold2(PyObject* self, PyObject* args, PyObject* kwds)
{
	int keyCode;

	if(!PyArg_ParseTuple(args, "i", &keyCode)) {
		return NULL;
	}
	
	/* Since we have symbolic constants for this in Python, we don't guard   */
	/* anything. It's up to the user to provide a sensible number.           */
	m_qual2 = keyCode;

	Py_Return;
}

	
char SCA_KeyboardSensor::GetPressedKeys_doc[] = 
"getPressedKeys()\n"
"\tGet a list of pressed keys that have either been pressed, or just released this frame.\n" ;

PyObject* SCA_KeyboardSensor::PyGetPressedKeys(PyObject* self, PyObject* args, PyObject* kwds)
{
	SCA_IInputDevice* inputdev = m_pKeyboardMgr->GetInputDevice();

	int num = inputdev->GetNumJustEvents();
	PyObject* resultlist = PyList_New(num);

	if (num > 0)
	{
		
		int index = 0;
		
		for (int i=SCA_IInputDevice::KX_BEGINKEY ; i< SCA_IInputDevice::KX_ENDKEY;i++)
		{
			const SCA_InputEvent & inevent = inputdev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) i);
			if ((inevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED)
				|| (inevent.m_status == SCA_InputEvent::KX_JUSTRELEASED))
			{
				if (index < num)
				{
					PyObject* keypair = PyList_New(2);
					PyList_SetItem(keypair,0,PyInt_FromLong(i));
					PyList_SetItem(keypair,1,PyInt_FromLong(inevent.m_status));
					PyList_SetItem(resultlist,index,keypair);
					index++;
				}
			}
		}	
		if (index>0) return resultlist;
	}
	
	Py_Return;
}



char SCA_KeyboardSensor::GetCurrentlyPressedKeys_doc[] = 
"getCurrentlyPressedKeys()\n"
"\tGet a list of keys that are currently pressed.\n" ;

PyObject* SCA_KeyboardSensor::PyGetCurrentlyPressedKeys(PyObject* self, PyObject* args, PyObject* kwds)
{
SCA_IInputDevice* inputdev = m_pKeyboardMgr->GetInputDevice();

	int num = inputdev->GetNumActiveEvents();
	PyObject* resultlist = PyList_New(num);

	if (num > 0)
	{
		int index = 0;
		
		for (int i=SCA_IInputDevice::KX_BEGINKEY ; i< SCA_IInputDevice::KX_ENDKEY;i++)
		{
			const SCA_InputEvent & inevent = inputdev->GetEventValue((SCA_IInputDevice::KX_EnumInputs) i);
			if ( (inevent.m_status == SCA_InputEvent::KX_ACTIVE)
				 || (inevent.m_status == SCA_InputEvent::KX_JUSTACTIVATED))
			{
				if (index < num)
				{
					PyObject* keypair = PyList_New(2);
					PyList_SetItem(keypair,0,PyInt_FromLong(i));
					PyList_SetItem(keypair,1,PyInt_FromLong(inevent.m_status));
					PyList_SetItem(resultlist,index,keypair);
					index++;
				}
			}
		}

		/* why?*/
		if (index > 0) return resultlist;
	}

	Py_Return;
}

/* ------------------------------------------------------------------------- */
/* Python functions : integration hooks                                      */
/* ------------------------------------------------------------------------- */

PyTypeObject SCA_KeyboardSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_KeyboardSensor",
	sizeof(SCA_KeyboardSensor),
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

PyParentObject SCA_KeyboardSensor::Parents[] = {
	&SCA_KeyboardSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef SCA_KeyboardSensor::Methods[] = {
  {"getKey", (PyCFunction) SCA_KeyboardSensor::sPyGetKey, METH_VARARGS, GetKey_doc},
  {"setKey", (PyCFunction) SCA_KeyboardSensor::sPySetKey, METH_VARARGS, SetKey_doc},
  {"getHold1", (PyCFunction) SCA_KeyboardSensor::sPyGetHold1, METH_VARARGS, GetHold1_doc},
  {"setHold1", (PyCFunction) SCA_KeyboardSensor::sPySetHold1, METH_VARARGS, SetHold1_doc},
  {"getHold2", (PyCFunction) SCA_KeyboardSensor::sPyGetHold2, METH_VARARGS, GetHold2_doc},
  {"setHold2", (PyCFunction) SCA_KeyboardSensor::sPySetHold2, METH_VARARGS, SetHold2_doc},
//  {"getUseAllKeys", (PyCFunction) SCA_KeyboardSensor::sPyGetUseAllKeys, METH_VARARGS, GetUseAllKeys_doc},
//  {"setUseAllKeys", (PyCFunction) SCA_KeyboardSensor::sPySetUseAllKeys, METH_VARARGS, SetUseAllKeys_doc},
  {"getPressedKeys", (PyCFunction) SCA_KeyboardSensor::sPyGetPressedKeys, METH_VARARGS, GetPressedKeys_doc},
  {"getCurrentlyPressedKeys", (PyCFunction) SCA_KeyboardSensor::sPyGetCurrentlyPressedKeys, METH_VARARGS, GetCurrentlyPressedKeys_doc},
//  {"getKeyEvents", (PyCFunction) SCA_KeyboardSensor::sPyGetKeyEvents, METH_VARARGS, GetKeyEvents_doc},
  {NULL,NULL} //Sentinel
};

PyObject*
SCA_KeyboardSensor::_getattr(const STR_String& attr)
{
  _getattr_up(SCA_ISensor);
}

