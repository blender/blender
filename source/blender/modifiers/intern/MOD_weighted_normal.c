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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_linklist.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_library.h"
#include "BKE_mesh.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"

#define CLNORS_VALID_VEC_LEN (1e-6f)

typedef struct ModePair {
  float val; /* Contains mode based value (face area / corner angle). */
  int index; /* Index value per poly or per loop. */
} ModePair;

/* Sorting function used in modifier, sorts in decreasing order. */
static int modepair_cmp_by_val_inverse(const void *p1, const void *p2)
{
  ModePair *r1 = (ModePair *)p1;
  ModePair *r2 = (ModePair *)p2;

  return (r1->val < r2->val) ? 1 : ((r1->val > r2->val) ? -1 : 0);
}

/* There will be one of those per vertex (simple case, computing one normal per vertex), or per smooth fan. */
typedef struct WeightedNormalDataAggregateItem {
  float normal[3];

  int num_loops;     /* Count number of loops using this item so far. */
  float curr_val;    /* Current max val for this item. */
  int curr_strength; /* Current max strength encountered for this item. */
} WeightedNormalDataAggregateItem;

#define NUM_CACHED_INVERSE_POWERS_OF_WEIGHT 128

typedef struct WeightedNormalData {
  const int numVerts;
  const int numEdges;
  const int numLoops;
  const int numPolys;

  MVert *mvert;
  MEdge *medge;

  MLoop *mloop;
  short (*clnors)[2];
  const bool has_clnors; /* True if clnors already existed, false if we had to create them. */
  const float split_angle;

  MPoly *mpoly;
  float (*polynors)[3];
  int *poly_strength;

  MDeformVert *dvert;
  const int defgrp_index;
  const bool use_invert_vgroup;

  const float weight;
  const short mode;

  /* Lower-level, internal processing data. */
  float cached_inverse_powers_of_weight[NUM_CACHED_INVERSE_POWERS_OF_WEIGHT];

  WeightedNormalDataAggregateItem *items_data;

  ModePair *mode_pair;

  int *loop_to_poly;
} WeightedNormalData;

/* Check strength of given poly compared to those found so far for that given item (vertex or smooth fan),
 * and reset matching item_data in case we get a stronger new strength. */
static bool check_item_poly_strength(WeightedNormalData *wn_data,
                                     WeightedNormalDataAggregateItem *item_data,
                                     const int mp_index)
{
  BLI_assert(wn_data->poly_strength != NULL);

  const int mp_strength = wn_data->poly_strength[mp_index];

  if (mp_strength > item_data->curr_strength) {
    item_data->curr_strength = mp_strength;
    item_data->curr_val = 0.0f;
    item_data->num_loops = 0;
    zero_v3(item_data->normal);
  }

  return mp_strength == item_data->curr_strength;
}

static void aggregate_item_normal(WeightedNormalModifierData *wnmd,
                                  WeightedNormalData *wn_data,
                                  WeightedNormalDataAggregateItem *item_data,
                                  const int mv_index,
                                  const int mp_index,
                                  const float curr_val,
                                  const bool use_face_influence)
{
  float(*polynors)[3] = wn_data->polynors;

  MDeformVert *dvert = wn_data->dvert;
  const int defgrp_index = wn_data->defgrp_index;
  const bool use_invert_vgroup = wn_data->use_invert_vgroup;

  const float weight = wn_data->weight;

  float *cached_inverse_powers_of_weight = wn_data->cached_inverse_powers_of_weight;

  const bool has_vgroup = dvert != NULL;
  const bool vert_of_group = has_vgroup &&
                             defvert_find_index(&dvert[mv_index], defgrp_index) != NULL;

  if (has_vgroup &&
      ((vert_of_group && use_invert_vgroup) || (!vert_of_group && !use_invert_vgroup))) {
    return;
  }

  if (use_face_influence && !check_item_poly_strength(wn_data, item_data, mp_index)) {
    return;
  }

  /* If item's curr_val is 0 init it to present value. */
  if (item_data->curr_val == 0.0f) {
    item_data->curr_val = curr_val;
  }
  if (!compare_ff(item_data->curr_val, curr_val, wnmd->thresh)) {
    /* item's curr_val and present value differ more than threshold, update. */
    item_data->num_loops++;
    item_data->curr_val = curr_val;
  }

  /* Exponentially divided weight for each normal (since a few values will be used by most cases, we cache those). */
  const int num_loops = item_data->num_loops;
  if (num_loops < NUM_CACHED_INVERSE_POWERS_OF_WEIGHT &&
      cached_inverse_powers_of_weight[num_loops] == 0.0f) {
    cached_inverse_powers_of_weight[num_loops] = 1.0f / powf(weight, num_loops);
  }
  const float inverted_n_weight = num_loops < NUM_CACHED_INVERSE_POWERS_OF_WEIGHT ?
                                      cached_inverse_powers_of_weight[num_loops] :
                                      1.0f / powf(weight, num_loops);

  madd_v3_v3fl(item_data->normal, polynors[mp_index], curr_val * inverted_n_weight);
}

