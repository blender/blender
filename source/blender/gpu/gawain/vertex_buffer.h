
// Gawain geometry batch
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "vertex_format.h"

// How to create a VertexBuffer:
// 1) verts = create_VertexBuffer() or init_VertexBuffer(verts)
// 2) add_attrib(verts->format, ...)
// 3) allocate_vertex_data(verts, vertex_ct) <-- finalizes/packs vertex format
// 4) fillAttrib(verts, pos, application_pos_buffer)
// 5) prime_VertexBuffer(verts);

// Is VertexBuffer always used as part of a Batch?

typedef struct {
	VertexFormat format;
	unsigned vertex_ct;
	GLubyte* data; // NULL indicates data in VRAM (unmapped) or not yet allocated
	GLuint vbo_id; // 0 indicates not yet sent to VRAM
} VertexBuffer;

VertexBuffer* VertexBuffer_create(void); // create means allocate, then init
void VertexBuffer_init(VertexBuffer*);

// TODO: use copy of existing format
// void init_VertexBuffer_with_format(VertexBuffer*, VertexFormat*);

void VertexBuffer_allocate_data(VertexBuffer*, unsigned v_ct);

// The most important setAttrib variant is the untyped one. Get it right first.
// It takes a void* so the app developer is responsible for matching their app data types
// to the vertex attribute's type and component count. They're in control of both, so this
// should not be a problem.

void setAttrib(VertexBuffer*, unsigned a_idx, unsigned v_idx, const void* data);
void fillAttrib(VertexBuffer*, unsigned a_idx, const void* data); // tightly packed, non interleaved input data
void fillAttribStride(VertexBuffer*, unsigned a_idx, unsigned stride, const void* data);

// TODO: decide whether to keep the functions below
// doesn't immediate mode satisfy these needs?

//	void setAttrib1f(unsigned a_idx, unsigned v_idx, float x);
//	void setAttrib2f(unsigned a_idx, unsigned v_idx, float x, float y);
//	void setAttrib3f(unsigned a_idx, unsigned v_idx, float x, float y, float z);
//	void setAttrib4f(unsigned a_idx, unsigned v_idx, float x, float y, float z, float w);
//
//	void setAttrib3ub(unsigned a_idx, unsigned v_idx, unsigned char r, unsigned char g, unsigned char b);
//	void setAttrib4ub(unsigned a_idx, unsigned v_idx, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

void VertexBuffer_use(VertexBuffer*);
