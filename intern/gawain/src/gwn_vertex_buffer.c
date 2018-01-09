
// Gawain vertex buffer
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gwn_vertex_buffer.h"
#include "gwn_buffer_id.h"
#include "gwn_vertex_format_private.h"
#include <stdlib.h>
#include <string.h>

#define KEEP_SINGLE_COPY 1

static unsigned vbo_memory_usage;

Gwn_VertBuf* GWN_vertbuf_create(void)
	{
	Gwn_VertBuf* verts = malloc(sizeof(Gwn_VertBuf));
	GWN_vertbuf_init(verts);
	return verts;
	}

Gwn_VertBuf* GWN_vertbuf_create_with_format(const Gwn_VertFormat* format)
	{
	Gwn_VertBuf* verts = GWN_vertbuf_create();
	GWN_vertformat_copy(&verts->format, format);
	if (!format->packed)
		VertexFormat_pack(&verts->format);
	return verts;

	// this function might seem redundant, but there is potential for memory savings here...
	// TODO: implement those memory savings
	}

void GWN_vertbuf_init(Gwn_VertBuf* verts)
	{
	memset(verts, 0, sizeof(Gwn_VertBuf));
	}

void GWN_vertbuf_init_with_format(Gwn_VertBuf* verts, const Gwn_VertFormat* format)
	{
	GWN_vertbuf_init(verts);
	GWN_vertformat_copy(&verts->format, format);
	if (!format->packed)
		VertexFormat_pack(&verts->format);
	}

/**
 * Like #GWN_vertbuf_discard but doesn't free.
 */
void GWN_vertbuf_clear(Gwn_VertBuf* verts)
	{
	if (verts->vbo_id) {
		GWN_buf_id_free(verts->vbo_id);
		vbo_memory_usage -= GWN_vertbuf_size_get(verts);
	}
#if KEEP_SINGLE_COPY
	else
#endif
	if (verts->data && verts->own_data)
		{
		free(verts->data);
		verts->data = NULL;
		}
	}

void GWN_vertbuf_discard(Gwn_VertBuf* verts)
	{
	if (verts->vbo_id)
		{
		GWN_buf_id_free(verts->vbo_id);
		vbo_memory_usage -= GWN_vertbuf_size_get(verts);
		}
#if KEEP_SINGLE_COPY
	else
#endif
	if (verts->data && verts->own_data)
		{
		free(verts->data);
		}


	free(verts);
	}

unsigned GWN_vertbuf_size_get(const Gwn_VertBuf* verts)
	{
	return vertex_buffer_size(&verts->format, verts->vertex_ct);
	}

void GWN_vertbuf_data_alloc(Gwn_VertBuf* verts, unsigned v_ct)
	{
	Gwn_VertFormat* format = &verts->format;
	if (!format->packed)
		VertexFormat_pack(format);

	verts->vertex_ct = v_ct;
	verts->own_data = true;

	// Data initially lives in main memory. Will be transferred to VRAM when we "prime" it.
	verts->data = malloc(GWN_vertbuf_size_get(verts));
	}

void GWN_vertbuf_data_set(Gwn_VertBuf* verts, unsigned v_ct, void* data, bool pass_ownership)
	{
	Gwn_VertFormat* format = &verts->format;
	if (!format->packed)
		VertexFormat_pack(format);

	verts->vertex_ct = v_ct;
	verts->own_data = pass_ownership;

	// Data initially lives in main memory. Will be transferred to VRAM when we "prime" it.
	verts->data = data;
	}

void GWN_vertbuf_data_resize(Gwn_VertBuf* verts, unsigned v_ct)
	{
#if TRUST_NO_ONE
	assert(verts->vertex_ct != v_ct); // allow this?
	assert(verts->data != NULL); // has already been allocated
	assert(verts->vbo_id == 0); // has not been sent to VRAM
#endif

	verts->vertex_ct = v_ct;
	verts->data = realloc(verts->data, GWN_vertbuf_size_get(verts));
	// TODO: skip realloc if v_ct < existing vertex count
	// extra space will be reclaimed, and never sent to VRAM (see VertexBuffer_prime)
	}

