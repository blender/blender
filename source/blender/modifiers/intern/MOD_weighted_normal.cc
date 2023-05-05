/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_linklist.h"
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"
#include "MOD_util.hh"

#include "bmesh.h"

#define CLNORS_VALID_VEC_LEN (1e-6f)

struct ModePair {
  float val; /* Contains mode based value (face area / corner angle). */
  int index; /* Index value per poly or per loop. */
};

/* Sorting function used in modifier, sorts in decreasing order. */
static int modepair_cmp_by_val_inverse(const void *p1, const void *p2)
{
  ModePair *r1 = (ModePair *)p1;
  ModePair *r2 = (ModePair *)p2;

  return (r1->val < r2->val) ? 1 : ((r1->val > r2->val) ? -1 : 0);
}

/* There will be one of those per vertex
 * (simple case, computing one normal per vertex), or per smooth fan. */
struct WeightedNormalDataAggregateItem {
  float normal[3];

  int loops_num;     /* Count number of loops using this item so far. */
  float curr_val;    /* Current max val for this item. */
  int curr_strength; /* Current max strength encountered for this item. */
};

#define NUM_CACHED_INVERSE_POWERS_OF_WEIGHT 128

struct WeightedNormalData {
  int verts_num;

  blender::Span<blender::float3> vert_positions;
  blender::Span<blender::float3> vert_normals;
  blender::Span<blender::int2> edges;
  blender::MutableSpan<bool> sharp_edges;

  blender::Span<int> corner_verts;
  blender::Span<int> corner_edges;
  blender::Span<int> loop_to_poly;
  blender::MutableSpan<blender::short2> clnors;
  bool has_clnors; /* True if clnors already existed, false if we had to create them. */
  float split_angle;

  blender::OffsetIndices<int> polys;
  blender::Span<blender::float3> poly_normals;
  const bool *sharp_faces;
  const int *poly_strength;

  const MDeformVert *dvert;
  int defgrp_index;
  bool use_invert_vgroup;

  float weight;
  short mode;

  /* Lower-level, internal processing data. */
  float cached_inverse_powers_of_weight[NUM_CACHED_INVERSE_POWERS_OF_WEIGHT];

  blender::Span<WeightedNormalDataAggregateItem> items_data;

  ModePair *mode_pair;
};

/**
 * Check strength of given poly compared to those found so far for that given item
 * (vertex or smooth fan), and reset matching item_data in case we get a stronger new strength.
 */
static bool check_item_poly_strength(WeightedNormalData *wn_data,
                                     WeightedNormalDataAggregateItem *item_data,
                                     const int poly_index)
{
  BLI_assert(wn_data->poly_strength != nullptr);

  const int mp_strength = wn_data->poly_strength[poly_index];

  if (mp_strength > item_data->curr_strength) {
    item_data->curr_strength = mp_strength;
    item_data->curr_val = 0.0f;
    item_data->loops_num = 0;
    zero_v3(item_data->normal);
  }

  return mp_strength == item_data->curr_strength;
}

