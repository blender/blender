/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Particle API for render engines
 */

#include "BLI_color.hh"
#include "DNA_collection_types.h"
#include "DNA_curves_types.h"
#include "DNA_scene_types.h"
#include "DRW_render.hh"

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_offset_indices.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "ED_particle.hh"

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_material.hh"

#include "DEG_depsgraph_query.hh"

#include "IMB_colormanagement.hh"

#include "draw_attributes.hh"
#include "draw_cache_impl.hh" /* own include */
#include "draw_hair_private.hh"

namespace blender::draw {

static void particle_batch_cache_clear(ParticleSystem *psys);

/* ---------------------------------------------------------------------- */
/* Particle gpu::Batch Cache */

struct ParticleHairFinalCache {
  /* Output of the subdivision stage: vertex buff sized to subdiv level. */
  blender::gpu::VertBuf *proc_buf;

  /* Just contains a huge index buffer used to draw the final hair. */
  blender::gpu::Batch *proc_hairs[MAX_THICKRES];

  int strands_res; /* points per hair, at least 2 */
};

struct ParticleHairCache {
  blender::gpu::VertBuf *pos;
  blender::gpu::IndexBuf *indices;
  blender::gpu::Batch *hairs;
  int strands_len;
  int elems_len;
  int point_len;

  /* Equivalent to the new Curves data structure.
   * Allows to create the eval cache. */
  Vector<int> points_by_curve_storage;
  Vector<int> evaluated_points_by_curve_storage;

  CurvesEvalCache eval_cache;
};

struct ParticlePointCache {
  gpu::VertBuf *pos;
  gpu::Batch *points;
  int elems_len;
  int point_len;
};

struct ParticleBatchCache {
  /* Object mode strands for hair and points for particle,
   * strands for paths when in edit mode.
   */
  ParticleHairCache hair;   /* Used for hair strands */
  ParticlePointCache point; /* Used for particle points. */

  /* Control points when in edit mode. */
  ParticleHairCache edit_hair;

  gpu::VertBuf *edit_pos;
  gpu::Batch *edit_strands;

  gpu::VertBuf *edit_inner_pos;
  gpu::Batch *edit_inner_points;
  int edit_inner_point_len;

  gpu::VertBuf *edit_tip_pos;
  gpu::Batch *edit_tip_points;
  int edit_tip_point_len;

  /* Settings to determine if cache is invalid. */
  bool is_dirty;
  bool edit_is_weight;
};

/* gpu::Batch cache management. */

struct HairAttributeID {
  uint pos;
  uint tan;
  uint ind;
};

struct EditStrandData {
  float pos[3];
  float selection;
};

static const GPUVertFormat *edit_points_vert_format_get(uint *r_pos_id, uint *r_selection_id)
{
  static uint pos_id, selection_id;
  static const GPUVertFormat edit_point_format = [&]() {
    GPUVertFormat format{};
    pos_id = GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32_32);
    selection_id = GPU_vertformat_attr_add(&format, "selection", gpu::VertAttrType::SFLOAT_32);
    return format;
  }();
  *r_pos_id = pos_id;
  *r_selection_id = selection_id;
  return &edit_point_format;
}

static bool particle_batch_cache_valid(ParticleSystem *psys)
{
  ParticleBatchCache *cache = static_cast<ParticleBatchCache *>(psys->batch_cache);

  if (cache == nullptr) {
    return false;
  }

  if (cache->is_dirty == false) {
    return true;
  }

  return false;

  return true;
}

static void particle_batch_cache_init(ParticleSystem *psys)
{
  ParticleBatchCache *cache = static_cast<ParticleBatchCache *>(psys->batch_cache);

  if (!cache) {
    cache = MEM_new<ParticleBatchCache>(__func__);
    psys->batch_cache = cache;
  }
  else {
    cache->edit_hair.eval_cache = {};
    cache->hair.eval_cache = {};
  }

  cache->is_dirty = false;
}

static ParticleBatchCache *particle_batch_cache_get(ParticleSystem *psys)
{
  if (!particle_batch_cache_valid(psys)) {
    particle_batch_cache_clear(psys);
    particle_batch_cache_init(psys);
  }
  return static_cast<ParticleBatchCache *>(psys->batch_cache);
}

void DRW_particle_batch_cache_dirty_tag(ParticleSystem *psys, int mode)
{
  ParticleBatchCache *cache = static_cast<ParticleBatchCache *>(psys->batch_cache);
  if (cache == nullptr) {
    return;
  }
  switch (mode) {
    case BKE_PARTICLE_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    default:
      BLI_assert(0);
  }
}

static void particle_batch_cache_clear_point(ParticlePointCache *point_cache)
{
  GPU_BATCH_DISCARD_SAFE(point_cache->points);
  GPU_VERTBUF_DISCARD_SAFE(point_cache->pos);
}

static void particle_batch_cache_clear_hair(ParticleHairCache *hair_cache)
{
  /* TODO: more granular update tagging. */

  /* "Normal" legacy hairs */
  GPU_BATCH_DISCARD_SAFE(hair_cache->hairs);
  GPU_VERTBUF_DISCARD_SAFE(hair_cache->pos);
  GPU_INDEXBUF_DISCARD_SAFE(hair_cache->indices);

  hair_cache->evaluated_points_by_curve_storage.clear();
  hair_cache->points_by_curve_storage.clear();

  hair_cache->eval_cache.clear();
}

static void particle_batch_cache_clear(ParticleSystem *psys)
{
  ParticleBatchCache *cache = static_cast<ParticleBatchCache *>(psys->batch_cache);
  if (!cache) {
    return;
  }

  /* All memory allocated by `cache` must be freed. */

  particle_batch_cache_clear_point(&cache->point);

  particle_batch_cache_clear_hair(&cache->hair);
  particle_batch_cache_clear_hair(&cache->edit_hair);

  GPU_BATCH_DISCARD_SAFE(cache->edit_inner_points);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_inner_pos);
  GPU_BATCH_DISCARD_SAFE(cache->edit_tip_points);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_tip_pos);
}

void DRW_particle_batch_cache_free(ParticleSystem *psys)
{
  particle_batch_cache_clear(psys);
  ParticleBatchCache *batch_cache = static_cast<ParticleBatchCache *>(psys->batch_cache);
  MEM_delete(batch_cache);
  psys->batch_cache = nullptr;
}

void ParticleSpans::foreach_strand(FunctionRef<void(Span<ParticleCacheKey>)> callback)
{
  for (const auto &particle : parent) {
    callback(Span<ParticleCacheKey>(particle, particle->segments + 1));
  }
  for (const auto &particle : children) {
    callback(Span<ParticleCacheKey>(particle, particle->segments + 1));
  }
}

ParticleSpans ParticleDrawSource::particles_get()
{
  if (edit && edit->pathcache) {
    /* Edit particles only display their parent. */
    return {{edit->pathcache, edit->totcached}, {}};
  }

  ParticleSpans spans;
  const bool display_parent = !psys->childcache || (psys->part->draw & PART_DRAW_PARENT);
  if (psys->pathcache && display_parent) {
    spans.parent = {psys->pathcache, psys->totpart};
  }

  if (psys->childcache) {
    spans.children = {psys->childcache, psys->totchild * psys->part->disp / 100};
  }
  return spans;
}

