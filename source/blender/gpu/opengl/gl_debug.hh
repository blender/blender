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

#pragma once

namespace blender {
namespace gpu {
namespace debug {

/* Enabled on MacOS by default since there is no support for debug callbacks. */
#if defined(DEBUG) && defined(__APPLE__)
#  define GL_CHECK_ERROR(info) debug::check_gl_error(info)
#else
#  define GL_CHECK_ERROR(info)
#endif

#ifdef DEBUG
#  define GL_CHECK_RESOURCES(info) debug::check_gl_resources(info)
#else
#  define GL_CHECK_RESOURCES(info)
#endif

void check_gl_error(const char *info);
void check_gl_resources(const char *info);
void init_gl_callbacks(void);

}  // namespace debug
}  // namespace gpu
}  // namespace blender
