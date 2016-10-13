
// Gawain vertex format
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "vertex_format.h"
#include <stdlib.h>
#include <string.h>

#define PACK_DEBUG 0

#if PACK_DEBUG
  #include <stdio.h>
#endif

void VertexFormat_clear(VertexFormat* format)
	{
#if TRUST_NO_ONE
	memset(format, 0, sizeof(VertexFormat));
#else
	format->attrib_ct = 0;
	format->packed = false;
	format->name_offset = 0;
#endif
	}

void VertexFormat_copy(VertexFormat* dest, const VertexFormat* src)
	{
	// copy regular struct fields
	memcpy(dest, src, sizeof(VertexFormat));
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

unsigned vertex_buffer_size(const VertexFormat* format, unsigned vertex_ct)
	{
#if TRUST_NO_ONE
	assert(format->packed && format->stride > 0);
#endif

	return format->stride * vertex_ct;
	}

static const char* copy_attrib_name(VertexFormat* format, const char* name)
	{
	// strncpy does 110% of what we need; let's do exactly 100%
	char* name_copy = format->names + format->name_offset;
	unsigned available = VERTEX_ATTRIB_NAMES_BUFFER_LEN - format->name_offset;
	bool terminated = false;

	for (unsigned i = 0; i < available; ++i)
		{
		const char c = name[i];
		name_copy[i] = c;
		if (c == '\0')
			{
			terminated = true;
			format->name_offset += (i + 1);
			break;
			}
		}

#if TRUST_NO_ONE
	assert(terminated);
	assert(format->name_offset <= VERTEX_ATTRIB_NAMES_BUFFER_LEN);
#endif

	return name_copy;
	}

unsigned add_attrib(VertexFormat* format, const char* name, GLenum comp_type, unsigned comp_ct, VertexFetchMode fetch_mode)
	{
#if TRUST_NO_ONE
	assert(format->attrib_ct < MAX_VERTEX_ATTRIBS); // there's room for more
	assert(!format->packed); // packed means frozen/locked
	assert(comp_ct >= 1 && comp_ct <= 4);
	switch (comp_type)
		{
		case GL_FLOAT:
			// float type can only kept as float
			assert(fetch_mode == KEEP_FLOAT);
			break;
	#if 0 // enable this after switching to our own enum for comp_type
		default:
			// integer types can be kept as int or converted/normalized to float
			assert(fetch_mode != KEEP_FLOAT);
	#else
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
		case GL_INT:
		case GL_UNSIGNED_INT:
			// integer types can be converted, normalized, or kept as int
			assert(fetch_mode != KEEP_FLOAT);
			break;
		default:
			assert(false); // invalid comp_type
	#endif
		}
#endif

	const unsigned attrib_id = format->attrib_ct++;
	Attrib* attrib = format->attribs + attrib_id;

	attrib->name = copy_attrib_name(format, name);
	attrib->comp_type = comp_type;
	attrib->comp_ct = comp_ct;
	attrib->sz = attrib_sz(attrib);
	attrib->offset = 0; // offsets & stride are calculated later (during pack)
	attrib->fetch_mode = fetch_mode;

	return attrib_id;
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
	for (unsigned i = 0; i < pad; ++i)
		putchar('-');
	for (unsigned i = 0; i < sz; ++i)
		putchar(c);
	}
#endif

void VertexFormat_pack(VertexFormat* format)
	{
	// for now, attributes are packed in the order they were added,
	// making sure each attrib is naturally aligned (add padding where necessary)

	// later we can implement more efficient packing w/ reordering
	// (keep attrib ID order, adjust their offsets to reorder in buffer)

	// TODO:
	// realloc just enough to hold the final combo string. And just enough to
	// hold used attribs, not all 16.

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
