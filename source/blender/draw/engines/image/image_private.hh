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

#pragma once

#include <optional>

/* Forward declarations */
extern "C" {
struct GPUTexture;
struct ImBuf;
struct Image;
}

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

/* Shader parameters. */
#define IMAGE_DRAW_FLAG_SHOW_ALPHA (1 << 0)
#define IMAGE_DRAW_FLAG_APPLY_ALPHA (1 << 1)
#define IMAGE_DRAW_FLAG_SHUFFLING (1 << 2)
#define IMAGE_DRAW_FLAG_DEPTH (1 << 3)
#define IMAGE_DRAW_FLAG_DO_REPEAT (1 << 4)
#define IMAGE_DRAW_FLAG_USE_WORLD_POS (1 << 5)

struct ShaderParameters {
  constexpr static float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  int flags = 0;
  float shuffle[4];
  float far_near[2];
  bool use_premul_alpha = false;

  ShaderParameters()
  {
    copy_v4_fl(shuffle, 1.0f);
    copy_v2_fl2(far_near, 100.0f, 0.0f);
  }
};

/**
 *  Space accessor.
 *
 *  Image engine is used to draw the images inside multiple spaces \see SpaceLink.
 *  The AbstractSpaceAccessor is an interface to communicate with a space.
 */
class AbstractSpaceAccessor {
 public:
  virtual ~AbstractSpaceAccessor() = default;

  /**
   * Return the active image of the space.
   *
   * The returned image will be drawn in the space.
   *
   * The return value is optional.
   */
  virtual Image *get_image(Main *bmain) = 0;

  /**
   * Return the #ImageUser of the space.
   *
   * The return value is optional.
   */
  virtual ImageUser *get_image_user() = 0;

  /**
   * Acquire the image buffer of the image.
   *
   * \param image: Image to get the buffer from. Image is the same as returned from the #get_image
   * member.
   * \param lock: pointer to a lock object.
   * \return Image buffer of the given image.
   */
  virtual ImBuf *acquire_image_buffer(Image *image, void **lock) = 0;

  /**
   * Release a previous locked image from #acquire_image_buffer.
   */
  virtual void release_buffer(Image *image, ImBuf *image_buffer, void *lock) = 0;

  /**
   * Update the r_shader_parameters with space specific settings.
   *
   * Only update the #ShaderParameters.flags and #ShaderParameters.shuffle. Other parameters
   * are updated inside the image engine.
   */
  virtual void get_shader_parameters(ShaderParameters &r_shader_parameters,
                                     ImBuf *image_buffer,
                                     bool is_tiled) = 0;

  /**
   * Retrieve the gpu textures to draw.
   */
  virtual void get_gpu_textures(Image *image,
                                ImageUser *iuser,
                                ImBuf *image_buffer,
                                GPUTexture **r_gpu_texture,
                                bool *r_owns_texture,
                                GPUTexture **r_tex_tile_data) = 0;

  /**
   * Does this space override the view.
   * When so this member should return true and the create_view_override must return the view to
   * use during drawing.
   */
  virtual bool has_view_override() const = 0;

  /**
   * Override the view for drawing.
   * Should match #has_view_override.
   */
  virtual DRWView *create_view_override(const ARegion *UNUSED(region)) = 0;

  /**
   * Initialize the matrix that will be used to draw the image. The matrix will be send as object
   * matrix to the drawing pipeline.
   */
  virtual void get_image_mat(const ImBuf *image_buffer,
                             const ARegion *region,
                             float r_mat[4][4]) const = 0;
};  // namespace blender::draw::image_engine

/**
 * Abstract class for a drawing mode of the image engine.
 *
 * The drawing mode decides how to draw the image on the screen. Each way how to draw would have
 * its own subclass. For now there is only a single drawing mode. #DefaultDrawingMode.
 **/
class AbstractDrawingMode {
 public:
  virtual ~AbstractDrawingMode() = default;
  virtual void cache_init(IMAGE_Data *vedata) const = 0;
  virtual void cache_image(AbstractSpaceAccessor *space,
                           IMAGE_Data *vedata,
                           Image *image,
                           ImageUser *iuser,
                           ImBuf *image_buffer) const = 0;
  virtual void draw_scene(IMAGE_Data *vedata) const = 0;
  virtual void draw_finish(IMAGE_Data *vedata) const = 0;
};

/* image_shader.c */
GPUShader *IMAGE_shader_image_get(bool is_tiled_image);
void IMAGE_shader_library_ensure(void);
void IMAGE_shader_free(void);

}  // namespace blender::draw::image_engine
