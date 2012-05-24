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

#ifndef _COM_CPUDevice_h
#define _COM_CPUDevice_h

#include "COM_Device.h"

/**
  * @brief class representing a CPU device. 
  * @note for every hardware thread in the system a CPUDevice instance will exist in the workscheduler
  */
class CPUDevice: public Device {
public:
	/**
	  * @brief execute a WorkPackage
	  * @param work the WorkPackage to execute
	  */
	void execute(WorkPackage *work);
};

#endif
