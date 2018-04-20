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

/** \file blender/draw/intern/draw_manager.c
 *  \ingroup draw
 */

#include <stdio.h>

#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BLF_api.h"

#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_workspace.h"

#include "draw_manager.h"
#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_uniformbuffer.h"
#include "GPU_viewport.h"
#include "GPU_matrix.h"

#include "IMB_colormanagement.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "wm_window.h"

#include "draw_manager_text.h"
#include "draw_manager_profiling.h"

/* only for callbacks */
#include "draw_cache_impl.h"

#include "draw_mode_engines.h"
#include "engines/clay/clay_engine.h"
#include "engines/eevee/eevee_engine.h"
#include "engines/basic/basic_engine.h"
#include "engines/workbench/workbench_engine.h"
#include "engines/external/external_engine.h"

#include "../../../intern/gawain/gawain/gwn_context.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#ifdef USE_GPU_SELECT
#  include "GPU_select.h"
#endif

/** Render State: No persistent data between draw calls. */
DRWManager DST = {NULL};

ListBase DRW_engines = {NULL, NULL};

extern struct GPUUniformBuffer *view_ubo; /* draw_manager_exec.c */

static void drw_state_prepare_clean_for_draw(DRWManager *dst)
{
	memset(dst, 0x0, offsetof(DRWManager, ogl_context));
}

/* This function is used to reset draw manager to a state
 * where we don't re-use data by accident across different
 * draw calls.
 */
#ifdef DEBUG
static void drw_state_ensure_not_reused(DRWManager *dst)
{
	memset(dst, 0xff, offsetof(DRWManager, ogl_context));
}
#endif

/* -------------------------------------------------------------------- */

void DRW_draw_callbacks_pre_scene(void)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;

	gpuLoadProjectionMatrix(rv3d->winmat);
	gpuLoadMatrix(rv3d->viewmat);
}

void DRW_draw_callbacks_post_scene(void)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;

	gpuLoadProjectionMatrix(rv3d->winmat);
	gpuLoadMatrix(rv3d->viewmat);
}

struct DRWTextStore *DRW_text_cache_ensure(void)
{
	BLI_assert(DST.text_store_p);
	if (*DST.text_store_p == NULL) {
		*DST.text_store_p = DRW_text_cache_create();
	}
	return *DST.text_store_p;
}


/* -------------------------------------------------------------------- */

/** \name Settings
 * \{ */

bool DRW_object_is_renderable(Object *ob)
{
	BLI_assert(BKE_object_is_visible(ob, OB_VISIBILITY_CHECK_UNKNOWN_RENDER_MODE));

	if (ob->type == OB_MESH) {
		if (ob == DST.draw_ctx.object_edit) {
			IDProperty *props = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_EDIT, "");
			bool do_show_occlude_wire = BKE_collection_engine_property_value_get_bool(props, "show_occlude_wire");
			if (do_show_occlude_wire) {
				return false;
			}
			bool do_show_weight = BKE_collection_engine_property_value_get_bool(props, "show_weight");
			if (do_show_weight) {
				return false;
			}
		}
	}

	return true;
}

/**
 * Return whether this object is visible depending if
 * we are rendering or drawing in the viewport.
 */
bool DRW_check_object_visible_within_active_context(Object *ob)
{
	const eObjectVisibilityCheck mode = DRW_state_is_scene_render() ?
	                                     OB_VISIBILITY_CHECK_FOR_RENDER :
	                                     OB_VISIBILITY_CHECK_FOR_VIEWPORT;
	return BKE_object_is_visible(ob, mode);
}

bool DRW_object_is_flat_normal(const Object *ob)
{
	if (ob->type == OB_MESH) {
		const Mesh *me = ob->data;
		if (me->mpoly && me->mpoly[0].flag & ME_SMOOTH) {
			return false;
		}
	}
	return true;
}

/**
 * Return true if the object has its own draw mode.
 * Caller must check this is active */
