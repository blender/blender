/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_toolsystem.c
 *  \ingroup wm
 *
 * Experimental tool-system>
 */

#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

void WM_toolsystem_unlink(bContext *C, WorkSpace *workspace)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = bmain->wm.first;

	if (workspace->tool.manipulator_group[0]) {
		wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_find(workspace->tool.manipulator_group, false);
		if (wgt != NULL) {
			bool found = false;

			/* Check another workspace isn't using this tool. */
			for (wmWindow *win = wm->windows.first; win; win = win->next) {
				const WorkSpace *workspace_iter = WM_window_get_active_workspace(win);
				if (workspace != workspace_iter) {
					if (STREQ(workspace->tool.manipulator_group, workspace_iter->tool.manipulator_group)) {
						found = true;
						break;
					}
				}
			}

			if (!found) {
				wmManipulatorMapType *mmap_type = WM_manipulatormaptype_ensure(&wgt->mmap_params);
				WM_manipulatormaptype_group_unlink(C, bmain, mmap_type, wgt);
			}
		}
	}
}

void WM_toolsystem_link(bContext *UNUSED(C), WorkSpace *workspace)
{
	if (workspace->tool.manipulator_group[0]) {
		WM_manipulator_group_type_ensure(workspace->tool.manipulator_group);
	}
}

void WM_toolsystem_set(bContext *C, const bToolDef *tool)
{
	WorkSpace *workspace = CTX_wm_workspace(C);

	WM_toolsystem_unlink(C, workspace);

	workspace->tool.index = tool->index;
	workspace->tool.spacetype = tool->spacetype;

	if (&workspace->tool != tool) {
		BLI_strncpy(workspace->tool.keymap, tool->keymap, sizeof(tool->keymap));
		BLI_strncpy(workspace->tool.manipulator_group, tool->manipulator_group, sizeof(tool->manipulator_group));
		workspace->tool.spacetype = tool->spacetype;
	}

	WM_toolsystem_link(C, workspace);

	{
		struct wmMsgBus *mbus = CTX_wm_message_bus(C);
		WM_msg_publish_rna_prop(
		        mbus, &workspace->id, workspace, WorkSpace, tool_keymap);
	}
}

void WM_toolsystem_init(bContext *C)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = bmain->wm.first;

	for (wmWindow *win = wm->windows.first; win; win = win->next) {
		WorkSpace *workspace = WM_window_get_active_workspace(win);
		WM_toolsystem_link(C, workspace);
	}
}
