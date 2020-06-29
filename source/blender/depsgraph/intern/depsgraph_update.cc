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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/depsgraph_update.h"

#include "DEG_depsgraph.h"

#include "intern/depsgraph_type.h"

namespace deg = blender::deg;

namespace blender {
namespace deg {

static DEG_EditorUpdateIDCb deg_editor_update_id_cb = nullptr;
static DEG_EditorUpdateSceneCb deg_editor_update_scene_cb = nullptr;

void deg_editors_id_update(const DEGEditorUpdateContext *update_ctx, ID *id)
{
  if (deg_editor_update_id_cb != nullptr) {
    deg_editor_update_id_cb(update_ctx, id);
  }
}

void deg_editors_scene_update(const DEGEditorUpdateContext *update_ctx, bool updated)
{
  if (deg_editor_update_scene_cb != nullptr) {
    deg_editor_update_scene_cb(update_ctx, updated);
  }
}

}  // namespace deg
}  // namespace blender

/* Set callbacks which are being called when depsgraph changes. */
void DEG_editors_set_update_cb(DEG_EditorUpdateIDCb id_func, DEG_EditorUpdateSceneCb scene_func)
{
  deg::deg_editor_update_id_cb = id_func;
  deg::deg_editor_update_scene_cb = scene_func;
}