int DRW_object_is_mode_shade(const Object *ob)
{
	BLI_assert(ob == DST.draw_ctx.obact);
	UNUSED_VARS_NDEBUG(ob);
	if ((DST.draw_ctx.object_mode & OB_MODE_EDIT) == 0) {
		if (DST.draw_ctx.object_mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)) {
			if ((DST.draw_ctx.v3d->flag2 & V3D_SHOW_MODE_SHADE_OVERRIDE) == 0) {
				return true;
			}
			else {
				return false;
			}
		}
	}
	return -1;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Color Management
 * \{ */

/* Use color management profile to draw texture to framebuffer */
void DRW_transform_to_display(GPUTexture *tex)
{
	drw_state_set(DRW_STATE_WRITE_COLOR);

	Gwn_VertFormat *vert_format = immVertexFormat();
	unsigned int pos = GWN_vertformat_attr_add(vert_format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int texco = GWN_vertformat_attr_add(vert_format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	const float dither = 1.0f;

	bool use_ocio = false;

	/* View transform is already applied for offscreen, don't apply again, see: T52046 */
	if (!(DST.options.is_image_render && !DST.options.is_scene_render)) {
		Scene *scene = DST.draw_ctx.scene;
		use_ocio = IMB_colormanagement_setup_glsl_draw_from_space(
		        &scene->view_settings, &scene->display_settings, NULL, dither, false);
	}

	if (!use_ocio) {
		/* View transform is already applied for offscreen, don't apply again, see: T52046 */
		if (DST.options.is_image_render && !DST.options.is_scene_render) {
			immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_COLOR);
			immUniformColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		}
		else {
			immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_LINEAR_TO_SRGB);
		}
		immUniform1i("image", 0);
	}

	GPU_texture_bind(tex, 0); /* OCIO texture bind point is 0 */

	float mat[4][4];
	unit_m4(mat);
	immUniformMatrix4fv("ModelViewProjectionMatrix", mat);

	/* Full screen triangle */
	immBegin(GWN_PRIM_TRIS, 3);
	immAttrib2f(texco, 0.0f, 0.0f);
	immVertex2f(pos, -1.0f, -1.0f);

	immAttrib2f(texco, 2.0f, 0.0f);
	immVertex2f(pos, 3.0f, -1.0f);

	immAttrib2f(texco, 0.0f, 2.0f);
	immVertex2f(pos, -1.0f, 3.0f);
	immEnd();

	GPU_texture_unbind(tex);

	if (use_ocio) {
		IMB_colormanagement_finish_glsl_draw();
	}
	else {
		immUnbindProgram();
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Viewport (DRW_viewport)
 * \{ */

void *drw_viewport_engine_data_ensure(void *engine_type)
{
	void *data = GPU_viewport_engine_data_get(DST.viewport, engine_type);

	if (data == NULL) {
		data = GPU_viewport_engine_data_create(DST.viewport, engine_type);
	}
	return data;
}

void DRW_engine_viewport_data_size_get(
        const void *engine_type_v,
        int *r_fbl_len, int *r_txl_len, int *r_psl_len, int *r_stl_len)
{
	const DrawEngineType *engine_type = engine_type_v;

	if (r_fbl_len) {
		*r_fbl_len = engine_type->vedata_size->fbl_len;
	}
	if (r_txl_len) {
		*r_txl_len = engine_type->vedata_size->txl_len;
	}
	if (r_psl_len) {
		*r_psl_len = engine_type->vedata_size->psl_len;
	}
	if (r_stl_len) {
		*r_stl_len = engine_type->vedata_size->stl_len;
	}
}

const float *DRW_viewport_size_get(void)
{
	return DST.size;
}

const float *DRW_viewport_invert_size_get(void)
{
	return DST.inv_size;
}

const float *DRW_viewport_screenvecs_get(void)
{
	return &DST.screenvecs[0][0];
}

const float *DRW_viewport_pixelsize_get(void)
{
	return &DST.pixsize;
}

static void drw_viewport_cache_resize(void)
{
	/* Release the memiter before clearing the mempools that references them */
	GPU_viewport_cache_release(DST.viewport);

	if (DST.vmempool != NULL) {
		BLI_mempool_clear_ex(DST.vmempool->calls, BLI_mempool_len(DST.vmempool->calls));
		BLI_mempool_clear_ex(DST.vmempool->states, BLI_mempool_len(DST.vmempool->states));
		BLI_mempool_clear_ex(DST.vmempool->shgroups, BLI_mempool_len(DST.vmempool->shgroups));
		BLI_mempool_clear_ex(DST.vmempool->uniforms, BLI_mempool_len(DST.vmempool->uniforms));
		BLI_mempool_clear_ex(DST.vmempool->passes, BLI_mempool_len(DST.vmempool->passes));
	}

	DRW_instance_data_list_free_unused(DST.idatalist);
	DRW_instance_data_list_resize(DST.idatalist);
}

/* Not a viewport variable, we could split this out. */
static void drw_context_state_init(void)
{
	if (DST.draw_ctx.obact) {
		DST.draw_ctx.object_mode = DST.draw_ctx.obact->mode;
	}
	else {
		DST.draw_ctx.object_mode = OB_MODE_OBJECT;
	}

	/* Edit object. */
	if (DST.draw_ctx.object_mode & OB_MODE_EDIT) {
		DST.draw_ctx.object_edit = DST.draw_ctx.obact;
	}
	else {
		DST.draw_ctx.object_edit = NULL;
	}

	/* Pose object. */
	if (DST.draw_ctx.object_mode & OB_MODE_POSE) {
		DST.draw_ctx.object_pose = DST.draw_ctx.obact;
	}
	else if (DST.draw_ctx.object_mode & OB_MODE_WEIGHT_PAINT) {
		DST.draw_ctx.object_pose = BKE_object_pose_armature_get(DST.draw_ctx.obact);
	}
	else {
		DST.draw_ctx.object_pose = NULL;
	}
}

/* It also stores viewport variable to an immutable place: DST
 * This is because a cache uniform only store reference
 * to its value. And we don't want to invalidate the cache
 * if this value change per viewport */
static void drw_viewport_var_init(void)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;
	/* Refresh DST.size */
	if (DST.viewport) {
		int size[2];
		GPU_viewport_size_get(DST.viewport, size);
		DST.size[0] = size[0];
		DST.size[1] = size[1];
		DST.inv_size[0] = 1.0f / size[0];
		DST.inv_size[1] = 1.0f / size[1];

		DefaultFramebufferList *fbl = (DefaultFramebufferList *)GPU_viewport_framebuffer_list_get(DST.viewport);
		DST.default_framebuffer = fbl->default_fb;

		DST.vmempool = GPU_viewport_mempool_get(DST.viewport);

		if (DST.vmempool->calls == NULL) {
			DST.vmempool->calls = BLI_mempool_create(sizeof(DRWCall), 0, 512, 0);
		}
		if (DST.vmempool->states == NULL) {
			DST.vmempool->states = BLI_mempool_create(sizeof(DRWCallState), 0, 512, BLI_MEMPOOL_ALLOW_ITER);
		}
		if (DST.vmempool->shgroups == NULL) {
			DST.vmempool->shgroups = BLI_mempool_create(sizeof(DRWShadingGroup), 0, 256, 0);
		}
		if (DST.vmempool->uniforms == NULL) {
			DST.vmempool->uniforms = BLI_mempool_create(sizeof(DRWUniform), 0, 512, 0);
		}
		if (DST.vmempool->passes == NULL) {
			DST.vmempool->passes = BLI_mempool_create(sizeof(DRWPass), 0, 64, 0);
		}

		DST.idatalist = GPU_viewport_instance_data_list_get(DST.viewport);
		DRW_instance_data_list_reset(DST.idatalist);
	}
	else {
		DST.size[0] = 0;
		DST.size[1] = 0;

		DST.inv_size[0] = 0;
		DST.inv_size[1] = 0;

		DST.default_framebuffer = NULL;
		DST.vmempool = NULL;
	}

	if (rv3d != NULL) {
		/* Refresh DST.screenvecs */
		copy_v3_v3(DST.screenvecs[0], rv3d->viewinv[0]);
		copy_v3_v3(DST.screenvecs[1], rv3d->viewinv[1]);
		normalize_v3(DST.screenvecs[0]);
		normalize_v3(DST.screenvecs[1]);

		/* Refresh DST.pixelsize */
		DST.pixsize = rv3d->pixsize;

		copy_m4_m4(DST.original_mat.mat[DRW_MAT_PERS], rv3d->persmat);
		copy_m4_m4(DST.original_mat.mat[DRW_MAT_PERSINV], rv3d->persinv);
		copy_m4_m4(DST.original_mat.mat[DRW_MAT_VIEW], rv3d->viewmat);
		copy_m4_m4(DST.original_mat.mat[DRW_MAT_VIEWINV], rv3d->viewinv);
		copy_m4_m4(DST.original_mat.mat[DRW_MAT_WIN], rv3d->winmat);
		invert_m4_m4(DST.original_mat.mat[DRW_MAT_WININV], rv3d->winmat);

		memcpy(DST.view_data.matstate.mat, DST.original_mat.mat, sizeof(DST.original_mat.mat));

		copy_v4_v4(DST.view_data.viewcamtexcofac, rv3d->viewcamtexcofac);
	}
	else {
		copy_v4_fl4(DST.view_data.viewcamtexcofac, 1.0f, 1.0f, 0.0f, 0.0f);
	}

	/* Reset facing */
	DST.frontface = GL_CCW;
	DST.backface = GL_CW;
	glFrontFace(DST.frontface);

	if (DST.draw_ctx.object_edit) {
		ED_view3d_init_mats_rv3d(DST.draw_ctx.object_edit, rv3d);
	}

	/* Alloc array of texture reference. */
	if (DST.RST.bound_texs == NULL) {
		DST.RST.bound_texs = MEM_callocN(sizeof(GPUTexture *) * GPU_max_textures(), "Bound GPUTexture refs");
	}
	if (DST.RST.bound_tex_slots == NULL) {
		DST.RST.bound_tex_slots = MEM_callocN(sizeof(char) * GPU_max_textures(), "Bound Texture Slots");
	}
	if (DST.RST.bound_ubos == NULL) {
		DST.RST.bound_ubos = MEM_callocN(sizeof(GPUUniformBuffer *) * GPU_max_ubo_binds(), "Bound GPUUniformBuffer refs");
	}
	if (DST.RST.bound_ubo_slots == NULL) {
		DST.RST.bound_ubo_slots = MEM_callocN(sizeof(char) * GPU_max_ubo_binds(), "Bound Ubo Slots");
	}

	if (view_ubo == NULL) {
		view_ubo = DRW_uniformbuffer_create(sizeof(ViewUboStorage), NULL);
	}

	DST.override_mat = 0;
	DST.dirty_mat = true;
	DST.state_cache_id = 1;

	DST.clipping.updated = false;

	memset(DST.common_instance_data, 0x0, sizeof(DST.common_instance_data));
}

void DRW_viewport_matrix_get(float mat[4][4], DRWViewportMatrixType type)
{
	BLI_assert(type >= 0 && type < DRW_MAT_COUNT);
	BLI_assert(((DST.override_mat & (1 << type)) != 0) || DST.draw_ctx.rv3d != NULL); /* Can't use this in render mode. */

	copy_m4_m4(mat, DST.view_data.matstate.mat[type]);
}

void DRW_viewport_matrix_get_all(DRWMatrixState *state)
{
	memcpy(state, DST.view_data.matstate.mat, sizeof(DRWMatrixState));
}

void DRW_viewport_matrix_override_set(const float mat[4][4], DRWViewportMatrixType type)
{
	BLI_assert(type < DRW_MAT_COUNT);
	copy_m4_m4(DST.view_data.matstate.mat[type], mat);
	DST.override_mat |= (1 << type);
	DST.dirty_mat = true;
	DST.clipping.updated = false;
}

void DRW_viewport_matrix_override_unset(DRWViewportMatrixType type)
{
	BLI_assert(type < DRW_MAT_COUNT);
	copy_m4_m4(DST.view_data.matstate.mat[type], DST.original_mat.mat[type]);
	DST.override_mat &= ~(1 << type);
	DST.dirty_mat = true;
	DST.clipping.updated = false;
}

void DRW_viewport_matrix_override_set_all(DRWMatrixState *state)
{
	memcpy(DST.view_data.matstate.mat, state, sizeof(DRWMatrixState));
	DST.override_mat = 0xFFFFFF;
	DST.dirty_mat = true;
	DST.clipping.updated = false;
}

void DRW_viewport_matrix_override_unset_all(void)
{
	memcpy(DST.view_data.matstate.mat, DST.original_mat.mat, sizeof(DRWMatrixState));
	DST.override_mat = 0;
	DST.dirty_mat = true;
	DST.clipping.updated = false;
}

bool DRW_viewport_is_persp_get(void)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;
	if (rv3d) {
		return rv3d->is_persp;
	}
	else {
		return DST.view_data.matstate.mat[DRW_MAT_WIN][3][3] == 0.0f;
	}
	BLI_assert(0);
	return false;
}

DefaultFramebufferList *DRW_viewport_framebuffer_list_get(void)
{
	return GPU_viewport_framebuffer_list_get(DST.viewport);
}

DefaultTextureList *DRW_viewport_texture_list_get(void)
{
	return GPU_viewport_texture_list_get(DST.viewport);
}

void DRW_viewport_request_redraw(void)
{
	GPU_viewport_tag_update(DST.viewport);
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name ViewLayers (DRW_scenelayer)
 * \{ */

void *DRW_view_layer_engine_data_get(DrawEngineType *engine_type)
{
	for (ViewLayerEngineData *sled = DST.draw_ctx.view_layer->drawdata.first; sled; sled = sled->next) {
		if (sled->engine_type == engine_type) {
			return sled->storage;
		}
	}
	return NULL;
}

void **DRW_view_layer_engine_data_ensure(DrawEngineType *engine_type, void (*callback)(void *storage))
{
	ViewLayerEngineData *sled;

	for (sled = DST.draw_ctx.view_layer->drawdata.first; sled; sled = sled->next) {
		if (sled->engine_type == engine_type) {
			return &sled->storage;
		}
	}

	sled = MEM_callocN(sizeof(ViewLayerEngineData), "ViewLayerEngineData");
	sled->engine_type = engine_type;
	sled->free = callback;
	BLI_addtail(&DST.draw_ctx.view_layer->drawdata, sled);

	return &sled->storage;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Objects (DRW_object)
 * \{ */

ObjectEngineData *DRW_object_engine_data_get(Object *ob, DrawEngineType *engine_type)
{
	for (ObjectEngineData *oed = ob->drawdata.first; oed; oed = oed->next) {
		if (oed->engine_type == engine_type) {
			return oed;
		}
	}
	return NULL;
}

ObjectEngineData *DRW_object_engine_data_ensure(
        Object *ob,
        DrawEngineType *engine_type,
        size_t size,
        ObjectEngineDataInitCb init_cb,
        ObjectEngineDataFreeCb free_cb)
{
	BLI_assert(size >= sizeof(ObjectEngineData));
	/* Try to re-use existing data. */
	ObjectEngineData *oed = DRW_object_engine_data_get(ob, engine_type);
	if (oed != NULL) {
		return oed;
	}
	/* Allocate new data. */
	if ((ob->base_flag & BASE_FROMDUPLI) != 0) {
		/* NOTE: data is not persistent in this case. It is reset each redraw. */
		BLI_assert(free_cb == NULL); /* No callback allowed. */
		/* Round to sizeof(float) for DRW_instance_data_request(). */
		const size_t t = sizeof(float) - 1;
		size = (size + t) & ~t;
		size_t fsize = size / sizeof(float);
		if (DST.common_instance_data[fsize] == NULL) {
			DST.common_instance_data[fsize] = DRW_instance_data_request(DST.idatalist, fsize, 16);
		}
		oed = (ObjectEngineData *)DRW_instance_data_next(DST.common_instance_data[fsize]);
		memset(oed, 0, size);
	}
	else {
		oed = MEM_callocN(size, "ObjectEngineData");
	}
	oed->engine_type = engine_type;
	oed->free = free_cb;
	/* Perform user-side initialization, if needed. */
	if (init_cb != NULL) {
		init_cb(oed);
	}
	/* Register in the list. */
	BLI_addtail(&ob->drawdata, oed);
	return oed;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Rendering (DRW_engines)
 * \{ */

static void drw_engines_init(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);
		PROFILE_START(stime);

		if (engine->engine_init) {
			engine->engine_init(data);
		}

		PROFILE_END_UPDATE(data->init_time, stime);
	}
}

static void drw_engines_cache_init(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);

		if (data->text_draw_cache) {
			DRW_text_cache_destroy(data->text_draw_cache);
			data->text_draw_cache = NULL;
		}
		if (DST.text_store_p == NULL) {
			DST.text_store_p = &data->text_draw_cache;
		}

		if (engine->cache_init) {
			engine->cache_init(data);
		}
	}
}

static void drw_engines_cache_populate(Object *ob)
{
	DST.ob_state = NULL;

	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);

		if (engine->id_update) {
			engine->id_update(data, &ob->id);
		}

		if (engine->cache_populate) {
			engine->cache_populate(data, ob);
		}
	}
}

static void drw_engines_cache_finish(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);

		if (engine->cache_finish) {
			engine->cache_finish(data);
		}
	}
}

