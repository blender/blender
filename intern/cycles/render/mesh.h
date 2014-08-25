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
 * limitations under the License
 */

#ifndef __MESH_H__
#define __MESH_H__

#include "attribute.h"
#include "shader.h"

#include "util_boundbox.h"
#include "util_list.h"
#include "util_map.h"
#include "util_param.h"
#include "util_transform.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class BVH;
class Device;
class DeviceScene;
class Mesh;
class Progress;
class Scene;
class SceneParams;
class AttributeRequest;

/* Mesh */

class Mesh {
public:
	/* Mesh Triangle */
	struct Triangle {
		int v[3];

		void bounds_grow(const float3 *verts, BoundBox& bounds) const;
	};

	/* Mesh Curve */
	struct Curve {
		int first_key;
		int num_keys;
		uint shader;

		int num_segments() { return num_keys - 1; }

		void bounds_grow(const int k, const float4 *curve_keys, BoundBox& bounds) const;
	};

	/* Displacement */
	enum DisplacementMethod {
		DISPLACE_BUMP,
		DISPLACE_TRUE,
		DISPLACE_BOTH
	};

	ustring name;

	/* Mesh Data */
	bool geometry_synced;  /* used to distinguish meshes with no verts
	                          and meshed for which geometry is not created */

	vector<float3> verts;
	vector<Triangle> triangles;
	vector<uint> shader;
	vector<bool> smooth;

	vector<float4> curve_keys; /* co + radius */
	vector<Curve> curves;

	vector<uint> used_shaders;
	AttributeSet attributes;
	AttributeSet curve_attributes;

	BoundBox bounds;
	bool transform_applied;
	bool transform_negative_scaled;
	Transform transform_normal;
	DisplacementMethod displacement_method;

	uint motion_steps;
	bool use_motion_blur;

	/* Update Flags */
	bool need_update;
	bool need_update_rebuild;

	/* BVH */
	BVH *bvh;
	size_t tri_offset;
	size_t vert_offset;

	size_t curve_offset;
	size_t curvekey_offset;

	/* Functions */
	Mesh();
	~Mesh();

	void reserve(int numverts, int numfaces, int numcurves, int numcurvekeys);
	void clear();
	void set_triangle(int i, int v0, int v1, int v2, int shader, bool smooth);
	void add_triangle(int v0, int v1, int v2, int shader, bool smooth);
	void add_curve_key(float3 loc, float radius);
	void add_curve(int first_key, int num_keys, int shader);
	int split_vertex(int vertex);

	void compute_bounds();
	void add_face_normals();
	void add_vertex_normals();

	void pack_normals(Scene *scene, float *shader, float4 *vnormal);
	void pack_verts(float4 *tri_verts, float4 *tri_vindex, size_t vert_offset);
	void pack_curves(Scene *scene, float4 *curve_key_co, float4 *curve_data, size_t curvekey_offset);
	void compute_bvh(SceneParams *params, Progress *progress, int n, int total);

	bool need_attribute(Scene *scene, AttributeStandard std);
	bool need_attribute(Scene *scene, ustring name);

	void tag_update(Scene *scene, bool rebuild);

	bool has_motion_blur() const;
};

/* Mesh Manager */

class MeshManager {
public:
	BVH *bvh;

	bool need_update;

	MeshManager();
	~MeshManager();

	bool displace(Device *device, DeviceScene *dscene, Scene *scene, Mesh *mesh, Progress& progress);

	/* attributes */
	void update_osl_attributes(Device *device, Scene *scene, vector<AttributeRequestSet>& mesh_attributes);
	void update_svm_attributes(Device *device, DeviceScene *dscene, Scene *scene, vector<AttributeRequestSet>& mesh_attributes);

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_object(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_mesh(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_attributes(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_bvh(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __MESH_H__ */

