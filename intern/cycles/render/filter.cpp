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

#include "camera.h"
#include "device.h"
#include "filter.h"
#include "scene.h"

#include "kernel_types.h"

#include "util_algorithm.h"
#include "util_debug.h"
#include "util_math.h"

CCL_NAMESPACE_BEGIN

Filter::Filter()
{
	filter_type = FILTER_BOX;
	filter_width = 1.0f;
	need_update = true;
}

Filter::~Filter()
{
}

static float filter_func_box(float v, float width)
{
	return (float)1;
}

static float filter_func_gaussian(float v, float width)
{
	v *= (float)2/width;
	return (float)expf((float)-2*v*v);
}

static vector<float> filter_table(FilterType type, float width)
{
	const int filter_table_size = FILTER_TABLE_SIZE-1;
	vector<float> filter_table_cdf(filter_table_size+1);
	vector<float> filter_table(filter_table_size+1);
	float (*filter_func)(float, float) = NULL;
	int i, half_size = filter_table_size/2;

	switch(type) {
		case FILTER_BOX:
			filter_func = filter_func_box;
			break;
		case FILTER_GAUSSIAN:
			filter_func = filter_func_gaussian;
			break;
		default:
			assert(0);
	}

	/* compute cumulative distribution function */
	filter_table_cdf[0] = 0.0f;
	
	for(i=0; i<filter_table_size; i++) {
		float x = i*width*0.5f/(filter_table_size-1);
		float y = filter_func(x, width);
		filter_table_cdf[i+1] += filter_table_cdf[i] + fabsf(y);
	}

	for(i=0; i<=filter_table_size; i++)
		filter_table_cdf[i] /= filter_table_cdf[filter_table_size];
	
	/* create importance sampling table */
	for(i=0; i<=half_size; i++) {
		float x = i/(float)half_size;
		int index = upper_bound(filter_table_cdf.begin(), filter_table_cdf.end(), x) - filter_table_cdf.begin();
		float t;

		if(index < filter_table_size+1) {
			t = (x - filter_table_cdf[index])/(filter_table_cdf[index+1] - filter_table_cdf[index]);
		}
		else {
			t = 0.0f;
			index = filter_table_size;
		}

		float y = ((index + t)/(filter_table_size))*width;

		filter_table[half_size+i] = 0.5f*(1.0f + y);
		filter_table[half_size-i] = 0.5f*(1.0f - y);
	}

	return filter_table;
}

void Filter::device_update(Device *device, DeviceScene *dscene)
{
	if(!need_update)
		return;

	device_free(device, dscene);

	/* update __filter_table */
	vector<float> table = filter_table(filter_type, filter_width);

	dscene->filter_table.copy(&table[0], table.size());
	device->tex_alloc("__filter_table", dscene->filter_table, true);

	need_update = false;
}

void Filter::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->filter_table);
	dscene->filter_table.clear();
}

bool Filter::modified(const Filter& filter)
{
	return !(filter_type == filter.filter_type &&
		filter_width == filter.filter_width);
}

void Filter::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END