static void drw_engines_draw_background(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);

		if (engine->draw_background) {
			PROFILE_START(stime);

			DRW_stats_group_start(engine->idname);
			engine->draw_background(data);
			DRW_stats_group_end();

			PROFILE_END_UPDATE(data->background_time, stime);
			return;
		}
	}

	/* No draw_background found, doing default background */
	if (DRW_state_draw_background()) {
		DRW_draw_background();
	}
}

static void drw_engines_draw_scene(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);
		PROFILE_START(stime);

		if (engine->draw_scene) {
			DRW_stats_group_start(engine->idname);
			engine->draw_scene(data);
			/* Restore for next engine */
			if (DRW_state_is_fbo()) {
				GPU_framebuffer_bind(DST.default_framebuffer);
			}
			DRW_stats_group_end();
		}

		PROFILE_END_UPDATE(data->render_time, stime);
	}
}

static void drw_engines_draw_text(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);
		PROFILE_START(stime);

		if (data->text_draw_cache) {
			DRW_text_cache_draw(data->text_draw_cache, DST.draw_ctx.v3d, DST.draw_ctx.ar, false);
		}

		PROFILE_END_UPDATE(data->render_time, stime);
	}
}

#define MAX_INFO_LINES 10

/**
 * Returns the offset required for the drawing of engines info.
 */
int DRW_draw_region_engine_info_offset(void)
{
	int lines = 0;
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);

		/* Count the number of lines. */
		if (data->info[0] != '\0') {
			lines++;
			char *c = data->info;
			while (*c++ != '\0') {
				if (*c == '\n') {
					lines++;
				}
			}
		}
	}
	return MIN2(MAX_INFO_LINES, lines) * UI_UNIT_Y;
}

/**
 * Actual drawing;
 */
