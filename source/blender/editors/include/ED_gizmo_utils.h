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
 * \ingroup editors
 *
 * \name Generic Gizmo Utilities.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct wmGizmoGroupType;

/** Wrapper function (operator name can't be guessed). */
bool ED_gizmo_poll_or_unlink_delayed_from_operator(const struct bContext *C,
                                                   struct wmGizmoGroupType *gzgt,
                                                   const char *idname);

bool ED_gizmo_poll_or_unlink_delayed_from_tool_ex(const struct bContext *C,
                                                  struct wmGizmoGroupType *gzgt,
                                                  const char *gzgt_idname);

/** Use this as poll function directly for: #wmGizmoGroupType.poll */
bool ED_gizmo_poll_or_unlink_delayed_from_tool(const struct bContext *C,
                                               struct wmGizmoGroupType *gzgt);

#ifdef __cplusplus
}
#endif
