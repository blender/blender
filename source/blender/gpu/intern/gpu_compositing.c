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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_compositing.c
 *  \ingroup gpu
 *
 * System that manages framebuffer compositing.
 */

#include "BLI_sys_types.h"
#include "BLI_rect.h"
#include "BLI_math.h"

#include "BLI_rand.h"

#include "DNA_vec_types.h"
#include "DNA_scene_types.h"
#include "DNA_gpu_types.h"

#include "GPU_compositing.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "MEM_guardedalloc.h"

static const float fullscreencos[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}};
static const float fullscreenuvs[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};


/* shader interfaces (legacy GL 2 style, without uniform buffer objects) */

typedef struct {
	int ssao_uniform;
	int ssao_color_uniform;
	int color_uniform;
	int depth_uniform;
	int viewvecs_uniform;
	int ssao_sample_params_uniform;
	int ssao_concentric_tex;
	int ssao_jitter_uniform;
} GPUSSAOShaderInterface;

typedef struct {
	int invrendertargetdim_uniform;
	int color_uniform;
	int dof_uniform;
	int depth_uniform;
	int viewvecs_uniform;
} GPUDOFHQPassOneInterface;

typedef struct {
	int rendertargetdim_uniform;
	int color_uniform;
	int coc_uniform;
	int select_uniform;
	int dof_uniform;
} GPUDOFHQPassTwoInterface;

typedef struct {
	int dof_uniform;
	int invrendertargetdim_uniform;
	int color_uniform;
	int far_uniform;
	int near_uniform;
	int viewvecs_uniform;
	int depth_uniform;
} GPUDOFHQPassThreeInterface;

typedef struct {
	int dof_uniform;
	int invrendertargetdim_uniform;
	int color_uniform;
	int depth_uniform;
	int viewvecs_uniform;
} GPUDOFPassOneInterface;

typedef struct {
	int dof_uniform;
	int invrendertargetdim_uniform;
	int color_uniform;
	int depth_uniform;
	int viewvecs_uniform;
} GPUDOFPassTwoInterface;

typedef struct {
	int near_coc_downsampled;
	int near_coc_blurred;
} GPUDOFPassThreeInterface;

typedef struct {
	int near_coc_downsampled;
	int invrendertargetdim_uniform;
} GPUDOFPassFourInterface;

typedef struct {
	int medium_blurred_uniform;
	int high_blurred_uniform;
	int dof_uniform;
	int invrendertargetdim_uniform;
	int original_uniform;
	int depth_uniform;
	int viewvecs_uniform;
} GPUDOFPassFiveInterface;

typedef struct {
	int depth_uniform;
} GPUDepthResolveInterface;


struct GPUFX {
	/* we borrow the term gbuffer from deferred rendering however this is just a regular
	 * depth/color framebuffer. Could be extended later though */
	GPUFrameBuffer *gbuffer;

	/* dimensions of the gbuffer */
	int gbuffer_dim[2];

	/* texture bound to the first color attachment of the gbuffer */
	GPUTexture *color_buffer;

	/* second texture used for ping-pong compositing */
	GPUTexture *color_buffer_sec;
	/* texture bound to the depth attachment of the gbuffer */
	GPUTexture *depth_buffer;
	GPUTexture *depth_buffer_xray;

	/* texture used for jittering for various effects */
	GPUTexture *jitter_buffer;

	/* all those buffers below have to coexist.
	 * Fortunately they are all quarter sized (1/16th of memory) of original framebuffer */
	int dof_downsampled_w;
	int dof_downsampled_h;

	/* texture used for near coc and color blurring calculation */
	GPUTexture *dof_near_coc_buffer;
	/* blurred near coc buffer. */
	GPUTexture *dof_near_coc_blurred_buffer;
	/* final near coc buffer. */
	GPUTexture *dof_near_coc_final_buffer;

	/* half size blur buffer */
	GPUTexture *dof_half_downsampled_near;
	GPUTexture *dof_half_downsampled_far;
	/* high quality dof texture downsamplers. 6 levels means 64 pixels wide - should be enough */
	GPUTexture *dof_nearfar_coc;
	GPUTexture *dof_near_blur;
	GPUTexture *dof_far_blur;

	/* for high quality we use again a spiral texture with radius adapted */
	bool dof_high_quality;

	/* texture used for ssao */
	int ssao_sample_count_cache;
	GPUTexture *ssao_spiral_samples_tex;


	GPUFXSettings settings;

	/* or-ed flags of enabled effects */
	int effects;

	/* number of passes, needed to detect if ping pong buffer allocation is needed */
	int num_passes;

	/* we have a stencil, restore the previous state */
	bool restore_stencil;

	unsigned int vbuffer;
};

#if 0
/* concentric mapping, see "A Low Distortion Map Between Disk and Square" and
 * http://psgraphics.blogspot.nl/2011/01/improved-code-for-concentric-map.html */
static GPUTexture * create_concentric_sample_texture(int side)
{
	GPUTexture *tex;
	float midpoint = 0.5f * (side - 1);
	float *texels = (float *)MEM_mallocN(sizeof(float) * 2 * side * side, "concentric_tex");
	int i, j;

	for (i = 0; i < side; i++) {
		for (j = 0; j < side; j++) {
			int index = (i * side + j) * 2;
			float a = 1.0f - i / midpoint;
			float b = 1.0f - j / midpoint;
			float phi, r;
			if (a * a > b * b) {
				r = a;
				phi = (M_PI_4) * (b / a);
			}
			else {
				r = b;
				phi = M_PI_2 - (M_PI_4) * (a / b);
			}
			texels[index] = r * cos(phi);
			texels[index + 1] = r * sin(phi);
		}
	}

	tex = GPU_texture_create_1D_procedural(side * side, texels, NULL);
	MEM_freeN(texels);
	return tex;
}
#endif

static GPUTexture *create_spiral_sample_texture(int numsaples)
{
	GPUTexture *tex;
	float (*texels)[2] = MEM_mallocN(sizeof(float[2]) * numsaples, "concentric_tex");
	const float numsaples_inv = 1.0f / numsaples;
	int i;
	/* arbitrary number to ensure we don't get conciding samples every circle */
	const float spirals = 7.357;

	for (i = 0; i < numsaples; i++) {
		float r = (i + 0.5f) * numsaples_inv;
		float phi = r * spirals * (float)(2.0 * M_PI);
		texels[i][0] = r * cosf(phi);
		texels[i][1] = r * sinf(phi);
	}

	tex = GPU_texture_create_1D_procedural(numsaples, (float *)texels, NULL);
	MEM_freeN(texels);
	return tex;
}

/* generate a new FX compositor */
GPUFX *GPU_fx_compositor_create(void)
{
	GPUFX *fx = MEM_callocN(sizeof(GPUFX), "GPUFX compositor");

	glGenBuffers(1, &fx->vbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, fx->vbuffer);
	glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, 8 * sizeof(float), fullscreencos);
	glBufferSubData(GL_ARRAY_BUFFER, 8 * sizeof(float), 8 * sizeof(float), fullscreenuvs);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return fx;
}