void DRW_draw_region_engine_info(void)
{
	const char *info_array_final[MAX_INFO_LINES + 1];
	/* This should be maxium number of engines running at the same time. */
	char info_array[MAX_INFO_LINES][GPU_INFO_SIZE];
	int i = 0;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ARegion *ar = draw_ctx->ar;
	float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.25f};

	UI_GetThemeColor3fv(TH_HIGH_GRAD, fill_color);
	mul_v3_fl(fill_color, fill_color[3]);

	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);

		if (data->info[0] != '\0') {
			char *chr_current = data->info;
			char *chr_start = chr_current;
			int line_len = 0;

			while (*chr_current++ != '\0') {
				line_len++;
				if (*chr_current == '\n') {
					BLI_strncpy(info_array[i++], chr_start, line_len + 1);
					/* Re-start counting. */
					chr_start = chr_current + 1;
					line_len = -1;
				}
			}

			BLI_strncpy(info_array[i++], chr_start, line_len + 1);

			if (i >= MAX_INFO_LINES) {
				break;
			}
		}
	}

	for (int j = 0; j < i; j++) {
		info_array_final[j] = info_array[j];
	}
	info_array_final[i] = NULL;

	if (info_array[0] != NULL) {
		ED_region_info_draw_multiline(ar, info_array_final, fill_color, true);
	}
}

#undef MAX_INFO_LINES

static void use_drw_engine(DrawEngineType *engine)
{
	LinkData *ld = MEM_callocN(sizeof(LinkData), "enabled engine link data");
	ld->data = engine;
	BLI_addtail(&DST.enabled_engines, ld);
}

/**
 * Use for external render engines.
 */
static void drw_engines_enable_external(void)
{
	use_drw_engine(DRW_engine_viewport_external_type.draw_engine);
}

/* TODO revisit this when proper layering is implemented */
/* Gather all draw engines needed and store them in DST.enabled_engines
 * That also define the rendering order of engines */
static void drw_engines_enable_from_engine(RenderEngineType *engine_type, int drawtype, int drawtype_lighting)
{
	switch (drawtype) {
		case OB_WIRE:
			break;

		case OB_SOLID:
			if (drawtype_lighting == V3D_LIGHTING_FLAT) {
				use_drw_engine(&draw_engine_workbench_solid_flat);

			} 
			else if (drawtype_lighting == V3D_LIGHTING_STUDIO) {
				use_drw_engine(&draw_engine_workbench_solid_studio);

			}
			break;

#ifdef WITH_CLAY_ENGINE
		case OB_CLAY:
			use_drw_engine(&draw_engine_clay_type);
			break;
#endif

		case OB_TEXTURE:
		case OB_MATERIAL:
		case OB_RENDER:
		default:
			/* TODO layers */
			if (engine_type->draw_engine != NULL) {
				use_drw_engine(engine_type->draw_engine);
			}

			if ((engine_type->flag & RE_INTERNAL) == 0) {
				drw_engines_enable_external();
			}
			break;
	}
}

static void drw_engines_enable_from_object_mode(void)
{
	use_drw_engine(&draw_engine_object_type);
}

static void drw_engines_enable_from_mode(int mode)
{
	switch (mode) {
		case CTX_MODE_EDIT_MESH:
			use_drw_engine(&draw_engine_edit_mesh_type);
			break;
		case CTX_MODE_EDIT_CURVE:
			use_drw_engine(&draw_engine_edit_curve_type);
			break;
		case CTX_MODE_EDIT_SURFACE:
			use_drw_engine(&draw_engine_edit_surface_type);
			break;
		case CTX_MODE_EDIT_TEXT:
			use_drw_engine(&draw_engine_edit_text_type);
			break;
		case CTX_MODE_EDIT_ARMATURE:
			use_drw_engine(&draw_engine_edit_armature_type);
			break;
		case CTX_MODE_EDIT_METABALL:
			use_drw_engine(&draw_engine_edit_metaball_type);
			break;
		case CTX_MODE_EDIT_LATTICE:
			use_drw_engine(&draw_engine_edit_lattice_type);
			break;
		case CTX_MODE_POSE:
			use_drw_engine(&draw_engine_pose_type);
			break;
		case CTX_MODE_SCULPT:
			use_drw_engine(&draw_engine_sculpt_type);
			break;
		case CTX_MODE_PAINT_WEIGHT:
			use_drw_engine(&draw_engine_pose_type);
			use_drw_engine(&draw_engine_paint_weight_type);
			break;
		case CTX_MODE_PAINT_VERTEX:
			use_drw_engine(&draw_engine_paint_vertex_type);
			break;
		case CTX_MODE_PAINT_TEXTURE:
			use_drw_engine(&draw_engine_paint_texture_type);
			break;
		case CTX_MODE_PARTICLE:
			use_drw_engine(&draw_engine_particle_type);
			break;
		case CTX_MODE_OBJECT:
			break;
		default:
			BLI_assert(!"Draw mode invalid");
			break;
	}
}

static void drw_engines_enable_from_overlays(int draw_overlays) {
	if (draw_overlays) {
		use_drw_engine(&draw_engine_overlay_type);
	}
}
/**
 * Use for select and depth-drawing.
 */
static void drw_engines_enable_basic(void)
{
	use_drw_engine(DRW_engine_viewport_basic_type.draw_engine);
}

static void drw_engines_enable(ViewLayer *view_layer, RenderEngineType *engine_type)
{
	Object *obact = OBACT(view_layer);
	const int mode = CTX_data_mode_enum_ex(DST.draw_ctx.object_edit, obact, DST.draw_ctx.object_mode);
	View3D * v3d = DST.draw_ctx.v3d;
	const int drawtype = v3d->drawtype;
	const int drawtype_lighting = v3d->drawtype_lighting;

	drw_engines_enable_from_engine(engine_type, drawtype, drawtype_lighting);

	if (DRW_state_draw_support()) {
		drw_engines_enable_from_overlays(v3d->overlays);
		drw_engines_enable_from_object_mode();
		drw_engines_enable_from_mode(mode);
	}
}

static void drw_engines_disable(void)
{
	BLI_freelistN(&DST.enabled_engines);
}

static unsigned int DRW_engines_get_hash(void)
{
	unsigned int hash = 0;
	/* The cache depends on enabled engines */
	/* FIXME : if collision occurs ... segfault */
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		hash += BLI_ghashutil_strhash_p(engine->idname);
	}

	return hash;
}

/* -------------------------------------------------------------------- */

/** \name View Update
 * \{ */

void DRW_notify_view_update(const DRWUpdateContext *update_ctx)
{
	RenderEngineType *engine_type = update_ctx->engine_type;
	ARegion *ar = update_ctx->ar;
	View3D *v3d = update_ctx->v3d;
	RegionView3D *rv3d = ar->regiondata;
	Depsgraph *depsgraph = update_ctx->depsgraph;
	Scene *scene = update_ctx->scene;
	ViewLayer *view_layer = update_ctx->view_layer;

	if (rv3d->viewport == NULL) {
		return;
	}

	/* XXX Really nasty locking. But else this could
	 * be executed by the material previews thread
	 * while rendering a viewport. */
	BLI_mutex_lock(&DST.ogl_context_mutex);

	/* Reset before using it. */
	drw_state_prepare_clean_for_draw(&DST);

	DST.viewport = rv3d->viewport;
	DST.draw_ctx = (DRWContextState){
		.ar = ar, .rv3d = rv3d, .v3d = v3d,
		.scene = scene, .view_layer = view_layer, .obact = OBACT(view_layer),
		.engine_type = engine_type,
		.depsgraph = depsgraph, .object_mode = OB_MODE_OBJECT,
	};

	drw_engines_enable(view_layer, engine_type);

	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *draw_engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(draw_engine);

		if (draw_engine->view_update) {
			draw_engine->view_update(data);
		}
	}

	DST.viewport = NULL;

	drw_engines_disable();

	BLI_mutex_unlock(&DST.ogl_context_mutex);
}

/** \} */

/** \name ID Update
 * \{ */

/* TODO(sergey): This code is run for each changed ID (including the ones which
 * are changed indirectly via update flush. Need to find a way to make this to
 * run really fast, hopefully without any memory allocations on a heap
 * Idea here could be to run every known engine's id_update() and make them
 * do nothing if there is no engine-specific data yet.
 */
