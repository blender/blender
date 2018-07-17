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

/** \file blender/gpu/gwn_batch.h
 *  \ingroup gpu
 *
 * Gawain geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#ifndef __GWN_BATCH_H__
#define __GWN_BATCH_H__

#include "gwn_vertex_buffer.h"
#include "gwn_element.h"
#include "gwn_shader_interface.h"

typedef enum {
	GWN_BATCH_READY_TO_FORMAT,
	GWN_BATCH_READY_TO_BUILD,
	GWN_BATCH_BUILDING,
	GWN_BATCH_READY_TO_DRAW
} Gwn_BatchPhase;

#define GWN_BATCH_VBO_MAX_LEN 3
#define GWN_BATCH_VAO_STATIC_LEN 3
#define GWN_BATCH_VAO_DYN_ALLOC_COUNT 16

typedef struct Gwn_Batch {
	/* geometry */
	Gwn_VertBuf* verts[GWN_BATCH_VBO_MAX_LEN]; /* verts[0] is required, others can be NULL */
	Gwn_VertBuf* inst; /* instance attribs */
	Gwn_IndexBuf* elem; /* NULL if element list not needed */
	uint32_t gl_prim_type;

	/* cached values (avoid dereferencing later) */
	uint32_t vao_id;
	uint32_t program;
	const struct Gwn_ShaderInterface* interface;

	/* book-keeping */
	uint owns_flag;
	struct Gwn_Context *context; /* used to free all vaos. this implies all vaos were created under the same context. */
	Gwn_BatchPhase phase;
	bool program_in_use;

	/* Vao management: remembers all geometry state (vertex attrib bindings & element buffer)
	 * for each shader interface. Start with a static number of vaos and fallback to dynamic count
	 * if necessary. Once a batch goes dynamic it does not go back. */
	bool is_dynamic_vao_count;
	union {
		/* Static handle count */
		struct {
			const struct Gwn_ShaderInterface* interfaces[GWN_BATCH_VAO_STATIC_LEN];
			uint32_t vao_ids[GWN_BATCH_VAO_STATIC_LEN];
		} static_vaos;
		/* Dynamic handle count */
		struct {
			uint count;
			const struct Gwn_ShaderInterface** interfaces;
			uint32_t* vao_ids;
		} dynamic_vaos;
	};

	/* XXX This is the only solution if we want to have some data structure using
	 * batches as key to identify nodes. We must destroy these nodes with this callback. */
	void (*free_callback)(struct Gwn_Batch*, void*);
	void* callback_data;
} Gwn_Batch;

enum {
	GWN_BATCH_OWNS_VBO = (1 << 0),
	/* each vbo index gets bit-shifted */
	GWN_BATCH_OWNS_INSTANCES = (1 << 30),
	GWN_BATCH_OWNS_INDEX = (1 << 31),
};

Gwn_Batch* GWN_batch_create_ex(Gwn_PrimType, Gwn_VertBuf*, Gwn_IndexBuf*, uint owns_flag);
void GWN_batch_init_ex(Gwn_Batch*, Gwn_PrimType, Gwn_VertBuf*, Gwn_IndexBuf*, uint owns_flag);
Gwn_Batch* GWN_batch_duplicate(Gwn_Batch* batch_src);

#define GWN_batch_create(prim, verts, elem) \
	GWN_batch_create_ex(prim, verts, elem, 0)
#define GWN_batch_init(batch, prim, verts, elem) \
	GWN_batch_init_ex(batch, prim, verts, elem, 0)

void GWN_batch_discard(Gwn_Batch*); /* verts & elem are not discarded */

void GWN_batch_callback_free_set(Gwn_Batch*, void (*callback)(Gwn_Batch*, void*), void*);

void GWN_batch_instbuf_set(Gwn_Batch*, Gwn_VertBuf*, bool own_vbo); /* Instancing */

int GWN_batch_vertbuf_add_ex(Gwn_Batch*, Gwn_VertBuf*, bool own_vbo);

#define GWN_batch_vertbuf_add(batch, verts) \
	GWN_batch_vertbuf_add_ex(batch, verts, false)

