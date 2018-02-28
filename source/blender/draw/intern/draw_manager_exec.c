/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_manager_exec.c
 *  \ingroup draw
 */

#include "draw_manager.h"

#include "BIF_glutil.h"

#include "BKE_global.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"

#ifdef USE_GPU_SELECT
#  include "ED_view3d.h"
#  include "ED_armature.h"
#  include "GPU_select.h"
#endif

#ifdef USE_GPU_SELECT
void DRW_select_load_id(unsigned int id)
{
	BLI_assert(G.f & G_PICKSEL);
	DST.select_id = id;
}
#endif

/* -------------------------------------------------------------------- */

/** \name Draw State (DRW_state)
 * \{ */

void drw_state_set(DRWState state)
{
	if (DST.state == state) {
		return;
	}

#define CHANGED_TO(f) \
	((DST.state & (f)) ? \
		((state & (f)) ?  0 : -1) : \
		((state & (f)) ?  1 :  0))

#define CHANGED_ANY(f) \
	((DST.state & (f)) != (state & (f)))

#define CHANGED_ANY_STORE_VAR(f, enabled) \
	((DST.state & (f)) != (enabled = (state & (f))))

	/* Depth Write */
	{
		int test;
		if ((test = CHANGED_TO(DRW_STATE_WRITE_DEPTH))) {
			if (test == 1) {
				glDepthMask(GL_TRUE);
			}
			else {
				glDepthMask(GL_FALSE);
			}
		}
	}

	/* Color Write */
	{
		int test;
		if ((test = CHANGED_TO(DRW_STATE_WRITE_COLOR))) {
			if (test == 1) {
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
			else {
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			}
		}
	}

	/* Cull */
	{
		DRWState test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_CULL_BACK | DRW_STATE_CULL_FRONT,
		        test))
		{
			if (test) {
				glEnable(GL_CULL_FACE);

				if ((state & DRW_STATE_CULL_BACK) != 0) {
					glCullFace(GL_BACK);
				}
				else if ((state & DRW_STATE_CULL_FRONT) != 0) {
					glCullFace(GL_FRONT);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				glDisable(GL_CULL_FACE);
			}
		}
	}

	/* Depth Test */
	{
		DRWState test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_DEPTH_LESS | DRW_STATE_DEPTH_EQUAL | DRW_STATE_DEPTH_GREATER | DRW_STATE_DEPTH_ALWAYS,
		        test))
		{
			if (test) {
				glEnable(GL_DEPTH_TEST);

				if (state & DRW_STATE_DEPTH_LESS) {
					glDepthFunc(GL_LEQUAL);
				}
				else if (state & DRW_STATE_DEPTH_EQUAL) {
					glDepthFunc(GL_EQUAL);
				}
				else if (state & DRW_STATE_DEPTH_GREATER) {
					glDepthFunc(GL_GREATER);
				}
				else if (state & DRW_STATE_DEPTH_ALWAYS) {
					glDepthFunc(GL_ALWAYS);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				glDisable(GL_DEPTH_TEST);
			}
		}
	}

	/* Wire Width */
	{
		if (CHANGED_ANY(DRW_STATE_WIRE)) {
			if ((state & DRW_STATE_WIRE) != 0) {
				glLineWidth(1.0f);
			}
			else {
				/* do nothing */
			}
		}
	}

	/* Points Size */
	{
		int test;
		if ((test = CHANGED_TO(DRW_STATE_POINT))) {
			if (test == 1) {
				GPU_enable_program_point_size();
				glPointSize(5.0f);
			}
			else {
				GPU_disable_program_point_size();
			}
		}
	}

	/* Blending (all buffer) */
	{
		int test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_BLEND | DRW_STATE_ADDITIVE | DRW_STATE_MULTIPLY | DRW_STATE_TRANSMISSION |
		        DRW_STATE_ADDITIVE_FULL,
		        test))
		{
			if (test) {
				glEnable(GL_BLEND);

				if ((state & DRW_STATE_BLEND) != 0) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, /* RGB */
					                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA); /* Alpha */
				}
				else if ((state & DRW_STATE_MULTIPLY) != 0) {
					glBlendFunc(GL_DST_COLOR, GL_ZERO);
				}
				else if ((state & DRW_STATE_TRANSMISSION) != 0) {
					glBlendFunc(GL_ONE, GL_SRC_ALPHA);
				}
				else if ((state & DRW_STATE_ADDITIVE) != 0) {
					/* Do not let alpha accumulate but premult the source RGB by it. */
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, /* RGB */
					                    GL_ZERO, GL_ONE); /* Alpha */
				}
				else if ((state & DRW_STATE_ADDITIVE_FULL) != 0) {
					/* Let alpha accumulate. */
					glBlendFunc(GL_ONE, GL_ONE);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				glDisable(GL_BLEND);
			}
		}
	}

	/* Clip Planes */
	{
		int test;
		if ((test = CHANGED_TO(DRW_STATE_CLIP_PLANES))) {
			if (test == 1) {
				for (int i = 0; i < DST.num_clip_planes; ++i) {
					glEnable(GL_CLIP_DISTANCE0 + i);
				}
			}
			else {
				for (int i = 0; i < MAX_CLIP_PLANES; ++i) {
					glDisable(GL_CLIP_DISTANCE0 + i);
				}
			}
		}
	}

	/* Line Stipple */
	{
		int test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_STIPPLE_2 | DRW_STATE_STIPPLE_3 | DRW_STATE_STIPPLE_4,
		        test))
		{
			if (test) {
				if ((state & DRW_STATE_STIPPLE_2) != 0) {
					setlinestyle(2);
				}
				else if ((state & DRW_STATE_STIPPLE_3) != 0) {
					setlinestyle(3);
				}
				else if ((state & DRW_STATE_STIPPLE_4) != 0) {
					setlinestyle(4);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				setlinestyle(0);
			}
		}
	}

	/* Stencil */
	{
		DRWState test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_WRITE_STENCIL |
		        DRW_STATE_STENCIL_EQUAL,
		        test))
		{
			if (test) {
				glEnable(GL_STENCIL_TEST);

				/* Stencil Write */
				if ((state & DRW_STATE_WRITE_STENCIL) != 0) {
					glStencilMask(0xFF);
					glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				}
				/* Stencil Test */
				else if ((state & DRW_STATE_STENCIL_EQUAL) != 0) {
					glStencilMask(0x00); /* disable write */
					DST.stencil_mask = 0;
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				/* disable write & test */
				DST.stencil_mask = 0;
				glStencilMask(0x00);
				glStencilFunc(GL_ALWAYS, 1, 0xFF);
				glDisable(GL_STENCIL_TEST);
			}
		}
	}

