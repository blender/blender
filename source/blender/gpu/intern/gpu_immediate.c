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

/** \file blender/gpu/intern/gpu_immediate.c
 *  \ingroup gpu
 *
 * GPU immediate mode work-alike
 */

#include "UI_resources.h"

#include "GPU_attr_binding.h"
#include "GPU_immediate.h"

#include "gpu_attr_binding_private.h"
#include "gpu_context_private.h"
#include "gpu_primitive_private.h"
#include "gpu_shader_private.h"
#include "gpu_vertex_format_private.h"

#include <string.h>
#include <stdlib.h>

/* necessary functions from matrix API */
extern void GPU_matrix_bind(const GPUShaderInterface *);
extern bool GPU_matrix_dirty_get(void);

typedef struct {
	/* TODO: organize this struct by frequency of change (run-time) */

	GPUBatch *batch;
	GPUContext *context;

	/* current draw call */
	GLubyte *buffer_data;
	uint buffer_offset;
	uint buffer_bytes_mapped;
	uint vertex_len;
	bool strict_vertex_len;
	GPUPrimType prim_type;

	GPUVertFormat vertex_format;

	/* current vertex */
	uint vertex_idx;
	GLubyte *vertex_data;
	uint16_t unassigned_attrib_bits; /* which attributes of current vertex have not been given values? */

	GLuint vbo_id;
	GLuint vao_id;

	GLuint bound_program;
	const GPUShaderInterface *shader_interface;
	GPUAttrBinding attrib_binding;
	uint16_t prev_enabled_attrib_bits; /* <-- only affects this VAO, so we're ok */
} Immediate;

/* size of internal buffer -- make this adjustable? */
#define IMM_BUFFER_SIZE (4 * 1024 * 1024)

static bool initialized = false;
static Immediate imm;

void immInit(void)
{
#if TRUST_NO_ONE
	assert(!initialized);
#endif
	memset(&imm, 0, sizeof(Immediate));

	imm.vbo_id = GPU_buf_alloc();
	glBindBuffer(GL_ARRAY_BUFFER, imm.vbo_id);
	glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);

	imm.prim_type = GPU_PRIM_NONE;
	imm.strict_vertex_len = true;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	initialized = true;
}

void immActivate(void)
{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == GPU_PRIM_NONE); /* make sure we're not between a Begin/End pair */
	assert(imm.vao_id == 0);
#endif
	imm.vao_id = GPU_vao_alloc();
	imm.context = GPU_context_active_get();
}

void immDeactivate(void)
{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == GPU_PRIM_NONE); /* make sure we're not between a Begin/End pair */
	assert(imm.vao_id != 0);
#endif
	GPU_vao_free(imm.vao_id, imm.context);
	imm.vao_id = 0;
	imm.prev_enabled_attrib_bits = 0;
}

void immDestroy(void)
{
	GPU_buf_free(imm.vbo_id);
	initialized = false;
}

GPUVertFormat *immVertexFormat(void)
{
	GPU_vertformat_clear(&imm.vertex_format);
	return &imm.vertex_format;
}

void immBindProgram(GLuint program, const GPUShaderInterface *shaderface)
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
	GPU_matrix_bind(shaderface);
}

void immBindBuiltinProgram(GPUBuiltinShader shader_id)
{
	GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);
	immBindProgram(shader->program, shader->interface);
}

void immUnbindProgram(void)
{
#if TRUST_NO_ONE
	assert(imm.bound_program != 0);
#endif
#if PROGRAM_NO_OPTI
	glUseProgram(0);
#endif
	imm.bound_program = 0;
}

