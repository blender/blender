/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_color.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_memiter.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BKE_editmesh.hh"
#include "BKE_editmesh_cache.hh"
#include "BKE_global.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_types.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BKE_unit.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "ED_view3d.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLF_api.hh"
#include "WM_api.hh"

#include "draw_manager_text.hh"
#include "intern/bmesh_polygon.hh"

using blender::float3;
using blender::Span;

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
  bool shadow;
  bool align_center;

  /* str is allocated past the end */
  char str[0];
};

struct DRWTextStore {
  BLI_memiter *cache_strings;
};

DRWTextStore *DRW_text_cache_create()
{
  DRWTextStore *dt = MEM_callocN<DRWTextStore>(__func__);
  dt->cache_strings = BLI_memiter_create(1 << 14); /* 16kb */
  return dt;
}

void DRW_text_cache_destroy(DRWTextStore *dt)
{
  if (dt == nullptr) {
    return;
  }
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
                        const uchar col[4],
                        const bool shadow,
                        const bool align_center)
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
  vos->shadow = shadow;
  vos->align_center = align_center;

  /* allocate past the end */
  if (flag & DRW_TEXT_CACHE_STRING_PTR) {
    memcpy(vos->str, &str, alloc_len);
  }
  else {
    memcpy(vos->str, str, alloc_len);
  }
}

static void drw_text_cache_draw_ex(const DRWTextStore *dt, const ARegion *region)
{
  ViewCachedString *vos;
  BLI_memiter_handle it;
  int col_pack_prev = 0;

  float original_proj[4][4];
  GPU_matrix_projection_get(original_proj);
  wmOrtho2_region_pixelspace(region);

  GPU_matrix_push();
  GPU_matrix_identity_set();

  BLF_default_size(UI_style_get()->widget.points);
  const int font_id = BLF_set_default();

  float outline_dark_color[4] = {0, 0, 0, 0.8f};
  float outline_light_color[4] = {1, 1, 1, 0.8f};
  bool outline_is_dark = true;

  BLI_memiter_iter_init(dt->cache_strings, &it);
  while ((vos = static_cast<ViewCachedString *>(BLI_memiter_iter_step(&it)))) {
    if (vos->sco[0] != IS_CLIPPED) {
      if (col_pack_prev != vos->col.pack) {
        BLF_color4ubv(font_id, vos->col.ub);
        const uchar lightness = srgb_to_grayscale_byte(vos->col.ub);
        outline_is_dark = lightness > 96;
        col_pack_prev = vos->col.pack;
      }

      if (vos->align_center) {
        /* Measure the size of the string, then offset to align to the vertex. */
        float width, height;
        BLF_width_and_height(font_id,
                             (vos->flag & DRW_TEXT_CACHE_STRING_PTR) ? *((const char **)vos->str) :
                                                                       vos->str,
                             vos->str_len,
                             &width,
                             &height);
        vos->xoffs -= short(width / 2.0f);
        vos->yoffs -= short(height / 2.0f);
      }

      const int font_id = BLF_default();
      if (vos->shadow) {
        BLF_enable(font_id, BLF_SHADOW);
        BLF_shadow(font_id,
                   FontShadowType::Outline,
                   outline_is_dark ? outline_dark_color : outline_light_color);
        BLF_shadow_offset(font_id, 0, 0);
      }
      else {
        BLF_disable(font_id, BLF_SHADOW);
      }
      BLF_draw_default(float(vos->sco[0] + vos->xoffs),
                       float(vos->sco[1] + vos->yoffs),
                       2.0f,
                       (vos->flag & DRW_TEXT_CACHE_STRING_PTR) ? *((const char **)vos->str) :
                                                                 vos->str,
                       vos->str_len);
    }
  }

  GPU_matrix_pop();
  GPU_matrix_projection_set(original_proj);
}

