
// Gawain element list (AKA index buffer)
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "common.h"

#define TRACK_INDEX_RANGE 1

typedef struct {
	unsigned index_ct;
#if TRACK_INDEX_RANGE
	GLenum index_type;
	unsigned min_index;
	unsigned max_index;
	unsigned base_index;
#endif
	void* data; // NULL indicates data in VRAM (unmapped) or not yet allocated
	GLuint vbo_id; // 0 indicates not yet sent to VRAM
} ElementList;

void ElementList_use(ElementList*);
void ElementList_done_using(void);
unsigned ElementList_size(const ElementList*);

typedef struct {
	unsigned max_allowed_index;
	unsigned max_index_ct;
	unsigned index_ct;
	GLenum prim_type;
	unsigned* data;
} ElementListBuilder;

// supported primitives:
//  GL_POINTS
//  GL_LINES
//  GL_TRIANGLES

void init_ElementListBuilder(ElementListBuilder*, GLenum prim_type, unsigned prim_ct, unsigned vertex_ct);
//void init_CustomElementListBuilder(ElementListBuilder*, GLenum prim_type, unsigned index_ct, unsigned vertex_ct);

void add_generic_vertex(ElementListBuilder*, unsigned v);

void add_point_vertex(ElementListBuilder*, unsigned v);
void add_line_vertices(ElementListBuilder*, unsigned v1, unsigned v2);
void add_triangle_vertices(ElementListBuilder*, unsigned v1, unsigned v2, unsigned v3);

void build_ElementList(ElementListBuilder*, ElementList*);
