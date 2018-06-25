
#include "workbench_private.h"

static struct {
	struct GPUShader *effect_taa_sh;

} e_data = {NULL};

extern char datatoc_workbench_effect_taa_frag_glsl[];

/*
 * Sub-sample positions for TAA8
 * first sample needs to be at 0.0f, 0.0f
 * as that sample depicts the z-buffer
 */
static const float SAMPLE_LOCS_8[8][2] = {
	{ 0.125f-0.125f,  0.375f-0.375f},
	{-0.625f-0.125f, -0.625f-0.375f},
	{ 0.875f-0.125f,  0.875f-0.375f},
	{-0.875f-0.125f,  0.125f-0.375f},
	{ 0.625f-0.125f, -0.125f-0.375f},
	{-0.375f-0.125f,  0.625f-0.375f},
	{ 0.375f-0.125f, -0.875f-0.375f},
	{-0.125f-0.125f, -0.375f-0.375f},
};

void workbench_taa_engine_init(WORKBENCH_Data *vedata)
{
	WORKBENCH_EffectInfo *effect_info = vedata->stl->effects;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	RegionView3D *rv3d = draw_ctx->rv3d;

	if (e_data.effect_taa_sh == NULL)
	{
		e_data.effect_taa_sh = DRW_shader_create_fullscreen(datatoc_workbench_effect_taa_frag_glsl, NULL);
	}

	/* reset complete drawing when navigating. */
	if (effect_info->jitter_index != 0)
	{
		if (rv3d && rv3d->rflag & RV3D_NAVIGATING)
		{
			effect_info->jitter_index = 0;
		}
	}

	if (effect_info->view_updated)
	{
		effect_info->jitter_index = 0;
		effect_info->view_updated = false;
	}

	{
		float view[4][4];
		float win[4][4];
		DRW_viewport_matrix_get(view, DRW_MAT_VIEW);
		DRW_viewport_matrix_get(win, DRW_MAT_WIN);
		mul_m4_m4m4(effect_info->curr_mat, view, win);
		if(!equals_m4m4(effect_info->curr_mat, effect_info->last_mat)){
			effect_info->jitter_index = 0;
		}
	}
}

void workbench_taa_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.effect_taa_sh);
}


DRWPass *workbench_taa_create_pass(WORKBENCH_TextureList *txl, WORKBENCH_EffectInfo *effect_info, WORKBENCH_FramebufferList *fbl, GPUTexture **color_buffer_tx)
{
	/*
	 * jitter_index is not updated yet. This will be done in during draw phase.
	 * so for now it is inversed.
	 */
	int previous_jitter_index = effect_info->jitter_index;
	bool previous_jitter_even = (previous_jitter_index & 1) == 0;

	{
		DRW_texture_ensure_fullscreen_2D(&txl->history_buffer1_tx, GPU_RGBA16F, 0);
		DRW_texture_ensure_fullscreen_2D(&txl->history_buffer2_tx, GPU_RGBA16F, 0);
		DRW_texture_ensure_fullscreen_2D(&txl->depth_buffer_tx, GPU_DEPTH24_STENCIL8, 0);
	}

	{
		GPU_framebuffer_ensure_config(&fbl->effect_taa_even_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE(txl->history_buffer1_tx),
		});
		GPU_framebuffer_ensure_config(&fbl->effect_taa_uneven_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE(txl->history_buffer2_tx),
		});
		GPU_framebuffer_ensure_config(&fbl->depth_buffer_fb, {
			GPU_ATTACHMENT_TEXTURE(txl->depth_buffer_tx),
		});
	}


	DRWPass *pass = DRW_pass_create("Effect TAA", DRW_STATE_WRITE_COLOR);
	DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_taa_sh, pass);
	DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", color_buffer_tx);
	if (previous_jitter_even) {
		DRW_shgroup_uniform_texture_ref(grp, "historyBuffer", &txl->history_buffer2_tx);
	}
	else {
		DRW_shgroup_uniform_texture_ref(grp, "historyBuffer", &txl->history_buffer1_tx);
	}

	DRW_shgroup_uniform_float(grp, "mixFactor", &effect_info->taa_mix_factor, 1);
	DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);

	if (previous_jitter_even)
	{
		effect_info->final_color_tx = txl->history_buffer1_tx;
		effect_info->final_color_fb = fbl->effect_taa_even_fb;
	}
	else {
		effect_info->final_color_tx = txl->history_buffer2_tx;
		effect_info->final_color_fb = fbl->effect_taa_uneven_fb;
	}
	return pass;
}