#undef CHANGED_TO
#undef CHANGED_ANY
#undef CHANGED_ANY_STORE_VAR

	DST.state = state;
}

static void drw_stencil_set(unsigned int mask)
{
	if (DST.stencil_mask != mask) {
		/* Stencil Write */
		if ((DST.state & DRW_STATE_WRITE_STENCIL) != 0) {
			glStencilFunc(GL_ALWAYS, mask, 0xFF);
			DST.stencil_mask = mask;
		}
		/* Stencil Test */
		else if ((DST.state & DRW_STATE_STENCIL_EQUAL) != 0) {
			glStencilFunc(GL_EQUAL, mask, 0xFF);
			DST.stencil_mask = mask;
		}
	}
}

/* Reset state to not interfer with other UI drawcall */
void DRW_state_reset_ex(DRWState state)
{
	DST.state = ~state;
	drw_state_set(state);
}

void DRW_state_reset(void)
{
	/* Reset blending function */
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	DRW_state_reset_ex(DRW_STATE_DEFAULT);
}

/* NOTE : Make sure to reset after use! */
void DRW_state_invert_facing(void)
{
	SWAP(GLenum, DST.backface, DST.frontface);
	glFrontFace(DST.frontface);
}

/**
 * This only works if DRWPasses have been tagged with DRW_STATE_CLIP_PLANES,
 * and if the shaders have support for it (see usage of gl_ClipDistance).
 * Be sure to call DRW_state_clip_planes_reset() after you finish drawing.
 **/
