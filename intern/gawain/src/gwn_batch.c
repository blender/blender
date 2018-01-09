
// Gawain geometry batch
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gwn_batch.h"
#include "gwn_buffer_id.h"
#include "gwn_primitive_private.h"
#include <stdlib.h>

// necessary functions from matrix API
extern void gpuBindMatrices(const Gwn_ShaderInterface* shaderface);
extern bool gpuMatricesDirty(void); // how best to use this here?

Gwn_Batch* GWN_batch_create_ex(
        Gwn_PrimType prim_type, Gwn_VertBuf* verts, Gwn_IndexBuf* elem,
        unsigned owns_flag)
	{
	Gwn_Batch* batch = calloc(1, sizeof(Gwn_Batch));

	GWN_batch_init_ex(batch, prim_type, verts, elem, owns_flag);

	return batch;
	}

void GWN_batch_init_ex(
        Gwn_Batch* batch, Gwn_PrimType prim_type, Gwn_VertBuf* verts, Gwn_IndexBuf* elem,
        unsigned owns_flag)
	{
#if TRUST_NO_ONE
	assert(verts != NULL);
#endif

	batch->verts[0] = verts;
	for (int v = 1; v < GWN_BATCH_VBO_MAX_LEN; ++v)
		batch->verts[v] = NULL;
	batch->elem = elem;
	batch->prim_type = prim_type;
	batch->gl_prim_type = convert_prim_type_to_gl(prim_type);
	batch->phase = GWN_BATCH_READY_TO_DRAW;
	batch->owns_flag = owns_flag;
	}

void GWN_batch_discard(Gwn_Batch* batch)
	{
	if (batch->owns_flag & GWN_BATCH_OWNS_INDEX)
		GWN_indexbuf_discard(batch->elem);

	if ((batch->owns_flag & ~GWN_BATCH_OWNS_INDEX) != 0)
		{
		for (int v = 0; v < GWN_BATCH_VBO_MAX_LEN; ++v)
			{
			if (batch->verts[v] == NULL)
				break;
			if (batch->owns_flag & (1 << v))
				GWN_vertbuf_discard(batch->verts[v]);
			}
		}

	if (batch->vao_id)
		GWN_vao_free(batch->vao_id);

	free(batch);
	}

int GWN_batch_vertbuf_add_ex(
        Gwn_Batch* batch, Gwn_VertBuf* verts,
        bool own_vbo)
	{
	for (unsigned v = 0; v < GWN_BATCH_VBO_MAX_LEN; ++v)
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
			if (own_vbo)
				batch->owns_flag |= (1 << v);
			return v;
			}
		}
	
	// we only make it this far if there is no room for another Gwn_VertBuf
#if TRUST_NO_ONE
	assert(false);
#endif
	return -1;
	}

void GWN_batch_program_set(Gwn_Batch* batch, GLuint program, const Gwn_ShaderInterface* shaderface)
	{
#if TRUST_NO_ONE
	assert(glIsProgram(program));
#endif

	batch->program = program;
	batch->interface = shaderface;
	batch->program_dirty = true;

	GWN_batch_program_use_begin(batch); // hack! to make Batch_Uniform* simpler
	}

// fclem : hack !
// we need this because we don't want to unbind the shader between drawcalls
// but we still want the correct shader to be bound outside the draw manager
void GWN_batch_program_unset(Gwn_Batch* batch)
	{
	batch->program_in_use = false;
	}

