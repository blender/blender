/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "BLI_linklist_stack.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh_mapping.hh"

#include "ED_image.hh"
#include "ED_mesh.hh"
#include "ED_uvedit.hh"

#include "WM_api.hh" /* For #WM_event_add_notifier to deal with stabilization nodes. */

#include "transform.hh"
#include "transform_convert.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name UVs Transform Creation
 * \{ */

static void UVsToTransData(const float aspect[2],
                           float *uv,
                           const float *center,
                           const float calc_dist,
                           const bool selected,
                           BMLoop *l,
                           TransData *r_td,
                           TransData2D *r_td2d)
{
  /* UV coords are scaled by aspects. this is needed for rotations and
   * proportional editing to be consistent with the stretched UV coords
   * that are displayed. this also means that for display and number-input,
   * and when the UV coords are flushed, these are converted each time. */
  r_td2d->loc[0] = uv[0] * aspect[0];
  r_td2d->loc[1] = uv[1] * aspect[1];
  r_td2d->loc[2] = 0.0f;
  r_td2d->loc2d = uv;

  r_td->flag = 0;
  r_td->loc = r_td2d->loc;
  copy_v2_v2(r_td->center, center ? center : r_td->loc);
  r_td->center[2] = 0.0f;
  copy_v3_v3(r_td->iloc, r_td->loc);

  memset(r_td->axismtx, 0, sizeof(r_td->axismtx));
  r_td->axismtx[2][2] = 1.0f;

  r_td->val = nullptr;

  if (selected) {
    r_td->flag |= TD_SELECTED;
    r_td->dist = 0.0;
  }
  else {
    r_td->dist = calc_dist;
  }
  unit_m3(r_td->mtx);
  unit_m3(r_td->smtx);
  r_td->extra = l;
}

/**
 * \param dists: Store the closest connected distance to selected vertices.
 */
static void uv_set_connectivity_distance(const ToolSettings *ts,
                                         BMesh *bm,
                                         float *dists,
                                         const float aspect[2])
{
#define TMP_LOOP_SELECT_TAG BM_ELEM_TAG_ALT
  /* Mostly copied from #transform_convert_mesh_connectivity_distance. */
  BLI_LINKSTACK_DECLARE(queue, BMLoop *);

  /* Any BM_ELEM_TAG'd loop is added to 'queue_next', this makes sure that we don't add things
   * twice. */
  BLI_LINKSTACK_DECLARE(queue_next, BMLoop *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);

  BMIter fiter, liter;
  BMVert *f;
  BMLoop *l;

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    /* Visible faces was tagged in #createTransUVs. */
    if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      float dist;
      bool uv_vert_sel = uvedit_uv_select_test_ex(ts, bm, l, offsets);

      if (uv_vert_sel) {
        BLI_LINKSTACK_PUSH(queue, l);
        BM_elem_flag_enable(l, TMP_LOOP_SELECT_TAG);
        dist = 0.0f;
      }
      else {
        BM_elem_flag_disable(l, TMP_LOOP_SELECT_TAG);
        dist = FLT_MAX;
      }

      /* Make sure all loops are in a clean tag state. */
      BLI_assert(BM_elem_flag_test(l, BM_ELEM_TAG) == 0);

      int loop_idx = BM_elem_index_get(l);

      dists[loop_idx] = dist;
    }
  }

  /* Need to be very careful of feedback loops here, store previous dist's to avoid feedback. */
  float *dists_prev = static_cast<float *>(MEM_dupallocN(dists));

  do {
    while ((l = BLI_LINKSTACK_POP(queue))) {
      BLI_assert(dists[BM_elem_index_get(l)] != FLT_MAX);

      BMLoop *l_other, *l_connected;
      BMIter l_connected_iter;

      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      float l_uv[2];

      copy_v2_v2(l_uv, luv);
      mul_v2_v2(l_uv, aspect);

      BM_ITER_ELEM (l_other, &liter, l->f, BM_LOOPS_OF_FACE) {
        if (l_other == l) {
          continue;
        }
        float other_uv[2], edge_vec[2];
        float *luv_other = BM_ELEM_CD_GET_FLOAT_P(l_other, offsets.uv);

        copy_v2_v2(other_uv, luv_other);
        mul_v2_v2(other_uv, aspect);

        sub_v2_v2v2(edge_vec, l_uv, other_uv);

        const int i = BM_elem_index_get(l);
        const int i_other = BM_elem_index_get(l_other);
        float dist = len_v2(edge_vec) + dists_prev[i];

        if (dist < dists[i_other]) {
          dists[i_other] = dist;
        }
        else {
          /* The face loop already has a shorter path to it. */
          continue;
        }

        bool other_vert_sel, connected_vert_sel;

        other_vert_sel = BM_elem_flag_test_bool(l_other, TMP_LOOP_SELECT_TAG);

        BM_ITER_ELEM (l_connected, &l_connected_iter, l_other->v, BM_LOOPS_OF_VERT) {
          if (l_connected == l_other) {
            continue;
          }
          /* Visible faces was tagged in #createTransUVs. */
          if (!BM_elem_flag_test(l_connected->f, BM_ELEM_TAG)) {
            continue;
          }

          float *luv_connected = BM_ELEM_CD_GET_FLOAT_P(l_connected, offsets.uv);
          connected_vert_sel = BM_elem_flag_test_bool(l_connected, TMP_LOOP_SELECT_TAG);

          /* Check if this loop is connected in UV space.
           * If the uv loops share the same selection state (if not, they are not connected as
           * they have been ripped or other edit commands have separated them). */
          bool connected = other_vert_sel == connected_vert_sel &&
                           equals_v2v2(luv_other, luv_connected);
          if (!connected) {
            continue;
          }

          /* The loop vert is occupying the same space, so it has the same distance. */
          const int i_connected = BM_elem_index_get(l_connected);
          dists[i_connected] = dist;

          if (BM_elem_flag_test(l_connected, BM_ELEM_TAG) == 0) {
            BM_elem_flag_enable(l_connected, BM_ELEM_TAG);
            BLI_LINKSTACK_PUSH(queue_next, l_connected);
          }
        }
      }
    }

    /* Clear elem flags for the next loop. */
    for (LinkNode *lnk = queue_next; lnk; lnk = lnk->next) {
      BMLoop *l_link = static_cast<BMLoop *>(lnk->link);
      const int i = BM_elem_index_get(l_link);

      BM_elem_flag_disable(l_link, BM_ELEM_TAG);

      /* Store all new dist values. */
      dists_prev[i] = dists[i];
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

  } while (BLI_LINKSTACK_SIZE(queue));

#ifndef NDEBUG
  /* Check that we didn't leave any loops tagged. */
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    /* Visible faces was tagged in #createTransUVs. */
    if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      BLI_assert(BM_elem_flag_test(l, BM_ELEM_TAG) == 0);
    }
  }
