/*
 * Copyright 2011-2014 Blender Foundation
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
 * limitations under the License
 */

#ifndef __BAKE_H__
#define __BAKE_H__

#include "util_vector.h"
#include "device.h"
#include "scene.h"
#include "session.h"

CCL_NAMESPACE_BEGIN

class BakeData {
public:
	BakeData(const int object, const int tri_offset, const int num_pixels);
	~BakeData();

	void set(int i, int prim, float uv[2]);
	int object();
	int size();
	uint4 data(int i);
	bool is_valid(int i);

private:
	int m_object;
	int m_tri_offset;
	int m_num_pixels;
	vector<int>m_primitive;
	vector<float>m_u;
	vector<float>m_v;
};

class BakeManager {
public:
	BakeManager();
	~BakeManager();

	bool get_baking();
	void set_baking(const bool value);

	BakeData *init(const int object, const int tri_offset, const int num_pixels);

	bool bake(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress, ShaderEvalType shader_type, BakeData *bake_data, float result[]);

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	bool need_update;

private:
	BakeData *m_bake_data;
	bool m_is_baking;
};

CCL_NAMESPACE_END

#endif /* __BAKE_H__ */

