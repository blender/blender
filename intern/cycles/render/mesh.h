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

#ifndef __MESH_H__
#define __MESH_H__

#include "attribute.h"
#include "node.h"
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
class DiagSplit;

/* Mesh */

class Mesh : public Node {
public:
	NODE_DECLARE;

	/* Mesh Triangle */
	struct Triangle {
		int v[3];

		void bounds_grow(const float3 *verts, BoundBox& bounds) const;
	};

	Triangle get_triangle(size_t i) const
	{
		Triangle tri = {{triangles[i*3 + 0], triangles[i*3 + 1], triangles[i*3 + 2]}};
		return tri;
	}

	size_t num_triangles() const
	{
		return triangles.size() / 3;
	}

	/* Mesh Curve */
	struct Curve {
		int first_key;
		int num_keys;

		int num_segments() { return num_keys - 1; }

		void bounds_grow(const int k, const float3 *curve_keys, const float *curve_radius, BoundBox& bounds) const;
	};

	Curve get_curve(size_t i) const
	{
		int first = curve_first_key[i];
		int next_first = (i+1 < curve_first_key.size()) ? curve_first_key[i+1] : curve_keys.size();

		Curve curve = {first, next_first - first};
		return curve;
	}

	size_t num_curves() const
	{
		return curve_first_key.size();
	}

	/* Displacement */
	enum DisplacementMethod {
		DISPLACE_BUMP = 0,
		DISPLACE_TRUE = 1,
		DISPLACE_BOTH = 2,

		DISPLACE_NUM_METHODS,
	};

	/* Mesh Data */
	enum GeometryFlags {
		GEOMETRY_NONE      = 0,
		GEOMETRY_TRIANGLES = (1 << 0),
		GEOMETRY_CURVES    = (1 << 1),
	};
	int geometry_flags;  /* used to distinguish meshes with no verts
	                        and meshed for which geometry is not created */

	array<int> triangles;
	array<float3> verts;
	array<int> shader;
	array<bool> smooth;
	array<bool> forms_quad; /* used to tell if triangle is part of a quad patch */

	bool has_volume;  /* Set in the device_update_flags(). */
	bool has_surface_bssrdf;  /* Set in the device_update_flags(). */

	array<float3> curve_keys;
	array<float> curve_radius;
	array<int> curve_first_key;
	array<int> curve_shader;

	vector<Shader*> used_shaders;
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

	void resize_mesh(int numverts, int numfaces);
	void reserve_mesh(int numverts, int numfaces);
	void resize_curves(int numcurves, int numkeys);
	void reserve_curves(int numcurves, int numkeys);
	void clear();
	void add_vertex(float3 P);
	void add_triangle(int v0, int v1, int v2, int shader, bool smooth, bool forms_quad = false);
	void add_curve_key(float3 loc, float radius);
	void add_curve(int first_key, int num_keys, int shader);
	int split_vertex(int vertex);

	void compute_bounds();
	void add_face_normals();
	void add_vertex_normals();

	void pack_normals(Scene *scene, uint *shader, float4 *vnormal);
	void pack_verts(float4 *tri_verts, float4 *tri_vindex, size_t vert_offset);
	void pack_curves(Scene *scene, float4 *curve_key_co, float4 *curve_data, size_t curvekey_offset);
	void compute_bvh(SceneParams *params, Progress *progress, int n, int total);

	bool need_attribute(Scene *scene, AttributeStandard std);
	bool need_attribute(Scene *scene, ustring name);

	void tag_update(Scene *scene, bool rebuild);

	bool has_motion_blur() const;

	/* Check whether the mesh should have own BVH built separately. Briefly,
	 * own BVH is needed for mesh, if:
	 *
	 * - It is instanced multiple times, so each instance object should share the
	 *   same BVH tree.
	 * - Special ray intersection is needed, for example to limit subsurface rays
	 *   to only the mesh itself.
	 */
	bool need_build_bvh() const;

	/* Check if the mesh should be treated as instanced. */
	bool is_instanced() const;

	void tessellate(DiagSplit *split);
};

/* Mesh Manager */

class MeshManager {
public:
	BVH *bvh;

	bool need_update;
	bool need_flags_update;

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
	void device_update_flags(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_displacement_images(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __MESH_H__ */

