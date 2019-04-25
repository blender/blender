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

#include "render/image.h"
#include "render/mesh.h"
#include "render/attribute.h"

#include "util/util_foreach.h"
#include "util/util_transform.h"

CCL_NAMESPACE_BEGIN

/* Attribute */

Attribute::~Attribute()
{
	/* for voxel data, we need to remove the image from the image manager */
	if(element == ATTR_ELEMENT_VOXEL) {
		VoxelAttribute *voxel_data = data_voxel();

		if(voxel_data && voxel_data->slot != -1) {
			voxel_data->manager->remove_image(voxel_data->slot);
		}
	}
}

void Attribute::set(ustring name_, TypeDesc type_, AttributeElement element_)
{
	name = name_;
	type = type_;
	element = element_;
	std = ATTR_STD_NONE;
	flags = 0;

	/* string and matrix not supported! */
	assert(type == TypeDesc::TypeFloat || type == TypeDesc::TypeColor ||
		type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
		type == TypeDesc::TypeNormal || type == TypeDesc::TypeMatrix ||
		type == TypeFloat2);
}

void Attribute::resize(Mesh *mesh, AttributePrimitive prim, bool reserve_only)
{
	if(reserve_only) {
		buffer.reserve(buffer_size(mesh, prim));
	}
	else {
		buffer.resize(buffer_size(mesh, prim), 0);
	}
}

void Attribute::resize(size_t num_elements)
{
	buffer.resize(num_elements * data_sizeof(), 0);
}

