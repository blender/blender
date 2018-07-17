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

/** \file blender/gpu/intern/gwn_vertex_format.c
 *  \ingroup gpu
 *
 * Gawain vertex format
 */

#include "GPU_vertex_format.h"
#include "gpu_vertex_format_private.h"
#include <stddef.h>
#include <string.h>

#define PACK_DEBUG 0

#if PACK_DEBUG
  #include <stdio.h>
#endif

void GWN_vertformat_clear(Gwn_VertFormat* format)
{
#if TRUST_NO_ONE
	memset(format, 0, sizeof(Gwn_VertFormat));
#else
	format->attr_len = 0;
	format->packed = false;
	format->name_offset = 0;
	format->name_len = 0;

	for (unsigned i = 0; i < GWN_VERT_ATTR_MAX_LEN; i++) {
		format->attribs[i].name_len = 0;
	}
#endif
}

void GWN_vertformat_copy(Gwn_VertFormat* dest, const Gwn_VertFormat* src)
{
	/* copy regular struct fields */
	memcpy(dest, src, sizeof(Gwn_VertFormat));

	for (unsigned i = 0; i < dest->attr_len; i++) {
		for (unsigned j = 0; j < dest->attribs[i].name_len; j++) {
			dest->attribs[i].name[j] = (char *)dest + (src->attribs[i].name[j] - ((char *)src));
		}
	}
}

static GLenum convert_comp_type_to_gl(Gwn_VertCompType type)
{
	static const GLenum table[] = {
		[GWN_COMP_I8] = GL_BYTE,
		[GWN_COMP_U8] = GL_UNSIGNED_BYTE,
		[GWN_COMP_I16] = GL_SHORT,
		[GWN_COMP_U16] = GL_UNSIGNED_SHORT,
		[GWN_COMP_I32] = GL_INT,
		[GWN_COMP_U32] = GL_UNSIGNED_INT,

		[GWN_COMP_F32] = GL_FLOAT,

		[GWN_COMP_I10] = GL_INT_2_10_10_10_REV
	};
	return table[type];
}

static unsigned comp_sz(Gwn_VertCompType type)
{
#if TRUST_NO_ONE
	assert(type <= GWN_COMP_F32); /* other types have irregular sizes (not bytes) */
#endif
	const GLubyte sizes[] = {1,1,2,2,4,4,4};
	return sizes[type];
}

static unsigned attrib_sz(const Gwn_VertAttr *a)
{
	if (a->comp_type == GWN_COMP_I10) {
		return 4; /* always packed as 10_10_10_2 */
	}
	return a->comp_len * comp_sz(a->comp_type);
}

static unsigned attrib_align(const Gwn_VertAttr *a)
{
	if (a->comp_type == GWN_COMP_I10) {
		return 4; /* always packed as 10_10_10_2 */
	}
	unsigned c = comp_sz(a->comp_type);
	if (a->comp_len == 3 && c <= 2) {
		return 4 * c; /* AMD HW can't fetch these well, so pad it out (other vendors too?) */
	}
	else {
		return c; /* most fetches are ok if components are naturally aligned */
	}
}

unsigned vertex_buffer_size(const Gwn_VertFormat* format, unsigned vertex_len)
{
#if TRUST_NO_ONE
	assert(format->packed && format->stride > 0);
#endif
	return format->stride * vertex_len;
}

static const char* copy_attrib_name(Gwn_VertFormat* format, const char* name)
{
	/* strncpy does 110% of what we need; let's do exactly 100% */
	char* name_copy = format->names + format->name_offset;
	unsigned available = GWN_VERT_ATTR_NAMES_BUF_LEN - format->name_offset;
	bool terminated = false;

	for (unsigned i = 0; i < available; ++i) {
		const char c = name[i];
		name_copy[i] = c;
		if (c == '\0') {
			terminated = true;
			format->name_offset += (i + 1);
			break;
		}
	}
#if TRUST_NO_ONE
	assert(terminated);
	assert(format->name_offset <= GWN_VERT_ATTR_NAMES_BUF_LEN);
#else
	(void)terminated;
#endif
	return name_copy;
}