static void aggregate_item_normal(WeightedNormalModifierData *wnmd,
                                  WeightedNormalData *wn_data,
                                  WeightedNormalDataAggregateItem *item_data,
                                  const int mv_index,
                                  const int poly_index,
                                  const float curr_val,
                                  const bool use_face_influence)
{
  const blender::Span<blender::float3> poly_normals = wn_data->poly_normals;

  const MDeformVert *dvert = wn_data->dvert;
  const int defgrp_index = wn_data->defgrp_index;
  const bool use_invert_vgroup = wn_data->use_invert_vgroup;

  const float weight = wn_data->weight;

  float *cached_inverse_powers_of_weight = wn_data->cached_inverse_powers_of_weight;

  const bool has_vgroup = dvert != nullptr;
  const bool vert_of_group = has_vgroup &&
                             BKE_defvert_find_index(&dvert[mv_index], defgrp_index) != nullptr;

  if (has_vgroup &&
      ((vert_of_group && use_invert_vgroup) || (!vert_of_group && !use_invert_vgroup))) {
    return;
  }

  if (use_face_influence && !check_item_poly_strength(wn_data, item_data, poly_index)) {
    return;
  }

  /* If item's curr_val is 0 init it to present value. */
  if (item_data->curr_val == 0.0f) {
    item_data->curr_val = curr_val;
  }
  if (!compare_ff(item_data->curr_val, curr_val, wnmd->thresh)) {
    /* item's curr_val and present value differ more than threshold, update. */
    item_data->loops_num++;
    item_data->curr_val = curr_val;
  }

  /* Exponentially divided weight for each normal
   * (since a few values will be used by most cases, we cache those). */
  const int loops_num = item_data->loops_num;
  if (loops_num < NUM_CACHED_INVERSE_POWERS_OF_WEIGHT &&
      cached_inverse_powers_of_weight[loops_num] == 0.0f)
  {
    cached_inverse_powers_of_weight[loops_num] = 1.0f / powf(weight, loops_num);
  }
  const float inverted_n_weight = loops_num < NUM_CACHED_INVERSE_POWERS_OF_WEIGHT ?
                                      cached_inverse_powers_of_weight[loops_num] :
                                      1.0f / powf(weight, loops_num);

  madd_v3_v3fl(item_data->normal, poly_normals[poly_index], curr_val * inverted_n_weight);
}

