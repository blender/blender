
// Gawain immediate mode work-alike
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gwn_immediate.h"
#include "gwn_buffer_id.h"
#include "gwn_attr_binding.h"
#include "gwn_attr_binding_private.h"
#include "gwn_vertex_format_private.h"
#include "gwn_vertex_array_id.h"
#include "gwn_primitive_private.h"
#include <string.h>

// necessary functions from matrix API
extern void gpuBindMatrices(const Gwn_ShaderInterface*);
extern bool gpuMatricesDirty(void);

typedef struct {
	// TODO: organize this struct by frequency of change (run-time)

#if IMM_BATCH_COMBO
	Gwn_Batch* batch;
#endif
	Gwn_Context* context;

	// current draw call
	GLubyte* buffer_data;
	unsigned buffer_offset;
	unsigned buffer_bytes_mapped;
	unsigned vertex_ct;
	bool strict_vertex_ct;
	Gwn_PrimType prim_type;

	Gwn_VertFormat vertex_format;

	// current vertex
	unsigned vertex_idx;
	GLubyte* vertex_data;
	uint16_t unassigned_attrib_bits; // which attributes of current vertex have not been given values?

	GLuint vbo_id;
	GLuint vao_id;
	
	GLuint bound_program;
	const Gwn_ShaderInterface* shader_interface;
	Gwn_AttrBinding attrib_binding;
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

	imm.vbo_id = GWN_buf_id_alloc();
	glBindBuffer(GL_ARRAY_BUFFER, imm.vbo_id);
	glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);

	imm.prim_type = GWN_PRIM_NONE;
	imm.strict_vertex_ct = true;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	initialized = true;

	immActivate();
	}

void immActivate(void)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == GWN_PRIM_NONE); // make sure we're not between a Begin/End pair
	assert(imm.vao_id == 0);
#endif
	imm.vao_id = GWN_vao_alloc();
	imm.context = GWN_context_active_get();
	}

void immDeactivate(void)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == GWN_PRIM_NONE); // make sure we're not between a Begin/End pair
	assert(imm.vao_id != 0);
#endif
	GWN_vao_free(imm.vao_id, imm.context);
	imm.vao_id = 0;
	imm.prev_enabled_attrib_bits = 0;
	}

void immDestroy(void)
	{
	immDeactivate();
	GWN_buf_id_free(imm.vbo_id);
	initialized = false;
	}

Gwn_VertFormat* immVertexFormat(void)
	{
	GWN_vertformat_clear(&imm.vertex_format);
	return &imm.vertex_format;
	}

void immBindProgram(GLuint program, const Gwn_ShaderInterface* shaderface)
	{
#if TRUST_NO_ONE
	assert(imm.bound_program == 0);
	assert(glIsProgram(program));
#endif

	imm.bound_program = program;
	imm.shader_interface = shaderface;

	if (!imm.vertex_format.packed)
		VertexFormat_pack(&imm.vertex_format);

	glUseProgram(program);
	get_attrib_locations(&imm.vertex_format, &imm.attrib_binding, shaderface);
	gpuBindMatrices(shaderface);
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
static bool vertex_count_makes_sense_for_primitive(unsigned vertex_ct, Gwn_PrimType prim_type)
	{
	// does vertex_ct make sense for this primitive type?
	if (vertex_ct == 0)
		return false;

	switch (prim_type)
		{
		case GWN_PRIM_POINTS:
			return true;
		case GWN_PRIM_LINES:
			return vertex_ct % 2 == 0;
		case GWN_PRIM_LINE_STRIP:
		case GWN_PRIM_LINE_LOOP:
			return vertex_ct >= 2;
		case GWN_PRIM_LINE_STRIP_ADJ:
			return vertex_ct >= 4;
		case GWN_PRIM_TRIS:
			return vertex_ct % 3 == 0;
		case GWN_PRIM_TRI_STRIP:
		case GWN_PRIM_TRI_FAN:
			return vertex_ct >= 3;
		default:
			return false;
		}
	}
#endif

void immBegin(Gwn_PrimType prim_type, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == GWN_PRIM_NONE); // make sure we haven't already begun
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
#if 1
		// this method works on all platforms, old & new
		glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);