static void apply_weights_vertex_normal(WeightedNormalModifierData *wnmd,
                                        WeightedNormalData *wn_data)
{
  const int numVerts = wn_data->numVerts;
  const int numEdges = wn_data->numEdges;
  const int numLoops = wn_data->numLoops;
  const int numPolys = wn_data->numPolys;

  MVert *mvert = wn_data->mvert;
  MEdge *medge = wn_data->medge;

  MLoop *mloop = wn_data->mloop;
  short(*clnors)[2] = wn_data->clnors;
  int *loop_to_poly = wn_data->loop_to_poly;

  MPoly *mpoly = wn_data->mpoly;
  float(*polynors)[3] = wn_data->polynors;
  int *poly_strength = wn_data->poly_strength;

  MDeformVert *dvert = wn_data->dvert;

  const short mode = wn_data->mode;
  ModePair *mode_pair = wn_data->mode_pair;

  const bool has_clnors = wn_data->has_clnors;
  const float split_angle = wn_data->split_angle;
  MLoopNorSpaceArray lnors_spacearr = {NULL};

  const bool keep_sharp = (wnmd->flag & MOD_WEIGHTEDNORMAL_KEEP_SHARP) != 0;
  const bool use_face_influence = (wnmd->flag & MOD_WEIGHTEDNORMAL_FACE_INFLUENCE) != 0 &&
                                  poly_strength != NULL;
  const bool has_vgroup = dvert != NULL;

  float(*loop_normals)[3] = NULL;

  WeightedNormalDataAggregateItem *items_data = NULL;
  int num_items = 0;
  if (keep_sharp) {
    BLI_bitmap *done_loops = BLI_BITMAP_NEW(numLoops, __func__);

    /* This will give us loop normal spaces, we do not actually care about computed loop_normals for now... */
    loop_normals = MEM_calloc_arrayN((size_t)numLoops, sizeof(*loop_normals), __func__);
    BKE_mesh_normals_loop_split(mvert,
                                numVerts,
                                medge,
                                numEdges,
                                mloop,
                                loop_normals,
                                numLoops,
                                mpoly,
                                polynors,
                                numPolys,
                                true,
                                split_angle,
                                &lnors_spacearr,
                                has_clnors ? clnors : NULL,
                                loop_to_poly);

    num_items = lnors_spacearr.num_spaces;
    items_data = MEM_calloc_arrayN((size_t)num_items, sizeof(*items_data), __func__);

    /* In this first loop, we assign each WeightedNormalDataAggregateItem
     * to its smooth fan of loops (aka lnor space). */
    MPoly *mp;
    int mp_index;
    int item_index;
    for (mp = mpoly, mp_index = 0, item_index = 0; mp_index < numPolys; mp++, mp_index++) {
      int ml_index = mp->loopstart;
      const int ml_end_index = ml_index + mp->totloop;

      for (; ml_index < ml_end_index; ml_index++) {
        if (BLI_BITMAP_TEST(done_loops, ml_index)) {
          /* Smooth fan of this loop has already been processed, skip it. */
          continue;
        }
        BLI_assert(item_index < num_items);

        WeightedNormalDataAggregateItem *itdt = &items_data[item_index];
        itdt->curr_strength = FACE_STRENGTH_WEAK;

        MLoopNorSpace *lnor_space = lnors_spacearr.lspacearr[ml_index];
        lnor_space->user_data = itdt;

        if (!(lnor_space->flags & MLNOR_SPACE_IS_SINGLE)) {
          for (LinkNode *lnode = lnor_space->loops; lnode; lnode = lnode->next) {
            const int ml_fan_index = POINTER_AS_INT(lnode->link);
            BLI_BITMAP_ENABLE(done_loops, ml_fan_index);
          }
        }
        else {
          BLI_BITMAP_ENABLE(done_loops, ml_index);
        }

        item_index++;
      }
    }

    MEM_freeN(done_loops);
  }
  else {
    num_items = numVerts;
    items_data = MEM_calloc_arrayN((size_t)num_items, sizeof(*items_data), __func__);
    if (use_face_influence) {
      for (int item_index = 0; item_index < num_items; item_index++) {
        items_data[item_index].curr_strength = FACE_STRENGTH_WEAK;
      }
    }
  }
  wn_data->items_data = items_data;

  switch (mode) {
    case MOD_WEIGHTEDNORMAL_MODE_FACE:
      for (int i = 0; i < numPolys; i++) {
        const int mp_index = mode_pair[i].index;
        const float mp_val = mode_pair[i].val;

        int ml_index = mpoly[mp_index].loopstart;
        const int ml_index_end = ml_index + mpoly[mp_index].totloop;
        for (; ml_index < ml_index_end; ml_index++) {
          const int mv_index = mloop[ml_index].v;
          WeightedNormalDataAggregateItem *item_data =
              keep_sharp ? lnors_spacearr.lspacearr[ml_index]->user_data : &items_data[mv_index];

          aggregate_item_normal(
              wnmd, wn_data, item_data, mv_index, mp_index, mp_val, use_face_influence);
        }
      }
      break;
    case MOD_WEIGHTEDNORMAL_MODE_ANGLE:
    case MOD_WEIGHTEDNORMAL_MODE_FACE_ANGLE:
      BLI_assert(loop_to_poly != NULL);

      for (int i = 0; i < numLoops; i++) {
        const int ml_index = mode_pair[i].index;
        const float ml_val = mode_pair[i].val;

        const int mp_index = loop_to_poly[ml_index];
        const int mv_index = mloop[ml_index].v;
        WeightedNormalDataAggregateItem *item_data =
            keep_sharp ? lnors_spacearr.lspacearr[ml_index]->user_data : &items_data[mv_index];

        aggregate_item_normal(
            wnmd, wn_data, item_data, mv_index, mp_index, ml_val, use_face_influence);
      }
      break;
    default:
      BLI_assert(0);
  }

  /* Validate computed weighted normals. */
  for (int item_index = 0; item_index < num_items; item_index++) {
    if (normalize_v3(items_data[item_index].normal) < CLNORS_VALID_VEC_LEN) {
      zero_v3(items_data[item_index].normal);
    }
  }

  if (keep_sharp) {
    /* Set loop normals for normal computed for each lnor space (smooth fan).
     * Note that loop_normals is already populated with clnors (before this modifier is applied, at start of
     * this function), so no need to recompute them here. */
    for (int ml_index = 0; ml_index < numLoops; ml_index++) {
      WeightedNormalDataAggregateItem *item_data = lnors_spacearr.lspacearr[ml_index]->user_data;
      if (!is_zero_v3(item_data->normal)) {
        copy_v3_v3(loop_normals[ml_index], item_data->normal);
      }
    }

    BKE_mesh_normals_loop_custom_set(mvert,
                                     numVerts,
                                     medge,
                                     numEdges,
                                     mloop,
                                     loop_normals,
                                     numLoops,
                                     mpoly,
                                     polynors,
                                     numPolys,
                                     clnors);
  }
  else {
    /* TODO: Ideally, we could add an option to BKE_mesh_normals_loop_custom_[from_vertices_]set() to keep current
     * clnors instead of resetting them to default autocomputed ones, when given new custom normal is zero-vec.
     * But this is not exactly trivial change, better to keep this optimization for later...
     */
    if (!has_vgroup) {
      /* Note: in theory, we could avoid this extra allocation & copying... But think we can live with it for now,
       * and it makes code simpler & cleaner. */
      float(*vert_normals)[3] = MEM_calloc_arrayN(
          (size_t)numVerts, sizeof(*loop_normals), __func__);

      for (int ml_index = 0; ml_index < numLoops; ml_index++) {
        const int mv_index = mloop[ml_index].v;
        copy_v3_v3(vert_normals[mv_index], items_data[mv_index].normal);
      }

      BKE_mesh_normals_loop_custom_from_vertices_set(mvert,
                                                     vert_normals,
                                                     numVerts,
                                                     medge,
                                                     numEdges,
                                                     mloop,
                                                     numLoops,
                                                     mpoly,
                                                     polynors,
                                                     numPolys,
                                                     clnors);

      MEM_freeN(vert_normals);
    }
    else {
      loop_normals = MEM_calloc_arrayN((size_t)numLoops, sizeof(*loop_normals), __func__);

      BKE_mesh_normals_loop_split(mvert,
                                  numVerts,
                                  medge,
                                  numEdges,
                                  mloop,
                                  loop_normals,
                                  numLoops,
                                  mpoly,
                                  polynors,
                                  numPolys,
                                  true,
                                  split_angle,
                                  NULL,
                                  has_clnors ? clnors : NULL,
                                  loop_to_poly);

      for (int ml_index = 0; ml_index < numLoops; ml_index++) {
        const int item_index = mloop[ml_index].v;
        if (!is_zero_v3(items_data[item_index].normal)) {
          copy_v3_v3(loop_normals[ml_index], items_data[item_index].normal);
        }
      }

      BKE_mesh_normals_loop_custom_set(mvert,
                                       numVerts,
                                       medge,
                                       numEdges,
                                       mloop,
                                       loop_normals,
                                       numLoops,
                                       mpoly,
                                       polynors,
                                       numPolys,
                                       clnors);
    }
  }

  if (keep_sharp) {
    BKE_lnor_spacearr_free(&lnors_spacearr);
  }
  MEM_SAFE_FREE(loop_normals);
}

