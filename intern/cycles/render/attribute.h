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

#ifndef __ATTRIBUTE_H__
#define __ATTRIBUTE_H__

#include "kernel_types.h"

#include "util_list.h"
#include "util_param.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Attribute;
class AttributeSet;
class AttributeRequest;
class AttributeRequestSet;
class Mesh;

/* Attribute
 *
 * Arbitrary data layers on meshes.
 * Supported types: Float, Color, Vector, Normal, Point */

class Attribute {
public:
	enum Element {
		VERTEX,
		FACE,
		CORNER
	};

	enum Standard {
		STD_NONE = 0,
		STD_VERTEX_NORMAL,
		STD_FACE_NORMAL,
		STD_UV,
		STD_GENERATED,
		STD_POSITION_UNDEFORMED,
		STD_POSITION_UNDISPLACED,
		STD_NUM
	};

	ustring name;
	Standard std;

	TypeDesc type;
	vector<char> buffer;
	Element element;

	Attribute() {}
	void set(ustring name, TypeDesc type, Element element);
	void reserve(int numverts, int numfaces);

	size_t data_sizeof();
	size_t element_size(int numverts, int numfaces);
	size_t buffer_size(int numverts, int numfaces);

	char *data() { return (buffer.size())? &buffer[0]: NULL; };
	float3 *data_float3() { return (float3*)data(); }
	float *data_float() { return (float*)data(); }

	const char *data() const { return (buffer.size())? &buffer[0]: NULL; }
	const float3 *data_float3() const { return (float3*)data(); }
	const float *data_float() const { return (float*)data(); }

	static bool same_storage(TypeDesc a, TypeDesc b);
	static ustring standard_name(Attribute::Standard std);
};

/* Attribute Set
 *
 * Set of attributes on a mesh. */

class AttributeSet {
public:
	Mesh *mesh;
	list<Attribute> attributes;

	AttributeSet();
	~AttributeSet();

	Attribute *add(ustring name, TypeDesc type, Attribute::Element element);
	Attribute *find(ustring name);
	void remove(ustring name);

	Attribute *add(Attribute::Standard std, ustring name = ustring());
	Attribute *find(Attribute::Standard std);
	void remove(Attribute::Standard std);

	Attribute *find(AttributeRequest& req);

	void reserve(int numverts, int numfaces);
	void clear();
};

/* AttributeRequest
 *
 * Request from a shader to use a certain attribute, so we can figure out
 * which ones we need to export from the host app end store for the kernel.
 * The attribute is found either by name or by standard. */

class AttributeRequest {
public:
	ustring name;
	Attribute::Standard std;

	/* temporary variables used by MeshManager */
	TypeDesc type;
	AttributeElement element;
	int offset;

	AttributeRequest(ustring name_);
	AttributeRequest(Attribute::Standard std);
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
	void add(Attribute::Standard std);
	void add(AttributeRequestSet& reqs);

	bool find(ustring name);
	bool find(Attribute::Standard std);

	size_t size();
	void clear();

	bool modified(const AttributeRequestSet& other);
};

CCL_NAMESPACE_END

#endif /* __ATTRIBUTE_H__ */

