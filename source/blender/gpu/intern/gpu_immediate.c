
// Gawain immediate mode work-alike, take 2
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "GPU_immediate.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define PACK_DEBUG 0

#if PACK_DEBUG
  #include <stdio.h>
#endif

void clear_VertexFormat(VertexFormat* format)
	{
	for (unsigned a = 0; a < format->attrib_ct; ++a)
		free(format->attribs[a].name);

#if TRUST_NO_ONE
	memset(format, 0, sizeof(VertexFormat));
#else
	format->attrib_ct = 0;
	format->packed = false;
#endif
	}

static unsigned comp_sz(GLenum type)
	{
#if TRUST_NO_ONE
	assert(type >= GL_BYTE && type <= GL_FLOAT);
#endif

	const GLubyte sizes[] = {1,1,2,2,4,4,4};
	return sizes[type - GL_BYTE];
	}

static unsigned attrib_sz(const Attrib *a)
	{
	return a->comp_ct * comp_sz(a->comp_type);
	}

static unsigned attrib_align(const Attrib *a)
	{
	unsigned c = comp_sz(a->comp_type);
	if (a->comp_ct == 3 && c <= 2)
		return 4 * c; // AMD HW can't fetch these well, so pad it out (other vendors too?)
	else
		return c; // most fetches are ok if components are naturally aligned
	}

static unsigned vertex_buffer_size(const VertexFormat* format, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(format->packed && format->stride > 0);
#endif

	return format->stride * vertex_ct;
	}

unsigned add_attrib(VertexFormat* format, const char* name, GLenum comp_type, unsigned comp_ct, VertexFetchMode fetch_mode)
	{
#if TRUST_NO_ONE
	assert(format->attrib_ct < MAX_VERTEX_ATTRIBS); // there's room for more
	assert(!format->packed); // packed means frozen/locked
#endif

	const unsigned attrib_id = format->attrib_ct++;
	Attrib* attrib = format->attribs + attrib_id;

	attrib->name = strdup(name);
	attrib->comp_type = comp_type;
	attrib->comp_ct = comp_ct;
	attrib->sz = attrib_sz(attrib);
	attrib->offset = 0; // offsets & stride are calculated later (during pack)
	attrib->fetch_mode = fetch_mode;

	return attrib_id;
	}

static unsigned padding(unsigned offset, unsigned alignment)
	{
	const unsigned mod = offset % alignment;
	return (mod == 0) ? 0 : (alignment - mod);
	}

#if PACK_DEBUG
void show_pack(a_idx, sz, pad)
	{
	const char c = 'A' + a_idx;
	for (unsigned i = 0; i < pad; ++i)
		putchar('-');
	for (unsigned i = 0; i < sz; ++i)
		putchar(c);
	}
#endif

void pack(VertexFormat* format)
	{
	// for now, attributes are packed in the order they were added,
	// making sure each attrib is naturally aligned (add padding where necessary)

	// later we can implement more efficient packing w/ reordering

	Attrib* a0 = format->attribs + 0;
	a0->offset = 0;
	unsigned offset = a0->sz;

#if PACK_DEBUG
	show_pack(0, a0->sz, 0);
#endif

	for (unsigned a_idx = 1; a_idx < format->attrib_ct; ++a_idx)
		{
		Attrib* a = format->attribs + a_idx;
		unsigned mid_padding = padding(offset, attrib_align(a));
		offset += mid_padding;
		a->offset = offset;
		offset += a->sz;

#if PACK_DEBUG
		show_pack(a_idx, a->sz, mid_padding);
#endif
		}

	unsigned end_padding = padding(offset, attrib_align(a0));

#if PACK_DEBUG
	show_pack(0, 0, end_padding);
	putchar('\n');
#endif

	format->stride = offset + end_padding;
	format->packed = true;
	}

void bind_attrib_locations(const VertexFormat* format, GLuint program)
	{
#if TRUST_NO_ONE
	assert(glIsProgram(program));
#endif

	for (unsigned a_idx = 0; a_idx < format->attrib_ct; ++a_idx)
		{
		const Attrib* a = &format->attribs[a_idx];
		glBindAttribLocation(program, a_idx, a->name);
		}
	}

// --- immediate mode work-alike --------------------------------

typedef struct {
	// current draw call
	void* buffer_data;
	unsigned buffer_offset;
	unsigned buffer_bytes_mapped;
	unsigned vertex_ct;
	GLenum primitive;

	// current vertex
	unsigned vertex_idx;
	void* vertex_data;
	unsigned short attrib_value_bits; // which attributes of current vertex have been given values?

	GLuint vbo_id;
	GLuint vao_id;
} Immediate;

