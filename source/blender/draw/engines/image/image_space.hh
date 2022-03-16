/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#pragma once

struct ShaderParameters;

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
                                     ImBuf *image_buffer) = 0;

  /**
   * Retrieve the gpu textures to draw.
   */
  virtual void get_gpu_textures(Image *image,
                                ImageUser *iuser,
                                ImBuf *image_buffer,
                                GPUTexture **r_gpu_texture,
                                bool *r_owns_texture,
                                GPUTexture **r_tex_tile_data) = 0;

  /** \brief Is (wrap) repeat option enabled in the space. */
  virtual bool use_tile_drawing() const = 0;

  /**
   * \brief Initialize r_uv_to_texture matrix to transform from normalized screen space coordinates
   * (0..1) to texture space UV coordinates.
   */
  virtual void init_ss_to_texture_matrix(const ARegion *region,
                                         const float image_display_offset[2],
                                         const float image_resolution[2],
                                         float r_uv_to_texture[4][4]) const = 0;

};  // namespace blender::draw::image_engine
