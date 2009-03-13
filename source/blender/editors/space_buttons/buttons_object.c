/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_group.h"
#include "BKE_main.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "WM_types.h"

static void object_panel_transform(const bContext *C, uiLayout *layout)
{
	Object *ob= CTX_data_active_object(C);
	PointerRNA obptr;

	RNA_id_pointer_create(&ob->id, &obptr);

	uiTemplateColumn(layout);
	uiItemR(layout, UI_TSLOT_COLUMN_1, NULL, 0, &obptr, "location");
	uiItemR(layout, UI_TSLOT_COLUMN_2, NULL, 0, &obptr, "rotation");
	uiItemR(layout, UI_TSLOT_COLUMN_3, NULL, 0, &obptr, "scale");
}

static void object_panel_groups(const bContext *C, uiLayout *layout)
{
	Main *bmain= CTX_data_main(C);
	Object *ob= CTX_data_active_object(C);
	Group *group;
	PointerRNA obptr, groupptr;
	uiLayout *sublayout;

	RNA_id_pointer_create(&ob->id, &obptr);

	uiTemplateColumn(layout);
	uiItemR(layout, UI_TSLOT_COLUMN_1, NULL, 0, &obptr, "pass_index");
	uiItemR(layout, UI_TSLOT_COLUMN_2, NULL, 0, &obptr, "parent");

	/* uiTemplateLeftRight(layout);
	uiItemO(layout, UI_TSLOT_LR_LEFT, NULL, 0, "OBJECT_OT_add_group"); */

	for(group=bmain->group.first; group; group=group->id.next) {
		if(object_in_group(ob, group)) {
			RNA_id_pointer_create(&group->id, &groupptr);

			sublayout= uiTemplateStack(layout);

			uiTemplateLeftRight(sublayout);
			uiItemR(sublayout, UI_TSLOT_LR_LEFT, NULL, 0, &groupptr, "name");
			// uiItemO(sublayout, UI_TSLOT_LR_RIGHT, "", ICON_X, "OBJECT_OT_remove_group");

			uiTemplateColumn(sublayout);
			uiItemR(sublayout, UI_TSLOT_COLUMN_1, NULL, 0, &groupptr, "layer");
			uiItemR(sublayout, UI_TSLOT_COLUMN_2, NULL, 0, &groupptr, "dupli_offset");
		}
	}
}

static void object_panel_display(const bContext *C, uiLayout *layout)
{
	Object *ob= CTX_data_active_object(C);
	PointerRNA obptr;

	RNA_id_pointer_create(&ob->id, &obptr);

	uiTemplateColumn(layout);
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Type", 0, &obptr, "max_draw_type");
	uiItemR(layout, UI_TSLOT_COLUMN_2, "Bounds", 0, &obptr, "draw_bounds_type");

	uiTemplateColumn(layout);
	uiItemLabel(layout, UI_TSLOT_COLUMN_1, "Extra", 0);
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Name", 0, &obptr, "draw_name");
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Axis", 0, &obptr, "draw_axis");
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Wire", 0, &obptr, "draw_wire");
	uiItemLabel(layout, UI_TSLOT_COLUMN_2, "", 0);
	uiItemR(layout, UI_TSLOT_COLUMN_2, "Texture Space", 0, &obptr, "draw_texture_space");
	uiItemR(layout, UI_TSLOT_COLUMN_2, "X-Ray", 0, &obptr, "x_ray");
	uiItemR(layout, UI_TSLOT_COLUMN_2, "Transparency", 0, &obptr, "draw_transparent");
}

static void object_panel_duplication(const bContext *C, uiLayout *layout)
{
	Object *ob= CTX_data_active_object(C);
	PointerRNA obptr;

	RNA_id_pointer_create(&ob->id, &obptr);

	uiTemplateColumn(layout);
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Frames", 0, &obptr, "dupli_frames");
	uiItemR(layout, UI_TSLOT_COLUMN_2, "Verts", 0, &obptr, "dupli_verts");
	uiItemR(layout, UI_TSLOT_COLUMN_3, "Faces", 0, &obptr, "dupli_faces");
	uiItemR(layout, UI_TSLOT_COLUMN_4, "Group", 0, &obptr, "use_dupli_group");

	if(RNA_boolean_get(&obptr, "dupli_frames")) {
		uiTemplateColumn(layout);
		uiItemR(layout, UI_TSLOT_COLUMN_1, "Start:", 0, &obptr, "dupli_frames_start");
		uiItemR(layout, UI_TSLOT_COLUMN_1, "End:", 0, &obptr, "dupli_frames_end");

		uiItemR(layout, UI_TSLOT_COLUMN_2, "On:", 0, &obptr, "dupli_frames_on");
		uiItemR(layout, UI_TSLOT_COLUMN_2, "Off:", 0, &obptr, "dupli_frames_off");
	}
}

static void object_panel_animation(const bContext *C, uiLayout *layout)
{
	Object *ob= CTX_data_active_object(C);
	PointerRNA obptr;

	RNA_id_pointer_create(&ob->id, &obptr);

	uiTemplateColumn(layout);
	uiItemLabel(layout, UI_TSLOT_COLUMN_1, "Time Offset:", 0);
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Edit", 0, &obptr, "time_offset_edit");
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Particle", 0, &obptr, "time_offset_particle");
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Parent", 0, &obptr, "time_offset_parent");
	uiItemR(layout, UI_TSLOT_COLUMN_1, NULL, 0, &obptr, "slow_parent");
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Offset: ", 0, &obptr, "time_offset");

	uiItemLabel(layout, UI_TSLOT_COLUMN_2, "Tracking:", 0);
	uiItemR(layout, UI_TSLOT_COLUMN_2, "Axis: ", 0, &obptr, "track_axis");
	uiItemR(layout, UI_TSLOT_COLUMN_2, "Up Axis: ", 0, &obptr, "up_axis");
	uiItemR(layout, UI_TSLOT_COLUMN_2, "Rotation", 0, &obptr, "track_rotation");
}

void buttons_object(const bContext *C, ARegion *ar)
{
	SpaceButs *sbuts= (SpaceButs*)CTX_wm_space_data(C);
	Object *ob= CTX_data_active_object(C);
	int tab= sbuts->tab[CONTEXT_OBJECT];

	if(tab == TAB_OBJECT_OBJECT) {
		if(!ob)
			return;

		uiPanelLayout(C, ar, "OBJECT_PT_transform", "Transform", "Object", object_panel_transform, 0);
		uiPanelLayout(C, ar, "OBJECT_PT_groups", "Groups", "Object", object_panel_groups, 1);
		uiPanelLayout(C, ar, "OBJECT_PT_display", "Display", "Object", object_panel_display, 2);
		uiPanelLayout(C, ar, "OBJECT_PT_duplication", "Duplication", "Object", object_panel_duplication, 3);
		uiPanelLayout(C, ar, "OBJECT_PT_animation", "Animation", "Object", object_panel_animation, 4);
	}
}