OffsetIndices<int> ParticleDrawSource::points_by_curve()
{
  if (!points_by_curve_storage_.is_empty()) {
    return points_by_curve_storage_.as_span();
  }

  int total = 0;
  points_by_curve_storage_.append(total);
  particles_get().foreach_strand([&](Span<ParticleCacheKey> strand) {
    total += strand.size();
    points_by_curve_storage_.append(total);
  });
  return points_by_curve_storage_.as_span();
}

OffsetIndices<int> ParticleDrawSource::evaluated_points_by_curve()
{
  if (additional_subdivision_ == 0) {
    return points_by_curve();
  }

  if (!evaluated_points_by_curve_storage_.is_empty()) {
    return evaluated_points_by_curve_storage_.as_span();
  }
  int segment_multiplier = this->resolution();

  int total = 0;
  evaluated_points_by_curve_storage_.append(total);
  particles_get().foreach_strand([&](Span<ParticleCacheKey> strand) {
    int size = strand.size();
    total += (size > 1) ? size * segment_multiplier : 1;
    evaluated_points_by_curve_storage_.append(total);
  });
  return evaluated_points_by_curve_storage_.as_span();
}

static void count_cache_segment_keys(ParticleCacheKey **pathcache,
                                     const int num_path_cache_keys,
                                     ParticleHairCache *hair_cache)
{
  for (int i = 0; i < num_path_cache_keys; i++) {
    ParticleCacheKey *path = pathcache[i];
    if (path->segments > 0) {
      hair_cache->strands_len++;
      hair_cache->elems_len += path->segments + 2;
      hair_cache->point_len += path->segments + 1;
    }
  }
}

static void ensure_seg_pt_count(PTCacheEdit *edit,
                                ParticleSystem *psys,
                                ParticleHairCache *hair_cache)
{
  if (hair_cache->pos != nullptr && hair_cache->indices != nullptr) {
    return;
  }

  hair_cache->strands_len = 0;
  hair_cache->elems_len = 0;
  hair_cache->point_len = 0;

  if (edit != nullptr && edit->pathcache != nullptr) {
    count_cache_segment_keys(edit->pathcache, edit->totcached, hair_cache);
  }
  else {
    if (psys->pathcache && (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT))) {
      count_cache_segment_keys(psys->pathcache, psys->totpart, hair_cache);
    }
    if (psys->childcache) {
      const int child_count = psys->totchild * psys->part->disp / 100;
      count_cache_segment_keys(psys->childcache, child_count, hair_cache);
    }
  }
}

static void particle_pack_mcol(MCol *mcol, ushort r_scol[3])
{
  /* Convert to linear ushort and swizzle */
  float3 col = {BLI_color_from_srgb_table[mcol->r],
                BLI_color_from_srgb_table[mcol->g],
                BLI_color_from_srgb_table[mcol->b]};
  IMB_colormanagement_rec709_to_scene_linear(col, col);
  r_scol[0] = unit_float_to_ushort_clamp(col[2]);
  r_scol[1] = unit_float_to_ushort_clamp(col[1]);
  r_scol[2] = unit_float_to_ushort_clamp(col[0]);
}

/* Used by parent particles and simple children. */
static void particle_calculate_parent_uvs(ParticleSystem *psys,
                                          ParticleSystemModifierData *psmd,
                                          const int num_uv_layers,
                                          const int parent_index,
                                          const MTFace **mtfaces,
                                          float (*r_uv)[2])
{
  if (psmd == nullptr) {
    return;
  }
  const int emit_from = psmd->psys->part->from;
  if (!ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME)) {
    return;
  }
  ParticleData *particle = &psys->particles[parent_index];
  int num = particle->num_dmcache;
  if (ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
    if (particle->num < psmd->mesh_final->totface_legacy) {
      num = particle->num;
    }
  }
  if (!ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
    const MFace *mfaces = static_cast<const MFace *>(
        CustomData_get_layer(&psmd->mesh_final->fdata_legacy, CD_MFACE));
    if (UNLIKELY(mfaces == nullptr)) {
      BLI_assert_msg(psmd->mesh_final->faces_num == 0,
                     "A mesh with polygons should always have a generated 'CD_MFACE' layer!");
      return;
    }
    const MFace *mface = &mfaces[num];
    for (int j = 0; j < num_uv_layers; j++) {
      psys_interpolate_uvs(mtfaces[j] + num, mface->v4, particle->fuv, r_uv[j]);
    }
  }
}

static void particle_calculate_parent_mcol(ParticleSystem *psys,
                                           ParticleSystemModifierData *psmd,
                                           const int num_col_layers,
                                           const int parent_index,
                                           const MCol **mcols,
                                           MCol *r_mcol)
{
  if (psmd == nullptr) {
    return;
  }
  const int emit_from = psmd->psys->part->from;
  if (!ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME)) {
    return;
  }
  ParticleData *particle = &psys->particles[parent_index];
  int num = particle->num_dmcache;
  if (ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
    if (particle->num < psmd->mesh_final->totface_legacy) {
      num = particle->num;
    }
  }
  if (!ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
    const MFace *mfaces = static_cast<const MFace *>(
        CustomData_get_layer(&psmd->mesh_final->fdata_legacy, CD_MFACE));
    if (UNLIKELY(mfaces == nullptr)) {
      BLI_assert_msg(psmd->mesh_final->faces_num == 0,
                     "A mesh with polygons should always have a generated 'CD_MFACE' layer!");
      return;
    }
    const MFace *mface = &mfaces[num];
    for (int j = 0; j < num_col_layers; j++) {
      /* CustomDataLayer CD_MCOL has 4 structs per face. */
      psys_interpolate_mcol(mcols[j] + num * 4, mface->v4, particle->fuv, &r_mcol[j]);
    }
  }
}

/* Used by interpolated children. */
static void particle_interpolate_children_uvs(ParticleSystem *psys,
                                              ParticleSystemModifierData *psmd,
                                              const int num_uv_layers,
                                              const int child_index,
                                              const MTFace **mtfaces,
                                              float (*r_uv)[2])
{
  if (psmd == nullptr) {
    return;
  }
  const int emit_from = psmd->psys->part->from;
  if (!ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME)) {
    return;
  }
  ChildParticle *particle = &psys->child[child_index];
  int num = particle->num;
  if (num != DMCACHE_NOTFOUND) {
    const MFace *mfaces = static_cast<const MFace *>(
        CustomData_get_layer(&psmd->mesh_final->fdata_legacy, CD_MFACE));
    const MFace *mface = &mfaces[num];
    for (int j = 0; j < num_uv_layers; j++) {
      psys_interpolate_uvs(mtfaces[j] + num, mface->v4, particle->fuv, r_uv[j]);
    }
  }
}

static void particle_interpolate_children_mcol(ParticleSystem *psys,
                                               ParticleSystemModifierData *psmd,
                                               const int num_col_layers,
                                               const int child_index,
                                               const MCol **mcols,
                                               MCol *r_mcol)
{
  if (psmd == nullptr) {
    return;
  }
  const int emit_from = psmd->psys->part->from;
  if (!ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME)) {
    return;
  }
  ChildParticle *particle = &psys->child[child_index];
  int num = particle->num;
  if (num != DMCACHE_NOTFOUND) {
    const MFace *mfaces = static_cast<const MFace *>(
        CustomData_get_layer(&psmd->mesh_final->fdata_legacy, CD_MFACE));
    const MFace *mface = &mfaces[num];
    for (int j = 0; j < num_col_layers; j++) {
      /* CustomDataLayer CD_MCOL has 4 structs per face. */
      psys_interpolate_mcol(mcols[j] + num * 4, mface->v4, particle->fuv, &r_mcol[j]);
    }
  }
}

