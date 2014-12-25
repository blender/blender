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

#ifndef __CURVES_H__
#define __CURVES_H__

#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

void curvebounds(float *lower, float *upper, float3 *p, int dim);

typedef enum curve_primitives {
	CURVE_TRIANGLES,
	CURVE_LINE_SEGMENTS,
	CURVE_SEGMENTS,
	CURVE_RIBBONS
} curve_primitives;

typedef enum curve_shape {
	CURVE_RIBBON,
	CURVE_THICK
} curve_shape;

typedef enum curve_triangles {
	CURVE_CAMERA_TRIANGLES,
	CURVE_TESSELATED_TRIANGLES
} curve_triangles;

typedef enum curve_lines {
	CURVE_ACCURATE,
	CURVE_CORRECTED,
	CURVE_UNCORRECTED
} curve_lines;

class ParticleCurveData {

public:

	ParticleCurveData();
	~ParticleCurveData();

	vector<int> psys_firstcurve;
	vector<int> psys_curvenum;
	vector<int> psys_shader;

	vector<float> psys_rootradius;
	vector<float> psys_tipradius;
	vector<float> psys_shape;
	vector<bool> psys_closetip;

	vector<int> curve_firstkey;
	vector<int> curve_keynum;
	vector<float> curve_length;
	vector<float3> curve_uv;
	vector<float3> curve_vcol;

	vector<float3> curvekey_co;
	vector<float> curvekey_time;
};

/* HairSystem Manager */

class CurveSystemManager {
public:

	int primitive;
	int curve_shape;
	int line_method;
	int triangle_method;
	int resolution;
	int subdivisions;

	float minimum_width;
	float maximum_width;

	bool use_curves;
	bool use_encasing;
	bool use_backfacing;
	bool use_tangent_normal_geometry;

	bool need_update;
	bool need_mesh_update;

	CurveSystemManager();
	~CurveSystemManager();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);
	bool modified(const CurveSystemManager& CurveSystemManager);
	bool modified_mesh(const CurveSystemManager& CurveSystemManager);

	void tag_update(Scene *scene);
	void tag_update_mesh();
};

CCL_NAMESPACE_END

#endif /* __CURVES_H__ */

