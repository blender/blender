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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/mesh_ops.c
 *  \ingroup edmesh
 */


#include <stdlib.h>
#include <math.h>


#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"


#include "mesh_intern.h"

/**************************** registration **********************************/

void ED_operatortypes_mesh(void)
{
	WM_operatortype_append(MESH_OT_select_all);
	WM_operatortype_append(MESH_OT_select_interior_faces);
	WM_operatortype_append(MESH_OT_select_more);
	WM_operatortype_append(MESH_OT_select_less);
	WM_operatortype_append(MESH_OT_select_non_manifold);
	WM_operatortype_append(MESH_OT_select_linked);
	WM_operatortype_append(MESH_OT_select_linked_pick);
	WM_operatortype_append(MESH_OT_select_random);
	WM_operatortype_append(MESH_OT_hide);
	WM_operatortype_append(MESH_OT_reveal);
	WM_operatortype_append(MESH_OT_select_by_number_vertices);
	WM_operatortype_append(MESH_OT_select_loose_verts);
	WM_operatortype_append(MESH_OT_select_mirror);
	WM_operatortype_append(MESH_OT_normals_make_consistent);
	WM_operatortype_append(MESH_OT_merge);
	WM_operatortype_append(MESH_OT_subdivide);
	WM_operatortype_append(MESH_OT_faces_select_linked_flat);
	WM_operatortype_append(MESH_OT_edges_select_sharp);
	WM_operatortype_append(MESH_OT_primitive_plane_add);
	WM_operatortype_append(MESH_OT_primitive_cube_add);
	WM_operatortype_append(MESH_OT_primitive_circle_add);
	WM_operatortype_append(MESH_OT_primitive_cylinder_add);
	WM_operatortype_append(MESH_OT_primitive_cone_add);
	WM_operatortype_append(MESH_OT_primitive_grid_add);
	WM_operatortype_append(MESH_OT_primitive_monkey_add);
	WM_operatortype_append(MESH_OT_primitive_uv_sphere_add);
	WM_operatortype_append(MESH_OT_primitive_ico_sphere_add);
	WM_operatortype_append(MESH_OT_duplicate);
	WM_operatortype_append(MESH_OT_remove_doubles);
	WM_operatortype_append(MESH_OT_vertices_sort);
	WM_operatortype_append(MESH_OT_vertices_randomize);
	WM_operatortype_append(MESH_OT_spin);
	WM_operatortype_append(MESH_OT_screw);

	WM_operatortype_append(MESH_OT_extrude_region);
	WM_operatortype_append(MESH_OT_extrude_faces_indiv);
	WM_operatortype_append(MESH_OT_extrude_edges_indiv);
	WM_operatortype_append(MESH_OT_extrude_verts_indiv);

	WM_operatortype_append(MESH_OT_split);
	WM_operatortype_append(MESH_OT_extrude_repeat);
	WM_operatortype_append(MESH_OT_edge_rotate);
	WM_operatortype_append(MESH_OT_select_vertex_path);
	WM_operatortype_append(MESH_OT_loop_to_region);
	WM_operatortype_append(MESH_OT_region_to_loop);
	WM_operatortype_append(MESH_OT_select_axis);
	
	WM_operatortype_append(MESH_OT_uvs_rotate);
	WM_operatortype_append(MESH_OT_uvs_reverse);
	WM_operatortype_append(MESH_OT_colors_rotate);
	WM_operatortype_append(MESH_OT_colors_reverse);
	
	WM_operatortype_append(MESH_OT_fill);
	WM_operatortype_append(MESH_OT_beautify_fill);
	WM_operatortype_append(MESH_OT_quads_convert_to_tris);
	WM_operatortype_append(MESH_OT_tris_convert_to_quads);
	WM_operatortype_append(MESH_OT_dissolve_limited);
	WM_operatortype_append(MESH_OT_faces_shade_smooth);
	WM_operatortype_append(MESH_OT_faces_shade_flat);
	WM_operatortype_append(MESH_OT_sort_faces);

	WM_operatortype_append(MESH_OT_delete);

	WM_operatortype_append(MESH_OT_separate);
	WM_operatortype_append(MESH_OT_dupli_extrude_cursor);
	WM_operatortype_append(MESH_OT_loop_select);
	WM_operatortype_append(MESH_OT_edge_face_add);
	WM_operatortype_append(MESH_OT_select_shortest_path);
	WM_operatortype_append(MESH_OT_select_similar);
	WM_operatortype_append(MESH_OT_loop_multi_select);
	WM_operatortype_append(MESH_OT_mark_seam);
	WM_operatortype_append(MESH_OT_mark_sharp);
	WM_operatortype_append(MESH_OT_vertices_smooth);
	WM_operatortype_append(MESH_OT_noise);
	WM_operatortype_append(MESH_OT_flip_normals);
	//WM_operatortype_append(MESH_OT_knife_cut);
	WM_operatortype_append(MESH_OT_rip);
	WM_operatortype_append(MESH_OT_blend_from_shape);
	WM_operatortype_append(MESH_OT_shape_propagate_to_all);
	
	WM_operatortype_append(MESH_OT_uv_texture_add);
	WM_operatortype_append(MESH_OT_uv_texture_remove);
	WM_operatortype_append(MESH_OT_vertex_color_add);
	WM_operatortype_append(MESH_OT_vertex_color_remove);
	WM_operatortype_append(MESH_OT_sticky_add);
	WM_operatortype_append(MESH_OT_sticky_remove);
	WM_operatortype_append(MESH_OT_drop_named_image);

	WM_operatortype_append(MESH_OT_edgering_select);
	WM_operatortype_append(MESH_OT_loopcut);

	WM_operatortype_append(MESH_OT_solidify);
	WM_operatortype_append(MESH_OT_select_nth);
	WM_operatortype_append(MESH_OT_vert_connect);
	WM_operatortype_append(MESH_OT_knifetool);

	WM_operatortype_append(MESH_OT_bevel);

	WM_operatortype_append(MESH_OT_select_next_loop);

	WM_operatortype_append(MESH_OT_bridge_edge_loops);

#ifdef WITH_GAMEENGINE
	WM_operatortype_append(MESH_OT_navmesh_make);
	WM_operatortype_append(MESH_OT_navmesh_face_copy);
	WM_operatortype_append(MESH_OT_navmesh_face_add);
	WM_operatortype_append(MESH_OT_navmesh_reset);
	WM_operatortype_append(MESH_OT_navmesh_clear);
#endif
}

