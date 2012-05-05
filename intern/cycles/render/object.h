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

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "util_boundbox.h"
#include "util_param.h"
#include "util_transform.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Mesh;
class Progress;
class Scene;
struct Transform;

/* Object */

class Object {
public:
	Mesh *mesh;
	Transform tfm;
	BoundBox bounds;
	ustring name;
	int pass_id;
	vector<ParamValue> attributes;
	uint visibility;
	MotionTransform motion;
	bool use_motion;
	bool use_holdout;

	Object();
	~Object();

	void tag_update(Scene *scene);

	void compute_bounds(bool motion_blur);
	void apply_transform();
};

/* Object Manager */

class ObjectManager {
public:
	bool need_update;

	ObjectManager();
	~ObjectManager();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_transforms(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);

	void apply_static_transforms(Scene *scene, Progress& progress);
};

CCL_NAMESPACE_END

#endif /* __OBJECT_H__ */

