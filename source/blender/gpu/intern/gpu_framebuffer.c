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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

static struct GPUFrameBufferGlobal {
	GLuint currentfb;
} GG = {0};

/* Number of maximum output slots.
 * We support 5 outputs for now (usually we wouldn't need more to preserve fill rate) */
#define GPU_FB_MAX_SLOTS 5

struct GPUFrameBuffer {
	GLuint object;
	GPUTexture *colortex[GPU_FB_MAX_SLOTS];
	GPUTexture *depthtex;
};

static void gpu_print_framebuffer_error(GLenum status, char err_out[256])
{
	const char *format = "GPUFrameBuffer: framebuffer status %s\n";
	const char *err = "unknown";

#define format_status(X) \
	case GL_FRAMEBUFFER_##X: err = "GL_FRAMEBUFFER_"#X; \
		break;

	switch (status) {
		/* success */
		format_status(COMPLETE)
		/* errors shared by OpenGL desktop & ES */
		format_status(INCOMPLETE_ATTACHMENT)
		format_status(INCOMPLETE_MISSING_ATTACHMENT)
		format_status(UNSUPPORTED)
#if 0 /* for OpenGL ES only */
		format_status(INCOMPLETE_DIMENSIONS)
#else /* for desktop GL only */
		format_status(INCOMPLETE_DRAW_BUFFER)
		format_status(INCOMPLETE_READ_BUFFER)
		format_status(INCOMPLETE_MULTISAMPLE)
		format_status(UNDEFINED)
#endif
	}

#undef format_status

	if (err_out) {
		BLI_snprintf(err_out, 256, format, err);
	}
	else {
		fprintf(stderr, format, err);
	}
}

/* GPUFrameBuffer */

GPUFrameBuffer *GPU_framebuffer_create(void)
{
	GPUFrameBuffer *fb;

	fb = MEM_callocN(sizeof(GPUFrameBuffer), "GPUFrameBuffer");
	glGenFramebuffers(1, &fb->object);

	if (!fb->object) {
		fprintf(stderr, "GPUFFrameBuffer: framebuffer gen failed.\n");
		GPU_framebuffer_free(fb);
		return NULL;
	}

	/* make sure no read buffer is enabled, so completeness check will not fail. We set those at binding time */
	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	glReadBuffer(GL_NONE);
	glDrawBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	return fb;
}

