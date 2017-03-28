
// Gawain immediate mode work-alike
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "immediate.h"
#include "attrib_binding.h"
#include "buffer_id.h"
#include <string.h>

// necessary functions from matrix API
extern void gpuBindMatrices(GLuint program);
extern bool gpuMatricesDirty(void);

typedef struct {
	// TODO: organize this struct by frequency of change (run-time)

#if IMM_BATCH_COMBO
	Batch* batch;
#endif

	// current draw call
	GLubyte* buffer_data;
	unsigned buffer_offset;
	unsigned buffer_bytes_mapped;
	unsigned vertex_ct;
	bool strict_vertex_ct;
	PrimitiveType prim_type;

	VertexFormat vertex_format;

	// current vertex
	unsigned vertex_idx;
	GLubyte* vertex_data;
	uint16_t unassigned_attrib_bits; // which attributes of current vertex have not been given values?

	GLuint vbo_id;
	GLuint vao_id;
	
	GLuint bound_program;
	AttribBinding attrib_binding;
	uint16_t prev_enabled_attrib_bits; // <-- only affects this VAO, so we're ok
} Immediate;

// size of internal buffer -- make this adjustable?
#define IMM_BUFFER_SIZE (4 * 1024 * 1024)

static bool initialized = false;
static Immediate imm;

void immInit(void)
	{
#if TRUST_NO_ONE
	assert(!initialized);
#endif

	memset(&imm, 0, sizeof(Immediate));

	imm.vbo_id = buffer_id_alloc();
	glBindBuffer(GL_ARRAY_BUFFER, imm.vbo_id);
	glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);

#if APPLE_LEGACY
	glBufferParameteriAPPLE(GL_ARRAY_BUFFER, GL_BUFFER_SERIALIZED_MODIFY_APPLE, GL_FALSE);
	glBufferParameteriAPPLE(GL_ARRAY_BUFFER, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
#endif

	imm.prim_type = PRIM_NONE;
	imm.strict_vertex_ct = true;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	initialized = true;

	immActivate();
	}

void immActivate(void)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == PRIM_NONE); // make sure we're not between a Begin/End pair
	assert(imm.vao_id == 0);
#endif

	imm.vao_id = vao_id_alloc();
	}

void immDeactivate(void)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == PRIM_NONE); // make sure we're not between a Begin/End pair
	assert(imm.vao_id != 0);
#endif

	vao_id_free(imm.vao_id);
	imm.vao_id = 0;
	imm.prev_enabled_attrib_bits = 0;
	}

void immDestroy(void)
	{
	immDeactivate();
	buffer_id_free(imm.vbo_id);
	initialized = false;
	}

VertexFormat* immVertexFormat(void)
	{
	VertexFormat_clear(&imm.vertex_format);
	return &imm.vertex_format;
	}

void immBindProgram(GLuint program)
	{
#if TRUST_NO_ONE
	assert(imm.bound_program == 0);
	assert(glIsProgram(program));
#endif

	if (!imm.vertex_format.packed)
		VertexFormat_pack(&imm.vertex_format);

	glUseProgram(program);
	get_attrib_locations(&imm.vertex_format, &imm.attrib_binding, program);
	imm.bound_program = program;

	gpuBindMatrices(program);
	}

void immUnbindProgram(void)
	{
#if TRUST_NO_ONE
	assert(imm.bound_program != 0);
#endif

	glUseProgram(0);
	imm.bound_program = 0;
	}

#if TRUST_NO_ONE
static bool vertex_count_makes_sense_for_primitive(unsigned vertex_ct, PrimitiveType prim_type)
	{
	// does vertex_ct make sense for this primitive type?
	if (vertex_ct == 0)
		return false;

	switch (prim_type)
		{
		case PRIM_POINTS:
			return true;
		case PRIM_LINES:
			return vertex_ct % 2 == 0;
		case PRIM_LINE_STRIP:
		case PRIM_LINE_LOOP:
			return vertex_ct >= 2;
		case PRIM_LINE_STRIP_ADJACENCY:
			return vertex_ct >= 4;
		case PRIM_TRIANGLES:
			return vertex_ct % 3 == 0;
		case PRIM_TRIANGLE_STRIP:
		case PRIM_TRIANGLE_FAN:
			return vertex_ct >= 3;
  #ifdef WITH_GL_PROFILE_COMPAT
		case PRIM_QUADS:
			return vertex_ct % 4 == 0;
  #endif
		default:
			return false;
		}
	}