void DRW_state_clip_planes_add(float plane_eq[4])
{
	BLI_assert(DST.num_clip_planes < MAX_CLIP_PLANES-1);
	copy_v4_v4(DST.view_data.clip_planes_eq[DST.num_clip_planes++], plane_eq);
}

void DRW_state_clip_planes_reset(void)
{
	DST.num_clip_planes = 0;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Draw (DRW_draw)
 * \{ */

static void draw_matrices_model_prepare(DRWCallState *st)
{
	/* OPTI : We can optimize further by sharing this computation for each call using the same object. */
	/* Order matters */
	if (st->matflag & (DRW_CALL_MODELVIEW | DRW_CALL_MODELVIEWINVERSE |
	                  DRW_CALL_NORMALVIEW | DRW_CALL_EYEVEC))
	{
		mul_m4_m4m4(st->modelview, DST.view_data.mat[DRW_MAT_VIEW], st->model);
	}
	if (st->matflag & DRW_CALL_MODELVIEWINVERSE) {
		invert_m4_m4(st->modelviewinverse, st->modelview);
	}
	if (st->matflag & DRW_CALL_MODELVIEWPROJECTION) {
		mul_m4_m4m4(st->modelviewprojection, DST.view_data.mat[DRW_MAT_PERS], st->model);
	}
	if (st->matflag & DRW_CALL_NORMALVIEW) {
		copy_m3_m4(st->normalview, st->modelview);
		invert_m3(st->normalview);
		transpose_m3(st->normalview);
	}
	if (st->matflag & DRW_CALL_EYEVEC) {
		/* Used by orthographic wires */
		float tmp[3][3];
		copy_v3_fl3(st->eyevec, 0.0f, 0.0f, 1.0f);
		invert_m3_m3(tmp, st->normalview);
		/* set eye vector, transformed to object coords */
		mul_m3_v3(tmp, st->eyevec);
	}
	/* Non view dependant */
	if (st->matflag & DRW_CALL_MODELINVERSE) {
		invert_m4_m4(st->modelinverse, st->model);
		st->matflag &= ~DRW_CALL_MODELINVERSE;
	}
	if (st->matflag & DRW_CALL_NORMALWORLD) {
		copy_m3_m4(st->normalworld, st->model);
		invert_m3(st->normalworld);
		transpose_m3(st->normalworld);
		st->matflag &= ~DRW_CALL_NORMALWORLD;
	}
}

static void draw_geometry_prepare(DRWShadingGroup *shgroup, DRWCallState *state)
{
	/* step 1 : bind object dependent matrices */
	if (state != NULL) {
		/* OPTI/IDEA(clem): Do this preparation in another thread. */
		draw_matrices_model_prepare(state);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->model, 16, 1, (float *)state->model);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->modelinverse, 16, 1, (float *)state->modelinverse);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->modelview, 16, 1, (float *)state->modelview);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->modelviewinverse, 16, 1, (float *)state->modelviewinverse);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->modelviewprojection, 16, 1, (float *)state->modelviewprojection);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->normalview, 9, 1, (float *)state->normalview);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->normalworld, 9, 1, (float *)state->normalworld);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->orcotexfac, 3, 2, (float *)state->orcotexfac);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->eye, 3, 1, (float *)state->eyevec);
	}
	else {
		BLI_assert((shgroup->normalview == -1) && (shgroup->normalworld == -1) && (shgroup->eye == -1));
		/* For instancing and batching. */
		float unitmat[4][4];
		unit_m4(unitmat);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->model, 16, 1, (float *)unitmat);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->modelinverse, 16, 1, (float *)unitmat);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->modelview, 16, 1, (float *)DST.view_data.mat[DRW_MAT_VIEW]);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->modelviewinverse, 16, 1, (float *)DST.view_data.mat[DRW_MAT_VIEWINV]);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->modelviewprojection, 16, 1, (float *)DST.view_data.mat[DRW_MAT_PERS]);
		GPU_shader_uniform_vector(shgroup->shader, shgroup->orcotexfac, 3, 2, (float *)shgroup->instance_orcofac);
	}
}