bool GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int mip)
{
	GLenum attachment;

	if (slot >= GPU_FB_MAX_SLOTS) {
		fprintf(stderr,
		        "Attaching to index %d framebuffer slot unsupported. "
		        "Use at most %d\n", slot, GPU_FB_MAX_SLOTS);
		return false;
	}

	if ((G.debug & G_DEBUG)) {
		if (GPU_texture_bound_number(tex) != -1) {
			fprintf(stderr,
			        "Feedback loop warning!: "
			        "Attempting to attach texture to framebuffer while still bound to texture unit for drawing!\n");
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	GG.currentfb = fb->object;

	if (GPU_texture_stencil(tex) && GPU_texture_depth(tex))
		attachment = GL_DEPTH_STENCIL_ATTACHMENT;
	else if (GPU_texture_depth(tex))
		attachment = GL_DEPTH_ATTACHMENT;
	else
		attachment = GL_COLOR_ATTACHMENT0 + slot;

	glFramebufferTexture(GL_FRAMEBUFFER, attachment, GPU_texture_opengl_bindcode(tex), mip);

	if (GPU_texture_depth(tex))
		fb->depthtex = tex;
	else
		fb->colortex[slot] = tex;

	GPU_texture_framebuffer_set(tex, fb, slot);

	return true;
}

static bool gpu_framebuffer_texture_layer_attach_ex(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int layer, int mip, bool cubemap)
{
	GLenum attachment;
	GLenum facetarget;

	if (slot >= GPU_FB_MAX_SLOTS) {
		fprintf(stderr,
		        "Attaching to index %d framebuffer slot unsupported. "
		        "Use at most %d\n", slot, GPU_FB_MAX_SLOTS);
		return false;
	}

	if ((G.debug & G_DEBUG)) {
		if (GPU_texture_bound_number(tex) != -1) {
			fprintf(stderr,
			        "Feedback loop warning!: "
			        "Attempting to attach texture to framebuffer while still bound to texture unit for drawing!\n");
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	GG.currentfb = fb->object;

	if (GPU_texture_stencil(tex) && GPU_texture_depth(tex))
		attachment = GL_DEPTH_STENCIL_ATTACHMENT;
	else if (GPU_texture_depth(tex))
		attachment = GL_DEPTH_ATTACHMENT;
	else
		attachment = GL_COLOR_ATTACHMENT0 + slot;

	if (cubemap) {
		facetarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer;
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, facetarget, GPU_texture_opengl_bindcode(tex), mip);
	}
	else {
		glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, GPU_texture_opengl_bindcode(tex), mip, layer);
	}

	if (GPU_texture_depth(tex))
		fb->depthtex = tex;
	else
		fb->colortex[slot] = tex;

	GPU_texture_framebuffer_set(tex, fb, slot);

	return true;
}

bool GPU_framebuffer_texture_layer_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int layer, int mip)
{
	return gpu_framebuffer_texture_layer_attach_ex(fb, tex, slot, layer, mip, false);
}

bool GPU_framebuffer_texture_cubeface_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int face, int mip)
{
	BLI_assert(GPU_texture_target(tex) == GL_TEXTURE_CUBE_MAP);
	return gpu_framebuffer_texture_layer_attach_ex(fb, tex, slot, face, mip, true);
}

void GPU_framebuffer_texture_detach(GPUTexture *tex)
{
	GLenum attachment;
	GPUFrameBuffer *fb = GPU_texture_framebuffer(tex);
	int fb_attachment = GPU_texture_framebuffer_attachment(tex);

	if (!fb)
		return;

	if (GG.currentfb != fb->object) {
		glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
		GG.currentfb = fb->object;
	}

	if (GPU_texture_stencil(tex) && GPU_texture_depth(tex)) {
		fb->depthtex = NULL;
		attachment = GL_DEPTH_STENCIL_ATTACHMENT;
	}
	else if (GPU_texture_depth(tex)) {
		fb->depthtex = NULL;
		attachment = GL_DEPTH_ATTACHMENT;
	}
	else {
		BLI_assert(fb->colortex[fb_attachment] == tex);
		fb->colortex[fb_attachment] = NULL;
		attachment = GL_COLOR_ATTACHMENT0 + fb_attachment;
	}

	glFramebufferTexture(GL_FRAMEBUFFER, attachment, 0, 0);

	GPU_texture_framebuffer_set(tex, NULL, -1);
}

void GPU_texture_bind_as_framebuffer(GPUTexture *tex)
{
	GPUFrameBuffer *fb = GPU_texture_framebuffer(tex);
	int fb_attachment = GPU_texture_framebuffer_attachment(tex);

	if (!fb) {
		fprintf(stderr, "Error, texture not bound to framebuffer!\n");
		return;
	}

	/* push attributes */
	gpuPushAttrib(GPU_ENABLE_BIT | GPU_VIEWPORT_BIT);
	glDisable(GL_SCISSOR_TEST);

	/* bind framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);

	if (GPU_texture_depth(tex)) {
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
	else {
		/* last bound prevails here, better allow explicit control here too */
		glDrawBuffer(GL_COLOR_ATTACHMENT0 + fb_attachment);
		glReadBuffer(GL_COLOR_ATTACHMENT0 + fb_attachment);
	}
	
	if (GPU_texture_target(tex) == GL_TEXTURE_2D_MULTISAMPLE) {
		glEnable(GL_MULTISAMPLE);
	}

	/* set default viewport */
	glViewport(0, 0, GPU_texture_width(tex), GPU_texture_height(tex));
	GG.currentfb = fb->object;
}