void workbench_taa_draw_scene_start(WORKBENCH_EffectInfo *effect_info)
{
	const float *viewport_size = DRW_viewport_size_get();
	const int samples = 8;
	float mix_factor;

	mix_factor = 1.0f / (effect_info->jitter_index + 1);

	const  int bitmask = samples - 1;
	const int jitter_index = effect_info->jitter_index;
	const float *transform_offset = SAMPLE_LOCS_8[jitter_index];
	effect_info->jitter_index = (jitter_index + 1) & bitmask;

	/* construct new matrices from transform delta */
	float viewmat[4][4];
	float persmat[4][4];
	DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
	DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);
	DRW_viewport_matrix_get(effect_info->override_winmat, DRW_MAT_WIN);

	window_translate_m4(
	        effect_info->override_winmat, persmat,
	        transform_offset[0] / viewport_size[0],
	        transform_offset[1] / viewport_size[1]);

	mul_m4_m4m4(effect_info->override_persmat, effect_info->override_winmat, viewmat);
	invert_m4_m4(effect_info->override_persinv, effect_info->override_persmat);
	invert_m4_m4(effect_info->override_wininv, effect_info->override_winmat);

	DRW_viewport_matrix_override_set(effect_info->override_persmat, DRW_MAT_PERS);
	DRW_viewport_matrix_override_set(effect_info->override_persinv, DRW_MAT_PERSINV);
	DRW_viewport_matrix_override_set(effect_info->override_winmat,  DRW_MAT_WIN);
	DRW_viewport_matrix_override_set(effect_info->override_wininv,  DRW_MAT_WININV);

	/* weight the mix factor by the jitter index */
	effect_info->taa_mix_factor = mix_factor;
}

void workbench_taa_draw_scene_end(WORKBENCH_Data *vedata)
{
	/*
	 * If first frame than the offset is 0.0 and its depth is the depth buffer to use
	 * for the rest of the draw engines. We store it in a persistent buffer.
	 *
	 * If it is not the first frame we copy the persistent buffer back to the
	 * default depth buffer
	 */
	const WORKBENCH_StorageList *stl = vedata->stl;
	const WORKBENCH_EffectInfo *effect_info = stl->effects;
	const WORKBENCH_FramebufferList *fbl = vedata->fbl;
	const DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

	if (effect_info->jitter_index == 1)
	{
		GPU_framebuffer_blit(dfbl->depth_only_fb, 0, fbl->depth_buffer_fb, 0, GPU_DEPTH_BIT);
	}
	else {
		GPU_framebuffer_blit(fbl->depth_buffer_fb, 0, dfbl->depth_only_fb, 0, GPU_DEPTH_BIT);
	}



	DRW_viewport_matrix_override_unset_all();
}

void workbench_taa_draw_pass(WORKBENCH_EffectInfo *effect_info, DRWPass *pass)
{
	GPU_framebuffer_bind(effect_info->final_color_fb);
	DRW_draw_pass(pass);

	copy_m4_m4(effect_info->last_mat, effect_info->curr_mat);
	if (effect_info->jitter_index != 0)
	{
		DRW_viewport_request_redraw();
	}
}

void workbench_taa_view_updated(WORKBENCH_Data *vedata)
{
	WORKBENCH_EffectInfo *effect_info = vedata->stl->effects;
	effect_info->view_updated = true;
}