#endif

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);

  MEM_freeN(dists_prev);
#undef TMP_LOOP_SELECT_TAG
}

static void createTransUVs(bContext *C, TransInfo *t)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = t->scene;

  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  const bool is_prop_connected = (t->flag & T_PROP_CONNECTED) != 0;
  const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td = nullptr;
    TransData2D *td2d = nullptr;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMFace *efa;
    BMIter iter, liter;
    UvElementMap *elementmap = nullptr;
    struct IslandCenter {
      float co[2];
      int co_num;
    } *island_center = nullptr;
    int count = 0, countsel = 0;
    const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);

    if (!ED_space_image_show_uvedit(sima, tc->obedit)) {
      continue;
    }

    /* Count. */
    if (is_island_center) {
      /* Create element map with island information. */
      elementmap = BM_uv_element_map_create(em->bm, scene, true, false, true, true);
      if (elementmap == nullptr) {
        continue;
      }

      island_center = MEM_calloc_arrayN<IslandCenter>(elementmap->total_islands, __func__);
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!uvedit_face_visible_test(scene, efa)) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        continue;
      }

      BM_elem_flag_enable(efa, BM_ELEM_TAG);
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        /* Make sure that the loop element flag is cleared for when we use it in
         * uv_set_connectivity_distance later. */
        BM_elem_flag_disable(l, BM_ELEM_TAG);
        if (uvedit_uv_select_test(scene, em->bm, l, offsets)) {
          countsel++;

          if (island_center) {
            UvElement *element = BM_uv_element_get(elementmap, l);

            if (element && !element->flag) {
              float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
              add_v2_v2(island_center[element->island].co, luv);
              island_center[element->island].co_num++;
              element->flag = true;
            }
          }
        }

        if (is_prop_edit) {
          count++;
        }
      }
    }

    float *prop_dists = nullptr;

    /* Support other objects using proportional editing to adjust these, unless connected is
     * enabled. */
    if (((is_prop_edit && !is_prop_connected) ? count : countsel) == 0) {
      goto finally;
    }

    if (is_island_center) {
      for (int i = 0; i < elementmap->total_islands; i++) {
        mul_v2_fl(island_center[i].co, 1.0f / island_center[i].co_num);
        mul_v2_v2(island_center[i].co, t->aspect);
      }
    }

    tc->data_len = (is_prop_edit) ? count : countsel;
    tc->data = MEM_calloc_arrayN<TransData>(tc->data_len, "TransObData(UV Editing)");
    /* For each 2d uv coord a 3d vector is allocated, so that they can be
     * treated just as if they were 3d verts. */
    tc->data_2d = MEM_calloc_arrayN<TransData2D>(tc->data_len, "TransObData2D(UV Editing)");

    if (sima->flag & SI_CLIP_UV) {
      t->flag |= T_CLIP_UV;
    }

    td = tc->data;
    td2d = tc->data_2d;

    if (is_prop_connected) {
      prop_dists = MEM_calloc_arrayN<float>(em->bm->totloop, "TransObPropDists(UV Editing)");

      uv_set_connectivity_distance(t->settings, em->bm, prop_dists, t->aspect);
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        const bool selected = uvedit_uv_select_test(scene, em->bm, l, offsets);
        float (*luv)[2];
        const float *center = nullptr;
        float prop_distance = FLT_MAX;

        if (!is_prop_edit && !selected) {
          continue;
        }

        if (is_prop_connected) {
          const int idx = BM_elem_index_get(l);
          prop_distance = prop_dists[idx];
        }

        if (is_island_center) {
          UvElement *element = BM_uv_element_get(elementmap, l);
          if (element) {
            center = island_center[element->island].co;
          }
        }

        luv = (float (*)[2])BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
        UVsToTransData(t->aspect, *luv, center, prop_distance, selected, l, td++, td2d++);
      }
    }

    if (sima->flag & SI_LIVE_UNWRAP) {
      wmWindow *win_modal = CTX_wm_window(C);
      ED_uvedit_live_unwrap_begin(t->scene, tc->obedit, win_modal);
    }

  finally:
    if (is_prop_connected) {
      MEM_SAFE_FREE(prop_dists);
    }
    if (is_island_center) {
      BM_uv_element_map_free(elementmap);

      MEM_freeN(island_center);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVs Transform Flush
 * \{ */

static void flushTransUVs(TransInfo *t)
{
  SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);
  const bool use_pixel_round = ((sima->pixel_round_mode != SI_PIXEL_ROUND_DISABLED) &&
                                (t->state != TRANS_CANCEL));

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData2D *td;
    int a;
    float aspect_inv[2], size[2];

    aspect_inv[0] = 1.0f / t->aspect[0];
    aspect_inv[1] = 1.0f / t->aspect[1];

    if (use_pixel_round) {
      int size_i[2];
      ED_space_image_get_size(sima, &size_i[0], &size_i[1]);
      size[0] = size_i[0];
      size[1] = size_i[1];
    }

    /* Flush to 2d vector from internally used 3d vector. */
    for (a = 0, td = tc->data_2d; a < tc->data_len; a++, td++) {
      td->loc2d[0] = td->loc[0] * aspect_inv[0];
      td->loc2d[1] = td->loc[1] * aspect_inv[1];

      if (use_pixel_round) {
        td->loc2d[0] *= size[0];
        td->loc2d[1] *= size[1];

        switch (sima->pixel_round_mode) {
          case SI_PIXEL_ROUND_CENTER:
            td->loc2d[0] = roundf(td->loc2d[0] - 0.5f) + 0.5f;
            td->loc2d[1] = roundf(td->loc2d[1] - 0.5f) + 0.5f;
            break;
          case SI_PIXEL_ROUND_CORNER:
            td->loc2d[0] = roundf(td->loc2d[0]);
            td->loc2d[1] = roundf(td->loc2d[1]);
            break;
        }

        td->loc2d[0] /= size[0];
        td->loc2d[1] /= size[1];
      }
    }
  }
}

