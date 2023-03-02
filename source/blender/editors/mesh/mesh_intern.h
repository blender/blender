/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edmesh
 */

/* Internal for editmesh_xxxx.c functions */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct BMEditMesh;
struct BMElem;
struct BMOperator;
struct EnumPropertyItem;
struct LinkNode;
struct Object;
struct bContext;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;

/* *** editmesh_utils.c *** */

/*
 * ok: the EDBM module is for editmode bmesh stuff.  in contrast, the
 * BMEdit module is for code shared with blenkernel that concerns
 * the BMEditMesh structure. */

/** Calls a bmesh op, reporting errors to the user, etc. */
bool EDBM_op_callf(struct BMEditMesh *em, struct wmOperator *op, const char *fmt, ...);
bool EDBM_op_call_and_selectf(struct BMEditMesh *em,
                              struct wmOperator *op,
                              const char *select_slot,
                              bool select_replace,
                              const char *fmt,
                              ...);
/**
 * Same as above, but doesn't report errors.
 */
bool EDBM_op_call_silentf(struct BMEditMesh *em, const char *fmt, ...);

/**
 * These next two functions are the split version of EDBM_op_callf, so you can
 * do stuff with a bmesh operator, after initializing it but before executing it.
 *
 * execute the operator with BM_Exec_Op */
bool EDBM_op_init(
    struct BMEditMesh *em, struct BMOperator *bmop, struct wmOperator *op, const char *fmt, ...);

/**
 * Cleans up after a bmesh operator.
 *
 * The return value:
 * - False on error (the mesh must not be changed).
 * - True on success, executes and finishes a #BMesh operator.
 */
bool EDBM_op_finish(struct BMEditMesh *em,
                    struct BMOperator *bmop,
                    struct wmOperator *op,
                    bool do_report);

void EDBM_stats_update(struct BMEditMesh *em);

/**
 * Poll call for mesh operators requiring a view3d context.
 */
bool EDBM_view3d_poll(struct bContext *C);

struct BMElem *EDBM_elem_from_selectmode(struct BMEditMesh *em,
                                         struct BMVert *eve,
                                         struct BMEdge *eed,
                                         struct BMFace *efa);

/**
 * Used when we want to store a single index for any vert/edge/face.
 *
 * Intended for use with operators.
 */
int EDBM_elem_to_index_any(struct BMEditMesh *em, struct BMElem *ele);
struct BMElem *EDBM_elem_from_index_any(struct BMEditMesh *em, uint index);

int EDBM_elem_to_index_any_multi(const struct Scene *scene,
                                 struct ViewLayer *view_layer,
                                 struct BMEditMesh *em,
                                 struct BMElem *ele,
                                 int *r_object_index);
struct BMElem *EDBM_elem_from_index_any_multi(const struct Scene *scene,
                                              struct ViewLayer *view_layer,
                                              uint object_index,
                                              uint elem_index,
                                              struct Object **r_obedit);

/**
 * Extrudes individual edges.
 */
bool edbm_extrude_edges_indiv(struct BMEditMesh *em,
                              struct wmOperator *op,
                              char hflag,
                              bool use_normal_flip);

/* *** editmesh_add.c *** */

void MESH_OT_primitive_plane_add(struct wmOperatorType *ot);
void MESH_OT_primitive_cube_add(struct wmOperatorType *ot);
void MESH_OT_primitive_circle_add(struct wmOperatorType *ot);
void MESH_OT_primitive_cylinder_add(struct wmOperatorType *ot);
void MESH_OT_primitive_cone_add(struct wmOperatorType *ot);
void MESH_OT_primitive_grid_add(struct wmOperatorType *ot);
void MESH_OT_primitive_monkey_add(struct wmOperatorType *ot);
void MESH_OT_primitive_uv_sphere_add(struct wmOperatorType *ot);
void MESH_OT_primitive_ico_sphere_add(struct wmOperatorType *ot);

/* *** editmesh_add_gizmo.c *** */

void MESH_OT_primitive_cube_add_gizmo(struct wmOperatorType *ot);

