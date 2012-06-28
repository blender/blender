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
 */

#ifndef __DEVICE_TASK_H__
#define __DEVICE_TASK_H__

#include "device_memory.h"

#include "util_function.h"
#include "util_list.h"
#include "util_task.h"

CCL_NAMESPACE_BEGIN

/* Device Task */

class Device;
class RenderBuffers;
class RenderTile;
class Tile;

class DeviceTask : public Task {
public:
	typedef enum { PATH_TRACE, TONEMAP, SHADER } Type;
	Type type;

	int x, y, w, h;
	device_ptr rgba;
	device_ptr buffer;
	int sample;
	int num_samples;
	int resolution;
	int offset, stride;

	device_ptr shader_input;
	device_ptr shader_output;
	int shader_eval_type;
	int shader_x, shader_w;

	DeviceTask(Type type = PATH_TRACE);

	void split(list<DeviceTask>& tasks, int num);
	void split_max_size(list<DeviceTask>& tasks, int max_size);

	boost::function<bool(Device *device, RenderTile&)> acquire_tile;
	boost::function<void(RenderTile&)> release_tile;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_TASK_H__ */