static void apply_weights_vertex_normal(WeightedNormalModifierData *wnmd,
                                        WeightedNormalData *wn_data)
{
  using namespace blender;
  const int verts_num = wn_data->verts_num;

  const blender::Span<blender::float3> positions = wn_data->vert_positions;
  const blender::Span<int2> edges = wn_data->edges;
  const blender::OffsetIndices polys = wn_data->polys;
  const blender::Span<int> corner_verts = wn_data->corner_verts;
  const blender::Span<int> corner_edges = wn_data->corner_edges;

  MutableSpan<short2> clnors = wn_data->clnors;
  const blender::Span<int> loop_to_poly = wn_data->loop_to_poly;

  const blender::Span<blender::float3> poly_normals = wn_data->poly_normals;
  const int *poly_strength = wn_data->poly_strength;

  const MDeformVert *dvert = wn_data->dvert;

  const short mode = wn_data->mode;
  ModePair *mode_pair = wn_data->mode_pair;

  const bool has_clnors = wn_data->has_clnors;
  const float split_angle = wn_data->split_angle;
  MLoopNorSpaceArray lnors_spacearr = {nullptr};

  const bool keep_sharp = (wnmd->flag & MOD_WEIGHTEDNORMAL_KEEP_SHARP) != 0;
  const bool use_face_influence = (wnmd->flag & MOD_WEIGHTEDNORMAL_FACE_INFLUENCE) != 0 &&
                                  poly_strength != nullptr;
  const bool has_vgroup = dvert != nullptr;

  blender::Array<blender::float3> loop_normals;

  Array<WeightedNormalDataAggregateItem> items_data;
  Array<int> item_index_per_corner;
  if (keep_sharp) {
    BLI_bitmap *done_loops = BLI_BITMAP_NEW(corner_verts.size(), __func__);

    /* This will give us loop normal spaces,
     * we do not actually care about computed loop_normals for now... */
    loop_normals.reinitialize(corner_verts.size());
    bke::mesh::normals_calc_loop(positions,
                                 edges,
                                 polys,
                                 corner_verts,
                                 corner_edges,
                                 loop_to_poly,
                                 wn_data->vert_normals,
                                 wn_data->poly_normals,
                                 wn_data->sharp_edges.data(),
                                 wn_data->sharp_faces,
                                 true,
                                 split_angle,
                                 has_clnors ? clnors.data() : nullptr,
                                 &lnors_spacearr,
                                 loop_normals);

    item_index_per_corner.reinitialize(corner_verts.size());
    items_data = Array<WeightedNormalDataAggregateItem>(lnors_spacearr.spaces_num,
                                                        WeightedNormalDataAggregateItem{});

    /* In this first loop, we assign each WeightedNormalDataAggregateItem
     * to its smooth fan of loops (aka lnor space). */
    int item_index = 0;
    for (const int i : polys.index_range()) {
      for (const int ml_index : polys[i]) {
        if (BLI_BITMAP_TEST(done_loops, ml_index)) {
          /* Smooth fan of this loop has already been processed, skip it. */
          continue;
        }

        WeightedNormalDataAggregateItem *itdt = &items_data[item_index];
        itdt->curr_strength = FACE_STRENGTH_WEAK;

        MLoopNorSpace *lnor_space = lnors_spacearr.lspacearr[ml_index];
        item_index_per_corner[ml_index] = item_index;

        if (!(lnor_space->flags & MLNOR_SPACE_IS_SINGLE)) {
          for (LinkNode *lnode = lnor_space->loops; lnode; lnode = lnode->next) {
            const int ml_fan_index = POINTER_AS_INT(lnode->link);
            item_index_per_corner[ml_fan_index] = item_index;
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
    items_data = Array<WeightedNormalDataAggregateItem>(verts_num,
                                                        WeightedNormalDataAggregateItem{});
    if (use_face_influence) {
      for (int item_index : items_data.index_range()) {
        items_data[item_index].curr_strength = FACE_STRENGTH_WEAK;
      }
    }
  }
  wn_data->items_data = items_data;

  switch (mode) {
    case MOD_WEIGHTEDNORMAL_MODE_FACE:
      for (const int i : polys.index_range()) {
        const int poly_index = mode_pair[i].index;
        const float mp_val = mode_pair[i].val;

        for (const int ml_index : polys[poly_index]) {
          const int mv_index = corner_verts[ml_index];
          WeightedNormalDataAggregateItem *item_data =
              keep_sharp ? &items_data[item_index_per_corner[ml_index]] : &items_data[mv_index];

          aggregate_item_normal(
              wnmd, wn_data, item_data, mv_index, poly_index, mp_val, use_face_influence);
        }
      }
      break;
    case MOD_WEIGHTEDNORMAL_MODE_ANGLE:
    case MOD_WEIGHTEDNORMAL_MODE_FACE_ANGLE:
      for (int i = 0; i < corner_verts.size(); i++) {
        const int ml_index = mode_pair[i].index;
        const float ml_val = mode_pair[i].val;

        const int poly_index = loop_to_poly[ml_index];
        const int mv_index = corner_verts[ml_index];
        WeightedNormalDataAggregateItem *item_data =
            keep_sharp ? &items_data[item_index_per_corner[ml_index]] : &items_data[mv_index];

        aggregate_item_normal(
            wnmd, wn_data, item_data, mv_index, poly_index, ml_val, use_face_influence);
      }
      break;
    default:
      BLI_assert_unreachable();
  }

  /* Validate computed weighted normals. */
  for (int item_index : items_data.index_range()) {
    if (normalize_v3(items_data[item_index].normal) < CLNORS_VALID_VEC_LEN) {
      zero_v3(items_data[item_index].normal);
    }
  }

  if (keep_sharp) {
    /* Set loop normals for normal computed for each lnor space (smooth fan).
     * Note that loop_normals is already populated with clnors
     * (before this modifier is applied, at start of this function),
     * so no need to recompute them here. */
    for (int ml_index = 0; ml_index < corner_verts.size(); ml_index++) {
      WeightedNormalDataAggregateItem *item_data = &items_data[item_index_per_corner[ml_index]];
      if (!is_zero_v3(item_data->normal)) {
        copy_v3_v3(loop_normals[ml_index], item_data->normal);
      }
    }

    blender::bke::mesh::normals_loop_custom_set(positions,
                                                edges,
                                                polys,
                                                corner_verts,
                                                corner_edges,
                                                wn_data->vert_normals,
                                                poly_normals,
                                                wn_data->sharp_faces,
                                                wn_data->sharp_edges,
                                                loop_normals,
                                                clnors);
  }
  else {
    /* TODO: Ideally, we could add an option to `BKE_mesh_normals_loop_custom_[from_verts_]set()`
     * to keep current clnors instead of resetting them to default auto-computed ones,
     * when given new custom normal is zero-vec.
     * But this is not exactly trivial change, better to keep this optimization for later...
     */
    if (!has_vgroup) {
      /* NOTE: in theory, we could avoid this extra allocation & copying...
       * But think we can live with it for now,
       * and it makes code simpler & cleaner. */
      blender::Array<blender::float3> vert_normals(verts_num, float3(0.0f));

      for (int ml_index = 0; ml_index < corner_verts.size(); ml_index++) {
        const int mv_index = corner_verts[ml_index];
        copy_v3_v3(vert_normals[mv_index], items_data[mv_index].normal);
      }

      blender::bke::mesh::normals_loop_custom_set_from_verts(positions,
                                                             edges,
                                                             polys,
                                                             corner_verts,
                                                             corner_edges,
                                                             wn_data->vert_normals,
                                                             poly_normals,
                                                             wn_data->sharp_faces,
                                                             wn_data->sharp_edges,
                                                             vert_normals,
                                                             clnors);
    }
    else {
      loop_normals.reinitialize(corner_verts.size());
      blender::bke::mesh::normals_calc_loop(positions,
                                            edges,
                                            polys,
                                            corner_verts,
                                            corner_edges,
                                            loop_to_poly,
                                            wn_data->vert_normals,
                                            poly_normals,
                                            wn_data->sharp_edges.data(),
                                            wn_data->sharp_faces,
                                            true,
                                            split_angle,
                                            has_clnors ? clnors.data() : nullptr,
                                            nullptr,
                                            loop_normals);

      for (int ml_index = 0; ml_index < corner_verts.size(); ml_index++) {
        const int item_index = corner_verts[ml_index];
        if (!is_zero_v3(items_data[item_index].normal)) {
          copy_v3_v3(loop_normals[ml_index], items_data[item_index].normal);
        }
      }
      blender::bke::mesh::normals_loop_custom_set(positions,
                                                  edges,
                                                  polys,
                                                  corner_verts,
                                                  corner_edges,
                                                  wn_data->vert_normals,
                                                  poly_normals,
                                                  wn_data->sharp_faces,
                                                  wn_data->sharp_edges,
                                                  loop_normals,
                                                  clnors);
    }
  }

  if (keep_sharp) {
    BKE_lnor_spacearr_free(&lnors_spacearr);
  }
}

static void wn_face_area(WeightedNormalModifierData *wnmd, WeightedNormalData *wn_data)
{
  const blender::Span<blender::float3> positions = wn_data->vert_positions;
  const blender::OffsetIndices polys = wn_data->polys;
  const blender::Span<int> corner_verts = wn_data->corner_verts;

  ModePair *face_area = static_cast<ModePair *>(
      MEM_malloc_arrayN(polys.size(), sizeof(*face_area), __func__));

  ModePair *f_area = face_area;
  for (const int i : polys.index_range()) {
    f_area[i].val = blender::bke::mesh::poly_area_calc(positions, corner_verts.slice(polys[i]));
    f_area[i].index = i;
  }

  qsort(face_area, polys.size(), sizeof(*face_area), modepair_cmp_by_val_inverse);

  wn_data->mode_pair = face_area;
  apply_weights_vertex_normal(wnmd, wn_data);
}

static void wn_corner_angle(WeightedNormalModifierData *wnmd, WeightedNormalData *wn_data)
{
  const blender::Span<blender::float3> positions = wn_data->vert_positions;
  const blender::OffsetIndices polys = wn_data->polys;
  const blender::Span<int> corner_verts = wn_data->corner_verts;

  ModePair *corner_angle = static_cast<ModePair *>(
      MEM_malloc_arrayN(corner_verts.size(), sizeof(*corner_angle), __func__));

  for (const int i : polys.index_range()) {
    const blender::IndexRange poly = polys[i];
    float *index_angle = static_cast<float *>(
        MEM_malloc_arrayN(poly.size(), sizeof(*index_angle), __func__));
    blender::bke::mesh::poly_angles_calc(
        positions, corner_verts.slice(poly), {index_angle, poly.size()});

    ModePair *c_angl = &corner_angle[poly.start()];
    float *angl = index_angle;
    for (int ml_index = poly.start(); ml_index < poly.start() + poly.size();
         ml_index++, c_angl++, angl++)
    {
      c_angl->val = float(M_PI) - *angl;
      c_angl->index = ml_index;
    }
    MEM_freeN(index_angle);
  }

  qsort(corner_angle, corner_verts.size(), sizeof(*corner_angle), modepair_cmp_by_val_inverse);

  wn_data->mode_pair = corner_angle;
  apply_weights_vertex_normal(wnmd, wn_data);
}

static void wn_face_with_angle(WeightedNormalModifierData *wnmd, WeightedNormalData *wn_data)
{
  const blender::Span<blender::float3> positions = wn_data->vert_positions;
  const blender::OffsetIndices polys = wn_data->polys;
  const blender::Span<int> corner_verts = wn_data->corner_verts;

  ModePair *combined = static_cast<ModePair *>(
      MEM_malloc_arrayN(corner_verts.size(), sizeof(*combined), __func__));

  for (const int i : polys.index_range()) {
    const blender::IndexRange poly = polys[i];
    const blender::Span<int> poly_verts = corner_verts.slice(poly);
    const float face_area = blender::bke::mesh::poly_area_calc(positions, poly_verts);
    float *index_angle = static_cast<float *>(
        MEM_malloc_arrayN(size_t(poly.size()), sizeof(*index_angle), __func__));
    blender::bke::mesh::poly_angles_calc(positions, poly_verts, {index_angle, poly.size()});

    ModePair *cmbnd = &combined[poly.start()];
    float *angl = index_angle;
    for (int ml_index = poly.start(); ml_index < poly.start() + poly.size();
         ml_index++, cmbnd++, angl++)
    {
      /* In this case val is product of corner angle and face area. */
      cmbnd->val = (float(M_PI) - *angl) * face_area;
      cmbnd->index = ml_index;
    }
    MEM_freeN(index_angle);
  }

  qsort(combined, corner_verts.size(), sizeof(*combined), modepair_cmp_by_val_inverse);

  wn_data->mode_pair = combined;
  apply_weights_vertex_normal(wnmd, wn_data);
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  using namespace blender;
  WeightedNormalModifierData *wnmd = (WeightedNormalModifierData *)md;
  Object *ob = ctx->object;

  /* XXX TODO(Rohan Rathi):
   * Once we fully switch to Mesh evaluation of modifiers,
   * we can expect to get that flag from the COW copy.
   * But for now, it is lost in the DM intermediate step,
   * so we need to directly check orig object's data. */
#if 0
  if (!(mesh->flag & ME_AUTOSMOOTH))
#else
  if (!(((Mesh *)ob->data)->flag & ME_AUTOSMOOTH))
#endif
  {
    BKE_modifier_set_error(
        ctx->object, (ModifierData *)wnmd, "Enable 'Auto Smooth' in Object Data Properties");
    return mesh;
  }

  Mesh *result;
  result = (Mesh *)BKE_id_copy_ex(nullptr, &mesh->id, nullptr, LIB_ID_COPY_LOCALIZE);

  const int verts_num = result->totvert;
  const blender::Span<blender::float3> positions = mesh->vert_positions();
  const blender::Span<int2> edges = mesh->edges();
  const OffsetIndices polys = result->polys();
  const blender::Span<int> corner_verts = mesh->corner_verts();
  const blender::Span<int> corner_edges = mesh->corner_edges();

  /* Right now:
   * If weight = 50 then all faces are given equal weight.
   * If weight > 50 then more weight given to faces with larger vals (face area / corner angle).
   * If weight < 50 then more weight given to faces with lesser vals. However current calculation
   * does not converge to min/max.
   */
  float weight = float(wnmd->weight) / 50.0f;
  if (wnmd->weight == 100) {
    weight = float(SHRT_MAX);
  }
  else if (wnmd->weight == 1) {
    weight = 1 / float(SHRT_MAX);
  }
  else if ((weight - 1) * 25 > 1) {
    weight = (weight - 1) * 25;
  }

  const float split_angle = mesh->smoothresh;
  blender::short2 *clnors = static_cast<blender::short2 *>(
      CustomData_get_layer_for_write(&result->ldata, CD_CUSTOMLOOPNORMAL, mesh->totloop));

  /* Keep info whether we had clnors,
   * it helps when generating clnor spaces and default normals. */
  const bool has_clnors = clnors != nullptr;
  if (!clnors) {
    clnors = static_cast<blender::short2 *>(CustomData_add_layer(
        &result->ldata, CD_CUSTOMLOOPNORMAL, CD_SET_DEFAULT, corner_verts.size()));
  }

  const MDeformVert *dvert;
  int defgrp_index;
  MOD_get_vgroup(ctx->object, mesh, wnmd->defgrp_name, &dvert, &defgrp_index);

  const Array<int> loop_to_poly_map = bke::mesh_topology::build_loop_to_poly_map(result->polys());

  bke::MutableAttributeAccessor attributes = result->attributes_for_write();
  bke::SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_edge", ATTR_DOMAIN_EDGE);

  WeightedNormalData wn_data{};
  wn_data.verts_num = verts_num;

  wn_data.vert_positions = positions;
  wn_data.vert_normals = result->vert_normals();
  wn_data.edges = edges;
  wn_data.sharp_edges = sharp_edges.span;

  wn_data.corner_verts = corner_verts;
  wn_data.corner_edges = corner_edges;
  wn_data.loop_to_poly = loop_to_poly_map;
  wn_data.clnors = {clnors, mesh->totloop};
  wn_data.has_clnors = has_clnors;
  wn_data.split_angle = split_angle;

  wn_data.polys = polys;
  wn_data.poly_normals = mesh->poly_normals();
  wn_data.sharp_faces = static_cast<const bool *>(
      CustomData_get_layer_named(&mesh->pdata, CD_PROP_BOOL, "sharp_face"));
  wn_data.poly_strength = static_cast<const int *>(CustomData_get_layer_named(
      &result->pdata, CD_PROP_INT32, MOD_WEIGHTEDNORMALS_FACEWEIGHT_CDLAYER_ID));

  wn_data.dvert = dvert;
  wn_data.defgrp_index = defgrp_index;
  wn_data.use_invert_vgroup = (wnmd->flag & MOD_WEIGHTEDNORMAL_INVERT_VGROUP) != 0;

  wn_data.weight = weight;
  wn_data.mode = wnmd->mode;

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

  MEM_SAFE_FREE(wn_data.mode_pair);

  result->runtime->is_original_bmesh = false;

  sharp_edges.finish();

  return result;
}

static void initData(ModifierData *md)
{
  WeightedNormalModifierData *wnmd = (WeightedNormalModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wnmd, modifier));

  MEMCPY_STRUCT_AFTER(wnmd, DNA_struct_default_get(WeightedNormalModifierData), modifier);
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  WeightedNormalModifierData *wnmd = (WeightedNormalModifierData *)md;

  r_cddata_masks->lmask = CD_MASK_CUSTOMLOOPNORMAL;

  if (wnmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  if (wnmd->flag & MOD_WEIGHTEDNORMAL_FACE_INFLUENCE) {
    r_cddata_masks->pmask |= CD_MASK_PROP_INT32;
  }
}

static bool dependsOnNormals(ModifierData * /*md*/)
{
  return true;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", 0, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "weight", 0, IFACE_("Weight"), ICON_NONE);
  uiItemR(layout, ptr, "thresh", 0, IFACE_("Threshold"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "keep_sharp", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_face_influence", 0, nullptr, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_WeightedNormal, panel_draw);
}

ModifierTypeInfo modifierType_WeightedNormal = {
    /*name*/ N_("WeightedNormal"),
    /*structName*/ "WeightedNormalModifierData",
    /*structSize*/ sizeof(WeightedNormalModifierData),
    /*srna*/ &RNA_WeightedNormalModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_NORMALEDIT,

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ modifyMesh,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ nullptr,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ dependsOnNormals,
    /*foreachIDLink*/ nullptr,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
