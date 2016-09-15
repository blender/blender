
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

typedef struct {
	// geometry
	VertexBuffer* verts;
	ElementList* elem; // NULL if element list not needed
	GLenum prim_type;

	// book-keeping
	GLuint vao_id; // remembers all geometry state (vertex attrib bindings & element buffer)
	bool program_dirty;

	// state
	GLuint program;
} Batch;

Batch* Batch_create(GLenum prim_type, VertexBuffer*, ElementList*);

void Batch_set_program(Batch*, GLuint program);
// Entire batch draws with one shader program, but can be redrawn later with another program.
// Vertex shader's inputs must be compatible with the batch's vertex format.

void Batch_draw(Batch*);






#if 0 // future plans

// Can multiple batches share a VertexBuffer? Use ref count?


// for multithreaded batch building:
typedef enum {
	READY_TO_FORMAT,
	READY_TO_BUILD,
	BUILDING, BUILDING_IMM, // choose one
	READY_TO_DRAW
} BatchPhase;


Batch* immBeginBatch(GLenum prim_type, unsigned v_ct);
// use standard immFunctions after this. immEnd will finalize the batch instead
// of drawing.


// We often need a batch with its own data, to be created and discarded together.
// WithOwn variants reduce number of system allocations.

typedef struct {
	Batch batch;
	VertexBuffer verts; // link batch.verts to this
} BatchWithOwnVertexBuffer;

typedef struct {
	Batch batch;
	ElementList elem; // link batch.elem to this
} BatchWithOwnElementList;

typedef struct {
	Batch batch;
	ElementList elem; // link batch.elem to this
	VertexBuffer verts; // link batch.verts to this
} BatchWithOwnVertexBufferAndElementList;

Batch* create_BatchWithOwnVertexBuffer(GLenum prim_type, VertexFormat*, unsigned v_ct, ElementList*);
Batch* create_BatchWithOwnElementList(GLenum prim_type, VertexBuffer*, unsigned prim_ct);
Batch* create_BatchWithOwnVertexBufferAndElementList(GLenum prim_type, VertexFormat*, unsigned v_ct, unsigned prim_ct);
// verts: shared, own
// elem: none, shared, own
Batch* create_BatchInGeneral(GLenum prim_type, VertexBufferStuff, ElementListStuff);

#endif // future plans
