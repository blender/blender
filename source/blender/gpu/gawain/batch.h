
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
#include "attrib_binding.h"

// How will this API be used?
// create batch
// ...
// profit!

// TODO: finalize Batch struct design & usage, pare down this file

typedef struct {
	VertexBuffer; // format is fixed at "vec3 pos"
	ElementList line_elem;
	ElementList triangle_elem;
	GLuint vao_id;
	GLenum prev_prim; // did most recent draw use GL_POINTS, GL_LINES or GL_TRIANGLES?
} BasicBatch;

// How to do this without replicating code?

typedef struct {
	VertexBuffer* verts;
	ElementList* elem; // <-- NULL if element list not needed
	GLenum prim_type;
	GLuint vao_id;
	GLuint bound_program;
	AttribBinding attrib_binding;
} Batch;

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

typedef struct {
	// geometry
	GLenum prim_type;
	VertexBuffer verts;
	ElementList elem; // <-- elem.index_ct = 0 if element list not needed

	// book-keeping
	GLuint vao_id; // <-- remembers all vertex state (array buffer, element buffer, attrib bindings)
	// wait a sec... I thought VAO held attrib bindings but not currently bound array buffer.
	// That's fine but verify that VAO holds *element* buffer binding.
	// Verified: ELEMENT_ARRAY_BUFFER_BINDING is part of VAO state.
	// VERTEX_ATTRIB_ARRAY_BUFFER_BINDING is too, per vertex attrib. Currently bound ARRAY_BUFFER is not.
	// Does APPLE_vertex_array_object also include ELEMENT_ARRAY_BUFFER_BINDING?
	// The extension spec refers only to APPLE_element_array, so.. maybe, maybe not?
	// Will have to test during development, maybe alter behavior for APPLE_LEGACY. Can strip out this
	// platform-specific cruft for Blender, keep it for legacy Gawain.

	// state
	GLuint bound_program;
	AttribBinding attrib_binding;
} Batch;

typedef struct {
	Batch* batch;
} BatchBuilder;

// One batch can be drawn with multiple shaders, as long as those shaders' inputs
// are compatible with the batch's vertex format.

// Can multiple batches share a VertexBuffer? Use ref count?

// BasicBatch
// Create one VertexBuffer from an object's verts (3D position only)
// Shader must depend only on position + uniforms: uniform color, depth only, or object ID.
// - draw verts via DrawArrays
// - draw lines via DrawElements (can have 2 element lists: true face edges, triangulated edges)
// - draw faces via DrawElements (raw triangles, not polygon faces)
// This is very 3D-mesh-modeling specific. I'm investigating what Gawain needs to allow/expose
// to meet Blender's needs, possibly other programs' needs.

Batch* BatchPlease(GLenum prim_type, unsigned prim_ct, unsigned v_ct);
//                   GL_TRIANGLES   12 triangles that share 8 vertices

// Is there ever a reason to index GL_POINTS? nothing comes to mind...
// (later) ok now that I'm thinking straight, *of course* you can draw
// indexed POINTS. Only some verts from the buffer will be drawn. I was
// just limiting my thinking to immediate needs. Batched needs.

Batch* batch = BatchPlease(GL_TRIANGLES, 12, 8);
unsigned pos = add_attrib(batch->verts.format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
pack(batch->verts->format); // or ...
finalize(batch); // <-- packs vertex format, allocates vertex buffer

Batch* create_Batch(GLenum prim_type, VertexBuffer*, ElementList*);

// and don't forget
Batch* immBeginBatch(GLenum prim_type, unsigned v_ct);
// use standard immFunctions after this. immEnd will finalize the batch instead
// of drawing.

typedef enum {
	READY_TO_FORMAT,
	READY_TO_BUILD,
	BUILDING, BUILDING_IMM, // choose one
	READY_TO_DRAW
} BatchPhase;