static void particle_calculate_uvs(ParticleSystem *psys,
                                   ParticleSystemModifierData *psmd,
                                   const bool is_simple,
                                   const int num_uv_layers,
                                   const int parent_index,
                                   const int child_index,
                                   const MTFace **mtfaces,
                                   float (**r_parent_uvs)[2],
                                   float (**r_uv)[2])
{
  if (psmd == nullptr) {
    return;
  }
  if (is_simple) {
    if (r_parent_uvs[parent_index] != nullptr) {
      *r_uv = r_parent_uvs[parent_index];
    }
    else {
      *r_uv = MEM_calloc_arrayN<float[2]>(num_uv_layers, "Particle UVs");
    }
  }
  else {
    *r_uv = MEM_calloc_arrayN<float[2]>(num_uv_layers, "Particle UVs");
  }
  if (child_index == -1) {
    /* Calculate UVs for parent particles. */
    if (is_simple) {
      r_parent_uvs[parent_index] = *r_uv;
    }
    particle_calculate_parent_uvs(psys, psmd, num_uv_layers, parent_index, mtfaces, *r_uv);
  }
  else {
    /* Calculate UVs for child particles. */
    if (!is_simple) {
      particle_interpolate_children_uvs(psys, psmd, num_uv_layers, child_index, mtfaces, *r_uv);
    }
    else if (!r_parent_uvs[psys->child[child_index].parent]) {
      r_parent_uvs[psys->child[child_index].parent] = *r_uv;
      particle_calculate_parent_uvs(psys, psmd, num_uv_layers, parent_index, mtfaces, *r_uv);
    }
  }
}

static void particle_calculate_mcol(ParticleSystem *psys,
                                    ParticleSystemModifierData *psmd,
                                    const bool is_simple,
                                    const int num_col_layers,
                                    const int parent_index,
                                    const int child_index,
                                    const MCol **mcols,
                                    MCol **r_parent_mcol,
                                    MCol **r_mcol)
{
  if (psmd == nullptr) {
    return;
  }
  if (is_simple) {
    if (r_parent_mcol[parent_index] != nullptr) {
      *r_mcol = r_parent_mcol[parent_index];
    }
    else {
      *r_mcol = MEM_calloc_arrayN<MCol>(num_col_layers, "Particle MCol");
    }
  }
  else {
    *r_mcol = MEM_calloc_arrayN<MCol>(num_col_layers, "Particle MCol");
  }
  if (child_index == -1) {
    /* Calculate MCols for parent particles. */
    if (is_simple) {
      r_parent_mcol[parent_index] = *r_mcol;
    }
    particle_calculate_parent_mcol(psys, psmd, num_col_layers, parent_index, mcols, *r_mcol);
  }
  else {
    /* Calculate MCols for child particles. */
    if (!is_simple) {
      particle_interpolate_children_mcol(psys, psmd, num_col_layers, child_index, mcols, *r_mcol);
    }
    else if (!r_parent_mcol[psys->child[child_index].parent]) {
      r_parent_mcol[psys->child[child_index].parent] = *r_mcol;
      particle_calculate_parent_mcol(psys, psmd, num_col_layers, parent_index, mcols, *r_mcol);
    }
  }
}

/* Will return last filled index. */
enum ParticleSource {
  PARTICLE_SOURCE_PARENT,
  PARTICLE_SOURCE_CHILDREN,
};
static int particle_batch_cache_fill_segments(ParticleSystem *psys,
                                              ParticleSystemModifierData *psmd,
                                              ParticleCacheKey **path_cache,
                                              const ParticleSource particle_source,
                                              const int global_offset,
                                              const int start_index,
                                              const int num_path_keys,
                                              const int num_uv_layers,
                                              const int num_col_layers,
                                              const MTFace **mtfaces,
                                              const MCol **mcols,
                                              uint *uv_id,
                                              uint *col_id,
                                              float (***r_parent_uvs)[2],
                                              MCol ***r_parent_mcol,
                                              GPUIndexBufBuilder *elb,
                                              HairAttributeID *attr_id,
                                              ParticleHairCache *hair_cache)
{
  const bool is_simple = (psys->part->childtype == PART_CHILD_PARTICLES);
  const bool is_child = (particle_source == PARTICLE_SOURCE_CHILDREN);
  if (is_simple && *r_parent_uvs == nullptr) {
    /* TODO(sergey): For edit mode it should be edit->totcached. */
    *r_parent_uvs = static_cast<float (**)[2]>(
        MEM_callocN(sizeof(*r_parent_uvs) * psys->totpart, "Parent particle UVs"));
  }
  if (is_simple && *r_parent_mcol == nullptr) {
    *r_parent_mcol = static_cast<MCol **>(
        MEM_callocN(sizeof(*r_parent_mcol) * psys->totpart, "Parent particle MCol"));
  }
  int curr_point = start_index;
  for (int i = 0; i < num_path_keys; i++) {
    ParticleCacheKey *path = path_cache[i];
    if (path->segments <= 0) {
      continue;
    }
    float tangent[3];
    float (*uv)[2] = nullptr;
    MCol *mcol = nullptr;
    particle_calculate_mcol(psys,
                            psmd,
                            is_simple,
                            num_col_layers,
                            is_child ? psys->child[i].parent : i,
                            is_child ? i : -1,
                            mcols,
                            *r_parent_mcol,
                            &mcol);
    particle_calculate_uvs(psys,
                           psmd,
                           is_simple,
                           num_uv_layers,
                           is_child ? psys->child[i].parent : i,
                           is_child ? i : -1,
                           mtfaces,
                           *r_parent_uvs,
                           &uv);
    for (int j = 0; j < path->segments; j++) {
      if (j == 0) {
        sub_v3_v3v3(tangent, path[j + 1].co, path[j].co);
      }
      else {
        sub_v3_v3v3(tangent, path[j + 1].co, path[j - 1].co);
      }
      GPU_vertbuf_attr_set(hair_cache->pos, attr_id->pos, curr_point, path[j].co);
      GPU_vertbuf_attr_set(hair_cache->pos, attr_id->tan, curr_point, tangent);
      GPU_vertbuf_attr_set(hair_cache->pos, attr_id->ind, curr_point, &i);
      if (psmd != nullptr) {
        for (int k = 0; k < num_uv_layers; k++) {
          GPU_vertbuf_attr_set(
              hair_cache->pos,
              uv_id[k],
              curr_point,
              (is_simple && is_child) ? (*r_parent_uvs)[psys->child[i].parent][k] : uv[k]);
        }
        for (int k = 0; k < num_col_layers; k++) {
          /* TODO: Put the conversion outside the loop. */
          ushort scol[4];
          particle_pack_mcol(
              (is_simple && is_child) ? &(*r_parent_mcol)[psys->child[i].parent][k] : &mcol[k],
              scol);
          GPU_vertbuf_attr_set(hair_cache->pos, col_id[k], curr_point, scol);
        }
      }
      GPU_indexbuf_add_generic_vert(elb, curr_point);
      curr_point++;
    }
    sub_v3_v3v3(tangent, path[path->segments].co, path[path->segments - 1].co);

    int global_index = i + global_offset;
    GPU_vertbuf_attr_set(hair_cache->pos, attr_id->pos, curr_point, path[path->segments].co);
    GPU_vertbuf_attr_set(hair_cache->pos, attr_id->tan, curr_point, tangent);
    GPU_vertbuf_attr_set(hair_cache->pos, attr_id->ind, curr_point, &global_index);

    if (psmd != nullptr) {
      for (int k = 0; k < num_uv_layers; k++) {
        GPU_vertbuf_attr_set(hair_cache->pos,
                             uv_id[k],
                             curr_point,
                             (is_simple && is_child) ? (*r_parent_uvs)[psys->child[i].parent][k] :
                                                       uv[k]);
      }
      for (int k = 0; k < num_col_layers; k++) {
        /* TODO: Put the conversion outside the loop. */
        ushort scol[4];
        particle_pack_mcol((is_simple && is_child) ? &(*r_parent_mcol)[psys->child[i].parent][k] :
                                                     &mcol[k],
                           scol);
        GPU_vertbuf_attr_set(hair_cache->pos, col_id[k], curr_point, scol);
      }
      if (!is_simple) {
        MEM_freeN(uv);
        MEM_freeN(mcol);
      }
    }
    /* Finish the segment and add restart primitive. */
    GPU_indexbuf_add_generic_vert(elb, curr_point);
    GPU_indexbuf_add_primitive_restart(elb);
    curr_point++;
  }
  return curr_point;
}