#else
		// TODO: use other (more recent) methods after thorough testing
		if (GLEW_VERSION_4_3 || GLEW_ARB_invalidate_subdata)
			glInvalidateBufferData(imm.vbo_id);
		else
			{
			// glitches!
//			glMapBufferRange(GL_ARRAY_BUFFER, 0, IMM_BUFFER_SIZE, GL_MAP_INVALIDATE_BUFFER_BIT);

			// works
//			glMapBufferRange(GL_ARRAY_BUFFER, 0, IMM_BUFFER_SIZE,
//			                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
//			glUnmapBuffer(GL_ARRAY_BUFFER);

			// also works
			glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);
			}
#endif

		imm.buffer_offset = 0;
		}

//	printf("mapping %u to %u\n", imm.buffer_offset, imm.buffer_offset + bytes_needed - 1);

	imm.buffer_data = glMapBufferRange(GL_ARRAY_BUFFER, imm.buffer_offset, bytes_needed,
	                                   GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | (imm.strict_vertex_ct ? 0 : GL_MAP_FLUSH_EXPLICIT_BIT));

#if TRUST_NO_ONE
	assert(imm.buffer_data != NULL);
#endif

	imm.buffer_bytes_mapped = bytes_needed;
	imm.vertex_data = imm.buffer_data;
	}

void immBeginAtMost(Gwn_PrimType prim_type, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(vertex_ct > 0);
#endif

	imm.strict_vertex_ct = false;
	immBegin(prim_type, vertex_ct);
	}

#if IMM_BATCH_COMBO

Gwn_Batch* immBeginBatch(Gwn_PrimType prim_type, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == GWN_PRIM_NONE); // make sure we haven't already begun
	assert(vertex_count_makes_sense_for_primitive(vertex_ct, prim_type));
#endif

	imm.prim_type = prim_type;
	imm.vertex_ct = vertex_ct;
	imm.vertex_idx = 0;
	imm.unassigned_attrib_bits = imm.attrib_binding.enabled_bits;

	Gwn_VertBuf* verts = GWN_vertbuf_create_with_format(&imm.vertex_format);
	GWN_vertbuf_data_alloc(verts, vertex_ct);

	imm.buffer_bytes_mapped = GWN_vertbuf_size_get(verts);
	imm.vertex_data = verts->data;

	imm.batch = GWN_batch_create(prim_type, verts, NULL);
	imm.batch->phase = GWN_BATCH_BUILDING;

	return imm.batch;
	}

Gwn_Batch* immBeginBatchAtMost(Gwn_PrimType prim_type, unsigned vertex_ct)
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
		for (unsigned loc = 0; loc < GWN_VERT_ATTR_MAX_LEN; ++loc)
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
		const Gwn_VertAttr* a = imm.vertex_format.attribs + a_idx;

		const unsigned offset = imm.buffer_offset + a->offset;
		const GLvoid* pointer = (const GLubyte*)0 + offset;

		const unsigned loc = read_attrib_location(&imm.attrib_binding, a_idx);

//		printf("specifying attrib %u '%s' with offset %u, stride %u\n", loc, a->name, offset, stride);

		switch (a->fetch_mode)
			{
			case GWN_FETCH_FLOAT:
			case GWN_FETCH_INT_TO_FLOAT:
				glVertexAttribPointer(loc, a->comp_ct, a->gl_comp_type, GL_FALSE, stride, pointer);
				break;
			case GWN_FETCH_INT_TO_FLOAT_UNIT:
				glVertexAttribPointer(loc, a->comp_ct, a->gl_comp_type, GL_TRUE, stride, pointer);
				break;
			case GWN_FETCH_INT:
				glVertexAttribIPointer(loc, a->comp_ct, a->gl_comp_type, stride, pointer);
			}
		}

	if (gpuMatricesDirty())
		gpuBindMatrices(imm.shader_interface);
	}

void immEnd(void)
	{
#if TRUST_NO_ONE
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
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

		// tell OpenGL what range was modified so it doesn't copy the whole mapped range
		// printf("flushing %u to %u\n", imm.buffer_offset, imm.buffer_offset + buffer_bytes_used - 1);
		glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, buffer_bytes_used);
		}

#if IMM_BATCH_COMBO
	if (imm.batch)
		{
		if (buffer_bytes_used != imm.buffer_bytes_mapped)
			{
			GWN_vertbuf_data_resize(imm.batch->verts[0], imm.vertex_ct);
			// TODO: resize only if vertex count is much smaller
			}

		GWN_batch_program_set(imm.batch, imm.bound_program, imm.shader_interface);
		imm.batch->phase = GWN_BATCH_READY_TO_DRAW;
		imm.batch = NULL; // don't free, batch belongs to caller
		}
	else
