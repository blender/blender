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

/** \file \ingroup edscr
 *
 * Helper functions for area/region API.
 */

#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "WM_api.h"
#include "WM_message.h"

#include "ED_screen.h"

#include "UI_interface.h"

/* -------------------------------------------------------------------- */
/** \name Generic Tool System Region Callbacks
 * \{ */

/**
 * Callback for #ARegionType.message_subscribe
 */
void ED_region_generic_tools_region_message_subscribe(
        const struct bContext *UNUSED(C),
        struct WorkSpace *UNUSED(workspace), struct Scene *UNUSED(scene),
        struct bScreen *UNUSED(screen), struct ScrArea *UNUSED(sa), struct ARegion *ar,
        struct wmMsgBus *mbus)
{
	wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
		.owner = ar,
		.user_data = ar,
		.notify = ED_region_do_msg_notify_tag_redraw,
	};
	WM_msg_subscribe_rna_anon_prop(mbus, WorkSpace, tools, &msg_sub_value_region_tag_redraw);
}

/**
 * Callback for #ARegionType.snap_size
 */
int ED_region_generic_tools_region_snap_size(const ARegion *ar, int size, int axis)
{
	if (axis == 0) {
		/* Note, this depends on the icon size: see #ICON_DEFAULT_HEIGHT_TOOLBAR. */
		const float snap_units[] = {2 + 0.8f, 4 + 0.8f};
		const float aspect = BLI_rctf_size_x(&ar->v2d.cur) / (BLI_rcti_size_x(&ar->v2d.mask) + 1);
		int best_diff = INT_MAX;
		int best_size = size;
		for (uint i = 0; i < ARRAY_SIZE(snap_units); i += 1) {
			const int test_size = (snap_units[i] * U.widget_unit) / (UI_DPI_FAC * aspect);
			const int test_diff = ABS(test_size - size);
			if (test_diff < best_diff) {
				best_size = test_size;
				best_diff = test_diff;
			}
		}
		return best_size;
	}
	return size;
}

/** \} */