void GPU_framebuffer_slots_bind(GPUFrameBuffer *fb, int slot)
{
	int numslots = 0, i;
	GLenum attachments[GPU_FB_MAX_SLOTS];
	
	if (!fb->colortex[slot]) {
		fprintf(stderr, "Error, framebuffer slot empty!\n");
		return;
	}
	
	for (i = 0; i < GPU_FB_MAX_SLOTS; i++) {
		if (fb->colortex[i]) {
			attachments[numslots] = GL_COLOR_ATTACHMENT0 + i;
			numslots++;
		}
	}
	
	/* push attributes */
	gpuPushAttrib(GPU_ENABLE_BIT | GPU_VIEWPORT_BIT);
	glDisable(GL_SCISSOR_TEST);

	/* bind framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);

	/* last bound prevails here, better allow explicit control here too */
	glDrawBuffers(numslots, attachments);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + slot);

	/* set default viewport */
	glViewport(0, 0, GPU_texture_width(fb->colortex[slot]), GPU_texture_height(fb->colortex[slot]));
	GG.currentfb = fb->object;
}

void GPU_framebuffer_bind(GPUFrameBuffer *fb)
{
	int numslots = 0, i;
	GLenum attachments[GPU_FB_MAX_SLOTS];
	GLenum readattachement = 0;
	GPUTexture *tex;

	for (i = 0; i < GPU_FB_MAX_SLOTS; i++) {
		if (fb->colortex[i]) {
			attachments[numslots] = GL_COLOR_ATTACHMENT0 + i;
			tex = fb->colortex[i];

			if (!readattachement)
				readattachement = GL_COLOR_ATTACHMENT0 + i;

			numslots++;
		}
	}

	/* bind framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);

	if (numslots == 0) {
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		tex = fb->depthtex;
	}
	else {
		/* last bound prevails here, better allow explicit control here too */
		glDrawBuffers(numslots, attachments);
		glReadBuffer(readattachement);
	}

	if (GPU_texture_target(tex) == GL_TEXTURE_2D_MULTISAMPLE) {
		glEnable(GL_MULTISAMPLE);
	}

	glViewport(0, 0, GPU_texture_width(tex), GPU_texture_height(tex));
	GG.currentfb = fb->object;
}

void GPU_framebuffer_texture_unbind(GPUFrameBuffer *UNUSED(fb), GPUTexture *UNUSED(tex))
{
	/* Restore attributes. */
	gpuPopAttrib();
}

void GPU_framebuffer_bind_no_save(GPUFrameBuffer *fb, int slot)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	/* last bound prevails here, better allow explicit control here too */
	glDrawBuffer(GL_COLOR_ATTACHMENT0 + slot);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + slot);

	/* push matrices and set default viewport and matrix */
	glViewport(0, 0, GPU_texture_width(fb->colortex[slot]), GPU_texture_height(fb->colortex[slot]));
	GG.currentfb = fb->object;
}

bool GPU_framebuffer_bound(GPUFrameBuffer *fb)
{
	return fb->object == GG.currentfb;
}

bool GPU_framebuffer_check_valid(GPUFrameBuffer *fb, char err_out[256])
{
	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	GG.currentfb = fb->object;

	/* On macOS glDrawBuffer must be set when checking completeness,
	 * otherwise it will return GL_FRAMEBUFFER_UNSUPPORTED when only a
	 * color buffer without depth is used. */
	if (fb->colortex[0]) {
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status != GL_FRAMEBUFFER_COMPLETE) {
		GPU_framebuffer_restore();
		gpu_print_framebuffer_error(status, err_out);
		return false;
	}

	return true;
}

