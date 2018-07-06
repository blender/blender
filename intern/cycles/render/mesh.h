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

#include "graph/node.h"

#include "render/attribute.h"
#include "render/shader.h"

#include "util/util_boundbox.h"
#include "util/util_list.h"
#include "util/util_map.h"
#include "util/util_param.h"
#include "util/util_transform.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Attribute;
class BVH;
class Device;
class DeviceScene;
class Mesh;
class Progress;
class Scene;
class SceneParams;
class AttributeRequest;
struct SubdParams;
class DiagSplit;
struct PackedPatchTable;

/* Mesh */

class Mesh : public Node {
public:
	NODE_DECLARE

	/* Mesh Triangle */
	struct Triangle {
		int v[3];

		void bounds_grow(const float3 *verts, BoundBox& bounds) const;

		void motion_verts(const float3 *verts,
		                  const float3 *vert_steps,
		                  size_t num_verts,
		                  size_t num_steps,
		                  float time,
		                  float3 r_verts[3]) const;

		void verts_for_step(const float3 *verts,
		                    const float3 *vert_steps,
		                    size_t num_verts,
		                    size_t num_steps,
		                    size_t step,
		                    float3 r_verts[3]) const;

		float3 compute_normal(const float3 *verts) const;

		bool valid(const float3 *verts) const;
	};

