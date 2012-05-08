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

#include "mesh.h"
#include "attribute.h"

#include "util_debug.h"
#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Attribute */

void Attribute::set(ustring name_, TypeDesc type_, Element element_)
{
	name = name_;
	type = type_;
	element = element_;
	std = STD_NONE;

	/* string and matrix not supported! */
	assert(type == TypeDesc::TypeFloat || type == TypeDesc::TypeColor ||
		type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
		type == TypeDesc::TypeNormal);
}

void Attribute::reserve(int numverts, int numtris)
{
	buffer.resize(buffer_size(numverts, numtris), 0);
}

size_t Attribute::data_sizeof()
{
	if(type == TypeDesc::TypeFloat)
		return sizeof(float);
	else
		return sizeof(float3);
}

size_t Attribute::element_size(int numverts, int numtris)
{
	if(element == VERTEX)
		return numverts;
	else if(element == FACE)
		return numtris;
	else
		return numtris*3;
}

size_t Attribute::buffer_size(int numverts, int numtris)
{
	return element_size(numverts, numtris)*data_sizeof();
}

bool Attribute::same_storage(TypeDesc a, TypeDesc b)
{
	if(a == b)
		return true;
	
	if(a == TypeDesc::TypeColor || a == TypeDesc::TypePoint ||
	   a == TypeDesc::TypeVector || a == TypeDesc::TypeNormal)
		if(b == TypeDesc::TypeColor || b == TypeDesc::TypePoint ||
		   b == TypeDesc::TypeVector || b == TypeDesc::TypeNormal)
			return true;
	
	return false;
}

ustring Attribute::standard_name(Attribute::Standard std)
{
	if(std == Attribute::STD_VERTEX_NORMAL)
		return ustring("N");
	else if(std == Attribute::STD_FACE_NORMAL)
		return ustring("Ng");
	else if(std == Attribute::STD_UV)
		return ustring("uv");
	else if(std == Attribute::STD_GENERATED)
		return ustring("generated");
	else if(std == Attribute::STD_POSITION_UNDEFORMED)
		return ustring("undeformed");
	else if(std == Attribute::STD_POSITION_UNDISPLACED)
		return ustring("undisplaced");

	return ustring();
}

/* Attribute Set */

AttributeSet::AttributeSet()
{
	mesh = NULL;
}

AttributeSet::~AttributeSet()
{
}

Attribute *AttributeSet::add(ustring name, TypeDesc type, Attribute::Element element)
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

	if(element == Attribute::VERTEX)
		attr->set(name, type, element);
	else if(element == Attribute::FACE)
		attr->set(name, type, element);
	else if(element == Attribute::CORNER)
		attr->set(name, type, element);
	
	if(mesh)
		attr->reserve(mesh->verts.size(), mesh->triangles.size());
	
	return attr;
}

Attribute *AttributeSet::find(ustring name)
{
	foreach(Attribute& attr, attributes)
		if(attr.name == name)
			return &attr;

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

Attribute *AttributeSet::add(Attribute::Standard std, ustring name)
{
	Attribute *attr = NULL;

	if(name == ustring())
		name = Attribute::standard_name(std);

	if(std == Attribute::STD_VERTEX_NORMAL)
		attr = add(name, TypeDesc::TypeNormal, Attribute::VERTEX);
	else if(std == Attribute::STD_FACE_NORMAL)
		attr = add(name, TypeDesc::TypeNormal, Attribute::FACE);
	else if(std == Attribute::STD_UV)
		attr = add(name, TypeDesc::TypePoint, Attribute::CORNER);
	else if(std == Attribute::STD_GENERATED)
		attr = add(name, TypeDesc::TypePoint, Attribute::VERTEX);
	else if(std == Attribute::STD_POSITION_UNDEFORMED)
		attr = add(name, TypeDesc::TypePoint, Attribute::VERTEX);
	else if(std == Attribute::STD_POSITION_UNDISPLACED)
		attr = add(name, TypeDesc::TypePoint, Attribute::VERTEX);
	else
		assert(0);

	attr->std = std;
	
	return attr;
}

Attribute *AttributeSet::find(Attribute::Standard std)
{
	foreach(Attribute& attr, attributes)
		if(attr.std == std)
			return &attr;

	return NULL;
}

void AttributeSet::remove(Attribute::Standard std)
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
	if(req.std == Attribute::STD_NONE)
		return find(req.name);
	else
		return find(req.std);
}

void AttributeSet::reserve(int numverts, int numtris)
{
	foreach(Attribute& attr, attributes)
		attr.reserve(numverts, numtris);
}

void AttributeSet::clear()
{
	attributes.clear();
}

/* AttributeRequest */

AttributeRequest::AttributeRequest(ustring name_)
{
	name = name_;
	std = Attribute::STD_NONE;

	type = TypeDesc::TypeFloat;
	element = ATTR_ELEMENT_NONE;
	offset = 0;
}

AttributeRequest::AttributeRequest(Attribute::Standard std_)
{
	name = ustring();
	std = std_;

	type = TypeDesc::TypeFloat;
	element = ATTR_ELEMENT_NONE;
	offset = 0;
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
				found = true;

		if(!found)
			return true;
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

void AttributeRequestSet::add(Attribute::Standard std)
{
	foreach(AttributeRequest& req, requests)
		if(req.std == std)
			return;

	requests.push_back(AttributeRequest(std));
}

void AttributeRequestSet::add(AttributeRequestSet& reqs)
{
	foreach(AttributeRequest& req, reqs.requests) {
		if(req.std == Attribute::STD_NONE)
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

bool AttributeRequestSet::find(Attribute::Standard std)
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

