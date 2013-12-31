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

#include "mesh.h"
#include "attribute.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_transform.h"

CCL_NAMESPACE_BEGIN

/* Attribute */

void Attribute::set(ustring name_, TypeDesc type_, AttributeElement element_)
{
	name = name_;
	type = type_;
	element = element_;
	std = ATTR_STD_NONE;

	/* string and matrix not supported! */
	assert(type == TypeDesc::TypeFloat || type == TypeDesc::TypeColor ||
		type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
		type == TypeDesc::TypeNormal || type == TypeDesc::TypeMatrix);
}

void Attribute::reserve(int numverts, int numtris, int numcurves, int numkeys)
{
	buffer.resize(buffer_size(numverts, numtris, numcurves, numkeys), 0);
}

void Attribute::add(const float& f)
{
	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

void Attribute::add(const float3& f)
{
	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

void Attribute::add(const Transform& f)
{
	char *data = (char*)&f;
	size_t size = sizeof(f);

	for(size_t i = 0; i < size; i++)
		buffer.push_back(data[i]);
}

size_t Attribute::data_sizeof() const
{
	if(type == TypeDesc::TypeFloat)
		return sizeof(float);
	else if(type == TypeDesc::TypeMatrix)
		return sizeof(Transform);
	else
		return sizeof(float3);
}

size_t Attribute::element_size(int numverts, int numtris, int numcurves, int numkeys) const
{
	size_t size;
	
	switch(element) {
		case ATTR_ELEMENT_OBJECT:
		case ATTR_ELEMENT_MESH:
			size = 1;
			break;
		case ATTR_ELEMENT_VERTEX:
			size = numverts;
			break;
		case ATTR_ELEMENT_FACE:
			size = numtris;
			break;
		case ATTR_ELEMENT_CORNER:
			size = numtris*3;
			break;
		case ATTR_ELEMENT_CURVE:
			size = numcurves;
			break;
		case ATTR_ELEMENT_CURVE_KEY:
			size = numkeys;
			break;
		default:
			size = 0;
			break;
	}
	
	return size;
}

size_t Attribute::buffer_size(int numverts, int numtris, int numcurves, int numkeys) const
{
	return element_size(numverts, numtris, numcurves, numkeys)*data_sizeof();
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

const char *Attribute::standard_name(AttributeStandard std)
{
	if(std == ATTR_STD_VERTEX_NORMAL)
		return "N";
	else if(std == ATTR_STD_FACE_NORMAL)
		return "Ng";
	else if(std == ATTR_STD_UV)
		return "uv";
	else if(std == ATTR_STD_GENERATED)
		return "generated";
	else if(std == ATTR_STD_UV_TANGENT)
		return "tangent";
	else if(std == ATTR_STD_UV_TANGENT_SIGN)
		return "tangent_sign";
	else if(std == ATTR_STD_POSITION_UNDEFORMED)
		return "undeformed";
	else if(std == ATTR_STD_POSITION_UNDISPLACED)
		return "undisplaced";
	else if(std == ATTR_STD_MOTION_PRE)
		return "motion_pre";
	else if(std == ATTR_STD_MOTION_POST)
		return "motion_post";
	else if(std == ATTR_STD_PARTICLE)
		return "particle";
	else if(std == ATTR_STD_CURVE_INTERCEPT)
		return "curve_intercept";
	else if(std == ATTR_STD_PTEX_FACE_ID)
		return "ptex_face_id";
	else if(std == ATTR_STD_PTEX_UV)
		return "ptex_uv";
	else if(std == ATTR_STD_GENERATED_TRANSFORM)
		return "generated_transform";
	
	return "";
}

/* Attribute Set */

AttributeSet::AttributeSet()
{
	triangle_mesh = NULL;
	curve_mesh = NULL;
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

	attributes.push_back(Attribute());
	attr = &attributes.back();

	attr->set(name, type, element);
	
	/* this is weak .. */
	if(triangle_mesh)
		attr->reserve(triangle_mesh->verts.size(), triangle_mesh->triangles.size(), 0, 0);
	if(curve_mesh)
		attr->reserve(0, 0, curve_mesh->curves.size(), curve_mesh->curve_keys.size());
	
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

	if(triangle_mesh) {
		switch(std) {
			case ATTR_STD_VERTEX_NORMAL:
				attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_VERTEX);
				break;
			case ATTR_STD_FACE_NORMAL:
				attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_FACE);
				break;
			case ATTR_STD_UV:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_CORNER);
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
			case ATTR_STD_MOTION_PRE:
			case ATTR_STD_MOTION_POST:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_VERTEX);
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
			default:
				assert(0);
				break;
		}
	}
	else if(curve_mesh) {
		switch(std) {
			case ATTR_STD_UV:
			case ATTR_STD_GENERATED:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_CURVE);
				break;
			case ATTR_STD_MOTION_PRE:
			case ATTR_STD_MOTION_POST:
				attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_CURVE_KEY);
				break;
			case ATTR_STD_CURVE_INTERCEPT:
				attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CURVE_KEY);
				break;
			case ATTR_STD_GENERATED_TRANSFORM:
				attr = add(name, TypeDesc::TypeMatrix, ATTR_ELEMENT_MESH);
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

void AttributeSet::reserve()
{
	foreach(Attribute& attr, attributes) {
		if(triangle_mesh)
			attr.reserve(triangle_mesh->verts.size(), triangle_mesh->triangles.size(), 0, 0);
		if(curve_mesh)
			attr.reserve(0, 0, curve_mesh->curves.size(), curve_mesh->curve_keys.size());
	}
}

void AttributeSet::clear()
{
	attributes.clear();
}

/* AttributeRequest */

AttributeRequest::AttributeRequest(ustring name_)
{
	name = name_;
	std = ATTR_STD_NONE;

	triangle_type = TypeDesc::TypeFloat;
	triangle_element = ATTR_ELEMENT_NONE;
	triangle_offset = 0;

	curve_type = TypeDesc::TypeFloat;
	curve_element = ATTR_ELEMENT_NONE;
	curve_offset = 0;
}

AttributeRequest::AttributeRequest(AttributeStandard std_)
{
	name = ustring();
	std = std_;

	triangle_type = TypeDesc::TypeFloat;
	triangle_element = ATTR_ELEMENT_NONE;
	triangle_offset = 0;

	curve_type = TypeDesc::TypeFloat;
	curve_element = ATTR_ELEMENT_NONE;
	curve_offset = 0;
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
	foreach(AttributeRequest& req, requests)
		if(req.name == name)
			return;

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

