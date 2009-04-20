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
 */

#include "GPC_System.h"

#include "GPC_KeyboardDevice.h"
#include "NG_NetworkDeviceInterface.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GPC_System::GPC_System()
//		: m_ndi(0)
{
}

/*
void GPC_System::NextFrame()
{
	// Have the imput devices proceed
	std::vector<SCA_IInputDevice*>::iterator idev;
	for (idev = m_inputDevices.begin(); !(idev == m_inputDevices.end()); idev++) {
		(*idev)->NextFrame();
	}

	// Have the network device proceed
	if (m_ndi) {
		m_ndi->NextFrame();
	}
}

void GPC_System::StartMainLoop()
{
}


void GPC_System::Sleep(int millisec)
{
	// do nothing for now ;)
}


void GPC_System::AddKey(unsigned char key, bool down)
{
	GPC_KeyboardDevice* keydev = (GPC_KeyboardDevice*) this->GetKeyboardDevice();
	if (keydev) {
		//SCA_IInputDevice::KX_EnumInputs inp = keydev->ToNative(key);
		keydev->ConvertEvent(key, down);
	}
}


void GPC_System::SetNetworkDevice(NG_NetworkDeviceInterface* ndi)
{
	m_ndi = ndi;
}


NG_NetworkDeviceInterface* GPC_System::GetNetworkDevice() const
{
	return m_ndi;
}
*/
