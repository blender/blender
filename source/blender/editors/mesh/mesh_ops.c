/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "mesh_intern.h"

/******************************** menus *************************************/

static int vertex_specials_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Vertex Specials", 0);
	layout= uiPupMenuLayout(pup);
	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

	uiItemO(layout, "Remove Doubles", 0, "MESH_OT_remove_doubles");
	uiItemO(layout, "Merge...", 0, "MESH_OT_merge");
	uiItemO(layout, "Smooth", 0, "MESH_OT_vertices_smooth");
	uiItemO(layout, "Select Vertex Path", 0, "MESH_OT_select_vertex_path"); 
	//uiItemO(layout, "Blend From Shape", 0, "MESH_OT_blend_from_shape");
	//uiItemO(layout, "Propagate to All Shapes", 0, "MESH_OT_shape_propagate_to_all");

	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

static void MESH_OT_vertex_specials(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Vertex Specials";
	//ot->description= "Perform special vertex operations.";
	ot->idname= "MESH_OT_vertex_specials";
	
	/* api callbacks */
	ot->invoke= vertex_specials_invoke;
	ot->poll= ED_operator_editmesh;
}

static int edge_specials_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Edge Specials", 0);
	layout= uiPupMenuLayout(pup);
	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

	uiItemO(layout, "Mark Seam", 0, "MESH_OT_mark_seam");
	uiItemBooleanO(layout, "Clear Seam", 0, "MESH_OT_mark_seam", "clear", 1);
	uiItemEnumO(layout, "Rotate Edge CW", 0, "MESH_OT_edge_rotate", "direction", 1);
	uiItemEnumO(layout, "Rotate Edge CCW", 0, "MESH_OT_edge_rotate", "direction", 2);
	//uiItemO(layout, "Loopcut", 0, "MESH_OT_loop_cut"); // CutEdgeloop(em, 1);
	//uiItemO(layout, "Edge Slide", 0, "MESH_OT_edge_slide"); // EdgeSlide(em, 0,0.0);
	uiItemO(layout, "Edge Loop", 0, "MESH_OT_loop_multi_select");
	uiItemBooleanO(layout, "Edge Ring", 0, "MESH_OT_loop_multi_select", "ring", 1);
	uiItemO(layout, NULL, 0, "MESH_OT_loop_to_region");
	uiItemO(layout, NULL, 0, "MESH_OT_region_to_loop");
	uiItemO(layout, "Mark Sharp", 0, "MESH_OT_mark_sharp");
	uiItemBooleanO(layout, "Clear Sharp", 0, "MESH_OT_mark_sharp", "clear", 1);

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

static void MESH_OT_edge_specials(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Edge Specials";
	//ot->description= "Perform special edge operations.";
	ot->idname= "MESH_OT_edge_specials";
	
	/* api callbacks */
	ot->invoke= edge_specials_invoke;
	ot->poll= ED_operator_editmesh;
}

static int face_specials_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Face Specials", 0);
	layout= uiPupMenuLayout(pup);
	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

	uiItemO(layout, NULL, 0, "MESH_OT_flip_normals");
	// uiItemO(layout, "Bevel", 0, "MESH_OT_bevel"); // bevelmenu(em)
	uiItemO(layout, NULL, 0, "MESH_OT_faces_shade_smooth");
	uiItemO(layout, NULL, 0, "MESH_OT_faces_shade_flat");
	uiItemO(layout, NULL, 0, "MESH_OT_quads_convert_to_tris");
	uiItemO(layout, NULL, 0, "MESH_OT_tris_convert_to_quads");
	uiItemO(layout, NULL, 0, "MESH_OT_edge_flip");

	uiItemS(layout);

	uiItemO(layout, NULL, 0, "MESH_OT_fill");
	uiItemO(layout, NULL, 0, "MESH_OT_beauty_fill");

	uiItemS(layout);

	// uiItemO(layout, NULL, 0, "MESH_OT_face_mode"); // mesh_set_face_flags(em, 1);
	// uiItemBooleanO(layout, NULL, 0, "MESH_OT_face_mode", "clear", 1); // mesh_set_face_flags(em, 0);
	//
	// uiItemS(layout);

	uiItemMenuEnumO(layout, NULL, 0, "MESH_OT_uvs_rotate", "direction");
	uiItemMenuEnumO(layout, NULL, 0, "MESH_OT_uvs_mirror", "axis");
	uiItemMenuEnumO(layout, NULL, 0, "MESH_OT_colors_rotate", "direction");
	uiItemMenuEnumO(layout, NULL, 0, "MESH_OT_colors_mirror", "axis");

	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