/* *** editmesh_attribute.cc *** */

void MESH_OT_attribute_set(struct wmOperatorType *ot);

/* *** editmesh_bevel.c *** */

void MESH_OT_bevel(struct wmOperatorType *ot);
struct wmKeyMap *bevel_modal_keymap(struct wmKeyConfig *keyconf);

/* *** editmesh_bisect.c *** */

void MESH_OT_bisect(struct wmOperatorType *ot);

/* *** editmesh_extrude.c *** */

void MESH_OT_extrude_repeat(struct wmOperatorType *ot);
void MESH_OT_extrude_region(struct wmOperatorType *ot);
void MESH_OT_extrude_context(struct wmOperatorType *ot);
void MESH_OT_extrude_verts_indiv(struct wmOperatorType *ot);
void MESH_OT_extrude_edges_indiv(struct wmOperatorType *ot);
void MESH_OT_extrude_faces_indiv(struct wmOperatorType *ot);
void MESH_OT_dupli_extrude_cursor(struct wmOperatorType *ot);

/* *** editmesh_extrude_screw.c *** */

void MESH_OT_screw(struct wmOperatorType *ot);

/* *** editmesh_extrude_spin.c *** */

void MESH_OT_spin(struct wmOperatorType *ot);

/* *** editmesh_extrude_spin_gizmo.c *** */

void MESH_GGT_spin(struct wmGizmoGroupType *gzgt);
void MESH_GGT_spin_redo(struct wmGizmoGroupType *gzgt);

/* *** editmesh_polybuild.c *** */

void MESH_OT_polybuild_face_at_cursor(struct wmOperatorType *ot);
void MESH_OT_polybuild_split_at_cursor(struct wmOperatorType *ot);
void MESH_OT_polybuild_dissolve_at_cursor(struct wmOperatorType *ot);
void MESH_OT_polybuild_transform_at_cursor(struct wmOperatorType *ot);
void MESH_OT_polybuild_delete_at_cursor(struct wmOperatorType *ot);

/* *** editmesh_inset.c *** */

void MESH_OT_inset(struct wmOperatorType *ot);

/* *** editmesh_intersect.c *** */

void MESH_OT_intersect(struct wmOperatorType *ot);
void MESH_OT_intersect_boolean(struct wmOperatorType *ot);
void MESH_OT_face_split_by_edges(struct wmOperatorType *ot);

/* *** editmesh_knife.c *** */

void MESH_OT_knife_tool(struct wmOperatorType *ot);
void MESH_OT_knife_project(struct wmOperatorType *ot);
/**
 * \param use_tag: When set, tag all faces inside the polylines.
 */
void EDBM_mesh_knife(struct ViewContext *vc,
                     struct Object **objects,
                     int objects_len,
                     struct LinkNode *polys,
                     bool use_tag,
                     bool cut_through);

struct wmKeyMap *knifetool_modal_keymap(struct wmKeyConfig *keyconf);

/* *** editmesh_loopcut.c *** */

void MESH_OT_loopcut(struct wmOperatorType *ot);

/* *** editmesh_rip.c *** */

void MESH_OT_rip(struct wmOperatorType *ot);
void MESH_OT_rip_edge(struct wmOperatorType *ot);

/* *** editmesh_select.cc *** */

