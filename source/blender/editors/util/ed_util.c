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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/util/ed_util.c
 *  \ingroup edutil
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_packedFile_types.h"

#include "BLI_blenlib.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_multires.h"
#include "BKE_packedFile.h"
#include "BKE_paint.h"

#include "ED_armature.h"
#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_types.h"
#include "WM_api.h"
#include "RNA_access.h"



/* ********* general editor util funcs, not BKE stuff please! ********* */

void ED_editors_init(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	Main *bmain = CTX_data_main(C);
	Scene *sce = CTX_data_scene(C);
	Object *ob, *obact = (sce && sce->basact) ? sce->basact->object : NULL;
	ID *data;

	/* toggle on modes for objects that were saved with these enabled. for
	 * e.g. linked objects we have to ensure that they are actually the
	 * active object in this scene. */
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		int mode = ob->mode;

		if (mode && (mode != OB_MODE_POSE)) {
			ob->mode = 0;
			data = ob->data;

			if (ob == obact && !ob->id.lib && !(data && data->lib))
				ED_object_toggle_modes(C, mode);
		}
	}

	/* image editor paint mode */
	if (sce) {
		ED_space_image_paint_update(wm, sce->toolsettings);
	}
}

/* frees all editmode stuff */
void ED_editors_exit(bContext *C)
{
	Main *bmain = CTX_data_main(C);
	Scene *sce;

	if (!bmain)
		return;
	
	/* frees all editmode undos */
	undo_editmode_clear();
	ED_undo_paint_free();
	
	for (sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->obedit) {
			Object *ob = sce->obedit;
		
			if (ob) {
				if (ob->type == OB_MESH) {
					Mesh *me = ob->data;
					if (me->edit_btmesh) {
						EDBM_mesh_free(me->edit_btmesh);
						MEM_freeN(me->edit_btmesh);
						me->edit_btmesh = NULL;
					}
				}
				else if (ob->type == OB_ARMATURE) {
					ED_armature_edit_free(ob->data);
				}
			}
		}
	}

	/* global in meshtools... */
	mesh_octree_table(NULL, NULL, NULL, 'e');
	mesh_mirrtopo_table(NULL, 'e');
}

/* flush any temp data from object editing to DNA before writing files,
 * rendering, copying, etc. */
void ED_editors_flush_edits(const bContext *C, bool for_render)
{
	Object *obact = CTX_data_active_object(C);
	Object *obedit = CTX_data_edit_object(C);

	/* get editmode results */
	if (obedit)
		ED_object_editmode_load(obedit);

	if (obact && (obact->mode & OB_MODE_SCULPT)) {
		/* flush multires changes (for sculpt) */
		multires_force_update(obact);

		if (for_render) {
			/* flush changes from dynamic topology sculpt */
			sculptsession_bm_to_me_for_render(obact);
		}
		else {
			/* Set reorder=false so that saving the file doesn't reorder
			 * the BMesh's elements */
			sculptsession_bm_to_me(obact, FALSE);
		}
	}
}

/* ***** XXX: functions are using old blender names, cleanup later ***** */


/* now only used in 2d spaces, like time, ipo, nla, sima... */
/* XXX shift/ctrl not configurable */
void apply_keyb_grid(int shift, int ctrl, float *val, float fac1, float fac2, float fac3, int invert)
{
	/* fac1 is for 'nothing', fac2 for CTRL, fac3 for SHIFT */
	if (invert)
		ctrl = !ctrl;
	
	if (ctrl && shift) {
		if (fac3 != 0.0f) *val = fac3 * floorf(*val / fac3 + 0.5f);
	}
	else if (ctrl) {
		if (fac2 != 0.0f) *val = fac2 * floorf(*val / fac2 + 0.5f);
	}
	else {
		if (fac1 != 0.0f) *val = fac1 * floorf(*val / fac1 + 0.5f);
	}
}