#if TRUST_NO_ONE
static bool vertex_count_makes_sense_for_primitive(uint vertex_len, GPUPrimType prim_type)
{
	/* does vertex_len make sense for this primitive type? */
	if (vertex_len == 0) {
		return false;
	}

	switch (prim_type) {
		case GPU_PRIM_POINTS:
			return true;
		case GPU_PRIM_LINES:
			return vertex_len % 2 == 0;
		case GPU_PRIM_LINE_STRIP:
		case GPU_PRIM_LINE_LOOP:
			return vertex_len >= 2;
		case GPU_PRIM_LINE_STRIP_ADJ:
			return vertex_len >= 4;
		case GPU_PRIM_TRIS:
			return vertex_len % 3 == 0;
		case GPU_PRIM_TRI_STRIP:
		case GPU_PRIM_TRI_FAN:
			return vertex_len >= 3;
		default:
			return false;
	}
}
#endif

void immBegin(GPUPrimType prim_type, uint vertex_len)
{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == GPU_PRIM_NONE); /* make sure we haven't already begun */
	assert(vertex_count_makes_sense_for_primitive(vertex_len, prim_type));
#endif
	imm.prim_type = prim_type;
	imm.vertex_len = vertex_len;
	imm.vertex_idx = 0;
	imm.unassigned_attrib_bits = imm.attrib_binding.enabled_bits;

	/* how many bytes do we need for this draw call? */
	const uint bytes_needed = vertex_buffer_size(&imm.vertex_format, vertex_len);

#if TRUST_NO_ONE
	assert(bytes_needed <= IMM_BUFFER_SIZE);
#endif

	glBindBuffer(GL_ARRAY_BUFFER, imm.vbo_id);

	/* does the current buffer have enough room? */
	const uint available_bytes = IMM_BUFFER_SIZE - imm.buffer_offset;
	/* ensure vertex data is aligned */
	const uint pre_padding = padding(imm.buffer_offset, imm.vertex_format.stride); /* might waste a little space, but it's safe */
	if ((bytes_needed + pre_padding) <= available_bytes) {
		imm.buffer_offset += pre_padding;
	}
	else {
		/* orphan this buffer & start with a fresh one */
		/* this method works on all platforms, old & new */
		glBufferData(GL_ARRAY_BUFFER, IMM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);

		imm.buffer_offset = 0;
	}

/*	printf("mapping %u to %u\n", imm.buffer_offset, imm.buffer_offset + bytes_needed - 1); */

	imm.buffer_data = glMapBufferRange(GL_ARRAY_BUFFER, imm.buffer_offset, bytes_needed,
	                                   GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | (imm.strict_vertex_len ? 0 : GL_MAP_FLUSH_EXPLICIT_BIT));

#if TRUST_NO_ONE
	assert(imm.buffer_data != NULL);
#endif

	imm.buffer_bytes_mapped = bytes_needed;
	imm.vertex_data = imm.buffer_data;
}

void immBeginAtMost(GPUPrimType prim_type, uint vertex_len)
{
#if TRUST_NO_ONE
	assert(vertex_len > 0);
#endif

	imm.strict_vertex_len = false;
	immBegin(prim_type, vertex_len);
}


GPUBatch *immBeginBatch(GPUPrimType prim_type, uint vertex_len)
{
#if TRUST_NO_ONE
	assert(initialized);
	assert(imm.prim_type == GPU_PRIM_NONE); /* make sure we haven't already begun */
	assert(vertex_count_makes_sense_for_primitive(vertex_len, prim_type));
#endif
	imm.prim_type = prim_type;
	imm.vertex_len = vertex_len;
	imm.vertex_idx = 0;
	imm.unassigned_attrib_bits = imm.attrib_binding.enabled_bits;

	GPUVertBuf *verts = GPU_vertbuf_create_with_format(&imm.vertex_format);
	GPU_vertbuf_data_alloc(verts, vertex_len);

	imm.buffer_bytes_mapped = GPU_vertbuf_size_get(verts);
	imm.vertex_data = verts->data;

	imm.batch = GPU_batch_create_ex(prim_type, verts, NULL, GPU_BATCH_OWNS_VBO);
	imm.batch->phase = GPU_BATCH_BUILDING;

	return imm.batch;
}

