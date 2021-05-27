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
 */

/** \file
 * \ingroup gpu
 */

#include "gl_compute.hh"

#include "gl_debug.hh"

#include "glew-mx.h"

namespace blender::gpu {

void GLCompute::dispatch(int group_x_len, int group_y_len, int group_z_len)
{
  glDispatchCompute(group_x_len, group_y_len, group_z_len);
  debug::check_gl_error("Dispatch Compute");
}

}  // namespace blender::gpu
