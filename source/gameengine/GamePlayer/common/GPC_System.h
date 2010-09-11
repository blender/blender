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

#ifndef __GPC_SYSTEM_H
#define __GPC_SYSTEM_H

#if defined(WIN32)
#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#endif /* WIN32 */

#include "KX_ISystem.h"

//class NG_NetworkDeviceInterface;

class GPC_System : public KX_ISystem
{
public:
	GPC_System();

//	virtual void NextFrame();
//	virtual void StartMainLoop();
	virtual double GetTimeInSeconds() = 0;
//	virtual void Sleep(int millisec);
	//virtual bool IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode);
//	void AddKey(unsigned char key, bool down);

//	virtual void SetNetworkDevice(NG_NetworkDeviceInterface* ndi);
//	virtual NG_NetworkDeviceInterface* GetNetworkDevice() const;

//protected:
//	NG_NetworkDeviceInterface* m_ndi;
};

#endif  // __GPC_SYSTEM_H

