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
 */

/** \file
 * \ingroup eduv
 *
 * Utilities for manipulating UV islands.
 *
 * \note This is similar to `uvedit_parametrizer.c`,
 * however the data structures there don't support arbitrary topology
 * such as an edge with 3 or more faces using it.
 * This API uses #BMesh data structures and doesn't have limitations for manifold meshes.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BLI_boxpack_2d.h"
#include "BLI_convexhull_2d.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_editmesh.h"

#include "DEG_depsgraph.h"

#include "ED_uvedit.h" /* Own include. */

#include "WM_api.h"
#include "WM_types.h"

#include "bmesh.h"

/* -------------------------------------------------------------------- */
/** \name UV Face Utilities
 * \{ */

static void bm_face_uv_scale_y(BMFace *f, const float scale_y, const int cd_loop_uv_offset)
{
  BMLoop *l_iter;
  BMLoop *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    luv->uv[1] *= scale_y;
  } while ((l_iter = l_iter->next) != l_first);
}

static void bm_face_uv_translate_and_scale_around_pivot(BMFace *f,
                                                        const float offset[2],
                                                        const float scale[2],
                                                        const float pivot[2],
                                                        const int cd_loop_uv_offset)
{
  BMLoop *l_iter;
  BMLoop *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    for (int i = 0; i < 2; i++) {
      luv->uv[i] = offset[i] + (((luv->uv[i] - pivot[i]) * scale[i]) + pivot[i]);
    }
  } while ((l_iter = l_iter->next) != l_first);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Face Array Utilities
 * \{ */

static void bm_face_array_calc_bounds(BMFace **faces,
                                      int faces_len,
                                      const uint cd_loop_uv_offset,
                                      rctf *r_bounds_rect)
{
  float bounds_min[2], bounds_max[2];
  INIT_MINMAX2(bounds_min, bounds_max);
  for (int i = 0; i < faces_len; i++) {
    BMFace *f = faces[i];
    BM_face_uv_minmax(f, bounds_min, bounds_max, cd_loop_uv_offset);
  }
  r_bounds_rect->xmin = bounds_min[0];
  r_bounds_rect->ymin = bounds_min[1];
  r_bounds_rect->xmax = bounds_max[0];
  r_bounds_rect->ymax = bounds_max[1];
}

/**
 * Return an array of un-ordered UV coordinates,
 * without duplicating coordinates for loops that share a vertex.
 */
static float (*bm_face_array_calc_unique_uv_coords(
    BMFace **faces, int faces_len, const uint cd_loop_uv_offset, int *r_coords_len))[2]
{
  int coords_len_alloc = 0;
  for (int i = 0; i < faces_len; i++) {
    BMFace *f = faces[i];
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_elem_flag_enable(l_iter, BM_ELEM_TAG);
    } while ((l_iter = l_iter->next) != l_first);
    coords_len_alloc += f->len;
  }

  float(*coords)[2] = MEM_mallocN(sizeof(*coords) * coords_len_alloc, __func__);
  int coords_len = 0;

  for (int i = 0; i < faces_len; i++) {
    BMFace *f = faces[i];
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (!BM_elem_flag_test(l_iter, BM_ELEM_TAG)) {
        /* Already walked over, continue. */
        continue;
      }

      BM_elem_flag_disable(l_iter, BM_ELEM_TAG);
      const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
      copy_v2_v2(coords[coords_len++], luv->uv);

      /* Un tag all connected so we don't add them twice.
       * Note that we will tag other loops not part of `faces` but this is harmless,
       * since we're only turning off a tag. */
      BMVert *v_pivot = l_iter->v;
      BMEdge *e_first = v_pivot->e;
      const BMEdge *e = e_first;
      do {
        const BMLoop *l_radial = e->l;
        do {
          if (l_radial->v == l_iter->v) {
            if (BM_elem_flag_test(l_radial, BM_ELEM_TAG)) {
              const MLoopUV *luv_radial = BM_ELEM_CD_GET_VOID_P(l_radial, cd_loop_uv_offset);
              if (equals_v2v2(luv->uv, luv_radial->uv)) {
                /* Don't add this UV when met in another face in `faces`. */
                BM_elem_flag_disable(l_iter, BM_ELEM_TAG);
              }
            }
          }
        } while ((l_radial = l_radial->radial_next) != e->l);
      } while ((e = BM_DISK_EDGE_NEXT(e, v_pivot)) != e_first);
    } while ((l_iter = l_iter->next) != l_first);
  }
  coords = MEM_reallocN(coords, sizeof(*coords) * coords_len);
  *r_coords_len = coords_len;
  return coords;
}

/**
 * \param align_to_axis:
 * - -1: don't align to an axis.
 * -  0: align horizontally.
 * -  1: align vertically.
 */