static void Batch_update_program_bindings(Gwn_Batch* batch, unsigned int v_first)
	{
	// disable all as a precaution
	// why are we not using prev_attrib_enabled_bits?? see immediate.c
	for (unsigned a_idx = 0; a_idx < GWN_VERT_ATTR_MAX_LEN; ++a_idx)
		glDisableVertexAttribArray(a_idx);

	for (int v = 0; v < GWN_BATCH_VBO_MAX_LEN; ++v)
		{
		Gwn_VertBuf* verts = batch->verts[v];
		if (verts == NULL)
			break;

		const Gwn_VertFormat* format = &verts->format;

		const unsigned attrib_ct = format->attrib_ct;
		const unsigned stride = format->stride;

		GWN_vertbuf_use(verts);

		for (unsigned a_idx = 0; a_idx < attrib_ct; ++a_idx)
			{
			const Gwn_VertAttr* a = format->attribs + a_idx;

			const GLvoid* pointer = (const GLubyte*)0 + a->offset + v_first * stride;

			for (unsigned n_idx = 0; n_idx < a->name_ct; ++n_idx)
				{
				const Gwn_ShaderInput* input = GWN_shaderinterface_attr(batch->interface, a->name[n_idx]);

				if (input == NULL) continue;

				glEnableVertexAttribArray(input->location);

				switch (a->fetch_mode)
					{
					case GWN_FETCH_FLOAT:
					case GWN_FETCH_INT_TO_FLOAT:
						glVertexAttribPointer(input->location, a->comp_ct, a->gl_comp_type, GL_FALSE, stride, pointer);
						break;
					case GWN_FETCH_INT_TO_FLOAT_UNIT:
						glVertexAttribPointer(input->location, a->comp_ct, a->gl_comp_type, GL_TRUE, stride, pointer);
						break;
					case GWN_FETCH_INT:
						glVertexAttribIPointer(input->location, a->comp_ct, a->gl_comp_type, stride, pointer);
					}
				}
			}
		}

	batch->program_dirty = false;
	}

void GWN_batch_program_use_begin(Gwn_Batch* batch)
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

void GWN_batch_program_use_end(Gwn_Batch* batch)
	{
	if (batch->program_in_use)
		{
		glUseProgram(0);
		batch->program_in_use = false;
		}
	}

#if TRUST_NO_ONE
  #define GET_UNIFORM const Gwn_ShaderInput* uniform = GWN_shaderinterface_uniform(batch->interface, name); assert(uniform);
#else
  #define GET_UNIFORM const Gwn_ShaderInput* uniform = GWN_shaderinterface_uniform(batch->interface, name);
#endif

void GWN_batch_uniform_1i(Gwn_Batch* batch, const char* name, int value)
	{
	GET_UNIFORM
	glUniform1i(uniform->location, value);
	}

void GWN_batch_uniform_1b(Gwn_Batch* batch, const char* name, bool value)
	{
	GET_UNIFORM
	glUniform1i(uniform->location, value ? GL_TRUE : GL_FALSE);
	}

void GWN_batch_uniform_2f(Gwn_Batch* batch, const char* name, float x, float y)
	{
	GET_UNIFORM
	glUniform2f(uniform->location, x, y);
	}

void GWN_batch_uniform_3f(Gwn_Batch* batch, const char* name, float x, float y, float z)
	{
	GET_UNIFORM
	glUniform3f(uniform->location, x, y, z);
	}

void GWN_batch_uniform_4f(Gwn_Batch* batch, const char* name, float x, float y, float z, float w)
	{
	GET_UNIFORM
	glUniform4f(uniform->location, x, y, z, w);
	}

void GWN_batch_uniform_1f(Gwn_Batch* batch, const char* name, float x)
	{
	GET_UNIFORM
	glUniform1f(uniform->location, x);
	}

void GWN_batch_uniform_2fv(Gwn_Batch* batch, const char* name, const float data[2])
	{
	GET_UNIFORM
	glUniform2fv(uniform->location, 1, data);
	}

void GWN_batch_uniform_3fv(Gwn_Batch* batch, const char* name, const float data[3])
	{
	GET_UNIFORM
	glUniform3fv(uniform->location, 1, data);
	}

void GWN_batch_uniform_4fv(Gwn_Batch* batch, const char* name, const float data[4])
	{
	GET_UNIFORM
	glUniform4fv(uniform->location, 1, data);
	}

