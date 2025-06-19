/* SPDX-FileCopyrightText: 2025 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* `Scene->nodetree` is deprecated but still relevant for backward compatibility. */
#define DNA_DEPRECATED_ALLOW

/** \file
 * \ingroup bke
 */

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

/* Whole Database Ops -------------------------------------------- */

using namespace blender;

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
  ANIMDATA_IDS_CB(bmain->nodetrees.first);

  /* textures */
  ANIMDATA_NODETREE_IDS_CB(bmain->textures.first, Tex);

  /* lights */
  ANIMDATA_NODETREE_IDS_CB(bmain->lights.first, Light);

  /* materials */
  ANIMDATA_NODETREE_IDS_CB(bmain->materials.first, Material);

  /* cameras */
  ANIMDATA_IDS_CB(bmain->cameras.first);

  /* shapekeys */
  ANIMDATA_IDS_CB(bmain->shapekeys.first);

  /* metaballs */
  ANIMDATA_IDS_CB(bmain->metaballs.first);

  /* curves */
  ANIMDATA_IDS_CB(bmain->curves.first);

  /* armatures */
  ANIMDATA_IDS_CB(bmain->armatures.first);

  /* lattices */
  ANIMDATA_IDS_CB(bmain->lattices.first);

  /* meshes */
  ANIMDATA_IDS_CB(bmain->meshes.first);

  /* particles */
  ANIMDATA_IDS_CB(bmain->particles.first);

  /* speakers */
  ANIMDATA_IDS_CB(bmain->speakers.first);

  /* movie clips */
  ANIMDATA_IDS_CB(bmain->movieclips.first);

  /* objects */
  ANIMDATA_IDS_CB(bmain->objects.first);

  /* masks */
  ANIMDATA_IDS_CB(bmain->masks.first);

  /* worlds */
  ANIMDATA_NODETREE_IDS_CB(bmain->worlds.first, World);

  /* scenes */
  ANIMDATA_NODETREE_IDS_CB(bmain->scenes.first, Scene);

  /* line styles */
  ANIMDATA_IDS_CB(bmain->linestyles.first);

  /* grease pencil */
  ANIMDATA_IDS_CB(bmain->gpencils.first);

  /* grease pencil */
  ANIMDATA_IDS_CB(bmain->grease_pencils.first);

  /* palettes */
  ANIMDATA_IDS_CB(bmain->palettes.first);

  /* cache files */
  ANIMDATA_IDS_CB(bmain->cachefiles.first);

  /* Hair Curves. */
  ANIMDATA_IDS_CB(bmain->hair_curves.first);

  /* pointclouds */
  ANIMDATA_IDS_CB(bmain->pointclouds.first);

  /* volumes */
  ANIMDATA_IDS_CB(bmain->volumes.first);
}

void BKE_animdata_fix_paths_rename_all(ID *ref_id,
                                       const char *prefix,
                                       const char *oldName,
                                       const char *newName)
{
  Main *bmain = G.main; /* XXX UGLY! */
  BKE_animdata_fix_paths_rename_all_ex(bmain, ref_id, prefix, oldName, newName, 0, 0, true);
}

void BKE_animdata_fix_paths_rename_all_ex(Main *bmain,
                                          ID *ref_id,
                                          const char *prefix,
                                          const char *oldName,
                                          const char *newName,
                                          const int oldSubscript,
                                          const int newSubscript,
                                          const bool verify_paths)
{
  BKE_animdata_main_cb(bmain, [&](ID *id, AnimData *adt) {
    BKE_animdata_fix_paths_rename(
        id, adt, ref_id, prefix, oldName, newName, oldSubscript, newSubscript, verify_paths);
  });
}
