/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 */

#pragma once

#include "BLI_enum_flags.hh"

#include "bmesh_class.hh"

struct BMEditSelection {
  struct BMEditSelection *next, *prev;
  BMElem *ele;
  char htype;
};

enum class BMSelectFlushFlag : uint8_t {
  None = 0,
  RecalcLenVert = (1 << 0),
  RecalcLenEdge = (1 << 1),
  RecalcLenFace = (1 << 2),
  /**
   * Flush selection down, depending on the selection mode.
   *
   * Disabled by default as edge & face selection functions flush down:
   * (functions #BM_edge_select_set & #BM_face_select_set).
   * However selection logic needs to take care to perform de-selection *before* selection,
   * otherwise flushing down *is* needed.
   */
  Down = (1 << 3),
};
ENUM_OPERATORS(BMSelectFlushFlag)

#define BMSelectFlushFlag_All \
  (BMSelectFlushFlag::RecalcLenVert | BMSelectFlushFlag::RecalcLenEdge | \
   BMSelectFlushFlag::RecalcLenFace)

#define BMSelectFlushFlag_Default BMSelectFlushFlag_All

/* Geometry hiding code. */

#define BM_elem_hide_set(bm, ele, hide) _bm_elem_hide_set(bm, &(ele)->head, hide)
void _bm_elem_hide_set(BMesh *bm, BMHeader *head, bool hide);
void BM_vert_hide_set(BMVert *v, bool hide);
void BM_edge_hide_set(BMEdge *e, bool hide);
void BM_face_hide_set(BMFace *f, bool hide);

/* Selection code. */

/**
 * \note use BM_elem_flag_test(ele, BM_ELEM_SELECT) to test selection
 * \note by design, this will not touch the editselection history stuff
 */
void BM_elem_select_set(BMesh *bm, BMElem *ele, bool select);

void BM_mesh_elem_hflag_enable_test(
    BMesh *bm, char htype, char hflag, bool respecthide, bool overwrite, char hflag_test);
void BM_mesh_elem_hflag_disable_test(
    BMesh *bm, char htype, char hflag, bool respecthide, bool overwrite, char hflag_test);

void BM_mesh_elem_hflag_enable_all(BMesh *bm, char htype, char hflag, bool respecthide);
void BM_mesh_elem_hflag_disable_all(BMesh *bm, char htype, char hflag, bool respecthide);

/* Individual element select functions, #BM_elem_select_set is a shortcut for these
 * that automatically detects which one to use. */

/**
 * \brief Select Vert
 *
 * Changes selection state of a single vertex
 * in a mesh
 */
void BM_vert_select_set(BMesh *bm, BMVert *v, bool select);
/**
 * \brief Select Edge
 *
 * Changes selection state of a single edge in a mesh.
 */
void BM_edge_select_set(BMesh *bm, BMEdge *e, bool select);
/**
 * \brief Select Face
 *
 * Changes selection state of a single
 * face in a mesh.
 */
void BM_face_select_set(BMesh *bm, BMFace *f, bool select);

/* Lower level functions which don't do flushing. */

void BM_edge_select_set_noflush(BMesh *bm, BMEdge *e, bool select);
void BM_face_select_set_noflush(BMesh *bm, BMFace *f, bool select);

/**
 * Return true when there are a mix of selected/unselected elements.
 */
bool BM_mesh_select_is_mixed(const BMesh *bm);

/**
 * \brief Select Mode Clean
 *
 * Remove isolated selected elements when in a mode doesn't support them.
 * eg: in edge-mode a selected vertex must be connected to a selected edge.
 *
 * \note this could be made a part of #BM_mesh_select_mode_flush_ex
 */
void BM_mesh_select_mode_clean_ex(BMesh *bm, short selectmode);
void BM_mesh_select_mode_clean(BMesh *bm);

/**
 * Select Mode Set
 *
 * Sets the selection mode for the bmesh,
 * updating the selection state.
 */
void BM_mesh_select_mode_set(BMesh *bm, int selectmode);
/**
 * \brief Select Mode Flush
 *
 * Makes sure to flush selections 'upwards'
 * (ie: all verts of an edge selects the edge and so on).
 * This should only be called by system and not tool authors.
 *
 * \note Flushing down can be enabled for edge/face modes
 * by enabling #BMSelectFlushFlag:Down for `flag`.
 */
void BM_mesh_select_mode_flush_ex(BMesh *bm, short selectmode, BMSelectFlushFlag flag);
void BM_mesh_select_mode_flush(BMesh *bm);