static void draw_geometry_execute_ex(
        DRWShadingGroup *shgroup, Gwn_Batch *geom, unsigned int start, unsigned int count)
{
	/* Special case: empty drawcall, placement is done via shader, don't bind anything. */
	if (geom == NULL) {
		BLI_assert(shgroup->type == DRW_SHG_TRIANGLE_BATCH); /* Add other type if needed. */
		/* Shader is already bound. */
		GWN_draw_primitive(GWN_PRIM_TRIS, count);
		return;
	}

	/* step 2 : bind vertex array & draw */
	GWN_batch_program_set_no_use(geom, GPU_shader_get_program(shgroup->shader), GPU_shader_get_interface(shgroup->shader));
	/* XXX hacking gawain. we don't want to call glUseProgram! (huge performance loss) */
	geom->program_in_use = true;
	if (ELEM(shgroup->type, DRW_SHG_INSTANCE, DRW_SHG_INSTANCE_EXTERNAL)) {
		GWN_batch_draw_range_ex(geom, start, count, true);
	}
	else {
		GWN_batch_draw_range(geom, start, count);
	}
	geom->program_in_use = false; /* XXX hacking gawain */
}

static void draw_geometry_execute(DRWShadingGroup *shgroup, Gwn_Batch *geom)
{
	draw_geometry_execute_ex(shgroup, geom, 0, 0);
}

static void bind_texture(GPUTexture *tex)
{
	int bind_num = GPU_texture_bound_number(tex);
	if (bind_num == -1) {
		for (int i = 0; i < GPU_max_textures(); ++i) {
			DST.RST.bind_tex_inc = (DST.RST.bind_tex_inc + 1) % GPU_max_textures();
			if (DST.RST.bound_tex_slots[DST.RST.bind_tex_inc] == false) {
				if (DST.RST.bound_texs[DST.RST.bind_tex_inc] != NULL) {
					GPU_texture_unbind(DST.RST.bound_texs[DST.RST.bind_tex_inc]);
				}
				GPU_texture_bind(tex, DST.RST.bind_tex_inc);
				DST.RST.bound_texs[DST.RST.bind_tex_inc] = tex;
				DST.RST.bound_tex_slots[DST.RST.bind_tex_inc] = true;
				// printf("Binds Texture %d %p\n", DST.RST.bind_tex_inc, tex);
				return;
			}
		}

		printf("Not enough texture slots! Reduce number of textures used by your shader.\n");
	}
	DST.RST.bound_tex_slots[bind_num] = true;
}

static void bind_ubo(GPUUniformBuffer *ubo)
{
	if (DST.RST.bind_ubo_inc < GPU_max_ubo_binds()) {
		GPU_uniformbuffer_bind(ubo, DST.RST.bind_ubo_inc);
		DST.RST.bind_ubo_inc++;
	}
	else {
		/* This is not depending on user input.
		 * It is our responsability to make sure there enough slots. */
		BLI_assert(0 && "Not enough ubo slots! This should not happen!\n");

		/* printf so user can report bad behaviour */
		printf("Not enough ubo slots! This should not happen!\n");
	}
}

static void release_texture_slots(void)
{
	memset(DST.RST.bound_tex_slots, 0x0, sizeof(bool) * GPU_max_textures());
}

static void release_ubo_slots(void)
{
	DST.RST.bind_ubo_inc = 0;
}

