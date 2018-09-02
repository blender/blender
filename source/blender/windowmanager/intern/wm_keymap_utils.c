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

/** \file blender/windowmanager/intern/wm_keymap_utils.c
 *  \ingroup wm
 *
 * Utilities to help define keymaps.
 */

#include <string.h>

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* menu wrapper for WM_keymap_add_item */

/* -------------------------------------------------------------------- */
/** \name Wrappers for #WM_keymap_add_item
 * \{ */

wmKeyMapItem *WM_keymap_add_menu(wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi = WM_keymap_add_item(keymap, "WM_OT_call_menu", type, val, modifier, keymodifier);
	RNA_string_set(kmi->ptr, "name", idname);
	return kmi;
}

wmKeyMapItem *WM_keymap_add_menu_pie(wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi = WM_keymap_add_item(keymap, "WM_OT_call_menu_pie", type, val, modifier, keymodifier);
	RNA_string_set(kmi->ptr, "name", idname);
	return kmi;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Introspection
 * \{ */

/* Guess an appropriate keymap from the operator name */
/* Needs to be kept up to date with Keymap and Operator naming */
wmKeyMap *WM_keymap_guess_opname(const bContext *C, const char *opname)
{
	/* Op types purposely skipped  for now:
	 *     BRUSH_OT
	 *     BOID_OT
	 *     BUTTONS_OT
	 *     CONSTRAINT_OT
	 *     PAINT_OT
	 *     ED_OT
	 *     FLUID_OT
	 *     TEXTURE_OT
	 *     UI_OT
	 *     VIEW2D_OT
	 *     WORLD_OT
	 */

	wmKeyMap *km = NULL;
	SpaceLink *sl = CTX_wm_space_data(C);

	/* Window */
	if (STRPREFIX(opname, "WM_OT")) {
		km = WM_keymap_find_all(C, "Window", 0, 0);
	}
	/* Screen & Render */
	else if (STRPREFIX(opname, "SCREEN_OT") ||
	         STRPREFIX(opname, "RENDER_OT") ||
	         STRPREFIX(opname, "SOUND_OT") ||
	         STRPREFIX(opname, "SCENE_OT"))
	{
		km = WM_keymap_find_all(C, "Screen", 0, 0);
	}
	/* Grease Pencil */
	else if (STRPREFIX(opname, "GPENCIL_OT")) {
		km = WM_keymap_find_all(C, "Grease Pencil", 0, 0);
	}
	/* Markers */
	else if (STRPREFIX(opname, "MARKER_OT")) {
		km = WM_keymap_find_all(C, "Markers", 0, 0);
	}
	/* Import/Export*/
	else if (STRPREFIX(opname, "IMPORT_") ||
	         STRPREFIX(opname, "EXPORT_"))
	{
		km = WM_keymap_find_all(C, "Window", 0, 0);
	}


	/* 3D View */
	else if (STRPREFIX(opname, "VIEW3D_OT")) {
		km = WM_keymap_find_all(C, "3D View", sl->spacetype, 0);
	}
	else if (STRPREFIX(opname, "OBJECT_OT")) {
		/* exception, this needs to work outside object mode too */
		if (STRPREFIX(opname, "OBJECT_OT_mode_set"))
			km = WM_keymap_find_all(C, "Object Non-modal", 0, 0);
		else
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
	}
	/* Object mode related */
	else if (STRPREFIX(opname, "GROUP_OT") ||
	         STRPREFIX(opname, "MATERIAL_OT") ||
	         STRPREFIX(opname, "PTCACHE_OT") ||
	         STRPREFIX(opname, "RIGIDBODY_OT"))
	{
		km = WM_keymap_find_all(C, "Object Mode", 0, 0);
	}

	/* Editing Modes */
	else if (STRPREFIX(opname, "MESH_OT")) {
		km = WM_keymap_find_all(C, "Mesh", 0, 0);

		/* some mesh operators are active in object mode too, like add-prim */
		if (km && !WM_keymap_poll((bContext *)C, km)) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (STRPREFIX(opname, "CURVE_OT") ||
	         STRPREFIX(opname, "SURFACE_OT"))
	{
		km = WM_keymap_find_all(C, "Curve", 0, 0);

		/* some curve operators are active in object mode too, like add-prim */
		if (km && !WM_keymap_poll((bContext *)C, km)) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (STRPREFIX(opname, "ARMATURE_OT") ||
	         STRPREFIX(opname, "SKETCH_OT"))
	{
		km = WM_keymap_find_all(C, "Armature", 0, 0);
	}
	else if (STRPREFIX(opname, "POSE_OT") ||
	         STRPREFIX(opname, "POSELIB_OT"))
	{
		km = WM_keymap_find_all(C, "Pose", 0, 0);
	}
	else if (STRPREFIX(opname, "SCULPT_OT")) {
		switch (CTX_data_mode_enum(C)) {
			case OB_MODE_SCULPT:
				km = WM_keymap_find_all(C, "Sculpt", 0, 0);
				break;
			case OB_MODE_EDIT:
				km = WM_keymap_find_all(C, "UV Sculpt", 0, 0);
				break;
		}
	}
	else if (STRPREFIX(opname, "MBALL_OT")) {
		km = WM_keymap_find_all(C, "Metaball", 0, 0);

		/* some mball operators are active in object mode too, like add-prim */
		if (km && !WM_keymap_poll((bContext *)C, km)) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (STRPREFIX(opname, "LATTICE_OT")) {
		km = WM_keymap_find_all(C, "Lattice", 0, 0);
	}
	else if (STRPREFIX(opname, "PARTICLE_OT")) {
		km = WM_keymap_find_all(C, "Particle", 0, 0);
	}
	else if (STRPREFIX(opname, "FONT_OT")) {
		km = WM_keymap_find_all(C, "Font", 0, 0);
	}
	/* Paint Face Mask */
	else if (STRPREFIX(opname, "PAINT_OT_face_select")) {
		km = WM_keymap_find_all(C, "Face Mask", 0, 0);
	}
	else if (STRPREFIX(opname, "PAINT_OT")) {
		/* check for relevant mode */
		switch (CTX_data_mode_enum(C)) {
			case OB_MODE_WEIGHT_PAINT:
				km = WM_keymap_find_all(C, "Weight Paint", 0, 0);
				break;
			case OB_MODE_VERTEX_PAINT:
				km = WM_keymap_find_all(C, "Vertex Paint", 0, 0);
				break;
			case OB_MODE_TEXTURE_PAINT:
				km = WM_keymap_find_all(C, "Image Paint", 0, 0);
				break;
		}
	}
	/* Timeline */
	else if (STRPREFIX(opname, "TIME_OT")) {
		km = WM_keymap_find_all(C, "Timeline", sl->spacetype, 0);
	}
	/* Image Editor */
	else if (STRPREFIX(opname, "IMAGE_OT")) {
		km = WM_keymap_find_all(C, "Image", sl->spacetype, 0);
	}
	/* Clip Editor */
	else if (STRPREFIX(opname, "CLIP_OT")) {
		km = WM_keymap_find_all(C, "Clip", sl->spacetype, 0);
	}
	else if (STRPREFIX(opname, "MASK_OT")) {
		km = WM_keymap_find_all(C, "Mask Editing", 0, 0);
	}
	/* UV Editor */
	else if (STRPREFIX(opname, "UV_OT")) {
		/* Hack to allow using UV unwrapping ops from 3DView/editmode.
		 * Mesh keymap is probably not ideal, but best place I could find to put those. */
		if (sl->spacetype == SPACE_VIEW3D) {
			km = WM_keymap_find_all(C, "Mesh", 0, 0);
			if (km && !WM_keymap_poll((bContext *)C, km)) {
				km = NULL;
			}
		}
		if (!km) {
			km = WM_keymap_find_all(C, "UV Editor", 0, 0);
		}
	}
	/* Node Editor */
	else if (STRPREFIX(opname, "NODE_OT")) {
		km = WM_keymap_find_all(C, "Node Editor", sl->spacetype, 0);
	}
	/* Animation Editor Channels */
	else if (STRPREFIX(opname, "ANIM_OT_channels")) {
		km = WM_keymap_find_all(C, "Animation Channels", 0, 0);
	}
	/* Animation Generic - after channels */
	else if (STRPREFIX(opname, "ANIM_OT")) {
		km = WM_keymap_find_all(C, "Animation", 0, 0);
	}
	/* Graph Editor */
	else if (STRPREFIX(opname, "GRAPH_OT")) {
		km = WM_keymap_find_all(C, "Graph Editor", sl->spacetype, 0);
	}
	/* Dopesheet Editor */
	else if (STRPREFIX(opname, "ACTION_OT")) {
		km = WM_keymap_find_all(C, "Dopesheet", sl->spacetype, 0);
	}
	/* NLA Editor */
	else if (STRPREFIX(opname, "NLA_OT")) {
		km = WM_keymap_find_all(C, "NLA Editor", sl->spacetype, 0);
	}
	/* Script */
	else if (STRPREFIX(opname, "SCRIPT_OT")) {
		km = WM_keymap_find_all(C, "Script", sl->spacetype, 0);
	}
	/* Text */
	else if (STRPREFIX(opname, "TEXT_OT")) {
		km = WM_keymap_find_all(C, "Text", sl->spacetype, 0);
	}
	/* Sequencer */
	else if (STRPREFIX(opname, "SEQUENCER_OT")) {
		km = WM_keymap_find_all(C, "Sequencer", sl->spacetype, 0);
	}
	/* Console */
	else if (STRPREFIX(opname, "CONSOLE_OT")) {
		km = WM_keymap_find_all(C, "Console", sl->spacetype, 0);
	}
	/* Console */
	else if (STRPREFIX(opname, "INFO_OT")) {
		km = WM_keymap_find_all(C, "Info", sl->spacetype, 0);
	}
	/* File browser */
	else if (STRPREFIX(opname, "FILE_OT")) {
		km = WM_keymap_find_all(C, "File Browser", sl->spacetype, 0);
	}
	/* Logic Editor */
	else if (STRPREFIX(opname, "LOGIC_OT")) {
		km = WM_keymap_find_all(C, "Logic Editor", sl->spacetype, 0);
	}
	/* Outliner */
	else if (STRPREFIX(opname, "OUTLINER_OT")) {
		km = WM_keymap_find_all(C, "Outliner", sl->spacetype, 0);
	}
	/* Transform */
	else if (STRPREFIX(opname, "TRANSFORM_OT")) {
		/* check for relevant editor */
		switch (sl->spacetype) {
			case SPACE_VIEW3D:
				km = WM_keymap_find_all(C, "3D View", sl->spacetype, 0);
				break;
			case SPACE_IPO:
				km = WM_keymap_find_all(C, "Graph Editor", sl->spacetype, 0);
				break;
			case SPACE_ACTION:
				km = WM_keymap_find_all(C, "Dopesheet", sl->spacetype, 0);
				break;
			case SPACE_NLA:
				km = WM_keymap_find_all(C, "NLA Editor", sl->spacetype, 0);
				break;
			case SPACE_IMAGE:
				km = WM_keymap_find_all(C, "UV Editor", 0, 0);
				break;
			case SPACE_NODE:
				km = WM_keymap_find_all(C, "Node Editor", sl->spacetype, 0);
				break;
			case SPACE_SEQ:
				km = WM_keymap_find_all(C, "Sequencer", sl->spacetype, 0);
				break;
		}
	}

	return km;
}

/** \} */
