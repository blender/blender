
// Gawain vertex buffer
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "gwn_vertex_format.h"

// How to create a Gwn_VertBuf:
// 1) verts = GWN_vertbuf_create() or GWN_vertbuf_init(verts)
// 2) GWN_vertformat_attr_add(verts->format, ...)
// 3) GWN_vertbuf_data_alloc(verts, vertex_ct) <-- finalizes/packs vertex format
// 4) GWN_vertbuf_attr_fill(verts, pos, application_pos_buffer)

// Is Gwn_VertBuf always used as part of a Gwn_Batch?

typedef struct Gwn_VertBuf {
	Gwn_VertFormat format;
	unsigned vertex_ct;
	bool own_data; // does gawain own the data an is able to free it
	GLubyte* data; // NULL indicates data in VRAM (unmapped) or not yet allocated
	GLuint vbo_id; // 0 indicates not yet sent to VRAM
} Gwn_VertBuf;

Gwn_VertBuf* GWN_vertbuf_create(void);
Gwn_VertBuf* GWN_vertbuf_create_with_format(const Gwn_VertFormat*);

void GWN_vertbuf_clear(Gwn_VertBuf* verts);
void GWN_vertbuf_discard(Gwn_VertBuf*);

void GWN_vertbuf_init(Gwn_VertBuf*);
void GWN_vertbuf_init_with_format(Gwn_VertBuf*, const Gwn_VertFormat*);

unsigned GWN_vertbuf_size_get(const Gwn_VertBuf*);
void GWN_vertbuf_data_alloc(Gwn_VertBuf*, unsigned v_ct);
void GWN_vertbuf_data_set(Gwn_VertBuf*, unsigned v_ct, void* data, bool pass_ownership);
void GWN_vertbuf_data_resize(Gwn_VertBuf*, unsigned v_ct);

// The most important set_attrib variant is the untyped one. Get it right first.
// It takes a void* so the app developer is responsible for matching their app data types
// to the vertex attribute's type and component count. They're in control of both, so this
// should not be a problem.

void GWN_vertbuf_attr_set(Gwn_VertBuf*, unsigned a_idx, unsigned v_idx, const void* data);
void GWN_vertbuf_attr_fill(Gwn_VertBuf*, unsigned a_idx, const void* data); // tightly packed, non interleaved input data
void GWN_vertbuf_attr_fill_stride(Gwn_VertBuf*, unsigned a_idx, unsigned stride, const void* data);

// For low level access only
typedef struct Gwn_VertBufRaw {
	unsigned size;
	unsigned stride;
	GLubyte* data;
	GLubyte* data_init;
#if TRUST_NO_ONE
	// Only for overflow check
	GLubyte* _data_end;
#endif
} Gwn_VertBufRaw;

GWN_INLINE void *GWN_vertbuf_raw_step(Gwn_VertBufRaw *a)
	{
	GLubyte* data = a->data;
	a->data += a->stride;
#if TRUST_NO_ONE
	assert(data < a->_data_end);
#endif
	return (void *)data;
	}

GWN_INLINE unsigned GWN_vertbuf_raw_used(Gwn_VertBufRaw *a)
	{
	return ((a->data - a->data_init) / a->stride);
	}

void GWN_vertbuf_attr_get_raw_data(Gwn_VertBuf*, unsigned a_idx, Gwn_VertBufRaw *access);

// TODO: decide whether to keep the functions below
// doesn't immediate mode satisfy these needs?

//	void setAttrib1f(unsigned a_idx, unsigned v_idx, float x);
//	void setAttrib2f(unsigned a_idx, unsigned v_idx, float x, float y);
//	void setAttrib3f(unsigned a_idx, unsigned v_idx, float x, float y, float z);
//	void setAttrib4f(unsigned a_idx, unsigned v_idx, float x, float y, float z, float w);
//
//	void setAttrib3ub(unsigned a_idx, unsigned v_idx, unsigned char r, unsigned char g, unsigned char b);
//	void setAttrib4ub(unsigned a_idx, unsigned v_idx, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

void GWN_vertbuf_use(Gwn_VertBuf*);


// Metrics

unsigned GWN_vertbuf_get_memory_usage(void);


// Macros

#define GWN_VERTBUF_DISCARD_SAFE(verts) do { \
	if (verts != NULL) { \
		GWN_vertbuf_discard(verts); \
		verts = NULL; \
	} \
} while (0)
