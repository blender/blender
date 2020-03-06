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
 * \ingroup edscr
 *
 * Helper functions for area/region API.
 */

#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "WM_message.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

/* -------------------------------------------------------------------- */
/** \name Generic Tool System Region Callbacks
 * \{ */

/**
 * Callback for #ARegionType.message_subscribe
 */
void ED_region_generic_tools_region_message_subscribe(const struct bContext *UNUSED(C),
                                                      struct WorkSpace *UNUSED(workspace),
                                                      struct Scene *UNUSED(scene),
                                                      struct bScreen *UNUSED(screen),
                                                      struct ScrArea *UNUSED(sa),
                                                      struct ARegion *region,
                                                      struct wmMsgBus *mbus)
{
  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = region,
      .user_data = region,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };
  WM_msg_subscribe_rna_anon_prop(mbus, WorkSpace, tools, &msg_sub_value_region_tag_redraw);
}

/**
 * Callback for #ARegionType.snap_size
 */
int ED_region_generic_tools_region_snap_size(const ARegion *region, int size, int axis)
{
  if (axis == 0) {
    /* Using Y axis avoids slight feedback loop when adjusting X. */
    const float aspect = BLI_rctf_size_y(&region->v2d.cur) /
                         (BLI_rcti_size_y(&region->v2d.mask) + 1);
    const float icon_size = ICON_DEFAULT_HEIGHT_TOOLBAR / aspect;
    const float column = 1.25f * icon_size;
    const float margin = 0.5f * icon_size;
    const float snap_units[] = {
        column + margin,
        (2.0f * column) + margin,
        (2.7f * column) + margin,
    };
    int best_diff = INT_MAX;
    int best_size = size;
    /* Only snap if less than last snap unit. */
    if (size <= snap_units[ARRAY_SIZE(snap_units) - 1]) {
      for (uint i = 0; i < ARRAY_SIZE(snap_units); i += 1) {
        const int test_size = snap_units[i];
        const int test_diff = abs(test_size - size);
        if (test_diff < best_diff) {
          best_size = test_size;
          best_diff = test_diff;
        }
      }
    }
    return best_size;
  }
  return size;
}

/** \} */
