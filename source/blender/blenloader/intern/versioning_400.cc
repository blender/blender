/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "CLG_log.h"

#include "BKE_main.h"
#include "BKE_mesh_legacy_convert.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "versioning_common.h"

// static CLG_LogRef LOG = {"blo.readfile.doversion"};

static void version_mesh_legacy_to_struct_of_array_format(Mesh &mesh)
{
  BKE_mesh_legacy_convert_flags_to_hide_layers(&mesh);
  BKE_mesh_legacy_convert_mpoly_to_material_indices(&mesh);
  BKE_mesh_legacy_bevel_weight_to_layers(&mesh);
}

void blo_do_versions_400(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  // if (!MAIN_VERSION_ATLEAST(bmain, 400, 0)) {
  /* This is done here because we will continue to write with the old format until 4.0, so we need
   * to convert even "current" files. Keep the check commented out for now so the versioning isn't
   * turned off right after the 4.0 bump. */
  LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
    version_mesh_legacy_to_struct_of_array_format(*mesh);
  }
  // }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}