void Attribute::add(const float& f)
{
	assert(data_sizeof() == sizeof(float));

	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

void Attribute::add(const uchar4& f)
{
	assert(data_sizeof() == sizeof(uchar4));

	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

void Attribute::add(const float2& f)
{
	assert(data_sizeof() == sizeof(float2));

	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

void Attribute::add(const float3& f)
{
	assert(data_sizeof() == sizeof(float3));

	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

void Attribute::add(const Transform& f)
{
	assert(data_sizeof() == sizeof(Transform));

	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

void Attribute::add(const VoxelAttribute& f)
{
	assert(data_sizeof() == sizeof(VoxelAttribute));

	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

void Attribute::add(const char *data)
{
	size_t size = data_sizeof();

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

size_t Attribute::data_sizeof() const
{
	if(element == ATTR_ELEMENT_VOXEL)
		return sizeof(VoxelAttribute);
	else if(element == ATTR_ELEMENT_CORNER_BYTE)
		return sizeof(uchar4);
	else if(type == TypeDesc::TypeFloat)
		return sizeof(float);
	else if(type == TypeFloat2)
		return sizeof(float2);
	else if(type == TypeDesc::TypeMatrix)
		return sizeof(Transform);
	else
		return sizeof(float3);
}

size_t Attribute::element_size(Mesh *mesh, AttributePrimitive prim) const
{
	if(flags & ATTR_FINAL_SIZE) {
		return buffer.size() / data_sizeof();
	}

	size_t size;

	switch(element) {
		case ATTR_ELEMENT_OBJECT:
		case ATTR_ELEMENT_MESH:
		case ATTR_ELEMENT_VOXEL:
			size = 1;
			break;
		case ATTR_ELEMENT_VERTEX:
			size = mesh->verts.size() + mesh->num_ngons;
			if(prim == ATTR_PRIM_SUBD) {
				size -= mesh->num_subd_verts;
			}
			break;
		case ATTR_ELEMENT_VERTEX_MOTION:
			size = (mesh->verts.size() + mesh->num_ngons) * (mesh->motion_steps - 1);
			if(prim == ATTR_PRIM_SUBD) {
				size -= mesh->num_subd_verts * (mesh->motion_steps - 1);
			}
			break;
		case ATTR_ELEMENT_FACE:
			if(prim == ATTR_PRIM_TRIANGLE) {
				size = mesh->num_triangles();
			}
			else {
				size = mesh->subd_faces.size() + mesh->num_ngons;
			}
			break;
		case ATTR_ELEMENT_CORNER:
		case ATTR_ELEMENT_CORNER_BYTE:
			if(prim == ATTR_PRIM_TRIANGLE) {
				size = mesh->num_triangles()*3;
			}
			else {
				size = mesh->subd_face_corners.size() + mesh->num_ngons;
			}
			break;
		case ATTR_ELEMENT_CURVE:
			size = mesh->num_curves();
			break;
		case ATTR_ELEMENT_CURVE_KEY:
			size = mesh->curve_keys.size();
			break;
		case ATTR_ELEMENT_CURVE_KEY_MOTION:
			size = mesh->curve_keys.size() * (mesh->motion_steps - 1);
			break;
		default:
			size = 0;
			break;
	}

	return size;
}

size_t Attribute::buffer_size(Mesh *mesh, AttributePrimitive prim) const
{
	return element_size(mesh, prim)*data_sizeof();
}

bool Attribute::same_storage(TypeDesc a, TypeDesc b)
{
	if(a == b)
		return true;

	if(a == TypeDesc::TypeColor || a == TypeDesc::TypePoint ||
	   a == TypeDesc::TypeVector || a == TypeDesc::TypeNormal)
	{
		if(b == TypeDesc::TypeColor || b == TypeDesc::TypePoint ||
		   b == TypeDesc::TypeVector || b == TypeDesc::TypeNormal)
		{
			return true;
		}
	}
	return false;
}

void Attribute::zero_data(void* dst)
{
	memset(dst, 0, data_sizeof());
}

void Attribute::add_with_weight(void* dst, void* src, float weight)
{
	if(element == ATTR_ELEMENT_CORNER_BYTE) {
		for(int i = 0; i < 4; i++) {
			((uchar*)dst)[i] += uchar(((uchar*)src)[i] * weight);
		}
	}
	else if(same_storage(type, TypeDesc::TypeFloat)) {
		*((float*)dst) += *((float*)src) * weight;
	}
	else if(same_storage(type, TypeFloat2)) {
		*((float2*)dst) += *((float2*)src) * weight;
	}
	else if(same_storage(type, TypeDesc::TypeVector)) {
		*((float4*)dst) += *((float4*)src) * weight;
	}
	else {
		assert(!"not implemented for this type");
	}
}

const char *Attribute::standard_name(AttributeStandard std)
{
	switch(std) {
		case ATTR_STD_VERTEX_NORMAL:
			return "N";
		case ATTR_STD_FACE_NORMAL:
			return "Ng";
		case ATTR_STD_UV:
			return "uv";
		case ATTR_STD_GENERATED:
			return "generated";
		case ATTR_STD_GENERATED_TRANSFORM:
			return "generated_transform";
		case ATTR_STD_UV_TANGENT:
			return "tangent";
		case ATTR_STD_UV_TANGENT_SIGN:
			return "tangent_sign";
		case ATTR_STD_POSITION_UNDEFORMED:
			return "undeformed";
		case ATTR_STD_POSITION_UNDISPLACED:
			return "undisplaced";
		case ATTR_STD_MOTION_VERTEX_POSITION:
			return "motion_P";
		case ATTR_STD_MOTION_VERTEX_NORMAL:
			return "motion_N";
		case ATTR_STD_PARTICLE:
			return "particle";
		case ATTR_STD_CURVE_INTERCEPT:
			return "curve_intercept";
		case ATTR_STD_CURVE_RANDOM:
			return "curve_random";
		case ATTR_STD_PTEX_FACE_ID:
			return "ptex_face_id";
		case ATTR_STD_PTEX_UV:
			return "ptex_uv";
		case ATTR_STD_VOLUME_DENSITY:
			return "density";
		case ATTR_STD_VOLUME_COLOR:
			return "color";
		case ATTR_STD_VOLUME_FLAME:
			return "flame";
		case ATTR_STD_VOLUME_HEAT:
			return "heat";
		case ATTR_STD_VOLUME_TEMPERATURE:
			return "temperature";
		case ATTR_STD_VOLUME_VELOCITY:
			return "velocity";
		case ATTR_STD_POINTINESS:
			return "pointiness";
		case ATTR_STD_NOT_FOUND:
		case ATTR_STD_NONE:
		case ATTR_STD_NUM:
			return "";
	}

	return "";
}

AttributeStandard Attribute::name_standard(const char *name)
{
	if(name) {
		for(int std = ATTR_STD_NONE; std < ATTR_STD_NUM; std++) {
			if(strcmp(name, Attribute::standard_name((AttributeStandard)std)) == 0) {
				return (AttributeStandard)std;
			}
		}
	}

	return ATTR_STD_NONE;
}

/* Attribute Set */

AttributeSet::AttributeSet()
{
	triangle_mesh = NULL;
	curve_mesh = NULL;
	subd_mesh = NULL;
}

AttributeSet::~AttributeSet()
{
}

Attribute *AttributeSet::add(ustring name, TypeDesc type, AttributeElement element)
{
	Attribute *attr = find(name);

	if(attr) {
		/* return if same already exists */
		if(attr->type == type && attr->element == element)
			return attr;

		/* overwrite attribute with same name but different type/element */
		remove(name);
	}

#if __cplusplus >= 201103L
	attributes.emplace_back();
	attr = &attributes.back();
	attr->set(name, type, element);
#else
	{
		Attribute attr_temp;
		attr_temp.set(name, type, element);
		attributes.push_back(attr_temp);
		attr = &attributes.back();
	}
#endif

	/* this is weak .. */
	if(triangle_mesh)
		attr->resize(triangle_mesh, ATTR_PRIM_TRIANGLE, false);
	if(curve_mesh)
		attr->resize(curve_mesh, ATTR_PRIM_CURVE, false);
	if(subd_mesh)
		attr->resize(subd_mesh, ATTR_PRIM_SUBD, false);

	return attr;
}

Attribute *AttributeSet::find(ustring name) const
{
	foreach(const Attribute& attr, attributes)
		if(attr.name == name)
			return (Attribute*)&attr;

	return NULL;
}

void AttributeSet::remove(ustring name)
{
	Attribute *attr = find(name);

	if(attr) {
		list<Attribute>::iterator it;

		for(it = attributes.begin(); it != attributes.end(); it++) {
			if(&*it == attr) {
				attributes.erase(it);
				return;
			}
		}
	}
}

Attribute *AttributeSet::add(AttributeStandard std, ustring name)
{
	Attribute *attr = NULL;

	if(name == ustring())
		name = Attribute::standard_name(std);

	if(triangle_mesh || subd_mesh) {
		switch(std) {
			case ATTR_STD_VERTEX_NORMAL:
				attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_VERTEX);
				break;
			case ATTR_STD_FACE_NORMAL:
				attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_FACE);
				break;
			case ATTR_STD_UV:
				attr = add(name, TypeFloat2, ATTR_ELEMENT_CORNER);
				break;
			case ATTR_STD_UV_TANGENT:
				attr = add(name, TypeDesc::TypeVector, ATTR_ELEMENT_CORNER);
				break;
			case ATTR_STD_UV_TANGENT_SIGN:
				attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CORNER);
				break;
			case ATTR_STD_GENERATED:
			case ATTR_STD_POSITION_UNDEFORMED:
			case ATTR_STD_POSITION_UNDISPLACED:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_VERTEX);
				break;
			case ATTR_STD_MOTION_VERTEX_POSITION:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_VERTEX_MOTION);
				break;
			case ATTR_STD_MOTION_VERTEX_NORMAL:
				attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_VERTEX_MOTION);
				break;
			case ATTR_STD_PTEX_FACE_ID:
				attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_FACE);
				break;
			case ATTR_STD_PTEX_UV:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_VERTEX);
				break;
			case ATTR_STD_GENERATED_TRANSFORM:
				attr = add(name, TypeDesc::TypeMatrix, ATTR_ELEMENT_MESH);
				break;
			case ATTR_STD_VOLUME_DENSITY:
			case ATTR_STD_VOLUME_FLAME:
			case ATTR_STD_VOLUME_HEAT:
			case ATTR_STD_VOLUME_TEMPERATURE:
				attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VOXEL);
				break;
			case ATTR_STD_VOLUME_COLOR:
				attr = add(name, TypeDesc::TypeColor, ATTR_ELEMENT_VOXEL);
				break;
			case ATTR_STD_VOLUME_VELOCITY:
				attr = add(name, TypeDesc::TypeVector, ATTR_ELEMENT_VOXEL);
				break;
			case ATTR_STD_POINTINESS:
				attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VERTEX);
				break;
			default:
				assert(0);
				break;
		}
	}
	else if(curve_mesh) {
		switch(std) {
			case ATTR_STD_UV:
				attr = add(name, TypeFloat2, ATTR_ELEMENT_CURVE);
				break;
			case ATTR_STD_GENERATED:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_CURVE);
				break;
			case ATTR_STD_MOTION_VERTEX_POSITION:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_CURVE_KEY_MOTION);
				break;
			case ATTR_STD_CURVE_INTERCEPT:
				attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CURVE_KEY);
				break;
			case ATTR_STD_CURVE_RANDOM:
				attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CURVE);
				break;
			case ATTR_STD_GENERATED_TRANSFORM:
				attr = add(name, TypeDesc::TypeMatrix, ATTR_ELEMENT_MESH);
				break;
			case ATTR_STD_POINTINESS:
				attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VERTEX);
				break;
			default:
				assert(0);
				break;
		}
	}

	attr->std = std;

	return attr;
}

