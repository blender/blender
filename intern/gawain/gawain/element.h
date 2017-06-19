
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

#include "primitive.h"

#define GWN_TRACK_INDEX_RANGE 1

typedef enum {
	GWN_INDEX_U8, // GL has this, Vulkan does not
	GWN_INDEX_U16,
	GWN_INDEX_U32
} Gwn_IndexBufType;

typedef struct {
	unsigned index_ct;
#if GWN_TRACK_INDEX_RANGE
	Gwn_IndexBufType index_type;
	GLenum gl_index_type;
	unsigned min_index;
	unsigned max_index;
	unsigned base_index;
#endif
	void* data; // NULL indicates data in VRAM (unmapped) or not yet allocated
	GLuint vbo_id; // 0 indicates not yet sent to VRAM
} Gwn_IndexBuf;

void GWN_indexbuf_use(Gwn_IndexBuf*);
unsigned GWN_indexbuf_size_get(const Gwn_IndexBuf*);

typedef struct {
	unsigned max_allowed_index;
	unsigned max_index_ct;
	unsigned index_ct;
	Gwn_PrimType prim_type;
	unsigned* data;
} Gwn_IndexBufBuilder;

// supported primitives:
//  GWN_PRIM_POINTS
//  GWN_PRIM_LINES
//  GWN_PRIM_TRIS

void GWN_indexbuf_init(Gwn_IndexBufBuilder*, Gwn_PrimType, unsigned prim_ct, unsigned vertex_ct);
//void GWN_indexbuf_init_custom(Gwn_IndexBufBuilder*, Gwn_PrimType, unsigned index_ct, unsigned vertex_ct);

void GWN_indexbuf_add_generic_vert(Gwn_IndexBufBuilder*, unsigned v);

void GWN_indexbuf_add_point_vert(Gwn_IndexBufBuilder*, unsigned v);
void GWN_indexbuf_add_line_verts(Gwn_IndexBufBuilder*, unsigned v1, unsigned v2);
void GWN_indexbuf_add_tri_verts(Gwn_IndexBufBuilder*, unsigned v1, unsigned v2, unsigned v3);

Gwn_IndexBuf* GWN_indexbuf_build(Gwn_IndexBufBuilder*);
void GWN_indexbuf_build_in_place(Gwn_IndexBufBuilder*, Gwn_IndexBuf*);

void GWN_indexbuf_discard(Gwn_IndexBuf*);


/* Macros */

#define GWN_INDEXBUF_DISCARD_SAFE(elem) do { \
	if (elem != NULL) { \
		GWN_indexbuf_discard(elem); \
		elem = NULL; \
	} \
} while (0)