static void wn_face_area(WeightedNormalModifierData *wnmd, WeightedNormalData *wn_data)
{
  const int numPolys = wn_data->numPolys;

  MVert *mvert = wn_data->mvert;
  MLoop *mloop = wn_data->mloop;
  MPoly *mpoly = wn_data->mpoly;

  MPoly *mp;
  int mp_index;

  ModePair *face_area = MEM_malloc_arrayN((size_t)numPolys, sizeof(*face_area), __func__);

  ModePair *f_area = face_area;
  for (mp_index = 0, mp = mpoly; mp_index < numPolys; mp_index++, mp++, f_area++) {
    f_area->val = BKE_mesh_calc_poly_area(mp, &mloop[mp->loopstart], mvert);
    f_area->index = mp_index;
  }

  qsort(face_area, numPolys, sizeof(*face_area), modepair_cmp_by_val_inverse);

  wn_data->mode_pair = face_area;
  apply_weights_vertex_normal(wnmd, wn_data);
}

static void wn_corner_angle(WeightedNormalModifierData *wnmd, WeightedNormalData *wn_data)
{
  const int numLoops = wn_data->numLoops;
  const int numPolys = wn_data->numPolys;

  MVert *mvert = wn_data->mvert;
  MLoop *mloop = wn_data->mloop;
  MPoly *mpoly = wn_data->mpoly;

  MPoly *mp;
  int mp_index;

  int *loop_to_poly = MEM_malloc_arrayN((size_t)numLoops, sizeof(*loop_to_poly), __func__);

  ModePair *corner_angle = MEM_malloc_arrayN((size_t)numLoops, sizeof(*corner_angle), __func__);

  for (mp_index = 0, mp = mpoly; mp_index < numPolys; mp_index++, mp++) {
    MLoop *ml_start = &mloop[mp->loopstart];

    float *index_angle = MEM_malloc_arrayN((size_t)mp->totloop, sizeof(*index_angle), __func__);
    BKE_mesh_calc_poly_angles(mp, ml_start, mvert, index_angle);

    ModePair *c_angl = &corner_angle[mp->loopstart];
    float *angl = index_angle;
    for (int ml_index = mp->loopstart; ml_index < mp->loopstart + mp->totloop;
         ml_index++, c_angl++, angl++) {
      c_angl->val = (float)M_PI - *angl;
      c_angl->index = ml_index;

      loop_to_poly[ml_index] = mp_index;
    }
    MEM_freeN(index_angle);
  }

  qsort(corner_angle, numLoops, sizeof(*corner_angle), modepair_cmp_by_val_inverse);

  wn_data->loop_to_poly = loop_to_poly;
  wn_data->mode_pair = corner_angle;
  apply_weights_vertex_normal(wnmd, wn_data);
}

