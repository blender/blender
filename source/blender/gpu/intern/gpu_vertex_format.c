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
 * Contributor(s): Blender Foundation, Clément Foucault
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_vertex_format.c
 *  \ingroup gpu
 *
 * GPU vertex format
 */

#include "GPU_shader_interface.h"

#include "GPU_vertex_format.h"
#include "gpu_vertex_format_private.h"
#include <stddef.h>
#include <string.h>

#include "BLI_utildefines.h"

#define PACK_DEBUG 0

#if PACK_DEBUG
#  include <stdio.h>
#endif

void GPU_vertformat_clear(GPUVertFormat *format)
{
#if TRUST_NO_ONE
	memset(format, 0, sizeof(GPUVertFormat));
#else
	format->attr_len = 0;
	format->packed = false;
	format->name_offset = 0;
	format->name_len = 0;

	for (uint i = 0; i < GPU_VERT_ATTR_MAX_LEN; i++) {
		format->attribs[i].name_len = 0;
	}
#endif
}

void GPU_vertformat_copy(GPUVertFormat *dest, const GPUVertFormat *src)
{
	/* copy regular struct fields */
	memcpy(dest, src, sizeof(GPUVertFormat));

	for (uint i = 0; i < dest->attr_len; i++) {
		for (uint j = 0; j < dest->attribs[i].name_len; j++) {
			dest->attribs[i].name[j] = (char *)dest + (src->attribs[i].name[j] - ((char *)src));
		}
	}
}

static GLenum convert_comp_type_to_gl(GPUVertCompType type)
{
	static const GLenum table[] = {
		[GPU_COMP_I8] = GL_BYTE,
		[GPU_COMP_U8] = GL_UNSIGNED_BYTE,
		[GPU_COMP_I16] = GL_SHORT,
		[GPU_COMP_U16] = GL_UNSIGNED_SHORT,
		[GPU_COMP_I32] = GL_INT,
		[GPU_COMP_U32] = GL_UNSIGNED_INT,

		[GPU_COMP_F32] = GL_FLOAT,

		[GPU_COMP_I10] = GL_INT_2_10_10_10_REV
	};
	return table[type];
}

static uint comp_sz(GPUVertCompType type)
{
#if TRUST_NO_ONE
	assert(type <= GPU_COMP_F32); /* other types have irregular sizes (not bytes) */
#endif
	const GLubyte sizes[] = {1, 1, 2, 2, 4, 4, 4};
	return sizes[type];
}

static uint attrib_sz(const GPUVertAttr *a)
{
	if (a->comp_type == GPU_COMP_I10) {
		return 4; /* always packed as 10_10_10_2 */
	}
	return a->comp_len * comp_sz(a->comp_type);
}

static uint attrib_align(const GPUVertAttr *a)
{
	if (a->comp_type == GPU_COMP_I10) {
		return 4; /* always packed as 10_10_10_2 */
	}
	uint c = comp_sz(a->comp_type);
	if (a->comp_len == 3 && c <= 2) {
		return 4 * c; /* AMD HW can't fetch these well, so pad it out (other vendors too?) */
	}
	else {
		return c; /* most fetches are ok if components are naturally aligned */
	}
}

uint vertex_buffer_size(const GPUVertFormat *format, uint vertex_len)
{
#if TRUST_NO_ONE
	assert(format->packed && format->stride > 0);
#endif
	return format->stride * vertex_len;
}

static const char *copy_attrib_name(GPUVertFormat *format, const char *name, const char *suffix)
{
	/* strncpy does 110% of what we need; let's do exactly 100% */
	char *name_copy = format->names + format->name_offset;
	uint available = GPU_VERT_ATTR_NAMES_BUF_LEN - format->name_offset;
	bool terminated = false;

	for (uint i = 0; i < available; ++i) {
		const char c = name[i];
		name_copy[i] = c;
		if (c == '\0') {
			if (suffix) {
				for (uint j = 0; j < available; ++j) {
					const char s = suffix[j];
					name_copy[i + j] = s;
					if (s == '\0') {
						terminated = true;
						format->name_offset += (i + j + 1);
						break;
					}
				}
			}
			else {
				terminated = true;
				format->name_offset += (i + 1);
			}
			break;
		}
	}
#if TRUST_NO_ONE
	assert(terminated);
	assert(format->name_offset <= GPU_VERT_ATTR_NAMES_BUF_LEN);
#else
	(void)terminated;
#endif
	return name_copy;
}

