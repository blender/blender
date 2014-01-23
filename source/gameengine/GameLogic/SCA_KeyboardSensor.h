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
 */

/** \file SCA_KeyboardSensor.h
 *  \ingroup gamelogic
 *  \brief Sensor for keyboard input
 */

#ifndef __SCA_KEYBOARDSENSOR_H__
#define __SCA_KEYBOARDSENSOR_H__

#include "SCA_ISensor.h"
#include "BoolValue.h"
#include <list>

/**
 * The keyboard sensor listens to the keyboard, and passes on events
 * on selected keystrokes. It has an alternate mode in which it logs
 * keypresses to a property. Note that these modes are not mutually
 * exclusive.  */
class SCA_KeyboardSensor : public SCA_ISensor
{
	Py_Header

	/**
	 * the key this sensor is sensing for
	 */
	int							m_hotkey;
	short int					m_qual,m_qual2;
	short int					m_val;
	/**
	 * If this toggle is true, all incoming key events generate a
	 * response.
	 */
	bool						m_bAllKeys;

	/**
	 * The name of the property to which logged text is appended. If
	 * this property is not defined, no logging takes place.
	 */
	STR_String	m_targetprop;
	/**
	 * The property that indicates whether or not to log text when in
	 * logging mode. If the property equals 0, no logging is done. For
	 * all other values, logging is active. Logging can only become
	 * active if there is a property to log to. Logging is independent
	 * from hotkey settings. */
	STR_String	m_toggleprop;

	/**
	 * Log the keystrokes from the current input buffer.
	 */
	void LogKeystrokes(void);
	
	/**
	 * Adds this key-code to the target prop.
	 */
	void AddToTargetProp(int keyIndex, int unicode);

	/**
	 * Tests whether shift is pressed.
	 */
	bool IsShifted(void);
	
public:
	SCA_KeyboardSensor(class SCA_KeyboardManager* keybdmgr,
					   short int hotkey,
					   short int qual,
					   short int qual2,
					   bool bAllKeys,
					   const STR_String& targetProp,
					   const STR_String& toggleProp,
					   SCA_IObject* gameobj,
					   short int exitKey);
	virtual ~SCA_KeyboardSensor();
	virtual CValue* GetReplica();
	virtual void Init();


	short int GetHotkey();
	virtual bool Evaluate();
	virtual bool IsPositiveTrigger();
	bool	TriggerOnAllKeys();

#ifdef WITH_PYTHON
	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	// KeyEvents: 
	KX_PYMETHOD_DOC_NOARGS(SCA_KeyboardSensor,getEventList);
	// KeyStatus: 
	KX_PYMETHOD_DOC_O(SCA_KeyboardSensor,getKeyStatus);
	
	static PyObject*	pyattr_get_events(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
#endif
};


/**
 * Transform keycodes to something printable.
 */
char ToCharacter(int keyIndex, bool shifted);

/**
 * Determine whether this character can be printed. We cannot use
 * the library functions here, because we need to test our own
 * keycodes. */
bool IsPrintable(int keyIndex);

/**
 * Tests whether this is a delete key.
 */
bool IsDelete(int keyIndex);


#endif  /* __SCA_KEYBOARDSENSOR_H__ */
