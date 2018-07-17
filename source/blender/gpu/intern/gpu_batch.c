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

/** \file blender/gpu/intern/gwn_batch.c
 *  \ingroup gpu
 *
 * Gawain geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_buffer_id.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_vertex_array_id.h"

#include "gpu_batch_private.h"
#include "gpu_primitive_private.h"
#include "gpu_shader_private.h"

#include <stdlib.h>
#include <string.h>

static void batch_update_program_bindings(Gwn_Batch* batch, uint v_first);

void gwn_batch_vao_cache_clear(Gwn_Batch* batch)
{
	if (batch->context == NULL) {
		return;
	}
	if (batch->is_dynamic_vao_count) {
		for (int i = 0; i < batch->dynamic_vaos.count; ++i) {
			if (batch->dynamic_vaos.vao_ids[i]) {
				GWN_vao_free(batch->dynamic_vaos.vao_ids[i], batch->context);
			}
			if (batch->dynamic_vaos.interfaces[i]) {
				GWN_shaderinterface_remove_batch_ref((Gwn_ShaderInterface *)batch->dynamic_vaos.interfaces[i], batch);
			}
		}
		free(batch->dynamic_vaos.interfaces);
		free(batch->dynamic_vaos.vao_ids);
	}
	else {
		for (int i = 0; i < GWN_BATCH_VAO_STATIC_LEN; ++i) {
			if (batch->static_vaos.vao_ids[i]) {
				GWN_vao_free(batch->static_vaos.vao_ids[i], batch->context);
			}
			if (batch->static_vaos.interfaces[i]) {
				GWN_shaderinterface_remove_batch_ref((Gwn_ShaderInterface *)batch->static_vaos.interfaces[i], batch);
			}
		}
	}
	batch->is_dynamic_vao_count = false;
	for (int i = 0; i < GWN_BATCH_VAO_STATIC_LEN; ++i) {
		batch->static_vaos.vao_ids[i] = 0;
		batch->static_vaos.interfaces[i] = NULL;
	}
	gwn_context_remove_batch(batch->context, batch);
	batch->context = NULL;
}

Gwn_Batch* GWN_batch_create_ex(
        Gwn_PrimType prim_type, Gwn_VertBuf* verts, Gwn_IndexBuf* elem,
        uint owns_flag)
{
	Gwn_Batch* batch = calloc(1, sizeof(Gwn_Batch));
	GWN_batch_init_ex(batch, prim_type, verts, elem, owns_flag);
	return batch;
}

void GWN_batch_init_ex(
        Gwn_Batch* batch, Gwn_PrimType prim_type, Gwn_VertBuf* verts, Gwn_IndexBuf* elem,
        uint owns_flag)
{
#if TRUST_NO_ONE
	assert(verts != NULL);
#endif

	batch->verts[0] = verts;
	for (int v = 1; v < GWN_BATCH_VBO_MAX_LEN; ++v) {
		batch->verts[v] = NULL;
	}
	batch->inst = NULL;
	batch->elem = elem;
	batch->gl_prim_type = convert_prim_type_to_gl(prim_type);
	batch->phase = GWN_BATCH_READY_TO_DRAW;
	batch->is_dynamic_vao_count = false;
	batch->owns_flag = owns_flag;
	batch->free_callback = NULL;
}

/* This will share the VBOs with the new batch. */
Gwn_Batch* GWN_batch_duplicate(Gwn_Batch* batch_src)
{
	Gwn_Batch* batch = GWN_batch_create_ex(GWN_PRIM_POINTS, batch_src->verts[0], batch_src->elem, 0);

	batch->gl_prim_type = batch_src->gl_prim_type;
	for (int v = 1; v < GWN_BATCH_VBO_MAX_LEN; ++v) {
		batch->verts[v] = batch_src->verts[v];
	}
	return batch;
}

void GWN_batch_discard(Gwn_Batch* batch)
{
	if (batch->owns_flag & GWN_BATCH_OWNS_INDEX) {
		GWN_indexbuf_discard(batch->elem);
	}
	if (batch->owns_flag & GWN_BATCH_OWNS_INSTANCES) {
		GWN_vertbuf_discard(batch->inst);
	}
	if ((batch->owns_flag & ~GWN_BATCH_OWNS_INDEX) != 0) {
		for (int v = 0; v < GWN_BATCH_VBO_MAX_LEN; ++v) {
			if (batch->verts[v] == NULL) {
				break;
			}
			if (batch->owns_flag & (1 << v)) {
				GWN_vertbuf_discard(batch->verts[v]);
			}
		}
	}
	gwn_batch_vao_cache_clear(batch);

	if (batch->free_callback) {
		batch->free_callback(batch, batch->callback_data);
	}
	free(batch);
}

