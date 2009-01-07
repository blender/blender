/*
 * SND_DeviceManager.h
 *
 * singleton for creating, switching and deleting audiodevices
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "SND_DeviceManager.h"
#include "SND_DependKludge.h"
#include "SND_DummyDevice.h"
#ifdef USE_FMOD
#include "SND_FmodDevice.h"
#endif
#ifdef USE_OPENAL
#include "SND_OpenALDevice.h"
#endif

SND_IAudioDevice* SND_DeviceManager::m_instance = NULL;
int SND_DeviceManager::m_subscriptions = 0;

#ifdef USE_OPENAL
int SND_DeviceManager::m_device_type = snd_e_openaldevice;
#else
#	ifdef USE_FMOD
int SND_DeviceManager::m_device_type = snd_e_fmoddevice;
#	else
int SND_DeviceManager::m_device_type = snd_e_dummydevice;
#	endif
#endif

void SND_DeviceManager::Subscribe()
{
	++m_subscriptions;
}



void SND_DeviceManager::Unsubscribe()
{
	--m_subscriptions;

	// only release memory if there is a m_instance but no subscriptions left
	if (m_subscriptions == 0 && m_instance)
	{
		delete m_instance;
		m_instance = NULL;
	}
	
	if (m_subscriptions < 0)
		m_subscriptions = 0;
}



SND_IAudioDevice* SND_DeviceManager::Instance()
{
	// only give away an instance if there are subscriptions
	if (m_subscriptions)
	{
		// if there's no instance yet, set and create a new one
		if (m_instance == NULL)
		{
			SetDeviceType(m_device_type);
		}

		return m_instance;
	}
	else
	{
		return NULL;
	}
}
		


void SND_DeviceManager::SetDeviceType(int device_type)
{
	// if we want to change devicetype, first delete the old one
	if (m_instance)
	{
		delete m_instance;
		m_instance = NULL;
	}
	
	// let's create the chosen device
	switch (device_type)
	{
#ifdef USE_FMOD
	case snd_e_fmoddevice:
		{
			m_instance = new SND_FmodDevice();
			m_device_type = device_type;
			break;
		}
#endif
#ifdef USE_OPENAL
	case snd_e_openaldevice:
		{
			m_instance = new SND_OpenALDevice();
			m_device_type = device_type;
			break;
		}
#endif
	default:
		{
			m_instance = new SND_DummyDevice();
			m_device_type = device_type;
			break;
		}
	}
}
