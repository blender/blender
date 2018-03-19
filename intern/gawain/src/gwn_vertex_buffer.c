
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

static GLenum convert_usage_type_to_gl(Gwn_UsageType type)
	{
	static const GLenum table[] = {
		[GWN_USAGE_STREAM] = GL_STREAM_DRAW,
		[GWN_USAGE_STATIC] = GL_STATIC_DRAW,
		[GWN_USAGE_DYNAMIC] = GL_DYNAMIC_DRAW
		};
	return table[type];
	}

Gwn_VertBuf* GWN_vertbuf_create(Gwn_UsageType usage)
	{
	Gwn_VertBuf* verts = malloc(sizeof(Gwn_VertBuf));
	GWN_vertbuf_init(verts, usage);
	return verts;
	}

Gwn_VertBuf* GWN_vertbuf_create_with_format_ex(const Gwn_VertFormat* format, Gwn_UsageType usage)
	{
	Gwn_VertBuf* verts = GWN_vertbuf_create(usage);
	GWN_vertformat_copy(&verts->format, format);
	if (!format->packed)
		VertexFormat_pack(&verts->format);
	return verts;

	// this function might seem redundant, but there is potential for memory savings here...
	// TODO: implement those memory savings
	}

void GWN_vertbuf_init(Gwn_VertBuf* verts, Gwn_UsageType usage)
	{
	memset(verts, 0, sizeof(Gwn_VertBuf));
	verts->usage = usage;
	verts->dirty = true;
	}

void GWN_vertbuf_init_with_format_ex(Gwn_VertBuf* verts, const Gwn_VertFormat* format, Gwn_UsageType usage)
	{
	GWN_vertbuf_init(verts, usage);
	GWN_vertformat_copy(&verts->format, format);
	if (!format->packed)
		VertexFormat_pack(&verts->format);
	}

void GWN_vertbuf_discard(Gwn_VertBuf* verts)
	{
	if (verts->vbo_id)
		{
		GWN_buf_id_free(verts->vbo_id);
#if VRAM_USAGE
		vbo_memory_usage -= GWN_vertbuf_size_get(verts);
#endif
		}

	if (verts->data)
		free(verts->data);

	free(verts);
	}

unsigned GWN_vertbuf_size_get(const Gwn_VertBuf* verts)
	{
	return vertex_buffer_size(&verts->format, verts->vertex_ct);
	}

// create a new allocation, discarding any existing data
void GWN_vertbuf_data_alloc(Gwn_VertBuf* verts, unsigned v_ct)
	{
	Gwn_VertFormat* format = &verts->format;
	if (!format->packed)
		VertexFormat_pack(format);

#if TRUST_NO_ONE
	// catch any unnecessary use
	assert(verts->vertex_ct != v_ct || verts->data == NULL);
#endif

	// only create the buffer the 1st time
	if (verts->vbo_id == 0)
		verts->vbo_id = GWN_buf_id_alloc();

	// discard previous data if any
	if (verts->data)
		free(verts->data);

#if VRAM_USAGE
	vbo_memory_usage -= GWN_vertbuf_size_get(verts);
#endif

	verts->dirty = true;
	verts->vertex_ct = v_ct;
	verts->data = malloc(sizeof(GLubyte) * GWN_vertbuf_size_get(verts));

#if VRAM_USAGE
	vbo_memory_usage += GWN_vertbuf_size_get(verts);
#endif
	}

// resize buffer keeping existing data
void GWN_vertbuf_data_resize(Gwn_VertBuf* verts, unsigned v_ct)
	{
#if TRUST_NO_ONE
	assert(verts->data != NULL);
	assert(verts->vertex_ct != v_ct);
#endif

#if VRAM_USAGE
	vbo_memory_usage -= GWN_vertbuf_size_get(verts);
#endif

	verts->dirty = true;
	verts->vertex_ct = v_ct;
	verts->data = realloc(verts->data, sizeof(GLubyte) * GWN_vertbuf_size_get(verts));

#if VRAM_USAGE
	vbo_memory_usage += GWN_vertbuf_size_get(verts);
#endif
	}

void GWN_vertbuf_attr_set(Gwn_VertBuf* verts, unsigned a_idx, unsigned v_idx, const void* data)
	{
	const Gwn_VertFormat* format = &verts->format;
	const Gwn_VertAttr* a = format->attribs + a_idx;

#if TRUST_NO_ONE
	assert(a_idx < format->attrib_ct);
	assert(v_idx < verts->vertex_ct);
	assert(verts->data != NULL);
#endif

	verts->dirty = true;
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
	assert(verts->data != NULL);
#endif

	verts->dirty = true;
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
	assert(verts->data != NULL);
#endif

	verts->dirty = true;

	access->size = a->sz;
	access->stride = format->stride;
	access->data = (GLubyte*)verts->data + a->offset;
	access->data_init = access->data;
#if TRUST_NO_ONE
	access->_data_end = access->data_init + (size_t)(verts->vertex_ct * format->stride);
#endif
	}

static void VertBuffer_upload_data(Gwn_VertBuf* verts)
	{
	unsigned buffer_sz = GWN_vertbuf_size_get(verts);

	// orphan the vbo to avoid sync
	glBufferData(GL_ARRAY_BUFFER, buffer_sz, NULL, convert_usage_type_to_gl(verts->usage));
	// upload data
	glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_sz, verts->data);

	if (verts->usage == GWN_USAGE_STATIC)
		{
		free(verts->data);
		verts->data = NULL;
		}

	verts->dirty = false;
	}

void GWN_vertbuf_use(Gwn_VertBuf* verts)
	{
	glBindBuffer(GL_ARRAY_BUFFER, verts->vbo_id);

	if (verts->dirty)
		VertBuffer_upload_data(verts);
	}

unsigned GWN_vertbuf_get_memory_usage(void)
	{
	return vbo_memory_usage;
	}
