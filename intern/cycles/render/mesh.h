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
	};

	/* Displacement */
	enum DisplacementMethod {
		DISPLACE_BUMP,
		DISPLACE_TRUE,
		DISPLACE_BOTH
	};

	ustring name;

	/* Mesh Data */
	vector<float3> verts;
	vector<Triangle> triangles;
	vector<uint> shader;
	vector<bool> smooth;

	vector<uint> used_shaders;
	AttributeSet attributes;

	BoundBox bounds;
	bool transform_applied;
	bool transform_negative_scaled;
	DisplacementMethod displacement_method;

	/* Update Flags */
	bool need_update;
	bool need_update_rebuild;

	/* BVH */
	BVH *bvh;
	size_t tri_offset;
	size_t vert_offset;

	/* Functions */
	Mesh();
	~Mesh();

	void reserve(int numverts, int numfaces);
	void clear();
	void add_triangle(int v0, int v1, int v2, int shader, bool smooth);

	void compute_bounds();
	void add_face_normals();
	void add_vertex_normals();

	void pack_normals(Scene *scene, float4 *normal, float4 *vnormal);
	void pack_verts(float4 *tri_verts, float4 *tri_vindex, size_t vert_offset);
	void compute_bvh(SceneParams *params, Progress *progress, int n, int total);

	bool need_attribute(Scene *scene, AttributeStandard std);
	bool need_attribute(Scene *scene, ustring name);

	void tag_update(Scene *scene, bool rebuild);
};

/* Mesh Manager */

class MeshManager {
public:
	BVH *bvh;

	bool need_update;

	MeshManager();
	~MeshManager();

	bool displace(Device *device, Scene *scene, Mesh *mesh, Progress& progress);

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