static void cleanup_fx_dof_buffers(GPUFX *fx)
{
	if (fx->dof_near_coc_blurred_buffer) {
		GPU_texture_free(fx->dof_near_coc_blurred_buffer);
		fx->dof_near_coc_blurred_buffer = NULL;
	}
	if (fx->dof_near_coc_buffer) {
		GPU_texture_free(fx->dof_near_coc_buffer);
		fx->dof_near_coc_buffer = NULL;
	}
	if (fx->dof_near_coc_final_buffer) {
		GPU_texture_free(fx->dof_near_coc_final_buffer);
		fx->dof_near_coc_final_buffer = NULL;
	}

	if (fx->dof_half_downsampled_near) {
		GPU_texture_free(fx->dof_half_downsampled_near);
		fx->dof_half_downsampled_near = NULL;
	}
	if (fx->dof_half_downsampled_far) {
		GPU_texture_free(fx->dof_half_downsampled_far);
		fx->dof_half_downsampled_far = NULL;
	}
	if (fx->dof_nearfar_coc) {
		GPU_texture_free(fx->dof_nearfar_coc);
		fx->dof_nearfar_coc = NULL;
	}
	if (fx->dof_near_blur) {
		GPU_texture_free(fx->dof_near_blur);
		fx->dof_near_blur = NULL;
	}
	if (fx->dof_far_blur) {
		GPU_texture_free(fx->dof_far_blur);
		fx->dof_far_blur = NULL;
	}
}

static void cleanup_fx_gl_data(GPUFX *fx, bool do_fbo)
{
	if (fx->color_buffer) {
		GPU_framebuffer_texture_detach(fx->color_buffer);
		GPU_texture_free(fx->color_buffer);
		fx->color_buffer = NULL;
	}

	if (fx->color_buffer_sec) {
		GPU_framebuffer_texture_detach(fx->color_buffer_sec);
		GPU_texture_free(fx->color_buffer_sec);
		fx->color_buffer_sec = NULL;
	}

	if (fx->depth_buffer) {
		GPU_framebuffer_texture_detach(fx->depth_buffer);
		GPU_texture_free(fx->depth_buffer);
		fx->depth_buffer = NULL;
	}

	if (fx->depth_buffer_xray) {
		GPU_framebuffer_texture_detach(fx->depth_buffer_xray);
		GPU_texture_free(fx->depth_buffer_xray);
		fx->depth_buffer_xray = NULL;
	}

	cleanup_fx_dof_buffers(fx);

	if (fx->ssao_spiral_samples_tex) {
		GPU_texture_free(fx->ssao_spiral_samples_tex);
		fx->ssao_spiral_samples_tex = NULL;
	}

	if (fx->jitter_buffer && do_fbo) {
		GPU_texture_free(fx->jitter_buffer);
		fx->jitter_buffer = NULL;
	}

	if (fx->gbuffer && do_fbo) {
		GPU_framebuffer_free(fx->gbuffer);
		fx->gbuffer = NULL;
	}
}

/* destroy a text compositor */
void GPU_fx_compositor_destroy(GPUFX *fx)
{
	cleanup_fx_gl_data(fx, true);
	glDeleteBuffers(1, &fx->vbuffer);
	MEM_freeN(fx);
}

static GPUTexture * create_jitter_texture(void)
{
	float jitter[64 * 64][2];
	int i;

	for (i = 0; i < 64 * 64; i++) {
		jitter[i][0] = 2.0f * BLI_frand() - 1.0f;
		jitter[i][1] = 2.0f * BLI_frand() - 1.0f;
		normalize_v2(jitter[i]);
	}

	return GPU_texture_create_2D_procedural(64, 64, &jitter[0][0], true, NULL);
}


