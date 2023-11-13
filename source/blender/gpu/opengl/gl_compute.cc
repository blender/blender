/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gl_compute.hh"

#include "gl_debug.hh"

namespace blender::gpu {

void GLCompute::dispatch(int group_x_len, int group_y_len, int group_z_len)
{
  GL_CHECK_RESOURCES("Compute");

  /* Sometime we reference a dispatch size but we want to skip it by setting one dimension to 0.
   * Avoid error being reported on some implementation for these case. */
  if (group_x_len == 0 || group_y_len == 0 || group_z_len == 0) {
    return;
  }

  glDispatchCompute(group_x_len, group_y_len, group_z_len);
}

}  // namespace blender::gpu