#endif

void immBegin(PrimitiveType prim_type, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == PRIM_NONE); // make sure we haven't already begun
	assert(vertex_count_makes_sense_for_primitive(vertex_ct, prim_type));
#endif

	imm.prim_type = prim_type;
	imm.vertex_ct = vertex_ct;
	imm.vertex_idx = 0;
	imm.unassigned_attrib_bits = imm.attrib_binding.enabled_bits;

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
	imm.buffer_data = glMapBufferRange(GL_ARRAY_BUFFER, imm.buffer_offset, bytes_needed,
	                                   GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | (imm.strict_vertex_ct ? 0 : GL_MAP_FLUSH_EXPLICIT_BIT));
#endif

#if TRUST_NO_ONE
	assert(imm.buffer_data != NULL);
#endif

	imm.buffer_bytes_mapped = bytes_needed;
	imm.vertex_data = imm.buffer_data;
	}

void immBeginAtMost(PrimitiveType prim_type, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(vertex_ct > 0);
#endif

	imm.strict_vertex_ct = false;
	immBegin(prim_type, vertex_ct);
	}

#if IMM_BATCH_COMBO

Batch* immBeginBatch(PrimitiveType prim_type, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == PRIM_NONE); // make sure we haven't already begun
	assert(vertex_count_makes_sense_for_primitive(vertex_ct, prim_type));
#endif

	imm.prim_type = prim_type;
	imm.vertex_ct = vertex_ct;
	imm.vertex_idx = 0;
	imm.unassigned_attrib_bits = imm.attrib_binding.enabled_bits;

	VertexBuffer* verts = VertexBuffer_create_with_format(&imm.vertex_format);
	VertexBuffer_allocate_data(verts, vertex_ct);

	imm.buffer_bytes_mapped = VertexBuffer_size(verts);
	imm.vertex_data = verts->data;

	imm.batch = Batch_create(prim_type, verts, NULL);
	imm.batch->phase = BUILDING;

	Batch_set_program(imm.batch, imm.bound_program);

	return imm.batch;
	}

Batch* immBeginBatchAtMost(PrimitiveType prim_type, unsigned vertex_ct)
	{
	imm.strict_vertex_ct = false;
	return immBeginBatch(prim_type, vertex_ct);
	}

#endif // IMM_BATCH_COMBO

static void immDrawSetup(void)
	{
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

	if (gpuMatricesDirty())
		gpuBindMatrices(imm.bound_program);
	}

void immEnd(void)
	{
#if TRUST_NO_ONE
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	unsigned buffer_bytes_used;
	if (imm.strict_vertex_ct)
		{
#if TRUST_NO_ONE
		assert(imm.vertex_idx == imm.vertex_ct); // with all vertices defined
#endif
		buffer_bytes_used = imm.buffer_bytes_mapped;
		}
	else
		{
#if TRUST_NO_ONE
		assert(imm.vertex_idx <= imm.vertex_ct);
#endif
		// printf("used %u of %u verts,", imm.vertex_idx, imm.vertex_ct);
		if (imm.vertex_idx == imm.vertex_ct)
			{
			buffer_bytes_used = imm.buffer_bytes_mapped;
			}
		else
			{
#if TRUST_NO_ONE
			assert(imm.vertex_idx == 0 || vertex_count_makes_sense_for_primitive(imm.vertex_idx, imm.prim_type));
#endif
			imm.vertex_ct = imm.vertex_idx;
			buffer_bytes_used = vertex_buffer_size(&imm.vertex_format, imm.vertex_ct);
			// unused buffer bytes are available to the next immBegin
			// printf(" %u of %u bytes\n", buffer_bytes_used, imm.buffer_bytes_mapped);
			}
#if !APPLE_LEGACY
		// tell OpenGL what range was modified so it doesn't copy the whole mapped range
		// printf("flushing %u to %u\n", imm.buffer_offset, imm.buffer_offset + buffer_bytes_used - 1);
		glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, buffer_bytes_used);
#endif
		}

#if IMM_BATCH_COMBO
	if (imm.batch)
		{
		if (buffer_bytes_used != imm.buffer_bytes_mapped)
			{
			VertexBuffer_resize_data(imm.batch->verts[0], imm.vertex_ct);
			// TODO: resize only if vertex count is much smaller
			}

		imm.batch->phase = READY_TO_DRAW;
		imm.batch = NULL; // don't free, batch belongs to caller
		}
	else
