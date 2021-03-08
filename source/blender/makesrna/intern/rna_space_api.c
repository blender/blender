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
 * \ingroup RNA
 */

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#  include "BKE_global.h"

#  include "ED_fileselect.h"
#  include "ED_screen.h"
#  include "ED_text.h"

static void rna_RegionView3D_update(ID *id, RegionView3D *rv3d, bContext *C)
{
  bScreen *screen = (bScreen *)id;

  ScrArea *area;
  ARegion *region;

  area_region_from_regiondata(screen, rv3d, &area, &region);

  if (area && region && area->spacetype == SPACE_VIEW3D) {
    Main *bmain = CTX_data_main(C);
    View3D *v3d = area->spacedata.first;
    wmWindowManager *wm = CTX_wm_manager(C);
    wmWindow *win;

    for (win = wm->windows.first; win; win = win->next) {
      if (WM_window_get_active_screen(win) == screen) {
        Scene *scene = WM_window_get_active_scene(win);
        ViewLayer *view_layer = WM_window_get_active_view_layer(win);
        Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);

        ED_view3d_update_viewmat(depsgraph, scene, v3d, region, NULL, NULL, NULL, false);
        break;
      }
    }
  }
}

static void rna_SpaceTextEditor_region_location_from_cursor(
    ID *id, SpaceText *st, int line, int column, int r_pixel_pos[2])
{
  bScreen *screen = (bScreen *)id;
  ScrArea *area = BKE_screen_find_area_from_space(screen, (SpaceLink *)st);
  if (area) {
    ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
    const int cursor_co[2] = {line, column};
    ED_text_region_location_from_cursor(st, region, cursor_co, r_pixel_pos);
  }
}

#else

void RNA_api_region_view3d(StructRNA *srna)
{
  FunctionRNA *func;

  func = RNA_def_function(srna, "update", "rna_RegionView3D_update");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Recalculate the view matrices");
}

void RNA_api_space_node(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(
      srna, "cursor_location_from_region", "rna_SpaceNodeEditor_cursor_location_from_region");
  RNA_def_function_ui_description(func, "Set the cursor location using region coordinates");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_int(func, "x", 0, INT_MIN, INT_MAX, "x", "Region x coordinate", -10000, 10000);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func, "y", 0, INT_MIN, INT_MAX, "y", "Region y coordinate", -10000, 10000);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

void RNA_api_space_text(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(
      srna, "region_location_from_cursor", "rna_SpaceTextEditor_region_location_from_cursor");
  RNA_def_function_ui_description(
      func, "Retrieve the region position from the given line and character position");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_int(func, "line", 0, INT_MIN, INT_MAX, "Line", "Line index", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func, "column", 0, INT_MIN, INT_MAX, "Column", "Column index", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int_array(
      func, "result", 2, NULL, -1, INT_MAX, "", "Region coordinates", -1, INT_MAX);
  RNA_def_function_output(func, parm);
}

void RNA_api_space_filebrowser(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "activate_asset_by_id", "ED_fileselect_activate_by_id");
  RNA_def_function_ui_description(func, "Activate the asset entry that represents the given ID");

  parm = RNA_def_property(func, "id_to_activate", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "ID");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  parm = RNA_def_boolean(
      func,
      "deferred",
      0,
      "",
      "Whether to activate the ID immediately (false) or after the file browser refreshes (true)");
}

#endif
