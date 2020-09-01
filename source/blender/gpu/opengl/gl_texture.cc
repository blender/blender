/*
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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "gl_backend.hh"

#include "gl_texture.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GLTexture::GLTexture(const char *name) : Texture(name)
{
  BLI_assert(GPU_context_active_get() != NULL);

  glGenTextures(1, &tex_id_);

#ifndef __APPLE__
  if ((G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    char sh_name[64];
    SNPRINTF(sh_name, "Texture-%s", name);
    glObjectLabel(GL_TEXTURE, tex_id_, -1, sh_name);
  }
#endif
}

GLTexture::~GLTexture()
{
  GLBackend::get()->tex_free(tex_id_);
}

void GLTexture::init(void)
{
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operations
 * \{ */

void GLTexture::bind(int /*slot*/)
{
}

void GLTexture::update(void * /*data*/)
{
}

void GLTexture::update_sub(void * /*data*/, int /*offset*/[3], int /*size*/[3])
{
}

void GLTexture::generate_mipmap(void)
{
}

void GLTexture::copy_to(Texture * /*tex*/)
{
}

void GLTexture::swizzle_set(char /*swizzle_mask*/[4])
{
}

/** \} */

/* TODO(fclem) Legacy. Should be removed at some point. */
uint GLTexture::gl_bindcode_get(void)
{
  return tex_id_;
}

}  // namespace blender::gpu