#endif
		{
#if APPLE_LEGACY
		// tell OpenGL what range was modified so it doesn't copy the whole buffer
		// printf("flushing %u to %u\n", imm.buffer_offset, imm.buffer_offset + buffer_bytes_used - 1);
		glFlushMappedBufferRangeAPPLE(GL_ARRAY_BUFFER, imm.buffer_offset, buffer_bytes_used);
#endif
		glUnmapBuffer(GL_ARRAY_BUFFER);

		if (imm.vertex_ct > 0)
			{
			immDrawSetup();
			glDrawArrays(imm.prim_type, 0, imm.vertex_ct);
			}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// prep for next immBegin
		imm.buffer_offset += buffer_bytes_used;
		}

	// prep for next immBegin
	imm.prim_type = PRIM_NONE;
	imm.strict_vertex_ct = true;
	}

static void setAttribValueBit(unsigned attrib_id)
	{
	uint16_t mask = 1 << attrib_id;

#if TRUST_NO_ONE
	assert(imm.unassigned_attrib_bits & mask); // not already set
#endif

	imm.unassigned_attrib_bits &= ~mask;
	}


// --- generic attribute functions ---

void immAttrib1f(unsigned attrib_id, float x)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == COMP_F32);
	assert(attrib->comp_ct == 1);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
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
	assert(attrib->comp_type == COMP_F32);
	assert(attrib->comp_ct == 2);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
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
	assert(attrib->comp_type == COMP_F32);
	assert(attrib->comp_ct == 3);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = (float*)(imm.vertex_data + attrib->offset);
//	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data);

	data[0] = x;
	data[1] = y;
	data[2] = z;
	}

void immAttrib4f(unsigned attrib_id, float x, float y, float z, float w)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == COMP_F32);
	assert(attrib->comp_ct == 4);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = (float*)(imm.vertex_data + attrib->offset);
//	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data);

	data[0] = x;
	data[1] = y;
	data[2] = z;
	data[3] = w;
	}

void immAttrib2i(unsigned attrib_id, int x, int y)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == COMP_I32);
	assert(attrib->comp_ct == 2);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	int* data = (int*)(imm.vertex_data + attrib->offset);

	data[0] = x;
	data[1] = y;
	}

void immAttrib2s(unsigned attrib_id, short x, short y)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == COMP_I16);
	assert(attrib->comp_ct == 2);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	short* data = (short*)(imm.vertex_data + attrib->offset);

	data[0] = x;
	data[1] = y;
	}

void immAttrib3fv(unsigned attrib_id, const float data[3])
	{
	immAttrib3f(attrib_id, data[0], data[1], data[2]);
	}

void immAttrib4fv(unsigned attrib_id, const float data[4])
	{
	immAttrib4f(attrib_id, data[0], data[1], data[2], data[3]);
	}

void immAttrib3ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b)
	{
	Attrib* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == COMP_U8);
	assert(attrib->comp_ct == 3);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
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
	assert(attrib->comp_type == COMP_U8);
	assert(attrib->comp_ct == 4);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	GLubyte* data = imm.vertex_data + attrib->offset;
//	printf("%s %td %p\n", __FUNCTION__, data - imm.buffer_data, data);

	data[0] = r;
	data[1] = g;
	data[2] = b;
	data[3] = a;
	}

void immAttrib3ubv(unsigned attrib_id, const unsigned char data[3])
	{
	immAttrib3ub(attrib_id, data[0], data[1], data[2]);
	}

void immAttrib4ubv(unsigned attrib_id, const unsigned char data[4])
	{
	immAttrib4ub(attrib_id, data[0], data[1], data[2], data[3]);
	}

void immSkipAttrib(unsigned attrib_id)
	{
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);
	}

