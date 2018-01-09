
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

typedef struct Gwn_Batch {
	// geometry
	Gwn_VertBuf* verts[GWN_BATCH_VBO_MAX_LEN]; // verts[0] is required, others can be NULL
	Gwn_IndexBuf* elem; // NULL if element list not needed
	Gwn_PrimType prim_type;
	GLenum gl_prim_type;

	// book-keeping
	GLuint vao_id; // remembers all geometry state (vertex attrib bindings & element buffer)
	Gwn_BatchPhase phase;
	bool program_dirty;
	bool program_in_use;
	unsigned owns_flag;

	// state
	GLuint program;
	const Gwn_ShaderInterface* interface;
} Gwn_Batch;

enum {
	GWN_BATCH_OWNS_VBO = (1 << 0),
	/* each vbo index gets bit-shifted */
	GWN_BATCH_OWNS_INDEX = (1 << 31),
};

Gwn_Batch* GWN_batch_create_ex(Gwn_PrimType, Gwn_VertBuf*, Gwn_IndexBuf*, unsigned owns_flag);
void GWN_batch_init_ex(Gwn_Batch*, Gwn_PrimType, Gwn_VertBuf*, Gwn_IndexBuf*, unsigned owns_flag);

#define GWN_batch_create(prim, verts, elem) \
	GWN_batch_create_ex(prim, verts, elem, 0)
#define GWN_batch_init(batch, prim, verts, elem) \
	GWN_batch_init_ex(batch, prim, verts, elem, 0)

void GWN_batch_discard(Gwn_Batch*); // verts & elem are not discarded

int GWN_batch_vertbuf_add_ex(Gwn_Batch*, Gwn_VertBuf*, bool own_vbo);

#define GWN_batch_vertbuf_add(batch, verts) \
	GWN_batch_vertbuf_add_ex(batch, verts, false)

void GWN_batch_program_set(Gwn_Batch*, GLuint program, const Gwn_ShaderInterface*);
void GWN_batch_program_unset(Gwn_Batch*);
// Entire batch draws with one shader program, but can be redrawn later with another program.
// Vertex shader's inputs must be compatible with the batch's vertex format.

void GWN_batch_program_use_begin(Gwn_Batch*); // call before Batch_Uniform (temp hack?)
void GWN_batch_program_use_end(Gwn_Batch*);

void GWN_batch_uniform_1i(Gwn_Batch*, const char* name, int value);
void GWN_batch_uniform_1b(Gwn_Batch*, const char* name, bool value);
void GWN_batch_uniform_1f(Gwn_Batch*, const char* name, float value);
void GWN_batch_uniform_2f(Gwn_Batch*, const char* name, float x, float y);
void GWN_batch_uniform_3f(Gwn_Batch*, const char* name, float x, float y, float z);
void GWN_batch_uniform_4f(Gwn_Batch*, const char* name, float x, float y, float z, float w);
void GWN_batch_uniform_2fv(Gwn_Batch*, const char* name, const float data[2]);
void GWN_batch_uniform_3fv(Gwn_Batch*, const char* name, const float data[3]);
void GWN_batch_uniform_4fv(Gwn_Batch*, const char* name, const float data[4]);

void GWN_batch_draw(Gwn_Batch*);


// clement : temp stuff
void GWN_batch_draw_stupid(Gwn_Batch*, int v_first, int v_count);
void GWN_batch_draw_stupid_instanced(Gwn_Batch*, unsigned int instance_vbo, int instance_first, int instance_count,
                                 int attrib_nbr, int attrib_stride, int attrib_loc[16], int attrib_size[16]);
void GWN_batch_draw_stupid_instanced_with_batch(Gwn_Batch*, Gwn_Batch*);





#if 0 // future plans

// Can multiple batches share a Gwn_VertBuf? Use ref count?


// We often need a batch with its own data, to be created and discarded together.
// WithOwn variants reduce number of system allocations.

typedef struct BatchWithOwnVertexBuffer {
	Gwn_Batch batch;
	Gwn_VertBuf verts; // link batch.verts to this
} BatchWithOwnVertexBuffer;

typedef struct BatchWithOwnElementList {
	Gwn_Batch batch;
	Gwn_IndexBuf elem; // link batch.elem to this
} BatchWithOwnElementList;

typedef struct BatchWithOwnVertexBufferAndElementList {
	Gwn_Batch batch;
	Gwn_IndexBuf elem; // link batch.elem to this
	Gwn_VertBuf verts; // link batch.verts to this
} BatchWithOwnVertexBufferAndElementList;

Gwn_Batch* create_BatchWithOwnVertexBuffer(Gwn_PrimType, Gwn_VertFormat*, unsigned v_ct, Gwn_IndexBuf*);
Gwn_Batch* create_BatchWithOwnElementList(Gwn_PrimType, Gwn_VertBuf*, unsigned prim_ct);
Gwn_Batch* create_BatchWithOwnVertexBufferAndElementList(Gwn_PrimType, Gwn_VertFormat*, unsigned v_ct, unsigned prim_ct);
// verts: shared, own
// elem: none, shared, own
Gwn_Batch* create_BatchInGeneral(Gwn_PrimType, VertexBufferStuff, ElementListStuff);

#endif // future plans


/* Macros */

#define GWN_BATCH_DISCARD_SAFE(batch) do { \
	if (batch != NULL) { \
		GWN_batch_discard(batch); \
		batch = NULL; \
	} \
} while (0)
