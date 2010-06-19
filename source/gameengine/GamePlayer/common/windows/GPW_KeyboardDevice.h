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

#ifndef _GPW_KEYBOARDDEVICE_H_
#define _GPW_KEYBOARDDEVICE_H_

#pragma warning (disable : 4786)

#include <windows.h>
#include "GPC_KeyboardDevice.h"

/**
 * Win32 implementation of GPC_KeyboardDevice.
 * The contructor fills the keyboard code translation map.
 * Base class GPC_KeyboardDevice does the rest.
 * @see SCA_IInputDevice
 */
class GPW_KeyboardDevice : public GPC_KeyboardDevice
{
public:
	GPW_KeyboardDevice(void);

	virtual ~GPW_KeyboardDevice(void);

	void ConvertWinEvent(WPARAM wParam, bool isDown);

protected:

	void ConvertModifierKey(WPARAM wParam, bool isDown);

	void CheckForSeperateLeftRight(WPARAM wParam);

	/** Stores the capability of this system to distinguish left and right modifier keys. */
	bool m_separateLeftRight;
	/** Stores the initialization state of the member m_leftRightDistinguishable. */
	bool m_separateLeftRightInitialized;
};

#endif //_GPW_KEYBOARDDEVICE_H_

