/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_memiter.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.hh"
#include "BKE_global.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.h"
#include "BKE_unit.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLF_api.h"
#include "WM_api.h"

#include "draw_manager_text.h"
#include "intern/bmesh_polygon.h"

struct ViewCachedString {
  float vec[3];
  union {
    uchar ub[4];
    int pack;
  } col;
  short sco[2];
  short xoffs, yoffs;
  short flag;
  int str_len;

  /* str is allocated past the end */
  char str[0];
};

struct DRWTextStore {
  BLI_memiter *cache_strings;
};

DRWTextStore *DRW_text_cache_create()
{
  DRWTextStore *dt = MEM_cnew<DRWTextStore>(__func__);
  dt->cache_strings = BLI_memiter_create(1 << 14); /* 16kb */
  return dt;
}

void DRW_text_cache_destroy(DRWTextStore *dt)
{
  BLI_memiter_destroy(dt->cache_strings);
  MEM_freeN(dt);
}

void DRW_text_cache_add(DRWTextStore *dt,
                        const float co[3],
                        const char *str,
                        const int str_len,
                        short xoffs,
                        short yoffs,
                        short flag,
                        const uchar col[4])
{
  int alloc_len;
  ViewCachedString *vos;

  if (flag & DRW_TEXT_CACHE_STRING_PTR) {
    BLI_assert(str_len == strlen(str));
    alloc_len = sizeof(void *);
  }
  else {
    alloc_len = str_len + 1;
  }

  vos = static_cast<ViewCachedString *>(
      BLI_memiter_alloc(dt->cache_strings, sizeof(ViewCachedString) + alloc_len));

  copy_v3_v3(vos->vec, co);
  copy_v4_v4_uchar(vos->col.ub, col);
  vos->xoffs = xoffs;
  vos->yoffs = yoffs;
  vos->flag = flag;
  vos->str_len = str_len;

  /* allocate past the end */
  if (flag & DRW_TEXT_CACHE_STRING_PTR) {
    memcpy(vos->str, &str, alloc_len);
  }
  else {
    memcpy(vos->str, str, alloc_len);
  }
}

static void drw_text_cache_draw_ex(DRWTextStore *dt, ARegion *region)
{
  ViewCachedString *vos;
  BLI_memiter_handle it;
  int col_pack_prev = 0;

  float original_proj[4][4];
  GPU_matrix_projection_get(original_proj);
  wmOrtho2_region_pixelspace(region);

  GPU_matrix_push();
  GPU_matrix_identity_set();

  const int font_id = BLF_default();

  const uiStyle *style = UI_style_get();

  BLF_size(font_id, style->widget.points * UI_SCALE_FAC);

  BLI_memiter_iter_init(dt->cache_strings, &it);
  while ((vos = static_cast<ViewCachedString *>(BLI_memiter_iter_step(&it)))) {
    if (vos->sco[0] != IS_CLIPPED) {
      if (col_pack_prev != vos->col.pack) {
        BLF_color4ubv(font_id, vos->col.ub);
        col_pack_prev = vos->col.pack;
      }

      BLF_position(
          font_id, float(vos->sco[0] + vos->xoffs), float(vos->sco[1] + vos->yoffs), 2.0f);
      BLF_draw(font_id,
               (vos->flag & DRW_TEXT_CACHE_STRING_PTR) ? *((const char **)vos->str) : vos->str,
               vos->str_len);
    }
  }

  GPU_matrix_pop();
  GPU_matrix_projection_set(original_proj);
}

void DRW_text_cache_draw(DRWTextStore *dt, ARegion *region, View3D *v3d)
{
  ViewCachedString *vos;
  if (v3d) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    int tot = 0;
    /* project first and test */
    BLI_memiter_handle it;
    BLI_memiter_iter_init(dt->cache_strings, &it);
    while ((vos = static_cast<ViewCachedString *>(BLI_memiter_iter_step(&it)))) {
      if (ED_view3d_project_short_ex(
              region,
              (vos->flag & DRW_TEXT_CACHE_GLOBALSPACE) ? rv3d->persmat : rv3d->persmatob,
              (vos->flag & DRW_TEXT_CACHE_LOCALCLIP) != 0,
              vos->vec,
              vos->sco,
              V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR) ==
          V3D_PROJ_RET_OK)
      {
        tot++;
      }
      else {
        vos->sco[0] = IS_CLIPPED;
      }
    }

    if (tot) {
      /* Disable clipping for text */
      const bool rv3d_clipping_enabled = RV3D_CLIPPING_ENABLED(v3d, rv3d);
      if (rv3d_clipping_enabled) {
        GPU_clip_distances(0);
      }

      drw_text_cache_draw_ex(dt, region);

      if (rv3d_clipping_enabled) {
        GPU_clip_distances(6);
      }
    }
  }
  else {
    /* project first */
    BLI_memiter_handle it;
    BLI_memiter_iter_init(dt->cache_strings, &it);
    View2D *v2d = &region->v2d;
    float viewmat[4][4];
    rctf region_space = {0.0f, float(region->winx), 0.0f, float(region->winy)};
    BLI_rctf_transform_calc_m4_pivot_min(&v2d->cur, &region_space, viewmat);

    while ((vos = static_cast<ViewCachedString *>(BLI_memiter_iter_step(&it)))) {
      float p[3];
      copy_v3_v3(p, vos->vec);
      mul_m4_v3(viewmat, p);

      vos->sco[0] = p[0];
      vos->sco[1] = p[1];
    }

    drw_text_cache_draw_ex(dt, region);
  }
}