void DRW_notify_id_update(const DRWUpdateContext *update_ctx, ID *id)
{
	RenderEngineType *engine_type = update_ctx->engine_type;
	ARegion *ar = update_ctx->ar;
	View3D *v3d = update_ctx->v3d;
	RegionView3D *rv3d = ar->regiondata;
	Depsgraph *depsgraph = update_ctx->depsgraph;
	Scene *scene = update_ctx->scene;
	ViewLayer *view_layer = update_ctx->view_layer;
	if (rv3d->viewport == NULL) {
		return;
	}
	/* Reset before using it. */
	drw_state_prepare_clean_for_draw(&DST);
	DST.viewport = rv3d->viewport;
	DST.draw_ctx = (DRWContextState){
		.ar = ar, .rv3d = rv3d, .v3d = v3d,
		.scene = scene, .view_layer = view_layer, .obact = OBACT(view_layer),
		.engine_type = engine_type,
		.depsgraph = depsgraph, .object_mode = OB_MODE_OBJECT,
	};
	drw_engines_enable(view_layer, engine_type);
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *draw_engine = link->data;
		ViewportEngineData *data = drw_viewport_engine_data_ensure(draw_engine);
		if (draw_engine->id_update) {
			draw_engine->id_update(data, id);
		}
	}
	DST.viewport = NULL;
	drw_engines_disable();
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Main Draw Loops (DRW_draw)
 * \{ */

/* Everything starts here.
 * This function takes care of calling all cache and rendering functions
 * for each relevant engine / mode engine. */
void DRW_draw_view(const bContext *C)
{
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->drawtype);

	/* Reset before using it. */
	drw_state_prepare_clean_for_draw(&DST);
	DRW_draw_render_loop_ex(depsgraph, engine_type, ar, v3d, C);
}

/**
 * Used for both regular and off-screen drawing.
 * Need to reset DST before calling this function
 */
void DRW_draw_render_loop_ex(
        struct Depsgraph *depsgraph,
        RenderEngineType *engine_type,
        ARegion *ar, View3D *v3d,
        const bContext *evil_C)
{

	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
	RegionView3D *rv3d = ar->regiondata;

	DST.draw_ctx.evil_C = evil_C;

	DST.viewport = rv3d->viewport;
	v3d->zbuf = true;

	/* Setup viewport */
	GPU_viewport_engines_data_validate(DST.viewport, DRW_engines_get_hash());

	DST.draw_ctx = (DRWContextState){
	    .ar = ar, .rv3d = rv3d, .v3d = v3d,
	    .scene = scene, .view_layer = view_layer, .obact = OBACT(view_layer),
	    .engine_type = engine_type,
	    .depsgraph = depsgraph,

	    /* reuse if caller sets */
	    .evil_C = DST.draw_ctx.evil_C,
	};
	drw_context_state_init();
	drw_viewport_var_init();

	/* Get list of enabled engines */
	drw_engines_enable(view_layer, engine_type);

	/* Update ubos */
	DRW_globals_update();

	/* No framebuffer allowed before drawing. */
	BLI_assert(GPU_framebuffer_current_get() == 0);

	/* Init engines */
	drw_engines_init();

	/* Cache filling */
	{
		PROFILE_START(stime);
		drw_engines_cache_init();

		DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN(depsgraph, ob, DRW_iterator_mode_get())
		{
			drw_engines_cache_populate(ob);
		}
		DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;

		drw_engines_cache_finish();

		DRW_render_instance_buffer_finish();

#ifdef USE_PROFILE
		double *cache_time = GPU_viewport_cache_time_get(DST.viewport);
		PROFILE_END_UPDATE(*cache_time, stime);
#endif
	}

	DRW_stats_begin();

	GPU_framebuffer_bind(DST.default_framebuffer);

	/* Start Drawing */
	DRW_state_reset();

	drw_engines_draw_background();

	/* WIP, single image drawn over the camera view (replace) */
	bool do_bg_image = false;
	if (rv3d->persp == RV3D_CAMOB) {
		Object *cam_ob = v3d->camera;
		if (cam_ob && cam_ob->type == OB_CAMERA) {
			Camera *cam = cam_ob->data;
			if (!BLI_listbase_is_empty(&cam->bg_images)) {
				do_bg_image = true;
			}
		}
	}

	if (do_bg_image) {
		ED_view3d_draw_bgpic_test(scene, depsgraph, ar, v3d, false, true);
	}


	DRW_draw_callbacks_pre_scene();
	if (DST.draw_ctx.evil_C) {
		ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.ar, REGION_DRAW_PRE_VIEW);
	}

	drw_engines_draw_scene();

	DRW_draw_callbacks_post_scene();
	if (DST.draw_ctx.evil_C) {
		ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.ar, REGION_DRAW_POST_VIEW);
	}

	DRW_state_reset();

	drw_engines_draw_text();

	if (DST.draw_ctx.evil_C) {
		/* needed so manipulator isn't obscured */
		glDisable(GL_DEPTH_TEST);
		DRW_draw_manipulator_3d();

		DRW_draw_region_info();

		/* Draw 2D after region info so we can draw on top of the camera passepartout overlay.
		 * 'DRW_draw_region_info' sets the projection in pixel-space. */
		DRW_draw_manipulator_2d();
		glEnable(GL_DEPTH_TEST);
	}

	DRW_stats_reset();

	if (do_bg_image) {
		ED_view3d_draw_bgpic_test(scene, depsgraph, ar, v3d, true, true);
	}

	if (G.debug_value > 20) {
		glDisable(GL_DEPTH_TEST);
		rcti rect; /* local coordinate visible rect inside region, to accomodate overlapping ui */
		ED_region_visible_rect(DST.draw_ctx.ar, &rect);
		DRW_stats_draw(&rect);
		glEnable(GL_DEPTH_TEST);
	}

	GPU_framebuffer_restore();

	DRW_state_reset();
	drw_engines_disable();

	drw_viewport_cache_resize();

#ifdef DEBUG
	/* Avoid accidental reuse. */
	drw_state_ensure_not_reused(&DST);
#endif
}

void DRW_draw_render_loop(
        struct Depsgraph *depsgraph,
        ARegion *ar, View3D *v3d)
{
	/* Reset before using it. */
	drw_state_prepare_clean_for_draw(&DST);

	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->drawtype);

	DRW_draw_render_loop_ex(depsgraph, engine_type, ar, v3d, NULL);
}

/* @viewport CAN be NULL, in this case we create one. */
void DRW_draw_render_loop_offscreen(
        struct Depsgraph *depsgraph, RenderEngineType *engine_type,
        ARegion *ar, View3D *v3d,
        const bool draw_background, GPUOffScreen *ofs,
        GPUViewport *viewport)
{
	RegionView3D *rv3d = ar->regiondata;

	/* backup */
	void *backup_viewport = rv3d->viewport;
	{
		/* backup (_never_ use rv3d->viewport) */
		if (viewport == NULL) {
			rv3d->viewport = GPU_viewport_create_from_offscreen(ofs);
		}
		else {
			rv3d->viewport = viewport;
		}
	}

	GPU_framebuffer_restore();

	/* Reset before using it. */
	drw_state_prepare_clean_for_draw(&DST);
	DST.options.is_image_render = true;
	DST.options.draw_background = draw_background;
	DRW_draw_render_loop_ex(depsgraph, engine_type, ar, v3d, NULL);

	/* restore */
	{
		if (viewport == NULL) {
			/* don't free data owned by 'ofs' */
			GPU_viewport_clear_from_offscreen(rv3d->viewport);
			GPU_viewport_free(rv3d->viewport);
		}

		rv3d->viewport = backup_viewport;
	}

	/* we need to re-bind (annoying!) */
	GPU_offscreen_bind(ofs, false);
}

