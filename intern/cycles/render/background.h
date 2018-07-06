/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BACKGROUND_H__
#define __BACKGROUND_H__

#include "graph/node.h"

#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;
class Shader;

class Background : public Node {
public:
	NODE_DECLARE

	float ao_factor;
	float ao_distance;

	bool use_shader;
	bool use_ao;

	uint visibility;
	Shader *shader;

	bool transparent;
	bool transparent_glass;
	float transparent_roughness_threshold;

	bool need_update;

	Background();
	~Background();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene);
	void device_free(Device *device, DeviceScene *dscene);

	bool modified(const Background& background);
	void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __BACKGROUND_H__ */