bool GPU_fx_compositor_initialize_passes(
        GPUFX *fx, const rcti *rect, const rcti *scissor_rect,
        const GPUFXSettings *fx_settings)
{
	int w = BLI_rcti_size_x(rect), h = BLI_rcti_size_y(rect);
	char err_out[256];
	int num_passes = 0;
	char fx_flag;

	fx->effects = 0;

	if (!GLEW_EXT_framebuffer_object)
		return false;

	if (!fx_settings) {
		cleanup_fx_gl_data(fx, true);
		return false;
	}

	fx_flag = fx_settings->fx_flag;

	/* disable effects if no options passed for them */
	if (!fx_settings->dof) {
		fx_flag &= ~GPU_FX_FLAG_DOF;
	}
	if (!fx_settings->ssao || fx_settings->ssao->samples < 1) {
		fx_flag &= ~GPU_FX_FLAG_SSAO;
	}

	if (!fx_flag) {
		cleanup_fx_gl_data(fx, true);
		return false;
	}

	/* scissor is missing when drawing offscreen, in that case, dimensions match exactly. In opposite case
	 * add one to match viewport dimensions */
	if (scissor_rect) {
		w++;
		h++;
	}

	fx->num_passes = 0;
	/* dof really needs a ping-pong buffer to work */
	if (fx_flag & GPU_FX_FLAG_DOF)
		num_passes++;

	if (fx_flag & GPU_FX_FLAG_SSAO)
		num_passes++;

	if (!fx->gbuffer) {
		fx->gbuffer = GPU_framebuffer_create();

		if (!fx->gbuffer) {
			return false;
		}
	}

	/* try creating the jitter texture */
	if (!fx->jitter_buffer)
		fx->jitter_buffer = create_jitter_texture();

	/* check if color buffers need recreation */
	if (!fx->color_buffer || !fx->depth_buffer || w != fx->gbuffer_dim[0] || h != fx->gbuffer_dim[1]) {
		cleanup_fx_gl_data(fx, false);

		if (!(fx->color_buffer = GPU_texture_create_2D(w, h, NULL, GPU_HDR_NONE, err_out))) {
			printf(".256%s\n", err_out);
			cleanup_fx_gl_data(fx, true);
			return false;
		}

		if (!(fx->depth_buffer = GPU_texture_create_depth(w, h, err_out))) {
			printf("%.256s\n", err_out);
			cleanup_fx_gl_data(fx, true);
			return false;
		}
	}

	if (fx_flag & GPU_FX_FLAG_SSAO) {
		if (fx_settings->ssao->samples != fx->ssao_sample_count_cache || !fx->ssao_spiral_samples_tex) {
			if (fx_settings->ssao->samples < 1)
				fx_settings->ssao->samples = 1;

			fx->ssao_sample_count_cache = fx_settings->ssao->samples;

			if (fx->ssao_spiral_samples_tex) {
				GPU_texture_free(fx->ssao_spiral_samples_tex);
			}

			fx->ssao_spiral_samples_tex = create_spiral_sample_texture(fx_settings->ssao->samples);
		}
	}
	else {
		if (fx->ssao_spiral_samples_tex) {
			GPU_texture_free(fx->ssao_spiral_samples_tex);
			fx->ssao_spiral_samples_tex = NULL;
		}
	}

	/* create textures for dof effect */
	if (fx_flag & GPU_FX_FLAG_DOF) {
		bool dof_high_quality = (fx_settings->dof->high_quality != 0) &&
		                        GPU_geometry_shader_support() && GPU_instanced_drawing_support();

		/* cleanup buffers if quality setting has changed (no need to keep more buffers around than necessary ) */
		if (dof_high_quality != fx->dof_high_quality)
			cleanup_fx_dof_buffers(fx);

		if (dof_high_quality) {
			fx->dof_downsampled_w = w / 2;
			fx->dof_downsampled_h = h / 2;

			if (!fx->dof_half_downsampled_near || !fx->dof_nearfar_coc || !fx->dof_near_blur ||
			    !fx->dof_far_blur || !fx->dof_half_downsampled_far)
			{

				if (!(fx->dof_half_downsampled_near = GPU_texture_create_2D(
				      fx->dof_downsampled_w, fx->dof_downsampled_h, NULL, GPU_HDR_NONE, err_out)))
				{
					printf("%.256s\n", err_out);
					cleanup_fx_gl_data(fx, true);
					return false;
				}
				if (!(fx->dof_half_downsampled_far = GPU_texture_create_2D(
				      fx->dof_downsampled_w, fx->dof_downsampled_h, NULL, GPU_HDR_NONE, err_out)))
				{
					printf("%.256s\n", err_out);
					cleanup_fx_gl_data(fx, true);
					return false;
				}
				if (!(fx->dof_nearfar_coc = GPU_texture_create_2D_procedural(
				      fx->dof_downsampled_w, fx->dof_downsampled_h, NULL, false, err_out)))
				{
					printf("%.256s\n", err_out);
					cleanup_fx_gl_data(fx, true);
					return false;
				}


				if (!(fx->dof_near_blur = GPU_texture_create_2D(
				    fx->dof_downsampled_w, fx->dof_downsampled_h, NULL, GPU_HDR_HALF_FLOAT, err_out)))
				{
					printf("%.256s\n", err_out);
					cleanup_fx_gl_data(fx, true);
					return false;
				}

				if (!(fx->dof_far_blur = GPU_texture_create_2D(
				    fx->dof_downsampled_w, fx->dof_downsampled_h, NULL, GPU_HDR_HALF_FLOAT, err_out)))
				{
					printf("%.256s\n", err_out);
					cleanup_fx_gl_data(fx, true);
					return false;
				}
			}
		}
		else {
			fx->dof_downsampled_w = w / 4;
			fx->dof_downsampled_h = h / 4;

			if (!fx->dof_near_coc_buffer || !fx->dof_near_coc_blurred_buffer || !fx->dof_near_coc_final_buffer) {

				if (!(fx->dof_near_coc_buffer = GPU_texture_create_2D(
				          fx->dof_downsampled_w, fx->dof_downsampled_h, NULL, GPU_HDR_NONE, err_out)))
				{
					printf("%.256s\n", err_out);
					cleanup_fx_gl_data(fx, true);
					return false;
				}
				if (!(fx->dof_near_coc_blurred_buffer = GPU_texture_create_2D(
				          fx->dof_downsampled_w, fx->dof_downsampled_h, NULL, GPU_HDR_NONE, err_out)))
				{
					printf("%.256s\n", err_out);
					cleanup_fx_gl_data(fx, true);
					return false;
				}
				if (!(fx->dof_near_coc_final_buffer = GPU_texture_create_2D(
				          fx->dof_downsampled_w, fx->dof_downsampled_h, NULL, GPU_HDR_NONE, err_out)))
				{
					printf("%.256s\n", err_out);
					cleanup_fx_gl_data(fx, true);
					return false;
				}
			}
		}

		fx->dof_high_quality = dof_high_quality;
	}
	else {
		/* cleanup unnecessary buffers */
		cleanup_fx_dof_buffers(fx);
	}

	/* we need to pass data between shader stages, allocate an extra color buffer */
	if (num_passes > 1) {
		if (!fx->color_buffer_sec) {
			if (!(fx->color_buffer_sec = GPU_texture_create_2D(w, h, NULL, GPU_HDR_NONE, err_out))) {
				printf(".256%s\n", err_out);
				cleanup_fx_gl_data(fx, true);
				return false;
			}
		}
	}
	else {
		if (fx->color_buffer_sec) {
			GPU_framebuffer_texture_detach(fx->color_buffer_sec);
			GPU_texture_free(fx->color_buffer_sec);
			fx->color_buffer_sec = NULL;
		}
	}

	/* bind the buffers */

	/* first depth buffer, because system assumes read/write buffers */
	if (!GPU_framebuffer_texture_attach(fx->gbuffer, fx->depth_buffer, 0, err_out))
		printf("%.256s\n", err_out);

	if (!GPU_framebuffer_texture_attach(fx->gbuffer, fx->color_buffer, 0, err_out))
		printf("%.256s\n", err_out);

	if (!GPU_framebuffer_check_valid(fx->gbuffer, err_out))
		printf("%.256s\n", err_out);

	GPU_texture_bind_as_framebuffer(fx->color_buffer);

	/* enable scissor test. It's needed to ensure sculpting works correctly */
	if (scissor_rect) {
		int w_sc = BLI_rcti_size_x(scissor_rect) + 1;
		int h_sc = BLI_rcti_size_y(scissor_rect) + 1;
		glPushAttrib(GL_SCISSOR_BIT);
		glEnable(GL_SCISSOR_TEST);
		glScissor(scissor_rect->xmin - rect->xmin, scissor_rect->ymin - rect->ymin,
		          w_sc, h_sc);
		fx->restore_stencil = true;
	}
	else {
		fx->restore_stencil = false;
	}

	fx->effects = fx_flag;

	if (fx_settings)
		fx->settings = *fx_settings;
	fx->gbuffer_dim[0] = w;
	fx->gbuffer_dim[1] = h;

	fx->num_passes = num_passes;

	return true;
}

static void gpu_fx_bind_render_target(int *passes_left, GPUFX *fx, struct GPUOffScreen *ofs, GPUTexture *target)
{
	if ((*passes_left)-- == 1) {
		GPU_framebuffer_texture_unbind(fx->gbuffer, NULL);
		if (ofs) {
			GPU_offscreen_bind(ofs, false);
		}
		else
			GPU_framebuffer_restore();
	}
	else {
		/* bind the ping buffer to the color buffer */
		GPU_framebuffer_texture_attach(fx->gbuffer, target, 0, NULL);
	}
}

void GPU_fx_compositor_setup_XRay_pass(GPUFX *fx, bool do_xray)
{
	char err_out[256];

	if (do_xray) {
		if (!fx->depth_buffer_xray &&
		    !(fx->depth_buffer_xray = GPU_texture_create_depth(fx->gbuffer_dim[0], fx->gbuffer_dim[1], err_out)))
		{
			printf("%.256s\n", err_out);
			cleanup_fx_gl_data(fx, true);
			return;
		}
	}
	else {
		if (fx->depth_buffer_xray) {
			GPU_framebuffer_texture_detach(fx->depth_buffer_xray);
			GPU_texture_free(fx->depth_buffer_xray);
			fx->depth_buffer_xray = NULL;
		}
		return;
	}

	GPU_framebuffer_texture_detach(fx->depth_buffer);

	/* first depth buffer, because system assumes read/write buffers */
	if (!GPU_framebuffer_texture_attach(fx->gbuffer, fx->depth_buffer_xray, 0, err_out))
		printf("%.256s\n", err_out);
}