static float particle_key_weight(const ParticleData *particle, int strand, float t)
{
  const ParticleData *part = particle + strand;
  const HairKey *hkeys = part->hair;
  float edit_key_seg_t = 1.0f / (part->totkey - 1);
  if (t == 1.0) {
    return hkeys[part->totkey - 1].weight;
  }

  float interp = t / edit_key_seg_t;
  int index = int(interp);
  interp -= floorf(interp); /* Time between 2 edit key */
  float s1 = hkeys[index].weight;
  float s2 = hkeys[index + 1].weight;
  return s1 + interp * (s2 - s1);
}

static int particle_batch_cache_fill_segments_edit(
    const PTCacheEdit * /*edit*/, /* nullptr for weight data */
    const ParticleData *particle, /* nullptr for select data */
    ParticleCacheKey **path_cache,
    const int start_index,
    const int num_path_keys,
    GPUIndexBufBuilder *elb,
    GPUVertBufRaw *attr_step)
{
  int curr_point = start_index;
  for (int i = 0; i < num_path_keys; i++) {
    ParticleCacheKey *path = path_cache[i];
    if (path->segments <= 0) {
      continue;
    }
    for (int j = 0; j <= path->segments; j++) {
      EditStrandData *seg_data = (EditStrandData *)GPU_vertbuf_raw_step(attr_step);
      copy_v3_v3(seg_data->pos, path[j].co);
      float strand_t = float(j) / path->segments;
      if (particle) {
        float weight = particle_key_weight(particle, i, strand_t);
        /* NaN or unclamped become 1.0f */
        seg_data->selection = (weight < 1.0f) ? weight : 1.0f;
      }
      else {
        /* Computed in psys_cache_edit_paths_iter(). */
        seg_data->selection = path[j].col[0];
      }
      GPU_indexbuf_add_generic_vert(elb, curr_point);
      curr_point++;
    }
    /* Finish the segment and add restart primitive. */
    GPU_indexbuf_add_primitive_restart(elb);
  }
  return curr_point;
}

static void particle_batch_cache_ensure_pos_and_seg(PTCacheEdit *edit,
                                                    ParticleSystem *psys,
                                                    ModifierData *md,
                                                    ParticleHairCache *hair_cache)
{
  if (hair_cache->pos != nullptr && hair_cache->indices != nullptr) {
    return;
  }

  int curr_point = 0;
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

  GPU_VERTBUF_DISCARD_SAFE(hair_cache->pos);
  GPU_INDEXBUF_DISCARD_SAFE(hair_cache->indices);

  GPUVertFormat format = {0};
  HairAttributeID attr_id;
  uint *uv_id = nullptr;
  uint *col_id = nullptr;
  Vector<StringRef> color_attribute_names;
  int num_col_layers = 0;
  const MTFace **mtfaces = nullptr;
  const MCol **mcols = nullptr;
  float (**parent_uvs)[2] = nullptr;
  MCol **parent_mcol = nullptr;

  VectorSet<StringRefNull> uv_map_names;
  if (psmd != nullptr) {
    psmd->mesh_final->uv_map_names();
    psmd->mesh_final->attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
      if (iter.domain == bke::AttrDomain::Corner && iter.data_type == bke::AttrType::ColorByte) {
        color_attribute_names.append(iter.name);
      }
    });
    num_col_layers = color_attribute_names.size();
  }
  int num_uv_layers = uv_map_names.size();

  attr_id.pos = GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32_32);
  attr_id.tan = GPU_vertformat_attr_add(&format, "nor", gpu::VertAttrType::SFLOAT_32_32_32);
  attr_id.ind = GPU_vertformat_attr_add(&format, "ind", gpu::VertAttrType::SINT_32);

  if (psmd) {
    const StringRef active_uv = psmd->mesh_final->default_uv_map_name();
    const char *active_col = psmd->mesh_final->active_color_attribute;
    uv_id = MEM_malloc_arrayN<uint>(num_uv_layers, "UV attr format");
    col_id = MEM_malloc_arrayN<uint>(color_attribute_names.size(), "Col attr format");

    for (int i = 0; i < num_uv_layers; i++) {

      char uuid[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const StringRef name = uv_map_names[i];
      GPU_vertformat_safe_attr_name(name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

      SNPRINTF_UTF8(uuid, "a%s", attr_safe_name);
      uv_id[i] = GPU_vertformat_attr_add(&format, uuid, blender::gpu::VertAttrType::SFLOAT_32_32);

      if (name == active_uv) {
        GPU_vertformat_alias_add(&format, "a");
      }
    }

    for (int i = 0; i < num_col_layers; i++) {
      const StringRef name = color_attribute_names[i];
      char uuid[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      GPU_vertformat_safe_attr_name(name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

      SNPRINTF_UTF8(uuid, "a%s", attr_safe_name);
      col_id[i] = GPU_vertformat_attr_add(
          &format, uuid, blender::gpu::VertAttrType::UNORM_16_16_16_16);

      if (name == active_col) {
        GPU_vertformat_alias_add(&format, "c");
      }
    }
  }

  hair_cache->pos = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*hair_cache->pos, hair_cache->point_len);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, hair_cache->elems_len, hair_cache->point_len);

  if (num_uv_layers || num_col_layers) {
    BKE_mesh_tessface_ensure(psmd->mesh_final);
    if (num_uv_layers) {
      mtfaces = static_cast<const MTFace **>(
          MEM_mallocN(sizeof(*mtfaces) * num_uv_layers, "Faces UV layers"));
      for (int i = 0; i < num_uv_layers; i++) {
        mtfaces[i] = (const MTFace *)CustomData_get_layer_n(
            &psmd->mesh_final->fdata_legacy, CD_MTFACE, i);
      }
    }
    if (num_col_layers) {
      mcols = static_cast<const MCol **>(
          MEM_mallocN(sizeof(*mcols) * num_col_layers, "Color layers"));
      for (int i = 0; i < num_col_layers; i++) {
        mcols[i] = (const MCol *)CustomData_get_layer_n(
            &psmd->mesh_final->fdata_legacy, CD_MCOL, i);
      }
    }
  }

  if (edit != nullptr && edit->pathcache != nullptr) {
    curr_point = particle_batch_cache_fill_segments(psys,
                                                    psmd,
                                                    edit->pathcache,
                                                    PARTICLE_SOURCE_PARENT,
                                                    0,
                                                    0,
                                                    edit->totcached,
                                                    num_uv_layers,
                                                    num_col_layers,
                                                    mtfaces,
                                                    mcols,
                                                    uv_id,
                                                    col_id,
                                                    &parent_uvs,
                                                    &parent_mcol,
                                                    &elb,
                                                    &attr_id,
                                                    hair_cache);
  }
  else {
    if ((psys->pathcache != nullptr) &&
        (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT)))
    {
      curr_point = particle_batch_cache_fill_segments(psys,
                                                      psmd,
                                                      psys->pathcache,
                                                      PARTICLE_SOURCE_PARENT,
                                                      0,
                                                      0,
                                                      psys->totpart,
                                                      num_uv_layers,
                                                      num_col_layers,
                                                      mtfaces,
                                                      mcols,
                                                      uv_id,
                                                      col_id,
                                                      &parent_uvs,
                                                      &parent_mcol,
                                                      &elb,
                                                      &attr_id,
                                                      hair_cache);
    }
    if (psys->childcache != nullptr) {
      const int child_count = psys->totchild * psys->part->disp / 100;
      curr_point = particle_batch_cache_fill_segments(psys,
                                                      psmd,
                                                      psys->childcache,
                                                      PARTICLE_SOURCE_CHILDREN,
                                                      psys->totpart,
                                                      curr_point,
                                                      child_count,
                                                      num_uv_layers,
                                                      num_col_layers,
                                                      mtfaces,
                                                      mcols,
                                                      uv_id,
                                                      col_id,
                                                      &parent_uvs,
                                                      &parent_mcol,
                                                      &elb,
                                                      &attr_id,
                                                      hair_cache);
    }
  }
  /* Cleanup. */
  if (parent_uvs != nullptr) {
    /* TODO(sergey): For edit mode it should be edit->totcached. */
    for (int i = 0; i < psys->totpart; i++) {
      MEM_SAFE_FREE(parent_uvs[i]);
    }
    MEM_freeN(parent_uvs);
  }
  if (parent_mcol != nullptr) {
    for (int i = 0; i < psys->totpart; i++) {
      MEM_SAFE_FREE(parent_mcol[i]);
    }
    MEM_freeN(parent_mcol);
  }
  if (num_uv_layers) {
    MEM_freeN(mtfaces);
  }
  if (num_col_layers) {
    MEM_freeN(mcols);
  }
  if (psmd != nullptr) {
    MEM_freeN(uv_id);
  }
  hair_cache->indices = GPU_indexbuf_build(&elb);
}

