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

class OpenCLDevice;

#ifndef _COM_OpenCLDevice_h
#define _COM_OpenCLDevice_h

#include "COM_Device.h"
#include "OCL_opencl.h"
#include "COM_WorkScheduler.h"


/**
 * @brief device representing an GPU OpenCL device.
 * an instance of this class represents a single cl_device
 */
class OpenCLDevice : public Device {
private:
	/**
	   *@brief opencl context
	 */
	cl_context context;
	
	/**
	   *@brief opencl device
	 */
	cl_device_id device;
	
	/**
	   *@brief opencl program
	 */
	cl_program program;
	
	/**
	   *@brief opencl command queue
	 */
	cl_command_queue queue;
public:
	/**
	   *@brief constructor with opencl device
	   *@param context
	   *@param device
	 */
	OpenCLDevice(cl_context context, cl_device_id device, cl_program program);
	
	
	/**
	 * @brief initialize the device
	 * During initialization the OpenCL cl_command_queue is created
	 * the command queue is stored in the field queue.
	 * @see queue
	 */
	bool initialize();
	
	/**
	 * @brief deinitialize the device
	 * During deintiialization the command queue is cleared
	 */
	void deinitialize();
	
	/**
	 * @brief execute a WorkPackage
	 * @param work the WorkPackage to execute
	 */
	void execute(WorkPackage *work);
};

#endif