static void bm_face_array_uv_rotate_fit_aabb(BMFace **faces,
                                             int faces_len,
                                             int align_to_axis,
                                             const uint cd_loop_uv_offset)
{
  /* Calculate unique coordinates since calculating a convex hull can be an expensive operation. */
  int coords_len;
  float(*coords)[2] = bm_face_array_calc_unique_uv_coords(
      faces, faces_len, cd_loop_uv_offset, &coords_len);

  float angle = BLI_convexhull_aabb_fit_points_2d(coords, coords_len);

  if (align_to_axis != -1) {
    if (angle != 0.0f) {
      float matrix[2][2];
      angle_to_mat2(matrix, angle);
      for (int i = 0; i < coords_len; i++) {
        mul_m2_v2(matrix, coords[i]);
      }
    }

    float bounds_min[2], bounds_max[2];
    INIT_MINMAX2(bounds_min, bounds_max);
    for (int i = 0; i < coords_len; i++) {
      minmax_v2v2_v2(bounds_min, bounds_max, coords[i]);
    }

    float size[2];
    sub_v2_v2v2(size, bounds_max, bounds_min);
    if (align_to_axis ? (size[1] < size[0]) : (size[0] < size[1])) {
      angle += DEG2RAD(90.0);
    }
  }

  MEM_freeN(coords);

  if (angle != 0.0f) {
    float matrix[2][2];
    angle_to_mat2(matrix, angle);
    for (int i = 0; i < faces_len; i++) {
      BM_face_uv_transform(faces[i], matrix, cd_loop_uv_offset);
    }
  }
}

