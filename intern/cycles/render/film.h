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

#ifndef __FILM_H__
#define __FILM_H__

#include "util/util_string.h"
#include "util/util_vector.h"

#include "kernel/kernel_types.h"

#include "graph/node.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

typedef enum FilterType {
	FILTER_BOX,
	FILTER_GAUSSIAN,
	FILTER_BLACKMAN_HARRIS,

	FILTER_NUM_TYPES,
} FilterType;

class Pass {
public:
	PassType type;
	int components;
	bool filter;
	bool exposure;
	PassType divide_type;

	static void add(PassType type, array<Pass>& passes);
	static bool equals(const array<Pass>& A, const array<Pass>& B);
	static bool contains(const array<Pass>& passes, PassType);
};

class Film : public Node {
public:
	NODE_DECLARE

	float exposure;
	array<Pass> passes;
	bool denoising_data_pass;
	bool denoising_clean_pass;
	int denoising_flags;
	float pass_alpha_threshold;

	int pass_stride;
	int denoising_data_offset;
	int denoising_clean_offset;

	FilterType filter_type;
	float filter_width;
	size_t filter_table_offset;

	float mist_start;
	float mist_depth;
	float mist_falloff;

	bool use_light_visibility;
	bool use_sample_clamp;

	bool need_update;

	Film();
	~Film();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene);
	void device_free(Device *device, DeviceScene *dscene, Scene *scene);

	bool modified(const Film& film);
	void tag_passes_update(Scene *scene, const array<Pass>& passes_);
	void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __FILM_H__ */