static void particle_batch_cache_ensure_pos(Object *object,
                                            ParticleSystem *psys,
                                            ParticlePointCache *point_cache)
{
  if (point_cache->pos != nullptr) {
    return;
  }

  int i, curr_point;
  ParticleData *pa;
  ParticleKey state;
  ParticleSimulationData sim = {nullptr};
  const DRWContext *draw_ctx = DRW_context_get();

  sim.depsgraph = draw_ctx->depsgraph;
  sim.scene = draw_ctx->scene;
  sim.ob = object;
  sim.psys = psys;
  sim.psmd = psys_get_modifier(object, psys);
  psys_sim_data_init(&sim);

  GPU_VERTBUF_DISCARD_SAFE(point_cache->pos);

  static uint pos_id, rot_id, val_id;
  static const GPUVertFormat format = [&]() {
    GPUVertFormat format{};
    pos_id = GPU_vertformat_attr_add(&format, "part_pos", gpu::VertAttrType::SFLOAT_32_32_32);
    val_id = GPU_vertformat_attr_add(&format, "part_val", gpu::VertAttrType::SFLOAT_32);
    rot_id = GPU_vertformat_attr_add(&format, "part_rot", gpu::VertAttrType::SFLOAT_32_32_32_32);
    return format;
  }();

  point_cache->pos = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*point_cache->pos, psys->totpart);

  for (curr_point = 0, i = 0, pa = psys->particles; i < psys->totpart; i++, pa++) {
    state.time = DEG_get_ctime(draw_ctx->depsgraph);
    if (!psys_get_particle_state(&sim, i, &state, false)) {
      continue;
    }

    float val;

    GPU_vertbuf_attr_set(point_cache->pos, pos_id, curr_point, state.co);
    GPU_vertbuf_attr_set(point_cache->pos, rot_id, curr_point, state.rot);

    switch (psys->part->draw_col) {
      case PART_DRAW_COL_VEL:
        val = len_v3(state.vel) / psys->part->color_vec_max;
        break;
      case PART_DRAW_COL_ACC:
        val = len_v3v3(state.vel, pa->prev_state.vel) /
              ((state.time - pa->prev_state.time) * psys->part->color_vec_max);
        break;
      default:
        val = -1.0f;
        break;
    }

    GPU_vertbuf_attr_set(point_cache->pos, val_id, curr_point, &val);

    curr_point++;
  }

  if (curr_point != psys->totpart) {
    GPU_vertbuf_data_resize(*point_cache->pos, curr_point);
  }

  psys_sim_data_free(&sim);
}

static void drw_particle_update_ptcache_edit(Object *object_eval,
                                             ParticleSystem *psys,
                                             PTCacheEdit *edit)
{
  if (edit->psys == nullptr) {
    return;
  }
  /* NOTE: Get flag from particle system coming from drawing object.
   * this is where depsgraph will be setting flags to.
   */
  const DRWContext *draw_ctx = DRW_context_get();
  Scene *scene_orig = DEG_get_original(draw_ctx->scene);
  Object *object_orig = DEG_get_original(object_eval);
  if (psys->flag & PSYS_HAIR_UPDATED) {
    PE_update_object(draw_ctx->depsgraph, scene_orig, object_orig, 0);
    psys->flag &= ~PSYS_HAIR_UPDATED;
  }
  if (edit->pathcache == nullptr) {
    Depsgraph *depsgraph = draw_ctx->depsgraph;
    psys_cache_edit_paths(depsgraph,
                          scene_orig,
                          object_orig,
                          edit,
                          DEG_get_ctime(depsgraph),
                          DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  }
}

void drw_particle_update_ptcache(Object *object_eval, ParticleSystem *psys)
{
  if ((object_eval->mode & OB_MODE_PARTICLE_EDIT) == 0) {
    return;
  }
  const DRWContext *draw_ctx = DRW_context_get();
  Scene *scene_orig = DEG_get_original(draw_ctx->scene);
  Object *object_orig = DEG_get_original(object_eval);
  PTCacheEdit *edit = PE_create_current(draw_ctx->depsgraph, scene_orig, object_orig);
  if (edit != nullptr) {
    drw_particle_update_ptcache_edit(object_eval, psys, edit);
  }
}

ParticleDrawSource drw_particle_get_hair_source(Object *object,
                                                ParticleSystem *psys,
                                                ModifierData *md,
                                                PTCacheEdit *edit,
                                                const int additional_subdivision)
{
  const DRWContext *draw_ctx = DRW_context_get();
  if (psys_in_edit_mode(draw_ctx->depsgraph, psys)) {
    object = DEG_get_original(object);
    psys = psys_orig_get(psys);
  }
  ParticleBatchCache *cache = particle_batch_cache_get(psys);

  ParticleDrawSource src = ParticleDrawSource(cache->hair.points_by_curve_storage,
                                              cache->hair.evaluated_points_by_curve_storage,
                                              math::clamp(additional_subdivision, 0, 3));
  src.object = object;
  src.psys = psys;
  src.md = md;
  src.edit = edit;
  return src;
}

gpu::Batch *DRW_particles_batch_cache_get_hair(Object *object,
                                               ParticleSystem *psys,
                                               ModifierData *md)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);
  if (cache->hair.hairs == nullptr) {
    drw_particle_update_ptcache(object, psys);
    ParticleDrawSource source = drw_particle_get_hair_source(object, psys, md, nullptr, 0);
    ensure_seg_pt_count(source.edit, source.psys, &cache->hair);
    particle_batch_cache_ensure_pos_and_seg(source.edit, source.psys, source.md, &cache->hair);
    cache->hair.hairs = GPU_batch_create(
        GPU_PRIM_LINE_STRIP, cache->hair.pos, cache->hair.indices);
  }
  return cache->hair.hairs;
}