uint GPU_vertformat_attr_add(
        GPUVertFormat *format, const char *name,
        GPUVertCompType comp_type, uint comp_len, GPUVertFetchMode fetch_mode)
{
#if TRUST_NO_ONE
	assert(format->name_len < GPU_VERT_ATTR_MAX_LEN); /* there's room for more */
	assert(format->attr_len < GPU_VERT_ATTR_MAX_LEN); /* there's room for more */
	assert(!format->packed); /* packed means frozen/locked */
	assert((comp_len >= 1 && comp_len <= 4) || comp_len == 8 || comp_len == 12 || comp_len == 16);

	switch (comp_type) {
		case GPU_COMP_F32:
			/* float type can only kept as float */
			assert(fetch_mode == GPU_FETCH_FLOAT);
			break;
		case GPU_COMP_I10:
			/* 10_10_10 format intended for normals (xyz) or colors (rgb)
			 * extra component packed.w can be manually set to { -2, -1, 0, 1 } */
			assert(comp_len == 3 || comp_len == 4);
			assert(fetch_mode == GPU_FETCH_INT_TO_FLOAT_UNIT); /* not strictly required, may relax later */
			break;
		default:
			/* integer types can be kept as int or converted/normalized to float */
			assert(fetch_mode != GPU_FETCH_FLOAT);
			/* only support float matrices (see Batch_update_program_bindings) */
			assert(comp_len != 8 && comp_len != 12 && comp_len != 16);
	}
#endif
	format->name_len++; /* multiname support */

	const uint attrib_id = format->attr_len++;
	GPUVertAttr *attrib = format->attribs + attrib_id;

	attrib->name[attrib->name_len++] = copy_attrib_name(format, name, NULL);
	attrib->comp_type = comp_type;
	attrib->gl_comp_type = convert_comp_type_to_gl(comp_type);
	attrib->comp_len = (comp_type == GPU_COMP_I10) ? 4 : comp_len; /* system needs 10_10_10_2 to be 4 or BGRA */
	attrib->sz = attrib_sz(attrib);
	attrib->offset = 0; /* offsets & stride are calculated later (during pack) */
	attrib->fetch_mode = fetch_mode;

	return attrib_id;
}

void GPU_vertformat_alias_add(GPUVertFormat *format, const char *alias)
{
	GPUVertAttr *attrib = format->attribs + (format->attr_len - 1);
#if TRUST_NO_ONE
	assert(format->name_len < GPU_VERT_ATTR_MAX_LEN); /* there's room for more */
	assert(attrib->name_len < GPU_VERT_ATTR_MAX_NAMES);
#endif
	format->name_len++; /* multiname support */
	attrib->name[attrib->name_len++] = copy_attrib_name(format, alias, NULL);
}

int GPU_vertformat_attr_id_get(const GPUVertFormat *format, const char *name)
{
	for (int i = 0; i < format->attr_len; i++) {
		const GPUVertAttr *attrib = format->attribs + i;
		for (int j = 0; j < attrib->name_len; j++) {
			if (STREQ(name, attrib->name[j])) {
				return i;
			}
		}
	}
	return -1;
}