void GWN_batch_callback_free_set(Gwn_Batch* batch, void (*callback)(Gwn_Batch*, void*), void* user_data)
{
	batch->free_callback = callback;
	batch->callback_data = user_data;
}

void GWN_batch_instbuf_set(Gwn_Batch* batch, Gwn_VertBuf* inst, bool own_vbo)
{
#if TRUST_NO_ONE
	assert(inst != NULL);
#endif
	/* redo the bindings */
	gwn_batch_vao_cache_clear(batch);

	if (batch->inst != NULL && (batch->owns_flag & GWN_BATCH_OWNS_INSTANCES)) {
		GWN_vertbuf_discard(batch->inst);
	}
	batch->inst = inst;

	if (own_vbo) {
		batch->owns_flag |= GWN_BATCH_OWNS_INSTANCES;
	}
	else {
		batch->owns_flag &= ~GWN_BATCH_OWNS_INSTANCES;
	}
}

/* Returns the index of verts in the batch. */
int GWN_batch_vertbuf_add_ex(
        Gwn_Batch* batch, Gwn_VertBuf* verts,
        bool own_vbo)
{
	/* redo the bindings */
	gwn_batch_vao_cache_clear(batch);

	for (uint v = 0; v < GWN_BATCH_VBO_MAX_LEN; ++v) {
		if (batch->verts[v] == NULL) {
#if TRUST_NO_ONE
			/* for now all VertexBuffers must have same vertex_len */
			assert(verts->vertex_len == batch->verts[0]->vertex_len);
#endif
			batch->verts[v] = verts;
			/* TODO: mark dirty so we can keep attrib bindings up-to-date */
			if (own_vbo)
				batch->owns_flag |= (1 << v);
			return v;
		}
	}

	/* we only make it this far if there is no room for another Gwn_VertBuf */
#if TRUST_NO_ONE
	assert(false);
#endif
	return -1;
}

static GLuint batch_vao_get(Gwn_Batch *batch)
{
	/* Search through cache */
	if (batch->is_dynamic_vao_count) {
		for (int i = 0; i < batch->dynamic_vaos.count; ++i)
			if (batch->dynamic_vaos.interfaces[i] == batch->interface)
				return batch->dynamic_vaos.vao_ids[i];
	}
	else {
		for (int i = 0; i < GWN_BATCH_VAO_STATIC_LEN; ++i)
			if (batch->static_vaos.interfaces[i] == batch->interface)
				return batch->static_vaos.vao_ids[i];
	}

	/* Set context of this batch.
	 * It will be bound to it until gwn_batch_vao_cache_clear is called.
	 * Until then it can only be drawn with this context. */
	if (batch->context == NULL) {
		batch->context = GWN_context_active_get();
		gwn_context_add_batch(batch->context, batch);
	}
#if TRUST_NO_ONE
	else {
		/* Make sure you are not trying to draw this batch in another context. */
		assert(batch->context == GWN_context_active_get());
	}
#endif

	/* Cache miss, time to add a new entry! */
	GLuint new_vao = 0;
	if (!batch->is_dynamic_vao_count) {
		int i; /* find first unused slot */
		for (i = 0; i < GWN_BATCH_VAO_STATIC_LEN; ++i) 
			if (batch->static_vaos.vao_ids[i] == 0)
				break;

		if (i < GWN_BATCH_VAO_STATIC_LEN) {
			batch->static_vaos.interfaces[i] = batch->interface;
			batch->static_vaos.vao_ids[i] = new_vao = GWN_vao_alloc();
		}
		else {
			/* Not enough place switch to dynamic. */
			batch->is_dynamic_vao_count = true;
			/* Erase previous entries, they will be added back if drawn again. */
			for (int j = 0; j < GWN_BATCH_VAO_STATIC_LEN; ++j) {
				GWN_shaderinterface_remove_batch_ref((Gwn_ShaderInterface*)batch->static_vaos.interfaces[j], batch);
				GWN_vao_free(batch->static_vaos.vao_ids[j], batch->context);
			}
			/* Init dynamic arrays and let the branch below set the values. */
			batch->dynamic_vaos.count = GWN_BATCH_VAO_DYN_ALLOC_COUNT;
			batch->dynamic_vaos.interfaces = calloc(batch->dynamic_vaos.count, sizeof(Gwn_ShaderInterface*));
			batch->dynamic_vaos.vao_ids = calloc(batch->dynamic_vaos.count, sizeof(GLuint));
		}
	}

	if (batch->is_dynamic_vao_count) {
		int i; /* find first unused slot */
		for (i = 0; i < batch->dynamic_vaos.count; ++i)
			if (batch->dynamic_vaos.vao_ids[i] == 0)
				break;

		if (i == batch->dynamic_vaos.count) {
			/* Not enough place, realloc the array. */
			i = batch->dynamic_vaos.count;
			batch->dynamic_vaos.count += GWN_BATCH_VAO_DYN_ALLOC_COUNT;
			batch->dynamic_vaos.interfaces = realloc(batch->dynamic_vaos.interfaces, sizeof(Gwn_ShaderInterface*) * batch->dynamic_vaos.count);
			batch->dynamic_vaos.vao_ids = realloc(batch->dynamic_vaos.vao_ids, sizeof(GLuint) * batch->dynamic_vaos.count);
			memset(batch->dynamic_vaos.interfaces + i, 0, sizeof(Gwn_ShaderInterface*) * GWN_BATCH_VAO_DYN_ALLOC_COUNT);
			memset(batch->dynamic_vaos.vao_ids + i, 0, sizeof(GLuint) * GWN_BATCH_VAO_DYN_ALLOC_COUNT);
		}
		batch->dynamic_vaos.interfaces[i] = batch->interface;
		batch->dynamic_vaos.vao_ids[i] = new_vao = GWN_vao_alloc();
	}

	GWN_shaderinterface_add_batch_ref((Gwn_ShaderInterface*)batch->interface, batch);

#if TRUST_NO_ONE
	assert(new_vao != 0);
#endif

	/* We just got a fresh VAO we need to initialize it. */
	glBindVertexArray(new_vao);
	batch_update_program_bindings(batch, 0);
	glBindVertexArray(0);

	return new_vao;
}