void DRW_text_cache_draw(const DRWTextStore *dt, const ARegion *region, const View3D *v3d)
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
    const View2D *v2d = &region->v2d;
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

void DRW_text_edit_mesh_measure_stats(const ARegion *region,
                                      const View3D *v3d,
                                      const Object *ob,
                                      const UnitSettings &unit,
                                      DRWTextStore *dt)
{
  /* Do not use ASCII when using non-default unit system, some unit chars are UTF8
   * (micro, square, etc.). See #36090. */
  const short txt_flag = DRW_TEXT_CACHE_GLOBALSPACE;
  const Mesh *mesh = BKE_object_get_editmesh_eval_cage(ob);
  if (!mesh) {
    return;
  }
  const BMEditMesh *em = mesh->runtime->edit_mesh.get();
  if (!BKE_editmesh_eval_orig_map_available(*mesh, BKE_object_get_pre_modified_mesh(ob))) {
    return;
  }
  char numstr[32];                      /* Stores the measurement display text here */
  const char *conv_float;               /* Use a float conversion matching the grid size */
  blender::uchar4 col = {0, 0, 0, 255}; /* color of the text to draw */
  const float grid = unit.system ? unit.scale_length : v3d->grid;
  const bool do_global = (v3d->flag & V3D_GLOBAL_STATS) != 0;
  const bool do_moving = (G.moving & G_TRANSFORM_EDIT) != 0;
  blender::float4x4 clip_planes;
  /* allow for displaying shape keys and deform mods */
  BMIter iter;
  const Span<float3> vert_positions = BKE_mesh_wrapper_vert_coords(mesh);
  const bool use_coords = !vert_positions.is_empty();

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

    ED_view3d_clipping_calc(&bb, clip_planes.ptr(), region, ob, &rect);
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
        float3 v1, v2;
        float3 v1_clip, v2_clip;

        if (use_coords) {
          v1 = vert_positions[BM_elem_index_get(eed->v1)];
          v2 = vert_positions[BM_elem_index_get(eed->v2)];
        }
        else {
          v1 = eed->v1->co;
          v2 = eed->v2->co;
        }

        if (clip_segment_v3_plane_n(v1, v2, clip_planes.ptr(), 4, v1_clip, v2_clip)) {
          const float3 co = blender::math::transform_point(ob->object_to_world(),
                                                           0.5 * (v1_clip + v2_clip));

          if (do_global) {
            v1 = ob->object_to_world().view<3, 3>() * v1;
            v2 = ob->object_to_world().view<3, 3>() * v2;
          }

          const size_t numstr_len =
              unit.system ?
                  BKE_unit_value_as_string_scaled(
                      numstr, sizeof(numstr), len_v3v3(v1, v2), 3, B_UNIT_LENGTH, unit, false) :
                  SNPRINTF_RLEN(numstr, conv_float, len_v3v3(v1, v2));

          DRW_text_cache_add(dt, co, numstr, numstr_len, 0, edge_tex_sep, txt_flag, col);
        }
      }
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_ANG) {
    const bool is_rad = (unit.system_rotation == USER_UNIT_ROT_RADIANS);
    BMEdge *eed;

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGEANG, col);

    Span<float3> face_normals;
    if (use_coords) {
      BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_FACE);
      /* TODO: This is not const correct for wrapper meshes, but it should be okay because
       * every evaluated object gets its own evaluated cage mesh (they are not shared). */
      face_normals = BKE_mesh_wrapper_face_normals(const_cast<Mesh *>(mesh));
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
          float3 v1, v2;
          float3 v1_clip, v2_clip;

          if (use_coords) {
            v1 = vert_positions[BM_elem_index_get(eed->v1)];
            v2 = vert_positions[BM_elem_index_get(eed->v2)];
          }
          else {
            v1 = eed->v1->co;
            v2 = eed->v2->co;
          }

          if (clip_segment_v3_plane_n(v1, v2, clip_planes.ptr(), 4, v1_clip, v2_clip)) {
            float3 no_a, no_b;

            const float3 co = blender::math::transform_point(ob->object_to_world(),
                                                             0.5 * (v1_clip + v2_clip));

            if (use_coords) {
              no_a = face_normals[BM_elem_index_get(l_a->f)];
              no_b = face_normals[BM_elem_index_get(l_b->f)];
            }
            else {
              no_a = l_a->f->no;
              no_b = l_b->f->no;
            }

            if (do_global) {
              no_a = blender::math::normalize(ob->world_to_object().view<3, 3>() * no_a);
              no_b = blender::math::normalize(ob->world_to_object().view<3, 3>() * no_b);
            }

            const float angle = angle_normalized_v3v3(no_a, no_b);

            const size_t numstr_len = SNPRINTF_RLEN(numstr,
                                                    "%.3f%s",
                                                    (is_rad) ? angle : RAD2DEGF(angle),
                                                    (is_rad) ? "r" : BLI_STR_UTF8_DEGREE_SIGN);

            DRW_text_cache_add(dt, co, numstr, numstr_len, 0, -edge_tex_sep, txt_flag, col);
          }
        }
      }
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_AREA) {
    /* would be nice to use BM_face_calc_area, but that is for 2d faces
     * so instead add up tessellation triangle areas */

    UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEAREA, col);

    int i;
    BMFace *f = nullptr;
    /* Alternative to using `poly_to_tri_count(i, BM_elem_index_get(f->l_first))`
     * without having to add an extra loop. */
    int tri_index = 0;
    BM_ITER_MESH_INDEX (f, &iter, em->bm, BM_FACES_OF_MESH, i) {
      const int f_corner_tris_len = f->len - 2;
      if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
        int n = 0;
        float area = 0; /* area of the face */
        float3 vmid(0.0f);
        const std::array<BMLoop *, 3> *ltri_array = &em->looptris[tri_index];
        for (int j = 0; j < f_corner_tris_len; j++) {
          float3 v1, v2, v3;

          if (use_coords) {
            v1 = vert_positions[BM_elem_index_get(ltri_array[j][0]->v)];
            v2 = vert_positions[BM_elem_index_get(ltri_array[j][1]->v)];
            v3 = vert_positions[BM_elem_index_get(ltri_array[j][2]->v)];
          }
          else {
            v1 = ltri_array[j][0]->v->co;
            v2 = ltri_array[j][1]->v->co;
            v3 = ltri_array[j][2]->v->co;
          }

          vmid += v1;
          vmid += v2;
          vmid += v3;
          n += 3;

          if (do_global) {
            v1 = ob->object_to_world().view<3, 3>() * v1;
            v2 = ob->object_to_world().view<3, 3>() * v2;
            v3 = ob->object_to_world().view<3, 3>() * v3;
          }

          area += area_tri_v3(v1, v2, v3);
        }

        vmid *= 1.0f / float(n);
        vmid = blender::math::transform_point(ob->object_to_world(), vmid);

        const size_t numstr_len =
            unit.system ? BKE_unit_value_as_string_scaled(
                              numstr, sizeof(numstr), area, 3, B_UNIT_AREA, unit, false) :
                          SNPRINTF_RLEN(numstr, conv_float, area);

        DRW_text_cache_add(dt, vmid, numstr, numstr_len, 0, 0, txt_flag, col);
      }
      tri_index += f_corner_tris_len;
    }
  }

  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_ANG) {
    BMFace *efa;
    const bool is_rad = (unit.system_rotation == USER_UNIT_ROT_RADIANS);

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
            float3 v1, v2, v3;
            float3 vmid;

            /* lazy init center calc */
            if (is_first) {
              if (use_coords) {
                BM_face_calc_center_bounds_vcos(em->bm, efa, vmid, vert_positions);
              }
              else {
                BM_face_calc_center_bounds(efa, vmid);
              }
              is_first = false;
            }
            if (use_coords) {
              v1 = vert_positions[BM_elem_index_get(loop->prev->v)];
              v2 = vert_positions[BM_elem_index_get(loop->v)];
              v3 = vert_positions[BM_elem_index_get(loop->next->v)];
            }
            else {
              v1 = loop->prev->v->co;
              v2 = loop->v->co;
              v3 = loop->next->v->co;
            }

            const float3 v2_local = v2;

            if (do_global) {
              v1 = ob->object_to_world().view<3, 3>() * v1;
              v2 = ob->object_to_world().view<3, 3>() * v2;
              v3 = ob->object_to_world().view<3, 3>() * v3;
            }

            const float angle = angle_v3v3v3(v1, v2, v3);

            const size_t numstr_len = SNPRINTF_RLEN(numstr,
                                                    "%.3f%s",
                                                    (is_rad) ? angle : RAD2DEGF(angle),
                                                    (is_rad) ? "r" : BLI_STR_UTF8_DEGREE_SIGN);
            const float3 co = blender::math::transform_point(
                ob->object_to_world(), blender::math::interpolate(vmid, v2_local, 0.8f));
            DRW_text_cache_add(dt, co, numstr, numstr_len, 0, 0, txt_flag, col);
          }
        }
      }
    }
  }

  /* This option is for mesh ops and addons debugging; only available in UI if Blender starts
   * with
   * --debug */
  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_INDICES) {
    int i;

    UI_GetThemeColor4ubv(TH_TEXT_HI, col);

    if (em->selectmode & SCE_SELECT_VERTEX) {
      BMVert *v;

      if (use_coords) {
        BM_mesh_elem_index_ensure(em->bm, BM_VERT);
      }
      BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, i) {
        if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
          const float3 co = blender::math::transform_point(
              ob->object_to_world(), use_coords ? vert_positions[BM_elem_index_get(v)] : v->co);

          const size_t numstr_len = SNPRINTF_RLEN(numstr, "%d", i);
          DRW_text_cache_add(dt, co, numstr, numstr_len, 0, 0, txt_flag, col, true, false);
        }
      }
    }

    if (em->selectmode & SCE_SELECT_EDGE) {
      BMEdge *eed;

      const bool use_edge_tex_sep = (edge_tex_count == 2);
      const bool use_edge_tex_len = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_EDGE_LEN);

      BM_ITER_MESH_INDEX (eed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          float3 v1, v2;
          float3 v1_clip, v2_clip;

          if (use_coords) {
            v1 = vert_positions[BM_elem_index_get(eed->v1)];
            v2 = vert_positions[BM_elem_index_get(eed->v2)];
          }
          else {
            v1 = eed->v1->co;
            v2 = eed->v2->co;
          }

          if (clip_segment_v3_plane_n(v1, v2, clip_planes.ptr(), 4, v1_clip, v2_clip)) {
            const float3 co = blender::math::transform_point(ob->object_to_world(),
                                                             0.5 * (v1_clip + v2_clip));

            const size_t numstr_len = SNPRINTF_RLEN(numstr, "%d", i);
            DRW_text_cache_add(
                dt,
                co,
                numstr,
                numstr_len,
                0,
                (use_edge_tex_sep) ? (use_edge_tex_len) ? -edge_tex_sep : edge_tex_sep : 0,
                txt_flag,
                col,
                true,
                false);
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
          float3 co;

          if (use_coords) {
            BM_face_calc_center_median_vcos(em->bm, f, co, vert_positions);
          }
          else {
            BM_face_calc_center_median(f, co);
          }

          co = blender::math::transform_point(ob->object_to_world(), co);

          const size_t numstr_len = SNPRINTF_RLEN(numstr, "%d", i);
          DRW_text_cache_add(dt, co, numstr, numstr_len, 0, 0, txt_flag, col, true, false);
        }
      }
    }
  }
}