void DRW_render_to_image(RenderEngine *engine, struct Depsgraph *depsgraph)
{
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
	RenderEngineType *engine_type = engine->type;
	DrawEngineType *draw_engine_type = engine_type->draw_engine;
	RenderData *r = &scene->r;
	Render *render = engine->re;
	/* Changing Context */
	DRW_opengl_context_enable();
	/* IMPORTANT: We dont support immediate mode in render mode!
	 * This shall remain in effect until immediate mode supports
	 * multiple threads. */

	/* Reset before using it. */
	drw_state_prepare_clean_for_draw(&DST);
	DST.options.is_image_render = true;
	DST.options.is_scene_render = true;
	DST.options.draw_background = scene->r.alphamode == R_ADDSKY;

	DST.draw_ctx = (DRWContextState){
	    .scene = scene, .view_layer = view_layer,
	    .engine_type = engine_type,
	    .depsgraph = depsgraph, .object_mode = OB_MODE_OBJECT,
	};
	drw_context_state_init();

	DST.viewport = GPU_viewport_create();
	const int size[2] = {(r->size * r->xsch) / 100, (r->size * r->ysch) / 100};
	GPU_viewport_size_set(DST.viewport, size);

	drw_viewport_var_init();

	ViewportEngineData *data = drw_viewport_engine_data_ensure(draw_engine_type);

	/* set default viewport */
	glViewport(0, 0, size[0], size[1]);

	/* Main rendering. */
	rctf view_rect;
	rcti render_rect;
	RE_GetViewPlane(render, &view_rect, &render_rect);
	if (BLI_rcti_is_empty(&render_rect)) {
		BLI_rcti_init(&render_rect, 0, size[0], 0, size[1]);
	}

	/* Init render result. */
	RenderResult *render_result = RE_engine_begin_result(
	        engine,
	        0,
	        0,
	        (int)size[0],
	        (int)size[1],
	        view_layer->name,
	        /* RR_ALL_VIEWS */ NULL);

	RenderLayer *render_layer = render_result->layers.first;
	for (RenderView *render_view = render_result->views.first;
	     render_view != NULL;
	     render_view = render_view->next)
	{
		RE_SetActiveRenderView(render, render_view->name);
		engine_type->draw_engine->render_to_image(data, engine, render_layer, &render_rect);
		DST.buffer_finish_called = false;
	}

	RE_engine_end_result(engine, render_result, false, false, false);

	/* Force cache to reset. */
	drw_viewport_cache_resize();

	/* TODO grease pencil */

	GPU_viewport_free(DST.viewport);
	GPU_framebuffer_restore();

	/* Changing Context */
	DRW_opengl_context_disable();

#ifdef DEBUG
	/* Avoid accidental reuse. */
	drw_state_ensure_not_reused(&DST);
#endif
}

void DRW_render_object_iter(
	void *vedata, RenderEngine *engine, struct Depsgraph *depsgraph,
	void (*callback)(void *vedata, Object *ob, RenderEngine *engine, struct Depsgraph *depsgraph))
{
	DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN(depsgraph, ob, DRW_iterator_mode_get())
	{
		DST.ob_state = NULL;
		callback(vedata, ob, engine, depsgraph);
	}
	DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END
}

static struct DRWSelectBuffer {
	struct GPUFrameBuffer *framebuffer;
	struct GPUTexture *texture_depth;
} g_select_buffer = {NULL};

static void draw_select_framebuffer_setup(const rcti *rect)
{
	if (g_select_buffer.framebuffer == NULL) {
		g_select_buffer.framebuffer = GPU_framebuffer_create();
	}

	/* If size mismatch recreate the texture. */
	if ((g_select_buffer.texture_depth != NULL) &&
	    ((GPU_texture_width(g_select_buffer.texture_depth) != BLI_rcti_size_x(rect)) ||
	     (GPU_texture_height(g_select_buffer.texture_depth) != BLI_rcti_size_y(rect))))
	{
		GPU_texture_free(g_select_buffer.texture_depth);
		g_select_buffer.texture_depth = NULL;
	}

	if (g_select_buffer.texture_depth == NULL) {
		g_select_buffer.texture_depth = GPU_texture_create_depth(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), NULL);

		GPU_framebuffer_texture_attach(g_select_buffer.framebuffer, g_select_buffer.texture_depth, 0, 0);

		if (!GPU_framebuffer_check_valid(g_select_buffer.framebuffer, NULL)) {
			printf("Error invalid selection framebuffer\n");
		}
	}
}

/* Must run after all instance datas have been added. */
void DRW_render_instance_buffer_finish(void)
{
	BLI_assert(!DST.buffer_finish_called && "DRW_render_instance_buffer_finish called twice!");
	DST.buffer_finish_called = true;
	DRW_instance_buffer_finish(DST.idatalist);
}

/**
 * object mode select-loop, see: ED_view3d_draw_select_loop (legacy drawing).
 */
void DRW_draw_select_loop(
        struct Depsgraph *depsgraph,
        ARegion *ar, View3D *v3d,
        bool UNUSED(use_obedit_skip), bool UNUSED(use_nearest), const rcti *rect,
        DRW_SelectPassFn select_pass_fn, void *select_pass_user_data)
{
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->drawtype);
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
	Object *obact = OBACT(view_layer);
	Object *obedit = OBEDIT_FROM_OBACT(obact);
#ifndef USE_GPU_SELECT
	UNUSED_VARS(vc, scene, view_layer, v3d, ar, rect);
#else
	RegionView3D *rv3d = ar->regiondata;

	/* Reset before using it. */
	drw_state_prepare_clean_for_draw(&DST);

	/* backup (_never_ use rv3d->viewport) */
	void *backup_viewport = rv3d->viewport;
	rv3d->viewport = NULL;

	bool use_obedit = false;
	int obedit_mode = 0;
	if (obedit != NULL) {
		if (obedit->type == OB_MBALL) {
			use_obedit = true;
			obedit_mode = CTX_MODE_EDIT_METABALL;
		}
		else if (obedit->type == OB_ARMATURE) {
			use_obedit = true;
			obedit_mode = CTX_MODE_EDIT_ARMATURE;
		}
	}

	struct GPUViewport *viewport = GPU_viewport_create();
	GPU_viewport_size_set(viewport, (const int[2]){BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)});

	DST.viewport = viewport;
	v3d->zbuf = true;

	DST.options.is_select = true;

	/* Get list of enabled engines */
	if (use_obedit) {
		drw_engines_enable_from_mode(obedit_mode);
	}
	else {
		drw_engines_enable_basic();
		drw_engines_enable_from_object_mode();
	}

	/* Setup viewport */

	/* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
	DST.draw_ctx = (DRWContextState){
		.ar = ar, .rv3d = rv3d, .v3d = v3d,
		.scene = scene, .view_layer = view_layer, .obact = obact,
		.engine_type = engine_type,
		.depsgraph = depsgraph,
	};
	drw_context_state_init();
	drw_viewport_var_init();

	/* Update ubos */
	DRW_globals_update();

	/* Init engines */
	drw_engines_init();

	{
		drw_engines_cache_init();

		if (use_obedit) {
#if 0
			drw_engines_cache_populate(obact);
#else
			FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, obact->mode, ob_iter) {
				drw_engines_cache_populate(ob_iter);
			}
			FOREACH_OBJECT_IN_MODE_END;
#endif
		}
		else {
			DEG_OBJECT_ITER_BEGIN(
			        depsgraph, ob, DRW_iterator_mode_get(),
			        DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
			        DEG_ITER_OBJECT_FLAG_VISIBLE |
			        DEG_ITER_OBJECT_FLAG_DUPLI)
			{
				if ((ob->base_flag & BASE_SELECTABLED) != 0) {
					DRW_select_load_id(ob->select_color);
					drw_engines_cache_populate(ob);
				}
			}
			DEG_OBJECT_ITER_END;
		}

		drw_engines_cache_finish();

		DRW_render_instance_buffer_finish();
	}

	/* Setup framebuffer */
	draw_select_framebuffer_setup(rect);
	GPU_framebuffer_bind(g_select_buffer.framebuffer);
	GPU_framebuffer_clear_depth(g_select_buffer.framebuffer, 1.0f);

	/* Start Drawing */
	DRW_state_reset();
	DRW_draw_callbacks_pre_scene();

	DRW_state_lock(
	        DRW_STATE_WRITE_DEPTH |
	        DRW_STATE_DEPTH_ALWAYS |
	        DRW_STATE_DEPTH_LESS |
	        DRW_STATE_DEPTH_EQUAL |
	        DRW_STATE_DEPTH_GREATER |
	        DRW_STATE_DEPTH_ALWAYS);

	/* Only 1-2 passes. */
	while (true) {
		if (!select_pass_fn(DRW_SELECT_PASS_PRE, select_pass_user_data)) {
			break;
		}

		drw_engines_draw_scene();

		if (!select_pass_fn(DRW_SELECT_PASS_POST, select_pass_user_data)) {
			break;
		}
	}

	DRW_state_lock(0);

	DRW_draw_callbacks_post_scene();

	DRW_state_reset();
	drw_engines_disable();

