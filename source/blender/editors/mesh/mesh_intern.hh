/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

/* Internal for editmesh_xxxx.c functions */

#pragma once

#include "BLI_span.hh"
#include "BLI_sys_types.h"

struct bContext;
struct BMEditMesh;
struct BMEdge;
struct BMElem;
struct BMFace;
struct BMVert;
struct BMOperator;
struct EnumPropertyItem;
struct LinkNode;
struct Object;
struct Scene;
struct wmGizmoGroupType;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;
struct ViewContext;
struct ViewLayer;

/* *** editmesh_utils.cc *** */

/*
 * ok: the EDBM module is for editmode bmesh stuff.  in contrast, the
 * BMEdit module is for code shared with blenkernel that concerns
 * the BMEditMesh structure. */

/** Calls a bmesh op, reporting errors to the user, etc. */
bool EDBM_op_callf(BMEditMesh *em, wmOperator *op, const char *fmt, ...);
bool EDBM_op_call_and_selectf(BMEditMesh *em,
                              wmOperator *op,
                              const char *select_slot,
                              bool select_replace,
                              const char *fmt,
                              ...);
/**
 * Same as above, but doesn't report errors.
 */
bool EDBM_op_call_silentf(BMEditMesh *em, const char *fmt, ...);

/**
 * These next two functions are the split version of EDBM_op_callf, so you can
 * do stuff with a bmesh operator, after initializing it but before executing it.
 *
 * execute the operator with #BMO_op_exec.
 */
bool EDBM_op_init(BMEditMesh *em, BMOperator *bmop, wmOperator *op, const char *fmt, ...);

/**
 * Cleans up after a bmesh operator.
 *
 * The return value:
 * - False on error (the mesh must not be changed).
 * - True on success, executes and finishes a #BMesh operator.
 */
bool EDBM_op_finish(BMEditMesh *em, BMOperator *bmop, wmOperator *op, bool do_report);

void EDBM_stats_update(BMEditMesh *em);

/**
 * Poll call for mesh operators requiring a view3d context.
 */
bool EDBM_view3d_poll(bContext *C);

BMElem *EDBM_elem_from_selectmode(BMEditMesh *em, BMVert *eve, BMEdge *eed, BMFace *efa);

/**
 * Used when we want to store a single index for any vert/edge/face.
 *
 * Intended for use with operators.
 */
int EDBM_elem_to_index_any(BMEditMesh *em, BMElem *ele);
BMElem *EDBM_elem_from_index_any(BMEditMesh *em, uint index);

int EDBM_elem_to_index_any_multi(
    const Scene *scene, ViewLayer *view_layer, BMEditMesh *em, BMElem *ele, int *r_object_index);
BMElem *EDBM_elem_from_index_any_multi(const Scene *scene,
                                       ViewLayer *view_layer,
                                       uint object_index,
                                       uint elem_index,
                                       Object **r_obedit);

/**
 * Extrudes individual edges.
 */
bool edbm_extrude_edges_indiv(BMEditMesh *em, wmOperator *op, char hflag, bool use_normal_flip);

/* *** `editmesh_add.cc` *** */

void MESH_OT_primitive_plane_add(wmOperatorType *ot);
void MESH_OT_primitive_cube_add(wmOperatorType *ot);
void MESH_OT_primitive_circle_add(wmOperatorType *ot);
void MESH_OT_primitive_cylinder_add(wmOperatorType *ot);
void MESH_OT_primitive_cone_add(wmOperatorType *ot);
void MESH_OT_primitive_grid_add(wmOperatorType *ot);
void MESH_OT_primitive_monkey_add(wmOperatorType *ot);
void MESH_OT_primitive_uv_sphere_add(wmOperatorType *ot);
void MESH_OT_primitive_ico_sphere_add(wmOperatorType *ot);

/* *** `editmesh_add_gizmo.cc` *** */

void MESH_OT_primitive_cube_add_gizmo(wmOperatorType *ot);

/* *** editmesh_attribute.cc *** */

void MESH_OT_attribute_set(wmOperatorType *ot);

/* *** `editmesh_bevel.cc` *** */

void MESH_OT_bevel(wmOperatorType *ot);
wmKeyMap *bevel_modal_keymap(wmKeyConfig *keyconf);

/* *** `editmesh_bisect.cc` *** */

void MESH_OT_bisect(wmOperatorType *ot);

/* *** `editmesh_extrude.cc` *** */

void MESH_OT_extrude_repeat(wmOperatorType *ot);
void MESH_OT_extrude_region(wmOperatorType *ot);
void MESH_OT_extrude_context(wmOperatorType *ot);
void MESH_OT_extrude_verts_indiv(wmOperatorType *ot);
void MESH_OT_extrude_edges_indiv(wmOperatorType *ot);
void MESH_OT_extrude_faces_indiv(wmOperatorType *ot);
void MESH_OT_dupli_extrude_cursor(wmOperatorType *ot);