gpu::Batch *DRW_particles_batch_cache_get_dots(Object *object, ParticleSystem *psys)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);

  if (cache->point.points == nullptr) {
    particle_batch_cache_ensure_pos(object, psys, &cache->point);
    cache->point.points = GPU_batch_create(GPU_PRIM_POINTS, cache->point.pos, nullptr);
  }

  return cache->point.points;
}

static void particle_batch_cache_ensure_edit_pos_and_seg(PTCacheEdit *edit,
                                                         ParticleSystem *psys,
                                                         ModifierData * /*md*/,
                                                         ParticleHairCache *hair_cache,
                                                         bool use_weight)
{
  if (hair_cache->pos != nullptr && hair_cache->indices != nullptr) {
    return;
  }

  ParticleData *particle = (use_weight) ? psys->particles : nullptr;

  GPU_VERTBUF_DISCARD_SAFE(hair_cache->pos);
  GPU_INDEXBUF_DISCARD_SAFE(hair_cache->indices);

  GPUVertBufRaw data_step;
  GPUIndexBufBuilder elb;
  uint pos_id, selection_id;
  const GPUVertFormat *edit_point_format = edit_points_vert_format_get(&pos_id, &selection_id);

  hair_cache->pos = GPU_vertbuf_create_with_format(*edit_point_format);
  GPU_vertbuf_data_alloc(*hair_cache->pos, hair_cache->point_len);
  GPU_vertbuf_attr_get_raw_data(hair_cache->pos, pos_id, &data_step);

  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, hair_cache->elems_len, hair_cache->point_len);

  if (edit != nullptr && edit->pathcache != nullptr) {
    particle_batch_cache_fill_segments_edit(
        edit, particle, edit->pathcache, 0, edit->totcached, &elb, &data_step);
  }
  hair_cache->indices = GPU_indexbuf_build(&elb);
}

gpu::Batch *DRW_particles_batch_cache_get_edit_strands(Object *object,
                                                       ParticleSystem *psys,
                                                       PTCacheEdit *edit,
                                                       bool use_weight)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);
  if (cache->edit_is_weight != use_weight) {
    GPU_VERTBUF_DISCARD_SAFE(cache->edit_hair.pos);
    GPU_BATCH_DISCARD_SAFE(cache->edit_hair.hairs);
  }
  if (cache->edit_hair.hairs != nullptr) {
    return cache->edit_hair.hairs;
  }
  drw_particle_update_ptcache_edit(object, psys, edit);
  ensure_seg_pt_count(edit, psys, &cache->edit_hair);
  particle_batch_cache_ensure_edit_pos_and_seg(edit, psys, nullptr, &cache->edit_hair, use_weight);
  cache->edit_hair.hairs = GPU_batch_create(
      GPU_PRIM_LINE_STRIP, cache->edit_hair.pos, cache->edit_hair.indices);
  cache->edit_is_weight = use_weight;
  return cache->edit_hair.hairs;
}

static void ensure_edit_inner_points_count(const PTCacheEdit *edit, ParticleBatchCache *cache)
{
  if (cache->edit_inner_pos != nullptr) {
    return;
  }
  cache->edit_inner_point_len = 0;
  for (int point_index = 0; point_index < edit->totpoint; point_index++) {
    const PTCacheEditPoint *point = &edit->points[point_index];
    if (point->flag & PEP_HIDE) {
      continue;
    }
    BLI_assert(point->totkey >= 1);
    cache->edit_inner_point_len += (point->totkey - 1);
  }
}

static void particle_batch_cache_ensure_edit_inner_pos(PTCacheEdit *edit,
                                                       ParticleBatchCache *cache)
{
  if (cache->edit_inner_pos != nullptr) {
    return;
  }

  uint pos_id, selection_id;
  const GPUVertFormat *edit_point_format = edit_points_vert_format_get(&pos_id, &selection_id);

  cache->edit_inner_pos = GPU_vertbuf_create_with_format(*edit_point_format);
  GPU_vertbuf_data_alloc(*cache->edit_inner_pos, cache->edit_inner_point_len);

  int global_key_index = 0;
  for (int point_index = 0; point_index < edit->totpoint; point_index++) {
    const PTCacheEditPoint *point = &edit->points[point_index];
    if (point->flag & PEP_HIDE) {
      continue;
    }
    for (int key_index = 0; key_index < point->totkey - 1; key_index++) {
      PTCacheEditKey *key = &point->keys[key_index];
      float selection = (key->flag & PEK_SELECT) ? 1.0f : 0.0f;
      GPU_vertbuf_attr_set(cache->edit_inner_pos, pos_id, global_key_index, key->world_co);
      GPU_vertbuf_attr_set(cache->edit_inner_pos, selection_id, global_key_index, &selection);
      global_key_index++;
    }
  }
}

gpu::Batch *DRW_particles_batch_cache_get_edit_inner_points(Object *object,
                                                            ParticleSystem *psys,
                                                            PTCacheEdit *edit)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);
  if (cache->edit_inner_points != nullptr) {
    return cache->edit_inner_points;
  }
  drw_particle_update_ptcache_edit(object, psys, edit);
  ensure_edit_inner_points_count(edit, cache);
  particle_batch_cache_ensure_edit_inner_pos(edit, cache);
  cache->edit_inner_points = GPU_batch_create(GPU_PRIM_POINTS, cache->edit_inner_pos, nullptr);
  return cache->edit_inner_points;
}

static void ensure_edit_tip_points_count(const PTCacheEdit *edit, ParticleBatchCache *cache)
{
  if (cache->edit_tip_pos != nullptr) {
    return;
  }
  cache->edit_tip_point_len = 0;
  for (int point_index = 0; point_index < edit->totpoint; point_index++) {
    const PTCacheEditPoint *point = &edit->points[point_index];
    if (point->flag & PEP_HIDE) {
      continue;
    }
    cache->edit_tip_point_len += 1;
  }
}

static void particle_batch_cache_ensure_edit_tip_pos(PTCacheEdit *edit, ParticleBatchCache *cache)
{
  if (cache->edit_tip_pos != nullptr) {
    return;
  }

  uint pos_id, selection_id;
  const GPUVertFormat *edit_point_format = edit_points_vert_format_get(&pos_id, &selection_id);

  cache->edit_tip_pos = GPU_vertbuf_create_with_format(*edit_point_format);
  GPU_vertbuf_data_alloc(*cache->edit_tip_pos, cache->edit_tip_point_len);

  int global_point_index = 0;
  for (int point_index = 0; point_index < edit->totpoint; point_index++) {
    const PTCacheEditPoint *point = &edit->points[point_index];
    if (point->flag & PEP_HIDE) {
      continue;
    }
    PTCacheEditKey *key = &point->keys[point->totkey - 1];
    float selection = (key->flag & PEK_SELECT) ? 1.0f : 0.0f;

    GPU_vertbuf_attr_set(cache->edit_tip_pos, pos_id, global_point_index, key->world_co);
    GPU_vertbuf_attr_set(cache->edit_tip_pos, selection_id, global_point_index, &selection);
    global_point_index++;
  }
}