#ifdef DEBUG
	/* Avoid accidental reuse. */
	drw_state_ensure_not_reused(&DST);
#endif
	GPU_framebuffer_restore();

	/* Cleanup for selection state */
	GPU_viewport_free(viewport);

	/* restore */
	rv3d->viewport = backup_viewport;
#endif  /* USE_GPU_SELECT */
}

static void draw_depth_texture_to_screen(GPUTexture *texture)
{
	const float w = (float)GPU_texture_width(texture);
	const float h = (float)GPU_texture_height(texture);

	Gwn_VertFormat *format = immVertexFormat();
	unsigned int texcoord = GWN_vertformat_attr_add(format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_DEPTH_COPY);

	GPU_texture_bind(texture, 0);

	immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

	immBegin(GWN_PRIM_TRI_STRIP, 4);

	immAttrib2f(texcoord, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, 0.0f);

	immAttrib2f(texcoord, 1.0f, 0.0f);
	immVertex2f(pos, w, 0.0f);

	immAttrib2f(texcoord, 0.0f, 1.0f);
	immVertex2f(pos, 0.0f, h);

	immAttrib2f(texcoord, 1.0f, 1.0f);
	immVertex2f(pos, w, h);

	immEnd();

	GPU_texture_unbind(texture);

	immUnbindProgram();
}

/**
 * object mode select-loop, see: ED_view3d_draw_depth_loop (legacy drawing).
 */
void DRW_draw_depth_loop(
        Depsgraph *depsgraph,
        ARegion *ar, View3D *v3d)
{
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->drawtype);
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
	RegionView3D *rv3d = ar->regiondata;

	DRW_opengl_context_enable();

	/* backup (_never_ use rv3d->viewport) */
	void *backup_viewport = rv3d->viewport;
	rv3d->viewport = NULL;

	/* Reset before using it. */
	drw_state_prepare_clean_for_draw(&DST);

	struct GPUViewport *viewport = GPU_viewport_create();
	GPU_viewport_size_set(viewport, (const int[2]){ar->winx, ar->winy});

	/* Setup framebuffer */
	draw_select_framebuffer_setup(&ar->winrct);
	GPU_framebuffer_bind(g_select_buffer.framebuffer);
	GPU_framebuffer_clear_depth(g_select_buffer.framebuffer, 1.0f);

	bool cache_is_dirty;
	DST.viewport = viewport;
	v3d->zbuf = true;

	DST.options.is_depth = true;

	/* Get list of enabled engines */
	{
		drw_engines_enable_basic();
		drw_engines_enable_from_object_mode();
	}

	/* Setup viewport */
	cache_is_dirty = true;

	/* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
	DST.draw_ctx = (DRWContextState){
		.ar = ar, .rv3d = rv3d, .v3d = v3d,
		.scene = scene, .view_layer = view_layer, .obact = OBACT(view_layer),
		.engine_type = engine_type,
		.depsgraph = depsgraph,
	};
	drw_context_state_init();
	drw_viewport_var_init();

	/* Update ubos */
	DRW_globals_update();

	/* Init engines */
	drw_engines_init();

	/* TODO : tag to refresh by the dependency graph */
	/* ideally only refresh when objects are added/removed */
	/* or render properties / materials change */
	if (cache_is_dirty) {
		drw_engines_cache_init();

		DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN(depsgraph, ob, DRW_iterator_mode_get())
		{
			drw_engines_cache_populate(ob);
		}
		DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;

		drw_engines_cache_finish();

		DRW_render_instance_buffer_finish();
	}

	/* Start Drawing */
	DRW_state_reset();
	DRW_draw_callbacks_pre_scene();
	drw_engines_draw_scene();
	DRW_draw_callbacks_post_scene();

	DRW_state_reset();
	drw_engines_disable();

#ifdef DEBUG
	/* Avoid accidental reuse. */
	drw_state_ensure_not_reused(&DST);
#endif

	/* TODO: Reading depth for operators should be done here. */

	GPU_framebuffer_restore();

	/* Cleanup for selection state */
	GPU_viewport_free(viewport);

	/* Changin context */
	DRW_opengl_context_disable();

	/* XXX Drawing the resulting buffer to the BACK_BUFFER */
	gpuPushMatrix();
	gpuPushProjectionMatrix();
	wmOrtho2_region_pixelspace(ar);
	gpuLoadIdentity();

	glEnable(GL_DEPTH_TEST); /* Cannot write to depth buffer without testing */
	glDepthFunc(GL_ALWAYS);
	draw_depth_texture_to_screen(g_select_buffer.texture_depth);
	glDepthFunc(GL_LEQUAL);

	gpuPopMatrix();
	gpuPopProjectionMatrix();

	/* restore */
	rv3d->viewport = backup_viewport;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Draw Manager State (DRW_state)
 * \{ */

void DRW_state_dfdy_factors_get(float dfdyfac[2])
{
	GPU_get_dfdy_factors(dfdyfac);
}

/**
 * When false, drawing doesn't output to a pixel buffer
 * eg: Occlusion queries, or when we have setup a context to draw in already.
 */
bool DRW_state_is_fbo(void)
{
	return ((DST.default_framebuffer != NULL) || DST.options.is_image_render);
}

/**
 * For when engines need to know if this is drawing for selection or not.
 */
bool DRW_state_is_select(void)
{
	return DST.options.is_select;
}

bool DRW_state_is_depth(void)
{
	return DST.options.is_depth;
}

/**
 * Whether we are rendering for an image
 */
bool DRW_state_is_image_render(void)
{
	return DST.options.is_image_render;
}

/**
 * Whether we are rendering only the render engine,
 * or if we should also render the mode engines.
 */
bool DRW_state_is_scene_render(void)
{
	BLI_assert(DST.options.is_scene_render ?
	           DST.options.is_image_render : true);
	return DST.options.is_scene_render;
}

/**
* Whether we are rendering simple opengl render
*/
bool DRW_state_is_opengl_render(void)
{
	return DST.options.is_image_render && !DST.options.is_scene_render;
}

/**
 * Gives you the iterator mode to use for depsgraph.
 */
eDepsObjectIteratorMode DRW_iterator_mode_get(void)
{
	return DRW_state_is_scene_render() ? DEG_ITER_OBJECT_MODE_RENDER :
	                                     DEG_ITER_OBJECT_MODE_VIEWPORT;
}

/**
 * Should text draw in this mode?
 */
bool DRW_state_show_text(void)
{
	return (DST.options.is_select) == 0 &&
	       (DST.options.is_depth) == 0 &&
	       (DST.options.is_scene_render) == 0;
}

/**
 * Should draw support elements
 * Objects center, selection outline, probe data, ...
 */
bool DRW_state_draw_support(void)
{
	View3D *v3d = DST.draw_ctx.v3d;
	return (DRW_state_is_scene_render() == false) &&
	        (v3d != NULL) &&
	        ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0);
}

