/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::ocio {

class PackedImage;

class CPUProcessor {
 public:
  virtual ~CPUProcessor() = default;

  /**
   * Returns true if the processor is known to not perform any actual color space conversion.
   */
  virtual bool is_noop() const = 0;

  /**
   * Apply the processor on a single pixel.
   * The pixel is modified in-place.
   */
  virtual void apply_rgb(float rgb[3]) const = 0;

  /**
   * Apply the processor on a single pixel with straight (un-premultiplied) alpha.
   * The pixel is modified in-place.
   */
  virtual void apply_rgba(float rgba[4]) const = 0;

  /**
   * Apply the processor on a single pixel with associated (premultiplied) alpha.
   * The pixel is modified in-place.
   */
  virtual void apply_rgba_predivide(float rgba[4]) const = 0;

  /**
   * Apply processor on every pixel of the image with straight (un-premultiplied) alpha.
   */
  virtual void apply(const PackedImage &image) const = 0;

  /**
   * Apply processor on every pixel of the image with associated (premultiplied) alpha.
   */
  virtual void apply_predivide(const PackedImage &image) const = 0;
};

}  // namespace blender::ocio
