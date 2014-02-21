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

#include "DNA_scene_types.h"
#include "DNA_modifier_types.h"

#include "BLI_math.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"

#include "mesh_intern.h"  /* own include */

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
	WM_operatortype_append(MESH_OT_select_ungrouped);
	WM_operatortype_append(MESH_OT_hide);
	WM_operatortype_append(MESH_OT_reveal);
	WM_operatortype_append(MESH_OT_select_face_by_sides);
	WM_operatortype_append(MESH_OT_select_loose);
	WM_operatortype_append(MESH_OT_select_mirror);
	WM_operatortype_append(MESH_OT_normals_make_consistent);
	WM_operatortype_append(MESH_OT_merge);
	WM_operatortype_append(MESH_OT_subdivide);
	WM_operatortype_append(MESH_OT_subdivide_edgering);
	WM_operatortype_append(MESH_OT_unsubdivide);
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
	WM_operatortype_append(MESH_OT_spin);
	WM_operatortype_append(MESH_OT_screw);

	WM_operatortype_append(MESH_OT_extrude_region);
	WM_operatortype_append(MESH_OT_extrude_faces_indiv);
	WM_operatortype_append(MESH_OT_extrude_edges_indiv);
	WM_operatortype_append(MESH_OT_extrude_verts_indiv);

	WM_operatortype_append(MESH_OT_split);
	WM_operatortype_append(MESH_OT_extrude_repeat);
	WM_operatortype_append(MESH_OT_edge_rotate);
	WM_operatortype_append(MESH_OT_shortest_path_select);
	WM_operatortype_append(MESH_OT_loop_to_region);
	WM_operatortype_append(MESH_OT_region_to_loop);
	WM_operatortype_append(MESH_OT_select_axis);
	
	WM_operatortype_append(MESH_OT_uvs_rotate);
	WM_operatortype_append(MESH_OT_uvs_reverse);
	WM_operatortype_append(MESH_OT_colors_rotate);
	WM_operatortype_append(MESH_OT_colors_reverse);
	
	WM_operatortype_append(MESH_OT_fill);
	WM_operatortype_append(MESH_OT_fill_grid);
	WM_operatortype_append(MESH_OT_fill_holes);
	WM_operatortype_append(MESH_OT_beautify_fill);
	WM_operatortype_append(MESH_OT_quads_convert_to_tris);
	WM_operatortype_append(MESH_OT_tris_convert_to_quads);
	WM_operatortype_append(MESH_OT_dissolve_verts);
	WM_operatortype_append(MESH_OT_dissolve_edges);
	WM_operatortype_append(MESH_OT_dissolve_faces);
	WM_operatortype_append(MESH_OT_dissolve_mode);
	WM_operatortype_append(MESH_OT_dissolve_limited);
	WM_operatortype_append(MESH_OT_dissolve_degenerate);
	WM_operatortype_append(MESH_OT_delete_edgeloop);
	WM_operatortype_append(MESH_OT_faces_shade_smooth);
	WM_operatortype_append(MESH_OT_faces_shade_flat);
	WM_operatortype_append(MESH_OT_sort_elements);
#ifdef WITH_FREESTYLE
	WM_operatortype_append(MESH_OT_mark_freestyle_face);
#endif

	WM_operatortype_append(MESH_OT_delete);
	WM_operatortype_append(MESH_OT_delete_loose);
	WM_operatortype_append(MESH_OT_edge_collapse);

	WM_operatortype_append(MESH_OT_separate);
	WM_operatortype_append(MESH_OT_dupli_extrude_cursor);
	WM_operatortype_append(MESH_OT_loop_select);
	WM_operatortype_append(MESH_OT_edge_face_add);
	WM_operatortype_append(MESH_OT_shortest_path_pick);
	WM_operatortype_append(MESH_OT_select_similar);
	WM_operatortype_append(MESH_OT_select_mode);
	WM_operatortype_append(MESH_OT_loop_multi_select);
	WM_operatortype_append(MESH_OT_mark_seam);
	WM_operatortype_append(MESH_OT_mark_sharp);
#ifdef WITH_FREESTYLE
	WM_operatortype_append(MESH_OT_mark_freestyle_edge);
