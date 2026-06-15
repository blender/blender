/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include "BKE_screen.hh"

#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"

#include "BLT_translation.hh"

#include "userpref_intern.hh"

namespace blender {

void userpref_panels_register(ARegionType &region_type)
{
  PanelType *panel_type = MEM_new_zeroed<PanelType>(__func__);
  /* TODO panel should be renamed to just "USERPREF_PT_asset_libraries, it's not contained in the
   * file paths section anymore. This is a compatibility breaking change though. */
  STRNCPY_UTF8(panel_type->idname, "USERPREF_PT_file_paths_asset_libraries");
  STRNCPY_UTF8(panel_type->label, N_("Asset Libraries"));
  STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  STRNCPY(panel_type->context, "assets");
  panel_type->space_type = SPACE_USERPREF;
  panel_type->region_type = RGN_TYPE_WINDOW;
  panel_type->draw = userpref_asset_libraries_panel_draw;
  BLI_addtail(&region_type.paneltypes, panel_type);
}

}  // namespace blender