GPUBatch *immBeginBatchAtMost(GPUPrimType prim_type, uint vertex_len)
{
	imm.strict_vertex_len = false;
	return immBeginBatch(prim_type, vertex_len);
}

static void immDrawSetup(void)
{
	/* set up VAO -- can be done during Begin or End really */
	glBindVertexArray(imm.vao_id);

	/* enable/disable vertex attribs as needed */
	if (imm.attrib_binding.enabled_bits != imm.prev_enabled_attrib_bits) {
		for (uint loc = 0; loc < GPU_VERT_ATTR_MAX_LEN; ++loc) {
			bool is_enabled = imm.attrib_binding.enabled_bits & (1 << loc);
			bool was_enabled = imm.prev_enabled_attrib_bits & (1 << loc);

			if (is_enabled && !was_enabled) {
				glEnableVertexAttribArray(loc);
			}
			else if (was_enabled && !is_enabled) {
				glDisableVertexAttribArray(loc);
			}
		}

		imm.prev_enabled_attrib_bits = imm.attrib_binding.enabled_bits;
	}

	const uint stride = imm.vertex_format.stride;

	for (uint a_idx = 0; a_idx < imm.vertex_format.attr_len; ++a_idx) {
		const GPUVertAttr *a = imm.vertex_format.attribs + a_idx;

		const uint offset = imm.buffer_offset + a->offset;
		const GLvoid *pointer = (const GLubyte *)0 + offset;

		const uint loc = read_attrib_location(&imm.attrib_binding, a_idx);

		switch (a->fetch_mode) {
			case GPU_FETCH_FLOAT:
			case GPU_FETCH_INT_TO_FLOAT:
				glVertexAttribPointer(loc, a->comp_len, a->gl_comp_type, GL_FALSE, stride, pointer);
				break;
			case GPU_FETCH_INT_TO_FLOAT_UNIT:
				glVertexAttribPointer(loc, a->comp_len, a->gl_comp_type, GL_TRUE, stride, pointer);
				break;
			case GPU_FETCH_INT:
				glVertexAttribIPointer(loc, a->comp_len, a->gl_comp_type, stride, pointer);
		}
	}

	if (GPU_matrix_dirty_get()) {
		GPU_matrix_bind(imm.shader_interface);
	}
}

void immEnd(void)
{
#if TRUST_NO_ONE
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif

	uint buffer_bytes_used;
	if (imm.strict_vertex_len) {
#if TRUST_NO_ONE
		assert(imm.vertex_idx == imm.vertex_len); /* with all vertices defined */
#endif
		buffer_bytes_used = imm.buffer_bytes_mapped;
	}
	else {
#if TRUST_NO_ONE
		assert(imm.vertex_idx <= imm.vertex_len);
#endif
		if (imm.vertex_idx == imm.vertex_len) {
			buffer_bytes_used = imm.buffer_bytes_mapped;
		}
		else {
#if TRUST_NO_ONE
			assert(imm.vertex_idx == 0 || vertex_count_makes_sense_for_primitive(imm.vertex_idx, imm.prim_type));
#endif
			imm.vertex_len = imm.vertex_idx;
			buffer_bytes_used = vertex_buffer_size(&imm.vertex_format, imm.vertex_len);
			/* unused buffer bytes are available to the next immBegin */
		}
		/* tell OpenGL what range was modified so it doesn't copy the whole mapped range */
		glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, buffer_bytes_used);
	}

	if (imm.batch) {
		if (buffer_bytes_used != imm.buffer_bytes_mapped) {
			GPU_vertbuf_data_resize(imm.batch->verts[0], imm.vertex_len);
			/* TODO: resize only if vertex count is much smaller */
		}
		GPU_batch_program_set(imm.batch, imm.bound_program, imm.shader_interface);
		imm.batch->phase = GPU_BATCH_READY_TO_DRAW;
		imm.batch = NULL; /* don't free, batch belongs to caller */
	}
	else {
		glUnmapBuffer(GL_ARRAY_BUFFER);
		if (imm.vertex_len > 0) {
			immDrawSetup();
			glDrawArrays(convert_prim_type_to_gl(imm.prim_type), 0, imm.vertex_len);
		}
		/* These lines are causing crash on startup on some old GPU + drivers.
		 * They are not required so just comment them. (T55722) */
		// glBindBuffer(GL_ARRAY_BUFFER, 0);
		// glBindVertexArray(0);
		/* prep for next immBegin */
		imm.buffer_offset += buffer_bytes_used;
	}

	/* prep for next immBegin */
	imm.prim_type = GPU_PRIM_NONE;
	imm.strict_vertex_len = true;
}