unsigned GWN_vertformat_attr_add(Gwn_VertFormat* format, const char* name, Gwn_VertCompType comp_type, unsigned comp_len, Gwn_VertFetchMode fetch_mode)
{
#if TRUST_NO_ONE
	assert(format->name_len < GWN_VERT_ATTR_MAX_LEN); /* there's room for more */
	assert(format->attr_len < GWN_VERT_ATTR_MAX_LEN); /* there's room for more */
	assert(!format->packed); /* packed means frozen/locked */
	assert((comp_len >= 1 && comp_len <= 4) || comp_len == 8 || comp_len == 12 || comp_len == 16);

	switch (comp_type) {
		case GWN_COMP_F32:
			/* float type can only kept as float */
			assert(fetch_mode == GWN_FETCH_FLOAT);
			break;
		case GWN_COMP_I10:
			/* 10_10_10 format intended for normals (xyz) or colors (rgb)
			 * extra component packed.w can be manually set to { -2, -1, 0, 1 } */
			assert(comp_len == 3 || comp_len == 4);
			assert(fetch_mode == GWN_FETCH_INT_TO_FLOAT_UNIT); /* not strictly required, may relax later */
			break;
		default:
			/* integer types can be kept as int or converted/normalized to float */
			assert(fetch_mode != GWN_FETCH_FLOAT);
			/* only support float matrices (see Batch_update_program_bindings) */
			assert(comp_len != 8 && comp_len != 12 && comp_len != 16);
	}
#endif
	format->name_len++; /* multiname support */

	const unsigned attrib_id = format->attr_len++;
	Gwn_VertAttr* attrib = format->attribs + attrib_id;

	attrib->name[attrib->name_len++] = copy_attrib_name(format, name);
	attrib->comp_type = comp_type;
	attrib->gl_comp_type = convert_comp_type_to_gl(comp_type);
	attrib->comp_len = (comp_type == GWN_COMP_I10) ? 4 : comp_len; /* system needs 10_10_10_2 to be 4 or BGRA */
	attrib->sz = attrib_sz(attrib);
	attrib->offset = 0; /* offsets & stride are calculated later (during pack) */
	attrib->fetch_mode = fetch_mode;

	return attrib_id;
}

void GWN_vertformat_alias_add(Gwn_VertFormat* format, const char* alias)
{
	Gwn_VertAttr* attrib = format->attribs + (format->attr_len - 1);
#if TRUST_NO_ONE
	assert(format->name_len < GWN_VERT_ATTR_MAX_LEN); /* there's room for more */
	assert(attrib->name_len < GWN_VERT_ATTR_MAX_NAMES);
#endif
	format->name_len++; /* multiname support */
	attrib->name[attrib->name_len++] = copy_attrib_name(format, alias);
}

unsigned padding(unsigned offset, unsigned alignment)
{
	const unsigned mod = offset % alignment;
	return (mod == 0) ? 0 : (alignment - mod);
}

#if PACK_DEBUG
static void show_pack(unsigned a_idx, unsigned sz, unsigned pad)
{
	const char c = 'A' + a_idx;
	for (unsigned i = 0; i < pad; ++i) {
		putchar('-');
	}
	for (unsigned i = 0; i < sz; ++i) {
		putchar(c);
	}
}
#endif

void VertexFormat_pack(Gwn_VertFormat* format)
{
	/* For now, attributes are packed in the order they were added,
	 * making sure each attrib is naturally aligned (add padding where necessary)
	 * Later we can implement more efficient packing w/ reordering
	 * (keep attrib ID order, adjust their offsets to reorder in buffer). */

	/* TODO: realloc just enough to hold the final combo string. And just enough to
	 * hold used attribs, not all 16. */

	Gwn_VertAttr* a0 = format->attribs + 0;
	a0->offset = 0;
	unsigned offset = a0->sz;

#if PACK_DEBUG
	show_pack(0, a0->sz, 0);
#endif

	for (unsigned a_idx = 1; a_idx < format->attr_len; ++a_idx) {
		Gwn_VertAttr* a = format->attribs + a_idx;
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


/* OpenGL ES packs in a different order as desktop GL but component conversion is the same.
 * Of the code here, only struct Gwn_PackedNormal needs to change. */

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

Gwn_PackedNormal GWN_normal_convert_i10_v3(const float data[3])
{
	Gwn_PackedNormal n = { .x = quantize(data[0]), .y = quantize(data[1]), .z = quantize(data[2]) };
	return n;
}

Gwn_PackedNormal GWN_normal_convert_i10_s3(const short data[3])
{
	Gwn_PackedNormal n = { .x = convert_i16(data[0]), .y = convert_i16(data[1]), .z = convert_i16(data[2]) };
	return n;
}