void GPU_framebuffer_free(GPUFrameBuffer *fb)
{
	int i;
	if (fb->depthtex)
		GPU_framebuffer_texture_detach(fb->depthtex);

	for (i = 0; i < GPU_FB_MAX_SLOTS; i++) {
		if (fb->colortex[i]) {
			GPU_framebuffer_texture_detach(fb->colortex[i]);
		}
	}

	if (fb->object) {
		glDeleteFramebuffers(1, &fb->object);

		if (GG.currentfb == fb->object) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			GG.currentfb = 0;
		}
	}

	MEM_freeN(fb);
}

void GPU_framebuffer_restore(void)
{
	if (GG.currentfb != 0) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		GG.currentfb = 0;
	}
}

void GPU_framebuffer_blur(
        GPUFrameBuffer *fb, GPUTexture *tex,
        GPUFrameBuffer *blurfb, GPUTexture *blurtex)
{
	const float fullscreencos[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}};
	const float fullscreenuvs[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};

	static Gwn_VertFormat format = {0};
	static Gwn_VertBuf vbo = {{0}};
	static Gwn_Batch batch = {{0}};

	const float scaleh[2] = {1.0f / GPU_texture_width(blurtex), 0.0f};
	const float scalev[2] = {0.0f, 1.0f / GPU_texture_height(tex)};

	GPUShader *blur_shader = GPU_shader_get_builtin_shader(GPU_SHADER_SEP_GAUSSIAN_BLUR);

	if (!blur_shader)
		return;

	/* Preparing to draw quad */
	if (format.attrib_ct == 0) {
		unsigned int i = 0;
		/* Vertex format */
		unsigned int pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		unsigned int uvs = GWN_vertformat_attr_add(&format, "uvs", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

		/* Vertices */
		GWN_vertbuf_init_with_format(&vbo, &format);
		GWN_vertbuf_data_alloc(&vbo, 36);

		for (int j = 0; j < 3; ++j) {
			GWN_vertbuf_attr_set(&vbo, uvs, i, fullscreenuvs[j]);
			GWN_vertbuf_attr_set(&vbo, pos, i++, fullscreencos[j]);
		}
		for (int j = 1; j < 4; ++j) {
			GWN_vertbuf_attr_set(&vbo, uvs, i, fullscreenuvs[j]);
			GWN_vertbuf_attr_set(&vbo, pos, i++, fullscreencos[j]);
		}

		GWN_batch_init(&batch, GL_TRIANGLES, &vbo, NULL);
	}
		
	glDisable(GL_DEPTH_TEST);
	
	/* Blurring horizontally */
	/* We do the bind ourselves rather than using GPU_framebuffer_texture_bind() to avoid
	 * pushing unnecessary matrices onto the OpenGL stack. */
	glBindFramebuffer(GL_FRAMEBUFFER, blurfb->object);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	
	/* avoid warnings from texture binding */
	GG.currentfb = blurfb->object;

	glViewport(0, 0, GPU_texture_width(blurtex), GPU_texture_height(blurtex));

	GPU_texture_bind(tex, 0);

	GWN_batch_program_set_builtin(&batch, GPU_SHADER_SEP_GAUSSIAN_BLUR);
	GWN_batch_uniform_2f(&batch, "ScaleU", scaleh[0], scaleh[1]);
	GWN_batch_uniform_1i(&batch, "textureSource", GL_TEXTURE0);
	GWN_batch_draw(&batch);

	/* Blurring vertically */
	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	
	GG.currentfb = fb->object;
	
	glViewport(0, 0, GPU_texture_width(tex), GPU_texture_height(tex));

	GPU_texture_bind(blurtex, 0);

	/* Hack to make the following uniform stick */
	GWN_batch_program_set_builtin(&batch, GPU_SHADER_SEP_GAUSSIAN_BLUR);
	GWN_batch_uniform_2f(&batch, "ScaleU", scalev[0], scalev[1]);
	GWN_batch_uniform_1i(&batch, "textureSource", GL_TEXTURE0);
	GWN_batch_draw(&batch);
}

