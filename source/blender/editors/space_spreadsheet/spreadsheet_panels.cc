/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_screen.h"

#include "BLT_translation.h"

#include "spreadsheet_dataset_draw.hh"
#include "spreadsheet_intern.hh"

namespace blender::ed::spreadsheet {

void spreadsheet_data_set_region_panels_register(ARegionType &region_type)
{
  PanelType *panel_type = MEM_cnew<PanelType>(__func__);
  STRNCPY(panel_type->idname, "SPREADSHEET_PT_data_set");
  STRNCPY(panel_type->label, N_("Data Set"));
  STRNCPY(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  panel_type->flag = PANEL_TYPE_NO_HEADER;
  panel_type->draw = spreadsheet_data_set_panel_draw;
  BLI_addtail(&region_type.paneltypes, panel_type);
}

}  // namespace blender::ed::spreadsheet
