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

#ifndef __SND_DEVICEMANAGER_H
#define __SND_DEVICEMANAGER_H

#include "SND_IAudioDevice.h"

class SND_DeviceManager
{
public :

	/**
	 * a subscription is needed before instances are given away
	 * applications must call subscribe first, get an instance, and
	 * when they are finished with sound, unsubscribe
	 */
	static void Subscribe();
	static void Unsubscribe();

	static SND_IAudioDevice* Instance();
	static void	SetDeviceType(int device_type);

private :

	/**
	 * Private to enforce singleton
	 */
	SND_DeviceManager();
	SND_DeviceManager(const SND_DeviceManager&);
	~SND_DeviceManager();
	
	static SND_IAudioDevice* m_instance;

	/**
	 * The type of device to be created on a call
	 * to Instance().
	 */
	static int m_device_type;

	/**
	 * Remember the number of subscriptions.
	 * if 0, delete the device
	 */
	static int m_subscriptions;
};

#endif //__SND_DEVICEMANAGER_H

