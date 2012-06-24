/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#ifndef _COM_Device_h
#define _COM_Device_h

#include "COM_WorkPackage.h"

/**
 * @brief Abstract class for device implementations to be used by the Compositor.
 * devices are queried, initialized and used by the WorkScheduler.
 * work are packaged as a WorkPackage instance.
 */
class Device {
public:
	/**
	 * @brief initialize the device
	 */
	virtual bool initialize() { return true; }

	/**
	 * @brief deinitialize the device
	 */
	virtual void deinitialize() {}

	/**
	 * @brief execute a WorkPackage
	 * @param work the WorkPackage to execute
	 */
	virtual void execute(WorkPackage *work) = 0;

};

#endif
