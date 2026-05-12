/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_math_matrix_types.hh"

namespace blender {

struct ARegion;
struct ImBuf;
struct Image;
struct ImageUser;
struct Main;

namespace image_engine {

struct ShaderParameters;

/**
 * Space accessor.
 *
 * Image engine is used to draw the images inside multiple spaces \see SpaceLink.
 * The #AbstractSpaceAccessor is an interface to communicate with a space.
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
  virtual blender::Image *get_image(Main *bmain) = 0;

  /**
   * Return the #ImageUser of the space.
   *
   * The return value is optional.
   */
  virtual blender::ImageUser *get_image_user() = 0;

  /**
   * Acquire the image buffer of the image.
   *
   * \param image: Image to get the buffer from. Image is the same as returned from the #get_image
   * member.
   * \param lock: pointer to a lock object.
   * \return Image buffer of the given image.
   */
  virtual ImBuf *acquire_image_buffer(blender::Image *image, void **lock) = 0;

  /**
   * Release a previous locked image from #acquire_image_buffer.
   */
  virtual void release_buffer(blender::Image *image, ImBuf *image_buffer, void *lock) = 0;

  /**
   * Update the r_shader_parameters with space specific settings.
   *
   * Only update the #ShaderParameters.flags and #ShaderParameters.shuffle. Other parameters
   * are updated inside the image engine.
   */
  virtual void get_shader_parameters(ShaderParameters &r_shader_parameters,
                                     ImBuf *image_buffer) = 0;

  /** \brief Is (wrap) repeat option enabled in the space. */
  virtual bool use_tile_drawing() const = 0;

  /** \brief Draw image with display window offsets. */
  virtual bool use_display_window() const = 0;

  /** \brief Gets the zoom factor of the space. A factor of 2 is a zoom-in by two times. */
  virtual float get_zoom() const = 0;

  /** \brief Gets the aspect ratio of the image. The ratio is for the vertical axis. */
  virtual float get_aspect_ratio() const = 0;

  /** \brief Gets the pan offset of the space in image pixel space. */
  virtual float2 get_pan_offset() const = 0;
};

}  // namespace image_engine

}  // namespace blender
