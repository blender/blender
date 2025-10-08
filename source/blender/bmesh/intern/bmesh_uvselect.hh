/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "bmesh_class.hh"

#include "BLI_vector_list.hh"

/** \file
 * \ingroup bmesh
 *
 * Overview
 * ========
 *
 * The `BM_uvselect_*` API deals with synchronizing selection
 * between UV's and selected vertices edges & faces,
 * where a selected vertex in the 3D viewport may only have some of its
 * UV vertices selected in the UV editor.
 *
 * Supporting this involves flushing in both directions depending on the selection being edited.
 *
 * \note See #78393 for a user-level overview of this functionality.
 * This describes the motivation to synchronize selection between UV's and the mesh.
 *
 * \note A short-hand term for vertex/edge/face selection used
 * in this file is View3D abbreviated to `v3d`, since this is the section
 * manipulated in the viewport, e.g. #BM_mesh_uvselect_sync_to_mesh.
 *
 * \note This is quite involved. As a last resort the UV selection can always be cleared
 * and re-set from the mesh (v3d) selection, however it's good to keep UV selection
 * if possible because resetting may extend vertex selection to other UV islands.
 *
 * Terms
 * =====
 *
 * - Synchronized Selection (abbreviated to "sync"). See #BMesh::uv_select_sync_valid.
 *   When the UV synchronized data is valid, it means there is a valid relationship
 *   between the UV selection flags (#BM_ELEM_SELECT_UV & #BM_ELEM_SELECT_UV_EDGE)
 *   and the meshes selection (#BM_ELEM_SELECT_UV).
 *
 *   - When the UV selection changes (from the UV editor)
 *     this needs to be synchronized to the mesh.
 *   - When the base-selection flags change (from the 3D viewport)
 *     this needs to be synchronized to the UV's.
 *     Synchronizing in this direction may be lossy, although (depending on the operation),
 *     support for maintaining a synchronized selection may be possible.
 *
 * - Flushing Selection ("flush")
 *   When an element is selected or de-selected, the selection state
 *   of connected geometry may change too.
 *   So, de-selecting a vertex must de-select all faces that use that vertex.
 *
 *   The rules for flushing may depend on the selection mode.
 *   When de-selecting a face in vertex-select-mode, all its vertices & edges
 *   must also be de-selected. When de-selecting a face in face-select-mode,
 *   only vertices and edges no longer connected to any selected faces will be de-selected.
 *
 *   Since applying these rules while selecting individual elements is often impractical,
 *   it's common to adjust the selection, then flush based on the selection mode afterwards.
 *
 * - Flushing up:
 *   Flushing the selection from [verts -> edges/faces], [edges -> faces].
 * - Flushing down:
 *   Flushing the selection from [faces -> verts/edges], [edges -> verts].
 *
 * - Isolated vertex or edge selection:
 *   When a vertex or edge is selected without being connected to a selected face.
 *
 * UV Selection Flags
 * ==================
 *
 * - UV selection uses:
 *   - #BM_ELEM_SELECT_UV & #BM_ELEM_SELECT_UV_EDGE for #BMLoop
 *     to define selected vertices & edges.
 *   - #BM_ELEM_SELECT_UV for #BMFace.
 *
 * Hidden Flags
 * ============
 *
 * Unlike viewport selection there is no requirement for hidden elements not to be selected.
 * Therefor, UV selection checks must check the underlying geometry is not hidden.
 * In practice this means hidden faces must be assumed unselected,
 * since UV's are part of the faces (there is no such thing as a hidden face-corner)
 * and any hidden edge or vertex causes connected faces to be hidden.
 *
 * UV Selection Flushing
 * =====================
 *
 * Selection setting functions flush down (unless the `_noflush(...)` version is used),
 * this means selecting a face also selects all verts & edges,
 * selecting an edge selects its vertices.
 *
 * However it's expected the selection is flushed,
 * de-selecting a vertex or edge must de-select it's faces (flushing up).
 * For this, there are various flushing functions,
 * exactly what is needed depends on the selection operation performed and the selection mode.
 *
 * There are also situations that shouldn't be allowed such as a single selected vertex in face
 * select mode.
 *
 * Flushing & Synchronizing
 * ========================
 *
 * Properly handling the selection state is important for operators that adjust the UV selection.
 * This typically involves the following steps:
 *
 * - The UV selection changes.
 * - The UV selection must be flushed between elements to ensure the selection is valid,
 *   (see: `BM_mesh_uvselect_flush_*` & `BM_mesh_uvselect_mode_flush_*` functions).
 * - The UV selection must be synchronized to the mesh selection
 *   (see #BM_mesh_uvselect_sync_to_mesh).
 * - The mesh must then flush selection to its elements
 *   (see: `BM_mesh_select_flush_*` & `BM_mesh_select_mode_flush_*` functions).
 *
 * Valid State
 * ===========
 *
 * For a valid state:
 * - A selected UV-vertex must have its underlying mesh vertex selected.
 * - A selected mesh-vertex must have at least one UV-vertex selected.
 *
 * This is *mostly* true for edges/faces too, however there cases where
 * the UV selection causes an edge/face to be selected in mesh space but not UV space.
 *
 * See #BM_mesh_uvselect_is_valid for details.
 *
 * Clearing the Valid State
 * ========================
 *
 * As already noted, tools should maintain the synchronized UV selection where possible.
 * However when this information *isn't* needed it should be cleared aggressively
 * (see #BM_mesh_uvselect_clear), since it adds both computation & memory overhead.
 *
 * For actions that overwrite the selection such as selecting or de-selecting all,
 * it's safe to "clear" the data, other actions such as adding new geometry that replaces
 * the selection can also safely "clear" the UV selection.
 *
 * In practice users modeling in the 3D viewport are likely to clear the UV selection data
 * since selecting the mesh without extending the selection is effectively a "De-select All".
 * So the chances this data persists when it's not needed over many editing operations are low.
 */