void GPU_vertformat_triple_load(GPUVertFormat *format)
{
#if TRUST_NO_ONE
	assert(!format->packed);
	assert(format->attr_len * 3 < GPU_VERT_ATTR_MAX_LEN);
	assert(format->name_len + format->attr_len * 3 < GPU_VERT_ATTR_MAX_LEN);
#endif

	VertexFormat_pack(format);

	uint old_attr_len = format->attr_len;
	for (uint a_idx = 0; a_idx < old_attr_len; ++a_idx) {
		GPUVertAttr *attrib = format->attribs + a_idx;
		/* Duplicate attrib twice */
		for (int i = 1; i < 3; ++i) {
			GPUVertAttr *dst_attrib = format->attribs + format->attr_len;
			memcpy(dst_attrib, attrib, sizeof(GPUVertAttr));
			/* Increase offset to the next vertex. */
			dst_attrib->offset += format->stride * i;
			/* Only copy first name for now. */
			dst_attrib->name_len = 0;
			dst_attrib->name[dst_attrib->name_len++] = copy_attrib_name(format, attrib->name[0], (i == 1) ? "1" : "2");
			format->attr_len++;
		}

#if TRUST_NO_ONE
		assert(attrib->name_len < GPU_VERT_ATTR_MAX_NAMES);
#endif
		/* Add alias to first attrib. */
		format->name_len++;
		attrib->name[attrib->name_len++] = copy_attrib_name(format, attrib->name[0], "0");
	}
}

uint padding(uint offset, uint alignment)
{
	const uint mod = offset % alignment;
	return (mod == 0) ? 0 : (alignment - mod);
}

#if PACK_DEBUG
static void show_pack(uint a_idx, uint sz, uint pad)
{
	const char c = 'A' + a_idx;
	for (uint i = 0; i < pad; ++i) {
		putchar('-');
	}
	for (uint i = 0; i < sz; ++i) {
		putchar(c);
	}
}
#endif

void VertexFormat_pack(GPUVertFormat *format)
{
	/* For now, attributes are packed in the order they were added,
	 * making sure each attrib is naturally aligned (add padding where necessary)
	 * Later we can implement more efficient packing w/ reordering
	 * (keep attrib ID order, adjust their offsets to reorder in buffer). */

	/* TODO: realloc just enough to hold the final combo string. And just enough to
	 * hold used attribs, not all 16. */

	GPUVertAttr *a0 = format->attribs + 0;
	a0->offset = 0;
	uint offset = a0->sz;

#if PACK_DEBUG
	show_pack(0, a0->sz, 0);
#endif

	for (uint a_idx = 1; a_idx < format->attr_len; ++a_idx) {
		GPUVertAttr *a = format->attribs + a_idx;
		uint mid_padding = padding(offset, attrib_align(a));
		offset += mid_padding;
		a->offset = offset;
		offset += a->sz;

#if PACK_DEBUG
		show_pack(a_idx, a->sz, mid_padding);
#endif
	}

	uint end_padding = padding(offset, attrib_align(a0));

#if PACK_DEBUG
	show_pack(0, 0, end_padding);
	putchar('\n');
#endif
	format->stride = offset + end_padding;
	format->packed = true;
}

static uint calc_input_component_size(const GPUShaderInput *input)
{
	int size = input->size;
	switch (input->gl_type) {
		case GL_FLOAT_VEC2:
		case GL_INT_VEC2:
		case GL_UNSIGNED_INT_VEC2:
			return size * 2;
		case GL_FLOAT_VEC3:
		case GL_INT_VEC3:
		case GL_UNSIGNED_INT_VEC3:
			return size * 3;
		case GL_FLOAT_VEC4:
		case GL_FLOAT_MAT2:
		case GL_INT_VEC4:
		case GL_UNSIGNED_INT_VEC4:
			return size * 4;
		case GL_FLOAT_MAT3:
			return size * 9;
		case GL_FLOAT_MAT4:
			return size * 16;
		case GL_FLOAT_MAT2x3:
		case GL_FLOAT_MAT3x2:
			return size * 6;
		case GL_FLOAT_MAT2x4:
		case GL_FLOAT_MAT4x2:
			return size * 8;
		case GL_FLOAT_MAT3x4:
		case GL_FLOAT_MAT4x3:
			return size * 12;
		default:
			return size;
	}
}

