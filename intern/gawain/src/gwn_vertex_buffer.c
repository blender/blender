
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

#if TRUST_NO_ONE
	assert(verts->vbo_id == 0);
#endif

	unsigned buffer_sz = GWN_vertbuf_size_get(verts);
#if VRAM_USAGE
	vbo_memory_usage += buffer_sz;
#endif

	// create an array buffer and map it to memory
	verts->vbo_id = GWN_buf_id_alloc();
	glBindBuffer(GL_ARRAY_BUFFER, verts->vbo_id);
	glBufferData(GL_ARRAY_BUFFER, buffer_sz, NULL, convert_usage_type_to_gl(verts->usage));
	verts->data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	}

void GWN_vertbuf_data_resize_ex(Gwn_VertBuf* verts, unsigned v_ct, bool keep_data)
	{
#if TRUST_NO_ONE
	assert(verts->vbo_id != 0);
#endif

	if (verts->vertex_ct == v_ct)
		return;

	unsigned old_buf_sz = GWN_vertbuf_size_get(verts);
	verts->vertex_ct = v_ct;
	unsigned new_buf_sz = GWN_vertbuf_size_get(verts);
#if VRAM_USAGE
	vbo_memory_usage += new_buf_sz - old_buf_sz;
#endif

	if (keep_data)
		{
		// we need to do a copy to keep the existing data
		GLuint vbo_tmp;
		glGenBuffers(1, &vbo_tmp);
		// only copy the data that can fit in the new buffer
		unsigned copy_sz = (old_buf_sz < new_buf_sz) ? old_buf_sz : new_buf_sz;
		glBindBuffer(GL_COPY_WRITE_BUFFER, vbo_tmp);
		glBufferData(GL_COPY_WRITE_BUFFER, copy_sz, NULL, GL_STREAM_COPY);

		glBindBuffer(GL_COPY_READ_BUFFER, verts->vbo_id);
		// we cannot copy from/to a mapped buffer
		if (verts->data)
			glUnmapBuffer(GL_COPY_READ_BUFFER);

		// save data, resize the buffer, restore data
		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, copy_sz);
		glBufferData(GL_COPY_READ_BUFFER, new_buf_sz, NULL, convert_usage_type_to_gl(verts->usage));
		glCopyBufferSubData(GL_COPY_WRITE_BUFFER, GL_COPY_READ_BUFFER, 0, 0, copy_sz);

		glDeleteBuffers(1, &vbo_tmp);
		}
	else
		{
		glBindBuffer(GL_COPY_READ_BUFFER, verts->vbo_id);
		glBufferData(GL_COPY_READ_BUFFER, new_buf_sz, NULL, convert_usage_type_to_gl(verts->usage));
		}

	// if the buffer was mapped, update it's pointer
	if (verts->data)
		verts->data = glMapBuffer(GL_COPY_READ_BUFFER, GL_WRITE_ONLY);
	}

static void VertexBuffer_map(Gwn_VertBuf* verts)
	{
	glBindBuffer(GL_ARRAY_BUFFER, verts->vbo_id);
	verts->data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	}

static void VertexBuffer_unmap(Gwn_VertBuf* verts)
	{
	glBindBuffer(GL_ARRAY_BUFFER, verts->vbo_id);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	verts->data = NULL;
	}

void GWN_vertbuf_attr_set(Gwn_VertBuf* verts, unsigned a_idx, unsigned v_idx, const void* data)
	{
	const Gwn_VertFormat* format = &verts->format;
	const Gwn_VertAttr* a = format->attribs + a_idx;

#if TRUST_NO_ONE
	assert(a_idx < format->attrib_ct);
	assert(v_idx < verts->vertex_ct);
#endif

	if (verts->data == NULL)
		VertexBuffer_map(verts);

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
#endif

	const unsigned vertex_ct = verts->vertex_ct;

	if (verts->data == NULL)
		VertexBuffer_map(verts);

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
#endif

	if (verts->data == NULL)
		VertexBuffer_map(verts);

	access->size = a->sz;
	access->stride = format->stride;
	access->data = (GLubyte*)verts->data + a->offset;
	access->data_init = access->data;
#if TRUST_NO_ONE
	access->_data_end = access->data_init + (size_t)(verts->vertex_ct * format->stride);
#endif
	}

void GWN_vertbuf_use(Gwn_VertBuf* verts)
	{
	if (verts->data)
		// this also calls glBindBuffer
		VertexBuffer_unmap(verts);
	else
		glBindBuffer(GL_ARRAY_BUFFER, verts->vbo_id);
	}

unsigned GWN_vertbuf_get_memory_usage(void)
	{
	return vbo_memory_usage;
	}
