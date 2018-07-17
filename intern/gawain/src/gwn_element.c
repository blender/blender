/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gwn_element.c
 *  \ingroup gpu
 *
 * Gawain element list (AKA index buffer)
 */

#include "gwn_element.h"
#include "gwn_buffer_id.h"
#include <stdlib.h>

#define KEEP_SINGLE_COPY 1

static GLenum convert_index_type_to_gl(Gwn_IndexBufType type)
{
	static const GLenum table[] = {
		[GWN_INDEX_U8] = GL_UNSIGNED_BYTE, /* GL has this, Vulkan does not */
		[GWN_INDEX_U16] = GL_UNSIGNED_SHORT,
		[GWN_INDEX_U32] = GL_UNSIGNED_INT
	};
	return table[type];
}

uint GWN_indexbuf_size_get(const Gwn_IndexBuf* elem)
{
#if GWN_TRACK_INDEX_RANGE
	static const uint table[] = {
		[GWN_INDEX_U8] = sizeof(GLubyte), /* GL has this, Vulkan does not */
		[GWN_INDEX_U16] = sizeof(GLushort),
		[GWN_INDEX_U32] = sizeof(GLuint)
	};
	return elem->index_len * table[elem->index_type];
#else
	return elem->index_len * sizeof(GLuint);
#endif
}

void GWN_indexbuf_init_ex(
        Gwn_IndexBufBuilder* builder, Gwn_PrimType prim_type,
        uint index_len, uint vertex_len, bool use_prim_restart)
{
	builder->use_prim_restart = use_prim_restart;
	builder->max_allowed_index = vertex_len - 1;
	builder->max_index_len = index_len;
	builder->index_len = 0; // start empty
	builder->prim_type = prim_type;
	builder->data = calloc(builder->max_index_len, sizeof(uint));
}

void GWN_indexbuf_init(Gwn_IndexBufBuilder* builder, Gwn_PrimType prim_type, uint prim_len, uint vertex_len)
{
	uint verts_per_prim = 0;
	switch (prim_type) {
		case GWN_PRIM_POINTS:
			verts_per_prim = 1;
			break;
		case GWN_PRIM_LINES:
			verts_per_prim = 2;
			break;
		case GWN_PRIM_TRIS:
			verts_per_prim = 3;
			break;
		case GWN_PRIM_LINES_ADJ:
			verts_per_prim = 4;
			break;
		default:
#if TRUST_NO_ONE
			assert(false);
#endif
			return;
	}

	GWN_indexbuf_init_ex(builder, prim_type, prim_len * verts_per_prim, vertex_len, false);
}

void GWN_indexbuf_add_generic_vert(Gwn_IndexBufBuilder* builder, uint v)
{
#if TRUST_NO_ONE
	assert(builder->data != NULL);
	assert(builder->index_len < builder->max_index_len);
	assert(v <= builder->max_allowed_index);
#endif
	builder->data[builder->index_len++] = v;
}

void GWN_indexbuf_add_primitive_restart(Gwn_IndexBufBuilder* builder)
{
#if TRUST_NO_ONE
	assert(builder->data != NULL);
	assert(builder->index_len < builder->max_index_len);
	assert(builder->use_prim_restart);
#endif
	builder->data[builder->index_len++] = GWN_PRIM_RESTART;
}

void GWN_indexbuf_add_point_vert(Gwn_IndexBufBuilder* builder, uint v)
{
#if TRUST_NO_ONE
	assert(builder->prim_type == GWN_PRIM_POINTS);
#endif
	GWN_indexbuf_add_generic_vert(builder, v);
}

void GWN_indexbuf_add_line_verts(Gwn_IndexBufBuilder* builder, uint v1, uint v2)
{
#if TRUST_NO_ONE
	assert(builder->prim_type == GWN_PRIM_LINES);
	assert(v1 != v2);
#endif
	GWN_indexbuf_add_generic_vert(builder, v1);
	GWN_indexbuf_add_generic_vert(builder, v2);
}

void GWN_indexbuf_add_tri_verts(Gwn_IndexBufBuilder* builder, uint v1, uint v2, uint v3)
{
#if TRUST_NO_ONE
	assert(builder->prim_type == GWN_PRIM_TRIS);
	assert(v1 != v2 && v2 != v3 && v3 != v1);
#endif
	GWN_indexbuf_add_generic_vert(builder, v1);
	GWN_indexbuf_add_generic_vert(builder, v2);
	GWN_indexbuf_add_generic_vert(builder, v3);
}

void GWN_indexbuf_add_line_adj_verts(Gwn_IndexBufBuilder* builder, uint v1, uint v2, uint v3, uint v4)
{
#if TRUST_NO_ONE
	assert(builder->prim_type == GWN_PRIM_LINES_ADJ);
	assert(v2 != v3); /* only the line need diff indices */
#endif
	GWN_indexbuf_add_generic_vert(builder, v1);
	GWN_indexbuf_add_generic_vert(builder, v2);
	GWN_indexbuf_add_generic_vert(builder, v3);
	GWN_indexbuf_add_generic_vert(builder, v4);
}