#endif
	WM_operatortype_append(MESH_OT_vertices_smooth);
	WM_operatortype_append(MESH_OT_vertices_smooth_laplacian);
	WM_operatortype_append(MESH_OT_noise);
	WM_operatortype_append(MESH_OT_flip_normals);
	WM_operatortype_append(MESH_OT_rip);
	WM_operatortype_append(MESH_OT_blend_from_shape);
	WM_operatortype_append(MESH_OT_shape_propagate_to_all);
	
	WM_operatortype_append(MESH_OT_uv_texture_add);
	WM_operatortype_append(MESH_OT_uv_texture_remove);
	WM_operatortype_append(MESH_OT_vertex_color_add);
	WM_operatortype_append(MESH_OT_vertex_color_remove);
	WM_operatortype_append(MESH_OT_customdata_clear_mask);
	WM_operatortype_append(MESH_OT_customdata_clear_skin);
	WM_operatortype_append(MESH_OT_drop_named_image);

	WM_operatortype_append(MESH_OT_edgering_select);
	WM_operatortype_append(MESH_OT_loopcut);

	WM_operatortype_append(MESH_OT_solidify);
	WM_operatortype_append(MESH_OT_select_nth);
	WM_operatortype_append(MESH_OT_vert_connect);
	WM_operatortype_append(MESH_OT_vert_connect_nonplanar);
	WM_operatortype_append(MESH_OT_knife_tool);
	WM_operatortype_append(MESH_OT_knife_project);

	WM_operatortype_append(MESH_OT_bevel);

	WM_operatortype_append(MESH_OT_select_next_loop);

	WM_operatortype_append(MESH_OT_bridge_edge_loops);
	WM_operatortype_append(MESH_OT_inset);
	WM_operatortype_append(MESH_OT_poke);
	WM_operatortype_append(MESH_OT_wireframe);
	WM_operatortype_append(MESH_OT_edge_split);

#ifdef WITH_BULLET
	WM_operatortype_append(MESH_OT_convex_hull);
#endif

	WM_operatortype_append(MESH_OT_bisect);
	WM_operatortype_append(MESH_OT_symmetrize);
	WM_operatortype_append(MESH_OT_symmetry_snap);

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
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_MESH) {
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
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
	
	ot = WM_operatortype_append_macro("MESH_OT_loopcut_slide", "Loop Cut and Slide", "Cut mesh loop and slide it",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "MESH_OT_loopcut");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_edge_slide");
	RNA_boolean_set(otmacro->ptr, "release_confirm", false);

	ot = WM_operatortype_append_macro("MESH_OT_duplicate_move", "Add Duplicate", "Duplicate mesh and move",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "MESH_OT_duplicate");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);

	ot = WM_operatortype_append_macro("MESH_OT_rip_move", "Rip", "Rip polygons and move the result",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	otmacro = WM_operatortype_macro_define(ot, "MESH_OT_rip");
	RNA_boolean_set(otmacro->ptr, "use_fill", false);
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);

	/* annoying we can't pass 'use_fill' through the macro */
	ot = WM_operatortype_append_macro("MESH_OT_rip_move_fill", "Rip Fill", "Rip-fill polygons and move the result",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	otmacro = WM_operatortype_macro_define(ot, "MESH_OT_rip");
	RNA_boolean_set(otmacro->ptr, "use_fill", true);
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);

	ot = WM_operatortype_append_macro("MESH_OT_extrude_region_move", "Extrude Region and Move",
	                                  "Extrude region and move result", OPTYPE_UNDO | OPTYPE_REGISTER);
	otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_region");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);

	ot = WM_operatortype_append_macro("MESH_OT_extrude_region_shrink_fatten", "Extrude Region and Shrink/Fatten",
	                                  "Extrude region and move result", OPTYPE_UNDO | OPTYPE_REGISTER);
	otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_region");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_shrink_fatten");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);

	ot = WM_operatortype_append_macro("MESH_OT_extrude_faces_move", "Extrude Individual Faces and Move",
	                                  "Extrude faces and move result", OPTYPE_UNDO | OPTYPE_REGISTER);
	otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_faces_indiv");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_shrink_fatten");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);

	ot = WM_operatortype_append_macro("MESH_OT_extrude_edges_move", "Extrude Only Edges and Move",
	                                  "Extrude edges and move result", OPTYPE_UNDO | OPTYPE_REGISTER);
	otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_edges_indiv");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);

	ot = WM_operatortype_append_macro("MESH_OT_extrude_vertices_move", "Extrude Only Vertices and Move",
	                                  "Extrude vertices and move result", OPTYPE_UNDO | OPTYPE_REGISTER);
	otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_verts_indiv");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);
}