void GPU_fx_compositor_XRay_resolve(GPUFX *fx)
{
	GPUShader *depth_resolve_shader;
	GPU_framebuffer_texture_detach(fx->depth_buffer_xray);

	/* attach regular framebuffer */
	GPU_framebuffer_texture_attach(fx->gbuffer, fx->depth_buffer, 0, NULL);

	/* full screen quad where we will always write to depth buffer */
	glPushAttrib(GL_DEPTH_BUFFER_BIT | GL_SCISSOR_BIT);
	glDepthFunc(GL_ALWAYS);
	/* disable scissor from sculpt if any */
	glDisable(GL_SCISSOR_TEST);
	/* disable writing to color buffer, it's depth only pass */
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	/* set up quad buffer */
	glBindBuffer(GL_ARRAY_BUFFER, fx->vbuffer);
	glVertexPointer(2, GL_FLOAT, 0, NULL);
	glTexCoordPointer(2, GL_FLOAT, 0, ((GLubyte *)NULL + 8 * sizeof(float)));
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	depth_resolve_shader = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_RESOLVE, false);

	if (depth_resolve_shader) {
		GPUDepthResolveInterface *interface = GPU_shader_get_interface(depth_resolve_shader);

		GPU_shader_bind(depth_resolve_shader);

		GPU_texture_bind(fx->depth_buffer_xray, 0);
		GPU_texture_filter_mode(fx->depth_buffer_xray, false, true);
		GPU_shader_uniform_texture(depth_resolve_shader, interface->depth_uniform, fx->depth_buffer_xray);

		/* draw */
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		/* disable bindings */
		GPU_texture_filter_mode(fx->depth_buffer_xray, true, false);
		GPU_texture_unbind(fx->depth_buffer_xray);

		GPU_shader_unbind();
	}

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glPopAttrib();
}


bool GPU_fx_do_composite_pass(
        GPUFX *fx, float projmat[4][4], bool is_persp,
        struct Scene *scene, struct GPUOffScreen *ofs)
{
	GPUTexture *src, *target;
	int numslots = 0;
	float invproj[4][4];
	int i;
	float dfdyfac[2];
	/* number of passes left. when there are no more passes, the result is passed to the frambuffer */
	int passes_left = fx->num_passes;
	/* view vectors for the corners of the view frustum. Can be used to recreate the world space position easily */
	float viewvecs[3][4] = {
	    {-1.0f, -1.0f, -1.0f, 1.0f},
	    {1.0f, -1.0f, -1.0f, 1.0f},
	    {-1.0f, 1.0f, -1.0f, 1.0f}
	};

	if (fx->effects == 0)
		return false;

	GPU_get_dfdy_factors(dfdyfac);
	/* first, unbind the render-to-texture framebuffer */
	GPU_framebuffer_texture_detach(fx->color_buffer);
	GPU_framebuffer_texture_detach(fx->depth_buffer);

	if (fx->restore_stencil)
		glPopAttrib();

	src = fx->color_buffer;
	target = fx->color_buffer_sec;

	/* set up quad buffer */
	glBindBuffer(GL_ARRAY_BUFFER, fx->vbuffer);
	glVertexPointer(2, GL_FLOAT, 0, NULL);
	glTexCoordPointer(2, GL_FLOAT, 0, ((GLubyte *)NULL + 8 * sizeof(float)));
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	/* full screen FX pass */

	/* invert the view matrix */
	invert_m4_m4(invproj, projmat);

	/* convert the view vectors to view space */
	for (i = 0; i < 3; i++) {
		mul_m4_v4(invproj, viewvecs[i]);
		/* normalized trick see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
		mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
		if (is_persp)
			mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
		viewvecs[i][3] = 1.0;
	}

	/* we need to store the differences */
	viewvecs[1][0] -= viewvecs[0][0];
	viewvecs[1][1] = viewvecs[2][1] - viewvecs[0][1];

	/* calculate a depth offset as well */
	if (!is_persp) {
		float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
		mul_m4_v4(invproj, vec_far);
		mul_v3_fl(vec_far, 1.0f / vec_far[3]);
		viewvecs[1][2] = vec_far[2] - viewvecs[0][2];
	}

	/* set invalid color in case shader fails */
	glColor3f(1.0, 0.0, 1.0);
	glDisable(GL_DEPTH_TEST);

	/* ssao pass */
	if (fx->effects & GPU_FX_FLAG_SSAO) {
		GPUShader *ssao_shader;
		ssao_shader = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_SSAO, is_persp);
		if (ssao_shader) {
			const GPUSSAOSettings *fx_ssao = fx->settings.ssao;
			/* adjust attenuation to be scale invariant */
			float attenuation = fx_ssao->attenuation / (fx_ssao->distance_max * fx_ssao->distance_max);
			float ssao_params[4] = {fx_ssao->distance_max, fx_ssao->factor, attenuation, 0.0f};
			float sample_params[3];

			sample_params[0] = fx->ssao_sample_count_cache;
			/* multiplier so we tile the random texture on screen */
			sample_params[1] = fx->gbuffer_dim[0] / 64.0;
			sample_params[2] = fx->gbuffer_dim[1] / 64.0;

			ssao_params[3] = (passes_left == 1 && !ofs) ? dfdyfac[0] : dfdyfac[1];

			GPUSSAOShaderInterface *interface = GPU_shader_get_interface(ssao_shader);

			GPU_shader_bind(ssao_shader);

			GPU_shader_uniform_vector(ssao_shader, interface->ssao_uniform, 4, 1, ssao_params);
			GPU_shader_uniform_vector(ssao_shader, interface->ssao_color_uniform, 4, 1, fx_ssao->color);
			GPU_shader_uniform_vector(ssao_shader, interface->viewvecs_uniform, 4, 3, viewvecs[0]);
			GPU_shader_uniform_vector(ssao_shader, interface->ssao_sample_params_uniform, 3, 1, sample_params);

			GPU_texture_bind(src, numslots++);
			GPU_shader_uniform_texture(ssao_shader, interface->color_uniform, src);

			GPU_texture_bind(fx->depth_buffer, numslots++);
			GPU_texture_filter_mode(fx->depth_buffer, false, true);
			GPU_shader_uniform_texture(ssao_shader, interface->depth_uniform, fx->depth_buffer);

			GPU_texture_bind(fx->jitter_buffer, numslots++);
			GPU_shader_uniform_texture(ssao_shader, interface->ssao_jitter_uniform, fx->jitter_buffer);

			GPU_texture_bind(fx->ssao_spiral_samples_tex, numslots++);
			GPU_shader_uniform_texture(ssao_shader, interface->ssao_concentric_tex, fx->ssao_spiral_samples_tex);

			/* draw */
			gpu_fx_bind_render_target(&passes_left, fx, ofs, target);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			/* disable bindings */
			GPU_texture_unbind(src);
			GPU_texture_filter_mode(fx->depth_buffer, true, false);
			GPU_texture_unbind(fx->depth_buffer);
			GPU_texture_unbind(fx->jitter_buffer);
			GPU_texture_unbind(fx->ssao_spiral_samples_tex);

			/* may not be attached, in that case this just returns */
			if (target) {
				GPU_framebuffer_texture_detach(target);
				if (ofs) {
					GPU_offscreen_bind(ofs, false);
				}
				else {
					GPU_framebuffer_restore();
				}
			}

			/* swap here, after src/target have been unbound */
			SWAP(GPUTexture *, target, src);
			numslots = 0;
		}
	}