void GPU_framebuffer_blit(
        GPUFrameBuffer *fb_read, int read_slot, GPUFrameBuffer *fb_write,
        int write_slot, bool use_depth, bool use_stencil)
{
	GPUTexture *read_tex = (use_depth || use_stencil) ? fb_read->depthtex : fb_read->colortex[read_slot];
	GPUTexture *write_tex = (use_depth || use_stencil) ? fb_write->depthtex : fb_write->colortex[write_slot];
	int read_attach = (use_depth) ? GL_DEPTH_ATTACHMENT :
	                  (use_stencil) ? GL_DEPTH_STENCIL_ATTACHMENT :
	                  GL_COLOR_ATTACHMENT0 + GPU_texture_framebuffer_attachment(read_tex);
	int write_attach = (use_depth) ? GL_DEPTH_ATTACHMENT :
	                   (use_stencil) ? GL_DEPTH_STENCIL_ATTACHMENT :
	                   GL_COLOR_ATTACHMENT0 + GPU_texture_framebuffer_attachment(write_tex);
	int read_bind = GPU_texture_opengl_bindcode(read_tex);
	int write_bind = GPU_texture_opengl_bindcode(write_tex);
	const int read_w = GPU_texture_width(read_tex);
	const int read_h = GPU_texture_height(read_tex);
	const int write_w = GPU_texture_width(write_tex);
	const int write_h = GPU_texture_height(write_tex);


	/* Never both! */
	BLI_assert(!(use_depth && use_stencil));

	if (use_depth) {
		BLI_assert(GPU_texture_depth(read_tex) && GPU_texture_depth(write_tex));
		BLI_assert(GPU_texture_format(read_tex) == GPU_texture_format(write_tex));
	}
	else if (use_stencil) {
		BLI_assert(GPU_texture_stencil(read_tex) && GPU_texture_stencil(write_tex));
		BLI_assert(GPU_texture_format(read_tex) == GPU_texture_format(write_tex));
	}

	/* read from multi-sample buffer */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fb_read->object);
	glFramebufferTexture2D(
	        GL_READ_FRAMEBUFFER, read_attach,
	        GPU_texture_target(read_tex), read_bind, 0);
	BLI_assert(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	/* write into new single-sample buffer */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_write->object);
	glFramebufferTexture2D(
	        GL_DRAW_FRAMEBUFFER, write_attach,
	        GPU_texture_target(write_tex), write_bind, 0);
	BLI_assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	glDrawBuffer((use_depth || use_stencil) ? GL_COLOR_ATTACHMENT0 : read_attach);
	glBlitFramebuffer(0, 0, read_w, read_h, 0, 0, write_w, write_h,
	                  (use_depth) ? GL_DEPTH_BUFFER_BIT :
	                  (use_stencil) ? GL_STENCIL_BUFFER_BIT :
	                  GL_COLOR_BUFFER_BIT, GL_NEAREST);

	/* Restore previous framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, GG.currentfb);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
}

/**
 * Use this if you need to custom downsample your texture and use the previous mip level as input.
 * This function only takes care of the correct texture handling. It execute the callback for each texture level.
 **/
