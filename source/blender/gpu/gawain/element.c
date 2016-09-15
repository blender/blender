
// Gawain element list (AKA index buffer)
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "element.h"
#include <stdlib.h>

#define KEEP_SINGLE_COPY 1

unsigned ElementList_size(const ElementList* elem)
	{
#if TRACK_INDEX_RANGE
	switch (elem->index_type)
		{
		case GL_UNSIGNED_BYTE: return elem->index_ct * sizeof(GLubyte);
		case GL_UNSIGNED_SHORT: return elem->index_ct * sizeof(GLushort);
		case GL_UNSIGNED_INT: return elem->index_ct * sizeof(GLuint);
		default:
			assert(false);
			return 0;
		}

#else
	return elem->index_ct * sizeof(GLuint);
#endif
	}

static void ElementList_prime(ElementList* elem)
	{
	glGenBuffers(1, &elem->vbo_id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elem->vbo_id);
	// fill with delicious data & send to GPU the first time only
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementList_size(elem), elem->data, GL_STATIC_DRAW);

#if KEEP_SINGLE_COPY
	// now that GL has a copy, discard original
	free(elem->data);
	elem->data = NULL;
#endif
	}

void ElementList_use(ElementList* elem)
	{
	if (elem->vbo_id)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elem->vbo_id);
	else
		ElementList_prime(elem);
	}

void ElementListBuilder_init(ElementListBuilder* builder, GLenum prim_type, unsigned prim_ct, unsigned vertex_ct)
	{
	unsigned verts_per_prim = 0;
	switch (prim_type)
		{
		case GL_POINTS:
			verts_per_prim = 1;
			break;
		case GL_LINES:
			verts_per_prim = 2;
			break;
		case GL_TRIANGLES:
			verts_per_prim = 3;
			break;
		default:
			assert(false);
		}

	builder->max_allowed_index = vertex_ct - 1;
	builder->max_index_ct = prim_ct * verts_per_prim;
	builder->index_ct = 0; // start empty
	builder->prim_type = prim_type;
	builder->data = calloc(builder->max_index_ct, sizeof(unsigned));
	}

void add_generic_vertex(ElementListBuilder* builder, unsigned v)
	{
#if TRUST_NO_ONE
	assert(builder->data != NULL);
	assert(builder->index_ct < builder->max_index_ct);
	assert(v <= builder->max_allowed_index);
#endif

	builder->data[builder->index_ct++] = v;
	}

void add_point_vertex(ElementListBuilder* builder, unsigned v)
	{
#if TRUST_NO_ONE
	assert(builder->prim_type == GL_POINTS);
#endif

	add_generic_vertex(builder, v);
	}

void add_line_vertices(ElementListBuilder* builder, unsigned v1, unsigned v2)
	{
#if TRUST_NO_ONE
	assert(builder->prim_type == GL_LINES);
	assert(v1 != v2);
#endif

	add_generic_vertex(builder, v1);
	add_generic_vertex(builder, v2);
	}

void add_triangle_vertices(ElementListBuilder* builder, unsigned v1, unsigned v2, unsigned v3)
	{
#if TRUST_NO_ONE
	assert(builder->prim_type == GL_TRIANGLES);
	assert(v1 != v2 && v2 != v3 && v3 != v1);
#endif

	add_generic_vertex(builder, v1);
	add_generic_vertex(builder, v2);
	add_generic_vertex(builder, v3);
	}

#if TRACK_INDEX_RANGE
// Everything remains 32 bit while building to keep things simple.
// Find min/max after, then convert to smallest index type possible.

static unsigned index_range(const unsigned values[], unsigned value_ct, unsigned* min_out, unsigned* max_out)
	{
	unsigned min_value = values[0];
	unsigned max_value = values[0];
	for (unsigned i = 1; i < value_ct; ++i)
		{
		const unsigned value = values[i];
		if (value < min_value)
			min_value = value;
		else if (value > max_value)
			max_value = value;
		}
	*min_out = min_value;
	*max_out = max_value;
	return max_value - min_value;
	}

static void squeeze_indices_byte(const unsigned values[], ElementList* elem)
	{
	const unsigned index_ct = elem->index_ct;
	GLubyte* data = malloc(index_ct * sizeof(GLubyte));

	if (elem->max_index > 0xFF)
		{
		const unsigned base = elem->min_index;

		elem->base_index = base;
		elem->min_index = 0;
		elem->max_index -= base;

		for (unsigned i = 0; i < index_ct; ++i)
			data[i] = (GLubyte)(values[i] - base);
		}
	else
		{
		elem->base_index = 0;

		for (unsigned i = 0; i < index_ct; ++i)
			data[i] = (GLubyte)(values[i]);
		}

	elem->data = data;
	}

static void squeeze_indices_short(const unsigned values[], ElementList* elem)
	{
	const unsigned index_ct = elem->index_ct;
	GLushort* data = malloc(index_ct * sizeof(GLushort));

	if (elem->max_index > 0xFFFF)
		{
		const unsigned base = elem->min_index;

		elem->base_index = base;
		elem->min_index = 0;
		elem->max_index -= base;

		for (unsigned i = 0; i < index_ct; ++i)
			data[i] = (GLushort)(values[i] - base);
		}
	else
		{
		elem->base_index = 0;

		for (unsigned i = 0; i < index_ct; ++i)
			data[i] = (GLushort)(values[i]);
		}

	elem->data = data;
	}

#endif // TRACK_INDEX_RANGE

void ElementList_build(ElementListBuilder* builder, ElementList* elem)
	{
#if TRUST_NO_ONE
	assert(builder->data != NULL);
#endif

	elem->index_ct = builder->index_ct;

#if TRACK_INDEX_RANGE
	const unsigned range = index_range(builder->data, builder->index_ct, &elem->min_index, &elem->max_index);

	if (range <= 0xFF)
		{
		elem->index_type = GL_UNSIGNED_BYTE;
		squeeze_indices_byte(builder->data, elem);
		}
	else if (range <= 0xFFFF)
		{
		elem->index_type = GL_UNSIGNED_SHORT;
		squeeze_indices_short(builder->data, elem);
		}
	else
		{
		elem->index_type = GL_UNSIGNED_INT;
		elem->base_index = 0;

		if (builder->index_ct < builder->max_index_ct)
			{
			builder->data = realloc(builder->data, builder->index_ct * sizeof(unsigned));
			// TODO: realloc only if index_ct is much smaller than max_index_ct
			}

		elem->data = builder->data;
		}
#else
	if (builder->index_ct < builder->max_index_ct)
		{
		builder->data = realloc(builder->data, builder->index_ct * sizeof(unsigned));
		// TODO: realloc only if index_ct is much smaller than max_index_ct
		}

	elem->data = builder->data;
#endif

	// elem->data will never be *larger* than builder->data... how about converting
	// in place to avoid extra allocation?

	elem->vbo_id = 0;
	// TODO: create GL buffer object directly, based on an input flag

	// discard builder (one-time use)
	if (builder->data != elem->data)
		free(builder->data);
	builder->data = NULL;
	// other fields are safe to leave
	}