	/* second pass, dof */
	if (fx->effects & GPU_FX_FLAG_DOF) {
		const GPUDOFSettings *fx_dof = fx->settings.dof;
		float dof_params[4];
		float scale = scene->unit.system ? scene->unit.scale_length : 1.0f;
		/* this is factor that converts to the scene scale. focal length and sensor are expressed in mm
		 * unit.scale_length is how many meters per blender unit we have. We want to convert to blender units though
		 * because the shader reads coordinates in world space, which is in blender units.
		 * Note however that focus_distance is already in blender units and shall not be scaled here (see T48157). */
		float scale_camera = 0.001f / scale;
		/* we want radius here for the aperture number  */
		float aperture = 0.5f * scale_camera * fx_dof->focal_length / fx_dof->fstop;

		dof_params[0] = aperture * fabsf(scale_camera * fx_dof->focal_length /
		                                 (fx_dof->focus_distance - scale_camera * fx_dof->focal_length));
		dof_params[1] = fx_dof->focus_distance;
		dof_params[2] = fx->gbuffer_dim[0] / (scale_camera * fx_dof->sensor);
		dof_params[3] = fx_dof->num_blades;

		if (fx->dof_high_quality) {
			GPUShader *dof_shader_pass1, *dof_shader_pass2, *dof_shader_pass3;

			/* custom shaders close to the effect described in CryEngine 3 Graphics Gems */
			dof_shader_pass1 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_ONE, is_persp);
			dof_shader_pass2 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_TWO, is_persp);
			dof_shader_pass3 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_THREE, is_persp);

			/* error occured, restore framebuffers and return */
			if (!(dof_shader_pass1 && dof_shader_pass2 && dof_shader_pass3)) {
				GPU_framebuffer_texture_unbind(fx->gbuffer, NULL);
				GPU_framebuffer_restore();
				glDisableClientState(GL_VERTEX_ARRAY);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);

				GPU_shader_unbind();
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				return false;
			}

			/* pass first, downsample the color buffer to near/far targets and calculate coc texture */
			{
				float invrendertargetdim[2] = {1.0f / fx->dof_downsampled_w, 1.0f / fx->dof_downsampled_h};

				GPUDOFHQPassOneInterface *interface = GPU_shader_get_interface(dof_shader_pass1);

				GPU_shader_bind(dof_shader_pass1);

				GPU_shader_uniform_vector(dof_shader_pass1, interface->dof_uniform, 4, 1, dof_params);
				GPU_shader_uniform_vector(dof_shader_pass1, interface->invrendertargetdim_uniform, 2, 1, invrendertargetdim);
				GPU_shader_uniform_vector(dof_shader_pass1, interface->viewvecs_uniform, 4, 3, viewvecs[0]);

				GPU_shader_uniform_vector(dof_shader_pass1, interface->invrendertargetdim_uniform, 2, 1, invrendertargetdim);

				GPU_texture_bind(fx->depth_buffer, numslots++);
				GPU_texture_filter_mode(fx->depth_buffer, false, false);
				GPU_shader_uniform_texture(dof_shader_pass1, interface->depth_uniform, fx->depth_buffer);

				GPU_texture_bind(src, numslots++);
				/* disable filtering for the texture so custom downsample can do the right thing */
				GPU_texture_filter_mode(src, false, false);
				GPU_shader_uniform_texture(dof_shader_pass2, interface->color_uniform, src);

				/* target is the downsampled coc buffer */
				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_half_downsampled_near, 0, NULL);
				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_half_downsampled_far, 1, NULL);
				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_nearfar_coc, 2, NULL);
				/* binding takes care of setting the viewport to the downsampled size */
				GPU_framebuffer_slots_bind(fx->gbuffer, 0);

				GPU_framebuffer_check_valid(fx->gbuffer, NULL);

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				/* disable bindings */
				GPU_texture_filter_mode(src, false, true);
				GPU_texture_unbind(src);
				GPU_texture_filter_mode(fx->depth_buffer, true, false);
				GPU_texture_unbind(fx->depth_buffer);

				GPU_framebuffer_texture_detach(fx->dof_half_downsampled_near);
				GPU_framebuffer_texture_detach(fx->dof_half_downsampled_far);
				GPU_framebuffer_texture_detach(fx->dof_nearfar_coc);
				GPU_framebuffer_texture_unbind(fx->gbuffer, fx->dof_half_downsampled_near);

				numslots = 0;
			}

			/* second pass, shoot quads for every pixel in the downsampled buffers, scaling according
			 * to circle of confusion */
			{
				int rendertargetdim[2] = {fx->dof_downsampled_w, fx->dof_downsampled_h};
				float selection[2] = {0.0f, 1.0f};

				GPUDOFHQPassTwoInterface *interface = GPU_shader_get_interface(dof_shader_pass2);

				GPU_shader_bind(dof_shader_pass2);

				GPU_shader_uniform_vector(dof_shader_pass2, interface->dof_uniform, 4, 1, dof_params);
				GPU_shader_uniform_vector_int(dof_shader_pass2, interface->rendertargetdim_uniform, 2, 1, rendertargetdim);
				GPU_shader_uniform_vector(dof_shader_pass2, interface->select_uniform, 2, 1, selection);

				GPU_texture_bind(fx->dof_nearfar_coc, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass2, interface->coc_uniform, fx->dof_nearfar_coc);

				GPU_texture_bind(fx->dof_half_downsampled_far, numslots++);
				GPU_texture_bind(fx->dof_half_downsampled_near, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass2, interface->color_uniform, fx->dof_half_downsampled_far);
				GPU_texture_filter_mode(fx->dof_half_downsampled_far, false, false);

				/* target is the downsampled coc buffer */
				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_far_blur, 0, NULL);
				GPU_texture_bind_as_framebuffer(fx->dof_far_blur);

				glDisable(GL_DEPTH_TEST);
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glPointSize(1.0f);
				/* have to clear the buffer unfortunately */
				glClearColor(0.0, 0.0, 0.0, 0.0);
				glClear(GL_COLOR_BUFFER_BIT);
				/* the draw call we all waited for, draw a point per pixel, scaled per circle of confusion */
				glDrawArraysInstancedARB(GL_POINTS, 0, 1, fx->dof_downsampled_w * fx->dof_downsampled_h);

				GPU_texture_unbind(fx->dof_half_downsampled_far);
				GPU_framebuffer_texture_detach(fx->dof_far_blur);

				GPU_shader_uniform_texture(dof_shader_pass2, interface->color_uniform, fx->dof_half_downsampled_near);
				GPU_texture_filter_mode(fx->dof_half_downsampled_near, false, false);

				selection[0] = 1.0f;
				selection[1] = 0.0f;

				GPU_shader_uniform_vector(dof_shader_pass2, interface->select_uniform, 2, 1, selection);

				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_blur, 0, NULL);
				/* have to clear the buffer unfortunately */
				glClear(GL_COLOR_BUFFER_BIT);
				/* the draw call we all waited for, draw a point per pixel, scaled per circle of confusion */
				glDrawArraysInstancedARB(GL_POINTS, 0, 1, fx->dof_downsampled_w * fx->dof_downsampled_h);

				/* disable bindings */
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDisable(GL_BLEND);

				GPU_framebuffer_texture_detach(fx->dof_near_blur);

				GPU_texture_unbind(fx->dof_half_downsampled_near);
				GPU_texture_unbind(fx->dof_nearfar_coc);

				GPU_framebuffer_texture_unbind(fx->gbuffer, fx->dof_far_blur);
			}

			/* third pass, accumulate the near/far blur fields */
			{
				float invrendertargetdim[2] = {1.0f / fx->dof_downsampled_w, 1.0f / fx->dof_downsampled_h};

				GPUDOFHQPassThreeInterface *interface = GPU_shader_get_interface(dof_shader_pass3);

				GPU_shader_bind(dof_shader_pass3);

				GPU_shader_uniform_vector(dof_shader_pass3, interface->dof_uniform, 4, 1, dof_params);

				GPU_shader_uniform_vector(dof_shader_pass3, interface->invrendertargetdim_uniform, 2, 1, invrendertargetdim);
				GPU_shader_uniform_vector(dof_shader_pass3, interface->viewvecs_uniform, 4, 3, viewvecs[0]);

				GPU_texture_bind(fx->dof_near_blur, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass3, interface->near_uniform, fx->dof_near_blur);
				GPU_texture_filter_mode(fx->dof_near_blur, false, true);

				GPU_texture_bind(fx->dof_far_blur, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass3, interface->far_uniform, fx->dof_far_blur);
				GPU_texture_filter_mode(fx->dof_far_blur, false, true);

				GPU_texture_bind(fx->depth_buffer, numslots++);
				GPU_texture_filter_mode(fx->depth_buffer, false, false);
				GPU_shader_uniform_texture(dof_shader_pass3, interface->depth_uniform, fx->depth_buffer);

				GPU_texture_bind(src, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass3, interface->color_uniform, src);

				/* if this is the last pass, prepare for rendering on the frambuffer */
				gpu_fx_bind_render_target(&passes_left, fx, ofs, target);

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

				/* disable bindings */
				GPU_texture_unbind(fx->dof_near_blur);
				GPU_texture_unbind(fx->dof_far_blur);
				GPU_texture_unbind(src);
				GPU_texture_filter_mode(fx->depth_buffer, true, false);
				GPU_texture_unbind(fx->depth_buffer);

				/* may not be attached, in that case this just returns */
				if (target) {
					GPU_framebuffer_texture_detach(target);
					if (ofs) {
						GPU_offscreen_bind(ofs, false);
					}
					else {
						GPU_framebuffer_restore();
					}
				}

				numslots = 0;
			}
		}
		else {
			GPUShader *dof_shader_pass1, *dof_shader_pass2, *dof_shader_pass3, *dof_shader_pass4, *dof_shader_pass5;

			/* DOF effect has many passes but most of them are performed
			 * on a texture whose dimensions are 4 times less than the original
			 * (16 times lower than original screen resolution).
			 * Technique used is not very exact but should be fast enough and is based
			 * on "Practical Post-Process Depth of Field"
			 * see http://http.developer.nvidia.com/GPUGems3/gpugems3_ch28.html */
			dof_shader_pass1 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_ONE, is_persp);
			dof_shader_pass2 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_TWO, is_persp);
			dof_shader_pass3 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_THREE, is_persp);
			dof_shader_pass4 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FOUR, is_persp);
			dof_shader_pass5 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FIVE, is_persp);

			/* error occured, restore framebuffers and return */
			if (!(dof_shader_pass1 && dof_shader_pass2 && dof_shader_pass3 && dof_shader_pass4 && dof_shader_pass5)) {
				GPU_framebuffer_texture_unbind(fx->gbuffer, NULL);
				GPU_framebuffer_restore();
				glDisableClientState(GL_VERTEX_ARRAY);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);

				GPU_shader_unbind();
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				return false;
			}

			/* pass first, first level of blur in low res buffer */
			{
				float invrendertargetdim[2] = {1.0f / fx->gbuffer_dim[0], 1.0f / fx->gbuffer_dim[1]};

				GPUDOFPassOneInterface *interface = GPU_shader_get_interface(dof_shader_pass1);

				GPU_shader_bind(dof_shader_pass1);

				GPU_shader_uniform_vector(dof_shader_pass1, interface->dof_uniform, 4, 1, dof_params);
				GPU_shader_uniform_vector(dof_shader_pass1, interface->invrendertargetdim_uniform, 2, 1, invrendertargetdim);
				GPU_shader_uniform_vector(dof_shader_pass1, interface->viewvecs_uniform, 4, 3, viewvecs[0]);

				GPU_texture_bind(src, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass1, interface->color_uniform, src);

				GPU_texture_bind(fx->depth_buffer, numslots++);
				GPU_texture_filter_mode(fx->depth_buffer, false, true);
				GPU_shader_uniform_texture(dof_shader_pass1, interface->depth_uniform, fx->depth_buffer);

				/* target is the downsampled coc buffer */
				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_buffer, 0, NULL);
				/* binding takes care of setting the viewport to the downsampled size */
				GPU_texture_bind_as_framebuffer(fx->dof_near_coc_buffer);

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				/* disable bindings */
				GPU_texture_unbind(src);
				GPU_texture_filter_mode(fx->depth_buffer, true, false);
				GPU_texture_unbind(fx->depth_buffer);

				GPU_framebuffer_texture_detach(fx->dof_near_coc_buffer);
				numslots = 0;
			}

			/* second pass, gaussian blur the downsampled image */
			{
				float invrendertargetdim[2] = {1.0f / GPU_texture_width(fx->dof_near_coc_blurred_buffer),
				                               1.0f / GPU_texture_height(fx->dof_near_coc_blurred_buffer)};
				float tmp = invrendertargetdim[0];
				invrendertargetdim[0] = 0.0f;

				GPUDOFPassTwoInterface *interface = GPU_shader_get_interface(dof_shader_pass2);

				dof_params[2] = GPU_texture_width(fx->dof_near_coc_blurred_buffer) / (scale_camera * fx_dof->sensor);

				/* Blurring vertically */
				GPU_shader_bind(dof_shader_pass2);

				GPU_shader_uniform_vector(dof_shader_pass2, interface->dof_uniform, 4, 1, dof_params);
				GPU_shader_uniform_vector(dof_shader_pass2, interface->invrendertargetdim_uniform, 2, 1, invrendertargetdim);
				GPU_shader_uniform_vector(dof_shader_pass2, interface->viewvecs_uniform, 4, 3, viewvecs[0]);

				GPU_texture_bind(fx->depth_buffer, numslots++);
				GPU_texture_filter_mode(fx->depth_buffer, false, true);
				GPU_shader_uniform_texture(dof_shader_pass2, interface->depth_uniform, fx->depth_buffer);

				GPU_texture_bind(fx->dof_near_coc_buffer, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass2, interface->color_uniform, fx->dof_near_coc_buffer);

				/* use final buffer as a temp here */
				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_final_buffer, 0, NULL);

				/* Drawing quad */
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

				/* *unbind/detach */
				GPU_texture_unbind(fx->dof_near_coc_buffer);
				GPU_framebuffer_texture_detach(fx->dof_near_coc_final_buffer);

				/* Blurring horizontally */
				invrendertargetdim[0] = tmp;
				invrendertargetdim[1] = 0.0f;
				GPU_shader_uniform_vector(dof_shader_pass2, interface->invrendertargetdim_uniform, 2, 1, invrendertargetdim);

				GPU_texture_bind(fx->dof_near_coc_final_buffer, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass2, interface->color_uniform, fx->dof_near_coc_final_buffer);

				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_blurred_buffer, 0, NULL);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

				/* *unbind/detach */
				GPU_texture_filter_mode(fx->depth_buffer, true, false);
				GPU_texture_unbind(fx->depth_buffer);

				GPU_texture_unbind(fx->dof_near_coc_final_buffer);
				GPU_framebuffer_texture_detach(fx->dof_near_coc_blurred_buffer);

				dof_params[2] = fx->gbuffer_dim[0] / (scale_camera * fx_dof->sensor);

				numslots = 0;
			}

			/* third pass, calculate near coc */
			{
				GPUDOFPassThreeInterface *interface = GPU_shader_get_interface(dof_shader_pass3);

				GPU_shader_bind(dof_shader_pass3);

				GPU_texture_bind(fx->dof_near_coc_buffer, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass3, interface->near_coc_downsampled, fx->dof_near_coc_buffer);

				GPU_texture_bind(fx->dof_near_coc_blurred_buffer, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass3, interface->near_coc_blurred, fx->dof_near_coc_blurred_buffer);

				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_final_buffer, 0, NULL);

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				/* disable bindings */
				GPU_texture_unbind(fx->dof_near_coc_buffer);
				GPU_texture_unbind(fx->dof_near_coc_blurred_buffer);

				/* unbinding here restores the size to the original */
				GPU_framebuffer_texture_detach(fx->dof_near_coc_final_buffer);

				numslots = 0;
			}

			/* fourth pass blur final coc once to eliminate discontinuities */
			{
				float invrendertargetdim[2] = {1.0f / GPU_texture_width(fx->dof_near_coc_blurred_buffer),
				                               1.0f / GPU_texture_height(fx->dof_near_coc_blurred_buffer)};

				GPUDOFPassFourInterface *interface = GPU_shader_get_interface(dof_shader_pass4);

				GPU_shader_bind(dof_shader_pass4);

				GPU_texture_bind(fx->dof_near_coc_final_buffer, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass4, interface->near_coc_downsampled, fx->dof_near_coc_final_buffer);
				GPU_shader_uniform_vector(dof_shader_pass4, interface->invrendertargetdim_uniform, 2, 1, invrendertargetdim);

				GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_buffer, 0, NULL);

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				/* disable bindings */
				GPU_texture_unbind(fx->dof_near_coc_final_buffer);

				/* unbinding here restores the size to the original */
				GPU_framebuffer_texture_unbind(fx->gbuffer, fx->dof_near_coc_buffer);
				GPU_framebuffer_texture_detach(fx->dof_near_coc_buffer);

				numslots = 0;
			}

			/* final pass, merge blurred layers according to final calculated coc */
			{
				float invrendertargetdim[2] = {1.0f / fx->gbuffer_dim[0], 1.0f / fx->gbuffer_dim[1]};

				GPUDOFPassFiveInterface *interface = GPU_shader_get_interface(dof_shader_pass5);

				GPU_shader_bind(dof_shader_pass5);

				GPU_shader_uniform_vector(dof_shader_pass5, interface->dof_uniform, 4, 1, dof_params);
				GPU_shader_uniform_vector(dof_shader_pass5, interface->invrendertargetdim_uniform, 2, 1, invrendertargetdim);
				GPU_shader_uniform_vector(dof_shader_pass5, interface->viewvecs_uniform, 4, 3, viewvecs[0]);

				GPU_texture_bind(src, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass5, interface->original_uniform, src);

				GPU_texture_bind(fx->dof_near_coc_blurred_buffer, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass5, interface->high_blurred_uniform, fx->dof_near_coc_blurred_buffer);

				GPU_texture_bind(fx->dof_near_coc_buffer, numslots++);
				GPU_shader_uniform_texture(dof_shader_pass5, interface->medium_blurred_uniform, fx->dof_near_coc_buffer);

				GPU_texture_bind(fx->depth_buffer, numslots++);
				GPU_texture_filter_mode(fx->depth_buffer, false, true);
				GPU_shader_uniform_texture(dof_shader_pass5, interface->depth_uniform, fx->depth_buffer);

				/* if this is the last pass, prepare for rendering on the frambuffer */
				gpu_fx_bind_render_target(&passes_left, fx, ofs, target);

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				/* disable bindings */
				GPU_texture_unbind(fx->dof_near_coc_buffer);
				GPU_texture_unbind(fx->dof_near_coc_blurred_buffer);
				GPU_texture_unbind(src);
				GPU_texture_filter_mode(fx->depth_buffer, true, false);
				GPU_texture_unbind(fx->depth_buffer);

				/* may not be attached, in that case this just returns */
				if (target) {
					GPU_framebuffer_texture_detach(target);
					if (ofs) {
						GPU_offscreen_bind(ofs, false);
					}
					else {
						GPU_framebuffer_restore();
					}
				}

				SWAP(GPUTexture *, target, src);
				numslots = 0;
			}
		}
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GPU_shader_unbind();

	return true;
}

