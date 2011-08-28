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

#ifndef __FILTER_H__
#define __FILTER_H__

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

typedef enum FilterType {
	FILTER_BOX,
	FILTER_GAUSSIAN
} FilterType;

class Filter {
public:
	/* pixel filter */
	FilterType filter_type;
	float filter_width;
	bool need_update;

	Filter();
	~Filter();

	void device_update(Device *device, DeviceScene *dscene);
	void device_free(Device *device, DeviceScene *dscene);

	bool modified(const Filter& filter);
	void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __FILTER_H__ */

