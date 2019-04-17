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
 * \ingroup blenloader
 *
 * Apply edits to DNA at load time to behave as if old files were written with new names.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#include "DNA_genfile.h"
#include "DNA_listBase.h"

#include "BLO_readfile.h"
#include "readfile.h"

/**
 * Manipulates SDNA before calling #DNA_struct_get_compareflags,
 * allowing us to rename structs and struct members.
 *
 * - This means older versions of Blender won't have access to this data **USE WITH CARE**.
 *
 * - These changes are applied on file load (run-time), similar to versioning for compatibility.
 *
 * \attention ONLY USE THIS KIND OF VERSIONING WHEN `dna_rename_defs.h` ISN'T SUFFICIENT.
 */
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
