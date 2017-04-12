
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
#include "buffer_id.h"
#include <stdlib.h>

// necessary functions from matrix API
extern void gpuBindMatrices(GLuint program);
extern bool gpuMatricesDirty(void); // how best to use this here?

Batch* Batch_create(PrimitiveType prim_type, VertexBuffer* verts, ElementList* elem)
	{
	Batch* batch = calloc(1, sizeof(Batch));

	Batch_init(batch, prim_type, verts, elem);

	return batch;
	}

void Batch_init(Batch* batch, PrimitiveType prim_type, VertexBuffer* verts, ElementList* elem)
	{
#if TRUST_NO_ONE
	assert(verts != NULL);
#endif

	batch->verts[0] = verts;
	for (int v = 1; v < BATCH_MAX_VBO_CT; ++v)
		batch->verts[v] = NULL;
	batch->elem = elem;
	batch->prim_type = prim_type;
	batch->gl_prim_type = convert_prim_type_to_gl(prim_type);
	batch->phase = READY_TO_DRAW;
	}

void Batch_discard(Batch* batch)
	{
	if (batch->vao_id)
		vao_id_free(batch->vao_id);

	free(batch);
	}

void Batch_discard_all(Batch* batch)
	{
	for (int v = 0; v < BATCH_MAX_VBO_CT; ++v)
		{
		if (batch->verts[v] == NULL)
			break;
		VertexBuffer_discard(batch->verts[v]);
		}

	if (batch->elem)
		ElementList_discard(batch->elem);

	Batch_discard(batch);
	}

int Batch_add_VertexBuffer(Batch* batch, VertexBuffer* verts)
	{
	for (unsigned v = 0; v < BATCH_MAX_VBO_CT; ++v)
		{
		if (batch->verts[v] == NULL)
			{
#if TRUST_NO_ONE
			// for now all VertexBuffers must have same vertex_ct
			assert(verts->vertex_ct == batch->verts[0]->vertex_ct);
			// in the near future we will enable instanced attribs which have their own vertex_ct
#endif
			batch->verts[v] = verts;
			// TODO: mark dirty so we can keep attrib bindings up-to-date
			return v;
			}
		}
	
	// we only make it this far if there is no room for another VertexBuffer
#if TRUST_NO_ONE
	assert(false);
#endif
	return -1;
	}

void Batch_set_program(Batch* batch, GLuint program, const ShaderInterface* shaderface)
	{
#if TRUST_NO_ONE
	assert(glIsProgram(program));
#endif

	batch->program = program;
	batch->interface = shaderface;
	batch->program_dirty = true;

	Batch_use_program(batch); // hack! to make Batch_Uniform* simpler
	}