void GWN_batch_program_set_no_use(Gwn_Batch* batch, uint32_t program, const Gwn_ShaderInterface* shaderface)
{
#if TRUST_NO_ONE
	assert(glIsProgram(shaderface->program));
	assert(batch->program_in_use == 0);
#endif
	batch->interface = shaderface;
	batch->program = program;
	batch->vao_id = batch_vao_get(batch);
}

void GWN_batch_program_set(Gwn_Batch* batch, uint32_t program, const Gwn_ShaderInterface* shaderface)
{
	GWN_batch_program_set_no_use(batch, program, shaderface);
	GWN_batch_program_use_begin(batch); /* hack! to make Batch_Uniform* simpler */
}

void gwn_batch_remove_interface_ref(Gwn_Batch* batch, const Gwn_ShaderInterface* interface)
{
	if (batch->is_dynamic_vao_count) {
		for (int i = 0; i < batch->dynamic_vaos.count; ++i) {
			if (batch->dynamic_vaos.interfaces[i] == interface) {
				GWN_vao_free(batch->dynamic_vaos.vao_ids[i], batch->context);
				batch->dynamic_vaos.vao_ids[i] = 0;
				batch->dynamic_vaos.interfaces[i] = NULL;
				break; /* cannot have duplicates */
			}
		}
	}
	else {
		int i;
		for (i = 0; i < GWN_BATCH_VAO_STATIC_LEN; ++i) {
			if (batch->static_vaos.interfaces[i] == interface) {
				GWN_vao_free(batch->static_vaos.vao_ids[i], batch->context);
				batch->static_vaos.vao_ids[i] = 0;
				batch->static_vaos.interfaces[i] = NULL;
				break; /* cannot have duplicates */
			}
		}
	}
}

