
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
#include <stdint.h>
#include <string.h>

#define PACK_DEBUG 0

#if PACK_DEBUG
  #include <stdio.h>
#endif

#define APPLE_LEGACY (defined(__APPLE__) && defined(WITH_GL_PROFILE_COMPAT))

#if APPLE_LEGACY
  #undef glGenVertexArrays
  #define glGenVertexArrays glGenVertexArraysAPPLE

  #undef glDeleteVertexArrays
  #define glDeleteVertexArrays glDeleteVertexArraysAPPLE

  #undef glBindVertexArray
  #define glBindVertexArray glBindVertexArrayAPPLE
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
static void show_pack(unsigned a_idx, unsigned sz, unsigned pad)
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
		const Attrib* a = format->attribs + a_idx;
		glBindAttribLocation(program, a_idx, a->name);
		}
	}

typedef struct {
	uint64_t loc_bits; // store 4 bits for each of the 16 attribs
	uint16_t enabled_bits; // 1 bit for each attrib
} AttribBinding;

void clear_AttribBinding(AttribBinding* binding)
	{
	binding->loc_bits = 0;
	binding->enabled_bits = 0;
	}

unsigned read_attrib_location(const AttribBinding* binding, unsigned a_idx)
	{
#if TRUST_NO_ONE
	assert(MAX_VERTEX_ATTRIBS == 16);
	assert(a_idx < MAX_VERTEX_ATTRIBS);
	assert(binding->enabled_bits & (1 << a_idx));
#endif

	return (binding->loc_bits >> (4 * a_idx)) & 0xF;
	}

void write_attrib_location(AttribBinding* binding, unsigned a_idx, unsigned location)
	{
#if TRUST_NO_ONE
	assert(MAX_VERTEX_ATTRIBS == 16);
	assert(a_idx < MAX_VERTEX_ATTRIBS);
	assert(location < MAX_VERTEX_ATTRIBS);
#endif

	const unsigned shift = 4 * a_idx;
	const uint64_t mask = ((uint64_t)0xF) << shift;
	// overwrite this attrib's previous location
	binding->loc_bits = (binding->loc_bits & ~mask) | (location << shift);
	// mark this attrib as enabled
	binding->enabled_bits |= 1 << a_idx;
	}

void get_attrib_locations(const VertexFormat* format, AttribBinding* binding, GLuint program)
	{
#if TRUST_NO_ONE
	assert(glIsProgram(program));
#endif

	clear_AttribBinding(binding);

	for (unsigned a_idx = 0; a_idx < format->attrib_ct; ++a_idx)
		{
		const Attrib* a = format->attribs + a_idx;
		GLint loc = glGetAttribLocation(program, a->name);

#if TRUST_NO_ONE
		assert(loc != -1);
#endif

		write_attrib_location(binding, a_idx, loc);
		}
	}

// --- immediate mode work-alike --------------------------------

typedef struct {
	// TODO: organize this struct by frequency of change (run-time)

	// current draw call
	GLubyte* buffer_data;
	unsigned buffer_offset;
	unsigned buffer_bytes_mapped;
	unsigned vertex_ct;
	GLenum primitive;

	VertexFormat vertex_format;

	// current vertex
	unsigned vertex_idx;
	GLubyte* vertex_data;
	unsigned short attrib_value_bits; // which attributes of current vertex have been given values?

	GLuint vbo_id;
	GLuint vao_id;
	
	GLuint bound_program;
	AttribBinding attrib_binding;
	uint16_t prev_enabled_attrib_bits;
} Immediate;

// size of internal buffer -- make this adjustable?
#define IMM_BUFFER_SIZE (4 * 1024 * 1024)

static PER_THREAD bool initialized = false;
static PER_THREAD Immediate imm;

void immInit()
	{
#if TRUST_NO_ONE
	assert(!initialized);
#endif

	memset(&imm, 0, sizeof(Immediate));

	glGenVertexArrays(1, &imm.vao_id);
	glBindVertexArray(imm.vao_id);
	glGenBuffers(1, &imm.vbo_id);
	glBindBuffer(GL_ARRAY_BUFFER, imm.vbo_id);
	glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);