gpu::Batch *DRW_particles_batch_cache_get_edit_tip_points(Object *object,
                                                          ParticleSystem *psys,
                                                          PTCacheEdit *edit)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);
  if (cache->edit_tip_points != nullptr) {
    return cache->edit_tip_points;
  }
  drw_particle_update_ptcache_edit(object, psys, edit);
  ensure_edit_tip_points_count(edit, cache);
  particle_batch_cache_ensure_edit_tip_pos(edit, cache);
  cache->edit_tip_points = GPU_batch_create(GPU_PRIM_POINTS, cache->edit_tip_pos, nullptr);
  return cache->edit_tip_points;
}

/* Can return DMCACHE_NOTFOUND in case of invalid mapping. */
static int particle_mface_index(const ParticleData &particle, int face_count_legacy)
{
  if (!ELEM(particle.num_dmcache, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
    return particle.num_dmcache;
  }
  if (particle.num < face_count_legacy) {
    return (particle.num == DMCACHE_ISCHILD) ? DMCACHE_NOTFOUND : particle.num;
  }
  return DMCACHE_NOTFOUND;
}
static int particle_mface_index(const ChildParticle &particle, int /*face_count_legacy*/)
{
  return particle.num;
}

static float4 particle_mcol_convert(const MCol &mcol)
{
  /* Convert to linear ushort and swizzle */
  float4 col = {BLI_color_from_srgb_table[mcol.r],
                BLI_color_from_srgb_table[mcol.g],
                BLI_color_from_srgb_table[mcol.b],
                mcol.a / 255.0f};
  IMB_colormanagement_rec709_to_scene_linear(col, col);
  std::swap(col[0], col[2]);
  return col;
}

template<typename ParticleDataT>
static float4 interpolate(const ParticleDataT &particle, Span<MFace> mfaces, Span<MCol> mcols)
{
  int num = particle_mface_index(particle, mfaces.size());
  if (num == DMCACHE_NOTFOUND) {
    return float4(0, 0, 0, 1);
  }
  /* CustomDataLayer CD_MCOL has 4 structs per face. */
  MCol mcol;
  psys_interpolate_mcol(mcols.slice(num * 4, 4).data(), mfaces[num].v4, particle.fuv, &mcol);
  return particle_mcol_convert(mcol);
}

template<typename ParticleDataT>
static float2 interpolate(const ParticleDataT &particle, Span<MFace> mfaces, Span<MTFace> mtfaces)
{
  int num = particle_mface_index(particle, mfaces.size());
  if (num == DMCACHE_NOTFOUND) {
    return float2(0);
  }
  float2 uv;
  psys_interpolate_uvs(&mtfaces[num], mfaces[num].v4, particle.fuv, uv);
  return uv;
}

static std::optional<StringRef> get_first_uv_name(const bke::AttributeAccessor &attributes)
{
  std::optional<StringRef> name;
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.data_type == bke::AttrType::Float2) {
      name = iter.name;
      iter.stop();
    }
  });
  return name;
}

template<typename T>
Span<T> span_from_custom_data_layer(const Mesh &mesh,
                                    const eCustomDataType type,
                                    const StringRef name)
{
  int layer_id = CustomData_get_named_layer(&mesh.fdata_legacy, type, name);
  return {static_cast<const T *>(CustomData_get_layer_n(&mesh.fdata_legacy, type, layer_id)),
          /* There is 4 MCol per face. */
          mesh.totface_legacy * (std::is_same_v<T, MCol> ? 4 : 1)};
}

template<typename T>
Span<T> span_from_custom_data_layer(const Mesh &mesh, const eCustomDataType type)
{
  return {static_cast<const T *>(CustomData_get_layer(&mesh.fdata_legacy, type)),
          mesh.totface_legacy};
}

template<typename InputT, typename OutputT, eCustomDataType data_type>
static gpu::VertBufPtr interpolate_face_corner_attribute_to_curve(ParticleDrawSource &src,
                                                                  const StringRef name)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)src.md;
  Mesh &mesh = *psmd->mesh_final;

  /* TODO(fclem): Use normalized integer format. */
  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_with_format_ex(gpu::GenericVertexFormat<OutputT>::format(),
                                        GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY));
  vbo->allocate(src.curves_num());
  MutableSpan<OutputT> data = vbo->data<OutputT>();

  const int emit_from = psmd->psys->part->from;
  /* True if no interpolation for child particle. */
  const bool is_simple = (src.psys->part->childtype == PART_CHILD_PARTICLES) ||
                         !ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME);

  BKE_mesh_tessface_ensure(&mesh);
  Span<InputT> attr = span_from_custom_data_layer<InputT>(mesh, data_type, name);
  Span<MFace> mfaces = span_from_custom_data_layer<MFace>(mesh, CD_MFACE);
  Span<ChildParticle> children(src.psys->child, src.psys->totchild);
  Span<ParticleData> particles(src.psys->particles, src.psys->totpart);

  /* Index of the particle/hair curve. Note that the order of the loops matter. */
  int curve_index = 0;

  ParticleSpans part_spans = src.particles_get();
  for (const int particle_index : part_spans.parent.index_range()) {
    data[curve_index++] = interpolate(particles[particle_index], mfaces, attr);
  }

  if (is_simple) {
    /* Fallback array if parent particles are not displayed. */
    Vector<OutputT> parent_data;
    if (part_spans.parent.is_empty()) {
      parent_data.reserve(src.psys->totpart);
      for (int particle_index : IndexRange(src.psys->totpart)) {
        parent_data.append(interpolate(particles[particle_index], mfaces, attr));
      }
    }

    Span<OutputT> data_parent(part_spans.parent.is_empty() ? parent_data.data() : data.data(),
                              src.psys->totpart);

    for (const int particle_index : part_spans.children.index_range()) {
      /* Simple copy of the parent data. */
      data[curve_index++] = data_parent[children[particle_index].parent];
    }
  }
  else {
    for (const int particle_index : part_spans.children.index_range()) {
      data[curve_index++] = interpolate(children[particle_index], mfaces, attr);
    }
  }
  return vbo;
}

static gpu::VertBufPtr ensure_curve_attribute(ParticleDrawSource &src,
                                              const Mesh &mesh,
                                              const StringRef name,
                                              bool &r_is_point_domain)
{
  using namespace bke;
  /* Note: All legacy hair attributes come from the emitter mesh and are on per curve domain. */
  r_is_point_domain = false;

  const AttributeAccessor attributes = mesh.attributes();

  auto meta_data = attributes.lookup_meta_data(name);
  if (meta_data && meta_data->domain == bke::AttrDomain::Corner) {
    if (meta_data->data_type == AttrType::ColorByte) {
      return interpolate_face_corner_attribute_to_curve<MCol, float4, CD_MCOL>(src, name);
    }
    if (meta_data->data_type == AttrType::Float2) {
      return interpolate_face_corner_attribute_to_curve<MTFace, float2, CD_MTFACE>(src, name);
    }
  }
  /* Attribute doesn't exist or is of an incompatible type.
   * Replace it with a black curve domain attribute. */
  return gpu::VertBuf::from_varray(VArray<float>::from_single(1, src.curves_num()));
}