static void MESH_OT_face_specials(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Face Specials";
	//ot->description= "Perform special face operations.";
	ot->idname= "MESH_OT_face_specials";
	
	/* api callbacks */
	ot->invoke= face_specials_invoke;
	ot->poll= ED_operator_editmesh;
}

static int specials_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Specials", 0);
	layout= uiPupMenuLayout(pup);
	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

	uiItemO(layout, "Subdivide", 0, "MESH_OT_subdivide");
	uiItemFloatO(layout, "Subdivide Smooth", 0, "MESH_OT_subdivide", "smoothness", 1.0f);
	uiItemO(layout, "Merge...", 0, "MESH_OT_merge");
	uiItemO(layout, "Remove Doubles", 0, "MESH_OT_remove_doubles");
	uiItemO(layout, "Hide", 0, "MESH_OT_hide");
	uiItemO(layout, "Reveal", 0, "MESH_OT_reveal");
	uiItemO(layout, "Select Inverse", 0, "MESH_OT_select_inverse");
	uiItemO(layout, NULL, 0, "MESH_OT_flip_normals");
	uiItemO(layout, "Smooth", 0, "MESH_OT_vertices_smooth");
	// uiItemO(layout, "Bevel", 0, "MESH_OT_bevel"); // bevelmenu(em)
	uiItemO(layout, NULL, 0, "MESH_OT_faces_shade_smooth");
	uiItemO(layout, NULL, 0, "MESH_OT_faces_shade_flat");
	//uiItemO(layout, "Blend From Shape", 0, "MESH_OT_blend_from_shape");
	//uiItemO(layout, "Propagate to All Shapes", 0, "MESH_OT_shape_propagate_to_all");
	uiItemO(layout, "Select Vertex Path", 0, "MESH_OT_select_vertex_path"); 
	
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

static void MESH_OT_specials(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Specials";
	//ot->description= "Perform special vertice, edge or face operations.";
	ot->idname= "MESH_OT_specials";
	
	/* api callbacks */
	ot->invoke= specials_invoke;
	ot->poll= ED_operator_editmesh;
}

/**************************** registration **********************************/

