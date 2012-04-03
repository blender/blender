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

/** \file GPC_MouseDevice.h
 *  \ingroup player
 */

#ifndef __GPC_MOUSEDEVICE_H__
#define __GPC_MOUSEDEVICE_H__

#ifdef WIN32
#pragma warning (disable : 4786)
#endif // WIN32

#include "SCA_IInputDevice.h"


/**
 * Generic Ketsji mouse device.
 * \see SCA_IInputDevice
 */
class GPC_MouseDevice : public SCA_IInputDevice
{
public:
	/**
	 * Button identifier.
	 */
	typedef enum {
		buttonLeft,
		buttonMiddle,
		buttonRight,
		buttonWheelUp,
		buttonWheelDown
	} TButtonId;

	GPC_MouseDevice();
	virtual ~GPC_MouseDevice(void);

	virtual bool IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode);
	virtual void NextFrame();

	/**
	 * Call this routine to update the mouse device when a button state changes.
	 * \param button	Which button state changes.
	 * \param isDown	The new state of the button.
	 * \param x			Position x-coordinate of the cursor at the time of the state change.
	 * \param y			Position y-coordinate of the cursor at the time of the state change.
	 * \return Indication as to whether the event was processed.
	 */
	virtual bool ConvertButtonEvent(TButtonId button, bool isDown);

	/**
	 * Call this routine to update the mouse device when a button state and
	 * cursor position changes at the same time (e.g. in Win32 messages).
	 * \param button	Which button state changes.
	 * \param isDown	The new state of the button.
	 * \param x			Position x-coordinate of the cursor at the time of the state change.
	 * \param y			Position y-coordinate of the cursor at the time of the state change.
	 * \return Indication as to whether the event was processed.
	 */
	virtual bool ConvertButtonEvent(TButtonId button, bool isDown, int x, int y);

	/**
	 * Call this routine to update the mouse device when the cursor has moved.
	 * \param x			Position x-coordinate of the cursor.
	 * \param y			Position y-coordinate of the cursor.
	 * \return Indication as to whether the event was processed.
	 */
	virtual bool ConvertMoveEvent(int x, int y);

protected:
	/**
	 * This routine converts a single mouse event to a Ketsji mouse event.
	 * \param kxevent	Ketsji event code.
	 * \param eventval	Value for this event.
	 * \return Indication as to whether the event was processed.
	 */
	virtual bool ConvertEvent(KX_EnumInputs kxevent, int eventval);
};

#endif  // __GPC_MOUSEDEVICE_H__