void CurvesEvalCache::ensure_attribute(CurvesModule & /*module*/,
                                       ParticleDrawSource &src,
                                       const Mesh &mesh,
                                       const StringRef name,
                                       const int index)
{
  char sampler_name[32];
  drw_curves_get_attribute_sampler_name(name, sampler_name);

  gpu::VertBufPtr attr_buf = ensure_curve_attribute(
      src, mesh, name, attributes_point_domain[index]);

  /* Existing final data may have been for a different attribute (with a different name or domain),
   * free the data. */
  this->curve_attributes_buf[index].reset();

  /* Ensure final data for points. */
  if (attributes_point_domain[index]) {
    BLI_assert_unreachable();
  }
  else {
    this->curve_attributes_buf[index] = std::move(attr_buf);
  }
}

void CurvesEvalCache::ensure_attributes(CurvesModule &module,
                                        ParticleDrawSource &src,
                                        const GPUMaterial *gpu_material)
{
  ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)src.md;
  if (psmd == nullptr || psmd->mesh_final == nullptr || src.curves_num() == 0) {
    return;
  }
  const Mesh &mesh = *psmd->mesh_final;
  const bke::AttributeAccessor attributes = mesh.attributes();

  if (gpu_material) {
    VectorSet<std::string> attrs_needed;
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      StringRef name = gpu_attr->name;
      if (name.is_empty()) {
        if (std::optional<StringRef> uv_name = get_first_uv_name(attributes)) {
          drw_attributes_add_request(&attrs_needed, *uv_name);
        }
      }
      if (!attributes.contains(name)) {
        continue;
      }
      drw_attributes_add_request(&attrs_needed, name);
    }

    if (!drw_attributes_overlap(&attr_used, &attrs_needed)) {
      /* Some new attributes have been added, free all and start over. */
      for (const int i : IndexRange(GPU_MAX_ATTR)) {
        this->curve_attributes_buf[i].reset();
      }
      drw_attributes_merge(&attr_used, &attrs_needed);
    }
    drw_attributes_merge(&attr_used_over_time, &attrs_needed);
  }

  for (const int i : attr_used.index_range()) {
    if (this->curve_attributes_buf[i]) {
      continue;
    }
    ensure_attribute(module, src, mesh, attr_used[i], i);
  }
}

void CurvesEvalCache::ensure_common(ParticleDrawSource &src)
{
  if (points_by_curve_buf) {
    return;
  }

  this->points_by_curve_buf = gpu::VertBuf::from_span(src.points_by_curve().data());
  this->evaluated_points_by_curve_buf = gpu::VertBuf::from_span(
      src.evaluated_points_by_curve().data());

  /* Use the same type for all curves. */
  auto type_varray = VArray<int8_t>::from_single(CURVE_TYPE_CATMULL_ROM, src.curves_num());
  auto resolution_varray = VArray<int32_t>::from_single(src.resolution(), src.curves_num());
  /* Not used. */
  auto cyclic_offsets_varray = VArray<int32_t>::from_single(0, 2);
  /* TODO(fclem): Optimize shaders to avoid needing to upload this data if data is uniform.
   * This concerns all varray. */
  this->curves_type_buf = gpu::VertBuf::from_varray(type_varray);
  this->curves_resolution_buf = gpu::VertBuf::from_varray(resolution_varray);
  this->curves_cyclic_buf = gpu::VertBuf::from_varray(cyclic_offsets_varray);
}

/* Copied from cycles. */
static float hair_shape_radius(float shape, float root, float tip, float time)
{
  BLI_assert(time >= 0.0f);
  BLI_assert(time <= 1.0f);
  float radius = 1.0f - time;
  if (shape < 0.0f) {
    radius = powf(radius, 1.0f + shape);
  }
  else {
    radius = powf(radius, 1.0f / (1.0f - shape));
  }
  return (radius * (root - tip)) + tip;
}

void CurvesEvalCache::ensure_positions(CurvesModule &module, ParticleDrawSource &src)
{
  if (evaluated_pos_rad_buf) {
    return;
  }

  if (src.curves_num() == 0) {
    /* Garbage data. */
    this->evaluated_pos_rad_buf = gpu::VertBuf::device_only<float4>(1);
    this->evaluated_time_buf = gpu::VertBuf::device_only<float>(4);
    this->curves_length_buf = gpu::VertBuf::device_only<float>(4);
    return;
  }

  ensure_common(src);

  gpu::VertBufPtr points_pos_buf = gpu::VertBuf::from_size<float3>(src.points_num());
  gpu::VertBufPtr points_rad_buf = gpu::VertBuf::from_size<float>(src.points_num());

  MutableSpan<float3> points_pos = points_pos_buf->data<float3>();
  MutableSpan<float> points_rad = points_rad_buf->data<float>();

  const ParticleSettings &part = *src.psys->part;
  const float hair_rad_shape = part.shape;
  const float hair_rad_root = part.rad_root * part.rad_scale * 0.5f;
  const float hair_rad_tip = part.rad_tip * part.rad_scale * 0.5f;
  const bool hair_close_tip = (part.shape_flag & PART_SHAPE_CLOSE_TIP) != 0;

  int i = 0;
  src.particles_get().foreach_strand([&](Span<ParticleCacheKey> strand) {
    int j = 0;
    for (const ParticleCacheKey &point : strand) {
      points_pos[i] = point.co;
      points_rad[i] = (hair_close_tip && (j == strand.index_range().last())) ?
                          0.0f :
                          hair_shape_radius(
                              hair_rad_shape, hair_rad_root, hair_rad_tip, point.time);
      i++, j++;
    }
  });

  this->evaluated_pos_rad_buf = gpu::VertBuf::device_only<float4>(src.evaluated_points_num());

  float4x4 transform = src.object->world_to_object();

  module.evaluate_positions(true,
                            false,
                            false,
                            false,
                            false,
                            src.curves_num(),
                            *this,
                            std::move(points_pos_buf),
                            std::move(points_rad_buf),
                            evaluated_pos_rad_buf,
                            transform);

  /* TODO(fclem): Make time and length optional. */
  this->evaluated_time_buf = gpu::VertBuf::device_only<float>(src.evaluated_points_num());
  this->curves_length_buf = gpu::VertBuf::device_only<float>(src.curves_num());

  module.evaluate_curve_length_intercept(false, src.curves_num(), *this);
}

gpu::VertBufPtr &CurvesEvalCache::indirection_buf_get(CurvesModule &module,
                                                      ParticleDrawSource &src,
                                                      int face_per_segment)
{
  const bool is_ribbon = face_per_segment < 2;

  gpu::VertBufPtr &indirection_buf = is_ribbon ? this->indirection_ribbon_buf :
                                                 this->indirection_cylinder_buf;
  if (indirection_buf) {
    return indirection_buf;
  }

  if (src.curves_num() == 0) {
    /* Garbage data. */
    indirection_buf = gpu::VertBuf::device_only<int>(4);
    return indirection_buf;
  }

  ensure_common(src);

  indirection_buf = module.evaluate_topology_indirection(
      src.curves_num(), src.evaluated_points_num(), *this, is_ribbon, false);

  return indirection_buf;
}

CurvesEvalCache &hair_particle_get_eval_cache(ParticleDrawSource &src)
{
  ParticleBatchCache *cache = particle_batch_cache_get(src.psys);
  CurvesEvalCache &eval_cache = cache->hair.eval_cache;
  if (assign_if_different(eval_cache.resolution, src.resolution())) {
    particle_batch_cache_clear_hair(&cache->hair);
  }
  return eval_cache;
}

}  // namespace blender::draw
