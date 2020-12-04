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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */
#include "abc_subdiv_disabler.h"

#include <cstdio>

#include "BLI_listbase.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_modifier.h"

namespace blender::io::alembic {

SubdivModifierDisabler::SubdivModifierDisabler(Depsgraph *depsgraph) : depsgraph_(depsgraph)
{
}

SubdivModifierDisabler::~SubdivModifierDisabler()
{
  for (ModifierData *modifier : disabled_modifiers_) {
    modifier->mode &= ~eModifierMode_DisableTemporary;
  }
}

void SubdivModifierDisabler::disable_modifiers()
{
  Scene *scene = DEG_get_input_scene(depsgraph_);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph_);

  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    Object *object = base->object;

    if (object->type != OB_MESH) {
      continue;
    }

    ModifierData *subdiv = get_subdiv_modifier(scene, object);
    if (subdiv == nullptr) {
      continue;
    }

    /* This disables more modifiers than necessary, as it doesn't take restrictions like
     * "export selected objects only" into account. However, with the subsurfs disabled,
     * moving to a different frame is also going to be faster, so in the end this is probably
     * a good thing to do. */
    subdiv->mode |= eModifierMode_DisableTemporary;
    disabled_modifiers_.insert(subdiv);
    DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  }
}

/* Check if the mesh is a subsurf, ignoring disabled modifiers and
 * displace if it's after subsurf. */
ModifierData *SubdivModifierDisabler::get_subdiv_modifier(Scene *scene, Object *ob)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

  for (; md; md = md->prev) {
    if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Render)) {
      continue;
    }

    if (md->type == eModifierType_Subsurf) {
      SubsurfModifierData *smd = reinterpret_cast<SubsurfModifierData *>(md);

      if (smd->subdivType == ME_CC_SUBSURF) {
        return md;
      }
    }

    /* mesh is not a subsurf. break */
    if (!ELEM(md->type, eModifierType_Displace, eModifierType_ParticleSystem)) {
      return nullptr;
    }
  }

  return nullptr;
}

}  // namespace blender::io::alembic
