/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 *
 * Apply edits to DNA at load time to behave as if old files were written with new names.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#include "DNA_genfile.h"
#include "DNA_listBase.h"

#include "BLO_readfile.h"
#include "readfile.hh"

void blo_do_versions_dna(SDNA *sdna, const int versionfile, const int subversionfile)
{
#define DNA_VERSION_ATLEAST(ver, subver) \
  (versionfile > (ver) || (versionfile == (ver) && (subversionfile >= (subver))))

  if (!DNA_VERSION_ATLEAST(280, 2)) {
    /* Version files created in the 'blender2.8' branch
     * between October 2016, and November 2017 (>=280.0 and < 280.2). */
    if (versionfile >= 280) {
      DNA_sdna_patch_struct(sdna, "SceneLayer", "ViewLayer");
      DNA_sdna_patch_struct(sdna, "SceneLayerEngineData", "ViewLayerEngineData");
      DNA_sdna_patch_struct_member(sdna, "FileGlobal", "cur_render_layer", "cur_view_layer");
      DNA_sdna_patch_struct_member(sdna, "ParticleEditSettings", "scene_layer", "view_layer");
      DNA_sdna_patch_struct_member(sdna, "Scene", "active_layer", "active_view_layer");
      DNA_sdna_patch_struct_member(sdna, "Scene", "render_layers", "view_layers");
      DNA_sdna_patch_struct_member(sdna, "WorkSpace", "render_layer", "view_layer");
    }
  }

#undef DNA_VERSION_ATLEAST
}