void GPU_framebuffer_recursive_downsample(
        GPUFrameBuffer *fb, GPUTexture *tex, int num_iter, void (*callback)(void *userData, int level), void *userData)
{
	int i;
	int current_dim[2] = {GPU_texture_width(tex), GPU_texture_height(tex)};
	GLenum attachment;

	/* Manually setup framebuffer to not use GPU_texture_framebuffer_set() */
	glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
	GG.currentfb = fb->object;

	if (GPU_texture_stencil(tex) && GPU_texture_depth(tex))
		attachment = GL_DEPTH_STENCIL_ATTACHMENT;
	else if (GPU_texture_depth(tex))
		attachment = GL_DEPTH_ATTACHMENT;
	else
		attachment = GL_COLOR_ATTACHMENT0;

	/* last bound prevails here, better allow explicit control here too */
	if (GPU_texture_depth(tex)) {
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
	else {
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
	}

	for (i = 1; i < num_iter + 1; i++) {

		/* calculate next viewport size */
		current_dim[0] /= 2;
		current_dim[1] /= 2;

		if (current_dim[0] <= 2 && current_dim[1] <= 2) {
			/* Cannot reduce further. */
			break;
		}

		/* ensure that the viewport size is always at least 1x1 */
		CLAMP_MIN(current_dim[0], 1);
		CLAMP_MIN(current_dim[1], 1);

		glViewport(0, 0, current_dim[0], current_dim[1]);

		/* bind next level for rendering but first restrict fetches only to previous level */
		GPU_texture_bind(tex, 0);
		glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_BASE_LEVEL, i - 1);
		glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, i - 1);
		GPU_texture_unbind(tex);

		glFramebufferTexture(GL_FRAMEBUFFER, attachment, GPU_texture_opengl_bindcode(tex), i);

		callback(userData, i);
	}

	glFramebufferTexture(GL_FRAMEBUFFER, attachment, 0, 0);

	/* reset mipmap level range for the depth image */
	GPU_texture_bind(tex, 0);
	glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, i - 1);
	GPU_texture_unbind(tex);
}

/* GPUOffScreen */

struct GPUOffScreen {
	GPUFrameBuffer *fb;
	GPUTexture *color;
	GPUTexture *depth;
};

GPUOffScreen *GPU_offscreen_create(int width, int height, int samples, bool depth, bool high_bitdepth, char err_out[256])
{
	GPUOffScreen *ofs;

	ofs = MEM_callocN(sizeof(GPUOffScreen), "GPUOffScreen");

	ofs->fb = GPU_framebuffer_create();
	if (!ofs->fb) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	if (samples) {
		if (!GLEW_ARB_texture_multisample ||
		    /* This is required when blitting from a multi-sampled buffers,
		     * even though we're not scaling. */
		    !GLEW_EXT_framebuffer_multisample_blit_scaled)
		{
			samples = 0;
		}
	}

	if (depth) {
		ofs->depth = GPU_texture_create_depth_with_stencil_multisample(width, height, samples, err_out);
		if (!ofs->depth) {
			GPU_offscreen_free(ofs);
			return NULL;
		}

		if (!GPU_framebuffer_texture_attach(ofs->fb, ofs->depth, 0, 0)) {
			GPU_offscreen_free(ofs);
			return NULL;
		}
	}

	if (high_bitdepth) {
		ofs->color = GPU_texture_create_2D_custom_multisample(width, height, 4, GPU_RGBA16F, NULL, samples, err_out);
	}
	else {
		ofs->color = GPU_texture_create_2D_multisample(width, height, NULL, samples, err_out);
	}
	if (!ofs->color) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	if (!GPU_framebuffer_texture_attach(ofs->fb, ofs->color, 0, 0)) {
		GPU_offscreen_free(ofs);
		return NULL;
	}
	
	/* check validity at the very end! */
	if (!GPU_framebuffer_check_valid(ofs->fb, err_out)) {
		GPU_offscreen_free(ofs);
		return NULL;		
	}

	GPU_framebuffer_restore();

	return ofs;
}

void GPU_offscreen_free(GPUOffScreen *ofs)
{
	if (ofs->fb)
		GPU_framebuffer_free(ofs->fb);
	if (ofs->color)
		GPU_texture_free(ofs->color);
	if (ofs->depth)
		GPU_texture_free(ofs->depth);
	
	MEM_freeN(ofs);
}

