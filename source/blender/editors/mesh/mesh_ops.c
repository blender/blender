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
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 */

#include "DNA_scene_types.h"
#include "DNA_modifier_types.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "mesh_intern.h" /* own include */

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

  WM_operatortype_append(MESH_OT_primitive_cube_add_gizmo);

  WM_operatortype_append(MESH_OT_duplicate);
  WM_operatortype_append(MESH_OT_remove_doubles);
  WM_operatortype_append(MESH_OT_spin);
  WM_operatortype_append(MESH_OT_screw);

  WM_operatortype_append(MESH_OT_extrude_region);
  WM_operatortype_append(MESH_OT_extrude_context);
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
  WM_operatortype_append(MESH_OT_decimate);
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
  WM_operatortype_append(MESH_OT_select_similar_region);
  WM_operatortype_append(MESH_OT_select_mode);
  WM_operatortype_append(MESH_OT_loop_multi_select);
  WM_operatortype_append(MESH_OT_mark_seam);
  WM_operatortype_append(MESH_OT_mark_sharp);
#ifdef WITH_FREESTYLE
  WM_operatortype_append(MESH_OT_mark_freestyle_edge);
#endif
  WM_operatortype_append(MESH_OT_vertices_smooth);
  WM_operatortype_append(MESH_OT_vertices_smooth_laplacian);
  WM_operatortype_append(MESH_OT_flip_normals);
  WM_operatortype_append(MESH_OT_rip);
  WM_operatortype_append(MESH_OT_rip_edge);
  WM_operatortype_append(MESH_OT_blend_from_shape);
  WM_operatortype_append(MESH_OT_shape_propagate_to_all);

  /* editmesh_polybuild */
  WM_operatortype_append(MESH_OT_polybuild_face_at_cursor);
  WM_operatortype_append(MESH_OT_polybuild_split_at_cursor);
  WM_operatortype_append(MESH_OT_polybuild_dissolve_at_cursor);

  WM_operatortype_append(MESH_OT_uv_texture_add);
  WM_operatortype_append(MESH_OT_uv_texture_remove);
  WM_operatortype_append(MESH_OT_vertex_color_add);
  WM_operatortype_append(MESH_OT_vertex_color_remove);
  WM_operatortype_append(MESH_OT_customdata_mask_clear);
  WM_operatortype_append(MESH_OT_customdata_skin_add);
  WM_operatortype_append(MESH_OT_customdata_skin_clear);
  WM_operatortype_append(MESH_OT_customdata_custom_splitnormals_add);
  WM_operatortype_append(MESH_OT_customdata_custom_splitnormals_clear);

  WM_operatortype_append(MESH_OT_edgering_select);
  WM_operatortype_append(MESH_OT_loopcut);

  WM_operatortype_append(MESH_OT_solidify);
  WM_operatortype_append(MESH_OT_select_nth);
  WM_operatortype_append(MESH_OT_vert_connect);
  WM_operatortype_append(MESH_OT_vert_connect_path);
  WM_operatortype_append(MESH_OT_vert_connect_concave);
  WM_operatortype_append(MESH_OT_vert_connect_nonplanar);
  WM_operatortype_append(MESH_OT_face_make_planar);
  WM_operatortype_append(MESH_OT_knife_tool);
  WM_operatortype_append(MESH_OT_knife_project);

  WM_operatortype_append(MESH_OT_bevel);

  WM_operatortype_append(MESH_OT_bridge_edge_loops);
  WM_operatortype_append(MESH_OT_inset);
  WM_operatortype_append(MESH_OT_offset_edge_loops);
  WM_operatortype_append(MESH_OT_intersect);
  WM_operatortype_append(MESH_OT_intersect_boolean);
  WM_operatortype_append(MESH_OT_face_split_by_edges);
  WM_operatortype_append(MESH_OT_poke);
  WM_operatortype_append(MESH_OT_wireframe);
  WM_operatortype_append(MESH_OT_edge_split);

#ifdef WITH_BULLET
  WM_operatortype_append(MESH_OT_convex_hull);
#endif

  WM_operatortype_append(MESH_OT_bisect);
  WM_operatortype_append(MESH_OT_symmetrize);
  WM_operatortype_append(MESH_OT_symmetry_snap);

  WM_operatortype_append(MESH_OT_point_normals);
  WM_operatortype_append(MESH_OT_merge_normals);
  WM_operatortype_append(MESH_OT_split_normals);
  WM_operatortype_append(MESH_OT_normals_tools);
  WM_operatortype_append(MESH_OT_set_normals_from_faces);
  WM_operatortype_append(MESH_OT_average_normals);
  WM_operatortype_append(MESH_OT_smoothen_normals);
  WM_operatortype_append(MESH_OT_mod_weighted_strength);
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

  ot = WM_operatortype_append_macro("MESH_OT_loopcut_slide",
                                    "Loop Cut and Slide",
                                    "Cut mesh loop and slide it",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "MESH_OT_loopcut");
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_edge_slide");

  ot = WM_operatortype_append_macro("MESH_OT_offset_edge_loops_slide",
                                    "Offset Edge Slide",
                                    "Offset edge loop slide",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "MESH_OT_offset_edge_loops");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_edge_slide");
  RNA_boolean_set(otmacro->ptr, "single_side", true);

  ot = WM_operatortype_append_macro("MESH_OT_duplicate_move",
                                    "Add Duplicate",
                                    "Duplicate mesh and move",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "MESH_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_rip_move",
                                    "Rip",
                                    "Rip polygons and move the result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "MESH_OT_rip");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_rip_edge_move",
                                    "Extend Vertices",
                                    "Extend vertices and move the result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "MESH_OT_rip_edge");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_extrude_region_move",
                                    "Extrude Region and Move",
                                    "Extrude region and move result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_region");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_extrude_context_move",
                                    "Extrude Region and Move",
                                    "Extrude context and move result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_context");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_extrude_region_shrink_fatten",
                                    "Extrude Region and Shrink/Fatten",
                                    "Extrude along normals and move result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_region");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_shrink_fatten");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_extrude_faces_move",
                                    "Extrude Individual Faces and Move",
                                    "Extrude faces and move result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_faces_indiv");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_shrink_fatten");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_extrude_edges_move",
                                    "Extrude Only Edges and Move",
                                    "Extrude edges and move result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_edges_indiv");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_extrude_vertices_move",
                                    "Extrude Only Vertices and Move",
                                    "Extrude vertices and move result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "MESH_OT_extrude_verts_indiv");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_polybuild_face_at_cursor_move",
                                    "Face at Cursor Move",
                                    "",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "MESH_OT_polybuild_face_at_cursor");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("MESH_OT_polybuild_split_at_cursor_move",
                                    "Split at Cursor Move",
                                    "",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "MESH_OT_polybuild_split_at_cursor");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);
}

/* note mesh keymap also for other space? */
void ED_keymap_mesh(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Mesh", 0, 0);
  keymap->poll = ED_operator_editmesh;

  knifetool_modal_keymap(keyconf);
  point_normals_modal_keymap(keyconf);
  bevel_modal_keymap(keyconf);
}