static void draw_shgroup(DRWShadingGroup *shgroup, DRWState pass_state)
{
	BLI_assert(shgroup->shader);

	GPUTexture *tex;
	GPUUniformBuffer *ubo;
	int val;
	float fval;

	if (DST.shader != shgroup->shader) {
		if (DST.shader) GPU_shader_unbind();
		GPU_shader_bind(shgroup->shader);
		DST.shader = shgroup->shader;
	}

	release_texture_slots();
	release_ubo_slots();

	drw_state_set((pass_state & shgroup->state_extra_disable) | shgroup->state_extra);
	drw_stencil_set(shgroup->stencil_mask);

	/* Binding Uniform */
	/* Don't check anything, Interface should already contain the least uniform as possible */
	for (DRWUniform *uni = shgroup->uniforms; uni; uni = uni->next) {
		switch (uni->type) {
			case DRW_UNIFORM_SHORT_TO_INT:
				val = (int)*((short *)uni->value);
				GPU_shader_uniform_vector_int(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (int *)&val);
				break;
			case DRW_UNIFORM_SHORT_TO_FLOAT:
				fval = (float)*((short *)uni->value);
				GPU_shader_uniform_vector(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (float *)&fval);
				break;
			case DRW_UNIFORM_BOOL:
			case DRW_UNIFORM_INT:
				GPU_shader_uniform_vector_int(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (int *)uni->value);
				break;
			case DRW_UNIFORM_FLOAT:
				GPU_shader_uniform_vector(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (float *)uni->value);
				break;
			case DRW_UNIFORM_TEXTURE:
				tex = (GPUTexture *)uni->value;
				BLI_assert(tex);
				bind_texture(tex);
				GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
				break;
			case DRW_UNIFORM_BUFFER:
				if (!DRW_state_is_fbo()) {
					break;
				}
				tex = *((GPUTexture **)uni->value);
				BLI_assert(tex);
				bind_texture(tex);
				GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
				break;
			case DRW_UNIFORM_BLOCK:
				ubo = (GPUUniformBuffer *)uni->value;
				bind_ubo(ubo);
				GPU_shader_uniform_buffer(shgroup->shader, uni->location, ubo);
				break;
		}
	}

#ifdef USE_GPU_SELECT
#  define GPU_SELECT_LOAD_IF_PICKSEL(_select_id) \
	if (G.f & G_PICKSEL) { \
		GPU_select_load_id(_select_id); \
	} ((void)0)

#  define GPU_SELECT_LOAD_IF_PICKSEL_CALL(_call) \
	if ((G.f & G_PICKSEL) && (_call)) { \
		GPU_select_load_id((_call)->head.select_id); \
	} ((void)0)

#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST(_shgroup, _start, _count)  \
	_start = 0;                                                      \
	_count = _shgroup->instance_count;                     \
	int *select_id = NULL;                                           \
	if (G.f & G_PICKSEL) {                                           \
		if (_shgroup->override_selectid == -1) {                        \
			select_id = DRW_instance_data_get(_shgroup->inst_selectid); \
			switch (_shgroup->type) {                                             \
				case DRW_SHG_TRIANGLE_BATCH: _count = 3; break;                   \
				case DRW_SHG_LINE_BATCH: _count = 2; break;                       \
				default: _count = 1; break;                                       \
			}                                                                     \
		}                                                                         \
		else {                                                                    \
			GPU_select_load_id(_shgroup->override_selectid);            \
		}                                                                         \
	}                                                                \
	while (_start < _shgroup->instance_count) {            \
		if (select_id) {                                             \
			GPU_select_load_id(select_id[_start]);                   \
		}

# define GPU_SELECT_LOAD_IF_PICKSEL_LIST_END(_start, _count) \
		_start += _count;                                    \
	}

#else
#  define GPU_SELECT_LOAD_IF_PICKSEL(select_id)
#  define GPU_SELECT_LOAD_IF_PICKSEL_CALL(call)
#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST_END(start, count)
#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST(_shgroup, _start, _count) \
	_start = 0;                                                     \
	_count = _shgroup->interface.instance_count;

#endif

	/* Rendering Calls */
	if (!ELEM(shgroup->type, DRW_SHG_NORMAL)) {
		/* Replacing multiple calls with only one */
		if (ELEM(shgroup->type, DRW_SHG_INSTANCE, DRW_SHG_INSTANCE_EXTERNAL)) {
			if (shgroup->type == DRW_SHG_INSTANCE_EXTERNAL) {
				if (shgroup->instance_geom != NULL) {
					GPU_SELECT_LOAD_IF_PICKSEL(shgroup->override_selectid);
					draw_geometry_prepare(shgroup, NULL);
					draw_geometry_execute(shgroup, shgroup->instance_geom);
				}
			}
			else {
				if (shgroup->instance_count > 0) {
					unsigned int count, start;
					draw_geometry_prepare(shgroup, NULL);
					GPU_SELECT_LOAD_IF_PICKSEL_LIST(shgroup, start, count)
					{
						draw_geometry_execute_ex(shgroup, shgroup->instance_geom, start, count);
					}
					GPU_SELECT_LOAD_IF_PICKSEL_LIST_END(start, count)
				}
			}
		}
		else { /* DRW_SHG_***_BATCH */
			/* Some dynamic batch can have no geom (no call to aggregate) */
			if (shgroup->instance_count > 0) {
				unsigned int count, start;
				draw_geometry_prepare(shgroup, NULL);
				GPU_SELECT_LOAD_IF_PICKSEL_LIST(shgroup, start, count)
				{
					draw_geometry_execute_ex(shgroup, shgroup->batch_geom, start, count);
				}
				GPU_SELECT_LOAD_IF_PICKSEL_LIST_END(start, count)
			}
		}
	}
	else {
		bool prev_neg_scale = false;
		for (DRWCall *call = (DRWCall *)shgroup->calls.first; call; call = (DRWCall *)call->head.next) {
			if ((call->state.flag & DRW_CALL_CULLED) != 0)
				continue;

			/* Negative scale objects */
			bool neg_scale = call->state.flag & DRW_CALL_NEGSCALE;
			if (neg_scale != prev_neg_scale) {
				glFrontFace((neg_scale) ? DST.backface : DST.frontface);
				prev_neg_scale = neg_scale;
			}

			GPU_SELECT_LOAD_IF_PICKSEL_CALL(call);

			if (call->head.type == DRW_CALL_SINGLE) {
				draw_geometry_prepare(shgroup, &call->state);
				draw_geometry_execute(shgroup, call->geometry);
			}
			else {
				BLI_assert(call->head.type == DRW_CALL_GENERATE);
				DRWCallGenerate *callgen = ((DRWCallGenerate *)call);
				draw_geometry_prepare(shgroup, &callgen->state);
				callgen->geometry_fn(shgroup, draw_geometry_execute, callgen->user_data);
			}
		}
		/* Reset state */
		glFrontFace(DST.frontface);
	}

	/* TODO: remove, (currently causes alpha issue with sculpt, need to investigate) */
	DRW_state_reset();
}