static void recalcData_uv(TransInfo *t)
{
  SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);

  flushTransUVs(t);
  if (sima->flag & SI_LIVE_UNWRAP) {
    ED_uvedit_live_unwrap_re_solve();
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len) {
      DEG_id_tag_update(static_cast<ID *>(tc->obedit->data), ID_RECALC_GEOMETRY);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name API for Vert and Edge Slide
 * \{ */

struct UVGroups {
  int sd_len;

 private:
  Vector<int> groups_offs_buffer_;
  Vector<int> groups_offs_indices_;

 public:
  void init(const TransDataContainer *tc, BMesh *bm, const BMUVOffsets &offsets)
  {
    /* To identify #TransData by the corner, we first need to set all values in `index` to `-1`. */
    BMIter fiter;
    BMIter liter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BMLoop *l;
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        BM_elem_index_set(l, -1);
      }
    }

    /* Now, count and set the index for the corners being transformed. */
    this->sd_len = 0;
    tc->foreach_index_selected([&](const int i) {
      TransData *td = &tc->data[i];
      this->sd_len++;

      BMLoop *l = static_cast<BMLoop *>(td->extra);
      BM_elem_index_set(l, i);
    });
    bm->elem_index_dirty |= BM_LOOP;

    /* Create the groups. */
    groups_offs_buffer_.reserve(this->sd_len);
    groups_offs_indices_.reserve((this->sd_len / 4) + 2);

    TransData *td = tc->data;
    for (int i = 0; i < tc->data_len; i++, td++) {
      BMLoop *l_orig = static_cast<BMLoop *>(td->extra);
      if (BM_elem_index_get(l_orig) == -1) {
        /* Already added to a group. */
        continue;
      }

      const float2 uv_orig = BM_ELEM_CD_GET_FLOAT_P(l_orig, offsets.uv);
      groups_offs_indices_.append(groups_offs_buffer_.size());

      BMIter liter;
      BMLoop *l_iter;
      BM_ITER_ELEM (l_iter, &liter, l_orig->v, BM_LOOPS_OF_VERT) {
        if (BM_elem_index_get(l_iter) == -1) {
          /* Already added to a group or not participating in the transformation. */
          continue;
        }

        if (l_orig != l_iter &&
            !compare_v2v2(uv_orig, BM_ELEM_CD_GET_FLOAT_P(l_iter, offsets.uv), FLT_EPSILON))
        {
          /* Non-connected. */
          continue;
        }

        groups_offs_buffer_.append(BM_elem_index_get(l_iter));
        BM_elem_index_set(l_iter, -1);
      }
    }
    groups_offs_indices_.append(groups_offs_buffer_.size());
  }

  OffsetIndices<int> groups() const
  {
    return OffsetIndices<int>(groups_offs_indices_);
  }

  Span<int> td_indices_get(const int group_index) const
  {
    return groups_offs_buffer_.as_span().slice(this->groups()[group_index]);
  }

  Array<TransDataVertSlideVert> sd_array_create_and_init(TransDataContainer *tc)
  {
    Array<TransDataVertSlideVert> sv_array(this->sd_len);
    TransDataVertSlideVert *sv = sv_array.data();
    for (const int group_index : this->groups().index_range()) {
      for (int td_index : this->td_indices_get(group_index)) {
        TransData *td = &tc->data[td_index];
        sv->td = td;
        sv++;
      }
    }

    return sv_array;
  }

  Array<TransDataEdgeSlideVert> sd_array_create_and_init_edge(TransDataContainer *tc)
  {
    Array<TransDataEdgeSlideVert> sv_array(this->sd_len);
    TransDataEdgeSlideVert *sv = sv_array.data();
    for (const int group_index : this->groups().index_range()) {
      for (int td_index : this->td_indices_get(group_index)) {
        TransData *td = &tc->data[td_index];
        sv->td = td;
        sv->dir_side[0] = float3(0);
        sv->dir_side[1] = float3(0);
        sv->loop_nr = -1;
        sv++;
      }
    }

    return sv_array;
  }

  MutableSpan<TransDataVertSlideVert> sd_group_get(MutableSpan<TransDataVertSlideVert> sd_array,
                                                   const int group_index)
  {
    return sd_array.slice(this->groups()[group_index]);
  }

  MutableSpan<TransDataEdgeSlideVert> sd_group_get(MutableSpan<TransDataEdgeSlideVert> sd_array,
                                                   const int group_index)
  {
    return sd_array.slice(this->groups()[group_index]);
  }
};