static void create_bindings(
        Gwn_VertBuf* verts, const Gwn_ShaderInterface* interface,
        uint v_first, const bool use_instancing)
{
	const Gwn_VertFormat* format = &verts->format;

	const uint attr_len = format->attr_len;
	const uint stride = format->stride;

	GWN_vertbuf_use(verts);

	for (uint a_idx = 0; a_idx < attr_len; ++a_idx) {
		const Gwn_VertAttr* a = format->attribs + a_idx;
		const GLvoid* pointer = (const GLubyte*)0 + a->offset + v_first * stride;

		for (uint n_idx = 0; n_idx < a->name_len; ++n_idx) {
			const Gwn_ShaderInput* input = GWN_shaderinterface_attr(interface, a->name[n_idx]);

			if (input == NULL) continue;

			if (a->comp_len == 16 || a->comp_len == 12 || a->comp_len == 8) {
#if TRUST_NO_ONE
				assert(a->fetch_mode == GWN_FETCH_FLOAT);
				assert(a->gl_comp_type == GL_FLOAT);
#endif
				for (int i = 0; i < a->comp_len / 4; ++i) {
					glEnableVertexAttribArray(input->location + i);
					glVertexAttribDivisor(input->location + i, (use_instancing) ? 1 : 0);
					glVertexAttribPointer(input->location + i, 4, a->gl_comp_type, GL_FALSE, stride,
					                      (const GLubyte*)pointer + i * 16);
				}
			}
			else
				{
				glEnableVertexAttribArray(input->location);
				glVertexAttribDivisor(input->location, (use_instancing) ? 1 : 0);

				switch (a->fetch_mode) {
					case GWN_FETCH_FLOAT:
					case GWN_FETCH_INT_TO_FLOAT:
						glVertexAttribPointer(input->location, a->comp_len, a->gl_comp_type, GL_FALSE, stride, pointer);
						break;
					case GWN_FETCH_INT_TO_FLOAT_UNIT:
						glVertexAttribPointer(input->location, a->comp_len, a->gl_comp_type, GL_TRUE, stride, pointer);
						break;
					case GWN_FETCH_INT:
						glVertexAttribIPointer(input->location, a->comp_len, a->gl_comp_type, stride, pointer);
						break;
				}
			}
		}
	}
}

static void batch_update_program_bindings(Gwn_Batch* batch, uint v_first)
{
	for (int v = 0; v < GWN_BATCH_VBO_MAX_LEN && batch->verts[v] != NULL; ++v) {
		create_bindings(batch->verts[v], batch->interface, (batch->inst) ? 0 : v_first, false);
	}
	if (batch->inst) {
		create_bindings(batch->inst, batch->interface, v_first, true);
	}
	if (batch->elem) {
		GWN_indexbuf_use(batch->elem);
	}
}

void GWN_batch_program_use_begin(Gwn_Batch* batch)
{
	/* NOTE: use_program & done_using_program are fragile, depend on staying in sync with
	 *       the GL context's active program. use_program doesn't mark other programs as "not used". */
	/* TODO: make not fragile (somehow) */

	if (!batch->program_in_use) {
		glUseProgram(batch->program);
		batch->program_in_use = true;
	}
}

void GWN_batch_program_use_end(Gwn_Batch* batch)
{
	if (batch->program_in_use) {
#if PROGRAM_NO_OPTI
		glUseProgram(0);
#endif
		batch->program_in_use = false;
	}
}

#if TRUST_NO_ONE
  #define GET_UNIFORM const Gwn_ShaderInput* uniform = GWN_shaderinterface_uniform(batch->interface, name); assert(uniform);
#else
  #define GET_UNIFORM const Gwn_ShaderInput* uniform = GWN_shaderinterface_uniform(batch->interface, name);
#endif

void GWN_batch_uniform_1ui(Gwn_Batch* batch, const char* name, int value)
{
	GET_UNIFORM
	glUniform1ui(uniform->location, value);
}

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

void GWN_batch_uniform_2fv_array(Gwn_Batch* batch, const char* name, const int len, const float *data)
{
	GET_UNIFORM
	glUniform2fv(uniform->location, len, data);
}

void GWN_batch_uniform_4fv_array(Gwn_Batch* batch, const char* name, const int len, const float *data)
{
	GET_UNIFORM
	glUniform4fv(uniform->location, len, data);
}

void GWN_batch_uniform_mat4(Gwn_Batch* batch, const char* name, const float data[4][4])
{
	GET_UNIFORM
	glUniformMatrix4fv(uniform->location, 1, GL_FALSE, (const float *)data);
}

static void primitive_restart_enable(const Gwn_IndexBuf *el)
{
	// TODO(fclem) Replace by GL_PRIMITIVE_RESTART_FIXED_INDEX when we have ogl 4.3
	glEnable(GL_PRIMITIVE_RESTART);
	GLuint restart_index = (GLuint)0xFFFFFFFF;

#if GWN_TRACK_INDEX_RANGE
	if (el->index_type == GWN_INDEX_U8)
		restart_index = (GLuint)0xFF;
	else if (el->index_type == GWN_INDEX_U16)
		restart_index = (GLuint)0xFFFF;
#endif

	glPrimitiveRestartIndex(restart_index);
}

