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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 *
 * Array modifier: duplicates the object multiple times along an axis.
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object_deform.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

static void initData(ModifierData *md)
{
  ArrayModifierData *amd = (ArrayModifierData *)md;

  /* default to 2 duplicates distributed along the x-axis by an
   * offset of 1 object-width
   */
  amd->start_cap = amd->end_cap = amd->curve_ob = amd->offset_ob = NULL;
  amd->count = 2;
  zero_v3(amd->offset);
  amd->scale[0] = 1;
  amd->scale[1] = amd->scale[2] = 0;
  amd->length = 0;
  amd->merge_dist = 0.01;
  amd->fit_type = MOD_ARR_FIXEDCOUNT;
  amd->offset_type = MOD_ARR_OFF_RELATIVE;
  amd->flags = 0;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  ArrayModifierData *amd = (ArrayModifierData *)md;

  walk(userData, ob, &amd->start_cap, IDWALK_CB_NOP);
  walk(userData, ob, &amd->end_cap, IDWALK_CB_NOP);
  walk(userData, ob, &amd->curve_ob, IDWALK_CB_NOP);
  walk(userData, ob, &amd->offset_ob, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ArrayModifierData *amd = (ArrayModifierData *)md;
  bool need_transform_dependency = false;
  if (amd->start_cap != NULL) {
    DEG_add_object_relation(
        ctx->node, amd->start_cap, DEG_OB_COMP_GEOMETRY, "Array Modifier Start Cap");
  }
  if (amd->end_cap != NULL) {
    DEG_add_object_relation(
        ctx->node, amd->end_cap, DEG_OB_COMP_GEOMETRY, "Array Modifier End Cap");
  }
  if (amd->curve_ob) {
    DEG_add_object_relation(
        ctx->node, amd->curve_ob, DEG_OB_COMP_GEOMETRY, "Array Modifier Curve");
    DEG_add_special_eval_flag(ctx->node, &amd->curve_ob->id, DAG_EVAL_NEED_CURVE_PATH);
  }
  if (amd->offset_ob != NULL) {
    DEG_add_object_relation(
        ctx->node, amd->offset_ob, DEG_OB_COMP_TRANSFORM, "Array Modifier Offset");
    need_transform_dependency = true;
  }

  if (need_transform_dependency) {
    DEG_add_modifier_to_transform_relation(ctx->node, "Array Modifier");
  }
}

BLI_INLINE float sum_v3(const float v[3])
{
  return v[0] + v[1] + v[2];
}

/* Structure used for sorting vertices, when processing doubles */
typedef struct SortVertsElem {
  int vertex_num; /* The original index of the vertex, prior to sorting */
  float co[3];    /* Its coordinates */
  float sum_co;   /* sum_v3(co), just so we don't do the sum many times.  */
} SortVertsElem;

static int svert_sum_cmp(const void *e1, const void *e2)
{
  const SortVertsElem *sv1 = e1;
  const SortVertsElem *sv2 = e2;

  if (sv1->sum_co > sv2->sum_co) {
    return 1;
  }
  else if (sv1->sum_co < sv2->sum_co) {
    return -1;
  }
  else {
    return 0;
  }
}

static void svert_from_mvert(SortVertsElem *sv,
                             const MVert *mv,
                             const int i_begin,
                             const int i_end)
{
  int i;
  for (i = i_begin; i < i_end; i++, sv++, mv++) {
    sv->vertex_num = i;
    copy_v3_v3(sv->co, mv->co);
    sv->sum_co = sum_v3(mv->co);
  }
}

/**
 * Take as inputs two sets of verts, to be processed for detection of doubles and mapping.
 * Each set of verts is defined by its start within mverts array and its num_verts;
 * It builds a mapping for all vertices within source,
 * to vertices within target, or -1 if no double found.
 * The int doubles_map[num_verts_source] array must have been allocated by caller.
 */
static void dm_mvert_map_doubles(int *doubles_map,
                                 const MVert *mverts,
                                 const int target_start,
                                 const int target_num_verts,
                                 const int source_start,
                                 const int source_num_verts,
                                 const float dist)
{
  const float dist3 = ((float)M_SQRT3 + 0.00005f) * dist; /* Just above sqrt(3) */
  int i_source, i_target, i_target_low_bound, target_end, source_end;
  SortVertsElem *sorted_verts_target, *sorted_verts_source;
  SortVertsElem *sve_source, *sve_target, *sve_target_low_bound;
  bool target_scan_completed;

  target_end = target_start + target_num_verts;
  source_end = source_start + source_num_verts;

  /* build array of MVerts to be tested for merging */
  sorted_verts_target = MEM_malloc_arrayN(target_num_verts, sizeof(SortVertsElem), __func__);
  sorted_verts_source = MEM_malloc_arrayN(source_num_verts, sizeof(SortVertsElem), __func__);

  /* Copy target vertices index and cos into SortVertsElem array */
  svert_from_mvert(sorted_verts_target, mverts + target_start, target_start, target_end);

  /* Copy source vertices index and cos into SortVertsElem array */
  svert_from_mvert(sorted_verts_source, mverts + source_start, source_start, source_end);

  /* sort arrays according to sum of vertex coordinates (sumco) */
  qsort(sorted_verts_target, target_num_verts, sizeof(SortVertsElem), svert_sum_cmp);
  qsort(sorted_verts_source, source_num_verts, sizeof(SortVertsElem), svert_sum_cmp);

  sve_target_low_bound = sorted_verts_target;
  i_target_low_bound = 0;
  target_scan_completed = false;

  /* Scan source vertices, in SortVertsElem sorted array, */
  /* all the while maintaining the lower bound of possible doubles in target vertices */
  for (i_source = 0, sve_source = sorted_verts_source; i_source < source_num_verts;
       i_source++, sve_source++) {
    int best_target_vertex = -1;
    float best_dist_sq = dist * dist;
    float sve_source_sumco;

    /* If source has already been assigned to a target (in an earlier call, with other chunks) */
    if (doubles_map[sve_source->vertex_num] != -1) {
      continue;
    }

    /* If target fully scanned already, then all remaining source vertices cannot have a double */
    if (target_scan_completed) {
      doubles_map[sve_source->vertex_num] = -1;
      continue;
    }

    sve_source_sumco = sum_v3(sve_source->co);

    /* Skip all target vertices that are more than dist3 lower in terms of sumco */
    /* and advance the overall lower bound, applicable to all remaining vertices as well. */
    while ((i_target_low_bound < target_num_verts) &&
           (sve_target_low_bound->sum_co < sve_source_sumco - dist3)) {
      i_target_low_bound++;
      sve_target_low_bound++;
    }
    /* If end of target list reached, then no more possible doubles */
    if (i_target_low_bound >= target_num_verts) {
      doubles_map[sve_source->vertex_num] = -1;
      target_scan_completed = true;
      continue;
    }
    /* Test target candidates starting at the low bound of possible doubles,
     * ordered in terms of sumco. */
    i_target = i_target_low_bound;
    sve_target = sve_target_low_bound;

    /* i_target will scan vertices in the
     * [v_source_sumco - dist3;  v_source_sumco + dist3] range */

    while ((i_target < target_num_verts) && (sve_target->sum_co <= sve_source_sumco + dist3)) {
      /* Testing distance for candidate double in target */
      /* v_target is within dist3 of v_source in terms of sumco;  check real distance */
      float dist_sq;
      if ((dist_sq = len_squared_v3v3(sve_source->co, sve_target->co)) <= best_dist_sq) {
        /* Potential double found */
        best_dist_sq = dist_sq;
        best_target_vertex = sve_target->vertex_num;

        /* If target is already mapped, we only follow that mapping if final target remains
         * close enough from current vert (otherwise no mapping at all).
         * Note that if we later find another target closer than this one, then we check it.
         * But if other potential targets are farther,
         * then there will be no mapping at all for this source. */
        while (best_target_vertex != -1 &&
               !ELEM(doubles_map[best_target_vertex], -1, best_target_vertex)) {
          if (compare_len_v3v3(mverts[sve_source->vertex_num].co,
                               mverts[doubles_map[best_target_vertex]].co,
                               dist)) {
            best_target_vertex = doubles_map[best_target_vertex];
          }
          else {
            best_target_vertex = -1;
          }
        }
      }
      i_target++;
      sve_target++;
    }
    /* End of candidate scan: if none found then no doubles */
    doubles_map[sve_source->vertex_num] = best_target_vertex;
  }

  MEM_freeN(sorted_verts_source);
  MEM_freeN(sorted_verts_target);
}

static void mesh_merge_transform(Mesh *result,
                                 Mesh *cap_mesh,
                                 const float cap_offset[4][4],
                                 uint cap_verts_index,
                                 uint cap_edges_index,
                                 int cap_loops_index,
                                 int cap_polys_index,
                                 int cap_nverts,
                                 int cap_nedges,
                                 int cap_nloops,
                                 int cap_npolys,
                                 int *remap,
                                 int remap_len)
{
  int *index_orig;
  int i;
  MVert *mv;
  MEdge *me;
  MLoop *ml;
  MPoly *mp;

  CustomData_copy_data(&cap_mesh->vdata, &result->vdata, 0, cap_verts_index, cap_nverts);
  CustomData_copy_data(&cap_mesh->edata, &result->edata, 0, cap_edges_index, cap_nedges);
  CustomData_copy_data(&cap_mesh->ldata, &result->ldata, 0, cap_loops_index, cap_nloops);
  CustomData_copy_data(&cap_mesh->pdata, &result->pdata, 0, cap_polys_index, cap_npolys);

  mv = result->mvert + cap_verts_index;

  for (i = 0; i < cap_nverts; i++, mv++) {
    mul_m4_v3(cap_offset, mv->co);
    /* Reset MVert flags for caps */
    mv->flag = mv->bweight = 0;
  }

  /* remap the vertex groups if necessary */
  if (result->dvert != NULL) {
    BKE_object_defgroup_index_map_apply(
        &result->dvert[cap_verts_index], cap_nverts, remap, remap_len);
  }

  /* adjust cap edge vertex indices */
  me = result->medge + cap_edges_index;
  for (i = 0; i < cap_nedges; i++, me++) {
    me->v1 += cap_verts_index;
    me->v2 += cap_verts_index;
  }

  /* adjust cap poly loopstart indices */
  mp = result->mpoly + cap_polys_index;
  for (i = 0; i < cap_npolys; i++, mp++) {
    mp->loopstart += cap_loops_index;
  }

  /* adjust cap loop vertex and edge indices */
  ml = result->mloop + cap_loops_index;
  for (i = 0; i < cap_nloops; i++, ml++) {
    ml->v += cap_verts_index;
    ml->e += cap_edges_index;
  }

  /* set origindex */
  index_orig = CustomData_get_layer(&result->vdata, CD_ORIGINDEX);
  if (index_orig) {
    copy_vn_i(index_orig + cap_verts_index, cap_nverts, ORIGINDEX_NONE);
  }

  index_orig = CustomData_get_layer(&result->edata, CD_ORIGINDEX);
  if (index_orig) {
    copy_vn_i(index_orig + cap_edges_index, cap_nedges, ORIGINDEX_NONE);
  }

  index_orig = CustomData_get_layer(&result->pdata, CD_ORIGINDEX);
  if (index_orig) {
    copy_vn_i(index_orig + cap_polys_index, cap_npolys, ORIGINDEX_NONE);
  }

  index_orig = CustomData_get_layer(&result->ldata, CD_ORIGINDEX);
  if (index_orig) {
    copy_vn_i(index_orig + cap_loops_index, cap_nloops, ORIGINDEX_NONE);
  }
}

static Mesh *arrayModifier_doArray(ArrayModifierData *amd,
                                   const ModifierEvalContext *ctx,
                                   Mesh *mesh)
{
  const MVert *src_mvert;
  MVert *mv, *mv_prev, *result_dm_verts;

  MEdge *me;
  MLoop *ml;
  MPoly *mp;
  int i, j, c, count;
  float length = amd->length;
  /* offset matrix */
  float offset[4][4];
  float scale[3];
  bool offset_has_scale;
  float current_offset[4][4];
  float final_offset[4][4];
  int *full_doubles_map = NULL;
  int tot_doubles;

  const bool use_merge = (amd->flags & MOD_ARR_MERGE) != 0;
  const bool use_recalc_normals = (mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) || use_merge;
  const bool use_offset_ob = ((amd->offset_type & MOD_ARR_OFF_OBJ) && amd->offset_ob != NULL);

  int start_cap_nverts = 0, start_cap_nedges = 0, start_cap_npolys = 0, start_cap_nloops = 0;
  int end_cap_nverts = 0, end_cap_nedges = 0, end_cap_npolys = 0, end_cap_nloops = 0;
  int result_nverts = 0, result_nedges = 0, result_npolys = 0, result_nloops = 0;
  int chunk_nverts, chunk_nedges, chunk_nloops, chunk_npolys;
  int first_chunk_start, first_chunk_nverts, last_chunk_start, last_chunk_nverts;

  Mesh *result, *start_cap_mesh = NULL, *end_cap_mesh = NULL;

  int *vgroup_start_cap_remap = NULL;
  int vgroup_start_cap_remap_len = 0;
  int *vgroup_end_cap_remap = NULL;
  int vgroup_end_cap_remap_len = 0;

  chunk_nverts = mesh->totvert;
  chunk_nedges = mesh->totedge;
  chunk_nloops = mesh->totloop;
  chunk_npolys = mesh->totpoly;

  count = amd->count;

  Object *start_cap_ob = amd->start_cap;
  if (start_cap_ob && start_cap_ob != ctx->object) {
    vgroup_start_cap_remap = BKE_object_defgroup_index_map_create(
        start_cap_ob, ctx->object, &vgroup_start_cap_remap_len);

    start_cap_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(start_cap_ob, false);
    if (start_cap_mesh) {
      start_cap_nverts = start_cap_mesh->totvert;
      start_cap_nedges = start_cap_mesh->totedge;
      start_cap_nloops = start_cap_mesh->totloop;
      start_cap_npolys = start_cap_mesh->totpoly;
    }
  }
  Object *end_cap_ob = amd->end_cap;
  if (end_cap_ob && end_cap_ob != ctx->object) {
    vgroup_end_cap_remap = BKE_object_defgroup_index_map_create(
        end_cap_ob, ctx->object, &vgroup_end_cap_remap_len);

    end_cap_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(end_cap_ob, false);
    if (end_cap_mesh) {
      end_cap_nverts = end_cap_mesh->totvert;
      end_cap_nedges = end_cap_mesh->totedge;
      end_cap_nloops = end_cap_mesh->totloop;
      end_cap_npolys = end_cap_mesh->totpoly;
    }
  }

  /* Build up offset array, cumulating all settings options */

  unit_m4(offset);
  src_mvert = mesh->mvert;

  if (amd->offset_type & MOD_ARR_OFF_CONST) {
    add_v3_v3(offset[3], amd->offset);
  }

  if (amd->offset_type & MOD_ARR_OFF_RELATIVE) {
    float min[3], max[3];
    const MVert *src_mv;

    INIT_MINMAX(min, max);
    for (src_mv = src_mvert, j = chunk_nverts; j--; src_mv++) {
      minmax_v3v3_v3(min, max, src_mv->co);
    }

    for (j = 3; j--;) {
      offset[3][j] += amd->scale[j] * (max[j] - min[j]);
    }
  }

  if (use_offset_ob) {
    float obinv[4][4];
    float result_mat[4][4];

    if (ctx->object) {
      invert_m4_m4(obinv, ctx->object->obmat);
    }
    else {
      unit_m4(obinv);
    }

    mul_m4_series(result_mat, offset, obinv, amd->offset_ob->obmat);
    copy_m4_m4(offset, result_mat);
  }

  /* Check if there is some scaling.  If scaling, then we will not translate mapping */
  mat4_to_size(scale, offset);
  offset_has_scale = !is_one_v3(scale);

  if (amd->fit_type == MOD_ARR_FITCURVE && amd->curve_ob != NULL) {
    Object *curve_ob = amd->curve_ob;
    CurveCache *curve_cache = curve_ob->runtime.curve_cache;
    if (curve_cache != NULL && curve_cache->path != NULL) {
      float scale_fac = mat4_to_scale(curve_ob->obmat);
      length = scale_fac * curve_cache->path->totdist;
    }
  }

  /* About 67 million vertices max seems a decent limit for now. */
  const size_t max_num_vertices = 1 << 26;

  /* calculate the maximum number of copies which will fit within the
   * prescribed length */
  if (amd->fit_type == MOD_ARR_FITLENGTH || amd->fit_type == MOD_ARR_FITCURVE) {
    const float float_epsilon = 1e-6f;
    bool offset_is_too_small = false;
    float dist = len_v3(offset[3]);

    if (dist > float_epsilon) {
      /* this gives length = first copy start to last copy end
       * add a tiny offset for floating point rounding errors */
      count = (length + float_epsilon) / dist + 1;

      /* Ensure we keep things to a reasonable level, in terms of rough total amount of generated
       * vertices.
       */
      if (((size_t)count * (size_t)chunk_nverts + (size_t)start_cap_nverts +
           (size_t)end_cap_nverts) > max_num_vertices) {
        count = 1;
        offset_is_too_small = true;
      }
    }
    else {
      /* if the offset has no translation, just make one copy */
      count = 1;
      offset_is_too_small = true;
    }

    if (offset_is_too_small) {
      BKE_modifier_set_error(
          &amd->modifier,
          "The offset is too small, we cannot generate the amount of geometry it would require");
    }
  }
  /* Ensure we keep things to a reasonable level, in terms of rough total amount of generated
   * vertices.
   */
  else if (((size_t)count * (size_t)chunk_nverts + (size_t)start_cap_nverts +
            (size_t)end_cap_nverts) > max_num_vertices) {
    count = 1;
    BKE_modifier_set_error(&amd->modifier,
                           "The amount of copies is too high, we cannot generate the amount of "
                           "geometry it would require");
  }

  if (count < 1) {
    count = 1;
  }

  /* The number of verts, edges, loops, polys, before eventually merging doubles */
  result_nverts = chunk_nverts * count + start_cap_nverts + end_cap_nverts;
  result_nedges = chunk_nedges * count + start_cap_nedges + end_cap_nedges;
  result_nloops = chunk_nloops * count + start_cap_nloops + end_cap_nloops;
  result_npolys = chunk_npolys * count + start_cap_npolys + end_cap_npolys;

  /* Initialize a result dm */
  result = BKE_mesh_new_nomain_from_template(
      mesh, result_nverts, result_nedges, 0, result_nloops, result_npolys);
  result_dm_verts = result->mvert;

  if (use_merge) {
    /* Will need full_doubles_map for handling merge */
    full_doubles_map = MEM_malloc_arrayN(result_nverts, sizeof(int), "mod array doubles map");
    copy_vn_i(full_doubles_map, result_nverts, -1);
  }

  /* copy customdata to original geometry */
  CustomData_copy_data(&mesh->vdata, &result->vdata, 0, 0, chunk_nverts);
  CustomData_copy_data(&mesh->edata, &result->edata, 0, 0, chunk_nedges);
  CustomData_copy_data(&mesh->ldata, &result->ldata, 0, 0, chunk_nloops);
  CustomData_copy_data(&mesh->pdata, &result->pdata, 0, 0, chunk_npolys);

  /* Subsurf for eg won't have mesh data in the custom data arrays.
   * now add mvert/medge/mpoly layers. */
  if (!CustomData_has_layer(&mesh->vdata, CD_MVERT)) {
    memcpy(result->mvert, mesh->mvert, sizeof(*result->mvert) * mesh->totvert);
  }
  if (!CustomData_has_layer(&mesh->edata, CD_MEDGE)) {
    memcpy(result->medge, mesh->medge, sizeof(*result->medge) * mesh->totedge);
  }
  if (!CustomData_has_layer(&mesh->pdata, CD_MPOLY)) {
    memcpy(result->mloop, mesh->mloop, sizeof(*result->mloop) * mesh->totloop);
    memcpy(result->mpoly, mesh->mpoly, sizeof(*result->mpoly) * mesh->totpoly);
  }

  /* Remember first chunk, in case of cap merge */
  first_chunk_start = 0;
  first_chunk_nverts = chunk_nverts;

  unit_m4(current_offset);
  for (c = 1; c < count; c++) {
    /* copy customdata to new geometry */
    CustomData_copy_data(&mesh->vdata, &result->vdata, 0, c * chunk_nverts, chunk_nverts);
    CustomData_copy_data(&mesh->edata, &result->edata, 0, c * chunk_nedges, chunk_nedges);
    CustomData_copy_data(&mesh->ldata, &result->ldata, 0, c * chunk_nloops, chunk_nloops);
    CustomData_copy_data(&mesh->pdata, &result->pdata, 0, c * chunk_npolys, chunk_npolys);

    mv_prev = result_dm_verts;
    mv = mv_prev + c * chunk_nverts;

    /* recalculate cumulative offset here */
    mul_m4_m4m4(current_offset, current_offset, offset);

    /* apply offset to all new verts */
    for (i = 0; i < chunk_nverts; i++, mv++, mv_prev++) {
      mul_m4_v3(current_offset, mv->co);

      /* We have to correct normals too, if we do not tag them as dirty! */
      if (!use_recalc_normals) {
        float no[3];
        normal_short_to_float_v3(no, mv->no);
        mul_mat3_m4_v3(current_offset, no);
        normalize_v3(no);
        normal_float_to_short_v3(mv->no, no);
      }
    }

    /* adjust edge vertex indices */
    me = result->medge + c * chunk_nedges;
    for (i = 0; i < chunk_nedges; i++, me++) {
      me->v1 += c * chunk_nverts;
      me->v2 += c * chunk_nverts;
    }

    mp = result->mpoly + c * chunk_npolys;
    for (i = 0; i < chunk_npolys; i++, mp++) {
      mp->loopstart += c * chunk_nloops;
    }

    /* adjust loop vertex and edge indices */
    ml = result->mloop + c * chunk_nloops;
    for (i = 0; i < chunk_nloops; i++, ml++) {
      ml->v += c * chunk_nverts;
      ml->e += c * chunk_nedges;
    }

    /* Handle merge between chunk n and n-1 */
    if (use_merge && (c >= 1)) {
      if (!offset_has_scale && (c >= 2)) {
        /* Mapping chunk 3 to chunk 2 is a translation of mapping 2 to 1
         * ... that is except if scaling makes the distance grow */
        int k;
        int this_chunk_index = c * chunk_nverts;
        int prev_chunk_index = (c - 1) * chunk_nverts;
        for (k = 0; k < chunk_nverts; k++, this_chunk_index++, prev_chunk_index++) {
          int target = full_doubles_map[prev_chunk_index];
          if (target != -1) {
            target += chunk_nverts; /* translate mapping */
            while (target != -1 && !ELEM(full_doubles_map[target], -1, target)) {
              /* If target is already mapped, we only follow that mapping if final target remains
               * close enough from current vert (otherwise no mapping at all). */
              if (compare_len_v3v3(result_dm_verts[this_chunk_index].co,
                                   result_dm_verts[full_doubles_map[target]].co,
                                   amd->merge_dist)) {
                target = full_doubles_map[target];
              }
              else {
                target = -1;
              }
            }
          }
          full_doubles_map[this_chunk_index] = target;
        }
      }
      else {
        dm_mvert_map_doubles(full_doubles_map,
                             result_dm_verts,
                             (c - 1) * chunk_nverts,
                             chunk_nverts,
                             c * chunk_nverts,
                             chunk_nverts,
                             amd->merge_dist);
      }
    }
  }

  /* handle UVs */
  if (chunk_nloops > 0 && is_zero_v2(amd->uv_offset) == false) {
    const int totuv = CustomData_number_of_layers(&result->ldata, CD_MLOOPUV);
    for (i = 0; i < totuv; i++) {
      MLoopUV *dmloopuv = CustomData_get_layer_n(&result->ldata, CD_MLOOPUV, i);
      dmloopuv += chunk_nloops;
      for (c = 1; c < count; c++) {
        const float uv_offset[2] = {
            amd->uv_offset[0] * (float)c,
            amd->uv_offset[1] * (float)c,
        };
        int l_index = chunk_nloops;
        for (; l_index-- != 0; dmloopuv++) {
          dmloopuv->uv[0] += uv_offset[0];
          dmloopuv->uv[1] += uv_offset[1];
        }
      }
    }
  }

  last_chunk_start = (count - 1) * chunk_nverts;
  last_chunk_nverts = chunk_nverts;

  copy_m4_m4(final_offset, current_offset);

  if (use_merge && (amd->flags & MOD_ARR_MERGEFINAL) && (count > 1)) {
    /* Merge first and last copies */
    dm_mvert_map_doubles(full_doubles_map,
                         result_dm_verts,
                         last_chunk_start,
                         last_chunk_nverts,
                         first_chunk_start,
                         first_chunk_nverts,
                         amd->merge_dist);
  }

  /* start capping */
  if (start_cap_mesh) {
    float start_offset[4][4];
    int start_cap_start = result_nverts - start_cap_nverts - end_cap_nverts;
    invert_m4_m4(start_offset, offset);
    mesh_merge_transform(result,
                         start_cap_mesh,
                         start_offset,
                         result_nverts - start_cap_nverts - end_cap_nverts,
                         result_nedges - start_cap_nedges - end_cap_nedges,
                         result_nloops - start_cap_nloops - end_cap_nloops,
                         result_npolys - start_cap_npolys - end_cap_npolys,
                         start_cap_nverts,
                         start_cap_nedges,
                         start_cap_nloops,
                         start_cap_npolys,
                         vgroup_start_cap_remap,
                         vgroup_start_cap_remap_len);
    /* Identify doubles with first chunk */
    if (use_merge) {
      dm_mvert_map_doubles(full_doubles_map,
                           result_dm_verts,
                           first_chunk_start,
                           first_chunk_nverts,
                           start_cap_start,
                           start_cap_nverts,
                           amd->merge_dist);
    }
  }

  if (end_cap_mesh) {
    float end_offset[4][4];
    int end_cap_start = result_nverts - end_cap_nverts;
    mul_m4_m4m4(end_offset, current_offset, offset);
    mesh_merge_transform(result,
                         end_cap_mesh,
                         end_offset,
                         result_nverts - end_cap_nverts,
                         result_nedges - end_cap_nedges,
                         result_nloops - end_cap_nloops,
                         result_npolys - end_cap_npolys,
                         end_cap_nverts,
                         end_cap_nedges,
                         end_cap_nloops,
                         end_cap_npolys,
                         vgroup_end_cap_remap,
                         vgroup_end_cap_remap_len);
    /* Identify doubles with last chunk */
    if (use_merge) {
      dm_mvert_map_doubles(full_doubles_map,
                           result_dm_verts,
                           last_chunk_start,
                           last_chunk_nverts,
                           end_cap_start,
                           end_cap_nverts,
                           amd->merge_dist);
    }
  }
  /* done capping */

  /* Handle merging */
  tot_doubles = 0;
  if (use_merge) {
    for (i = 0; i < result_nverts; i++) {
      int new_i = full_doubles_map[i];
      if (new_i != -1) {
        /* We have to follow chains of doubles
         * (merge start/end especially is likely to create some),
         * those are not supported at all by BKE_mesh_merge_verts! */
        while (!ELEM(full_doubles_map[new_i], -1, new_i)) {
          new_i = full_doubles_map[new_i];
        }
        if (i == new_i) {
          full_doubles_map[i] = -1;
        }
        else {
          full_doubles_map[i] = new_i;
          tot_doubles++;
        }
      }
    }
    if (tot_doubles > 0) {
      result = BKE_mesh_merge_verts(
          result, full_doubles_map, tot_doubles, MESH_MERGE_VERTS_DUMP_IF_EQUAL);
    }
    MEM_freeN(full_doubles_map);
  }

  /* In case org dm has dirty normals, or we made some merging, mark normals as dirty in new mesh!
   * TODO: we may need to set other dirty flags as well?
   */
  if (use_recalc_normals) {
    result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  }

  if (vgroup_start_cap_remap) {
    MEM_freeN(vgroup_start_cap_remap);
  }
  if (vgroup_end_cap_remap) {
    MEM_freeN(vgroup_end_cap_remap);
  }

  return result;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  ArrayModifierData *amd = (ArrayModifierData *)md;
  return arrayModifier_doArray(amd, ctx, mesh);
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  ArrayModifierData *amd = (ArrayModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the curve/mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */

  if (amd->curve_ob && amd->curve_ob->type != OB_CURVE) {
    return true;
  }
  else if (amd->start_cap && amd->start_cap->type != OB_MESH) {
    return true;
  }
  else if (amd->end_cap && amd->end_cap->type != OB_MESH) {
    return true;
  }

  return false;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "fit_type", 0, NULL, ICON_NONE);

  int fit_type = RNA_enum_get(&ptr, "fit_type");
  if (fit_type == MOD_ARR_FIXEDCOUNT) {
    uiItemR(layout, &ptr, "count", 0, NULL, ICON_NONE);
  }
  else if (fit_type == MOD_ARR_FITLENGTH) {
    uiItemR(layout, &ptr, "fit_length", 0, NULL, ICON_NONE);
  }
  else if (fit_type == MOD_ARR_FITCURVE) {
    uiItemR(layout, &ptr, "curve", 0, NULL, ICON_NONE);
  }

  uiItemS(layout);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &ptr, "start_cap", 0, IFACE_("Cap Start"), ICON_NONE);
  uiItemR(col, &ptr, "end_cap", 0, IFACE_("End"), ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void relative_offset_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_relative_offset", 0, NULL, ICON_NONE);
}

static void relative_offset_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(&ptr, "use_relative_offset"));
  uiItemR(col, &ptr, "relative_offset_displace", 0, IFACE_("Factor"), ICON_NONE);
}