void ED_operatortypes_mesh(void)
{
	wmOperatorType *ot;
	
	WM_operatortype_append(MESH_OT_select_all_toggle);
	WM_operatortype_append(MESH_OT_select_more);
	WM_operatortype_append(MESH_OT_select_less);
	WM_operatortype_append(MESH_OT_select_inverse);
	WM_operatortype_append(MESH_OT_select_non_manifold);
	WM_operatortype_append(MESH_OT_select_linked);
	WM_operatortype_append(MESH_OT_select_linked_pick);
	WM_operatortype_append(MESH_OT_select_random);
	WM_operatortype_append(MESH_OT_selection_type);
	WM_operatortype_append(MESH_OT_hide);
	WM_operatortype_append(MESH_OT_reveal);
	WM_operatortype_append(MESH_OT_select_by_number_vertices);
	WM_operatortype_append(MESH_OT_normals_make_consistent);
	WM_operatortype_append(MESH_OT_merge);
	WM_operatortype_append(MESH_OT_subdivide);
	WM_operatortype_append(MESH_OT_faces_select_linked_flat);
	WM_operatortype_append(MESH_OT_edges_select_sharp);
	WM_operatortype_append(MESH_OT_primitive_plane_add);
	WM_operatortype_append(MESH_OT_primitive_cube_add);
	WM_operatortype_append(MESH_OT_primitive_circle_add);
	WM_operatortype_append(MESH_OT_primitive_cylinder_add);
	WM_operatortype_append(MESH_OT_primitive_tube_add);
	WM_operatortype_append(MESH_OT_primitive_cone_add);
	WM_operatortype_append(MESH_OT_primitive_grid_add);
	WM_operatortype_append(MESH_OT_primitive_monkey_add);
	WM_operatortype_append(MESH_OT_primitive_uv_sphere_add);
	WM_operatortype_append(MESH_OT_primitive_ico_sphere_add);
	WM_operatortype_append(MESH_OT_fgon_clear);
	WM_operatortype_append(MESH_OT_fgon_make);
	WM_operatortype_append(MESH_OT_duplicate);
	WM_operatortype_append(MESH_OT_remove_doubles);
	WM_operatortype_append(MESH_OT_extrude);
	WM_operatortype_append(MESH_OT_spin);
	WM_operatortype_append(MESH_OT_screw);
	
	WM_operatortype_append(MESH_OT_vertices_transform_to_sphere);
	WM_operatortype_append(MESH_OT_split);
	WM_operatortype_append(MESH_OT_extrude_repeat);
	WM_operatortype_append(MESH_OT_edge_rotate);
	WM_operatortype_append(MESH_OT_select_vertex_path);
	WM_operatortype_append(MESH_OT_loop_to_region);
	WM_operatortype_append(MESH_OT_region_to_loop);
	
	WM_operatortype_append(MESH_OT_uvs_rotate);
	WM_operatortype_append(MESH_OT_uvs_mirror);
	WM_operatortype_append(MESH_OT_colors_rotate);
	WM_operatortype_append(MESH_OT_colors_mirror);
	
	WM_operatortype_append(MESH_OT_fill);
	WM_operatortype_append(MESH_OT_beauty_fill);
	WM_operatortype_append(MESH_OT_quads_convert_to_tris);
	WM_operatortype_append(MESH_OT_tris_convert_to_quads);
	WM_operatortype_append(MESH_OT_edge_flip);
	WM_operatortype_append(MESH_OT_faces_shade_smooth);
	WM_operatortype_append(MESH_OT_faces_shade_flat);

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
	WM_operatortype_append(MESH_OT_flip_normals);
	WM_operatortype_append(MESH_OT_knife_cut);
	WM_operatortype_append(MESH_OT_rip);
	
	WM_operatortype_append(MESH_OT_uv_texture_add);
	WM_operatortype_append(MESH_OT_uv_texture_remove);
	WM_operatortype_append(MESH_OT_vertex_color_add);
	WM_operatortype_append(MESH_OT_vertex_color_remove);
	WM_operatortype_append(MESH_OT_sticky_add);
	WM_operatortype_append(MESH_OT_sticky_remove);

	WM_operatortype_append(MESH_OT_vertex_specials);
	WM_operatortype_append(MESH_OT_edge_specials);
	WM_operatortype_append(MESH_OT_face_specials);
	WM_operatortype_append(MESH_OT_specials);
	
	WM_operatortype_append(MESH_OT_edgering_select);

	/* macros */
	ot= WM_operatortype_append_macro("MESH_OT_duplicate_move", "Add Duplicate", OPTYPE_UNDO|OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "MESH_OT_duplicate");
	WM_operatortype_macro_define(ot, "TFM_OT_translate");

	ot= WM_operatortype_append_macro("MESH_OT_rip_move", "Rip", OPTYPE_UNDO|OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "MESH_OT_rip");
	WM_operatortype_macro_define(ot, "TFM_OT_translate");

	ot= WM_operatortype_append_macro("MESH_OT_extrude_move", "Extrude", OPTYPE_UNDO|OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "MESH_OT_extrude");
	WM_operatortype_macro_define(ot, "TFM_OT_translate");

	/*combining operators with invoke and exec portions doesn't work yet.
	
	ot= WM_operatortype_append_macro("MESH_OT_loopcut", "Loopcut", OPTYPE_UNDO|OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "MESH_OT_edgering_select");
	WM_operatortype_macro_define(ot, "MESH_OT_subdivide");
	*/
}