static void Batch_prime(Gwn_Batch* batch)
	{
	batch->vao_id = GWN_vao_alloc();
	glBindVertexArray(batch->vao_id);

	for (int v = 0; v < GWN_BATCH_VBO_MAX_LEN; ++v)
		{
		if (batch->verts[v] == NULL)
			break;
		GWN_vertbuf_use(batch->verts[v]);
		}

	if (batch->elem)
		GWN_indexbuf_use(batch->elem);

	// vertex attribs and element list remain bound to this VAO
	}

void GWN_batch_draw(Gwn_Batch* batch)
	{
#if TRUST_NO_ONE
	assert(batch->phase == GWN_BATCH_READY_TO_DRAW);
	assert(glIsProgram(batch->program));
#endif

	if (batch->vao_id)
		glBindVertexArray(batch->vao_id);
	else
		Batch_prime(batch);

	if (batch->program_dirty)
		Batch_update_program_bindings(batch, 0);

	GWN_batch_program_use_begin(batch);

	gpuBindMatrices(batch->interface);

	if (batch->elem)
		{
		const Gwn_IndexBuf* el = batch->elem;

#if GWN_TRACK_INDEX_RANGE
		if (el->base_index)
			glDrawRangeElementsBaseVertex(batch->gl_prim_type, el->min_index, el->max_index, el->index_ct, el->gl_index_type, 0, el->base_index);
		else
			glDrawRangeElements(batch->gl_prim_type, el->min_index, el->max_index, el->index_ct, el->gl_index_type, 0);
#else
		glDrawElements(batch->gl_prim_type, el->index_ct, GL_UNSIGNED_INT, 0);
#endif
		}
	else
		glDrawArrays(batch->gl_prim_type, 0, batch->verts[0]->vertex_ct);

	GWN_batch_program_use_end(batch);
	glBindVertexArray(0);
	}



// clement : temp stuff
void GWN_batch_draw_stupid(Gwn_Batch* batch, int v_first, int v_count)
	{
	if (batch->vao_id)
		glBindVertexArray(batch->vao_id);
	else
		Batch_prime(batch);

	if (batch->program_dirty)
		Batch_update_program_bindings(batch, v_first);

	// GWN_batch_program_use_begin(batch);

	//gpuBindMatrices(batch->program);

	// Infer lenght if vertex count is not given
	if (v_count == 0) {
		v_count = (batch->elem) ? batch->elem->index_ct : batch->verts[0]->vertex_ct;
	}

	if (batch->elem)
		{
		const Gwn_IndexBuf* el = batch->elem;

#if GWN_TRACK_INDEX_RANGE
		if (el->base_index)
			glDrawRangeElementsBaseVertex(batch->gl_prim_type, el->min_index, el->max_index, v_count, el->gl_index_type, 0, el->base_index);
		else
			glDrawRangeElements(batch->gl_prim_type, el->min_index, el->max_index, v_count, el->gl_index_type, 0);
#else
		glDrawElements(batch->gl_prim_type, v_count, GL_UNSIGNED_INT, 0);
#endif
		}
	else
		glDrawArrays(batch->gl_prim_type, 0, v_count);

	// GWN_batch_program_use_end(batch);
	glBindVertexArray(0);
	}