static void wn_face_with_angle(WeightedNormalModifierData *wnmd, WeightedNormalData *wn_data)
{
  const int numLoops = wn_data->numLoops;
  const int numPolys = wn_data->numPolys;

  MVert *mvert = wn_data->mvert;
  MLoop *mloop = wn_data->mloop;
  MPoly *mpoly = wn_data->mpoly;

  MPoly *mp;
  int mp_index;

  int *loop_to_poly = MEM_malloc_arrayN((size_t)numLoops, sizeof(*loop_to_poly), __func__);

  ModePair *combined = MEM_malloc_arrayN((size_t)numLoops, sizeof(*combined), __func__);

  for (mp_index = 0, mp = mpoly; mp_index < numPolys; mp_index++, mp++) {
    MLoop *ml_start = &mloop[mp->loopstart];

    float face_area = BKE_mesh_calc_poly_area(mp, ml_start, mvert);
    float *index_angle = MEM_malloc_arrayN((size_t)mp->totloop, sizeof(*index_angle), __func__);
    BKE_mesh_calc_poly_angles(mp, ml_start, mvert, index_angle);

    ModePair *cmbnd = &combined[mp->loopstart];
    float *angl = index_angle;
    for (int ml_index = mp->loopstart; ml_index < mp->loopstart + mp->totloop;
         ml_index++, cmbnd++, angl++) {
      /* In this case val is product of corner angle and face area. */
      cmbnd->val = ((float)M_PI - *angl) * face_area;
      cmbnd->index = ml_index;

      loop_to_poly[ml_index] = mp_index;
    }
    MEM_freeN(index_angle);
  }

  qsort(combined, numLoops, sizeof(*combined), modepair_cmp_by_val_inverse);

  wn_data->loop_to_poly = loop_to_poly;
  wn_data->mode_pair = combined;
  apply_weights_vertex_normal(wnmd, wn_data);
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  WeightedNormalModifierData *wnmd = (WeightedNormalModifierData *)md;
  Object *ob = ctx->object;

  /* XXX TODO ARG GRRR XYQWNMPRXTYY
   * Once we fully switch to Mesh evaluation of modifiers, we can expect to get that flag from the COW copy.
   * But for now, it is lost in the DM intermediate step, so we need to directly check orig object's data. */
#if 0
  if (!(mesh->flag & ME_AUTOSMOOTH))
#else
  if (!(((Mesh *)ob->data)->flag & ME_AUTOSMOOTH))
#endif
  {
    modifier_setError((ModifierData *)wnmd, "Enable 'Auto Smooth' option in mesh settings");
    return mesh;
  }

  Mesh *result;
  BKE_id_copy_ex(NULL, &mesh->id, (ID **)&result, LIB_ID_COPY_LOCALIZE);

  const int numVerts = result->totvert;
  const int numEdges = result->totedge;
  const int numLoops = result->totloop;
  const int numPolys = result->totpoly;

  MEdge *medge = result->medge;
  MPoly *mpoly = result->mpoly;
  MVert *mvert = result->mvert;
  MLoop *mloop = result->mloop;

  /* Right now:
 * If weight = 50 then all faces are given equal weight.
 * If weight > 50 then more weight given to faces with larger vals (face area / corner angle).
 * If weight < 50 then more weight given to faces with lesser vals. However current calculation
 * does not converge to min/max.
 */
  float weight = ((float)wnmd->weight) / 50.0f;
  if (wnmd->weight == 100) {
    weight = (float)SHRT_MAX;
  }
  else if (wnmd->weight == 1) {
    weight = 1 / (float)SHRT_MAX;
  }
  else if ((weight - 1) * 25 > 1) {
    weight = (weight - 1) * 25;
  }

  CustomData *pdata = &result->pdata;
  float(*polynors)[3] = CustomData_get_layer(pdata, CD_NORMAL);
  if (!polynors) {
    polynors = CustomData_add_layer(pdata, CD_NORMAL, CD_CALLOC, NULL, numPolys);
    CustomData_set_layer_flag(pdata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }
  BKE_mesh_calc_normals_poly(
      mvert, NULL, numVerts, mloop, mpoly, numLoops, numPolys, polynors, false);

  const float split_angle = mesh->smoothresh;
  short(*clnors)[2];
  CustomData *ldata = &result->ldata;
  clnors = CustomData_get_layer(ldata, CD_CUSTOMLOOPNORMAL);

  /* Keep info  whether we had clnors, it helps when generating clnor spaces and default normals. */
  const bool has_clnors = clnors != NULL;
  if (!clnors) {
    clnors = CustomData_add_layer(ldata, CD_CUSTOMLOOPNORMAL, CD_CALLOC, NULL, numLoops);
  }

  MDeformVert *dvert;
  int defgrp_index;
  MOD_get_vgroup(ctx->object, mesh, wnmd->defgrp_name, &dvert, &defgrp_index);

  WeightedNormalData wn_data = {
      .numVerts = numVerts,
      .numEdges = numEdges,
      .numLoops = numLoops,
      .numPolys = numPolys,

      .mvert = mvert,
      .medge = medge,

      .mloop = mloop,
      .clnors = clnors,
      .has_clnors = has_clnors,
      .split_angle = split_angle,

      .mpoly = mpoly,
      .polynors = polynors,
      .poly_strength = CustomData_get_layer_named(
          &result->pdata, CD_PROP_INT, MOD_WEIGHTEDNORMALS_FACEWEIGHT_CDLAYER_ID),

      .dvert = dvert,
      .defgrp_index = defgrp_index,
      .use_invert_vgroup = (wnmd->flag & MOD_WEIGHTEDNORMAL_INVERT_VGROUP) != 0,

      .weight = weight,
      .mode = wnmd->mode,
  };

  switch (wnmd->mode) {
    case MOD_WEIGHTEDNORMAL_MODE_FACE:
      wn_face_area(wnmd, &wn_data);
      break;
    case MOD_WEIGHTEDNORMAL_MODE_ANGLE:
      wn_corner_angle(wnmd, &wn_data);
      break;
    case MOD_WEIGHTEDNORMAL_MODE_FACE_ANGLE:
      wn_face_with_angle(wnmd, &wn_data);
      break;
  }

  MEM_SAFE_FREE(wn_data.loop_to_poly);
  MEM_SAFE_FREE(wn_data.mode_pair);
  MEM_SAFE_FREE(wn_data.items_data);

  /* Currently Modifier stack assumes there is no poly normal data passed around... */
  CustomData_free_layers(pdata, CD_NORMAL, numPolys);
  return result;
}

static void initData(ModifierData *md)
{
  WeightedNormalModifierData *wnmd = (WeightedNormalModifierData *)md;
  wnmd->mode = MOD_WEIGHTEDNORMAL_MODE_FACE;
  wnmd->weight = 50;
  wnmd->thresh = 1e-2f;
  wnmd->flag = 0;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  WeightedNormalModifierData *wnmd = (WeightedNormalModifierData *)md;

  r_cddata_masks->lmask = CD_MASK_CUSTOMLOOPNORMAL;

  if (wnmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  if (wnmd->flag & MOD_WEIGHTEDNORMAL_FACE_INFLUENCE) {
    r_cddata_masks->pmask |= CD_MASK_PROP_INT;
  }
}

static bool dependsOnNormals(ModifierData *UNUSED(md))
{
  return true;
}

ModifierTypeInfo modifierType_WeightedNormal = {
    /* name */ "Weighted Normal",
    /* structName */ "WeightedNormalModifierData",
    /* structSize */ sizeof(WeightedNormalModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
