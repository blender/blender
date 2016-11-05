
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

// necessary functions from matrix API
extern void gpuBindMatrices(GLuint program);
extern bool gpuMatricesDirty(void); // how best to use this here?

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
	batch->phase = READY_TO_DRAW;

	return batch;
	}

void Batch_discard(Batch* batch)
	{
	// TODO: clean up
	}

void Batch_discard_all(Batch* batch)
	{
	VertexBuffer_discard(batch->verts);
	if (batch->elem)
		ElementList_discard(batch->elem);
	Batch_discard(batch);
	}

void Batch_set_program(Batch* batch, GLuint program)
	{
#if TRUST_NO_ONE
	assert(glIsProgram(program));
#endif

	batch->program = program;
	batch->program_dirty = true;

	Batch_use_program(batch); // hack! to make Batch_Uniform* simpler
	}

static void Batch_update_program_bindings(Batch* batch)
	{
	const VertexFormat* format = &batch->verts->format;

	const unsigned attrib_ct = format->attrib_ct;
	const unsigned stride = format->stride;

	// disable all as a precaution
	// why are we not using prev_attrib_enabled_bits?? see immediate.c
	for (unsigned a_idx = 0; a_idx < MAX_VERTEX_ATTRIBS; ++a_idx)
		glDisableVertexAttribArray(a_idx);

	VertexBuffer_use(batch->verts);

	for (unsigned a_idx = 0; a_idx < attrib_ct; ++a_idx)
		{
		const Attrib* a = format->attribs + a_idx;

		const GLvoid* pointer = (const GLubyte*)0 + a->offset;

		const GLint loc = glGetAttribLocation(batch->program, a->name);

#if TRUST_NO_ONE
		assert(loc != -1);
#endif

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

void Batch_use_program(Batch* batch)
	{
	// NOTE: use_program & done_using_program are fragile, depend on staying in sync with
	//       the GL context's active program. use_program doesn't mark other programs as "not used".
	// TODO: make not fragile (somehow)

	if (!batch->program_in_use)
		{
		glUseProgram(batch->program);
		batch->program_in_use = true;
		}
	}

void Batch_done_using_program(Batch* batch)
	{
	if (batch->program_in_use)
		{
		glUseProgram(0);
		batch->program_in_use = false;
		}
	}

void Batch_Uniform1b(Batch* batch, const char* name, bool value)
	{
	int loc = glGetUniformLocation(batch->program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform1i(loc, value ? GL_TRUE : GL_FALSE);
	}

void Batch_Uniform2f(Batch* batch, const char* name, float x, float y)
	{
	int loc = glGetUniformLocation(batch->program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform2f(loc, x, y);
	}

void Batch_Uniform4f(Batch* batch, const char* name, float x, float y, float z, float w)
	{
	int loc = glGetUniformLocation(batch->program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform4f(loc, x, y, z, w);
	}

void Batch_Uniform1f(Batch* batch, const char* name, float x)
	{
	int loc = glGetUniformLocation(batch->program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform1f(loc, x);
	}

void Batch_Uniform3fv(Batch* batch, const char* name, const float data[3])
	{
	int loc = glGetUniformLocation(batch->program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform3fv(loc, 1, data);
	}

void Batch_Uniform4fv(Batch* batch, const char* name, const float data[4])
	{
	int loc = glGetUniformLocation(batch->program, name);

#if TRUST_NO_ONE
	assert(loc != -1);
#endif

	glUniform4fv(loc, 1, data);
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
#if TRUST_NO_ONE
	assert(batch->phase == READY_TO_DRAW);
	assert(glIsProgram(batch->program));
#endif

	if (batch->vao_id)
		glBindVertexArray(batch->vao_id);
	else
		Batch_prime(batch);

	if (batch->program_dirty)
		Batch_update_program_bindings(batch);

	Batch_use_program(batch);

	gpuBindMatrices(batch->program);

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

	Batch_done_using_program(batch);
	glBindVertexArray(0);
	}