/**
 * Whether we should render the background
 */
bool DRW_state_draw_background(void)
{
	if (DRW_state_is_image_render() == false) {
		return true;
	}
	return DST.options.draw_background;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Context State (DRW_context_state)
 * \{ */

const DRWContextState *DRW_context_state_get(void)
{
	return &DST.draw_ctx;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Init/Exit (DRW_engines)
 * \{ */

bool DRW_engine_render_support(DrawEngineType *draw_engine_type)
{
	return draw_engine_type->render_to_image;
}

void DRW_engine_register(DrawEngineType *draw_engine_type)
{
	BLI_addtail(&DRW_engines, draw_engine_type);
}

void DRW_engines_register(void)
{
#ifdef WITH_CLAY_ENGINE
	RE_engines_register(NULL, &DRW_engine_viewport_clay_type);
#endif
	RE_engines_register(NULL, &DRW_engine_viewport_eevee_type);
	RE_engines_register(NULL, &DRW_engine_viewport_workbench_type);

	DRW_engine_register(&draw_engine_workbench_solid_flat);
	DRW_engine_register(&draw_engine_workbench_solid_studio);

	DRW_engine_register(&draw_engine_object_type);
	DRW_engine_register(&draw_engine_edit_armature_type);
	DRW_engine_register(&draw_engine_edit_curve_type);
	DRW_engine_register(&draw_engine_edit_lattice_type);
	DRW_engine_register(&draw_engine_edit_mesh_type);
	DRW_engine_register(&draw_engine_edit_metaball_type);
	DRW_engine_register(&draw_engine_edit_surface_type);
	DRW_engine_register(&draw_engine_edit_text_type);
	DRW_engine_register(&draw_engine_overlay_type);
	DRW_engine_register(&draw_engine_paint_texture_type);
	DRW_engine_register(&draw_engine_paint_vertex_type);
	DRW_engine_register(&draw_engine_paint_weight_type);
	DRW_engine_register(&draw_engine_particle_type);
	DRW_engine_register(&draw_engine_pose_type);
	DRW_engine_register(&draw_engine_sculpt_type);

	/* setup callbacks */
	{
		/* BKE: mball.c */
		extern void *BKE_mball_batch_cache_dirty_cb;
		extern void *BKE_mball_batch_cache_free_cb;
		/* BKE: curve.c */
		extern void *BKE_curve_batch_cache_dirty_cb;
		extern void *BKE_curve_batch_cache_free_cb;
		/* BKE: mesh.c */
		extern void *BKE_mesh_batch_cache_dirty_cb;
		extern void *BKE_mesh_batch_cache_free_cb;
		/* BKE: lattice.c */
		extern void *BKE_lattice_batch_cache_dirty_cb;
		extern void *BKE_lattice_batch_cache_free_cb;
		/* BKE: particle.c */
		extern void *BKE_particle_batch_cache_dirty_cb;
		extern void *BKE_particle_batch_cache_free_cb;

		BKE_mball_batch_cache_dirty_cb = DRW_mball_batch_cache_dirty;
		BKE_mball_batch_cache_free_cb = DRW_mball_batch_cache_free;

		BKE_curve_batch_cache_dirty_cb = DRW_curve_batch_cache_dirty;
		BKE_curve_batch_cache_free_cb = DRW_curve_batch_cache_free;

		BKE_mesh_batch_cache_dirty_cb = DRW_mesh_batch_cache_dirty;
		BKE_mesh_batch_cache_free_cb = DRW_mesh_batch_cache_free;

		BKE_lattice_batch_cache_dirty_cb = DRW_lattice_batch_cache_dirty;
		BKE_lattice_batch_cache_free_cb = DRW_lattice_batch_cache_free;

		BKE_particle_batch_cache_dirty_cb = DRW_particle_batch_cache_dirty;
		BKE_particle_batch_cache_free_cb = DRW_particle_batch_cache_free;
	}
}

extern struct Gwn_VertFormat *g_pos_format; /* draw_shgroup.c */
extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GPUTexture *globals_ramp; /* draw_common.c */
void DRW_engines_free(void)
{
	DRW_opengl_context_enable();

	DRW_TEXTURE_FREE_SAFE(g_select_buffer.texture_depth);
	GPU_FRAMEBUFFER_FREE_SAFE(g_select_buffer.framebuffer);

	DRW_shape_cache_free();
	DRW_stats_free();
	DRW_globals_free();

	DrawEngineType *next;
	for (DrawEngineType *type = DRW_engines.first; type; type = next) {
		next = type->next;
		BLI_remlink(&R_engines, type);

		if (type->engine_free) {
			type->engine_free();
		}
	}

	DRW_UBO_FREE_SAFE(globals_ubo);
	DRW_UBO_FREE_SAFE(view_ubo);
	DRW_TEXTURE_FREE_SAFE(globals_ramp);
	MEM_SAFE_FREE(g_pos_format);

	MEM_SAFE_FREE(DST.RST.bound_texs);
	MEM_SAFE_FREE(DST.RST.bound_tex_slots);
	MEM_SAFE_FREE(DST.RST.bound_ubos);
	MEM_SAFE_FREE(DST.RST.bound_ubo_slots);

	DRW_opengl_context_disable();

#ifdef WITH_CLAY_ENGINE
	BLI_remlink(&R_engines, &DRW_engine_viewport_clay_type);
#endif
}

/** \} */

/** \name Init/Exit (DRW_opengl_ctx)
 * \{ */

void DRW_opengl_context_create(void)
{
	BLI_assert(DST.ogl_context == NULL); /* Ensure it's called once */

	BLI_mutex_init(&DST.ogl_context_mutex);

	immDeactivate();
	/* This changes the active context. */
	DST.ogl_context = WM_opengl_context_create();
	/* Be sure to create gawain.context too. */
	DST.gwn_context = GWN_context_create();
	immActivate();
	/* Set default Blender OpenGL state */
	GPU_state_init();
	/* So we activate the window's one afterwards. */
	wm_window_reset_drawable();
}

void DRW_opengl_context_destroy(void)
{
	BLI_assert(BLI_thread_is_main());
	if (DST.ogl_context != NULL) {
		WM_opengl_context_activate(DST.ogl_context);
		GWN_context_active_set(DST.gwn_context);
		GWN_context_discard(DST.gwn_context);
		WM_opengl_context_dispose(DST.ogl_context);
		BLI_mutex_end(&DST.ogl_context_mutex);
	}
}

void DRW_opengl_context_enable(void)
{
	if (DST.ogl_context != NULL) {
		/* IMPORTANT: We dont support immediate mode in render mode!
		 * This shall remain in effect until immediate mode supports
		 * multiple threads. */
		BLI_mutex_lock(&DST.ogl_context_mutex);
		if (BLI_thread_is_main()) {
			immDeactivate();
		}
		WM_opengl_context_activate(DST.ogl_context);
		GWN_context_active_set(DST.gwn_context);
		if (BLI_thread_is_main()) {
			immActivate();
			BLF_batch_reset();
		}
	}
}

void DRW_opengl_context_disable(void)
{
	if (DST.ogl_context != NULL) {
#ifdef __APPLE__
		/* Need to flush before disabling draw context, otherwise it does not
		 * always finish drawing and viewport can be empty or partially drawn */
		glFlush();
#endif

		if (BLI_thread_is_main()) {
			wm_window_reset_drawable();
		}
		else {
			WM_opengl_context_release(DST.ogl_context);
			GWN_context_active_set(NULL);
		}

		BLI_mutex_unlock(&DST.ogl_context_mutex);
	}
}

/** \} */
