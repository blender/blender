
// Gawain geometry batch
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "batch.h"
#include <stdlib.h>

Batch* Batch_create(GLenum prim_type, VertexBuffer* verts, ElementList* elem)
	{
#if TRUST_NO_ONE
	assert(verts != NULL);
	assert(prim_type == GL_POINTS || prim_type == GL_LINES || prim_type == GL_TRIANGLES);
	// we will allow other primitive types in a future update
#endif

	Batch* batch = calloc(1, sizeof(Batch));

	batch->verts = verts;
	batch->elem = elem;
	batch->prim_type = prim_type;

	return batch;
	}

void Batch_set_program(Batch* batch, GLuint program)
	{
	batch->program = program;
	batch->program_dirty = true;
	}

static void Batch_update_program_bindings(Batch* batch)
	{
#if TRUST_NO_ONE
	assert(glIsProgram(program));
#endif

	const VertexFormat* format = &batch->verts->format;

	const unsigned attrib_ct = format->attrib_ct;
	const unsigned stride = format->stride;

	for (unsigned a_idx = 0; a_idx < attrib_ct; ++a_idx)
		{
		const Attrib* a = format->attribs + a_idx;

		const GLvoid* pointer = (const GLubyte*)0 + a->offset;

		const unsigned loc = glGetAttribLocation(batch->program, a->name);

		glEnableVertexAttribArray(loc);

		switch (a->fetch_mode)
			{
			case KEEP_FLOAT:
			case CONVERT_INT_TO_FLOAT:
				glVertexAttribPointer(loc, a->comp_ct, a->comp_type, GL_FALSE, stride, pointer);
				break;
			case NORMALIZE_INT_TO_FLOAT:
				glVertexAttribPointer(loc, a->comp_ct, a->comp_type, GL_TRUE, stride, pointer);
				break;
			case KEEP_INT:
				glVertexAttribIPointer(loc, a->comp_ct, a->comp_type, stride, pointer);
			}
		}

	batch->program_dirty = false;
	}

static void Batch_prime(Batch* batch)
	{
	glGenVertexArrays(1, &batch->vao_id);
	glBindVertexArray(batch->vao_id);

	VertexBuffer_use(batch->verts);

	if (batch->elem)
		ElementList_use(batch->elem);

	// vertex attribs and element list remain bound to this VAO
	}

void Batch_draw(Batch* batch)
	{
	if (batch->vao_id)
		glBindVertexArray(batch->vao_id);
	else
		Batch_prime(batch);

	if (batch->program_dirty)
		Batch_update_program_bindings(batch);

	if (batch->elem)
		{
		const ElementList* el = batch->elem;

#if TRACK_INDEX_RANGE
		if (el->base_index)
			glDrawRangeElementsBaseVertex(batch->prim_type, el->min_index, el->max_index, el->index_ct, el->index_type, 0, el->base_index);
		else
			glDrawRangeElements(batch->prim_type, el->min_index, el->max_index, el->index_ct, el->index_type, 0);
#else
		glDrawElements(batch->prim_type, el->index_ct, GL_UNSIGNED_INT, 0);
#endif
		}
	else
		glDrawArrays(batch->prim_type, 0, batch->verts->vertex_ct);

	glBindVertexArray(0);
	}
