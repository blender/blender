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
#include "BKE_paint.h"
#include "BKE_workspace.h"

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

void WM_toolsystem_link(bContext *C, WorkSpace *workspace)
{
	if (workspace->tool.manipulator_group[0]) {
		WM_manipulator_group_type_ensure(workspace->tool.manipulator_group);
	}

	if (workspace->tool.data_block[0]) {
		Main *bmain = CTX_data_main(C);

		/* Currently only brush data-blocks supported. */
		struct Brush *brush = (struct Brush *)BKE_libblock_find_name(ID_BR, workspace->tool.data_block);

		if (brush) {
			wmWindowManager *wm = bmain->wm.first;
			for (wmWindow *win = wm->windows.first; win; win = win->next) {
				if (workspace == WM_window_get_active_workspace(win)) {
					Scene *scene = win->scene;
					ViewLayer *view_layer = BKE_workspace_view_layer_get(workspace, scene);
					Paint *paint = BKE_paint_get_active(scene, view_layer);
					if (paint) {
						if (brush) {
							BKE_paint_brush_set(paint, brush);
						}
					}
				}
			}
		}
	}
}

void WM_toolsystem_set(bContext *C, const bToolDef *tool)
{
	WorkSpace *workspace = CTX_wm_workspace(C);

	WM_toolsystem_unlink(C, workspace);

	workspace->tool.index = tool->index;
	workspace->tool.spacetype = tool->spacetype;

	if (&workspace->tool != tool) {
		STRNCPY(workspace->tool.keymap, tool->keymap);
		STRNCPY(workspace->tool.manipulator_group, tool->manipulator_group);
		STRNCPY(workspace->tool.data_block, tool->data_block);
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