static void drw_draw_pass_ex(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group)
{
	/* Start fresh */
	DST.shader = NULL;

	BLI_assert(DST.buffer_finish_called && "DRW_render_instance_buffer_finish had not been called before drawing");

	drw_state_set(pass->state);

	DRW_stats_query_start(pass->name);

	for (DRWShadingGroup *shgroup = start_group; shgroup; shgroup = shgroup->next) {
		draw_shgroup(shgroup, pass->state);
		/* break if upper limit */
		if (shgroup == end_group) {
			break;
		}
	}

	/* Clear Bound textures */
	for (int i = 0; i < GPU_max_textures(); i++) {
		if (DST.RST.bound_texs[i] != NULL) {
			GPU_texture_unbind(DST.RST.bound_texs[i]);
			DST.RST.bound_texs[i] = NULL;
		}
	}

	if (DST.shader) {
		GPU_shader_unbind();
		DST.shader = NULL;
	}

	DRW_stats_query_end();
}

void DRW_draw_pass(DRWPass *pass)
{
	drw_draw_pass_ex(pass, pass->shgroups.first, pass->shgroups.last);
}

/* Draw only a subset of shgroups. Used in special situations as grease pencil strokes */
void DRW_draw_pass_subset(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group)
{
	drw_draw_pass_ex(pass, start_group, end_group);
}

/** \} */