void GWN_batch_program_set_no_use(Gwn_Batch*, uint32_t program, const Gwn_ShaderInterface*);
void GWN_batch_program_set(Gwn_Batch*, uint32_t program, const Gwn_ShaderInterface*);
/* Entire batch draws with one shader program, but can be redrawn later with another program. */
/* Vertex shader's inputs must be compatible with the batch's vertex format. */

void GWN_batch_program_use_begin(Gwn_Batch*); /* call before Batch_Uniform (temp hack?) */
void GWN_batch_program_use_end(Gwn_Batch*);

void GWN_batch_uniform_1ui(Gwn_Batch*, const char* name, int value);
void GWN_batch_uniform_1i(Gwn_Batch*, const char* name, int value);
void GWN_batch_uniform_1b(Gwn_Batch*, const char* name, bool value);
void GWN_batch_uniform_1f(Gwn_Batch*, const char* name, float value);
void GWN_batch_uniform_2f(Gwn_Batch*, const char* name, float x, float y);
void GWN_batch_uniform_3f(Gwn_Batch*, const char* name, float x, float y, float z);
void GWN_batch_uniform_4f(Gwn_Batch*, const char* name, float x, float y, float z, float w);
void GWN_batch_uniform_2fv(Gwn_Batch*, const char* name, const float data[2]);
void GWN_batch_uniform_3fv(Gwn_Batch*, const char* name, const float data[3]);
void GWN_batch_uniform_4fv(Gwn_Batch*, const char* name, const float data[4]);
void GWN_batch_uniform_2fv_array(Gwn_Batch*, const char* name, int len, const float *data);
void GWN_batch_uniform_4fv_array(Gwn_Batch*, const char* name, int len, const float *data);
void GWN_batch_uniform_mat4(Gwn_Batch*, const char* name, const float data[4][4]);

void GWN_batch_draw(Gwn_Batch*);

/* This does not bind/unbind shader and does not call GPU_matrix_bind() */
void GWN_batch_draw_range_ex(Gwn_Batch*, int v_first, int v_count, bool force_instance);

/* Does not even need batch */
void GWN_draw_primitive(Gwn_PrimType, int v_count);

#if 0 /* future plans */

/* Can multiple batches share a Gwn_VertBuf? Use ref count? */


/* We often need a batch with its own data, to be created and discarded together. */
/* WithOwn variants reduce number of system allocations. */

typedef struct BatchWithOwnVertexBuffer {
	Gwn_Batch batch;
	Gwn_VertBuf verts; /* link batch.verts to this */
} BatchWithOwnVertexBuffer;

typedef struct BatchWithOwnElementList {
	Gwn_Batch batch;
	Gwn_IndexBuf elem; /* link batch.elem to this */
} BatchWithOwnElementList;

typedef struct BatchWithOwnVertexBufferAndElementList {
	Gwn_Batch batch;
	Gwn_IndexBuf elem; /* link batch.elem to this */
	Gwn_VertBuf verts; /* link batch.verts to this */
} BatchWithOwnVertexBufferAndElementList;

Gwn_Batch* create_BatchWithOwnVertexBuffer(Gwn_PrimType, Gwn_VertFormat*, uint v_len, Gwn_IndexBuf*);
Gwn_Batch* create_BatchWithOwnElementList(Gwn_PrimType, Gwn_VertBuf*, uint prim_len);
Gwn_Batch* create_BatchWithOwnVertexBufferAndElementList(Gwn_PrimType, Gwn_VertFormat*, uint v_len, uint prim_len);
/* verts: shared, own */
/* elem: none, shared, own */
Gwn_Batch* create_BatchInGeneral(Gwn_PrimType, VertexBufferStuff, ElementListStuff);

#endif /* future plans */


/* Macros */

#define GWN_BATCH_DISCARD_SAFE(batch) do { \
	if (batch != NULL) { \
		GWN_batch_discard(batch); \
		batch = NULL; \
	} \
} while (0)

#endif /* __GWN_BATCH_H__ */
