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
 * The Original Code is Copyright (C) 2015, Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GPU_glew.h"

#include <stdio.h>

#include "RAS_OpenGLOffScreen.h"
#include "RAS_ICanvas.h"

RAS_OpenGLOffScreen::RAS_OpenGLOffScreen(RAS_ICanvas *canvas)
    :m_canvas(canvas), m_depthrb(0), m_colorrb(0), 	m_depthtx(0), m_colortx(0),
     m_fbo(0), m_blitfbo(0), m_blitrbo(0), m_blittex(0), m_target(RAS_OFS_RENDER_BUFFER), m_bound(false)
{
	m_width = 0;
	m_height = 0;
	m_samples = 0;
	m_color = 0;
}

RAS_OpenGLOffScreen::~RAS_OpenGLOffScreen()
{
	Destroy();
}

bool RAS_OpenGLOffScreen::Create(int width, int height, int samples, RAS_OFS_RENDER_TARGET target)
{
	GLenum status;
	GLuint glo[2], fbo;
	GLint max_samples;
	GLenum textarget;

	if (m_fbo) {
		printf("RAS_OpenGLOffScreen::Create(): buffer exists already, destroy first\n");
		return false;
	}
	if (target != RAS_IOffScreen::RAS_OFS_RENDER_BUFFER &&
	    target != RAS_IOffScreen::RAS_OFS_RENDER_TEXTURE)
	{
		printf("RAS_OpenGLOffScreen::Create(): invalid offscren target\n");
		return false;
	}
	if (!GLEW_EXT_framebuffer_object) {
		printf("RAS_OpenGLOffScreen::Create(): frame buffer not supported\n");
		return false;
	}
	if (samples) {
		if (!GLEW_EXT_framebuffer_multisample ||
		    !GLEW_EXT_framebuffer_blit)
		{
			samples = 0;
		}
	}
	if (samples && target == RAS_OFS_RENDER_TEXTURE) {
		// we need this in addition if we use multisample textures
		if (!GLEW_ARB_texture_multisample ||
		    !GLEW_EXT_framebuffer_multisample_blit_scaled)
		{
			samples = 0;
		}
	}
	if (samples) {
		max_samples = 0;
		glGetIntegerv(GL_MAX_SAMPLES_EXT , &max_samples);
		if (samples > max_samples)
			samples = max_samples;
	}
	m_target = target;
	fbo = 0;
	glGenFramebuffersEXT(1, &fbo);
	if (fbo == 0) {
		printf("RAS_OpenGLOffScreen::Create(): frame buffer creation failed: %d\n", (int)glGetError());
		return false;
	}
	m_fbo = fbo;
	glo[0] = glo[1] = 0;
	if (target == RAS_OFS_RENDER_TEXTURE) {
		glGenTextures(2, glo);
		if (glo[0] == 0 || glo[1] == 0) {
			printf("RAS_OpenGLOffScreen::Create(): texture creation failed: %d\n", (int)glGetError());
			goto L_ERROR;
		}
		m_depthtx = glo[0];
		m_color = m_colortx = glo[1];
		if (samples) {
			textarget = GL_TEXTURE_2D_MULTISAMPLE;
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_depthtx);
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_DEPTH_COMPONENT, width, height, true);
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_colortx);
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA8, width, height, true);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
		}
		else {
			textarget = GL_TEXTURE_2D;
			glBindTexture(GL_TEXTURE_2D, m_depthtx);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
			glBindTexture(GL_TEXTURE_2D, m_colortx);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, textarget, m_depthtx, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, textarget, m_colortx, 0);
	}
	else {
		glGenRenderbuffersEXT(2, glo);
		if (glo[0] == 0 || glo[1] == 0) {
			printf("RAS_OpenGLOffScreen::Create(): render buffer creation failed: %d\n", (int)glGetError());
			goto L_ERROR;
		}
		m_depthrb = glo[0];
		m_colorrb = glo[1];
		glBindRenderbufferEXT(GL_RENDERBUFFER, m_depthrb);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT, width, height);
		glBindRenderbufferEXT(GL_RENDERBUFFER, m_colorrb);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
		glBindRenderbufferEXT(GL_RENDERBUFFER, 0);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER, m_depthrb);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER, m_colorrb);
	}
	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
		printf("RAS_OpenGLOffScreen::Create(): frame buffer incomplete: %d\n", (int)status);
		goto L_ERROR;
	}
	m_width = width;
	m_height = height;

	if (samples > 0) {
		GLuint blit_tex;
		GLuint blit_fbo;
		// create a secondary FBO to blit to before the pixel can be read

		/* write into new single-sample buffer */
		glGenFramebuffersEXT(1, &blit_fbo);
		if (!blit_fbo) {
			printf("RAS_OpenGLOffScreen::Create(): failed creating a FBO for multi-sample offscreen buffer\n");
			goto L_ERROR;
		}
		m_blitfbo = blit_fbo;
		blit_tex = 0;
		if (target == RAS_OFS_RENDER_TEXTURE) {
			glGenTextures(1, &blit_tex);
			if (!blit_tex) {
				printf("RAS_OpenGLOffScreen::Create(): failed creating a texture for multi-sample offscreen buffer\n");
				goto L_ERROR;
			}
			// m_color is the texture where the final render goes, the blit texture in this case
			m_color = m_blittex = blit_tex;
			glBindTexture(GL_TEXTURE_2D, m_blittex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_blitfbo);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_blittex, 0);
		}
		else {
			/* create render buffer for new 'fbo_blit' */
			glGenRenderbuffersEXT(1, &blit_tex);
			if (!blit_tex) {
				printf("RAS_OpenGLOffScreen::Create(): failed creating a render buffer for multi-sample offscreen buffer\n");
				goto L_ERROR;
			}
			m_blitrbo = blit_tex;
			glBindRenderbufferEXT(GL_RENDERBUFFER, m_blitrbo);
			glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, 0, GL_RGBA8, width, height);
			glBindRenderbufferEXT(GL_RENDERBUFFER, 0);
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_blitfbo);
			glFramebufferRenderbufferEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER, m_blitrbo);
		}
		status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER_EXT);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, 0);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			printf("RAS_OpenGLOffScreen::Create(): frame buffer for multi-sample offscreen buffer incomplete: %d\n", (int)status);
			goto L_ERROR;
		}
		// remember that multisample is enabled
		m_samples = 1;
	}
	return true;

