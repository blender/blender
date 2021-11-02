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
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

/* Forward declarations */
struct GPUTexture;
struct ImBuf;
struct Image;

/* *********** LISTS *********** */

namespace blender::draw::image_engine {

/* GPUViewport.storage
 * Is freed every time the viewport engine changes. */
struct IMAGE_PassList {
  DRWPass *image_pass;
};

struct IMAGE_PrivateData {
  void *lock;
  struct ImBuf *ibuf;
  struct Image *image;
  struct DRWView *view;

  struct GPUTexture *texture;
  bool owns_texture;
};

struct IMAGE_StorageList {
  IMAGE_PrivateData *pd;
};

struct IMAGE_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  IMAGE_PassList *psl;
  IMAGE_StorageList *stl;
};

/* image_shader.c */
GPUShader *IMAGE_shader_image_get(bool is_tiled_image);
void IMAGE_shader_library_ensure(void);
void IMAGE_shader_free(void);

}  // namespace blender::draw::image_engine