	Triangle get_triangle(size_t i) const
	{
		Triangle tri = {{triangles[i*3 + 0],
		                 triangles[i*3 + 1],
		                 triangles[i*3 + 2]}};
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

		void bounds_grow(const int k,
		                 const float3 *curve_keys,
		                 const float *curve_radius,
		                 BoundBox& bounds) const;
		void bounds_grow(float4 keys[4], BoundBox& bounds) const;
		void bounds_grow(const int k,
		                 const float3 *curve_keys,
		                 const float *curve_radius,
		                 const Transform& aligned_space,
		                 BoundBox& bounds) const;

		void motion_keys(const float3 *curve_keys,
		                 const float *curve_radius,
		                 const float3 *key_steps,
		                 size_t num_curve_keys,
		                 size_t num_steps,
		                 float time,
		                 size_t k0, size_t k1,
		                 float4 r_keys[2]) const;
		void cardinal_motion_keys(const float3 *curve_keys,
		                          const float *curve_radius,
		                          const float3 *key_steps,
		                          size_t num_curve_keys,
		                          size_t num_steps,
		                          float time,
		                          size_t k0, size_t k1,
		                          size_t k2, size_t k3,
		                          float4 r_keys[4]) const;

		void keys_for_step(const float3 *curve_keys,
		                   const float *curve_radius,
		                   const float3 *key_steps,
		                   size_t num_curve_keys,
		                   size_t num_steps,
		                   size_t step,
		                   size_t k0, size_t k1,
		                   float4 r_keys[2]) const;
		void cardinal_keys_for_step(const float3 *curve_keys,
		                            const float *curve_radius,
		                            const float3 *key_steps,
		                            size_t num_curve_keys,
		                            size_t num_steps,
		                            size_t step,
		                            size_t k0, size_t k1,
		                            size_t k2, size_t k3,
		                            float4 r_keys[4]) const;
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

	/* Mesh SubdFace */
	struct SubdFace {
		int start_corner;
		int num_corners;
		int shader;
		bool smooth;
		int ptex_offset;

		bool is_quad() { return num_corners == 4; }
		float3 normal(const Mesh *mesh) const;
		int num_ptex_faces() const { return num_corners == 4 ? 1 : num_corners; }
	};

	struct SubdEdgeCrease {
		int v[2];
		float crease;
	};

	enum SubdivisionType {
		SUBDIVISION_NONE,
		SUBDIVISION_LINEAR,
		SUBDIVISION_CATMULL_CLARK,
	};

	SubdivisionType subdivision_type;

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

	/* used for storing patch info for subd triangles, only allocated if there are patches */
	array<int> triangle_patch; /* must be < 0 for non subd triangles */
	array<float2> vert_patch_uv;

	float volume_isovalue;
	bool has_volume;          /* Set in the device_update_flags(). */
	bool has_surface_bssrdf;  /* Set in the device_update_flags(). */

	array<float3> curve_keys;
	array<float> curve_radius;
	array<int> curve_first_key;
	array<int> curve_shader;

	array<SubdFace> subd_faces;
	array<int> subd_face_corners;
	int num_ngons;

	array<SubdEdgeCrease> subd_creases;

	SubdParams *subd_params;

	vector<Shader*> used_shaders;
	AttributeSet attributes;
	AttributeSet curve_attributes;
	AttributeSet subd_attributes;

	BoundBox bounds;
	bool transform_applied;
	bool transform_negative_scaled;
	Transform transform_normal;

	PackedPatchTable *patch_table;

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

	size_t patch_offset;
	size_t patch_table_offset;
	size_t face_offset;
	size_t corner_offset;

	size_t attr_map_offset;

	size_t num_subd_verts;

	/* Functions */
	Mesh();
	~Mesh();

	void resize_mesh(int numverts, int numfaces);
	void reserve_mesh(int numverts, int numfaces);
	void resize_curves(int numcurves, int numkeys);
	void reserve_curves(int numcurves, int numkeys);
	void resize_subd_faces(int numfaces, int num_ngons, int numcorners);
	void reserve_subd_faces(int numfaces, int num_ngons, int numcorners);
	void clear(bool preserve_voxel_data = false);
	void add_vertex(float3 P);
	void add_vertex_slow(float3 P);
	void add_triangle(int v0, int v1, int v2, int shader, bool smooth);
	void add_curve_key(float3 loc, float radius);
	void add_curve(int first_key, int shader);
	void add_subd_face(int* corners, int num_corners, int shader_, bool smooth_);
	int split_vertex(int vertex);

	void compute_bounds();
	void add_face_normals();
	void add_vertex_normals();
	void add_undisplaced();

	void pack_shaders(Scene *scene, uint *shader);
	void pack_normals(float4 *vnormal);
	void pack_verts(const vector<uint>& tri_prim_index,
	                uint4 *tri_vindex,
	                uint *tri_patch,
	                float2 *tri_patch_uv,
	                size_t vert_offset,
	                size_t tri_offset);
	void pack_curves(Scene *scene, float4 *curve_key_co, float4 *curve_data, size_t curvekey_offset);
	void pack_patches(uint *patch_data, uint vert_offset, uint face_offset, uint corner_offset);

	void compute_bvh(Device *device,
	                 DeviceScene *dscene,
	                 SceneParams *params,
	                 Progress *progress,
	                 int n,
	                 int total);

	bool need_attribute(Scene *scene, AttributeStandard std);
	bool need_attribute(Scene *scene, ustring name);

	void tag_update(Scene *scene, bool rebuild);

	bool has_motion_blur() const;
	bool has_true_displacement() const;

	/* Convert between normalized -1..1 motion time and index
	 * in the VERTEX_MOTION attribute. */
	float motion_time(int step) const;
	int motion_step(float time) const;

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
	bool need_update;
	bool need_flags_update;

	MeshManager();
	~MeshManager();

	bool displace(Device *device, DeviceScene *dscene, Scene *scene, Mesh *mesh, Progress& progress);

	/* attributes */
	void update_osl_attributes(Device *device, Scene *scene, vector<AttributeRequestSet>& mesh_attributes);
	void update_svm_attributes(Device *device, DeviceScene *dscene, Scene *scene, vector<AttributeRequestSet>& mesh_attributes);

	void device_update_preprocess(Device *device, Scene *scene, Progress& progress);
	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);

	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);

	void create_volume_mesh(Scene *scene, Mesh *mesh, Progress &progress);

protected:
	/* Calculate verts/triangles/curves offsets in global arrays. */
	void mesh_calc_offset(Scene *scene);

	void device_update_object(Device *device,
	                          DeviceScene *dscene,
	                          Scene *scene,
	                          Progress& progress);

	void device_update_mesh(Device *device,
	                        DeviceScene *dscene,
	                        Scene *scene,
	                        bool for_displacement,
	                        Progress& progress);

	void device_update_attributes(Device *device,
	                              DeviceScene *dscene,
	                              Scene *scene,
	                              Progress& progress);

	void device_update_bvh(Device *device,
	                       DeviceScene *dscene,
	                       Scene *scene,
	                       Progress& progress);

	void device_update_displacement_images(Device *device,
	                                       Scene *scene,
	                                       Progress& progress);

	void device_update_volume_images(Device *device,
									 Scene *scene,
									 Progress& progress);
};

CCL_NAMESPACE_END

#endif /* __MESH_H__ */
