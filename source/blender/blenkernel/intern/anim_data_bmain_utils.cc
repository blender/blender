/* SPDX-FileCopyrightText: 2025 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* `Scene->nodetree` is deprecated but still relevant for backward compatibility. */
#define DNA_DEPRECATED_ALLOW

/** \file
 * \ingroup bke
 */

#include "BKE_anim_data.hh"
#include "BKE_animsys.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

namespace blender {

/* Whole Database Ops -------------------------------------------- */

void BKE_animdata_main_cb(Main *bmain, const FunctionRef<void(ID *, AnimData *)> func)
{
  ID *id;

/* standard data version */
#define ANIMDATA_IDS_CB(first) \
  for (id = static_cast<ID *>(first); id; id = static_cast<ID *>(id->next)) { \
    AnimData *adt = BKE_animdata_from_id(id); \
    if (adt) { \
      func(id, adt); \
    } \
  } \
  (void)0

/* "embedded" nodetree cases (i.e. scene/material/texture->nodetree) */
#define ANIMDATA_NODETREE_IDS_CB(first, NtId_Type) \
  for (id = static_cast<ID *>(first); id; id = static_cast<ID *>(id->next)) { \
    AnimData *adt = BKE_animdata_from_id(id); \
    NtId_Type *ntp = (NtId_Type *)id; \
    if (ntp->nodetree) { \
      AnimData *adt2 = BKE_animdata_from_id((ID *)ntp->nodetree); \
      if (adt2) { \
        func((ID *)ntp->nodetree, adt2); \
      } \
    } \
    if (adt) { \
      func(id, adt); \
    } \
  } \
  (void)0

  /* nodes */
  ANIMDATA_IDS_CB(bmain->nodetrees.first_as<ID>());

  /* textures */
  ANIMDATA_NODETREE_IDS_CB(bmain->textures.first_as<ID>(), Tex);

  /* lights */
  ANIMDATA_NODETREE_IDS_CB(bmain->lights.first_as<ID>(), Light);

  /* materials */
  ANIMDATA_NODETREE_IDS_CB(bmain->materials.first_as<ID>(), Material);

  /* cameras */
  ANIMDATA_IDS_CB(bmain->cameras.first_as<ID>());

  /* shapekeys */
  ANIMDATA_IDS_CB(bmain->shapekeys.first_as<ID>());

  /* metaballs */
  ANIMDATA_IDS_CB(bmain->metaballs.first_as<ID>());

  /* curves */
  ANIMDATA_IDS_CB(bmain->curves.first_as<ID>());

  /* armatures */
  ANIMDATA_IDS_CB(bmain->armatures.first_as<ID>());

  /* lattices */
  ANIMDATA_IDS_CB(bmain->lattices.first_as<ID>());

  /* meshes */
  ANIMDATA_IDS_CB(bmain->meshes.first_as<ID>());

  /* particles */
  ANIMDATA_IDS_CB(bmain->particles.first_as<ID>());

  /* speakers */
  ANIMDATA_IDS_CB(bmain->speakers.first_as<ID>());

  /* movie clips */
  ANIMDATA_IDS_CB(bmain->movieclips.first_as<ID>());

  /* objects */
  ANIMDATA_IDS_CB(bmain->objects.first_as<ID>());

  /* masks */
  ANIMDATA_IDS_CB(bmain->masks.first_as<ID>());

  /* worlds */
  ANIMDATA_NODETREE_IDS_CB(bmain->worlds.first_as<ID>(), World);

  /* scenes */
  ANIMDATA_NODETREE_IDS_CB(bmain->scenes.first_as<ID>(), Scene);

  /* line styles */
  ANIMDATA_IDS_CB(bmain->linestyles.first_as<ID>());

  /* grease pencil */
  ANIMDATA_IDS_CB(bmain->gpencils.first_as<ID>());

  /* grease pencil */
  ANIMDATA_IDS_CB(bmain->grease_pencils.first_as<ID>());

  /* palettes */
  ANIMDATA_IDS_CB(bmain->palettes.first_as<ID>());

  /* cache files */
  ANIMDATA_IDS_CB(bmain->cachefiles.first_as<ID>());

  /* Hair Curves. */
  ANIMDATA_IDS_CB(bmain->hair_curves.first_as<ID>());

  /* pointclouds */
  ANIMDATA_IDS_CB(bmain->pointclouds.first_as<ID>());

  /* volumes */
  ANIMDATA_IDS_CB(bmain->volumes.first_as<ID>());
}

}  // namespace blender