#if GWN_TRACK_INDEX_RANGE
/* Everything remains 32 bit while building to keep things simple.
 * Find min/max after, then convert to smallest index type possible. */

static uint index_range(const uint values[], uint value_len, uint* min_out, uint* max_out)
{
	if (value_len == 0) {
		*min_out = 0;
		*max_out = 0;
		return 0;
	}
	uint min_value = values[0];
	uint max_value = values[0];
	for (uint i = 1; i < value_len; ++i) {
		const uint value = values[i];
		if (value == GWN_PRIM_RESTART)
			continue;
		else if (value < min_value)
			min_value = value;
		else if (value > max_value)
			max_value = value;
	}
	*min_out = min_value;
	*max_out = max_value;
	return max_value - min_value;
}

static void squeeze_indices_byte(Gwn_IndexBufBuilder *builder, Gwn_IndexBuf* elem)
{
	const uint *values = builder->data;
	const uint index_len = elem->index_len;

	/* data will never be *larger* than builder->data...
	 * converting in place to avoid extra allocation */
	GLubyte *data = (GLubyte *)builder->data;

	if (elem->max_index > 0xFF) {
		const uint base = elem->min_index;
		elem->base_index = base;
		elem->min_index = 0;
		elem->max_index -= base;
		for (uint i = 0; i < index_len; ++i) {
			data[i] = (values[i] == GWN_PRIM_RESTART) ? 0xFF : (GLubyte)(values[i] - base);
		}
	}
	else {
		elem->base_index = 0;
		for (uint i = 0; i < index_len; ++i) {
			data[i] = (GLubyte)(values[i]);
		}
	}
}

static void squeeze_indices_short(Gwn_IndexBufBuilder *builder, Gwn_IndexBuf* elem)
{
	const uint *values = builder->data;
	const uint index_len = elem->index_len;

	/* data will never be *larger* than builder->data...
	 * converting in place to avoid extra allocation */
	GLushort *data = (GLushort *)builder->data;

	if (elem->max_index > 0xFFFF) {
		const uint base = elem->min_index;
		elem->base_index = base;
		elem->min_index = 0;
		elem->max_index -= base;
		for (uint i = 0; i < index_len; ++i) {
			data[i] = (values[i] == GWN_PRIM_RESTART) ? 0xFFFF : (GLushort)(values[i] - base);
		}
	}
	else {
		elem->base_index = 0;
		for (uint i = 0; i < index_len; ++i) {
			data[i] = (GLushort)(values[i]);
		}
	}
}

#endif /* GWN_TRACK_INDEX_RANGE */

Gwn_IndexBuf* GWN_indexbuf_build(Gwn_IndexBufBuilder* builder)
{
	Gwn_IndexBuf* elem = calloc(1, sizeof(Gwn_IndexBuf));
	GWN_indexbuf_build_in_place(builder, elem);
	return elem;
}

void GWN_indexbuf_build_in_place(Gwn_IndexBufBuilder* builder, Gwn_IndexBuf* elem)
{
#if TRUST_NO_ONE
	assert(builder->data != NULL);
#endif
	elem->index_len = builder->index_len;
	elem->use_prim_restart = builder->use_prim_restart;

#if GWN_TRACK_INDEX_RANGE
	uint range = index_range(builder->data, builder->index_len, &elem->min_index, &elem->max_index);

	/* count the primitive restart index. */
	if (elem->use_prim_restart) {
		range += 1;
	}

	if (range <= 0xFF) {
		elem->index_type = GWN_INDEX_U8;
		squeeze_indices_byte(builder, elem);
	}
	else if (range <= 0xFFFF) {
		elem->index_type = GWN_INDEX_U16;
		squeeze_indices_short(builder, elem);
	}
	else {
		elem->index_type = GWN_INDEX_U32;
		elem->base_index = 0;
	}
	elem->gl_index_type = convert_index_type_to_gl(elem->index_type);
#endif

	if (elem->vbo_id == 0) {
		elem->vbo_id = GWN_buf_id_alloc();
	}
	/* send data to GPU */
	/* GL_ELEMENT_ARRAY_BUFFER changes the state of the last VAO bound,
	 * so we use the GL_ARRAY_BUFFER here to create a buffer without
	 * interfering in the VAO state. */
	glBindBuffer(GL_ARRAY_BUFFER, elem->vbo_id);
	glBufferData(GL_ARRAY_BUFFER, GWN_indexbuf_size_get(elem), builder->data, GL_STATIC_DRAW);

	/* discard builder (one-time use) */
	free(builder->data);
	builder->data = NULL;
	/* other fields are safe to leave */
}

void GWN_indexbuf_use(Gwn_IndexBuf* elem)
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elem->vbo_id);
}

void GWN_indexbuf_discard(Gwn_IndexBuf* elem)
{
	if (elem->vbo_id) {
		GWN_buf_id_free(elem->vbo_id);
	}
	free(elem);
}