void MESH_OT_select_similar(struct wmOperatorType *ot);
void MESH_OT_select_similar_region(struct wmOperatorType *ot);
void MESH_OT_select_mode(struct wmOperatorType *ot);
void MESH_OT_loop_multi_select(struct wmOperatorType *ot);
void MESH_OT_loop_select(struct wmOperatorType *ot);
void MESH_OT_edgering_select(struct wmOperatorType *ot);
void MESH_OT_select_all(struct wmOperatorType *ot);
void MESH_OT_select_interior_faces(struct wmOperatorType *ot);
void MESH_OT_shortest_path_pick(struct wmOperatorType *ot);
void MESH_OT_select_linked(struct wmOperatorType *ot);
void MESH_OT_select_linked_pick(struct wmOperatorType *ot);
void MESH_OT_select_face_by_sides(struct wmOperatorType *ot);
void MESH_OT_select_loose(struct wmOperatorType *ot);
void MESH_OT_select_mirror(struct wmOperatorType *ot);
void MESH_OT_select_more(struct wmOperatorType *ot);
void MESH_OT_select_less(struct wmOperatorType *ot);
void MESH_OT_select_nth(struct wmOperatorType *ot);
void MESH_OT_edges_select_sharp(struct wmOperatorType *ot);
void MESH_OT_faces_select_linked_flat(struct wmOperatorType *ot);
void MESH_OT_select_non_manifold(struct wmOperatorType *ot);
void MESH_OT_select_random(struct wmOperatorType *ot);
void MESH_OT_select_ungrouped(struct wmOperatorType *ot);
void MESH_OT_select_axis(struct wmOperatorType *ot);
void MESH_OT_region_to_loop(struct wmOperatorType *ot);
void MESH_OT_loop_to_region(struct wmOperatorType *ot);
void MESH_OT_shortest_path_select(struct wmOperatorType *ot);

extern struct EnumPropertyItem *corner_type_items;

/* *** editmesh_tools.cc *** */
void MESH_OT_subdivide(struct wmOperatorType *ot);
void MESH_OT_subdivide_edgering(struct wmOperatorType *ot);
void MESH_OT_unsubdivide(struct wmOperatorType *ot);
void MESH_OT_normals_make_consistent(struct wmOperatorType *ot);
void MESH_OT_vertices_smooth(struct wmOperatorType *ot);
void MESH_OT_vertices_smooth_laplacian(struct wmOperatorType *ot);
void MESH_OT_vert_connect(struct wmOperatorType *ot);
void MESH_OT_vert_connect_path(struct wmOperatorType *ot);
void MESH_OT_vert_connect_concave(struct wmOperatorType *ot);
void MESH_OT_vert_connect_nonplanar(struct wmOperatorType *ot);
void MESH_OT_face_make_planar(struct wmOperatorType *ot);
void MESH_OT_edge_split(struct wmOperatorType *ot);
void MESH_OT_bridge_edge_loops(struct wmOperatorType *ot);
void MESH_OT_offset_edge_loops(struct wmOperatorType *ot);
void MESH_OT_wireframe(struct wmOperatorType *ot);
void MESH_OT_convex_hull(struct wmOperatorType *ot);
void MESH_OT_symmetrize(struct wmOperatorType *ot);
void MESH_OT_symmetry_snap(struct wmOperatorType *ot);
void MESH_OT_shape_propagate_to_all(struct wmOperatorType *ot);
void MESH_OT_blend_from_shape(struct wmOperatorType *ot);
void MESH_OT_sort_elements(struct wmOperatorType *ot);
void MESH_OT_uvs_rotate(struct wmOperatorType *ot);
void MESH_OT_uvs_reverse(struct wmOperatorType *ot);
void MESH_OT_colors_rotate(struct wmOperatorType *ot);
void MESH_OT_colors_reverse(struct wmOperatorType *ot);
void MESH_OT_delete(struct wmOperatorType *ot);
void MESH_OT_delete_loose(struct wmOperatorType *ot);
void MESH_OT_edge_collapse(struct wmOperatorType *ot);
void MESH_OT_faces_shade_smooth(struct wmOperatorType *ot);
void MESH_OT_faces_shade_flat(struct wmOperatorType *ot);
void MESH_OT_split(struct wmOperatorType *ot);
void MESH_OT_edge_rotate(struct wmOperatorType *ot);
void MESH_OT_hide(struct wmOperatorType *ot);
void MESH_OT_reveal(struct wmOperatorType *ot);
void MESH_OT_mark_seam(struct wmOperatorType *ot);
void MESH_OT_mark_sharp(struct wmOperatorType *ot);
void MESH_OT_flip_normals(struct wmOperatorType *ot);
void MESH_OT_solidify(struct wmOperatorType *ot);
void MESH_OT_knife_cut(struct wmOperatorType *ot);
void MESH_OT_separate(struct wmOperatorType *ot);
void MESH_OT_fill(struct wmOperatorType *ot);
void MESH_OT_fill_grid(struct wmOperatorType *ot);
void MESH_OT_fill_holes(struct wmOperatorType *ot);
void MESH_OT_beautify_fill(struct wmOperatorType *ot);
void MESH_OT_quads_convert_to_tris(struct wmOperatorType *ot);
void MESH_OT_tris_convert_to_quads(struct wmOperatorType *ot);
void MESH_OT_decimate(struct wmOperatorType *ot);
void MESH_OT_dissolve_verts(struct wmOperatorType *ot);
void MESH_OT_dissolve_edges(struct wmOperatorType *ot);
void MESH_OT_dissolve_faces(struct wmOperatorType *ot);
void MESH_OT_dissolve_mode(struct wmOperatorType *ot);
void MESH_OT_dissolve_limited(struct wmOperatorType *ot);
void MESH_OT_dissolve_degenerate(struct wmOperatorType *ot);
void MESH_OT_delete_edgeloop(struct wmOperatorType *ot);
void MESH_OT_edge_face_add(struct wmOperatorType *ot);
void MESH_OT_duplicate(struct wmOperatorType *ot);
void MESH_OT_merge(struct wmOperatorType *ot);
void MESH_OT_remove_doubles(struct wmOperatorType *ot);
void MESH_OT_poke(struct wmOperatorType *ot);
void MESH_OT_point_normals(struct wmOperatorType *ot);
void MESH_OT_merge_normals(struct wmOperatorType *ot);
void MESH_OT_split_normals(struct wmOperatorType *ot);
void MESH_OT_normals_tools(struct wmOperatorType *ot);
void MESH_OT_set_normals_from_faces(struct wmOperatorType *ot);
void MESH_OT_average_normals(struct wmOperatorType *ot);
void MESH_OT_smooth_normals(struct wmOperatorType *ot);
void MESH_OT_mod_weighted_strength(struct wmOperatorType *ot);
void MESH_OT_flip_quad_tessellation(struct wmOperatorType *ot);