#if APPLE_LEGACY
	glBufferParameteriAPPLE(GL_ARRAY_BUFFER, GL_BUFFER_SERIALIZED_MODIFY_APPLE, GL_FALSE);
	glBufferParameteriAPPLE(GL_ARRAY_BUFFER, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
#endif

	imm.primitive = GL_NONE;
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	initialized = true;
	}

void immDestroy()
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.primitive == GL_NONE); // make sure we're not between a Begin/End pair
#endif

	clear_VertexFormat(&imm.vertex_format);
	glDeleteVertexArrays(1, &imm.vao_id);
	glDeleteBuffers(1, &imm.vbo_id);
	initialized = false;
	}

VertexFormat* immVertexFormat()
	{
	clear_VertexFormat(&imm.vertex_format);
	return &imm.vertex_format;
	}

void immBindProgram(GLuint program)
	{
#if TRUST_NO_ONE
	assert(imm.bound_program == 0);
#endif

	if (!imm.vertex_format.packed)
		pack(&imm.vertex_format);

	glUseProgram(program);
	get_attrib_locations(&imm.vertex_format, &imm.attrib_binding, program);
	imm.bound_program = program;
	}

void immUnbindProgram()
	{
#if TRUST_NO_ONE
	assert(imm.bound_program != 0);
#endif

	glUseProgram(0);
	imm.bound_program = 0;
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
  #ifdef WITH_GL_PROFILE_COMPAT
		case GL_QUADS:
			assert(vertex_ct % 4 == 0);
			break;
  #endif
		default:
			assert(false);
		}
#endif

	imm.primitive = primitive;
	imm.vertex_ct = vertex_ct;
	imm.vertex_idx = 0;
	imm.attrib_value_bits = 0;

	// how many bytes do we need for this draw call?
	const unsigned bytes_needed = vertex_buffer_size(&imm.vertex_format, vertex_ct);

#if TRUST_NO_ONE
	assert(bytes_needed <= IMM_BUFFER_SIZE);
#endif

	glBindBuffer(GL_ARRAY_BUFFER, imm.vbo_id);

	// does the current buffer have enough room?
	const unsigned available_bytes = IMM_BUFFER_SIZE - imm.buffer_offset;
	// ensure vertex data is aligned
	const unsigned pre_padding = padding(imm.buffer_offset, imm.vertex_format.stride); // might waste a little space, but it's safe
	if ((bytes_needed + pre_padding) <= available_bytes)
		imm.buffer_offset += pre_padding;
	else
		{
		// orphan this buffer & start with a fresh one
#if APPLE_LEGACY
		glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);
#else
		if (GLEW_VERSION_4_3 || GLEW_ARB_invalidate_subdata)
			glInvalidateBufferData(imm.vbo_id);
		else
			glMapBufferRange(GL_ARRAY_BUFFER, 0, IMM_BUFFER_SIZE, GL_MAP_INVALIDATE_BUFFER_BIT);
#endif

		imm.buffer_offset = 0;
		}

//	printf("mapping %u to %u\n", imm.buffer_offset, imm.buffer_offset + bytes_needed - 1);

#if APPLE_LEGACY
	imm.buffer_data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY) + imm.buffer_offset;
