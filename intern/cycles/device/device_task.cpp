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

#include <stdlib.h>
#include <string.h>

#include "device_task.h"

#include "util_algorithm.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

/* Device Task */

DeviceTask::DeviceTask(Type type_)
: type(type_), x(0), y(0), w(0), h(0), rgba(0), buffer(0),
  sample(0), num_samples(1), resolution(0),
  shader_input(0), shader_output(0),
  shader_eval_type(0), shader_x(0), shader_w(0)
{
	last_update_time = time_dt();
}

void DeviceTask::split_max_size(list<DeviceTask>& tasks, int max_size)
{
	int num;

	if(type == SHADER) {
		num = (shader_w + max_size - 1)/max_size;
	}
	else {
		max_size = max(1, max_size/w);
		num = (h + max_size - 1)/max_size;
	}

	split(tasks, num);
}

void DeviceTask::split(list<DeviceTask>& tasks, int num)
{
	if(type == SHADER) {
		num = min(shader_w, num);

		for(int i = 0; i < num; i++) {
			int tx = shader_x + (shader_w/num)*i;
			int tw = (i == num-1)? shader_w - i*(shader_w/num): shader_w/num;

			DeviceTask task = *this;

			task.shader_x = tx;
			task.shader_w = tw;

			tasks.push_back(task);
		}
	}
	else if(type == PATH_TRACE) {
		for(int i = 0; i < num; i++)
			tasks.push_back(*this);
	}
	else {
		num = min(h, num);

		for(int i = 0; i < num; i++) {
			int ty = y + (h/num)*i;
			int th = (i == num-1)? h - i*(h/num): h/num;

			DeviceTask task = *this;

			task.y = ty;
			task.h = th;

			tasks.push_back(task);
		}
	}
}

void DeviceTask::update_progress(RenderTile &rtile)
{
	if (type != PATH_TRACE)
		return;

	if (update_tile_sample) {
		double current_time = time_dt();

		if (current_time - last_update_time >= 1.0f) {
			update_tile_sample(rtile);

			last_update_time = current_time;
		}
	}
}

CCL_NAMESPACE_END