#endif
		{
		glUnmapBuffer(GL_ARRAY_BUFFER);

		if (imm.vertex_ct > 0)
			{
			immDrawSetup();
			glDrawArrays(convert_prim_type_to_gl(imm.prim_type), 0, imm.vertex_ct);
			}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// prep for next immBegin
		imm.buffer_offset += buffer_bytes_used;
		}

	// prep for next immBegin
	imm.prim_type = GWN_PRIM_NONE;
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
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_F32);
	assert(attrib->comp_ct == 1);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = (float*)(imm.vertex_data + attrib->offset);
//	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data);

	data[0] = x;
	}

void immAttrib2f(unsigned attrib_id, float x, float y)
	{
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_F32);
	assert(attrib->comp_ct == 2);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = (float*)(imm.vertex_data + attrib->offset);
//	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data);

	data[0] = x;
	data[1] = y;
	}

void immAttrib3f(unsigned attrib_id, float x, float y, float z)
	{
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_F32);
	assert(attrib->comp_ct == 3);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
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
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_F32);
	assert(attrib->comp_ct == 4);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	float* data = (float*)(imm.vertex_data + attrib->offset);
//	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data);

	data[0] = x;
	data[1] = y;
	data[2] = z;
	data[3] = w;
	}

void immAttrib1u(unsigned attrib_id, unsigned x)
	{
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_U32);
	assert(attrib->comp_ct == 1);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	unsigned* data = (unsigned*)(imm.vertex_data + attrib->offset);

	data[0] = x;
	}

void immAttrib2i(unsigned attrib_id, int x, int y)
	{
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_I32);
	assert(attrib->comp_ct == 2);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	int* data = (int*)(imm.vertex_data + attrib->offset);

	data[0] = x;
	data[1] = y;
	}

void immAttrib2s(unsigned attrib_id, short x, short y)
	{
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_I16);
	assert(attrib->comp_ct == 2);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);

	short* data = (short*)(imm.vertex_data + attrib->offset);

	data[0] = x;
	data[1] = y;
	}

void immAttrib2fv(unsigned attrib_id, const float data[2])
	{
	immAttrib2f(attrib_id, data[0], data[1]);
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
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_U8);
	assert(attrib->comp_ct == 3);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
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
	Gwn_VertAttr* attrib = imm.vertex_format.attribs + attrib_id;

#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attrib_ct);
	assert(attrib->comp_type == GWN_COMP_U8);
	assert(attrib->comp_ct == 4);
	assert(imm.vertex_idx < imm.vertex_ct);
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
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
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
#endif

	setAttribValueBit(attrib_id);
	}

