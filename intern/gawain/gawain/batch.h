
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

#include "vertex_buffer.h"
#include "element.h"
#include "shader_interface.h"

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

	// state
	GLuint program;
	const Gwn_ShaderInterface* interface;
} Gwn_Batch;

Gwn_Batch* GWN_batch_create(Gwn_PrimType, Gwn_VertBuf*, Gwn_IndexBuf*);
void GWN_batch_init(Gwn_Batch*, Gwn_PrimType, Gwn_VertBuf*, Gwn_IndexBuf*);

void GWN_batch_discard(Gwn_Batch*); // verts & elem are not discarded
void GWN_batch_discard_all(Gwn_Batch*); // including verts & elem

int GWN_batch_vertbuf_add(Gwn_Batch*, Gwn_VertBuf*);

void GWN_batch_program_set(Gwn_Batch*, GLuint program, const Gwn_ShaderInterface*);
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
void GWN_batch_uniform_3fv(Gwn_Batch*, const char* name, const float data[3]);
void GWN_batch_uniform_4fv(Gwn_Batch*, const char* name, const float data[4]);

void GWN_batch_draw(Gwn_Batch*);


// clement : temp stuff
void GWN_batch_draw_stupid(Gwn_Batch*);
void GWN_batch_draw_stupid_instanced(Gwn_Batch*, unsigned int instance_vbo, int instance_count,
                                 int attrib_nbr, int attrib_stride, int attrib_loc[16], int attrib_size[16]);
void GWN_batch_draw_stupid_instanced_with_batch(Gwn_Batch*, Gwn_Batch*);





#if 0 // future plans

// Can multiple batches share a Gwn_VertBuf? Use ref count?


// We often need a batch with its own data, to be created and discarded together.
// WithOwn variants reduce number of system allocations.

typedef struct {
	Gwn_Batch batch;
	Gwn_VertBuf verts; // link batch.verts to this
} BatchWithOwnVertexBuffer;

typedef struct {
	Gwn_Batch batch;
	Gwn_IndexBuf elem; // link batch.elem to this
} BatchWithOwnElementList;

typedef struct {
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
#define BATCH_DISCARD_ALL_SAFE(batch) do { \
	if (batch != NULL) { \
		GWN_batch_discard_all(batch); \
		batch = NULL; \
	} \
} while (0)