static void primitive_restart_disable(void)
{
	glDisable(GL_PRIMITIVE_RESTART);
}

void GWN_batch_draw(Gwn_Batch* batch)
{
#if TRUST_NO_ONE
	assert(batch->phase == GWN_BATCH_READY_TO_DRAW);
	assert(batch->verts[0]->vbo_id != 0);
#endif
	GWN_batch_program_use_begin(batch);
	GPU_matrix_bind(batch->interface); // external call.

	GWN_batch_draw_range_ex(batch, 0, 0, false);

	GWN_batch_program_use_end(batch);
}

void GWN_batch_draw_range_ex(Gwn_Batch* batch, int v_first, int v_count, bool force_instance)
{
#if TRUST_NO_ONE
	assert(!(force_instance && (batch->inst == NULL)) || v_count > 0); // we cannot infer length if force_instance
#endif
	const bool do_instance = (force_instance || batch->inst);

	// If using offset drawing, use the default VAO and redo bindings.
	if (v_first != 0 && (do_instance || batch->elem)) {
		glBindVertexArray(GWN_vao_default());
		batch_update_program_bindings(batch, v_first);
	}
	else {
		glBindVertexArray(batch->vao_id);
	}

	if (do_instance) {
		/* Infer length if vertex count is not given */
		if (v_count == 0) {
			v_count = batch->inst->vertex_len;
		}

		if (batch->elem) {
			const Gwn_IndexBuf* el = batch->elem;

			if (el->use_prim_restart) {
				primitive_restart_enable(el);
			}
#if GWN_TRACK_INDEX_RANGE
			glDrawElementsInstancedBaseVertex(batch->gl_prim_type,
			                                  el->index_len,
			                                  el->gl_index_type,
			                                  0,
			                                  v_count,
			                                  el->base_index);
#else
			glDrawElementsInstanced(batch->gl_prim_type, el->index_len, GL_UNSIGNED_INT, 0, v_count);
#endif
			if (el->use_prim_restart) {
				primitive_restart_disable();
			}
		}
		else {
			glDrawArraysInstanced(batch->gl_prim_type, 0, batch->verts[0]->vertex_len, v_count);
		}
	}
	else {
		/* Infer length if vertex count is not given */
		if (v_count == 0) {
			v_count = (batch->elem) ? batch->elem->index_len : batch->verts[0]->vertex_len;
		}

		if (batch->elem) {
			const Gwn_IndexBuf* el = batch->elem;

			if (el->use_prim_restart) {
				primitive_restart_enable(el);
			}

#if GWN_TRACK_INDEX_RANGE
			if (el->base_index) {
				glDrawRangeElementsBaseVertex(batch->gl_prim_type,
				                              el->min_index,
				                              el->max_index,
				                              v_count,
				                              el->gl_index_type,
				                              0,
				                              el->base_index);
			}
			else {
				glDrawRangeElements(batch->gl_prim_type, el->min_index, el->max_index, v_count, el->gl_index_type, 0);
			}
#else
			glDrawElements(batch->gl_prim_type, v_count, GL_UNSIGNED_INT, 0);
#endif
			if (el->use_prim_restart) {
				primitive_restart_disable();
			}
		}
		else {
			glDrawArrays(batch->gl_prim_type, v_first, v_count);
		}
	}

	/* Performance hog if you are drawing with the same vao multiple time.
	 * Only activate for debugging. */
	// glBindVertexArray(0);
}

/* just draw some vertices and let shader place them where we want. */
void GWN_draw_primitive(Gwn_PrimType prim_type, int v_count)
	{
	/* we cannot draw without vao ... annoying ... */
	glBindVertexArray(GWN_vao_default());

	GLenum type = convert_prim_type_to_gl(prim_type);
	glDrawArrays(type, 0, v_count);

	/* Performance hog if you are drawing with the same vao multiple time.
	 * Only activate for debugging.*/
	// glBindVertexArray(0);
	}


/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void GWN_batch_program_set_builtin(Gwn_Batch *batch, GPUBuiltinShader shader_id)
{
	GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);
	GWN_batch_program_set(batch, shader->program, shader->interface);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit
 * \{ */

void gpu_batch_init(void)
{
	gpu_batch_presets_init();
}

void gpu_batch_exit(void)
{
	gpu_batch_presets_exit();
}

/** \} */