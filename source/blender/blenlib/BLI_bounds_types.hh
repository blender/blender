/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

namespace blender {

template<typename T> struct Bounds {
  T min;
  T max;
  Bounds() = default;
  Bounds(const T &value) : min(value), max(value) {}
  Bounds(const T &min, const T &max) : min(min), max(max) {}

  /**
   * Returns true when the size of the bounds is zero (or negative).
   * This matches the behavior of #BLI_rcti_is_empty/#BLI_rctf_is_empty.
   */
  bool is_empty() const;
  /**
   * Return the center (i.e. the midpoint) of the bounds.
   * This matches the behavior of #BLI_rctf_cent/#BLI_rcti_cent.
   */
  T center() const;
  /**
   * Return the size of the bounds.
   * E.g. for a Bounds<float3> this would return the dimensions of bounding box as a float3.
   * This matches the behavior of #BLI_rctf_size/#BLI_rcti_size.
   */
  T size() const;

  /**
   * Translate the bounds by #offset.
   * This matches the behavior of #BLI_rctf_translate/#BLI_rcti_translate.
   */
  void translate(const T &offset);
  /**
   * Scale the bounds from the center.
   * This matches the behavior of #BLI_rctf_scale/#BLI_rcti_scale.
   */
  void scale_from_center(const T &scale);

  /**
   * Resize the bounds in-place to ensure their size is #new_size.
   * The center of the bounds doesn't change.
   * This matches the behavior of #BLI_rctf_resize/#BLI_rcti_resize.
   */
  void resize(const T &new_size);
  /**
   * Translate the bounds such that their center is #new_center.
   * This matches the behavior of #BLI_rctf_recenter/#BLI_rcti_recenter.
   */
  void recenter(const T &new_center);

  /**
   * Adds some padding to the bounds.
   * This matches the behavior of #BLI_rcti_pad/#BLI_rctf_pad.
   */
  template<typename PaddingT> void pad(const PaddingT &padding);
};

}  // namespace blender