static UVGroups *mesh_uv_groups_get(TransDataContainer *tc, BMesh *bm, const BMUVOffsets &offsets)
{
  UVGroups *uv_groups = static_cast<UVGroups *>(tc->custom.type.data);
  if (uv_groups == nullptr) {
    uv_groups = MEM_new<UVGroups>(__func__);
    uv_groups->init(tc, bm, offsets);

    /* Edge Slide and Vert Slide are often called in sequence, so, to avoid recalculating the
     * groups, save them in the #TransDataContainer. */

    tc->custom.type.data = uv_groups;
    tc->custom.type.free_cb = [](TransInfo *, TransDataContainer *, TransCustomData *custom_data) {
      UVGroups *data = static_cast<UVGroups *>(custom_data->data);
      MEM_delete(data);
      custom_data->data = nullptr;
    };
  }

  return uv_groups;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name API for Vert Slide
 * \{ */

Array<TransDataVertSlideVert> transform_mesh_uv_vert_slide_data_create(
    const TransInfo *t, TransDataContainer *tc, Vector<float3> &r_loc_dst_buffer)
{

  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);

  UVGroups *uv_groups = mesh_uv_groups_get(tc, bm, offsets);

  Array<TransDataVertSlideVert> sv_array = uv_groups->sd_array_create_and_init(tc);

  r_loc_dst_buffer.reserve(sv_array.size() * 4);

  for (const int group_index : uv_groups->groups().index_range()) {
    const int size_prev = r_loc_dst_buffer.size();

    for (int td_index : uv_groups->td_indices_get(group_index)) {
      TransData *td = &tc->data[td_index];
      BMLoop *l = static_cast<BMLoop *>(td->extra);

      for (BMLoop *l_dst : {l->prev, l->next}) {
        const float2 uv_dest = BM_ELEM_CD_GET_FLOAT_P(l_dst, offsets.uv);
        Span<float3> uvs_added = r_loc_dst_buffer.as_span().drop_front(size_prev);

        bool skip = std::any_of(
            uvs_added.begin(), uvs_added.end(), [&](const float3 &uv_dest_added) {
              return compare_v2v2(uv_dest, uv_dest_added, FLT_EPSILON);
            });

        if (!skip) {
          r_loc_dst_buffer.append(float3(uv_dest, 0.0f));
        }
      }
    }

    const int size_new = r_loc_dst_buffer.size() - size_prev;
    for (TransDataVertSlideVert &sv : uv_groups->sd_group_get(sv_array, group_index)) {
      /* The buffer address may change as the vector is resized. Avoid setting #Span now. */
      // sv.targets = r_loc_dst_buffer.as_span().drop_front(size_prev);

      /* Store the buffer slice temporarily in `target_curr`. */
      sv.co_link_orig_3d = {static_cast<float3 *>(POINTER_FROM_INT(size_prev)), size_new};
      sv.co_link_curr = 0;
    }
  }

  if (t->aspect[0] != 1.0f || t->aspect[1] != 1.0f) {
    for (float3 &dest : r_loc_dst_buffer) {
      dest[0] *= t->aspect[0];
      dest[1] *= t->aspect[1];
    }
  }

  for (TransDataVertSlideVert &sv : sv_array) {
    int start = POINTER_AS_INT(sv.co_link_orig_3d.data());
    sv.co_link_orig_3d = r_loc_dst_buffer.as_span().slice(start, sv.co_link_orig_3d.size());
  }

  return sv_array;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name API for Edge Slide
 * \{ */

/* Check if the UV group is a vertex between 2 faces. */
static bool mesh_uv_group_is_inner(const TransDataContainer *tc,
                                   const BMUVOffsets &offsets,
                                   Span<int> group)
{
  if (group.size() == 1) {
    return false;
  }
  if (group.size() > 2) {
    return false;
  }

  TransData *td_a = &tc->data[group[0]];
  TransData *td_b = &tc->data[group[1]];
  BMLoop *l_a = static_cast<BMLoop *>(td_a->extra);
  BMLoop *l_b = static_cast<BMLoop *>(td_b->extra);
  BMLoop *l_a_prev = l_a->prev;
  BMLoop *l_a_next = l_a->next;
  BMLoop *l_b_prev = l_b->next;
  BMLoop *l_b_next = l_b->prev;
  if (l_a_prev->v != l_b_prev->v) {
    std::swap(l_b_prev, l_b_next);
    if (l_a_prev->v != l_b_prev->v) {
      return false;
    }
  }

  if (l_a_next->v != l_b_next->v) {
    return false;
  }

  const float2 uv_a_prev = BM_ELEM_CD_GET_FLOAT_P(l_a_prev, offsets.uv);
  const float2 uv_b_prev = BM_ELEM_CD_GET_FLOAT_P(l_b_prev, offsets.uv);
  if (!compare_v2v2(uv_a_prev, uv_b_prev, FLT_EPSILON)) {
    return false;
  }

  const float2 uv_a_next = BM_ELEM_CD_GET_FLOAT_P(l_a_next, offsets.uv);
  const float2 uv_b_next = BM_ELEM_CD_GET_FLOAT_P(l_b_next, offsets.uv);
  if (!compare_v2v2(uv_a_next, uv_b_next, FLT_EPSILON)) {
    return false;
  }

  return true;
}

/**
 * Find the closest point on the ngon on the opposite side.
 * used to set the edge slide distance for ngons.
 */
static bool bm_loop_uv_calc_opposite_co(const BMLoop *l_tmp,
                                        const float2 &uv_tmp,
                                        const BMUVOffsets &offsets,
                                        const float2 &ray_direction,
                                        float2 &r_co)
{
  /* skip adjacent edges */
  BMLoop *l_first = l_tmp->next;
  BMLoop *l_last = l_tmp->prev;
  BMLoop *l_iter;
  float dist_sq_best = FLT_MAX;
  bool found = false;

  l_iter = l_first;
  do {
    const float2 uv_iter = BM_ELEM_CD_GET_FLOAT_P(l_iter, offsets.uv);
    const float2 uv_iter_next = BM_ELEM_CD_GET_FLOAT_P(l_iter->next, offsets.uv);
    float lambda;
    if (isect_ray_seg_v2(uv_tmp, ray_direction, uv_iter, uv_iter_next, &lambda, nullptr) ||
        isect_ray_seg_v2(uv_tmp, -ray_direction, uv_iter, uv_iter_next, &lambda, nullptr))
    {
      float2 isect_co = uv_tmp + ray_direction * lambda;
      /* likelihood of multiple intersections per ngon is quite low,
       * it would have to loop back on itself, but better support it
       * so check for the closest opposite edge */
      const float dist_sq_test = math::distance_squared(uv_tmp, isect_co);
      if (dist_sq_test < dist_sq_best) {
        r_co = isect_co;
        dist_sq_best = dist_sq_test;
        found = true;
      }
    }
  } while ((l_iter = l_iter->next) != l_last);

  return found;
}

static float2 isect_face_dst(const BMLoop *l,
                             const float2 &uv,
                             const float2 &aspect,
                             const BMUVOffsets &offsets)
{
  BMFace *f = l->f;
  BMLoop *l_next = l->next;
  if (f->len == 4) {
    /* we could use code below, but in this case
     * sliding diagonally across the quad works well */
    return BM_ELEM_CD_GET_FLOAT_P(l_next->next, offsets.uv);
  }

  BMLoop *l_prev = l->prev;
  const float2 uv_prev = BM_ELEM_CD_GET_FLOAT_P(l_prev, offsets.uv);
  const float2 uv_next = BM_ELEM_CD_GET_FLOAT_P(l_next, offsets.uv);

  float2 ray_dir = (uv - uv_prev) + (uv_next - uv);
  ray_dir = math::orthogonal(ray_dir * aspect);
  ray_dir[0] /= aspect[0];
  ray_dir[1] /= aspect[1];

  float2 isect_co;
  if (!bm_loop_uv_calc_opposite_co(l, uv, offsets, ray_dir, isect_co)) {
    /* Rare case. */
    mid_v3_v3v3(isect_co, l->prev->v->co, l_next->v->co);
  }
  return isect_co;
}

Array<TransDataEdgeSlideVert> transform_mesh_uv_edge_slide_data_create(const TransInfo *t,
                                                                       TransDataContainer *tc,
                                                                       int *r_group_len)
{
  Array<TransDataEdgeSlideVert> sv_array;
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);

  const bool check_edge = ED_uvedit_select_mode_get(t->scene) == UV_SELECT_EDGE;

  UVGroups *uv_groups = mesh_uv_groups_get(tc, bm, offsets);
  Array<int2> groups_linked(uv_groups->groups().size(), int2(-1, -1));

  {
    /* Identify the group to which a loop belongs through the element's index value. */

    /* First we just need to "clean up" the neighboring loops.
     * This way we can identify where a group of sliding edges starts and where it ends. */
    tc->foreach_index_selected([&](const int i) {
      TransData *td = &tc->data[i];
      BMLoop *l = static_cast<BMLoop *>(td->extra);
      BM_elem_index_set(l->prev, -1);
      BM_elem_index_set(l->next, -1);
    });

    /* Now set the group indexes. */
    for (const int group_index : uv_groups->groups().index_range()) {
      for (int td_index : uv_groups->td_indices_get(group_index)) {
        TransData *td = &tc->data[td_index];
        BMLoop *l = static_cast<BMLoop *>(td->extra);
        BM_elem_index_set(l, group_index);
      }
    }
    bm->elem_index_dirty |= BM_LOOP;
  }

  for (const int group_index : uv_groups->groups().index_range()) {
    int2 &group_linked_pair = groups_linked[group_index];

    for (int td_index : uv_groups->td_indices_get(group_index)) {
      TransData *td = &tc->data[td_index];
      BMLoop *l = static_cast<BMLoop *>(td->extra);

      for (BMLoop *l_dst : {l->prev, l->next}) {
        const int group_index_dst = BM_elem_index_get(l_dst);
        if (group_index_dst == -1) {
          continue;
        }

        if (ELEM(group_index_dst, group_linked_pair[0], group_linked_pair[1])) {
          continue;
        }

        if (check_edge) {
          BMLoop *l_edge = l_dst == l->prev ? l_dst : l;
          if (!uvedit_edge_select_test_ex(t->settings, bm, l_edge, offsets)) {
            continue;
          }
        }

        if (group_linked_pair[1] != -1) {
          /* For Edge Slide, the vertex can only be connected to a maximum of 2 sliding edges. */
          return sv_array;
        }
        const int slot = int(group_linked_pair[0] != -1);
        group_linked_pair[slot] = group_index_dst;
      }
    }

    if (group_linked_pair[0] == -1) {
      /* For Edge Slide, the vertex must be connected to at least 1 sliding edge. */
      return sv_array;
    }
  }

  /* Alloc and initialize the #TransDataEdgeSlideVert. */
  sv_array = uv_groups->sd_array_create_and_init_edge(tc);

  /* Compute the sliding groups. */
  int loop_nr = 0;
  for (int i : sv_array.index_range()) {
    if (sv_array[i].loop_nr != -1) {
      /* This vertex has already been computed. */
      continue;
    }

    BMLoop *l = static_cast<BMLoop *>(sv_array[i].td->extra);
    int group_index = BM_elem_index_get(l);

    /* Start from a vertex connected to just a single edge or any if it doesn't exist. */
    int i_curr = group_index;
    int i_prev = groups_linked[group_index][1];
    while (!ELEM(i_prev, -1, group_index)) {
      int tmp = groups_linked[i_prev][0] != i_curr ? groups_linked[i_prev][0] :
                                                     groups_linked[i_prev][1];
      i_curr = i_prev;
      i_prev = tmp;
    }

    /**
     * We need at least 3 points to calculate the intersection of
     * `prev`-`curr` and `next`-`curr` destinations.
     *
     *  |         |         |
     *  |         |         |
     * prev ---- curr ---- next
     */
    struct SlideTempDataUV {
      int i; /* The group index. */
      struct {
        BMFace *f;
        float2 dst;
      } fdata[2];
      bool vert_is_inner; /* In the middle of two faces. */
      /**
       * Find the best direction to slide among the ones already computed.
       *
       * \param curr_prev: prev state of the #SlideTempDataUV where the faces are linked to the
       * previous edge.
       * \param l_src: the source corner in the edge to slide.
       * \param l_dst: the current destination corner.
       */
      int find_best_dir(const SlideTempDataUV *curr_side_other,
                        const BMLoop *l_src,
                        const BMLoop *l_dst,
                        const float2 &src,
                        const float2 &dst,
                        bool *r_do_isect_curr_dirs) const
      {
        *r_do_isect_curr_dirs = false;
        const BMFace *f_curr = l_src->f;
        if (curr_side_other->fdata[0].f &&
            (curr_side_other->fdata[0].f == f_curr ||
             compare_v2v2(dst, curr_side_other->fdata[0].dst, FLT_EPSILON)))
        {
          return 0;
        }

        if (curr_side_other->fdata[1].f &&
            (curr_side_other->fdata[1].f == f_curr ||
             compare_v2v2(dst, curr_side_other->fdata[1].dst, FLT_EPSILON)))
        {
          return 1;
        }

        if (curr_side_other->fdata[0].f || curr_side_other->fdata[1].f) {
          /* Find the best direction checking the edges that share faces between them. */
          int best_dir = -1;
          const BMLoop *l_edge_dst = l_src->prev == l_dst ? l_src->prev : l_src;
          const BMLoop *l_other = l_edge_dst->radial_next;
          while (l_other != l_edge_dst) {
            const BMLoop *l_other_dst = l_other->v == l_src->v ? l_other->next : l_other;
            if (BM_elem_index_get(l_other_dst) != -1) {
              /* This is a sliding edge corner. */
              break;
            }

            if (l_other->f == curr_side_other->fdata[0].f) {
              best_dir = 0;
              break;
            }
            if (l_other->f == curr_side_other->fdata[1].f) {
              best_dir = 1;
              break;
            }
            l_other = (l_other->v == l_src->v ? l_other->prev : l_other->next)->radial_next;
          }

          if (best_dir != -1) {
            *r_do_isect_curr_dirs = true;
            return best_dir;
          }
        }

        if (ELEM(nullptr, this->fdata[0].f, this->fdata[1].f)) {
          return int(this->fdata[0].f != nullptr);
        }

        /* Find the closest direction. */
        *r_do_isect_curr_dirs = true;

        float2 dir_curr = dst - src;
        float2 dir0 = math::normalize(this->fdata[0].dst - src);
        float2 dir1 = math::normalize(this->fdata[1].dst - src);
        float dot0 = math::dot(dir_curr, dir0);
        float dot1 = math::dot(dir_curr, dir1);
        return int(dot0 < dot1);
      }
    } prev = {}, curr = {}, next = {}, tmp = {};

    curr.i = i_curr;
    curr.vert_is_inner = mesh_uv_group_is_inner(tc, offsets, uv_groups->td_indices_get(curr.i));

    /* Do not compute `prev` for now. Let the loop calculate `curr` twice. */
    prev.i = -1;

    while (curr.i != -1) {
      int tmp_i = prev.i == -1 ? i_prev : prev.i;
      next.i = groups_linked[curr.i][0] != tmp_i ? groups_linked[curr.i][0] :
                                                   groups_linked[curr.i][1];
      if (next.i != -1) {
        next.vert_is_inner = mesh_uv_group_is_inner(
            tc, offsets, uv_groups->td_indices_get(next.i));

        tmp = curr;
        Span<int> td_indices_next = uv_groups->td_indices_get(next.i);

        for (int td_index_curr : uv_groups->td_indices_get(curr.i)) {
          BMLoop *l_curr = static_cast<BMLoop *>(tc->data[td_index_curr].extra);
          const float2 src = BM_ELEM_CD_GET_FLOAT_P(l_curr, offsets.uv);

          for (int td_index_next : td_indices_next) {
            BMLoop *l_next = static_cast<BMLoop *>(tc->data[td_index_next].extra);
            if (l_curr->f != l_next->f) {
              continue;
            }

            BLI_assert(l_curr != l_next);

            BMLoop *l1_dst, *l2_dst;
            if (l_curr->next == l_next) {
              l1_dst = l_curr->prev;
              l2_dst = l_next->next;
            }
            else {
              l1_dst = l_curr->next;
              l2_dst = l_next->prev;
            }

            const float2 dst = BM_ELEM_CD_GET_FLOAT_P(l1_dst, offsets.uv);

            /* Sometimes the sliding direction may fork (`isect_curr_dirs` is `true`).
             * In this case, the resulting direction is the intersection of the destinations. */
            bool isect_curr_dirs = false;

            /* Identify the slot to slide according to the directions already computed in `curr`.
             */
            int best_dir = curr.find_best_dir(&tmp, l_curr, l1_dst, src, dst, &isect_curr_dirs);

            if (curr.fdata[best_dir].f == nullptr) {
              curr.fdata[best_dir].f = l_curr->f;
              if (curr.vert_is_inner) {
                curr.fdata[best_dir].dst = isect_face_dst(l_curr, src, t->aspect, offsets);
              }
              else {
                curr.fdata[best_dir].dst = dst;
              }
            }

            /* Compute `next`. */
            next.fdata[best_dir].f = l_curr->f;
            if (BM_elem_index_get(l2_dst) != -1 || next.vert_is_inner) {
              /* Case where the vertex slides over the face. */
              const float2 src_next = BM_ELEM_CD_GET_FLOAT_P(l_next, offsets.uv);
              next.fdata[best_dir].dst = isect_face_dst(l_next, src_next, t->aspect, offsets);
            }
            else {
              /* Case where the vertex slides over an edge. */
              const float2 dst_next = BM_ELEM_CD_GET_FLOAT_P(l2_dst, offsets.uv);
              next.fdata[best_dir].dst = dst_next;
            }

            if (isect_curr_dirs) {
              /* The `best_dir` can only have one direction. */
              const float2 &dst0 = prev.fdata[best_dir].dst;
              const float2 &dst1 = curr.fdata[best_dir].dst;
              const float2 &dst2 = dst;
              const float2 &dst3 = next.fdata[best_dir].dst;
              if (isect_line_line_v2_point(dst0, dst1, dst2, dst3, curr.fdata[best_dir].dst) ==
                  ISECT_LINE_LINE_COLINEAR)
              {
                curr.fdata[best_dir].dst = math::midpoint(dst1, dst2);
              }
            }
            /* There is only one pair of corners to slide per face, we don't need to keep checking
             * `if (f_curr != l_next->f)`. */
            break;
          }
        }
      }

      TransDataEdgeSlideVert *sv_first = nullptr;
      for (TransDataEdgeSlideVert &sv : uv_groups->sd_group_get(sv_array, curr.i)) {
        if (sv_first) {
          TransData *td = sv.td;
          sv = *sv_first;
          sv.td = td;
        }
        else {
          sv_first = &sv;
          float2 iloc = sv.td->iloc;
          const float2 aspect = t->aspect;
          if (curr.fdata[0].f) {
            float2 dst = curr.fdata[0].dst * aspect;
            sv.dir_side[0] = float3(dst - iloc, 0.0f);
          }
          if (curr.fdata[1].f) {
            float2 dst = curr.fdata[1].dst * aspect;
            sv.dir_side[1] = float3(dst - iloc, 0.0f);
          }
          sv.edge_len = math::distance(sv.dir_side[0], sv.dir_side[1]);
          sv.loop_nr = loop_nr;
        }
      }

      if (i_prev != -1 && prev.i == i_prev) {
        /* Cycle returned to the beginning.
         * The data with index `i_curr` was computed twice to make sure the directions are
         * correct the second time. */
        break;
      }

      /* Move forward. */
      prev = curr;
      curr = next;
      next.fdata[0].f = next.fdata[1].f = nullptr;
    }
    loop_nr++;
  }
  *r_group_len = loop_nr;
  return sv_array;
}

/** \} */

TransConvertTypeInfo TransConvertType_MeshUV = {
    /*flags*/ (T_EDIT | T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransUVs,
    /*recalc_data*/ recalcData_uv,
    /*special_aftertrans_update*/ nullptr,
};

}  // namespace blender::ed::transform