static void setAttribValueBit(uint attrib_id)
{
	uint16_t mask = 1 << attrib_id;
#if TRUST_NO_ONE
	assert(imm.unassigned_attrib_bits & mask); /* not already set */
#endif
	imm.unassigned_attrib_bits &= ~mask;
}


/* --- generic attribute functions --- */

void immAttrib1f(uint attrib_id, float x)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_F32);
	assert(attrib->comp_len == 1);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	float *data = (float *)(imm.vertex_data + attrib->offset);
/*	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data); */

	data[0] = x;
}

void immAttrib2f(uint attrib_id, float x, float y)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_F32);
	assert(attrib->comp_len == 2);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	float *data = (float *)(imm.vertex_data + attrib->offset);
/*	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data); */

	data[0] = x;
	data[1] = y;
}

void immAttrib3f(uint attrib_id, float x, float y, float z)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_F32);
	assert(attrib->comp_len == 3);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	float *data = (float *)(imm.vertex_data + attrib->offset);
/*	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data); */

	data[0] = x;
	data[1] = y;
	data[2] = z;
}

void immAttrib4f(uint attrib_id, float x, float y, float z, float w)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_F32);
	assert(attrib->comp_len == 4);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	float *data = (float *)(imm.vertex_data + attrib->offset);
/*	printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm.buffer_data, data); */

	data[0] = x;
	data[1] = y;
	data[2] = z;
	data[3] = w;
}

void immAttrib1u(uint attrib_id, uint x)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_U32);
	assert(attrib->comp_len == 1);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	uint *data = (uint *)(imm.vertex_data + attrib->offset);

	data[0] = x;
}

void immAttrib2i(uint attrib_id, int x, int y)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_I32);
	assert(attrib->comp_len == 2);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	int *data = (int *)(imm.vertex_data + attrib->offset);

	data[0] = x;
	data[1] = y;
}

void immAttrib2s(uint attrib_id, short x, short y)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_I16);
	assert(attrib->comp_len == 2);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	short *data = (short *)(imm.vertex_data + attrib->offset);

	data[0] = x;
	data[1] = y;
}

void immAttrib2fv(uint attrib_id, const float data[2])
{
	immAttrib2f(attrib_id, data[0], data[1]);
}

void immAttrib3fv(uint attrib_id, const float data[3])
{
	immAttrib3f(attrib_id, data[0], data[1], data[2]);
}

void immAttrib4fv(uint attrib_id, const float data[4])
{
	immAttrib4f(attrib_id, data[0], data[1], data[2], data[3]);
}

void immAttrib3ub(uint attrib_id, unsigned char r, unsigned char g, unsigned char b)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_U8);
	assert(attrib->comp_len == 3);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	GLubyte *data = imm.vertex_data + attrib->offset;
/*	printf("%s %td %p\n", __FUNCTION__, data - imm.buffer_data, data); */

	data[0] = r;
	data[1] = g;
	data[2] = b;
}

void immAttrib4ub(uint attrib_id, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	GPUVertAttr *attrib = imm.vertex_format.attribs + attrib_id;
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(attrib->comp_type == GPU_COMP_U8);
	assert(attrib->comp_len == 4);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);

	GLubyte *data = imm.vertex_data + attrib->offset;
