/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_modifier_disabler.h"

#include <cstdio>

#include "BLI_listbase.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_layer.hh"
#include "BKE_modifier.hh"

namespace blender::io::usd {

ModifierData *ModifierDisabler::get_subsurf_modifier(Scene *scene, const Object *ob, ModifierMode mode)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

  for (; md; md = md->prev) {
    /* Ignore disabled modifiers. */
    if (!BKE_modifier_is_enabled(scene, md, mode)) {
      continue;
    }

    if (md->type == eModifierType_Subsurf) {
      SubsurfModifierData *smd = reinterpret_cast<SubsurfModifierData *>(md);

      if (smd->subdivType == ME_CC_SUBSURF) {
        /* This is a Catmull-Clark modifier. */
        return md;
      }

      /* Not Catmull-Clark, so ignnore it. */
      return nullptr;
    }

    if ((md->type != eModifierType_Displace) && (md->type != eModifierType_ParticleSystem)) {
      /*  */
      return NULL;
    }
  }

  return NULL;
}

ModifierDisabler::ModifierDisabler(Depsgraph *depsgraph, const USDExportParams &export_params)
    : depsgraph_(depsgraph), export_params_(export_params)
{
}

ModifierDisabler::~ModifierDisabler()
{
  /* Enable previously disabled modifiers. */
  for (ModifierData *modifier : disabled_modifiers_) {
    modifier->mode &= ~eModifierMode_DisableTemporary;
  }

  /* Update object to render with restored modifiers in the viewport. */
  for (Object *object : modified_objects_) {
    DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  }
}

void ModifierDisabler::disable_modifiers()
{
  /* We only handle the case where subdivision modifiers are to be
   * disabled. */
  if (export_params_.export_subdiv != USD_SUBDIV_TESSELLATE) {
    return;
  }

  eEvaluationMode eval_mode = DEG_get_mode(depsgraph_);
  const ModifierMode mode = eval_mode == DAG_EVAL_VIEWPORT ?
                                eModifierMode_Realtime :
                                eModifierMode_Render;

  Scene *scene = DEG_get_input_scene(depsgraph_);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph_);

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    Object *object = base->object;

    if (object->type != OB_MESH) {
      continue;
    }

    ModifierData *mod = get_subsurf_modifier(scene, object, mode);

    if (!mod) {
      continue;
    }

    /* This might disable more modifiers than necessary, as it doesn't take restrictions like
     * "export selected objects only" into account. */
    disable_modifier(mod);
    modified_objects_.insert(object);
    DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  }
}

void ModifierDisabler::disable_modifier(ModifierData *mod)
{
  mod->mode |= eModifierMode_DisableTemporary;
  disabled_modifiers_.insert(mod);
}

}  // namespace blender::io::usd