Attribute *AttributeSet::find(AttributeStandard std) const
{
	foreach(const Attribute& attr, attributes)
		if(attr.std == std)
			return (Attribute*)&attr;

	return NULL;
}

void AttributeSet::remove(AttributeStandard std)
{
	Attribute *attr = find(std);

	if(attr) {
		list<Attribute>::iterator it;

		for(it = attributes.begin(); it != attributes.end(); it++) {
			if(&*it == attr) {
				attributes.erase(it);
				return;
			}
		}
	}
}

Attribute *AttributeSet::find(AttributeRequest& req)
{
	if(req.std == ATTR_STD_NONE)
		return find(req.name);
	else
		return find(req.std);
}

void AttributeSet::remove(Attribute *attribute)
{
	if(attribute->std == ATTR_STD_NONE) {
		remove(attribute->name);
	}
	else {
		remove(attribute->std);
	}
}

void AttributeSet::resize(bool reserve_only)
{
	foreach(Attribute& attr, attributes) {
		if(triangle_mesh)
			attr.resize(triangle_mesh, ATTR_PRIM_TRIANGLE, reserve_only);
		if(curve_mesh)
			attr.resize(curve_mesh, ATTR_PRIM_CURVE, reserve_only);
		if(subd_mesh)
			attr.resize(subd_mesh, ATTR_PRIM_SUBD, reserve_only);
	}
}