void GPU_offscreen_bind(GPUOffScreen *ofs, bool save)
{
	glDisable(GL_SCISSOR_TEST);
	if (save)
		GPU_texture_bind_as_framebuffer(ofs->color);
	else {
		GPU_framebuffer_bind_no_save(ofs->fb, 0);
	}
}

void GPU_offscreen_unbind(GPUOffScreen *ofs, bool restore)
{
	if (restore)
		GPU_framebuffer_texture_unbind(ofs->fb, ofs->color);
	GPU_framebuffer_restore();
	glEnable(GL_SCISSOR_TEST);
}

void GPU_offscreen_blit(GPUOffScreen *ofs, int x, int y)
{
	const int w = GPU_texture_width(ofs->color);
	const int h = GPU_texture_height(ofs->color);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, ofs->fb->object);
	GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE) {
		glBlitFramebuffer(0, 0, w, h, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	else {
		gpu_print_framebuffer_error(status, NULL);
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void GPU_offscreen_read_pixels(GPUOffScreen *ofs, int type, void *pixels)
{
	const int w = GPU_texture_width(ofs->color);
	const int h = GPU_texture_height(ofs->color);

	if (GPU_texture_target(ofs->color) == GL_TEXTURE_2D_MULTISAMPLE) {
		/* For a multi-sample texture,
		 * we need to create an intermediate buffer to blit to,
		 * before its copied using 'glReadPixels' */

		/* not needed since 'ofs' needs to be bound to the framebuffer already */
// #define USE_FBO_CTX_SWITCH

		GLuint fbo_blit = 0;
		GLuint tex_blit = 0;
		GLenum status;

		/* create texture for new 'fbo_blit' */
		glGenTextures(1, &tex_blit);
		if (!tex_blit) {
			goto finally;
		}

		glBindTexture(GL_TEXTURE_2D, tex_blit);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, type, 0);

#ifdef USE_FBO_CTX_SWITCH
		/* read from multi-sample buffer */
		glBindFramebuffer(GL_READ_FRAMEBUFFER, ofs->color->fb->object);
		glFramebufferTexture2D(
		        GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + ofs->color->fb_attachment,
		        GL_TEXTURE_2D_MULTISAMPLE, ofs->color->bindcode, 0);
		status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			goto finally;
		}
#endif

		/* write into new single-sample buffer */
		glGenFramebuffers(1, &fbo_blit);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_blit);
		glFramebufferTexture2D(
		        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		        GL_TEXTURE_2D, tex_blit, 0);
		status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			goto finally;
		}

		/* perform the copy */
		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		/* read the results */
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_blit);
		glReadPixels(0, 0, w, h, GL_RGBA, type, pixels);

#ifdef USE_FBO_CTX_SWITCH
		/* restore the original frame-bufer */
		glBindFramebuffer(GL_FRAMEBUFFER, ofs->color->fb->object);
#undef USE_FBO_CTX_SWITCH
#endif


finally:
		/* cleanup */
		if (tex_blit) {
			glDeleteTextures(1, &tex_blit);
		}
		if (fbo_blit) {
			glDeleteFramebuffers(1, &fbo_blit);
		}
	}
	else {
		glReadPixels(0, 0, w, h, GL_RGBA, type, pixels);
	}
}

int GPU_offscreen_width(const GPUOffScreen *ofs)
{
	return GPU_texture_width(ofs->color);
}

int GPU_offscreen_height(const GPUOffScreen *ofs)
{
	return GPU_texture_height(ofs->color);
}

GPUTexture *GPU_offscreen_color_texture(const GPUOffScreen *ofs)
{
	return ofs->color;
}

/* only to be used by viewport code! */
void GPU_offscreen_viewport_data_get(
        GPUOffScreen *ofs,
        GPUFrameBuffer **r_fb, GPUTexture **r_color, GPUTexture **r_depth)
{
	*r_fb = ofs->fb;
	*r_color = ofs->color;
	*r_depth = ofs->depth;
}