static void immEndVertex(void) // and move on to the next vertex
	{
#if TRUST_NO_ONE
	assert(imm.prim_type != GWN_PRIM_NONE); // make sure we're between a Begin/End pair
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
				const Gwn_VertAttr* a = imm.vertex_format.attribs + a_idx;

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

void immVertex4f(unsigned attrib_id, float x, float y, float z, float w)
	{
	immAttrib4f(attrib_id, x, y, z, w);
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

#if 0
  #if TRUST_NO_ONE
    #define GET_UNIFORM const Gwn_ShaderInput* uniform = GWN_shaderinterface_uniform(imm.shader_interface, name); assert(uniform);
  #else
    #define GET_UNIFORM const Gwn_ShaderInput* uniform = GWN_shaderinterface_uniform(imm.shader_interface, name);
  #endif
#else
	// NOTE: It is possible to have uniform fully optimized out from the shader.
	//       In this case we can't assert failure or allow NULL-pointer dereference.
	// TODO(sergey): How can we detect existing-but-optimized-out uniform but still
	//               catch typos in uniform names passed to immUniform*() functions?
  #define GET_UNIFORM const Gwn_ShaderInput* uniform = GWN_shaderinterface_uniform(imm.shader_interface, name); if (uniform == NULL) return;
#endif

void immUniform1f(const char* name, float x)
	{
	GET_UNIFORM
	glUniform1f(uniform->location, x);
	}

void immUniform2f(const char* name, float x, float y)
	{
	GET_UNIFORM
	glUniform2f(uniform->location, x, y);
	}

void immUniform2fv(const char* name, const float data[2])
	{
	GET_UNIFORM
	glUniform2fv(uniform->location, 1, data);
	}

void immUniform3f(const char* name, float x, float y, float z)
	{
	GET_UNIFORM
	glUniform3f(uniform->location, x, y, z);
	}

void immUniform3fv(const char* name, const float data[3])
	{
	GET_UNIFORM
	glUniform3fv(uniform->location, 1, data);
	}

// can increase this limit or move to another file
#define MAX_UNIFORM_NAME_LEN 60

void immUniformArray3fv(const char* bare_name, const float *data, int count)
	{
	// look up "name[0]" when given "name"
	const size_t len = strlen(bare_name);
#if TRUST_NO_ONE
	assert(len <= MAX_UNIFORM_NAME_LEN);
#endif
	char name[MAX_UNIFORM_NAME_LEN];
	strcpy(name, bare_name);
	name[len + 0] = '[';
	name[len + 1] = '0';
	name[len + 2] = ']';
	name[len + 3] = '\0';

	GET_UNIFORM
	glUniform3fv(uniform->location, count, data);
	}

void immUniform4f(const char* name, float x, float y, float z, float w)
	{
	GET_UNIFORM
	glUniform4f(uniform->location, x, y, z, w);
	}

void immUniform4fv(const char* name, const float data[4])
	{
	GET_UNIFORM
	glUniform4fv(uniform->location, 1, data);
	}

void immUniformArray4fv(const char* bare_name, const float *data, int count)
	{
	// look up "name[0]" when given "name"
	const size_t len = strlen(bare_name);
#if TRUST_NO_ONE
	assert(len <= MAX_UNIFORM_NAME_LEN);
#endif
	char name[MAX_UNIFORM_NAME_LEN];
	strcpy(name, bare_name);
	name[len + 0] = '[';
	name[len + 1] = '0';
	name[len + 2] = ']';
	name[len + 3] = '\0';

	GET_UNIFORM
	glUniform4fv(uniform->location, count, data);
	}

void immUniformMatrix4fv(const char* name, const float data[4][4])
	{
	GET_UNIFORM
	glUniformMatrix4fv(uniform->location, 1, GL_FALSE, (float *)data);
	}

void immUniform1i(const char* name, int x)
	{
	GET_UNIFORM
	glUniform1i(uniform->location, x);
	}

void immUniform4iv(const char* name, const int data[4])
	{
	GET_UNIFORM
	glUniform4iv(uniform->location, 1, data);
	}

// --- convenience functions for setting "uniform vec4 color" ---

void immUniformColor4f(float r, float g, float b, float a)
	{
	const Gwn_ShaderInput* uniform = GWN_shaderinterface_uniform_builtin(imm.shader_interface, GWN_UNIFORM_COLOR);

#if TRUST_NO_ONE
	assert(uniform != NULL);
#endif

	glUniform4f(uniform->location, r, g, b, a);
	}

void immUniformColor4fv(const float rgba[4])
	{
	immUniformColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);
	}

void immUniformColor3f(float r, float g, float b)
	{
	immUniformColor4f(r, g, b, 1.0f);
	}

void immUniformColor3fv(const float rgb[3])
	{
	immUniformColor4f(rgb[0], rgb[1], rgb[2], 1.0f);
	}

void immUniformColor3fvAlpha(const float rgb[3], float a)
	{
	immUniformColor4f(rgb[0], rgb[1], rgb[2], a);
	}

// TODO: v-- treat as sRGB? --v

void immUniformColor3ub(unsigned char r, unsigned char g, unsigned char b)
	{
	const float scale = 1.0f / 255.0f;
	immUniformColor4f(scale * r, scale * g, scale * b, 1.0f);
	}

void immUniformColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
	const float scale = 1.0f / 255.0f;
	immUniformColor4f(scale * r, scale * g, scale * b, scale * a);
	}

void immUniformColor3ubv(const unsigned char rgb[3])
	{
	immUniformColor3ub(rgb[0], rgb[1], rgb[2]);
	}

void immUniformColor3ubvAlpha(const unsigned char rgb[3], unsigned char alpha)
	{
	immUniformColor4ub(rgb[0], rgb[1], rgb[2], alpha);
	}

void immUniformColor4ubv(const unsigned char rgba[4])
	{
	immUniformColor4ub(rgba[0], rgba[1], rgba[2], rgba[3]);
	}