/* note mesh keymap also for other space? */
void ED_keymap_mesh(wmWindowManager *wm)
{	
	ListBase *keymap= WM_keymap_listbase(wm, "EditMesh", 0, 0);
	wmKeymapItem *kmi;
	
	//WM_keymap_add_item(keymap, "MESH_OT_loopcut", RKEY, KM_PRESS, KM_CTRL, 0);

	/* selecting */
	/* standard mouse selection goes via space_view3d */
	WM_keymap_add_item(keymap, "MESH_OT_loop_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	kmi= WM_keymap_add_item(keymap, "MESH_OT_loop_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", 1);

	kmi= WM_keymap_add_item(keymap, "MESH_OT_edgering_select", SELECTMOUSE, KM_PRESS, KM_ALT|KM_CTRL, 0);
	kmi= WM_keymap_add_item(keymap, "MESH_OT_edgering_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", 1);

	WM_keymap_add_item(keymap, "MESH_OT_select_shortest_path", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "MESH_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_non_manifold", MKEY, KM_PRESS, (KM_CTRL|KM_SHIFT|KM_ALT), 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "MESH_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "deselect", 1);
	
	RNA_float_set(WM_keymap_add_item(keymap, "MESH_OT_faces_select_linked_flat", FKEY, KM_PRESS, (KM_CTRL|KM_SHIFT|KM_ALT), 0)->ptr,"sharpness",135.0);
	RNA_float_set(WM_keymap_add_item(keymap, "MESH_OT_edges_select_sharp", SKEY, KM_PRESS, (KM_CTRL|KM_SHIFT|KM_ALT), 0)->ptr,"sharpness",135.0);
	
	WM_keymap_add_item(keymap, "MESH_OT_vertices_transform_to_sphere", SKEY, KM_PRESS, KM_CTRL|KM_SHIFT , 0);

	WM_keymap_add_item(keymap, "MESH_OT_select_similar", GKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* selection mode */
	WM_keymap_add_item(keymap, "MESH_OT_selection_type", TABKEY, KM_PRESS, KM_CTRL, 0);
	
	/* hide */
	WM_keymap_add_item(keymap, "MESH_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "MESH_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "unselected", 1);
	WM_keymap_add_item(keymap, "MESH_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);
	
	/* tools */
	WM_keymap_add_item(keymap, "MESH_OT_normals_make_consistent", NKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "MESH_OT_normals_make_consistent", NKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0)->ptr, "inside", 1);
	
	WM_keymap_add_item(keymap, "MESH_OT_extrude_move", EKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_spin", RKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_screw", NINEKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_fill", FKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_beauty_fill", FKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_quads_convert_to_tris", TKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_tris_convert_to_quads", JKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_edge_flip", FKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_split", FOURKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_extrude_repeat", FOURKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_edge_rotate", FIVEKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_loop_to_region",SIXKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_region_to_loop",SIXKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_uvs_rotate",SEVENKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_uvs_mirror",SEVENKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_colors_rotate",EIGHTKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_colors_mirror",EIGHTKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "MESH_OT_rip_move",VKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_merge", MKEY, KM_PRESS, KM_ALT, 0);

	/* add/remove */
	WM_keymap_add_item(keymap, "MESH_OT_edge_face_add", FKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MESH_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_mesh_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_separate", PKEY, KM_PRESS, KM_SHIFT, 0);
						/* use KM_RELEASE because same key is used for tweaks */
	WM_keymap_add_item(keymap, "MESH_OT_dupli_extrude_cursor", LEFTMOUSE, KM_RELEASE, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_delete", XKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_fgon_make", FKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MESH_OT_fgon_clear", FKEY, KM_PRESS, KM_SHIFT|KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_knife_cut", LEFTMOUSE, KM_PRESS, KM_CTRL, XKEY);

	/* menus */
	WM_keymap_add_item(keymap, "MESH_OT_vertex_specials", VKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_edge_specials", EKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_face_specials", FKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MESH_OT_specials", WKEY, KM_PRESS, 0, 0);
	
	/* UV's */
	WM_keymap_add_item(keymap, "UV_OT_mapping_menu", UKEY, KM_PRESS, 0, 0);
}