static void immEndVertex(void) // and move on to the next vertex
	{
#if TRUST_NO_ONE
	assert(imm.prim_type != PRIM_NONE); // make sure we're between a Begin/End pair
	assert(imm.vertex_idx < imm.vertex_ct);
#endif

	// have all attribs been assigned values?
	// if not, copy value from previous vertex
	if (imm.unassigned_attrib_bits)
		{
#if TRUST_NO_ONE
		assert(imm.vertex_idx > 0); // first vertex must have all attribs specified
#endif

		for (unsigned a_idx = 0; a_idx < imm.vertex_format.attrib_ct; ++a_idx)
			{
			if ((imm.unassigned_attrib_bits >> a_idx) & 1)
				{
				const Attrib* a = imm.vertex_format.attribs + a_idx;

//				printf("copying %s from vertex %u to %u\n", a->name, imm.vertex_idx - 1, imm.vertex_idx);

				GLubyte* data = imm.vertex_data + a->offset;
				memcpy(data, data - imm.vertex_format.stride, a->sz);
				// TODO: consolidate copy of adjacent attributes
				}
			}
		}

	imm.vertex_idx++;
	imm.vertex_data += imm.vertex_format.stride;
	imm.unassigned_attrib_bits = imm.attrib_binding.enabled_bits;
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

void immVertex2i(unsigned attrib_id, int x, int y)
	{
	immAttrib2i(attrib_id, x, y);
	immEndVertex();
	}

void immVertex2s(unsigned attrib_id, short x, short y)
	{
	immAttrib2s(attrib_id, x, y);
	immEndVertex();
	}

void immVertex2fv(unsigned attrib_id, const float data[2])
	{
	immAttrib2f(attrib_id, data[0], data[1]);
	immEndVertex();
	}

void immVertex3fv(unsigned attrib_id, const float data[3])
	{
	immAttrib3f(attrib_id, data[0], data[1], data[2]);
	immEndVertex();
	}

void immVertex2iv(unsigned attrib_id, const int data[2])
	{
	immAttrib2i(attrib_id, data[0], data[1]);
	immEndVertex();
	}


// --- generic uniform functions ---

void immUniform1f(const char* name, float x)
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform1f(loc, x);
	}

void immUniform2f(const char* name, float x, float y)
{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform2f(loc, x, y);
}

void immUniform2fv(const char* name, const float data[2])
{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform2fv(loc, 1, data);
}

void immUniform3f(const char* name, float x, float y, float z)
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform3f(loc, x, y, z);
	}

void immUniform3fv(const char* name, const float data[3])
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform3fv(loc, 1, data);
	}

void immUniformArray3fv(const char* name, const float *data, int count)
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
	assert(count > 0);
#endif

	glUniform3fv(loc, count, data);
	}

void immUniform4f(const char* name, float x, float y, float z, float w)
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform4f(loc, x, y, z, w);
	}

void immUniform4fv(const char* name, const float data[4])
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform4fv(loc, 1, data);
	}

void immUniformMatrix4fv(const char* name, const float data[4][4])
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniformMatrix4fv(loc, 1, GL_FALSE, (float *)data);
	}

void immUniform1i(const char* name, int x)
	{
	int loc = glGetUniformLocation(imm.bound_program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform1i(loc, x);
	}


// --- convenience functions for setting "uniform vec4 color" ---

void immUniformColor4f(float r, float g, float b, float a)
	{
	immUniform4f("color", r, g, b, a);
	}

void immUniformColor4fv(const float rgba[4])
	{
	immUniform4fv("color", rgba);
	}

void immUniformColor3f(float r, float g, float b)
	{
	immUniform4f("color", r, g, b, 1.0f);
	}

void immUniformColor3fv(const float rgb[3])
	{
	immUniform4f("color", rgb[0], rgb[1], rgb[2], 1.0f);
	}

void immUniformColor3fvAlpha(const float rgb[3], float a)
	{
	immUniform4f("color", rgb[0], rgb[1], rgb[2], a);
	}

// TODO: v-- treat as sRGB? --v

void immUniformColor3ub(unsigned char r, unsigned char g, unsigned char b)
	{
	const float scale = 1.0f / 255.0f;
	immUniform4f("color", scale * r, scale * g, scale * b, 1.0f);
	}

void immUniformColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
	const float scale = 1.0f / 255.0f;
	immUniform4f("color", scale * r, scale * g, scale * b, scale * a);
	}

void immUniformColor3ubv(const unsigned char rgb[3])
	{
	immUniformColor3ub(rgb[0], rgb[1], rgb[2]);
	}

void immUniformColor4ubv(const unsigned char rgba[4])
	{
	immUniformColor4ub(rgba[0], rgba[1], rgba[2], rgba[3]);
	}