void GWN_vertbuf_attr_set(Gwn_VertBuf* verts, unsigned a_idx, unsigned v_idx, const void* data)
	{
	const Gwn_VertFormat* format = &verts->format;
	const Gwn_VertAttr* a = format->attribs + a_idx;

#if TRUST_NO_ONE
	assert(a_idx < format->attrib_ct);
	assert(v_idx < verts->vertex_ct);
	assert(verts->data != NULL); // data must be in main mem
#endif

	memcpy((GLubyte*)verts->data + a->offset + v_idx * format->stride, data, a->sz);
	}

void GWN_vertbuf_attr_fill(Gwn_VertBuf* verts, unsigned a_idx, const void* data)
	{
	const Gwn_VertFormat* format = &verts->format;
	const Gwn_VertAttr* a = format->attribs + a_idx;

#if TRUST_NO_ONE
	assert(a_idx < format->attrib_ct);
#endif

	const unsigned stride = a->sz; // tightly packed input data

	GWN_vertbuf_attr_fill_stride(verts, a_idx, stride, data);
	}

void GWN_vertbuf_attr_fill_stride(Gwn_VertBuf* verts, unsigned a_idx, unsigned stride, const void* data)
	{
	const Gwn_VertFormat* format = &verts->format;
	const Gwn_VertAttr* a = format->attribs + a_idx;

#if TRUST_NO_ONE
	assert(a_idx < format->attrib_ct);
	assert(verts->data != NULL); // data must be in main mem
#endif

	const unsigned vertex_ct = verts->vertex_ct;

	if (format->attrib_ct == 1 && stride == format->stride)
		{
		// we can copy it all at once
		memcpy(verts->data, data, vertex_ct * a->sz);
		}
	else
		{
		// we must copy it per vertex
		for (unsigned v = 0; v < vertex_ct; ++v)
			memcpy((GLubyte*)verts->data + a->offset + v * format->stride, (const GLubyte*)data + v * stride, a->sz);
		}
	}

void GWN_vertbuf_attr_get_raw_data(Gwn_VertBuf* verts, unsigned a_idx, Gwn_VertBufRaw *access)
	{
	const Gwn_VertFormat* format = &verts->format;
	const Gwn_VertAttr* a = format->attribs + a_idx;

#if TRUST_NO_ONE
	assert(a_idx < format->attrib_ct);
	assert(verts->data != NULL); // data must be in main mem
#endif

	access->size = a->sz;
	access->stride = format->stride;
	access->data = (GLubyte*)verts->data + a->offset;
	access->data_init = access->data;
#if TRUST_NO_ONE
	access->_data_end = access->data_init + (size_t)(verts->vertex_ct * format->stride);
#endif
	}


static void VertexBuffer_prime(Gwn_VertBuf* verts)
	{
	const unsigned buffer_sz = GWN_vertbuf_size_get(verts);

	verts->vbo_id = GWN_buf_id_alloc();
	glBindBuffer(GL_ARRAY_BUFFER, verts->vbo_id);
	// fill with delicious data & send to GPU the first time only
	glBufferData(GL_ARRAY_BUFFER, buffer_sz, verts->data, GL_STATIC_DRAW);

	vbo_memory_usage += buffer_sz;

#if KEEP_SINGLE_COPY
	// now that GL has a copy, discard original
	if (verts->own_data)
		{
		free(verts->data);
		verts->data = NULL;
		}
#endif
	}

void GWN_vertbuf_use(Gwn_VertBuf* verts)
	{
	if (verts->vbo_id)
		glBindBuffer(GL_ARRAY_BUFFER, verts->vbo_id);
	else
		VertexBuffer_prime(verts);
	}

unsigned GWN_vertbuf_get_memory_usage(void)
	{
	return vbo_memory_usage;
	}