static void get_fetch_mode_and_comp_type(
        int gl_type,
        GPUVertCompType *r_comp_type,
        uint *r_gl_comp_type,
        GPUVertFetchMode *r_fetch_mode)
{
	switch (gl_type) {
		case GL_FLOAT:
		case GL_FLOAT_VEC2:
		case GL_FLOAT_VEC3:
		case GL_FLOAT_VEC4:
		case GL_FLOAT_MAT2:
		case GL_FLOAT_MAT3:
		case GL_FLOAT_MAT4:
		case GL_FLOAT_MAT2x3:
		case GL_FLOAT_MAT2x4:
		case GL_FLOAT_MAT3x2:
		case GL_FLOAT_MAT3x4:
		case GL_FLOAT_MAT4x2:
		case GL_FLOAT_MAT4x3:
			*r_comp_type = GPU_COMP_F32;
			*r_gl_comp_type = GL_FLOAT;
			*r_fetch_mode = GPU_FETCH_FLOAT;
			break;
		case GL_INT:
		case GL_INT_VEC2:
		case GL_INT_VEC3:
		case GL_INT_VEC4:
			*r_comp_type = GPU_COMP_I32;
			*r_gl_comp_type = GL_INT;
			*r_fetch_mode = GPU_FETCH_INT;
			break;
		case GL_UNSIGNED_INT:
		case GL_UNSIGNED_INT_VEC2:
		case GL_UNSIGNED_INT_VEC3:
		case GL_UNSIGNED_INT_VEC4:
			*r_comp_type = GPU_COMP_U32;
			*r_gl_comp_type = GL_UNSIGNED_INT;
			*r_fetch_mode = GPU_FETCH_INT;
			break;
		default:
			BLI_assert(0);
	}
}

void GPU_vertformat_from_interface(GPUVertFormat *format, const GPUShaderInterface *shaderface)
{
	const char *name_buffer = shaderface->name_buffer;

	for (int i = 0; i < GPU_NUM_SHADERINTERFACE_BUCKETS; i++) {
		const GPUShaderInput *input = shaderface->attrib_buckets[i];
		if (input == NULL) {
			continue;
		}

		const GPUShaderInput *next = input;
		while (next != NULL) {
			input = next;
			next = input->next;

			format->name_len++; /* multiname support */
			format->attr_len++;

			GPUVertAttr *attrib = format->attribs + input->location;

			attrib->name[attrib->name_len++] = copy_attrib_name(format, name_buffer + input->name_offset, NULL);
			attrib->offset = 0; /* offsets & stride are calculated later (during pack) */
			attrib->comp_len = calc_input_component_size(input);
			attrib->sz = attrib->comp_len * 4;
			get_fetch_mode_and_comp_type(input->gl_type, &attrib->comp_type, &attrib->gl_comp_type, &attrib->fetch_mode);
		}
	}
}


/* OpenGL ES packs in a different order as desktop GL but component conversion is the same.
 * Of the code here, only struct GPUPackedNormal needs to change. */

#define SIGNED_INT_10_MAX  511
#define SIGNED_INT_10_MIN -512

static int clampi(int x, int min_allowed, int max_allowed)
{
#if TRUST_NO_ONE
	assert(min_allowed <= max_allowed);
#endif
	if (x < min_allowed) {
		return min_allowed;
	}
	else if (x > max_allowed) {
		return max_allowed;
	}
	else {
		return x;
	}
}

static int quantize(float x)
{
	int qx = x * 511.0f;
	return clampi(qx, SIGNED_INT_10_MIN, SIGNED_INT_10_MAX);
}

static int convert_i16(short x)
{
	/* 16-bit signed --> 10-bit signed */
	/* TODO: round? */
	return x >> 6;
}

GPUPackedNormal GPU_normal_convert_i10_v3(const float data[3])
{
	GPUPackedNormal n = { .x = quantize(data[0]), .y = quantize(data[1]), .z = quantize(data[2]) };
	return n;
}

GPUPackedNormal GPU_normal_convert_i10_s3(const short data[3])
{
	GPUPackedNormal n = { .x = convert_i16(data[0]), .y = convert_i16(data[1]), .z = convert_i16(data[2]) };
	return n;
}