static void constant_offset_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_constant_offset", 0, NULL, ICON_NONE);
}

static void constant_offset_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(&ptr, "use_constant_offset"));
  uiItemR(col, &ptr, "constant_offset_displace", 0, IFACE_("Distance"), ICON_NONE);
}

/**
 * Object offset in a subpanel for consistency with the other offset types.
 */
static void object_offset_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_object_offset", 0, NULL, ICON_NONE);
}

static void object_offset_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(&ptr, "use_object_offset"));
  uiItemR(col, &ptr, "offset_object", 0, IFACE_("Object"), ICON_NONE);
}

static void symmetry_panel_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_merge_vertices", 0, IFACE_("Merge"), ICON_NONE);
}

static void symmetry_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, RNA_boolean_get(&ptr, "use_merge_vertices"));
  uiItemR(col, &ptr, "merge_threshold", 0, IFACE_("Distance"), ICON_NONE);
  uiItemR(col, &ptr, "use_merge_vertices_cap", 0, IFACE_("First and Last Copies"), ICON_NONE);
}

static void uv_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, &ptr, "offset_u", UI_ITEM_R_EXPAND, IFACE_("Offset U"), ICON_NONE);
  uiItemR(col, &ptr, "offset_v", UI_ITEM_R_EXPAND, IFACE_("V"), ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Array, panel_draw);
  modifier_subpanel_register(region_type,
                             "relative_offset",
                             "",
                             relative_offset_header_draw,
                             relative_offset_draw,
                             panel_type);
  modifier_subpanel_register(region_type,
                             "constant_offset",
                             "",
                             constant_offset_header_draw,
                             constant_offset_draw,
                             panel_type);
  modifier_subpanel_register(
      region_type, "object_offset", "", object_offset_header_draw, object_offset_draw, panel_type);
  modifier_subpanel_register(
      region_type, "merge", "", symmetry_panel_header_draw, symmetry_panel_draw, panel_type);
  modifier_subpanel_register(region_type, "uv", "UVs", NULL, uv_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Array = {
    /* name */ "Array",
    /* structName */ "ArrayModifierData",
    /* structSize */ sizeof(ArrayModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_AcceptsCVs,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};
