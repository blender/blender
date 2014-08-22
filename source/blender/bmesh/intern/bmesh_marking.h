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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_MARKING_H__
#define __BMESH_MARKING_H__

/** \file blender/bmesh/intern/bmesh_marking.h
 *  \ingroup bmesh
 */

typedef struct BMEditSelection {
	struct BMEditSelection *next, *prev;
	BMElem *ele;
	char htype;
} BMEditSelection;

/* geometry hiding code */
#define BM_elem_hide_set(bm, ele, hide) _bm_elem_hide_set(bm, &(ele)->head, hide)
void _bm_elem_hide_set(BMesh *bm, BMHeader *ele, const bool hide);
void BM_vert_hide_set(BMVert *v, const bool hide);
void BM_edge_hide_set(BMEdge *e, const bool hide);
void BM_face_hide_set(BMFace *f, const bool hide);

/* Selection code */
void BM_elem_select_set(BMesh *bm, BMElem *ele, const bool select);

void BM_mesh_elem_hflag_enable_test(BMesh *bm, const char htype, const char hflag,
                                    const bool respecthide, const bool overwrite, const char hflag_test);
void BM_mesh_elem_hflag_disable_test(BMesh *bm, const char htype, const char hflag,
                                     const bool respecthide, const bool overwrite, const char hflag_test);

void BM_mesh_elem_hflag_enable_all(BMesh *bm, const char htype, const char hflag,
                                   const bool respecthide);
void BM_mesh_elem_hflag_disable_all(BMesh *bm, const char htype, const char hflag,
                                    const bool respecthide);

/* individual element select functions, BM_elem_select_set is a shortcut for these
 * that automatically detects which one to use*/
void BM_vert_select_set(BMesh *bm, BMVert *v, const bool select);
void BM_edge_select_set(BMesh *bm, BMEdge *e, const bool select);
void BM_face_select_set(BMesh *bm, BMFace *f, const bool select);

void BM_mesh_select_mode_clean_ex(BMesh *bm, const short selectmode);
void BM_mesh_select_mode_clean(BMesh *bm);

void BM_mesh_select_mode_set(BMesh *bm, int selectmode);
void BM_mesh_select_mode_flush_ex(BMesh *bm, const short selectmode);
void BM_mesh_select_mode_flush(BMesh *bm);

void BM_mesh_deselect_flush(BMesh *bm);
void BM_mesh_select_flush(BMesh *bm);

int BM_mesh_elem_hflag_count_enabled(BMesh *bm, const char htype, const char hflag, const bool respecthide);
int BM_mesh_elem_hflag_count_disabled(BMesh *bm, const char htype, const char hflag, const bool respecthide);

/* edit selection stuff */
void    BM_mesh_active_face_set(BMesh *bm, BMFace *f);
BMFace *BM_mesh_active_face_get(BMesh *bm, const bool is_sloppy, const bool is_selected);
BMEdge *BM_mesh_active_edge_get(BMesh *bm);
BMVert *BM_mesh_active_vert_get(BMesh *bm);
BMElem *BM_mesh_active_elem_get(BMesh *bm);

void    BM_editselection_center(BMEditSelection *ese, float r_center[3]);
void    BM_editselection_normal(BMEditSelection *ese, float r_normal[3]);
void    BM_editselection_plane(BMEditSelection *ese,  float r_plane[3]);

#define BM_select_history_check(bm, ele)        _bm_select_history_check(bm,        &(ele)->head)
#define BM_select_history_remove(bm, ele)       _bm_select_history_remove(bm,       &(ele)->head)
#define BM_select_history_store_notest(bm, ele) _bm_select_history_store_notest(bm, &(ele)->head)
#define BM_select_history_store(bm, ele)        _bm_select_history_store(bm,        &(ele)->head)
#define BM_select_history_store_after_notest(bm, ese_ref, ele) _bm_select_history_store_after_notest(bm, ese_ref, &(ele)->head)
#define BM_select_history_store_after(bm, ese, ese_ref)        _bm_select_history_store_after(bm,        ese_ref, &(ele)->head)

bool _bm_select_history_check(BMesh *bm,  const BMHeader *ele);
bool _bm_select_history_remove(BMesh *bm,       BMHeader *ele);
void _bm_select_history_store_notest(BMesh *bm, BMHeader *ele);
void _bm_select_history_store(BMesh *bm,        BMHeader *ele);
void _bm_select_history_store_after(BMesh *bm,  BMEditSelection *ese_ref, BMHeader *ele);
void _bm_select_history_store_after_notest(BMesh *bm,  BMEditSelection *ese_ref, BMHeader *ele);

void BM_select_history_validate(BMesh *bm);
void BM_select_history_clear(BMesh *bm);
bool BM_select_history_active_get(BMesh *bm, struct BMEditSelection *ese);

#endif /* __BMESH_MARKING_H__ */