void GPU_fx_compositor_init_dof_settings(GPUDOFSettings *fx_dof)
{
	fx_dof->fstop = 128.0f;
	fx_dof->focal_length = 1.0f;
	fx_dof->focus_distance = 1.0f;
	fx_dof->sensor = 1.0f;
	fx_dof->num_blades = 6;
}

void GPU_fx_compositor_init_ssao_settings(GPUSSAOSettings *fx_ssao)
{
	fx_ssao->factor = 1.0f;
	fx_ssao->distance_max = 0.2f;
	fx_ssao->attenuation = 1.0f;
	fx_ssao->samples = 20;
}

void GPU_fx_shader_init_interface(struct GPUShader *shader, GPUFXShaderEffect effect)
{
	if (!shader)
		return;

	switch (effect) {
		case GPU_SHADER_FX_SSAO:
		{
			GPUSSAOShaderInterface *interface = MEM_mallocN(sizeof(GPUSSAOShaderInterface), "GPUSSAOShaderInterface");

			interface->ssao_uniform = GPU_shader_get_uniform(shader, "ssao_params");
			interface->ssao_color_uniform = GPU_shader_get_uniform(shader, "ssao_color");
			interface->color_uniform = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->depth_uniform = GPU_shader_get_uniform(shader, "depthbuffer");
			interface->viewvecs_uniform = GPU_shader_get_uniform(shader, "viewvecs");
			interface->ssao_sample_params_uniform = GPU_shader_get_uniform(shader, "ssao_sample_params");
			interface->ssao_concentric_tex = GPU_shader_get_uniform(shader, "ssao_concentric_tex");
			interface->ssao_jitter_uniform = GPU_shader_get_uniform(shader, "jitter_tex");

			GPU_shader_set_interface(shader, interface);
			break;
		}

		case GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_ONE:
		{
			GPUDOFHQPassOneInterface *interface = MEM_mallocN(sizeof(GPUDOFHQPassOneInterface), "GPUDOFHQPassOneInterface");

			interface->invrendertargetdim_uniform = GPU_shader_get_uniform(shader, "invrendertargetdim");
			interface->color_uniform = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->dof_uniform = GPU_shader_get_uniform(shader, "dof_params");
			interface->depth_uniform = GPU_shader_get_uniform(shader, "depthbuffer");
			interface->viewvecs_uniform = GPU_shader_get_uniform(shader, "viewvecs");

			GPU_shader_set_interface(shader, interface);
			break;
		}

		case GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_TWO:
		{
			GPUDOFHQPassTwoInterface *interface = MEM_mallocN(sizeof(GPUDOFHQPassTwoInterface), "GPUDOFHQPassTwoInterface");

			interface->rendertargetdim_uniform = GPU_shader_get_uniform(shader, "rendertargetdim");
			interface->color_uniform = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->coc_uniform = GPU_shader_get_uniform(shader, "cocbuffer");
			interface->select_uniform = GPU_shader_get_uniform(shader, "layerselection");
			interface->dof_uniform = GPU_shader_get_uniform(shader, "dof_params");

			GPU_shader_set_interface(shader, interface);
			break;
		}

		case GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_THREE:
		{
			GPUDOFHQPassThreeInterface *interface = MEM_mallocN(sizeof(GPUDOFHQPassThreeInterface), "GPUDOFHQPassThreeInterface");

			interface->dof_uniform = GPU_shader_get_uniform(shader, "dof_params");
			interface->invrendertargetdim_uniform = GPU_shader_get_uniform(shader, "invrendertargetdim");
			interface->color_uniform = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->far_uniform = GPU_shader_get_uniform(shader, "farbuffer");
			interface->near_uniform = GPU_shader_get_uniform(shader, "nearbuffer");
			interface->viewvecs_uniform = GPU_shader_get_uniform(shader, "viewvecs");
			interface->depth_uniform = GPU_shader_get_uniform(shader, "depthbuffer");

			GPU_shader_set_interface(shader, interface);
			break;
		}

		case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_ONE:
		{
			GPUDOFPassOneInterface *interface = MEM_mallocN(sizeof(GPUDOFPassOneInterface), "GPUDOFPassOneInterface");

			interface->dof_uniform = GPU_shader_get_uniform(shader, "dof_params");
			interface->invrendertargetdim_uniform = GPU_shader_get_uniform(shader, "invrendertargetdim");
			interface->color_uniform = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->depth_uniform = GPU_shader_get_uniform(shader, "depthbuffer");
			interface->viewvecs_uniform = GPU_shader_get_uniform(shader, "viewvecs");

			GPU_shader_set_interface(shader, interface);
			break;
		}
		case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_TWO:
		{
			GPUDOFPassTwoInterface *interface = MEM_mallocN(sizeof(GPUDOFPassTwoInterface), "GPUDOFPassTwoInterface");

			interface->dof_uniform = GPU_shader_get_uniform(shader, "dof_params");
			interface->invrendertargetdim_uniform = GPU_shader_get_uniform(shader, "invrendertargetdim");
			interface->color_uniform = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->depth_uniform = GPU_shader_get_uniform(shader, "depthbuffer");
			interface->viewvecs_uniform = GPU_shader_get_uniform(shader, "viewvecs");

			GPU_shader_set_interface(shader, interface);
			break;
		}
		case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_THREE:
		{
			GPUDOFPassThreeInterface *interface = MEM_mallocN(sizeof(GPUDOFPassThreeInterface), "GPUDOFPassThreeInterface");

			interface->near_coc_downsampled = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->near_coc_blurred = GPU_shader_get_uniform(shader, "blurredcolorbuffer");

			GPU_shader_set_interface(shader, interface);
			break;
		}
		case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FOUR:
		{
			GPUDOFPassFourInterface *interface = MEM_mallocN(sizeof(GPUDOFPassFourInterface), "GPUDOFPassFourInterface");

			interface->near_coc_downsampled = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->invrendertargetdim_uniform = GPU_shader_get_uniform(shader, "invrendertargetdim");

			GPU_shader_set_interface(shader, interface);
			break;
		}
		case GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FIVE:
		{
			GPUDOFPassFiveInterface *interface = MEM_mallocN(sizeof(GPUDOFPassFiveInterface), "GPUDOFPassFiveInterface");

			interface->medium_blurred_uniform = GPU_shader_get_uniform(shader, "mblurredcolorbuffer");
			interface->high_blurred_uniform = GPU_shader_get_uniform(shader, "blurredcolorbuffer");
			interface->dof_uniform = GPU_shader_get_uniform(shader, "dof_params");
			interface->invrendertargetdim_uniform = GPU_shader_get_uniform(shader, "invrendertargetdim");
			interface->original_uniform = GPU_shader_get_uniform(shader, "colorbuffer");
			interface->depth_uniform = GPU_shader_get_uniform(shader, "depthbuffer");
			interface->viewvecs_uniform = GPU_shader_get_uniform(shader, "viewvecs");

			GPU_shader_set_interface(shader, interface);
			break;
		}

		case GPU_SHADER_FX_DEPTH_RESOLVE:
		{
			GPUDepthResolveInterface *interface = MEM_mallocN(sizeof(GPUDepthResolveInterface), "GPUDepthResolveInterface");

			interface->depth_uniform = GPU_shader_get_uniform(shader, "depthbuffer");

			GPU_shader_set_interface(shader, interface);
			break;
		}

		default:
			break;
	}
}