void unpack_menu(bContext *C, const char *opname, const char *id_name, const char *abs_name, const char *folder, struct PackedFile *pf)
{
	PointerRNA props_ptr;
	uiPopupMenu *pup;
	uiLayout *layout;
	char line[FILE_MAX + 100];
	wmOperatorType *ot = WM_operatortype_find(opname, 1);

	pup = uiPupMenuBegin(C, IFACE_("Unpack File"), ICON_NONE);
	layout = uiPupMenuLayout(pup);

	props_ptr = uiItemFullO_ptr(layout, ot, IFACE_("Remove Pack"), ICON_NONE,
	                            NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&props_ptr, "method", PF_REMOVE);
	RNA_string_set(&props_ptr, "id", id_name);

	if (G.relbase_valid) {
		char local_name[FILE_MAXDIR + FILE_MAX], fi[FILE_MAX];

		BLI_split_file_part(abs_name, fi, sizeof(fi));
		BLI_snprintf(local_name, sizeof(local_name), "//%s/%s", folder, fi);
		if (strcmp(abs_name, local_name) != 0) {
			switch (checkPackedFile(local_name, pf)) {
				case PF_NOFILE:
					BLI_snprintf(line, sizeof(line), IFACE_("Create %s"), local_name);
					props_ptr = uiItemFullO_ptr(layout, ot, line, ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
					RNA_enum_set(&props_ptr, "method", PF_WRITE_LOCAL);
					RNA_string_set(&props_ptr, "id", id_name);

					break;
				case PF_EQUAL:
					BLI_snprintf(line, sizeof(line), IFACE_("Use %s (identical)"), local_name);
					//uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_USE_LOCAL);
					props_ptr = uiItemFullO_ptr(layout, ot, line, ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
					RNA_enum_set(&props_ptr, "method", PF_USE_LOCAL);
					RNA_string_set(&props_ptr, "id", id_name);

					break;
				case PF_DIFFERS:
					BLI_snprintf(line, sizeof(line), IFACE_("Use %s (differs)"), local_name);
					//uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_USE_LOCAL);
					props_ptr = uiItemFullO_ptr(layout, ot, line, ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
					RNA_enum_set(&props_ptr, "method", PF_USE_LOCAL);
					RNA_string_set(&props_ptr, "id", id_name);

					BLI_snprintf(line, sizeof(line), IFACE_("Overwrite %s"), local_name);
					//uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_WRITE_LOCAL);
					props_ptr = uiItemFullO_ptr(layout, ot, line, ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
					RNA_enum_set(&props_ptr, "method", PF_WRITE_LOCAL);
					RNA_string_set(&props_ptr, "id", id_name);
					break;
			}
		}
	}

	switch (checkPackedFile(abs_name, pf)) {
		case PF_NOFILE:
			BLI_snprintf(line, sizeof(line), IFACE_("Create %s"), abs_name);
			//uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_WRITE_ORIGINAL);
			props_ptr = uiItemFullO_ptr(layout, ot, line, ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
			RNA_enum_set(&props_ptr, "method", PF_WRITE_ORIGINAL);
			RNA_string_set(&props_ptr, "id", id_name);
			break;
		case PF_EQUAL:
			BLI_snprintf(line, sizeof(line), IFACE_("Use %s (identical)"), abs_name);
			//uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_USE_ORIGINAL);
			props_ptr = uiItemFullO_ptr(layout, ot, line, ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
			RNA_enum_set(&props_ptr, "method", PF_USE_ORIGINAL);
			RNA_string_set(&props_ptr, "id", id_name);
			break;
		case PF_DIFFERS:
			BLI_snprintf(line, sizeof(line), IFACE_("Use %s (differs)"), abs_name);
			//uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_USE_ORIGINAL);
			props_ptr = uiItemFullO_ptr(layout, ot, line, ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
			RNA_enum_set(&props_ptr, "method", PF_USE_ORIGINAL);
			RNA_string_set(&props_ptr, "id", id_name);

			BLI_snprintf(line, sizeof(line), IFACE_("Overwrite %s"), abs_name);
			//uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_WRITE_ORIGINAL);
			props_ptr = uiItemFullO_ptr(layout, ot, line, ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
			RNA_enum_set(&props_ptr, "method", PF_WRITE_ORIGINAL);
			RNA_string_set(&props_ptr, "id", id_name);
			break;
	}

	uiPupMenuEnd(C, pup);
}

/* ********************* generic callbacks for drawcall api *********************** */

/**
 * Callback that draws a line between the mouse and a position given as the initial argument.
 */
void ED_region_draw_mouse_line_cb(const bContext *C, ARegion *ar, void *arg_info)
{
	wmWindow *win = CTX_wm_window(C);
	const float *mval_src = (float *)arg_info;
	const int mval_dst[2] = {win->eventstate->x - ar->winrct.xmin,
	                         win->eventstate->y - ar->winrct.ymin};

	UI_ThemeColor(TH_WIRE);
	setlinestyle(3);
	glBegin(GL_LINE_STRIP);
	glVertex2iv(mval_dst);
	glVertex2fv(mval_src);
	glEnd();
	setlinestyle(0);
}
