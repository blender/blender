/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 *
 * Apply edits to DNA at load time to behave as if old files were written with new names.
 */

#include "DNA_genfile.h"

#include "readfile.hh"

void blo_do_versions_dna(SDNA *sdna, const int versionfile, const int subversionfile)
{
#define DNA_VERSION_ATLEAST(ver, subver) \
  (versionfile > (ver) || (versionfile == (ver) && (subversionfile >= (subver))))

  if (!DNA_VERSION_ATLEAST(280, 2)) {
    /* Version files created in the 'blender2.8' branch
     * between October 2016, and November 2017 (>=280.0 and < 280.2). */
    if (versionfile >= 280) {
      DNA_sdna_patch_struct_by_name(sdna, "SceneLayer", "ViewLayer");
      DNA_sdna_patch_struct_member_by_name(
          sdna, "FileGlobal", "cur_render_layer", "cur_view_layer");
      DNA_sdna_patch_struct_member_by_name(
          sdna, "ParticleEditSettings", "scene_layer", "view_layer");
      DNA_sdna_patch_struct_member_by_name(sdna, "Scene", "active_layer", "active_view_layer");
      DNA_sdna_patch_struct_member_by_name(sdna, "Scene", "render_layers", "view_layers");
      DNA_sdna_patch_struct_member_by_name(sdna, "WorkSpace", "render_layer", "view_layer");
    }
  }

  if (!DNA_VERSION_ATLEAST(500, 51)) {
    /* These old struct names were only used by an experimental feature. They were renamed before
     * the feature became official. This versioning just allows to read old files but does not
     * provide forward compatibility. */
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometryClosureInput", "NodeClosureInput");
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometryClosureInputItem", "NodeClosureInputItem");
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometryClosureOutputItem", "NodeClosureOutputItem");
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometryClosureInputItems", "NodeClosureInputItems");
    DNA_sdna_patch_struct_by_name(
        sdna, "NodeGeometryClosureOutputItems", "NodeClosureOutputItems");
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometryClosureOutput", "NodeClosureOutput");
    DNA_sdna_patch_struct_by_name(
        sdna, "NodeGeometryEvaluateClosureInputItem", "NodeEvaluateClosureInputItem");
    DNA_sdna_patch_struct_by_name(
        sdna, "NodeGeometryEvaluateClosureOutputItem", "NodeEvaluateClosureOutputItem");
    DNA_sdna_patch_struct_by_name(
        sdna, "NodeGeometryEvaluateClosureInputItems", "NodeEvaluateClosureInputItems");
    DNA_sdna_patch_struct_by_name(
        sdna, "NodeGeometryEvaluateClosureOutputItems", "NodeEvaluateClosureOutputItems");
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometryEvaluateClosure", "NodeEvaluateClosure");
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometryCombineBundleItem", "NodeCombineBundleItem");
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometryCombineBundle", "NodeCombineBundle");
    DNA_sdna_patch_struct_by_name(
        sdna, "NodeGeometrySeparateBundleItem", "NodeSeparateBundleItem");
    DNA_sdna_patch_struct_by_name(sdna, "NodeGeometrySeparateBundle", "NodeSeparateBundle");
  }

#undef DNA_VERSION_ATLEAST
}