// size of internal buffer -- make this adjustable?
// #define IMM_BUFFER_SIZE (4 * 1024 * 1024)
#define IMM_BUFFER_SIZE 1024

static PER_THREAD bool initialized = false;
static PER_THREAD Immediate imm;
PER_THREAD VertexFormat immVertexFormat;

void immInit()
	{
#if TRUST_NO_ONE
	assert(!initialized);
#endif

	clear_VertexFormat(&immVertexFormat);
	memset(&imm, 0, sizeof(Immediate));

	glGenVertexArrays(1, &imm.vao_id);
	glBindVertexArray(imm.vao_id);
	glGenBuffers(1, &imm.vbo_id);
	glBindBuffer(GL_ARRAY_BUFFER, imm.vbo_id);
	glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);

	imm.primitive = GL_NONE;
	
	// glBindBuffer(GL_ARRAY_BUFFER, 0);
	initialized = true;
	}

void immDestroy()
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.primitive == GL_NONE); // make sure we're not between a Begin/End pair
#endif

	clear_VertexFormat(&immVertexFormat);
	glDeleteVertexArrays(1, &imm.vao_id);
	glDeleteBuffers(1, &imm.vbo_id);
	initialized = false;
	}

void immBegin(GLenum primitive, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.primitive == GL_NONE); // make sure we haven't already begun

	// does vertex_ct make sense for this primitive type?
	assert(vertex_ct > 0);
	switch (primitive)
		{
		case GL_POINTS:
			break;
		case GL_LINES:
			assert(vertex_ct % 2 == 0);
			break;
		case GL_LINE_STRIP:
		case GL_LINE_LOOP:
			assert(vertex_ct > 2); // otherwise why bother?
			break;
		case GL_TRIANGLES:
			assert(vertex_ct % 3 == 0);
			break;
		default:
			assert(false);
		}
#endif

	imm.primitive = primitive;
	imm.vertex_ct = vertex_ct;
	imm.vertex_idx = 0;
	imm.attrib_value_bits = 0;

	// how many bytes do we need for this draw call?
	const unsigned bytes_needed = vertex_buffer_size(&immVertexFormat, vertex_ct);

#if TRUST_NO_ONE
	assert(bytes_needed <= IMM_BUFFER_SIZE);
#endif

//	glBindBuffer(GL_ARRAY_BUFFER, imm.vbo_id);

	// does the current buffer have enough room?
	const unsigned available_bytes = IMM_BUFFER_SIZE - imm.buffer_offset;
	// ensure vertex data is aligned
	const unsigned pre_padding = padding(imm.buffer_offset, immVertexFormat.stride); // might waste a little space, but it's safe
	if ((bytes_needed + pre_padding) <= available_bytes)
		imm.buffer_offset += pre_padding;
	else
		{
		// orphan this buffer & start with a fresh one
		glMapBufferRange(GL_ARRAY_BUFFER, 0, IMM_BUFFER_SIZE, GL_MAP_INVALIDATE_BUFFER_BIT);
		// glInvalidateBufferData(imm.vbo_id); // VERSION >= 4.3 || ARB_invalidate_subdata

		imm.buffer_offset = 0;
		}

//	printf("mapping %u to %u\n", imm.buffer_offset, imm.buffer_offset + bytes_needed - 1);

	imm.buffer_data = glMapBufferRange(GL_ARRAY_BUFFER, imm.buffer_offset, bytes_needed, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

#if TRUST_NO_ONE
	assert(imm.buffer_data != NULL);
#endif

	imm.buffer_bytes_mapped = bytes_needed;
	imm.vertex_data = imm.buffer_data;
	}