/* note mesh keymap also for other space? */
void ED_keymap_mesh(wmKeyConfig *keyconf)
{	
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	int i;
	
	keymap = WM_keymap_find(keyconf, "Mesh", 0, 0);
	keymap->poll = ED_operator_editmesh;
	
	WM_keymap_add_item(keymap, "MESH_OT_loopcut_slide", RKEY, KM_PRESS, KM_CTRL, 0);	
	WM_keymap_add_item(keymap, "MESH_OT_inset", IKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_poke", PKEY, KM_PRESS, KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_bevel", BKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "vertex_only", false);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_bevel", BKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "vertex_only", true);

	/* selecting */
	/* standard mouse selection goes via space_view3d */
	kmi = WM_keymap_add_item(keymap, "MESH_OT_loop_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_loop_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	kmi = WM_keymap_add_item(keymap, "MESH_OT_edgering_select", SELECTMOUSE, KM_PRESS, KM_ALT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_edgering_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	WM_keymap_add_item(keymap, "MESH_OT_shortest_path_pick", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "MESH_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "MESH_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_non_manifold", MKEY, KM_PRESS, (KM_CTRL | KM_SHIFT | KM_ALT), 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);
	
	WM_keymap_add_item(keymap, "MESH_OT_faces_select_linked_flat", FKEY, KM_PRESS, (KM_CTRL | KM_SHIFT | KM_ALT), 0);

	WM_keymap_add_item(keymap, "MESH_OT_select_similar", GKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* selection mode */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_select_mode", TABKEY, KM_PRESS, KM_CTRL, 0);
	
	/* hide */
	kmi = WM_keymap_add_item(keymap, "MESH_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", false);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", true);
	WM_keymap_add_item(keymap, "MESH_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);

	/* tools */
	kmi = WM_keymap_add_item(keymap, "MESH_OT_normals_make_consistent", NKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "inside", false);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_normals_make_consistent", NKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "inside", true);
	
	WM_keymap_add_item(keymap, "VIEW3D_OT_edit_mesh_extrude_move_normal", EKEY, KM_PRESS, 0, 0); /* python operator */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_extrude", EKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "TRANSFORM_OT_edge_crease", EKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_spin", RKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_fill", FKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_beautify_fill", FKEY, KM_PRESS, KM_SHIFT | KM_ALT, 0);

	kmi = WM_keymap_add_item(keymap, "MESH_OT_quads_convert_to_tris", TKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "quad_method", MOD_TRIANGULATE_QUAD_BEAUTY);
	RNA_enum_set(kmi->ptr, "ngon_method", MOD_TRIANGULATE_NGON_BEAUTY);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_quads_convert_to_tris", TKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "quad_method", MOD_TRIANGULATE_QUAD_FIXED);
	RNA_enum_set(kmi->ptr, "ngon_method", MOD_TRIANGULATE_NGON_EARCLIP);

	WM_keymap_add_item(keymap, "MESH_OT_tris_convert_to_quads", JKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "MESH_OT_rip_move", VKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_rip_move_fill", VKEY, KM_PRESS, KM_ALT, 0);

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

	/* Vertex Slide */
	WM_keymap_add_item(keymap, "TRANSFORM_OT_vert_slide", VKEY, KM_PRESS, KM_SHIFT, 0);
	/* use KM_CLICK because same key is used for tweaks */
	kmi = WM_keymap_add_item(keymap, "MESH_OT_dupli_extrude_cursor", ACTIONMOUSE, KM_CLICK, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "rotate_source", true);
	kmi = WM_keymap_add_item(keymap, "MESH_OT_dupli_extrude_cursor", ACTIONMOUSE, KM_CLICK, KM_SHIFT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "rotate_source", false);

	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_edit_mesh_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "MESH_OT_dissolve_mode", XKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_dissolve_mode", DELKEY, KM_PRESS, KM_CTRL, 0);
	
	kmi = WM_keymap_add_item(keymap, "MESH_OT_knife_tool", KKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "use_occlude_geometry", true);
	RNA_boolean_set(kmi->ptr, "only_selected",          false);

	kmi = WM_keymap_add_item(keymap, "MESH_OT_knife_tool", KKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "use_occlude_geometry", false);
	RNA_boolean_set(kmi->ptr, "only_selected",          true);
	
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
	for (i = 0; i <= 5; i++) {
		kmi = WM_keymap_add_item(keymap, "OBJECT_OT_subdivision_set", ZEROKEY + i, KM_PRESS, KM_CTRL, 0);
		RNA_int_set(kmi->ptr, "level", i);
	}
	
	ED_keymap_proportional_cycle(keyconf, keymap);
	ED_keymap_proportional_editmode(keyconf, keymap, true);

	knifetool_modal_keymap(keyconf);
}

