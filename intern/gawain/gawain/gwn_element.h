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

/** \file blender/gpu/gwn_element.h
 *  \ingroup gpu
 *
 * Gawain element list (AKA index buffer)
 */

#ifndef __GWN_ELEMENT_H__
#define __GWN_ELEMENT_H__

#include "gwn_primitive.h"

#define GWN_TRACK_INDEX_RANGE 1

#define GWN_PRIM_RESTART 0xFFFFFFFF

typedef enum {
	GWN_INDEX_U8, /* GL has this, Vulkan does not */
	GWN_INDEX_U16,
	GWN_INDEX_U32
} Gwn_IndexBufType;

typedef struct Gwn_IndexBuf {
	uint index_len;
#if GWN_TRACK_INDEX_RANGE
	Gwn_IndexBufType index_type;
	uint32_t gl_index_type;
	uint min_index;
	uint max_index;
	uint base_index;
#endif
	uint32_t vbo_id; /* 0 indicates not yet sent to VRAM */
	bool use_prim_restart;
} Gwn_IndexBuf;

void GWN_indexbuf_use(Gwn_IndexBuf*);
uint GWN_indexbuf_size_get(const Gwn_IndexBuf*);

typedef struct Gwn_IndexBufBuilder {
	uint max_allowed_index;
	uint max_index_len;
	uint index_len;
	Gwn_PrimType prim_type;
	uint* data;
	bool use_prim_restart;
} Gwn_IndexBufBuilder;


/* supports all primitive types. */
void GWN_indexbuf_init_ex(Gwn_IndexBufBuilder*, Gwn_PrimType, uint index_len, uint vertex_len, bool use_prim_restart);

/* supports only GWN_PRIM_POINTS, GWN_PRIM_LINES and GWN_PRIM_TRIS. */
void GWN_indexbuf_init(Gwn_IndexBufBuilder*, Gwn_PrimType, uint prim_len, uint vertex_len);

void GWN_indexbuf_add_generic_vert(Gwn_IndexBufBuilder*, uint v);
void GWN_indexbuf_add_primitive_restart(Gwn_IndexBufBuilder*);

void GWN_indexbuf_add_point_vert(Gwn_IndexBufBuilder*, uint v);
void GWN_indexbuf_add_line_verts(Gwn_IndexBufBuilder*, uint v1, uint v2);
void GWN_indexbuf_add_tri_verts(Gwn_IndexBufBuilder*, uint v1, uint v2, uint v3);
void GWN_indexbuf_add_line_adj_verts(Gwn_IndexBufBuilder*, uint v1, uint v2, uint v3, uint v4);

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

#endif /* __GWN_ELEMENT_H__ */
