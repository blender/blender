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

#ifndef __CURVES_H__
#define __CURVES_H__

#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

typedef enum curve_presets {
	CURVE_CUSTOM,
	CURVE_TANGENT_SHADING,
	CURVE_TRUE_NORMAL,
	CURVE_ACCURATE_PRESET
} curve_presets;

typedef enum curve_primitives {
	CURVE_TRIANGLES,
	CURVE_LINE_SEGMENTS,
	CURVE_SEGMENTS
} curve_primitives;

typedef enum curve_triangles {
	CURVE_CAMERA,
	CURVE_RIBBONS,
	CURVE_TESSELATED
} curve_triangles;

typedef enum curve_lines {
	CURVE_ACCURATE,
	CURVE_CORRECTED,
	CURVE_POSTCORRECTED,
	CURVE_UNCORRECTED
} curve_lines;

typedef enum curve_interpolation {
	CURVE_LINEAR,
	CURVE_CARDINAL,
	CURVE_BSPLINE
} curve_interpolation;

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

	vector<float3> curvekey_co;
	vector<float> curvekey_time;
};

/* HairSystem Manager */

class CurveSystemManager {
public:

	int primitive;
	int line_method;
	int interpolation;
	int triangle_method;
	int resolution;
	int segments;

	float normalmix;
	float encasing_ratio;

	bool use_curves;
	bool use_smooth;
	bool use_cache;
	bool use_parents;
	bool use_encasing;
	bool use_backfacing;
	bool use_tangent_normal;
	bool use_tangent_normal_correction;
	bool use_tangent_normal_geometry;
	bool use_joined;

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