void immEnd()
	{
#if TRUST_NO_ONE
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
	assert(imm.vertex_idx == imm.vertex_ct); // with all vertices defined
#endif

	glUnmapBuffer(GL_ARRAY_BUFFER);

	// set up VAO -- can be done during Begin or End really
//	glBindVertexArray(imm.vao_id);

	const unsigned stride = immVertexFormat.stride;

	for (unsigned a_idx = 0; a_idx < immVertexFormat.attrib_ct; ++a_idx)
		{
		const Attrib* a = immVertexFormat.attribs + a_idx;

		const unsigned offset = imm.buffer_offset + a->offset;
		const GLvoid* pointer = (const GLvoid*)0 + offset;

//		printf("enabling attrib %u '%s' at offset %u, stride %u\n", a_idx, a->name, offset, stride);
		glEnableVertexAttribArray(a_idx);

		switch (a->fetch_mode)
			{
			case KEEP_FLOAT:
			case CONVERT_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_FALSE, stride, pointer);
				break;
			case NORMALIZE_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_TRUE, stride, pointer);
				break;
			case KEEP_INT:
				glVertexAttribIPointer(a_idx, a->comp_ct, a->comp_type, stride, pointer);
			}
		}

	for (unsigned a_idx = immVertexFormat.attrib_ct; a_idx < MAX_VERTEX_ATTRIBS; ++a_idx)
		{
//		printf("disabling attrib %u\n", a_idx);
		glDisableVertexAttribArray(a_idx);
		// TODO: compare with previous draw's attrib_ct
		// will always need to update pointers, but can reduce Enable/Disable calls
		}

	glDrawArrays(imm.primitive, 0, imm.vertex_ct);

//	glBindVertexArray(0);

	// prep for next immBegin
	imm.buffer_offset += imm.buffer_bytes_mapped;
	imm.primitive = GL_NONE;

	// further optional cleanup
	imm.buffer_bytes_mapped = 0;
	imm.buffer_data = NULL;
	imm.vertex_data = NULL;
	}

static void setAttribValueBit(unsigned attrib_id)
	{
	unsigned short mask = 1 << attrib_id;

#if TRUST_NO_ONE
	assert((imm.attrib_value_bits & mask) == 0); // not already set
#endif

	imm.attrib_value_bits |= mask;
	}

void immAttrib1f(unsigned attrib_id, float x)
	{
	Attrib* attrib = immVertexFormat.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < immVertexFormat.attrib_ct);
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 1);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = imm.vertex_data + attrib->offset;
//	printf("%s %ld %p\n", __FUNCTION__, (void*)data - imm.buffer_data, data);

	data[0] = x;
	}

void immAttrib2f(unsigned attrib_id, float x, float y)
	{
	Attrib* attrib = immVertexFormat.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < immVertexFormat.attrib_ct);
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 2);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = imm.vertex_data + attrib->offset;
//	printf("%s %ld %p\n", __FUNCTION__, (void*)data - imm.buffer_data, data);

	data[0] = x;
	data[1] = y;
	}

void immAttrib3f(unsigned attrib_id, float x, float y, float z)
	{
	Attrib* attrib = immVertexFormat.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < immVertexFormat.attrib_ct);
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 3);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = imm.vertex_data + attrib->offset;
//	printf("%s %ld %p\n", __FUNCTION__, (void*)data - imm.buffer_data, data);

	data[0] = x;
	data[1] = y;
	data[2] = z;
	}

void immAttrib3ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b)
	{
	Attrib* attrib = immVertexFormat.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < immVertexFormat.attrib_ct);
	assert(attrib->comp_type == GL_UNSIGNED_BYTE);
	assert(attrib->comp_ct == 3);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	unsigned char* data = imm.vertex_data + attrib->offset;
//	printf("%s %ld %p\n", __FUNCTION__, (void*)data - imm.buffer_data, data);

	data[0] = r;
	data[1] = g;
	data[2] = b;
	}

void immAttrib4ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
	Attrib* attrib = immVertexFormat.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < immVertexFormat.attrib_ct);
	assert(attrib->comp_type == GL_UNSIGNED_BYTE);
	assert(attrib->comp_ct == 4);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	unsigned char* data = imm.vertex_data + attrib->offset;
//	printf("%s %ld %p\n", __FUNCTION__, (void*)data - imm.buffer_data, data);

	data[0] = r;
	data[1] = g;
	data[2] = b;
	data[3] = a;
	}

void immEndVertex()
	{
#if TRUST_NO_ONE
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair

	// have all attribs been assigned values?
	const unsigned short all_bits = ~(0xFFFFU << immVertexFormat.attrib_ct);
	assert(imm.attrib_value_bits == all_bits);
	
	assert(imm.vertex_idx < imm.vertex_ct);
#endif

	imm.vertex_idx++;
	imm.vertex_data += immVertexFormat.stride;
	imm.attrib_value_bits = 0;
	}

void immVertex2f(unsigned attrib_id, float x, float y)
	{
	immAttrib2f(attrib_id, x, y);
	immEndVertex();
	}

void immVertex3f(unsigned attrib_id, float x, float y, float z)
	{
	immAttrib3f(attrib_id, x, y, z);
	immEndVertex();
	}
