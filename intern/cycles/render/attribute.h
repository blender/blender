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

#ifndef __ATTRIBUTE_H__
#define __ATTRIBUTE_H__

#include "kernel/kernel_types.h"

#include "util/util_list.h"
#include "util/util_param.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Attribute;
class AttributeRequest;
class AttributeRequestSet;
class AttributeSet;
class ImageManager;
class Mesh;
struct Transform;

/* Attributes for voxels are images */

struct VoxelAttribute {
	ImageManager *manager;
	int slot;
};

/* Attribute
 *
 * Arbitrary data layers on meshes.
 * Supported types: Float, Color, Vector, Normal, Point */

class Attribute {
public:
	ustring name;
	AttributeStandard std;

	TypeDesc type;
	vector<char> buffer;
	AttributeElement element;
	uint flags; /* enum AttributeFlag */

	Attribute() {}
	~Attribute();
	void set(ustring name, TypeDesc type, AttributeElement element);
	void resize(Mesh *mesh, AttributePrimitive prim, bool reserve_only);
	void resize(size_t num_elements);

	size_t data_sizeof() const;
	size_t element_size(Mesh *mesh, AttributePrimitive prim) const;
	size_t buffer_size(Mesh *mesh, AttributePrimitive prim) const;

	char *data() { return (buffer.size())? &buffer[0]: NULL; };
	float3 *data_float3() { return (float3*)data(); }
	float4 *data_float4() { return (float4*)data(); }
	float *data_float() { return (float*)data(); }
	uchar4 *data_uchar4() { return (uchar4*)data(); }
	Transform *data_transform() { return (Transform*)data(); }
	VoxelAttribute *data_voxel()  { return ( VoxelAttribute*)data(); }

	const char *data() const { return (buffer.size())? &buffer[0]: NULL; }
	const float3 *data_float3() const { return (const float3*)data(); }
	const float4 *data_float4() const { return (const float4*)data(); }
	const float *data_float() const { return (const float*)data(); }
	const Transform *data_transform() const { return (const Transform*)data(); }
	const VoxelAttribute *data_voxel() const { return (const VoxelAttribute*)data(); }

	void zero_data(void* dst);
	void add_with_weight(void* dst, void* src, float weight);

	void add(const float& f);
	void add(const float3& f);
	void add(const uchar4& f);
	void add(const Transform& f);
	void add(const VoxelAttribute& f);
	void add(const char *data);

	static bool same_storage(TypeDesc a, TypeDesc b);
	static const char *standard_name(AttributeStandard std);
	static AttributeStandard name_standard(const char *name);
};

/* Attribute Set
 *
 * Set of attributes on a mesh. */

class AttributeSet {
public:
	Mesh *triangle_mesh;
	Mesh *curve_mesh;
	Mesh *subd_mesh;
	list<Attribute> attributes;

	AttributeSet();
	~AttributeSet();

	Attribute *add(ustring name, TypeDesc type, AttributeElement element);
	Attribute *find(ustring name) const;
	void remove(ustring name);

	Attribute *add(AttributeStandard std, ustring name = ustring());
	Attribute *find(AttributeStandard std) const;
	void remove(AttributeStandard std);

	Attribute *find(AttributeRequest& req);

	void resize(bool reserve_only = false);
	void clear();
};

/* AttributeRequest
 *
 * Request from a shader to use a certain attribute, so we can figure out
 * which ones we need to export from the host app end store for the kernel.
 * The attribute is found either by name or by standard attribute type. */

class AttributeRequest {
public:
	ustring name;
	AttributeStandard std;

	/* temporary variables used by MeshManager */
	TypeDesc triangle_type, curve_type, subd_type;
	AttributeDescriptor triangle_desc, curve_desc, subd_desc;

	explicit AttributeRequest(ustring name_);
	explicit AttributeRequest(AttributeStandard std);
};

/* AttributeRequestSet
 *
 * Set of attributes requested by a shader. */

class AttributeRequestSet {
public:
	vector<AttributeRequest> requests;

	AttributeRequestSet();
	~AttributeRequestSet();

	void add(ustring name);
	void add(AttributeStandard std);
	void add(AttributeRequestSet& reqs);

	bool find(ustring name);
	bool find(AttributeStandard std);

	size_t size();
	void clear();

	bool modified(const AttributeRequestSet& other);
};

CCL_NAMESPACE_END

#endif /* __ATTRIBUTE_H__ */

