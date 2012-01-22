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

#ifndef __LIGHT_H__
#define __LIGHT_H__

#include "kernel_types.h"

#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

class Light {
public:
	Light();

	LightType type;
	float3 co;

	float3 dir;
	float size;

	float3 axisu;
	float sizeu;
	float3 axisv;
	float sizev;

	int map_resolution;

	bool cast_shadow;

	int shader;

	void tag_update(Scene *scene);
};

class LightManager {
public:
	bool need_update;

	LightManager();
	~LightManager();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);

protected:
	void device_update_points(Device *device, DeviceScene *dscene, Scene *scene);
	void device_update_distribution(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_background(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
};

CCL_NAMESPACE_END

#endif /* __LIGHT_H__ */