void AttributeSet::clear(bool preserve_voxel_data)
{
	if(preserve_voxel_data) {
		list<Attribute>::iterator it;

		for(it = attributes.begin(); it != attributes.end();) {
			if(it->element == ATTR_ELEMENT_VOXEL || it->std == ATTR_STD_GENERATED_TRANSFORM) {
				it++;
			}
			else {
				attributes.erase(it++);
			}
		}
	}
	else {
		attributes.clear();
	}
}

/* AttributeRequest */

AttributeRequest::AttributeRequest(ustring name_)
{
	name = name_;
	std = ATTR_STD_NONE;

	triangle_type = TypeDesc::TypeFloat;
	triangle_desc.element = ATTR_ELEMENT_NONE;
	triangle_desc.offset = 0;
	triangle_desc.type = NODE_ATTR_FLOAT;

	curve_type = TypeDesc::TypeFloat;
	curve_desc.element = ATTR_ELEMENT_NONE;
	curve_desc.offset = 0;
	curve_desc.type = NODE_ATTR_FLOAT;

	subd_type = TypeDesc::TypeFloat;
	subd_desc.element = ATTR_ELEMENT_NONE;
	subd_desc.offset = 0;
	subd_desc.type = NODE_ATTR_FLOAT;
}

AttributeRequest::AttributeRequest(AttributeStandard std_)
{
	name = ustring();
	std = std_;

	triangle_type = TypeDesc::TypeFloat;
	triangle_desc.element = ATTR_ELEMENT_NONE;
	triangle_desc.offset = 0;
	triangle_desc.type = NODE_ATTR_FLOAT;

	curve_type = TypeDesc::TypeFloat;
	curve_desc.element = ATTR_ELEMENT_NONE;
	curve_desc.offset = 0;
	curve_desc.type = NODE_ATTR_FLOAT;

	subd_type = TypeDesc::TypeFloat;
	subd_desc.element = ATTR_ELEMENT_NONE;
	subd_desc.offset = 0;
	subd_desc.type = NODE_ATTR_FLOAT;
}

/* AttributeRequestSet */

AttributeRequestSet::AttributeRequestSet()
{
}

AttributeRequestSet::~AttributeRequestSet()
{
}

bool AttributeRequestSet::modified(const AttributeRequestSet& other)
{
	if(requests.size() != other.requests.size())
		return true;

	for(size_t i = 0; i < requests.size(); i++) {
		bool found = false;

		for(size_t j = 0; j < requests.size() && !found; j++)
			if(requests[i].name == other.requests[j].name &&
			   requests[i].std == other.requests[j].std)
			{
				found = true;
			}

		if(!found) {
			return true;
		}
	}

	return false;
}

void AttributeRequestSet::add(ustring name)
{
	foreach(AttributeRequest& req, requests) {
		if(req.name == name) {
			return;
		}
	}

	requests.push_back(AttributeRequest(name));
}

void AttributeRequestSet::add(AttributeStandard std)
{
	foreach(AttributeRequest& req, requests)
		if(req.std == std)
			return;

	requests.push_back(AttributeRequest(std));
}

void AttributeRequestSet::add(AttributeRequestSet& reqs)
{
	foreach(AttributeRequest& req, reqs.requests) {
		if(req.std == ATTR_STD_NONE)
			add(req.name);
		else
			add(req.std);
	}
}

void AttributeRequestSet::add_standard(ustring name)
{
	if(name.empty()) {
		return;
	}

	AttributeStandard std = Attribute::name_standard(name.c_str());

	if(std) {
		add(std);
	}
	else {
		add(name);
	}
}

bool AttributeRequestSet::find(ustring name)
{
	foreach(AttributeRequest& req, requests)
		if(req.name == name)
			return true;

	return false;
}

bool AttributeRequestSet::find(AttributeStandard std)
{
	foreach(AttributeRequest& req, requests)
		if(req.std == std)
			return true;

	return false;
}

size_t AttributeRequestSet::size()
{
	return requests.size();
}

void AttributeRequestSet::clear()
{
	requests.clear();
}

CCL_NAMESPACE_END