/* -------------------------------------------------------------------- */
/** \name UV Selection Functions (low level)
 *
 * Selection checking functions.
 * These should be used instead of checking #BM_ELEM_SELECT_UV,
 * so hidden geometry is never considered selected.
 * \{ */

bool BM_face_uvselect_test(const BMFace *f);
bool BM_loop_vert_uvselect_test(const BMLoop *l);
bool BM_loop_edge_uvselect_test(const BMLoop *l);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Connectivity Checks
 *
 * Regarding the `hflag` parameter: this is typically set to:
 * - #BM_ELEM_SELECT for mesh selection.
 * - #BM_ELEM_SELECT_UV for selected UV vertices.
 * - #BM_ELEM_SELECT_UV_EDGE for selected UV edges.
 * - #BM_ELEM_SELECT_TAG to allow the caller to use a separate non-selection flag.
 *
 * Each function asserts that a supported `hflag` is passed in.
 * \{ */

bool BM_loop_vert_uvselect_check_other_loop_vert(BMLoop *l, char hflag, int cd_loop_uv_offset);
bool BM_loop_vert_uvselect_check_other_loop_edge(BMLoop *l, char hflag, int cd_loop_uv_offset);
bool BM_loop_vert_uvselect_check_other_edge(BMLoop *l, char hflag, int cd_loop_uv_offset);
bool BM_loop_vert_uvselect_check_other_face(BMLoop *l, char hflag, int cd_loop_uv_offset);
bool BM_loop_edge_uvselect_check_other_loop_edge(BMLoop *l, char hflag, int cd_loop_uv_offset);
bool BM_loop_edge_uvselect_check_other_face(BMLoop *l, char hflag, int cd_loop_uv_offset);

bool BM_face_uvselect_check_edges_all(BMFace *f);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Functions
 * \{ */

/** Set the UV selection flag for `f` without flushing down to edges & vertices. */
void BM_face_uvselect_set_noflush(BMesh *bm, BMFace *f, bool select);
/** Set the UV selection flag for `f` & flush down to edges & vertices. */
void BM_face_uvselect_set(BMesh *bm, BMFace *f, bool select);

/** Set the UV selection flag for `e` without flushing down to vertices. */
void BM_loop_edge_uvselect_set_noflush(BMesh *bm, BMLoop *l, bool select);
/** Set the UV selection flag for `e` & flush down to vertices. */
void BM_loop_edge_uvselect_set(BMesh *bm, BMLoop *l, bool select);
/**
 * Set the UV selection flag for `v` without flushing down.
 * since there is nothing to flush down to.
 */
void BM_loop_vert_uvselect_set_noflush(BMesh *bm, BMLoop *l, bool select);

/**
 * Call this function when selecting mesh elements in the viewport and
 * the relationship with UV's is lost.
 *
 * \return True if UV select is cleared (a change was made).
 *
 * This has two purposes:
 *
 * - Maintaining the UV selection isn't needed:
 *   Some operations such as adding a new mesh primitive clear the selection,
 *   selecting all geometry from the new primitive.
 *   In this case a UV selection is redundant & should be cleared.
 *
 * - Maintaining the UV selection isn't supported:
 *   Some selection operations don't support maintaining a valid UV selection,
 *   in that case it's necessary to clear the UV selection otherwise tools may
 *   seem to be broken if they aren't operating on the selection properly.
 *
 *   NOTE(@ideasman42): It's worth noting that in this case clearing the selection is "lossy",
 *   users may wish that all selection operations would handle UV selection data too.
 *   Supporting additional operations is always possible, at the time of writing it's
 *   impractical to do so, see: #131642 design task for details.
 *
 * Internally this marks the UV selection data as invalid,
 * using the mesh selection as the "source-of-truth".
 *
 * \note By convention call this immediately after flushing.
 *
 * \note In many cases the UV selection can be maintained and this function removed,
 * although it adds some complexity & overhead.
 * See #UVSyncSelectFromMesh.
 *
 * \note Calls to this function that should *not* be removed in favor of supporting UV selection,
 * this should be mentioned in a code-comment, making it clear this is not a limitation to *fix*.
 */
bool BM_mesh_uvselect_clear(BMesh *bm);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Functions (Shared)
 * \{ */

void BM_loop_vert_uvselect_set_shared(BMesh *bm, BMLoop *l, bool select, int cd_loop_uv_offset);
void BM_loop_edge_uvselect_set_shared(BMesh *bm, BMLoop *l, bool select, int cd_loop_uv_offset);
void BM_face_uvselect_set_shared(BMesh *bm, BMFace *f, bool select, int cd_loop_uv_offset);

void BM_mesh_uvselect_set_elem_shared(BMesh *bm,
                                      bool select,
                                      int cd_loop_uv_offset,
                                      blender::Span<BMLoop *> loop_verts,
                                      blender::Span<BMLoop *> loop_edges,
                                      blender::Span<BMFace *> faces);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Picking
 * \{ */

struct BMUVSelectPickParams {
  /**
   * The custom data offset for the active UV layer.
   * May be -1, in this case UV connectivity checks are skipped.
   */
  int cd_loop_uv_offset = -1;
  /**
   * If true, selection changes propagate to all other UV elements
   * that share the same UV coordinates (contiguous selection).
   *
   * Typically derived from #ToolSettings::uv_sticky, although in some cases
   * it's assumed to be true (when switching selection modes for example)
   * because the tool settings aren't available in that context.
   *
   * A boolean can be used since "Shared Vertex" (uv_sticky mode)
   * can check the meshes vertex selection directly.
   */
  bool shared = true;
};

void BM_vert_uvselect_set_pick(BMesh *bm,
                               BMVert *v,
                               bool select,
                               const BMUVSelectPickParams &params);
void BM_edge_uvselect_set_pick(BMesh *bm,
                               BMEdge *e,
                               bool select,
                               const BMUVSelectPickParams &params);
void BM_face_uvselect_set_pick(BMesh *bm,
                               BMFace *f,
                               bool select,
                               const BMUVSelectPickParams &params);

/**
 * Select/deselect elements in the viewport,
 * then integrate the selection with the UV selection,
 * without clearing an re-initializing the synchronized state.
 * (likely to re-select islands bounds from a user-perspective).
 */
void BM_mesh_uvselect_set_elem_from_mesh(BMesh *bm,
                                         bool select,
                                         const BMUVSelectPickParams &params,
                                         blender::Span<BMVert *> verts,
                                         blender::Span<BMEdge *> edges,
                                         blender::Span<BMFace *> faces);
/** \copydoc #BM_mesh_uvselect_set_elem_from_mesh. */
void BM_mesh_uvselect_set_elem_from_mesh(BMesh *bm,
                                         bool select,
                                         const BMUVSelectPickParams &params,
                                         const blender::VectorList<BMVert *> &verts,
                                         const blender::VectorList<BMEdge *> &edges,
                                         const blender::VectorList<BMFace *> &faces);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Flushing (Only Select/De-Select)
 *
 * \note In most cases flushing assumes selection has already been flushed down.
 *
 * This means:
 * - A selected edge must have both UV vertices selected.
 * - A selected faces has all its edges & vertices selected.
 *
 * It's often useful to call #BM_mesh_uvselect_flush_shared_only_select
 * after using these non-UV-coordinate aware flushing functions.
 * \{ */

void BM_mesh_uvselect_flush_from_loop_verts_only_select(BMesh *bm);
void BM_mesh_uvselect_flush_from_loop_verts_only_deselect(BMesh *bm);
void BM_mesh_uvselect_flush_from_loop_edges_only_select(BMesh *bm);
void BM_mesh_uvselect_flush_from_loop_edges_only_deselect(BMesh *bm);
void BM_mesh_uvselect_flush_from_faces_only_select(BMesh *bm);
void BM_mesh_uvselect_flush_from_faces_only_deselect(BMesh *bm);

/**
 * A useful utility so simple selection operations can be performed on edges/faces,
 * afterwards this can be used to select UV's that are connected.
 * This avoids having to use more involved UV connectivity aware logic inline.
 */
void BM_mesh_uvselect_flush_shared_only_select(BMesh *bm, int cd_loop_uv_offset);
void BM_mesh_uvselect_flush_shared_only_deselect(BMesh *bm, int cd_loop_uv_offset);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Flushing (Between Elements)
 *
 * Regarding the `flush_down` argument.
 *
 * Primitive UV selection functions always flush down:
 * - #BM_face_uvselect_set
 * - #BM_loop_edge_uvselect_set
 * - #BM_loop_vert_uvselect_set
 *
 * This means it's often only necessary to flush up after the selection has been changed.
 * \{ */

/**
 * Mode independent UV selection/de-selection flush from UV vertices.
 *
 * \param select: When true, flush the selection state to de-selected elements,
 * otherwise perform the opposite, flushing de-selection.
 *
 * \note The caller may need to run #BM_mesh_uvselect_flush_shared_only_select afterwards.
 */
void BM_mesh_uvselect_flush_from_loop_verts(BMesh *bm);
/**
 * Mode independent UV selection/de-selection flush from UV edges.
 *
 * Flush from loop edges up to faces and optionally down to vertices (when `flush_down` is true).
 *
 * \note The caller may need to run #BM_mesh_uvselect_flush_shared_only_select afterwards.
 */
void BM_mesh_uvselect_flush_from_loop_edges(BMesh *bm, bool flush_down);
/**
 * Mode independent UV selection/de-selection flush from UV faces.
 *
 * Flush from faces down to edges & vertices (when `flush_down` is true).
 *
 * \note The caller may need to run #BM_mesh_uvselect_flush_shared_only_select afterwards.
 */
void BM_mesh_uvselect_flush_from_faces(BMesh *bm, bool flush_down);

/**
 * Mode independent UV selection/de-selection flush from UV vertices.
 *
 * Use this when it's know geometry was only selected/de-selected.
 *
 * \note An equivalent to #BM_mesh_select_flush_from_verts for the UV selection.
 */
void BM_mesh_uvselect_flush_from_verts(BMesh *bm, bool select);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Flushing (Selection Mode Aware)
 * \{ */

/**
 * \param flush_down: See #BMSelectFlushFlag::Down for notes on flushing down.
 */
void BM_mesh_uvselect_mode_flush_ex(BMesh *bm, const short selectmode, bool flush_down);
void BM_mesh_uvselect_mode_flush(BMesh *bm);

/**
 * Select elements based on the selection mode.
 * (flushes the selection *up* based on the mode).
 *
 * - With vertex selection mode enabled: flush up to edges and faces.
 * - With edge selection mode enabled: flush to faces.
 * - With *only* face selection mode enabled: do nothing.
 *
 * \note An "only deselect" version function could be added, it's not needed at the moment.a
 */
void BM_mesh_uvselect_mode_flush_only_select(BMesh *bm);

/**
 * When the select mode changes, update to ensure the selection is valid.
 * So single vertices aren't selected in edge-select mode for example.
 *
 * The mesh selection flushing must have already run.
 */
void BM_mesh_uvselect_mode_flush_update(BMesh *bm,
                                        short selectmode_old,
                                        short selectmode_new,
                                        int cd_loop_uv_offset);

/**
 * A specialized flushing that fills in selection information after subdividing.
 *
 * It's important this runs:
 * - After subdivision.
 * - After the mesh selection has already been flushed.
 *
 * \note Intended to be a generic utility to be used in any situation
 * new geometry is created by splitting existing geometry.
 */
void BM_mesh_uvselect_flush_post_subdivide(BMesh *bm, int cd_loop_uv_offset);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Flushing (From/To Mesh)
 * \{ */

/* From 3D viewport to UV selection.
 *
 * These functions correspond to #ToolSettings::uv_sticky options. */

void BM_mesh_uvselect_sync_from_mesh_sticky_location(BMesh *bm, int cd_loop_uv_offset);
void BM_mesh_uvselect_sync_from_mesh_sticky_disabled(BMesh *bm);
void BM_mesh_uvselect_sync_from_mesh_sticky_vert(BMesh *bm);

/**
 * Synchronize selection: from the UV selection to the 3D viewport.
 *
 * \note #BMesh::uv_select_sync_valid must be true.
 */
void BM_mesh_uvselect_sync_to_mesh(BMesh *bm);

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Validation
 * \{ */

/**
 * UV/Mesh Synchronization
 *
 * Check the selection has been properly synchronized between the mesh and the UV's.
 *
 * \note It is essential for this to be correct and return no errors.
 * Other checks are useful to ensure the selection state meets the expectations of the caller
 * but the state is not invalid - as it is when the selection is out-of-sync.
 */
struct UVSelectValidateInfo_Sync {
  /** When a vertex is unselected none of it's UV's may be selected. */
  int count_uv_vert_any_selected_with_vert_unselected = 0;
  /** When a vertex is selected at least one UV must be selected. */
  int count_uv_vert_none_selected_with_vert_selected = 0;

  /** When a edge is unselected none of it's UV's may be selected. */
  int count_uv_edge_any_selected_with_edge_unselected = 0;
  /** When a edge is selected at least one UV must be selected. */
  int count_uv_edge_none_selected_with_edge_selected = 0;
};

/**
 * Flushing between elements.
 *
 * Check the selection has been properly flushing between elements.
 */
struct UVSelectValidateInfo_Flush {
  /** Edges are selected without selected vertices. */
  int count_uv_edge_selected_with_any_verts_unselected = 0;
  /** Edges are unselected with all selected vertices. */
  int count_uv_edge_unselected_with_all_verts_selected = 0;

  /** Faces are selected without selected vertices. */
  int count_uv_face_selected_with_any_verts_unselected = 0;
  /** Faces  are unselected with all selected vertices. */
  int count_uv_face_unselected_with_all_verts_selected = 0;

  /** Faces are selected without selected edges. */
  int count_uv_face_selected_with_any_edges_unselected = 0;
  /** Faces  are unselected with all selected edges. */
  int count_uv_face_unselected_with_all_edges_selected = 0;
};

/**
 * Contiguous.
 *
 * Check the selected UV's are contiguous,
 * in situations where it's expected selecting a UV will select all "connected" UV's
 * (UV's sharing the same vertex with the same UV coordinate).
 */
struct UVSelectValidateInfo_Contiguous {
  /** When a vertices connected UV's are co-located without matching selection. */
  int count_uv_vert_non_contiguous_selected = 0;
  /** When a edges connected UV's are co-located without matching selection. */
  int count_uv_edge_non_contiguous_selected = 0;
};

/**
 * Flush & contiguous.
 *
 * In some cases it's necessary to check flushing and contiguous UV's are correct.
 */
struct UVSelectValidateInfo_FlushAndContiguous {
  /** A vertex is selected in edge/face modes without being part of a selected edge/face. */
  int count_uv_vert_isolated_in_edge_or_face_mode = 0;
  /** A vertex is selected in face modes without being part of a selected face. */
  int count_uv_vert_isolated_in_face_mode = 0;
  /** An edge is selected in face modes without being part of a selected face. */
  int count_uv_edge_isolated_in_face_mode = 0;
};

struct UVSelectValidateInfo {
  UVSelectValidateInfo_Sync sync;

  UVSelectValidateInfo_Flush flush;
  UVSelectValidateInfo_Contiguous contiguous;
  UVSelectValidateInfo_FlushAndContiguous flush_contiguous;
};

/**
 * Check the UV selection is valid, mainly for debugging & testing purposes.
 *
 * The primary check which should remain valid is: `check_sync`,
 * if there is ever a selected vertex without any selected UV's or a selected
 * UV without it's vertex being selected (and similar kinds of issues),
 * then the selection is out-of-sync, which Blender should *never* allow.
 *
 * While an invalid selection should not crash, tools that operate on selection
 * may behave unpredictably.
 *
 * The other checks may be desired or not although this depends more on the situation.
 *
 * \param cd_loop_uv_offset: The UV custom-data layer to check.
 * Ignored when -1 (UV checks wont be used).
 *
 * \param check_sync: When true, check the selection is synchronized
 * between the UV and mesh selection. This should practically always be true,
 * as it doesn't make sense to check the UV selection if valid otherwise,
 * unless the UV selection is being set and has not yet been synchronized.
 * \param check_flush: When true, check the selection is flushed based on #BMesh::selectmode.
 * \param check_contiguous: When true, check that UV selection is contiguous.
 * Note that this is not considered an *error* since users may cause this to happen and
 * tools are expected to work properly, however some operations are expected to maintain
 * a contiguous selection. This check is included to ensure those operations are working.
 */
bool BM_mesh_uvselect_is_valid(BMesh *bm,
                               int cd_loop_uv_offset,
                               bool check_sync,
                               bool check_flush,
                               bool check_contiguous,
                               UVSelectValidateInfo *info);

/** \} */