static void Batch_update_program_bindings(Batch* batch)
	{
	// disable all as a precaution
	// why are we not using prev_attrib_enabled_bits?? see immediate.c
	for (unsigned a_idx = 0; a_idx < MAX_VERTEX_ATTRIBS; ++a_idx)
		glDisableVertexAttribArray(a_idx);

	for (int v = 0; v < BATCH_MAX_VBO_CT; ++v)
		{
		VertexBuffer* verts = batch->verts[v];
		if (verts == NULL)
			break;

		const VertexFormat* format = &verts->format;

		const unsigned attrib_ct = format->attrib_ct;
		const unsigned stride = format->stride;

		VertexBuffer_use(verts);

		for (unsigned a_idx = 0; a_idx < attrib_ct; ++a_idx)
			{
			const Attrib* a = format->attribs + a_idx;

			const GLvoid* pointer = (const GLubyte*)0 + a->offset;

			const GLint loc = glGetAttribLocation(batch->program, a->name);

			if (loc == -1) continue;

			glEnableVertexAttribArray(loc);

			switch (a->fetch_mode)
				{
				case KEEP_FLOAT:
				case CONVERT_INT_TO_FLOAT:
					glVertexAttribPointer(loc, a->comp_ct, a->gl_comp_type, GL_FALSE, stride, pointer);
					break;
				case NORMALIZE_INT_TO_FLOAT:
					glVertexAttribPointer(loc, a->comp_ct, a->gl_comp_type, GL_TRUE, stride, pointer);
					break;
				case KEEP_INT:
					glVertexAttribIPointer(loc, a->comp_ct, a->gl_comp_type, stride, pointer);
				}
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

#if TRUST_NO_ONE
  #define GET_UNIFORM const ShaderInput* uniform = ShaderInterface_uniform(batch->interface, name); assert(uniform);
#else
  #define GET_UNIFORM const ShaderInput* uniform = ShaderInterface_uniform(batch->interface, name);
#endif

void Batch_Uniform1i(Batch* batch, const char* name, int value)
	{
	GET_UNIFORM
	glUniform1i(uniform->location, value);
	}

void Batch_Uniform1b(Batch* batch, const char* name, bool value)
	{
	GET_UNIFORM
	glUniform1i(uniform->location, value ? GL_TRUE : GL_FALSE);
	}

void Batch_Uniform2f(Batch* batch, const char* name, float x, float y)
	{
	GET_UNIFORM
	glUniform2f(uniform->location, x, y);
	}

void Batch_Uniform3f(Batch* batch, const char* name, float x, float y, float z)
	{
	GET_UNIFORM
	glUniform3f(uniform->location, x, y, z);
	}

void Batch_Uniform4f(Batch* batch, const char* name, float x, float y, float z, float w)
	{
	GET_UNIFORM
	glUniform4f(uniform->location, x, y, z, w);
	}

void Batch_Uniform1f(Batch* batch, const char* name, float x)
	{
	GET_UNIFORM
	glUniform1f(uniform->location, x);
	}

void Batch_Uniform3fv(Batch* batch, const char* name, const float data[3])
	{
	GET_UNIFORM
	glUniform3fv(uniform->location, 1, data);
	}

void Batch_Uniform4fv(Batch* batch, const char* name, const float data[4])
	{
	GET_UNIFORM
	glUniform4fv(uniform->location, 1, data);
	}

static void Batch_prime(Batch* batch)
	{
	batch->vao_id = vao_id_alloc();
	glBindVertexArray(batch->vao_id);

	for (int v = 0; v < BATCH_MAX_VBO_CT; ++v)
		{
		if (batch->verts[v] == NULL)
			break;
		VertexBuffer_use(batch->verts[v]);
		}

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
			glDrawRangeElementsBaseVertex(batch->gl_prim_type, el->min_index, el->max_index, el->index_ct, el->index_type, 0, el->base_index);
		else
			glDrawRangeElements(batch->gl_prim_type, el->min_index, el->max_index, el->index_ct, el->index_type, 0);
#else
		glDrawElements(batch->gl_prim_type, el->index_ct, GL_UNSIGNED_INT, 0);
#endif
		}
	else
		glDrawArrays(batch->gl_prim_type, 0, batch->verts[0]->vertex_ct);

	Batch_done_using_program(batch);
	glBindVertexArray(0);
	}



// clement : temp stuff
void Batch_draw_stupid(Batch* batch)
{
	if (batch->vao_id)
		glBindVertexArray(batch->vao_id);
	else
		Batch_prime(batch);

	if (batch->program_dirty)
		Batch_update_program_bindings(batch);

	// Batch_use_program(batch);

	//gpuBindMatrices(batch->program);

	if (batch->elem)
		{
		const ElementList* el = batch->elem;

#if TRACK_INDEX_RANGE
		if (el->base_index)
			glDrawRangeElementsBaseVertex(batch->gl_prim_type, el->min_index, el->max_index, el->index_ct, el->index_type, 0, el->base_index);
		else
			glDrawRangeElements(batch->gl_prim_type, el->min_index, el->max_index, el->index_ct, el->index_type, 0);
#else
		glDrawElements(batch->gl_prim_type, el->index_ct, GL_UNSIGNED_INT, 0);
#endif
		}
	else
		glDrawArrays(batch->gl_prim_type, 0, batch->verts[0]->vertex_ct);

	// Batch_done_using_program(batch);
	glBindVertexArray(0);
}

// clement : temp stuff
void Batch_draw_stupid_instanced(Batch* batch, unsigned int instance_vbo, int instance_count,
                                 int attrib_nbr, int attrib_stride, int attrib_size[16], int attrib_loc[16])
{
	if (batch->vao_id)
		glBindVertexArray(batch->vao_id);
	else
		Batch_prime(batch);

	if (batch->program_dirty)
		Batch_update_program_bindings(batch);

	glBindBuffer(GL_ARRAY_BUFFER, instance_vbo);
	int ptr_ofs = 0;
	for (int i = 0; i < attrib_nbr; ++i) {
		int size = attrib_size[i];
		int loc = attrib_loc[i];
		int atr_ofs = 0;

		while (size > 0) {
			glEnableVertexAttribArray(loc + atr_ofs);
			glVertexAttribPointer(loc + atr_ofs, (size > 4) ? 4 : size, GL_FLOAT, GL_FALSE,
			                      sizeof(float) * attrib_stride, (GLvoid*)(sizeof(float) * ptr_ofs));
			glVertexAttribDivisor(loc + atr_ofs, 1);
			atr_ofs++;
			ptr_ofs += (size > 4) ? 4 : size;
			size -= 4;
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Batch_use_program(batch);

	//gpuBindMatrices(batch->program);

	if (batch->elem)
		{
		const ElementList* el = batch->elem;

		glDrawElementsInstanced(batch->gl_prim_type, el->index_ct, GL_UNSIGNED_INT, 0, instance_count);
		}
	else
		glDrawArraysInstanced(batch->gl_prim_type, 0, batch->verts[0]->vertex_ct, instance_count);

	// Batch_done_using_program(batch);
	glBindVertexArray(0);
}