#else
	imm.buffer_data = glMapBufferRange(GL_ARRAY_BUFFER, imm.buffer_offset, bytes_needed, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
#endif

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

#if APPLE_LEGACY
	// tell OpenGL what range was modified so it doesn't copy the whole buffer
	glFlushMappedBufferRangeAPPLE(GL_ARRAY_BUFFER, imm.buffer_offset, imm.buffer_bytes_mapped);
//	printf("flushing %u to %u\n", imm.buffer_offset, imm.buffer_offset + imm.buffer_bytes_mapped - 1);
#endif
	glUnmapBuffer(GL_ARRAY_BUFFER);

	// set up VAO -- can be done during Begin or End really
	glBindVertexArray(imm.vao_id);

	// enable/disable vertex attribs as needed
	if (imm.attrib_binding.enabled_bits != imm.prev_enabled_attrib_bits)
		{
		for (unsigned loc = 0; loc < MAX_VERTEX_ATTRIBS; ++loc)
			{
			bool is_enabled = imm.attrib_binding.enabled_bits & (1 << loc);
			bool was_enabled = imm.prev_enabled_attrib_bits & (1 << loc);

			if (is_enabled && !was_enabled)
				{
//				printf("enabling attrib %u\n", loc);
				glEnableVertexAttribArray(loc);
				}
			else if (was_enabled && !is_enabled)
				{
//				printf("disabling attrib %u\n", loc);
				glDisableVertexAttribArray(loc);
				}
			}

		imm.prev_enabled_attrib_bits = imm.attrib_binding.enabled_bits;
		}

	const unsigned stride = imm.vertex_format.stride;

	for (unsigned a_idx = 0; a_idx < imm.vertex_format.attrib_ct; ++a_idx)
		{
		const Attrib* a = imm.vertex_format.attribs + a_idx;

		const unsigned offset = imm.buffer_offset + a->offset;
		const GLvoid* pointer = (const GLubyte*)0 + offset;

		const unsigned loc = read_attrib_location(&imm.attrib_binding, a_idx);

//		printf("specifying attrib %u '%s' with offset %u, stride %u\n", loc, a->name, offset, stride);

		switch (a->fetch_mode)
			{
			case KEEP_FLOAT:
			case CONVERT_INT_TO_FLOAT:
				glVertexAttribPointer(loc, a->comp_ct, a->comp_type, GL_FALSE, stride, pointer);
				break;
			case NORMALIZE_INT_TO_FLOAT:
				glVertexAttribPointer(loc, a->comp_ct, a->comp_type, GL_TRUE, stride, pointer);
				break;
			case KEEP_INT:
				glVertexAttribIPointer(loc, a->comp_ct, a->comp_type, stride, pointer);
			}
		}

	glDrawArrays(imm.primitive, 0, imm.vertex_ct);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

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
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 1);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = (float*)(imm.vertex_data + attrib->offset);
//	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data);

	data[0] = x;
	}

void immAttrib2f(unsigned attrib_id, float x, float y)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 2);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = (float*)(imm.vertex_data + attrib->offset);
//	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data);

	data[0] = x;
	data[1] = y;
	}

void immAttrib3f(unsigned attrib_id, float x, float y, float z)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 3);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = (float*)(imm.vertex_data + attrib->offset);
//	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data);

	data[0] = x;
	data[1] = y;
	data[2] = z;
	}

void immAttrib3ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GL_UNSIGNED_BYTE);
	assert(attrib->comp_ct == 3);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	GLubyte* data = imm.vertex_data + attrib->offset;
//	printf("%s %td %p\n", __FUNCTION__, data - imm.buffer_data, data);

	data[0] = r;
	data[1] = g;
	data[2] = b;
	}

void immAttrib4ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GL_UNSIGNED_BYTE);
	assert(attrib->comp_ct == 4);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	GLubyte* data = imm.vertex_data + attrib->offset;
//	printf("%s %td %p\n", __FUNCTION__, data - imm.buffer_data, data);

	data[0] = r;
	data[1] = g;
	data[2] = b;
	data[3] = a;
	}

void immEndVertex()
	{
#if TRUST_NO_ONE
	assert(imm.primitive != GL_NONE); // make sure we're between a Begin/End pair
	assert(imm.vertex_idx < imm.vertex_ct);
#endif

	// have all attribs been assigned values?
	// if not, copy value from previous vertex
	const unsigned short all_bits = ~(0xFFFFU << imm.vertex_format.attrib_ct);
	if (imm.attrib_value_bits != all_bits)
		{
#if TRUST_NO_ONE
		assert(imm.vertex_idx > 0); // first vertex must have all attribs specified
#endif

		for (unsigned a_idx = 0; a_idx < imm.vertex_format.attrib_ct; ++a_idx)
			{
			const uint16_t mask = 1 << a_idx;
			if ((imm.attrib_value_bits & mask) == 0)
				{
				const Attrib* a = imm.vertex_format.attribs + a_idx;

//				printf("copying %s from vertex %u to %u\n", a->name, imm.vertex_idx - 1, imm.vertex_idx);

				GLubyte* data = imm.vertex_data + a->offset;
				memcpy(data, data - imm.vertex_format.stride, a->sz);
				}
			}
		}
	
	imm.vertex_idx++;
	imm.vertex_data += imm.vertex_format.stride;
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

void immUniform4f(const char* name, float x, float y, float z, float w)
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform4f(loc, x, y, z, w);
	}