void DRW_text_edit_mesh_measure_stats(ARegion *region,
                                      View3D *v3d,
                                      Object *ob,
                                      const UnitSettings *unit)
{
  /* Do not use ascii when using non-default unit system, some unit chars are utf8 (micro, square,
   * etc.). See bug #36090.
   */
  DRWTextStore *dt = DRW_text_cache_ensure();
  const short txt_flag = DRW_TEXT_CACHE_GLOBALSPACE;
  Mesh *me = BKE_object_get_editmesh_eval_cage(ob);
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
  float clip_planes[4][4];
  /* allow for displaying shape keys and deform mods */
  BMIter iter;
  const float(*vert_coords)[3] = BKE_mesh_wrapper_vert_coords(me);
  const bool use_coords = (vert_coords != nullptr);

  /* when 2 or more edge-info options are enabled, space apart */
  short edge_tex_count = 0;
  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_LEN) {
    edge_tex_count += 1;
  }
  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_ANG) {
    edge_tex_count += 1;
  }
  if ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_INDICES) && (em->selectmode & SCE_SELECT_EDGE)) {
    edge_tex_count += 1;
  }
  const short edge_tex_sep = short((edge_tex_count - 1) * 5.0f * UI_SCALE_FAC);

  /* Make the precision of the display value proportionate to the grid-size. */

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
      (V3D_OVERLAY_EDIT_EDGE_LEN | V3D_OVERLAY_EDIT_EDGE_ANG | V3D_OVERLAY_EDIT_INDICES))
  {
    BoundBox bb;
    const rcti rect = {0, region->winx, 0, region->winy};

    ED_view3d_clipping_calc(&bb, clip_planes, region, ob, &rect);
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_LEN) {
    BMEdge *eed;

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGELEN, col);

    if (use_coords) {
      BM_mesh_elem_index_ensure(em->bm, BM_VERT);
    }

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      /* draw selected edges, or edges next to selected verts while dragging */
      if (BM_elem_flag_test(eed, BM_ELEM_SELECT) ||
          (do_moving && (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) ||
                         BM_elem_flag_test(eed->v2, BM_ELEM_SELECT))))
      {
        float v1_clip[3], v2_clip[3];

        if (vert_coords) {
          copy_v3_v3(v1, vert_coords[BM_elem_index_get(eed->v1)]);
          copy_v3_v3(v2, vert_coords[BM_elem_index_get(eed->v2)]);
        }
        else {
          copy_v3_v3(v1, eed->v1->co);
          copy_v3_v3(v2, eed->v2->co);
        }

        if (clip_segment_v3_plane_n(v1, v2, clip_planes, 4, v1_clip, v2_clip)) {

          mid_v3_v3v3(vmid, v1_clip, v2_clip);
          mul_m4_v3(ob->object_to_world, vmid);

          if (do_global) {
            mul_mat3_m4_v3(ob->object_to_world, v1);
            mul_mat3_m4_v3(ob->object_to_world, v2);
          }

          if (unit->system) {
            numstr_len = BKE_unit_value_as_string(numstr,
                                                  sizeof(numstr),
                                                  len_v3v3(v1, v2) * unit->scale_length,
                                                  3,
                                                  B_UNIT_LENGTH,
                                                  unit,
                                                  false);
          }
          else {
            numstr_len = SNPRINTF_RLEN(numstr, conv_float, len_v3v3(v1, v2));
          }

          DRW_text_cache_add(dt, vmid, numstr, numstr_len, 0, edge_tex_sep, txt_flag, col);
        }
      }
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_ANG) {
    const bool is_rad = (unit->system_rotation == USER_UNIT_ROT_RADIANS);
    BMEdge *eed;

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGEANG, col);

    const float(*face_normals)[3] = nullptr;
    if (use_coords) {
      BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_FACE);
      face_normals = BKE_mesh_wrapper_face_normals(me);
    }

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
                           BM_elem_flag_test(l_b->prev->v, BM_ELEM_SELECT))))
        {
          float v1_clip[3], v2_clip[3];

          if (vert_coords) {
            copy_v3_v3(v1, vert_coords[BM_elem_index_get(eed->v1)]);
            copy_v3_v3(v2, vert_coords[BM_elem_index_get(eed->v2)]);
          }
          else {
            copy_v3_v3(v1, eed->v1->co);
            copy_v3_v3(v2, eed->v2->co);
          }

          if (clip_segment_v3_plane_n(v1, v2, clip_planes, 4, v1_clip, v2_clip)) {
            float no_a[3], no_b[3];
            float angle;

            mid_v3_v3v3(vmid, v1_clip, v2_clip);
            mul_m4_v3(ob->object_to_world, vmid);

            if (use_coords) {
              copy_v3_v3(no_a, face_normals[BM_elem_index_get(l_a->f)]);
              copy_v3_v3(no_b, face_normals[BM_elem_index_get(l_b->f)]);
            }
            else {
              copy_v3_v3(no_a, l_a->f->no);
              copy_v3_v3(no_b, l_b->f->no);
            }

            if (do_global) {
              mul_mat3_m4_v3(ob->world_to_object, no_a);
              mul_mat3_m4_v3(ob->world_to_object, no_b);
              normalize_v3(no_a);
              normalize_v3(no_b);
            }

            angle = angle_normalized_v3v3(no_a, no_b);

            numstr_len = SNPRINTF_RLEN(numstr,
                                       "%.3f%s",
                                       (is_rad) ? angle : RAD2DEGF(angle),
                                       (is_rad) ? "r" : BLI_STR_UTF8_DEGREE_SIGN);

            DRW_text_cache_add(dt, vmid, numstr, numstr_len, 0, -edge_tex_sep, txt_flag, col);
          }
        }
      }
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_AREA) {
    /* would be nice to use BM_face_calc_area, but that is for 2d faces
     * so instead add up tessellation triangle areas */

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEAREA, col);

    int i, n;
    BMFace *f = nullptr;
    /* Alternative to using `poly_to_tri_count(i, BM_elem_index_get(f->l_first))`
     * without having to add an extra loop. */
    int looptri_index = 0;
    BM_ITER_MESH_INDEX (f, &iter, em->bm, BM_FACES_OF_MESH, i) {
      const int f_looptri_len = f->len - 2;
      if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
        n = 0;
        area = 0;
        zero_v3(vmid);
        BMLoop *(*l)[3] = &em->looptris[looptri_index];
        for (int j = 0; j < f_looptri_len; j++) {

          if (use_coords) {
            copy_v3_v3(v1, vert_coords[BM_elem_index_get(l[j][0]->v)]);
            copy_v3_v3(v2, vert_coords[BM_elem_index_get(l[j][1]->v)]);
            copy_v3_v3(v3, vert_coords[BM_elem_index_get(l[j][2]->v)]);
          }
          else {
            copy_v3_v3(v1, l[j][0]->v->co);
            copy_v3_v3(v2, l[j][1]->v->co);
            copy_v3_v3(v3, l[j][2]->v->co);
          }

          add_v3_v3(vmid, v1);
          add_v3_v3(vmid, v2);
          add_v3_v3(vmid, v3);
          n += 3;

          if (do_global) {
            mul_mat3_m4_v3(ob->object_to_world, v1);
            mul_mat3_m4_v3(ob->object_to_world, v2);
            mul_mat3_m4_v3(ob->object_to_world, v3);
          }

          area += area_tri_v3(v1, v2, v3);
        }

        mul_v3_fl(vmid, 1.0f / float(n));
        mul_m4_v3(ob->object_to_world, vmid);

        if (unit->system) {
          numstr_len = BKE_unit_value_as_string(
              numstr,
              sizeof(numstr),
              double(area * unit->scale_length * unit->scale_length),
              3,
              B_UNIT_AREA,
              unit,
              false);
        }
        else {
          numstr_len = SNPRINTF_RLEN(numstr, conv_float, area);
        }

        DRW_text_cache_add(dt, vmid, numstr, numstr_len, 0, 0, txt_flag, col);
      }
      looptri_index += f_looptri_len;
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_ANG) {
    BMFace *efa;
    const bool is_rad = (unit->system_rotation == USER_UNIT_ROT_RADIANS);

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEANG, col);

    if (use_coords) {
      BM_mesh_elem_index_ensure(em->bm, BM_VERT);
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      const bool is_face_sel = BM_elem_flag_test_bool(efa, BM_ELEM_SELECT);

      if (is_face_sel || do_moving) {
        BMIter liter;
        BMLoop *loop;
        bool is_first = true;

        BM_ITER_ELEM (loop, &liter, efa, BM_LOOPS_OF_FACE) {
          if (is_face_sel || (do_moving && (BM_elem_flag_test(loop->v, BM_ELEM_SELECT) ||
                                            BM_elem_flag_test(loop->prev->v, BM_ELEM_SELECT) ||
                                            BM_elem_flag_test(loop->next->v, BM_ELEM_SELECT))))
          {
            float v2_local[3];

            /* lazy init center calc */
            if (is_first) {
              if (use_coords) {
                BM_face_calc_center_bounds_vcos(em->bm, efa, vmid, vert_coords);
              }
              else {
                BM_face_calc_center_bounds(efa, vmid);
              }
              is_first = false;
            }
            if (use_coords) {
              copy_v3_v3(v1, vert_coords[BM_elem_index_get(loop->prev->v)]);
              copy_v3_v3(v2, vert_coords[BM_elem_index_get(loop->v)]);
              copy_v3_v3(v3, vert_coords[BM_elem_index_get(loop->next->v)]);
            }
            else {
              copy_v3_v3(v1, loop->prev->v->co);
              copy_v3_v3(v2, loop->v->co);
              copy_v3_v3(v3, loop->next->v->co);
            }

            copy_v3_v3(v2_local, v2);

            if (do_global) {
              mul_mat3_m4_v3(ob->object_to_world, v1);
              mul_mat3_m4_v3(ob->object_to_world, v2);
              mul_mat3_m4_v3(ob->object_to_world, v3);
            }

            float angle = angle_v3v3v3(v1, v2, v3);

            numstr_len = SNPRINTF_RLEN(numstr,
                                       "%.3f%s",
                                       (is_rad) ? angle : RAD2DEGF(angle),
                                       (is_rad) ? "r" : BLI_STR_UTF8_DEGREE_SIGN);
            interp_v3_v3v3(fvec, vmid, v2_local, 0.8f);
            mul_m4_v3(ob->object_to_world, fvec);
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

      if (use_coords) {
        BM_mesh_elem_index_ensure(em->bm, BM_VERT);
      }
      BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, i) {
        if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
          if (use_coords) {
            copy_v3_v3(v1, vert_coords[BM_elem_index_get(v)]);
          }
          else {
            copy_v3_v3(v1, v->co);
          }

          mul_m4_v3(ob->object_to_world, v1);

          numstr_len = SNPRINTF_RLEN(numstr, "%d", i);
          DRW_text_cache_add(dt, v1, numstr, numstr_len, 0, 0, txt_flag, col);
        }
      }
    }

    if (em->selectmode & SCE_SELECT_EDGE) {
      BMEdge *eed;

      const bool use_edge_tex_sep = (edge_tex_count == 2);
      const bool use_edge_tex_len = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_LEN);

      BM_ITER_MESH_INDEX (eed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          float v1_clip[3], v2_clip[3];

          if (use_coords) {
            copy_v3_v3(v1, vert_coords[BM_elem_index_get(eed->v1)]);
            copy_v3_v3(v2, vert_coords[BM_elem_index_get(eed->v2)]);
          }
          else {
            copy_v3_v3(v1, eed->v1->co);
            copy_v3_v3(v2, eed->v2->co);
          }

          if (clip_segment_v3_plane_n(v1, v2, clip_planes, 4, v1_clip, v2_clip)) {
            mid_v3_v3v3(vmid, v1_clip, v2_clip);
            mul_m4_v3(ob->object_to_world, vmid);

            numstr_len = SNPRINTF_RLEN(numstr, "%d", i);
            DRW_text_cache_add(
                dt,
                vmid,
                numstr,
                numstr_len,
                0,
                (use_edge_tex_sep) ? (use_edge_tex_len) ? -edge_tex_sep : edge_tex_sep : 0,
                txt_flag,
                col);
          }
        }
      }
    }

    if (em->selectmode & SCE_SELECT_FACE) {
      BMFace *f;

      if (use_coords) {
        BM_mesh_elem_index_ensure(em->bm, BM_VERT);
      }

      BM_ITER_MESH_INDEX (f, &iter, em->bm, BM_FACES_OF_MESH, i) {
        if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {

          if (use_coords) {
            BM_face_calc_center_median_vcos(em->bm, f, v1, vert_coords);
          }
          else {
            BM_face_calc_center_median(f, v1);
          }

          mul_m4_v3(ob->object_to_world, v1);

          numstr_len = SNPRINTF_RLEN(numstr, "%d", i);
          DRW_text_cache_add(dt, v1, numstr, numstr_len, 0, 0, txt_flag, col);
        }
      }
    }
  }
}