// clement : temp stuff
void GWN_batch_draw_stupid_instanced(Gwn_Batch* batch, unsigned int instance_vbo, int instance_first, int instance_count,
                                 int attrib_nbr, int attrib_stride, int attrib_size[16], int attrib_loc[16])
	{
	if (batch->vao_id)
		glBindVertexArray(batch->vao_id);
	else
		Batch_prime(batch);

	if (batch->program_dirty)
		Batch_update_program_bindings(batch, 0);

	glBindBuffer(GL_ARRAY_BUFFER, instance_vbo);
	int ptr_ofs = instance_first * attrib_stride;
	for (int i = 0; i < attrib_nbr; ++i)
		{
		int size = attrib_size[i];
		int loc = attrib_loc[i];
		int atr_ofs = 0;

		while (size > 0)
			{
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

	// GWN_batch_program_use_begin(batch);

	//gpuBindMatrices(batch->program);

	if (batch->elem)
		{
		const Gwn_IndexBuf* el = batch->elem;
#if GWN_TRACK_INDEX_RANGE
		glDrawElementsInstancedBaseVertex(batch->gl_prim_type, el->index_ct, el->gl_index_type, 0, instance_count, el->base_index);
#else
		glDrawElementsInstanced(batch->gl_prim_type, el->index_ct, GL_UNSIGNED_INT, 0, instance_count);
#endif
		}
	else
		glDrawArraysInstanced(batch->gl_prim_type, 0, batch->verts[0]->vertex_ct, instance_count);

	// Reset divisor to prevent messing the next draw
	for (unsigned a_idx = 0; a_idx < GWN_VERT_ATTR_MAX_LEN; ++a_idx)
		glVertexAttribDivisor(a_idx, 0);

	// GWN_batch_program_use_end(batch);
	glBindVertexArray(0);
	}

void GWN_batch_draw_stupid_instanced_with_batch(Gwn_Batch* batch_instanced, Gwn_Batch* batch_instancing)
	{
	if (batch_instanced->vao_id)
		glBindVertexArray(batch_instanced->vao_id);
	else
		Batch_prime(batch_instanced);

	if (batch_instanced->program_dirty)
		Batch_update_program_bindings(batch_instanced, 0);

	Gwn_VertBuf* verts = batch_instancing->verts[0];

	const Gwn_VertFormat* format = &verts->format;

	const unsigned attrib_ct = format->attrib_ct;
	const unsigned stride = format->stride;

	GWN_vertbuf_use(verts);

	for (unsigned a_idx = 0; a_idx < attrib_ct; ++a_idx)
		{
		const Gwn_VertAttr* a = format->attribs + a_idx;

		const GLvoid* pointer = (const GLubyte*)0 + a->offset;

		for (unsigned n_idx = 0; n_idx < a->name_ct; ++n_idx)
			{
			const Gwn_ShaderInput* input = GWN_shaderinterface_attr(batch_instanced->interface, a->name[n_idx]);

			if (input == NULL) continue;

			glEnableVertexAttribArray(input->location);
			glVertexAttribDivisor(input->location, 1);

			switch (a->fetch_mode)
				{
				case GWN_FETCH_FLOAT:
				case GWN_FETCH_INT_TO_FLOAT:
					glVertexAttribPointer(input->location, a->comp_ct, a->gl_comp_type, GL_FALSE, stride, pointer);
					break;
				case GWN_FETCH_INT_TO_FLOAT_UNIT:
					glVertexAttribPointer(input->location, a->comp_ct, a->gl_comp_type, GL_TRUE, stride, pointer);
					break;
				case GWN_FETCH_INT:
					glVertexAttribIPointer(input->location, a->comp_ct, a->gl_comp_type, stride, pointer);
				}
			}
		}

	// GWN_batch_program_use_begin(batch);

	//gpuBindMatrices(batch->program);

	if (batch_instanced->elem)
		{
		const Gwn_IndexBuf* el = batch_instanced->elem;

#if GWN_TRACK_INDEX_RANGE
		glDrawElementsInstancedBaseVertex(batch_instanced->gl_prim_type, el->index_ct, el->gl_index_type, 0, verts->vertex_ct, el->base_index);
#else
		glDrawElementsInstanced(batch_instanced->gl_prim_type, el->index_ct, GL_UNSIGNED_INT, 0, verts->vertex_ct);
#endif
		}
	else
		glDrawArraysInstanced(batch_instanced->gl_prim_type, 0, batch_instanced->verts[0]->vertex_ct, verts->vertex_ct);

	// Reset divisor to prevent messing the next draw
	for (unsigned a_idx = 0; a_idx < GWN_VERT_ATTR_MAX_LEN; ++a_idx)
		glVertexAttribDivisor(a_idx, 0);

	// GWN_batch_program_use_end(batch);
	glBindVertexArray(0);
	}