/* *** `editmesh_extrude_screw.cc` *** */

void MESH_OT_screw(wmOperatorType *ot);

/* *** `editmesh_extrude_spin.cc` *** */

void MESH_OT_spin(wmOperatorType *ot);

/* *** `editmesh_extrude_spin_gizmo.cc` *** */

void MESH_GGT_spin(wmGizmoGroupType *gzgt);
void MESH_GGT_spin_redo(wmGizmoGroupType *gzgt);

/* *** `editmesh_polybuild.cc` *** */

void MESH_OT_polybuild_face_at_cursor(wmOperatorType *ot);
void MESH_OT_polybuild_split_at_cursor(wmOperatorType *ot);
void MESH_OT_polybuild_dissolve_at_cursor(wmOperatorType *ot);
void MESH_OT_polybuild_transform_at_cursor(wmOperatorType *ot);
void MESH_OT_polybuild_delete_at_cursor(wmOperatorType *ot);

/* *** `editmesh_inset.cc` *** */

void MESH_OT_inset(wmOperatorType *ot);

/* *** `editmesh_intersect.cc` *** */

void MESH_OT_intersect(wmOperatorType *ot);
void MESH_OT_intersect_boolean(wmOperatorType *ot);
void MESH_OT_face_split_by_edges(wmOperatorType *ot);

/* *** `editmesh_knife.cc` *** */

void MESH_OT_knife_tool(wmOperatorType *ot);
void MESH_OT_knife_project(wmOperatorType *ot);
/**
 * \param use_tag: When set, tag all faces inside the polylines.
 */
void EDBM_mesh_knife(ViewContext *vc,
                     blender::Span<Object *> objects,
                     LinkNode *polys,
                     bool use_tag,
                     bool cut_through);

wmKeyMap *knifetool_modal_keymap(wmKeyConfig *keyconf);

/* *** `editmesh_loopcut.cc` *** */

void MESH_OT_loopcut(wmOperatorType *ot);

/* *** `editmesh_rip.cc` *** */

void MESH_OT_rip(wmOperatorType *ot);
void MESH_OT_rip_edge(wmOperatorType *ot);

/* *** editmesh_select.cc *** */

void MESH_OT_select_similar(wmOperatorType *ot);
void MESH_OT_select_similar_region(wmOperatorType *ot);
void MESH_OT_select_mode(wmOperatorType *ot);
void MESH_OT_loop_multi_select(wmOperatorType *ot);
void MESH_OT_loop_select(wmOperatorType *ot);
void MESH_OT_edgering_select(wmOperatorType *ot);
void MESH_OT_select_all(wmOperatorType *ot);
void MESH_OT_select_interior_faces(wmOperatorType *ot);
void MESH_OT_shortest_path_pick(wmOperatorType *ot);
void MESH_OT_select_linked(wmOperatorType *ot);
void MESH_OT_select_linked_pick(wmOperatorType *ot);
void MESH_OT_select_face_by_sides(wmOperatorType *ot);
void MESH_OT_select_loose(wmOperatorType *ot);
void MESH_OT_select_mirror(wmOperatorType *ot);
void MESH_OT_select_more(wmOperatorType *ot);
void MESH_OT_select_less(wmOperatorType *ot);
void MESH_OT_select_nth(wmOperatorType *ot);
void MESH_OT_edges_select_sharp(wmOperatorType *ot);
void MESH_OT_faces_select_linked_flat(wmOperatorType *ot);
void MESH_OT_select_non_manifold(wmOperatorType *ot);
void MESH_OT_select_random(wmOperatorType *ot);
void MESH_OT_select_ungrouped(wmOperatorType *ot);
void MESH_OT_select_axis(wmOperatorType *ot);
void MESH_OT_region_to_loop(wmOperatorType *ot);
void MESH_OT_loop_to_region(wmOperatorType *ot);
void MESH_OT_select_by_attribute(wmOperatorType *ot);
void MESH_OT_shortest_path_select(wmOperatorType *ot);

extern EnumPropertyItem *corner_type_items;