/*	printf("%s %td %p\n", __FUNCTION__, data - imm.buffer_data, data); */

	data[0] = r;
	data[1] = g;
	data[2] = b;
	data[3] = a;
}

void immAttrib3ubv(uint attrib_id, const unsigned char data[3])
{
	immAttrib3ub(attrib_id, data[0], data[1], data[2]);
}

void immAttrib4ubv(uint attrib_id, const unsigned char data[4])
{
	immAttrib4ub(attrib_id, data[0], data[1], data[2], data[3]);
}

void immSkipAttrib(uint attrib_id)
{
#if TRUST_NO_ONE
	assert(attrib_id < imm.vertex_format.attr_len);
	assert(imm.vertex_idx < imm.vertex_len);
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
#endif
	setAttribValueBit(attrib_id);
}

static void immEndVertex(void) /* and move on to the next vertex */
{
#if TRUST_NO_ONE
	assert(imm.prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
	assert(imm.vertex_idx < imm.vertex_len);
#endif

	/* have all attribs been assigned values?
	 * if not, copy value from previous vertex */
	if (imm.unassigned_attrib_bits) {
#if TRUST_NO_ONE
		assert(imm.vertex_idx > 0); /* first vertex must have all attribs specified */
#endif
		for (uint a_idx = 0; a_idx < imm.vertex_format.attr_len; ++a_idx) {
			if ((imm.unassigned_attrib_bits >> a_idx) & 1) {
				const GPUVertAttr *a = imm.vertex_format.attribs + a_idx;

/*				printf("copying %s from vertex %u to %u\n", a->name, imm.vertex_idx - 1, imm.vertex_idx); */

				GLubyte *data = imm.vertex_data + a->offset;
				memcpy(data, data - imm.vertex_format.stride, a->sz);
				/* TODO: consolidate copy of adjacent attributes */
			}
		}
	}

	imm.vertex_idx++;
	imm.vertex_data += imm.vertex_format.stride;
	imm.unassigned_attrib_bits = imm.attrib_binding.enabled_bits;
}

void immVertex2f(uint attrib_id, float x, float y)
{
	immAttrib2f(attrib_id, x, y);
	immEndVertex();
}

void immVertex3f(uint attrib_id, float x, float y, float z)
{
	immAttrib3f(attrib_id, x, y, z);
	immEndVertex();
}

void immVertex4f(uint attrib_id, float x, float y, float z, float w)
{
	immAttrib4f(attrib_id, x, y, z, w);
	immEndVertex();
}

void immVertex2i(uint attrib_id, int x, int y)
{
	immAttrib2i(attrib_id, x, y);
	immEndVertex();
}

void immVertex2s(uint attrib_id, short x, short y)
{
	immAttrib2s(attrib_id, x, y);
	immEndVertex();
}

void immVertex2fv(uint attrib_id, const float data[2])
{
	immAttrib2f(attrib_id, data[0], data[1]);
	immEndVertex();
}

void immVertex3fv(uint attrib_id, const float data[3])
{
	immAttrib3f(attrib_id, data[0], data[1], data[2]);
	immEndVertex();
}

void immVertex2iv(uint attrib_id, const int data[2])
{
	immAttrib2i(attrib_id, data[0], data[1]);
	immEndVertex();
}


/* --- generic uniform functions --- */

#if 0
#  if TRUST_NO_ONE
#    define GET_UNIFORM const GPUShaderInput* uniform = GPU_shaderinterface_uniform(imm.shader_interface, name); assert(uniform);
#  else
#    define GET_UNIFORM const GPUShaderInput* uniform = GPU_shaderinterface_uniform(imm.shader_interface, name);
#  endif
#else
	/* NOTE: It is possible to have uniform fully optimized out from the shader.
	 *       In this case we can't assert failure or allow NULL-pointer dereference.
	 * TODO(sergey): How can we detect existing-but-optimized-out uniform but still
	 *               catch typos in uniform names passed to immUniform*() functions? */
#  define GET_UNIFORM const GPUShaderInput* uniform = GPU_shaderinterface_uniform(imm.shader_interface, name); if (uniform == NULL) return;
#endif

void immUniform1f(const char *name, float x)
{
	GET_UNIFORM
	glUniform1f(uniform->location, x);
}

void immUniform2f(const char *name, float x, float y)
{
	GET_UNIFORM
	glUniform2f(uniform->location, x, y);
}

void immUniform2fv(const char *name, const float data[2])
{
	GET_UNIFORM
	glUniform2fv(uniform->location, 1, data);
}

void immUniform3f(const char *name, float x, float y, float z)
{
	GET_UNIFORM
	glUniform3f(uniform->location, x, y, z);
}

void immUniform3fv(const char *name, const float data[3])
{
	GET_UNIFORM
	glUniform3fv(uniform->location, 1, data);
}

/* can increase this limit or move to another file */
#define MAX_UNIFORM_NAME_LEN 60

void immUniformArray3fv(const char *bare_name, const float *data, int count)
{
	/* look up "name[0]" when given "name" */
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

void immUniform4f(const char *name, float x, float y, float z, float w)
{
	GET_UNIFORM
	glUniform4f(uniform->location, x, y, z, w);
}

void immUniform4fv(const char *name, const float data[4])
{
	GET_UNIFORM
	glUniform4fv(uniform->location, 1, data);
}

void immUniformArray4fv(const char *bare_name, const float *data, int count)
{
	/* look up "name[0]" when given "name" */
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

void immUniformMatrix4fv(const char *name, const float data[4][4])
{
	GET_UNIFORM
	glUniformMatrix4fv(uniform->location, 1, GL_FALSE, (float *)data);
}

void immUniform1i(const char *name, int x)
{
	GET_UNIFORM
	glUniform1i(uniform->location, x);
}

void immUniform4iv(const char *name, const int data[4])
{
	GET_UNIFORM
	glUniform4iv(uniform->location, 1, data);
}

/* --- convenience functions for setting "uniform vec4 color" --- */

void immUniformColor4f(float r, float g, float b, float a)
{
	const GPUShaderInput *uniform = GPU_shaderinterface_uniform_builtin(imm.shader_interface, GPU_UNIFORM_COLOR);
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

/* TODO: v-- treat as sRGB? --v */

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

void immUniformThemeColor(int color_id)
{
	float color[4];
	UI_GetThemeColor4fv(color_id, color);
	immUniformColor4fv(color);
}

void immUniformThemeColor3(int color_id)
{
	float color[3];
	UI_GetThemeColor3fv(color_id, color);
	immUniformColor3fv(color);
}

void immUniformThemeColorShade(int color_id, int offset)
{
	float color[4];
	UI_GetThemeColorShade4fv(color_id, offset, color);
	immUniformColor4fv(color);
}

void immUniformThemeColorShadeAlpha(int color_id, int color_offset, int alpha_offset)
{
	float color[4];
	UI_GetThemeColorShadeAlpha4fv(color_id, color_offset, alpha_offset, color);
	immUniformColor4fv(color);
}

void immUniformThemeColorBlendShade(int color_id1, int color_id2, float fac, int offset)
{
	float color[4];
	UI_GetThemeColorBlendShade4fv(color_id1, color_id2, fac, offset, color);
	immUniformColor4fv(color);
}

void immUniformThemeColorBlend(int color_id1, int color_id2, float fac)
{
	uint8_t color[3];
	UI_GetThemeColorBlend3ubv(color_id1, color_id2, fac, color);
	immUniformColor3ubv(color);
}

void immThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset)
{
	unsigned char col[4];
	UI_GetThemeColorShadeAlpha4ubv(colorid, coloffset, alphaoffset, col);
	immUniformColor4ub(col[0], col[1], col[2], col[3]);
}