static void bm_face_array_uv_scale_y(BMFace **faces,
                                     int faces_len,
                                     const float scale_y,
                                     const uint cd_loop_uv_offset)
{
  for (int i = 0; i < faces_len; i++) {
    BMFace *f = faces[i];
    bm_face_uv_scale_y(f, scale_y, cd_loop_uv_offset);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate UV Islands
 *
 * \note Currently this is a private API/type, it could be made public.
 * \{ */

struct FaceIsland {
  struct FaceIsland *next, *prev;
  BMFace **faces;
  int faces_len;
  rctf bounds_rect;
  /**
   * \note While this is duplicate information,
   * it allows islands from multiple meshes to be stored in the same list.
   */
  uint cd_loop_uv_offset;
  float aspect_y;
};

struct SharedUVLoopData {
  uint cd_loop_uv_offset;
  bool use_seams;
};

static bool bm_loop_uv_shared_edge_check(const BMLoop *l_a, const BMLoop *l_b, void *user_data)
{
  const struct SharedUVLoopData *data = user_data;

  if (data->use_seams) {
    if (BM_elem_flag_test(l_a->e, BM_ELEM_SEAM)) {
      return false;
    }
  }

  return BM_loop_uv_share_edge_check((BMLoop *)l_a, (BMLoop *)l_b, data->cd_loop_uv_offset);
}

/**
 * Calculate islands and add them to \a island_list returning the number of items added.
 */
static int bm_mesh_calc_uv_islands(const Scene *scene,
                                   BMesh *bm,
                                   ListBase *island_list,
                                   const bool only_selected_faces,
                                   const bool only_selected_uvs,
                                   const bool use_seams,
                                   const float aspect_y,
                                   const uint cd_loop_uv_offset)
{
  int island_added = 0;
  BM_mesh_elem_table_ensure(bm, BM_FACE);

  struct SharedUVLoopData user_data = {
      .cd_loop_uv_offset = cd_loop_uv_offset,
      .use_seams = use_seams,
  };

  int *groups_array = MEM_mallocN(sizeof(*groups_array) * (size_t)bm->totface, __func__);

  int(*group_index)[2];

  /* Calculate the tag to use. */
  uchar hflag_face_test = 0;
  if (only_selected_faces) {
    if (only_selected_uvs) {
      BMFace *f;
      BMIter iter;
      BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        bool value = false;
        if (BM_elem_flag_test(f, BM_ELEM_SELECT) &&
            uvedit_face_select_test(scene, f, cd_loop_uv_offset)) {
          value = true;
        }
        BM_elem_flag_set(f, BM_ELEM_TAG, value);
      }
      hflag_face_test = BM_ELEM_TAG;
    }
    else {
      hflag_face_test = BM_ELEM_SELECT;
    }
  }

  const int group_len = BM_mesh_calc_face_groups(bm,
                                                 groups_array,
                                                 &group_index,
                                                 NULL,
                                                 bm_loop_uv_shared_edge_check,
                                                 &user_data,
                                                 hflag_face_test,
                                                 BM_EDGE);

  for (int i = 0; i < group_len; i++) {
    const int faces_start = group_index[i][0];
    const int faces_len = group_index[i][1];
    BMFace **faces = MEM_mallocN(sizeof(*faces) * faces_len, __func__);

    float bounds_min[2], bounds_max[2];
    INIT_MINMAX2(bounds_min, bounds_max);

    for (int j = 0; j < faces_len; j++) {
      faces[j] = BM_face_at_index(bm, groups_array[faces_start + j]);
    }

    struct FaceIsland *island = MEM_callocN(sizeof(*island), __func__);
    island->faces = faces;
    island->faces_len = faces_len;
    island->cd_loop_uv_offset = cd_loop_uv_offset;
    island->aspect_y = aspect_y;
    BLI_addtail(island_list, island);
    island_added += 1;
  }

  MEM_freeN(groups_array);
  MEM_freeN(group_index);
  return island_added;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public UV Island Packing
 *
 * \note This behavior follows #param_pack.
 * \{ */

void ED_uvedit_pack_islands_multi(const Scene *scene,
                                  Object **objects,
                                  const uint objects_len,
                                  const struct UVPackIsland_Params *params)
{
  /* Align to the Y axis, could make this configurable. */
  const int rotate_align_axis = 1;
  ListBase island_list = {NULL};
  int island_list_len = 0;

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
    if (cd_loop_uv_offset == -1) {
      continue;
    }

    float aspect_y = 1.0f;
    if (params->correct_aspect) {
      float aspx, aspy;
      ED_uvedit_get_aspect(obedit, &aspx, &aspy);
      if (aspx != aspy) {
        aspect_y = aspx / aspy;
      }
    }

    island_list_len += bm_mesh_calc_uv_islands(scene,
                                               bm,
                                               &island_list,
                                               params->only_selected_faces,
                                               params->only_selected_uvs,
                                               params->use_seams,
                                               aspect_y,
                                               cd_loop_uv_offset);
  }

  if (island_list_len == 0) {
    return;
  }

  float margin = scene->toolsettings->uvcalc_margin;
  double area = 0.0f;

  struct FaceIsland **island_array = MEM_mallocN(sizeof(*island_array) * island_list_len,
                                                 __func__);
  BoxPack *boxarray = MEM_mallocN(sizeof(*boxarray) * island_list_len, __func__);

  int index;
  LISTBASE_FOREACH_INDEX (struct FaceIsland *, island, &island_list, index) {

    if (params->rotate) {
      if (island->aspect_y != 1.0f) {
        bm_face_array_uv_scale_y(
            island->faces, island->faces_len, 1.0f / island->aspect_y, island->cd_loop_uv_offset);
      }

      bm_face_array_uv_rotate_fit_aabb(
          island->faces, island->faces_len, rotate_align_axis, island->cd_loop_uv_offset);

      if (island->aspect_y != 1.0f) {
        bm_face_array_uv_scale_y(
            island->faces, island->faces_len, island->aspect_y, island->cd_loop_uv_offset);
      }
    }

    bm_face_array_calc_bounds(
        island->faces, island->faces_len, island->cd_loop_uv_offset, &island->bounds_rect);

    BoxPack *box = &boxarray[index];
    box->index = index;
    box->x = 0.0f;
    box->y = 0.0f;
    box->w = BLI_rctf_size_x(&island->bounds_rect);
    box->h = BLI_rctf_size_y(&island->bounds_rect);

    island_array[index] = island;

    if (margin > 0.0f) {
      area += (double)sqrtf(box->w * box->h);
    }
  }

  if (margin > 0.0f) {
    /* Logic matches behavior from #param_pack,
     * use area so multiply the margin by the area to give
     * predictable results not dependent on UV scale. */
    margin = (margin * (float)area) * 0.1f;
    for (int i = 0; i < island_list_len; i++) {
      struct FaceIsland *island = island_array[i];
      BoxPack *box = &boxarray[i];

      BLI_rctf_pad(&island->bounds_rect, margin, margin);
      box->w = BLI_rctf_size_x(&island->bounds_rect);
      box->h = BLI_rctf_size_y(&island->bounds_rect);
    }
  }

  float boxarray_size[2];
  BLI_box_pack_2d(boxarray, island_list_len, &boxarray_size[0], &boxarray_size[1]);

  /* Don't change the aspect when scaling. */
  boxarray_size[0] = boxarray_size[1] = max_ff(boxarray_size[0], boxarray_size[1]);

  const float scale[2] = {1.0f / boxarray_size[0], 1.0f / boxarray_size[1]};

  for (int i = 0; i < island_list_len; i++) {
    struct FaceIsland *island = island_array[boxarray[i].index];
    const float pivot[2] = {
        island->bounds_rect.xmin,
        island->bounds_rect.ymin,
    };
    const float offset[2] = {
        (boxarray[i].x * scale[0]) - island->bounds_rect.xmin,
        (boxarray[i].y * scale[1]) - island->bounds_rect.ymin,
    };
    for (int j = 0; j < island->faces_len; j++) {
      BMFace *efa = island->faces[j];
      bm_face_uv_translate_and_scale_around_pivot(
          efa, offset, scale, pivot, island->cd_loop_uv_offset);
    }
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
  }

  for (int i = 0; i < island_list_len; i++) {
    MEM_freeN(island_array[i]->faces);
    MEM_freeN(island_array[i]);
  }

  MEM_freeN(island_array);
  MEM_freeN(boxarray);
}

/** \} */