/* *** editmesh_tools.cc *** */
void MESH_OT_subdivide(wmOperatorType *ot);
void MESH_OT_subdivide_edgering(wmOperatorType *ot);
void MESH_OT_unsubdivide(wmOperatorType *ot);
void MESH_OT_normals_make_consistent(wmOperatorType *ot);
void MESH_OT_vertices_smooth(wmOperatorType *ot);
void MESH_OT_vertices_smooth_laplacian(wmOperatorType *ot);
void MESH_OT_vert_connect(wmOperatorType *ot);
void MESH_OT_vert_connect_path(wmOperatorType *ot);
void MESH_OT_vert_connect_concave(wmOperatorType *ot);
void MESH_OT_vert_connect_nonplanar(wmOperatorType *ot);
void MESH_OT_face_make_planar(wmOperatorType *ot);
void MESH_OT_edge_split(wmOperatorType *ot);
void MESH_OT_bridge_edge_loops(wmOperatorType *ot);
void MESH_OT_offset_edge_loops(wmOperatorType *ot);
void MESH_OT_wireframe(wmOperatorType *ot);
void MESH_OT_convex_hull(wmOperatorType *ot);
void MESH_OT_symmetrize(wmOperatorType *ot);
void MESH_OT_symmetry_snap(wmOperatorType *ot);
void MESH_OT_shape_propagate_to_all(wmOperatorType *ot);
void MESH_OT_blend_from_shape(wmOperatorType *ot);
void MESH_OT_sort_elements(wmOperatorType *ot);
void MESH_OT_uvs_rotate(wmOperatorType *ot);
void MESH_OT_uvs_reverse(wmOperatorType *ot);
void MESH_OT_colors_rotate(wmOperatorType *ot);
void MESH_OT_colors_reverse(wmOperatorType *ot);
void MESH_OT_delete(wmOperatorType *ot);
void MESH_OT_delete_loose(wmOperatorType *ot);
void MESH_OT_edge_collapse(wmOperatorType *ot);
void MESH_OT_faces_shade_smooth(wmOperatorType *ot);
void MESH_OT_faces_shade_flat(wmOperatorType *ot);
void MESH_OT_split(wmOperatorType *ot);
void MESH_OT_edge_rotate(wmOperatorType *ot);
void MESH_OT_hide(wmOperatorType *ot);
void MESH_OT_reveal(wmOperatorType *ot);
void MESH_OT_mark_seam(wmOperatorType *ot);
void MESH_OT_mark_sharp(wmOperatorType *ot);
void MESH_OT_flip_normals(wmOperatorType *ot);
void MESH_OT_solidify(wmOperatorType *ot);
void MESH_OT_knife_cut(wmOperatorType *ot);
void MESH_OT_separate(wmOperatorType *ot);
void MESH_OT_fill(wmOperatorType *ot);
void MESH_OT_fill_grid(wmOperatorType *ot);
void MESH_OT_fill_holes(wmOperatorType *ot);
void MESH_OT_beautify_fill(wmOperatorType *ot);
void MESH_OT_quads_convert_to_tris(wmOperatorType *ot);
void MESH_OT_tris_convert_to_quads(wmOperatorType *ot);
void MESH_OT_decimate(wmOperatorType *ot);
void MESH_OT_dissolve_verts(wmOperatorType *ot);
void MESH_OT_dissolve_edges(wmOperatorType *ot);
void MESH_OT_dissolve_faces(wmOperatorType *ot);
void MESH_OT_dissolve_mode(wmOperatorType *ot);
void MESH_OT_dissolve_limited(wmOperatorType *ot);
void MESH_OT_dissolve_degenerate(wmOperatorType *ot);
void MESH_OT_delete_edgeloop(wmOperatorType *ot);
void MESH_OT_edge_face_add(wmOperatorType *ot);
void MESH_OT_duplicate(wmOperatorType *ot);
void MESH_OT_merge(wmOperatorType *ot);
void MESH_OT_remove_doubles(wmOperatorType *ot);
void MESH_OT_poke(wmOperatorType *ot);
void MESH_OT_point_normals(wmOperatorType *ot);
void MESH_OT_merge_normals(wmOperatorType *ot);
void MESH_OT_split_normals(wmOperatorType *ot);
void MESH_OT_normals_tools(wmOperatorType *ot);
void MESH_OT_set_normals_from_faces(wmOperatorType *ot);
void MESH_OT_average_normals(wmOperatorType *ot);
void MESH_OT_smooth_normals(wmOperatorType *ot);
void MESH_OT_mod_weighted_strength(wmOperatorType *ot);
void MESH_OT_flip_quad_tessellation(wmOperatorType *ot);

/* *** editmesh_mask_extract.cc *** */

void MESH_OT_paint_mask_extract(wmOperatorType *ot);
void MESH_OT_face_set_extract(wmOperatorType *ot);
void MESH_OT_paint_mask_slice(wmOperatorType *ot);

/** Called in `transform_ops.cc`, on each regeneration of key-maps. */
wmKeyMap *point_normals_modal_keymap(wmKeyConfig *keyconf);

#if defined(WITH_FREESTYLE)
void MESH_OT_mark_freestyle_edge(wmOperatorType *ot);
void MESH_OT_mark_freestyle_face(wmOperatorType *ot);
#endif

/* *** mesh_data.cc *** */

void MESH_OT_uv_texture_add(wmOperatorType *ot);
void MESH_OT_uv_texture_remove(wmOperatorType *ot);
void MESH_OT_customdata_mask_clear(wmOperatorType *ot);
void MESH_OT_customdata_skin_add(wmOperatorType *ot);
void MESH_OT_customdata_skin_clear(wmOperatorType *ot);
void MESH_OT_customdata_custom_splitnormals_add(wmOperatorType *ot);
void MESH_OT_customdata_custom_splitnormals_clear(wmOperatorType *ot);
