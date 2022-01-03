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

#include "BKE_screen.h"

#include "BLT_translation.h"

#include "spreadsheet_dataset_draw.hh"
#include "spreadsheet_intern.hh"

namespace blender::ed::spreadsheet {

void spreadsheet_data_set_region_panels_register(ARegionType &region_type)
{
  PanelType *panel_type = MEM_cnew<PanelType>(__func__);
  strcpy(panel_type->idname, "SPREADSHEET_PT_data_set");
  strcpy(panel_type->label, N_("Data Set"));
  strcpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  panel_type->flag = PANEL_TYPE_NO_HEADER;
  panel_type->draw = spreadsheet_data_set_panel_draw;
  BLI_addtail(&region_type.paneltypes, panel_type);
}

}  // namespace blender::ed::spreadsheet
