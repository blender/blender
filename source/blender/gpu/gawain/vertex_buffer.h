
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

typedef struct {
	VertexFormat format;
	unsigned vertex_ct;
	GLubyte* data; // NULL indicates data in VRAM (unmapped) or not yet allocated
	GLuint vbo_id; // 0 indicates not yet sent to VRAM
} VertexBuffer;

VertexBuffer* create_VertexBuffer(VertexFormat*, unsigned v_ct); // create means allocate, then init
void init_VertexBuffer(VertexBuffer*, VertexFormat*, unsigned v_ct);

// The most important setAttrib variant is the untyped one. Get it right first.
// It takes a void* so the app developer is responsible for matching their app data types
// to the vertex attribute's type and component count. They're in control of both, so this
// should not be a problem.

void setAttrib(VertexBuffer*, unsigned a_idx, unsigned v_idx, const void* data);
void fillAttrib(VertexBuffer*, unsigned a_idx, const void* data);
void fillAttribStride(VertexBuffer*, unsigned a_idx, unsigned stride, const void* data);

//	void setAttrib1f(unsigned a_idx, unsigned v_idx, float x);
//	void setAttrib2f(unsigned a_idx, unsigned v_idx, float x, float y);
//	void setAttrib3f(unsigned a_idx, unsigned v_idx, float x, float y, float z);
//	void setAttrib4f(unsigned a_idx, unsigned v_idx, float x, float y, float z, float w);
//
//	void setAttrib3ub(unsigned a_idx, unsigned v_idx, unsigned char r, unsigned char g, unsigned char b);
//	void setAttrib4ub(unsigned a_idx, unsigned v_idx, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