/* *** editmesh_mask_extract.cc *** */

void MESH_OT_paint_mask_extract(struct wmOperatorType *ot);
void MESH_OT_face_set_extract(struct wmOperatorType *ot);
void MESH_OT_paint_mask_slice(struct wmOperatorType *ot);

/** Called in transform_ops.c, on each regeneration of key-maps. */
struct wmKeyMap *point_normals_modal_keymap(wmKeyConfig *keyconf);

#if defined(WITH_FREESTYLE)
void MESH_OT_mark_freestyle_edge(struct wmOperatorType *ot);
void MESH_OT_mark_freestyle_face(struct wmOperatorType *ot);
#endif

/* *** mesh_data.cc *** */

void MESH_OT_uv_texture_add(struct wmOperatorType *ot);
void MESH_OT_uv_texture_remove(struct wmOperatorType *ot);
void MESH_OT_customdata_mask_clear(struct wmOperatorType *ot);
void MESH_OT_customdata_skin_add(struct wmOperatorType *ot);
void MESH_OT_customdata_skin_clear(struct wmOperatorType *ot);
void MESH_OT_customdata_custom_splitnormals_add(struct wmOperatorType *ot);
void MESH_OT_customdata_custom_splitnormals_clear(struct wmOperatorType *ot);
void MESH_OT_customdata_bevel_weight_vertex_add(struct wmOperatorType *ot);
void MESH_OT_customdata_bevel_weight_vertex_clear(struct wmOperatorType *ot);
void MESH_OT_customdata_bevel_weight_edge_add(struct wmOperatorType *ot);
void MESH_OT_customdata_bevel_weight_edge_clear(struct wmOperatorType *ot);
void MESH_OT_customdata_crease_vertex_add(struct wmOperatorType *ot);
void MESH_OT_customdata_crease_vertex_clear(struct wmOperatorType *ot);
void MESH_OT_customdata_crease_edge_add(struct wmOperatorType *ot);
void MESH_OT_customdata_crease_edge_clear(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