/**
 * Mode independent selection/de-selection flush from vertices.
 *
 * \param select: When true, flush the selection state to de-selected elements,
 * otherwise perform the opposite, flushing de-selection.
 */
void BM_mesh_select_flush_from_verts(BMesh *bm, bool select);

int BM_mesh_elem_hflag_count_enabled(BMesh *bm, char htype, char hflag, bool respecthide);
int BM_mesh_elem_hflag_count_disabled(BMesh *bm, char htype, char hflag, bool respecthide);

/* Edit selection stuff. */

void BM_mesh_active_face_set(BMesh *bm, BMFace *f);
int BM_mesh_active_face_index_get(BMesh *bm, bool is_sloppy, bool is_selected);
int BM_mesh_active_edge_index_get(BMesh *bm);
int BM_mesh_active_vert_index_get(BMesh *bm);

BMFace *BM_mesh_active_face_get(BMesh *bm, bool is_sloppy, bool is_selected);
BMEdge *BM_mesh_active_edge_get(BMesh *bm);
BMVert *BM_mesh_active_vert_get(BMesh *bm);
BMElem *BM_mesh_active_elem_get(BMesh *bm);

/**
 * Generic way to get data from an #BMEditSelection type
 * These functions were written to be used by the Modifier widget
 * when in Rotate about active mode, but can be used anywhere.
 *
 * - #BM_editselection_center
 * - #BM_editselection_normal
 * - #BM_editselection_plane
 */
void BM_editselection_center(BMEditSelection *ese, float r_center[3]);
void BM_editselection_normal(BMEditSelection *ese, float r_normal[3]);
/**
 * Calculate a plane that is right angles to the edge/vert/faces normal
 * also make the plane run along an axis that is related to the geometry,
 * because this is used for the gizmos Y axis.
 */
void BM_editselection_plane(BMEditSelection *ese, float r_plane[3]);

#define BM_select_history_check(bm, ele) _bm_select_history_check(bm, &(ele)->head)
#define BM_select_history_remove(bm, ele) _bm_select_history_remove(bm, &(ele)->head)
#define BM_select_history_store_notest(bm, ele) _bm_select_history_store_notest(bm, &(ele)->head)
#define BM_select_history_store(bm, ele) _bm_select_history_store(bm, &(ele)->head)
#define BM_select_history_store_head_notest(bm, ele) \
  _bm_select_history_store_head_notest(bm, &(ele)->head)
#define BM_select_history_store_head(bm, ele) _bm_select_history_store_head(bm, &(ele)->head)
#define BM_select_history_store_after_notest(bm, ese_ref, ele) \
  _bm_select_history_store_after_notest(bm, ese_ref, &(ele)->head)
#define BM_select_history_store_after(bm, ese, ese_ref) \
  _bm_select_history_store_after(bm, ese_ref, &(ele)->head)

bool _bm_select_history_check(BMesh *bm, const BMHeader *ele);
bool _bm_select_history_remove(BMesh *bm, BMHeader *ele);
void _bm_select_history_store_notest(BMesh *bm, BMHeader *ele);
void _bm_select_history_store(BMesh *bm, BMHeader *ele);
void _bm_select_history_store_head_notest(BMesh *bm, BMHeader *ele);
void _bm_select_history_store_head(BMesh *bm, BMHeader *ele);
void _bm_select_history_store_after(BMesh *bm, BMEditSelection *ese_ref, BMHeader *ele);
void _bm_select_history_store_after_notest(BMesh *bm, BMEditSelection *ese_ref, BMHeader *ele);

void BM_select_history_validate(BMesh *bm);
void BM_select_history_clear(BMesh *bm);
/**
 * Get all element types present in a selection history.
 */
[[nodiscard]] char BM_select_history_htype_all(const BMesh *bm);
/**
 * Get the active mesh element (with active-face fallback).
 */
bool BM_select_history_active_get(BMesh *bm, struct BMEditSelection *ese);
/**
 * Return a map from #BMVert/#BMEdge/#BMFace -> #BMEditSelection.
 */
struct GHash *BM_select_history_map_create(BMesh *bm);

/**
 * Map arguments may all be the same pointer.
 */
void BM_select_history_merge_from_targetmap(
    BMesh *bm, GHash *vert_map, GHash *edge_map, GHash *face_map, bool use_chain);

#define BM_SELECT_HISTORY_BACKUP(bm) \
  { \
    ListBase _bm_prev_selected = (bm)->selected; \
    BLI_listbase_clear(&(bm)->selected)

#define BM_SELECT_HISTORY_RESTORE(bm) \
  (bm)->selected = _bm_prev_selected; \
  } \
  (void)0
