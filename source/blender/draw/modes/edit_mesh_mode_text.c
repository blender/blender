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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_unit.h"

#include "ED_view3d.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "UI_resources.h"

#include "draw_manager_text.h"

#include "edit_mesh_mode_intern.h" /* own include */

/* Copied from drawobject.c */
void DRW_edit_mesh_mode_text_measure_stats(ARegion *ar,
                                           View3D *v3d,
                                           Object *ob,
                                           const UnitSettings *unit)
{
  /* Do not use ascii when using non-default unit system, some unit chars are utf8 (micro, square,
   * etc.). See bug #36090.
   */
  struct DRWTextStore *dt = DRW_text_cache_ensure();
  const short txt_flag = DRW_TEXT_CACHE_GLOBALSPACE | (unit->system ? 0 : DRW_TEXT_CACHE_ASCII);
  Mesh *me = ob->data;
  BMEditMesh *em = me->edit_mesh;
  float v1[3], v2[3], v3[3], vmid[3], fvec[3];
  char numstr[32]; /* Stores the measurement display text here */
  size_t numstr_len;
  const char *conv_float;        /* Use a float conversion matching the grid size */
  uchar col[4] = {0, 0, 0, 255}; /* color of the text to draw */
  float area;                    /* area of the face */
  float grid = unit->system ? unit->scale_length : v3d->grid;
  const bool do_global = (v3d->flag & V3D_GLOBAL_STATS) != 0;
  const bool do_moving = (G.moving & G_TRANSFORM_EDIT) != 0;
  /* when 2 edge-info options are enabled, space apart */
  const bool do_edge_textpair = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_LEN) &&
                                (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_ANG);
  const short edge_texpair_sep = (short)(5.0f * U.dpi_fac);
  float clip_planes[4][4];
  /* allow for displaying shape keys and deform mods */
  BMIter iter;

  /* make the precision of the display value proportionate to the gridsize */

  if (grid <= 0.01f) {
    conv_float = "%.6g";
  }
  else if (grid <= 0.1f) {
    conv_float = "%.5g";
  }
  else if (grid <= 1.0f) {
    conv_float = "%.4g";
  }
  else if (grid <= 10.0f) {
    conv_float = "%.3g";
  }
  else {
    conv_float = "%.2g";
  }

  if (v3d->overlay.edit_flag &
      (V3D_OVERLAY_EDIT_EDGE_LEN | V3D_OVERLAY_EDIT_EDGE_ANG | V3D_OVERLAY_EDIT_INDICES)) {
    BoundBox bb;
    const rcti rect = {0, ar->winx, 0, ar->winy};

    ED_view3d_clipping_calc(&bb, clip_planes, ar, em->ob, &rect);
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_LEN) {
    BMEdge *eed;

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGELEN, col);

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      /* draw selected edges, or edges next to selected verts while dragging */
      if (BM_elem_flag_test(eed, BM_ELEM_SELECT) ||
          (do_moving && (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) ||
                         BM_elem_flag_test(eed->v2, BM_ELEM_SELECT)))) {
        float v1_clip[3], v2_clip[3];

        copy_v3_v3(v1, eed->v1->co);
        copy_v3_v3(v2, eed->v2->co);

        if (clip_segment_v3_plane_n(v1, v2, clip_planes, 4, v1_clip, v2_clip)) {

          mid_v3_v3v3(vmid, v1_clip, v2_clip);
          mul_m4_v3(ob->obmat, vmid);

          if (do_global) {
            mul_mat3_m4_v3(ob->obmat, v1);
            mul_mat3_m4_v3(ob->obmat, v2);
          }

          if (unit->system) {
            numstr_len = bUnit_AsString2(numstr,
                                         sizeof(numstr),
                                         len_v3v3(v1, v2) * unit->scale_length,
                                         3,
                                         B_UNIT_LENGTH,
                                         unit,
                                         false);
          }
          else {
            numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), conv_float, len_v3v3(v1, v2));
          }

          DRW_text_cache_add(dt,
                             vmid,
                             numstr,
                             numstr_len,
                             0,
                             (do_edge_textpair) ? edge_texpair_sep : 0,
                             txt_flag,
                             col);
        }
      }
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_ANG) {
    const bool is_rad = (unit->system_rotation == USER_UNIT_ROT_RADIANS);
    BMEdge *eed;

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGEANG, col);

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      BMLoop *l_a, *l_b;
      if (BM_edge_loop_pair(eed, &l_a, &l_b)) {
        /* Draw selected edges, or edges next to selected verts while dragging. */
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT) ||
            (do_moving && (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) ||
                           BM_elem_flag_test(eed->v2, BM_ELEM_SELECT) ||
                           /* Special case, this is useful to show when verts connected
                            * to this edge via a face are being transformed. */
                           BM_elem_flag_test(l_a->next->next->v, BM_ELEM_SELECT) ||
                           BM_elem_flag_test(l_a->prev->v, BM_ELEM_SELECT) ||
                           BM_elem_flag_test(l_b->next->next->v, BM_ELEM_SELECT) ||
                           BM_elem_flag_test(l_b->prev->v, BM_ELEM_SELECT)))) {
          float v1_clip[3], v2_clip[3];

          copy_v3_v3(v1, eed->v1->co);
          copy_v3_v3(v2, eed->v2->co);

          if (clip_segment_v3_plane_n(v1, v2, clip_planes, 4, v1_clip, v2_clip)) {
            float no_a[3], no_b[3];
            float angle;

            mid_v3_v3v3(vmid, v1_clip, v2_clip);
            mul_m4_v3(ob->obmat, vmid);

            copy_v3_v3(no_a, l_a->f->no);
            copy_v3_v3(no_b, l_b->f->no);

            if (do_global) {
              mul_mat3_m4_v3(ob->imat, no_a);
              mul_mat3_m4_v3(ob->imat, no_b);
              normalize_v3(no_a);
              normalize_v3(no_b);
            }

            angle = angle_normalized_v3v3(no_a, no_b);

            numstr_len = BLI_snprintf_rlen(numstr,
                                           sizeof(numstr),
                                           "%.3f%s",
                                           (is_rad) ? angle : RAD2DEGF(angle),
                                           (is_rad) ? "r" : "°");

            DRW_text_cache_add(dt,
                               vmid,
                               numstr,
                               numstr_len,
                               0,
                               (do_edge_textpair) ? -edge_texpair_sep : 0,
                               txt_flag,
                               col);
          }
        }
      }
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_AREA) {
    /* would be nice to use BM_face_calc_area, but that is for 2d faces
     * so instead add up tessellation triangle areas */

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEAREA, col);

    int i, n, numtri;
    BMFace *f = NULL;
    BM_ITER_MESH_INDEX (f, &iter, em->bm, BM_FACES_OF_MESH, i) {
      if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
        n = 0;
        numtri = f->len - 2;
        area = 0;
        zero_v3(vmid);
        BMLoop *(*l)[3] = &em->looptris[poly_to_tri_count(i, BM_elem_index_get(f->l_first))];
        for (int j = 0; j < numtri; j++) {
          copy_v3_v3(v1, l[j][0]->v->co);
          copy_v3_v3(v2, l[j][1]->v->co);
          copy_v3_v3(v3, l[j][2]->v->co);

          add_v3_v3(vmid, v1);
          add_v3_v3(vmid, v2);
          add_v3_v3(vmid, v3);
          n += 3;

          if (do_global) {
            mul_mat3_m4_v3(ob->obmat, v1);
            mul_mat3_m4_v3(ob->obmat, v2);
            mul_mat3_m4_v3(ob->obmat, v3);
          }

          area += area_tri_v3(v1, v2, v3);
        }

        mul_v3_fl(vmid, 1.0f / (float)n);
        mul_m4_v3(ob->obmat, vmid);

        if (unit->system) {
          numstr_len = bUnit_AsString2(numstr,
                                       sizeof(numstr),
                                       (double)(area * unit->scale_length * unit->scale_length),
                                       3,
                                       B_UNIT_AREA,
                                       unit,
                                       false);
        }
        else {
          numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), conv_float, area);
        }

        DRW_text_cache_add(dt, vmid, numstr, numstr_len, 0, 0, txt_flag, col);
      }
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_ANG) {
    BMFace *efa;
    const bool is_rad = (unit->system_rotation == USER_UNIT_ROT_RADIANS);

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEANG, col);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      const bool is_face_sel = BM_elem_flag_test_bool(efa, BM_ELEM_SELECT);

      if (is_face_sel || do_moving) {
        BMIter liter;
        BMLoop *loop;
        bool is_first = true;

        BM_ITER_ELEM (loop, &liter, efa, BM_LOOPS_OF_FACE) {
          if (is_face_sel || (do_moving && (BM_elem_flag_test(loop->v, BM_ELEM_SELECT) ||
                                            BM_elem_flag_test(loop->prev->v, BM_ELEM_SELECT) ||
                                            BM_elem_flag_test(loop->next->v, BM_ELEM_SELECT)))) {
            float v2_local[3];

            /* lazy init center calc */
            if (is_first) {
              BM_face_calc_center_bounds(efa, vmid);
              is_first = false;
            }
            copy_v3_v3(v1, loop->prev->v->co);
            copy_v3_v3(v2, loop->v->co);
            copy_v3_v3(v3, loop->next->v->co);

            copy_v3_v3(v2_local, v2);

            if (do_global) {
              mul_mat3_m4_v3(ob->obmat, v1);
              mul_mat3_m4_v3(ob->obmat, v2);
              mul_mat3_m4_v3(ob->obmat, v3);
            }

            float angle = angle_v3v3v3(v1, v2, v3);

            numstr_len = BLI_snprintf_rlen(numstr,
                                           sizeof(numstr),
                                           "%.3f%s",
                                           (is_rad) ? angle : RAD2DEGF(angle),
                                           (is_rad) ? "r" : "°");
            interp_v3_v3v3(fvec, vmid, v2_local, 0.8f);
            mul_m4_v3(ob->obmat, fvec);
            DRW_text_cache_add(dt, fvec, numstr, numstr_len, 0, 0, txt_flag, col);
          }
        }
      }
    }
  }

  /* This option is for mesh ops and addons debugging; only available in UI if Blender starts with
   * --debug */
  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_INDICES) {
    int i;

    /* For now, reuse an appropriate theme color */
    UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEANG, col);

    if (em->selectmode & SCE_SELECT_VERTEX) {
      BMVert *v;

      BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, i) {
        if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
          float vec[3];
          mul_v3_m4v3(vec, ob->obmat, v->co);

          numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", i);
          DRW_text_cache_add(dt, vec, numstr, numstr_len, 0, 0, txt_flag, col);
        }
      }
    }

    if (em->selectmode & SCE_SELECT_EDGE) {
      BMEdge *e;

      BM_ITER_MESH_INDEX (e, &iter, em->bm, BM_EDGES_OF_MESH, i) {
        if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          float v1_clip[3], v2_clip[3];

          copy_v3_v3(v1, e->v1->co);
          copy_v3_v3(v2, e->v2->co);

          if (clip_segment_v3_plane_n(v1, v2, clip_planes, 4, v1_clip, v2_clip)) {
            mid_v3_v3v3(vmid, v1_clip, v2_clip);
            mul_m4_v3(ob->obmat, vmid);

            numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", i);
            DRW_text_cache_add(dt, vmid, numstr, numstr_len, 0, 0, txt_flag, col);
          }
        }
      }
    }

    if (em->selectmode & SCE_SELECT_FACE) {
      BMFace *f;

      BM_ITER_MESH_INDEX (f, &iter, em->bm, BM_FACES_OF_MESH, i) {
        if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          BM_face_calc_center_median(f, v1);
          mul_m4_v3(ob->obmat, v1);

          numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", i);
          DRW_text_cache_add(dt, v1, numstr, numstr_len, 0, 0, txt_flag, col);
        }
      }
    }
  }
}