L_ERROR:
	Destroy();
	return false;
}

void RAS_OpenGLOffScreen::Destroy()
{
	GLuint globj;
	Unbind();
	if (m_fbo) {
		globj = m_fbo;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
		if (m_target == RAS_OFS_RENDER_TEXTURE) {
			GLenum textarget = (m_samples) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, textarget, 0, 0);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, textarget, 0, 0);
		}
		else {
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, 0);
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &globj);
		m_fbo = 0;
	}
	if (m_depthrb) {
		globj = m_depthrb;
		glDeleteRenderbuffers(1, &globj);
		m_depthrb = 0;
	}
	if (m_colorrb) {
		globj = m_colorrb;
		glDeleteRenderbuffers(1, &globj);
		m_colorrb = 0;
	}
	if (m_depthtx) {
		globj = m_depthtx;
		glDeleteTextures(1, &globj);
		m_depthtx = 0;
	}
	if (m_colortx) {
		globj = m_colortx;
		glDeleteTextures(1, &globj);
		m_colortx = 0;
	}
	if (m_blitfbo) {
		globj = m_blitfbo;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_blitfbo);
		if (m_target == RAS_OFS_RENDER_TEXTURE) {
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
		}
		else {
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, 0);
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &globj);
		m_blitfbo = 0;
	}
	if (m_blitrbo) {
		globj = m_blitrbo;
		glDeleteRenderbuffers(1, &globj);
		m_blitrbo = 0;
	}
	if (m_blittex) {
		globj = m_blittex;
		glDeleteTextures(1, &globj);
		m_blittex = 0;
	}
	m_width = 0;
	m_height = 0;
	m_samples = 0;
	m_color = 0;
	m_target = RAS_OFS_RENDER_BUFFER;
}

void RAS_OpenGLOffScreen::Bind(RAS_OFS_BIND_MODE mode)
{
	if (m_fbo) {
		if (mode == RAS_OFS_BIND_RENDER) {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glViewport(0, 0, m_width, m_height);
			glDisable(GL_SCISSOR_TEST);
		}
		else if (!m_blitfbo) {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		}
		else {
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_blitfbo);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		}
		m_bound = true;
	}
}

void RAS_OpenGLOffScreen::Unbind()
{
	if (!m_bound)
		return;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glEnable(GL_SCISSOR_TEST);
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);
	m_bound = false;
}

void RAS_OpenGLOffScreen::MipMap()
{
	if (m_color) {
		glBindTexture(GL_TEXTURE_2D, m_color);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void RAS_OpenGLOffScreen::Blit()
{
	if (m_bound && m_blitfbo) {
		// set the draw target to the secondary FBO, the read target is still the multisample FBO
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, m_blitfbo);

		// sample the primary
		glBlitFramebufferEXT(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// make sure the next glReadPixels will read from the secondary buffer
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER, m_blitfbo);
	}
}
