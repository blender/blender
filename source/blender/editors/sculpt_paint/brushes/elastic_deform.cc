/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_kelvinlet.h"
#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace elastic_deform_cc {

struct LocalData {
  Vector<float> factors;
  Vector<float3> translations;
};

BLI_NOINLINE static void calc_translations(const Brush &brush,
                                           const StrokeCache &cache,
                                           const KelvinletParams &kelvinet_params,
                                           const float3 &location,
                                           const float3 &offset,
                                           const Span<float3> positions,
                                           const MutableSpan<float3> translations)
{
  switch (eBrushElasticDeformType(brush.elastic_deform_type)) {
    case BRUSH_ELASTIC_DEFORM_GRAB: {
      for (const int i : positions.index_range()) {
        BKE_kelvinlet_grab(translations[i], &kelvinet_params, positions[i], location, offset);
      }
      scale_translations(translations, cache.bstrength * 20.0f);
      break;
    }
    case BRUSH_ELASTIC_DEFORM_GRAB_BISCALE: {
      for (const int i : positions.index_range()) {
        BKE_kelvinlet_grab_biscale(
            translations[i], &kelvinet_params, positions[i], location, offset);
      }
      scale_translations(translations, cache.bstrength * 20.0f);
      break;
    }
    case BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE: {
      for (const int i : positions.index_range()) {
        BKE_kelvinlet_grab_triscale(
            translations[i], &kelvinet_params, positions[i], location, offset);
      }
      scale_translations(translations, cache.bstrength * 20.0f);
      break;
    }
    case BRUSH_ELASTIC_DEFORM_SCALE: {
      for (const int i : positions.index_range()) {
        BKE_kelvinlet_scale(
            translations[i], &kelvinet_params, positions[i], location, cache.sculpt_normal_symm);
      }
      break;
    }
    case BRUSH_ELASTIC_DEFORM_TWIST: {
      for (const int i : positions.index_range()) {
        BKE_kelvinlet_twist(
            translations[i], &kelvinet_params, positions[i], location, cache.sculpt_normal_symm);
      }
      break;
    }
  }
}

static void calc_faces(const Sculpt &sd,
                       const Brush &brush,
                       const KelvinletParams &kelvinet_params,
                       const float3 &offset,
                       const Span<float3> positions_eval,
                       const PBVHNode &node,
                       Object &object,
                       LocalData &tls,
                       const MutableSpan<float3> positions_orig)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);
  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, orig_data.positions, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  tls.translations.reinitialize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(
      brush, cache, kelvinet_params, cache.location, offset, orig_data.positions, translations);

  write_translations(sd, object, positions_eval, verts, translations, positions_orig);
}

static void calc_grids(const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const KelvinletParams &kelvinet_params,
                       const float3 &offset,
                       PBVHNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);
  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const int grid_verts_num = grids.size() * key.grid_area;

  tls.factors.reinitialize(grid_verts_num);
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, orig_data.positions, factors);

  if (cache.automasking) {
    auto_mask::calc_grids_factors(object, *cache.automasking, node, grids, factors);
  }

  calc_brush_texture_factors(ss, brush, orig_data.positions, factors);

  tls.translations.reinitialize(grid_verts_num);
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(
      brush, cache, kelvinet_params, cache.location, offset, orig_data.positions, translations);

  clip_and_lock_translations(sd, ss, orig_data.positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const KelvinletParams &kelvinet_params,
                       const float3 &offset,
                       PBVHNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, orig_positions, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  tls.translations.reinitialize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(
      brush, cache, kelvinet_params, cache.location, offset, orig_positions, translations);

  clip_and_lock_translations(sd, ss, orig_positions, translations);
  apply_translations(translations, verts);
}

}  // namespace elastic_deform_cc

void do_elastic_deform_brush(const Sculpt &sd, Object &object, Span<PBVHNode *> nodes)
{
  const SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float strength = ss.cache->bstrength;

  float3 grab_delta = ss.cache->grab_delta_symmetry;
  if (ss.cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss.cache->normal_weight, grab_delta);
  }

  float dir;
  if (ss.cache->mouse[0] > ss.cache->initial_mouse[0]) {
    dir = 1.0f;
  }
  else {
    dir = -1.0f;
  }

  if (brush.elastic_deform_type == BRUSH_ELASTIC_DEFORM_TWIST) {
    int symm = ss.cache->mirror_symmetry_pass;
    if (ELEM(symm, 1, 2, 4, 7)) {
      dir = -dir;
    }
  }

  KelvinletParams params;
  float force = math::length(grab_delta) * dir * strength;
  BKE_kelvinlet_init_params(
      &params, ss.cache->radius, force, 1.0f, brush.elastic_deform_volume_preservation);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (BKE_pbvh_type(*object.sculpt->pbvh)) {
    case PBVH_FACES: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const PBVH &pbvh = *ss.pbvh;
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_faces(sd,
                     brush,
                     params,
                     grab_delta,
                     positions_eval,
                     *nodes[i],
                     object,
                     tls,
                     positions_orig);
          BKE_pbvh_node_mark_positions_update(nodes[i]);
        }
      });
      break;
    }
    case PBVH_GRIDS:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_grids(sd, object, brush, params, grab_delta, *nodes[i], tls);
        }
      });
      break;
    case PBVH_BMESH:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_bmesh(sd, object, brush, params, grab_delta, *nodes[i], tls);
        }
      });
      break;
  }
}

}  // namespace blender::ed::sculpt_paint