#if 0 /* UNUSED, remove? */
static int ED_operator_editmesh_face_select(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_MESH) {
		BMEditMesh *em = BMEdit_FromObject(obedit);
		if (em && em->selectmode & SCE_SELECT_FACE) {
			return 1;
		}
	}
	return 0;
}
#endif

void ED_operatormacros_mesh(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;
	
	ot= WM_operatortype_append_macro("MESH_OT_loopcut_slide", "Loop Cut and Slide", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Cut mesh loop and slide it";
	WM_operatortype_macro_define(ot, "MESH_OT_loopcut");
	otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_edge_slide");
	RNA_struct_idprops_unset(otmacro->ptr, "release_confirm");

	ot= WM_operatortype_append_macro("MESH_OT_duplicate_move", "Add Duplicate", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Duplicate mesh and move";
	WM_operatortype_macro_define(ot, "MESH_OT_duplicate");
	otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", FALSE);

	ot= WM_operatortype_append_macro("MESH_OT_rip_move", "Rip", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Rip polygons and move the result";
	WM_operatortype_macro_define(ot, "MESH_OT_rip");
	otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", FALSE);

	ot= WM_operatortype_append_macro("MESH_OT_extrude_region_move", "Extrude Region and Move", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Extrude region and move result";
	otmacro= WM_operatortype_macro_define(ot, "MESH_OT_extrude_region");
	otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", FALSE);

	ot= WM_operatortype_append_macro("MESH_OT_extrude_faces_move", "Extrude Individual Faces and Move", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Extrude faces and move result";
	otmacro= WM_operatortype_macro_define(ot, "MESH_OT_extrude_faces_indiv");
	otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_shrink_fatten");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", FALSE);

	ot= WM_operatortype_append_macro("MESH_OT_extrude_edges_move", "Extrude Only Edges and Move", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Extrude edges and move result";
	otmacro= WM_operatortype_macro_define(ot, "MESH_OT_extrude_edges_indiv");
	otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", FALSE);

	ot= WM_operatortype_append_macro("MESH_OT_extrude_vertices_move", "Extrude Only Vertices and Move", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Extrude vertices and move result";
	otmacro= WM_operatortype_macro_define(ot, "MESH_OT_extrude_verts_indiv");
	otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", FALSE);
}

/* note mesh keymap also for other space? */
void ED_keymap_mesh(wmKeyConfig *keyconf)
{	
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	int i;
	
	keymap= WM_keymap_find(keyconf, "Mesh", 0, 0);
	keymap->poll= ED_operator_editmesh;
	
	WM_keymap_add_item(keymap, "MESH_OT_loopcut_slide", RKEY, KM_PRESS, KM_CTRL, 0);

	/* selecting */
	/* standard mouse selection goes via space_view3d */
	kmi = WM_keymap_add_item(keymap, "MESH_OT_loop_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_loop_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);

	kmi = WM_keymap_add_item(keymap, "MESH_OT_edgering_select", SELECTMOUSE, KM_PRESS, KM_ALT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_edgering_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);

	WM_keymap_add_item(keymap, "MESH_OT_select_shortest_path", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "MESH_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "MESH_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_non_manifold", MKEY, KM_PRESS, (KM_CTRL|KM_SHIFT|KM_ALT), 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "deselect", FALSE);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", TRUE);
	
	WM_keymap_add_item(keymap, "MESH_OT_faces_select_linked_flat", FKEY, KM_PRESS, (KM_CTRL|KM_SHIFT|KM_ALT), 0);

	WM_keymap_add_item(keymap, "MESH_OT_select_similar", GKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* selection mode */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_select_mode", TABKEY, KM_PRESS, KM_CTRL, 0);
	
	/* hide */
	kmi = WM_keymap_add_item(keymap, "MESH_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", FALSE);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", TRUE);
	WM_keymap_add_item(keymap, "MESH_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);

	/* tools */
	kmi = WM_keymap_add_item(keymap, "MESH_OT_normals_make_consistent", NKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "inside", FALSE);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_normals_make_consistent", NKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "inside", TRUE);
	
	WM_keymap_add_item(keymap, "VIEW3D_OT_edit_mesh_extrude_move_normal", EKEY, KM_PRESS, 0, 0); /* python operator */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_extrude", EKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "TRANSFORM_OT_edge_crease", EKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_spin", RKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_fill", FKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_beautify_fill", FKEY, KM_PRESS, KM_SHIFT|KM_ALT, 0);

	WM_keymap_add_item(keymap, "MESH_OT_quads_convert_to_tris", TKEY, KM_PRESS, KM_CTRL, 0);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_quads_convert_to_tris", TKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "use_beauty", FALSE);

	WM_keymap_add_item(keymap, "MESH_OT_tris_convert_to_quads", JKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "MESH_OT_rip_move",VKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_merge", MKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "TRANSFORM_OT_shrink_fatten", SKEY, KM_PRESS, KM_ALT, 0);

	/* add/remove */
	WM_keymap_add_item(keymap, "MESH_OT_edge_face_add", FKEY, KM_PRESS, 0, 0);
//	WM_keymap_add_item(keymap, "MESH_OT_skin", FKEY, KM_PRESS, KM_CTRL|KM_ALT, 0); /* python, removed */
	WM_keymap_add_item(keymap, "MESH_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_menu(keymap, "INFO_MT_mesh_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_separate", PKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_split", YKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_vert_connect", JKEY, KM_PRESS, 0, 0);

	/* use KM_CLICK because same key is used for tweaks */
	kmi = WM_keymap_add_item(keymap, "MESH_OT_dupli_extrude_cursor", ACTIONMOUSE, KM_CLICK, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "rotate_source", TRUE);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_dupli_extrude_cursor", ACTIONMOUSE, KM_CLICK, KM_SHIFT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "rotate_source", FALSE);

	WM_keymap_add_item(keymap, "MESH_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_delete", DELKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_knifetool", KKEY, KM_PRESS, 0, 0);
	//RNA_enum_set(WM_keymap_add_item(keymap, "MESH_OT_knife_cut", LEFTMOUSE, KM_PRESS, KM_SHIFT, KKEY)->ptr, "type", 2/*KNIFE_MIDPOINT*/);
	
	WM_keymap_add_item(keymap, "OBJECT_OT_vertex_parent_set", PKEY, KM_PRESS, KM_CTRL, 0);

	/* menus */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_specials", WKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_faces", FKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_edges", EKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_vertices", VKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_hook", HKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_uv_map", UKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_vertex_group", GKEY, KM_PRESS, KM_CTRL, 0);
	
	/* useful stuff from object-mode */
	for (i=0; i<=5; i++) {
		kmi = WM_keymap_add_item(keymap, "OBJECT_OT_subdivision_set", ZEROKEY+i, KM_PRESS, KM_CTRL, 0);
		RNA_int_set(kmi->ptr, "level", i);
	}
	
	ED_object_generic_keymap(keyconf, keymap, 3);
	knifetool_modal_keymap(keyconf);
}

