/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "abc_subdiv_disabler.h"

#include <cstdio>

#include "BLI_listbase.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_layer.h"
#include "BKE_modifier.h"

namespace blender::io::alembic {

SubdivModifierDisabler::SubdivModifierDisabler(Depsgraph *depsgraph) : depsgraph_(depsgraph) {}

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

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
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
