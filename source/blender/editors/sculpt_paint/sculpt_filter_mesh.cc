/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "sculpt_filter.hh"  // TODO- Move this function's initialization to a separate file

#include <fmt/format.h>

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "BLT_translation.hh"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_view3d.hh"

#include "mesh_brush_common.hh"
#include "paint_intern.hh"
#include "sculpt_automask.hh"
#include "sculpt_boundary.hh"
#include "sculpt_cloth.hh"
#include "sculpt_face_set.hh"
#include "sculpt_intern.hh"
#include "sculpt_smooth.hh"
#include "sculpt_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "bmesh.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace blender::ed::sculpt_paint::filter {

float3x3 to_orientation_space(const filter::Cache &filter_cache)
{
  switch (filter_cache.orientation) {
    case FilterOrientation::Local:
      return float3x3::identity();
    case FilterOrientation::World:
      return float3x3(filter_cache.obmat);
    case FilterOrientation::View:
      return float3x3(filter_cache.obmat * filter_cache.viewmat);
  }
  BLI_assert_unreachable();
  return float3x3::identity();
}

float3x3 to_object_space(const filter::Cache &filter_cache)
{
  switch (filter_cache.orientation) {
    case FilterOrientation::Local:
      return float3x3::identity();
    case FilterOrientation::World:
      return float3x3(filter_cache.obmat_inv);
    case FilterOrientation::View:
      return float3x3(filter_cache.viewmat_inv * filter_cache.obmat_inv);
  }
  BLI_assert_unreachable();
  return float3x3::identity();
}

void zero_disabled_axis_components(const filter::Cache &filter_cache,
                                   const MutableSpan<float3> vectors)
{
  if (filter_cache.enabled_axis[0] && filter_cache.enabled_axis[1] && filter_cache.enabled_axis[2])
  {
    return;
  }

  if (filter_cache.orientation == FilterOrientation::Local) {
    for (const int i : vectors.index_range()) {
      for (int axis = 0; axis < 3; axis++) {
        if (!filter_cache.enabled_axis[axis]) {
          vectors[i][axis] = 0.0f;
        }
      }
    }
  }

  const float3x3 local_to_orientation = to_orientation_space(filter_cache);
  const float3x3 orientation_to_object = to_object_space(filter_cache);
  for (const int i : vectors.index_range()) {
    float3 vector = local_to_orientation * vectors[i];
    for (int axis = 0; axis < 3; axis++) {
      if (!filter_cache.enabled_axis[axis]) {
        vector[axis] = 0.0f;
      }
    }
    vectors[i] = orientation_to_object * vector;
  }
}

Cache::~Cache() = default;

void cache_init(bContext *C,
                Object &ob,
                const Sculpt &sd,
                const undo::Type undo_type,
                const float mval_fl[2],
                float area_normal_radius,
                float start_strength)
{
  SculptSession &ss = *ob.sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  ss.filter_cache = MEM_new<filter::Cache>(__func__);
  ss.filter_cache->start_filter_strength = start_strength;
  ss.filter_cache->random_seed = rand();

  ss.filter_cache->node_mask = bke::pbvh::search_nodes(
      pbvh, ss.filter_cache->node_mask_memory, [&](const bke::pbvh::Node &node) {
        return !node_fully_masked_or_hidden(node);
      });

  undo::push_nodes(*depsgraph, ob, ss.filter_cache->node_mask, undo_type);

  /* Setup orientation matrices. */
  copy_m4_m4(ss.filter_cache->obmat.ptr(), ob.object_to_world().ptr());
  invert_m4_m4(ss.filter_cache->obmat_inv.ptr(), ob.object_to_world().ptr());

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  ss.filter_cache->vc = vc;
  if (vc.rv3d) {
    copy_m4_m4(ss.filter_cache->viewmat.ptr(), vc.rv3d->viewmat);
    copy_m4_m4(ss.filter_cache->viewmat_inv.ptr(), vc.rv3d->viewinv);
  }

  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  float3 co;

  if (vc.rv3d && SCULPT_stroke_get_location(C, co, mval_fl, false)) {
    /* Get radius from brush. */
    const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

    float radius;
    if (brush) {
      if (BKE_brush_use_locked_size(scene, brush)) {
        radius = paint_calc_object_space_radius(
            vc, co, float(BKE_brush_size_get(scene, brush) * area_normal_radius));
      }
      else {
        radius = BKE_brush_unprojected_radius_get(scene, brush) * area_normal_radius;
      }
    }
    else {
      radius = paint_calc_object_space_radius(vc, co, float(ups->size) * area_normal_radius);
    }

    const float radius_sq = math::square(radius);

    IndexMaskMemory memory;
    const IndexMask node_mask = bke::pbvh::search_nodes(
        pbvh, memory, [&](const bke::pbvh::Node &node) {
          return !node_fully_masked_or_hidden(node) && node_in_sphere(node, co, radius_sq, true);
        });

    const std::optional<float3> area_normal = calc_area_normal(*depsgraph, *brush, ob, node_mask);
    if (BKE_paint_brush_for_read(&sd.paint) && area_normal) {
      ss.filter_cache->initial_normal = *area_normal;
      ss.last_normal = ss.filter_cache->initial_normal;
    }
    else {
      ss.filter_cache->initial_normal = ss.last_normal;
    }

    /* Update last stroke location */

    mul_m4_v3(ob.object_to_world().ptr(), co);

    add_v3_v3(ups->average_stroke_accum, co);
    ups->average_stroke_counter++;
    ups->last_stroke_valid = true;
  }
  else {
    /* Use last normal. */
    copy_v3_v3(ss.filter_cache->initial_normal, ss.last_normal);
  }

  /* Update view normal */
  float3x3 mat;
  float3 viewDir{0.0f, 0.0f, 1.0f};
  if (vc.rv3d) {
    invert_m4_m4(ob.runtime->world_to_object.ptr(), ob.object_to_world().ptr());
    copy_m3_m4(mat.ptr(), vc.rv3d->viewinv);
    mul_m3_v3(mat.ptr(), viewDir);
    copy_m3_m4(mat.ptr(), ob.world_to_object().ptr());
    mul_m3_v3(mat.ptr(), viewDir);
    normalize_v3_v3(ss.filter_cache->view_normal, viewDir);
  }
}

enum class MeshFilterType {
  Smooth = 0,
  Scale = 1,
  Inflate = 2,
  Sphere = 3,
  Random = 4,
  Relax = 5,
  RelaxFaceSets = 6,
  SurfaceSmooth = 7,
  Sharpen = 8,
  EnhanceDetails = 9,
  EraseDisplacement = 10,
};

static EnumPropertyItem prop_mesh_filter_types[] = {
    {int(MeshFilterType::Smooth), "SMOOTH", 0, "Smooth", "Smooth mesh"},
    {int(MeshFilterType::Scale), "SCALE", 0, "Scale", "Scale mesh"},
    {int(MeshFilterType::Inflate), "INFLATE", 0, "Inflate", "Inflate mesh"},
    {int(MeshFilterType::Sphere), "SPHERE", 0, "Sphere", "Morph into sphere"},
    {int(MeshFilterType::Random), "RANDOM", 0, "Random", "Randomize vertex positions"},
    {int(MeshFilterType::Relax), "RELAX", 0, "Relax", "Relax mesh"},
    {int(MeshFilterType::RelaxFaceSets),
     "RELAX_FACE_SETS",
     0,
     "Relax Face Sets",
     "Smooth the edges of all the Face Sets"},
    {int(MeshFilterType::SurfaceSmooth),
     "SURFACE_SMOOTH",
     0,
     "Surface Smooth",
     "Smooth the surface of the mesh, preserving the volume"},
    {int(MeshFilterType::Sharpen), "SHARPEN", 0, "Sharpen", "Sharpen the cavities of the mesh"},
    {int(MeshFilterType::EnhanceDetails),
     "ENHANCE_DETAILS",
     0,
     "Enhance Details",
     "Enhance the high frequency surface detail"},
    {int(MeshFilterType::EraseDisplacement),
     "ERASE_DISPLACEMENT",
     0,
     "Erase Displacement",
     "Deletes the displacement of the Multires Modifier"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum eMeshFilterDeformAxis {
  MESH_FILTER_DEFORM_X = 1 << 0,
  MESH_FILTER_DEFORM_Y = 1 << 1,
  MESH_FILTER_DEFORM_Z = 1 << 2,
};

static EnumPropertyItem prop_mesh_filter_deform_axis_items[] = {
    {MESH_FILTER_DEFORM_X, "X", 0, "X", "Deform in the X axis"},
    {MESH_FILTER_DEFORM_Y, "Y", 0, "Y", "Deform in the Y axis"},
    {MESH_FILTER_DEFORM_Z, "Z", 0, "Z", "Deform in the Z axis"},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem prop_mesh_filter_orientation_items[] = {
    {int(FilterOrientation::Local),
     "LOCAL",
     0,
     "Local",
     "Use the local axis to limit the displacement"},
    {int(FilterOrientation::World),
     "WORLD",
     0,
     "World",
     "Use the global axis to limit the displacement"},
    {int(FilterOrientation::View),
     "VIEW",
     0,
     "View",
     "Use the view axis to limit the displacement"},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool sculpt_mesh_filter_needs_pmap(MeshFilterType filter_type)
{
  return ELEM(filter_type,
              MeshFilterType::Smooth,
              MeshFilterType::Relax,
              MeshFilterType::RelaxFaceSets,
              MeshFilterType::SurfaceSmooth,
              MeshFilterType::EnhanceDetails,
              MeshFilterType::Sharpen);
}

static bool sculpt_mesh_filter_is_continuous(MeshFilterType type)
{
  return ELEM(type,
              MeshFilterType::Sharpen,
              MeshFilterType::Smooth,
              MeshFilterType::Relax,
              MeshFilterType::RelaxFaceSets);
}

BLI_NOINLINE static void clamp_factors(const MutableSpan<float> factors,
                                       const float min,
                                       const float max)
{
  for (float &factor : factors) {
    factor = std::clamp(factor, min, max);
  }
}

static void calc_smooth_filter(const Depsgraph &depsgraph,
                               const Sculpt &sd,
                               const float strength,
                               Object &object,
                               const IndexMask &node_mask,
                               const bool use_original_position)
{
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<int> neighbor_offsets;
    Vector<int> neighbor_data;
    Vector<float3> new_positions;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();
        const Span<float3> positions = gather_data_mesh(position_data.eval, verts, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_mesh(object, nodes[i]);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, -1.0f, 1.0f);

        const GroupedSpan<int> neighbors = calc_vert_neighbors_interior(faces,
                                                                        corner_verts,
                                                                        vert_to_face_map,
                                                                        ss.vertex_info.boundary,
                                                                        attribute_data.hide_poly,
                                                                        verts,
                                                                        tls.neighbor_offsets,
                                                                        tls.neighbor_data);

        tls.new_positions.resize(verts.size());
        const MutableSpan<float3> new_positions = tls.new_positions;
        smooth::neighbor_data_average_mesh_check_loose(
            position_data.eval, verts, neighbors, new_positions);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        if (use_original_position) {
          translations_from_new_positions(new_positions, orig_data.positions, translations);
        }
        else {
          translations_from_new_positions(new_positions, positions, translations);
        }
        scale_translations(translations, factors);
        if (use_original_position) {
          reset_translations_to_original(translations, positions, orig_data.positions);
        }

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = base_mesh.faces();
      const Span<int> corner_verts = base_mesh.corner_verts();

      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_grids(object, nodes[i]);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, -1.0f, 1.0f);

        tls.new_positions.resize(positions.size());
        const MutableSpan<float3> new_positions = tls.new_positions;
        smooth::neighbor_position_average_interior_grids(
            faces, corner_verts, ss.vertex_info.boundary, subdiv_ccg, grids, new_positions);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        if (use_original_position) {
          translations_from_new_positions(new_positions, orig_data.positions, translations);
        }
        else {
          translations_from_new_positions(new_positions, positions, translations);
        }
        scale_translations(translations, factors);
        if (use_original_position) {
          reset_translations_to_original(translations, positions, orig_data.positions);
        }

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);
        Array<float3> orig_positions(verts.size());
        orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, {});

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, -1.0f, 1.0f);

        tls.new_positions.resize(verts.size());
        const MutableSpan<float3> new_positions = tls.new_positions;
        smooth::neighbor_position_average_interior_bmesh(verts, new_positions);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        if (use_original_position) {
          translations_from_new_positions(new_positions, orig_positions, translations);
        }
        else {
          translations_from_new_positions(new_positions, positions, translations);
        }
        scale_translations(translations, factors);
        if (use_original_position) {
          reset_translations_to_original(translations, positions, orig_positions);
        }

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

static void calc_inflate_filter(const Depsgraph &depsgraph,
                                const Sculpt &sd,
                                const float strength,
                                Object &object,
                                const IndexMask &node_mask)
{
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
      const VArraySpan mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point);
      const PositionDeformData position_data(depsgraph, object);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();
        const OrigPositionData orig_data = orig_position_data_get_mesh(object, nodes[i]);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(hide_vert, mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_data.normals);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_grids(object, nodes[i]);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_data.normals);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);
        Array<float3> orig_positions(verts.size());
        Array<float3> orig_normals(verts.size());
        orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_normals);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

static void calc_scale_filter(const Depsgraph &depsgraph,
                              const Sculpt &sd,
                              const float strength,
                              Object &object,
                              const IndexMask &node_mask)
{
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();
        const OrigPositionData orig_data = orig_position_data_get_mesh(object, nodes[i]);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_data.positions);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_grids(object, nodes[i]);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_data.positions);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);
        Array<float3> orig_positions(verts.size());
        orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, {});

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_positions);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

BLI_NOINLINE static void calc_sphere_translations(const Span<float3> positions,
                                                  const Span<float> factors,
                                                  const MutableSpan<float3> translations)
{
  for (const int i : positions.index_range()) {
    translations[i] = math::midpoint(math::normalize(positions[i]), -positions[i]) *
                      math::abs(factors[i]);
  }
}

static void calc_sphere_filter(const Depsgraph &depsgraph,
                               const Sculpt &sd,
                               const float strength,
                               Object &object,
                               const IndexMask &node_mask)
{
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const PositionDeformData position_data(depsgraph, object);
      const MeshAttributeData attribute_data(mesh);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();
        const Span<float3> positions = gather_data_mesh(position_data.eval, verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        calc_sphere_translations(positions, factors, translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        calc_sphere_translations(positions, factors, translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        calc_sphere_translations(positions, factors, translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

BLI_NOINLINE static void randomize_factors(const Span<float3> positions,
                                           const int seed,
                                           const MutableSpan<float> factors)
{
  BLI_assert(positions.size() == factors.size());
  for (const int i : positions.index_range()) {
    const uint *hash_co = (const uint *)&positions[i];
    const uint hash = BLI_hash_int_2d(hash_co[0], hash_co[1]) ^ BLI_hash_int_2d(hash_co[2], seed);
    factors[i] *= (hash * (1.0f / float(0xFFFFFFFF)) - 0.5f);
  }
}

static void calc_random_filter(const Depsgraph &depsgraph,
                               const Sculpt &sd,
                               const float strength,
                               Object &object,
                               const IndexMask &node_mask)
{
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();
        const OrigPositionData orig_data = orig_position_data_get_mesh(object, nodes[i]);
        const Span<float3> positions = gather_data_mesh(position_data.eval, verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);

        randomize_factors(positions, ss.filter_cache->random_seed, factors);
        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_data.normals);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_grids(object, nodes[i]);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);

        randomize_factors(positions, ss.filter_cache->random_seed, factors);
        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_data.normals);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);
        Array<float3> orig_positions(verts.size());
        Array<float3> orig_normals(verts.size());
        orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);

        randomize_factors(positions, ss.filter_cache->random_seed, factors);
        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        translations.copy_from(orig_normals);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

static void calc_relax_filter(const Depsgraph &depsgraph,
                              const Sculpt &sd,
                              const float strength,
                              Object &object,
                              const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  bke::pbvh::update_normals(depsgraph, object, pbvh);
  switch (pbvh.type()) {
    struct LocalData {
      Vector<float> factors;
      Vector<float3> translations;
    };
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const PositionDeformData position_data(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const MeshAttributeData attribute_data(mesh);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::calc_relaxed_translations_faces(position_data.eval,
                                                vert_normals,
                                                faces,
                                                corner_verts,
                                                vert_to_face_map,
                                                ss.vertex_info.boundary,
                                                attribute_data.face_sets,
                                                attribute_data.hide_poly,
                                                false,
                                                verts,
                                                factors,
                                                translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      struct LocalData {
        Vector<float> factors;
        Vector<float3> positions;
        Vector<float3> translations;
      };
      const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = base_mesh.faces();
      const Span<int> corner_verts = base_mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = base_mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = base_mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                           bke::AttrDomain::Face);
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::calc_relaxed_translations_grids(subdiv_ccg,
                                                faces,
                                                corner_verts,
                                                face_sets,
                                                vert_to_face_map,
                                                ss.vertex_info.boundary,
                                                grids,
                                                false,
                                                factors,
                                                translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      struct LocalData {
        Vector<float> factors;
        Vector<float3> positions;
        Vector<float3> translations;
      };
      BMesh &bm = *ss.bm;
      const int face_set_offset = CustomData_get_offset_named(
          &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");

      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::calc_relaxed_translations_bmesh(
            verts, positions, face_set_offset, false, factors, translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

static void calc_relax_face_sets_filter(const Depsgraph &depsgraph,
                                        const Sculpt &sd,
                                        const float strength,
                                        Object &object,
                                        const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  bke::pbvh::update_normals(depsgraph, object, pbvh);

  /* When using the relax face sets meshes filter, each 3 iterations, do a whole mesh relax to
   * smooth the contents of the Face Set. This produces better results as the relax operation is no
   * completely focused on the boundaries. */
  const bool relax_face_sets = !(ss.filter_cache->iteration_count % 3 == 0);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      struct LocalData {
        Vector<float> factors;
        Vector<float3> positions;
        Vector<float3> translations;
      };
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const PositionDeformData position_data(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const MeshAttributeData attribute_data(mesh);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        face_set::filter_verts_with_unique_face_sets_mesh(
            vert_to_face_map, attribute_data.face_sets, relax_face_sets, verts, factors);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::calc_relaxed_translations_faces(position_data.eval,
                                                vert_normals,
                                                faces,
                                                corner_verts,
                                                vert_to_face_map,
                                                ss.vertex_info.boundary,
                                                attribute_data.face_sets,
                                                attribute_data.hide_poly,
                                                relax_face_sets,
                                                verts,
                                                factors,
                                                translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      struct LocalData {
        Vector<float> factors;
        Vector<float3> positions;
        Vector<float3> translations;
      };
      const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = base_mesh.faces();
      const Span<int> corner_verts = base_mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = base_mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = base_mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                           bke::AttrDomain::Face);

      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        face_set::filter_verts_with_unique_face_sets_grids(faces,
                                                           corner_verts,
                                                           vert_to_face_map,
                                                           face_sets,
                                                           subdiv_ccg,
                                                           relax_face_sets,
                                                           grids,
                                                           factors);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::calc_relaxed_translations_grids(subdiv_ccg,
                                                faces,
                                                corner_verts,
                                                face_sets,
                                                vert_to_face_map,
                                                ss.vertex_info.boundary,
                                                grids,
                                                relax_face_sets,
                                                factors,
                                                translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      struct LocalData {
        Vector<float> factors;
        Vector<float3> positions;
        Vector<float3> translations;
      };
      BMesh &bm = *ss.bm;
      const int face_set_offset = CustomData_get_offset_named(
          &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        face_set::filter_verts_with_unique_face_sets_bmesh(
            face_set_offset, relax_face_sets, verts, factors);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::calc_relaxed_translations_bmesh(
            verts, positions, face_set_offset, relax_face_sets, factors, translations);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

static void calc_surface_smooth_filter(const Depsgraph &depsgraph,
                                       const Sculpt &sd,
                                       const float strength,
                                       Object &object,
                                       const IndexMask &node_mask)
{
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<int> neighbor_offsets;
    Vector<int> neighbor_data;
    Vector<float3> average_positions;
    Vector<float3> laplacian_disp;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const float alpha = ss.filter_cache->surface_smooth_shape_preservation;
  const float beta = ss.filter_cache->surface_smooth_current_vertex;
  const MutableSpan<float3> all_laplacian_disp = ss.filter_cache->surface_smooth_laplacian_disp;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();
        const Span<float3> positions = gather_data_mesh(position_data.eval, verts, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_mesh(object, nodes[i]);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                               corner_verts,
                                                               vert_to_face_map,
                                                               attribute_data.hide_poly,
                                                               verts,
                                                               tls.neighbor_offsets,
                                                               tls.neighbor_data);

        tls.average_positions.reinitialize(verts.size());
        const MutableSpan<float3> average_positions = tls.average_positions;
        smooth::neighbor_data_average_mesh_check_loose(
            position_data.eval, verts, neighbors, average_positions);

        tls.laplacian_disp.reinitialize(verts.size());
        const MutableSpan<float3> laplacian_disp = tls.laplacian_disp;
        tls.translations.reinitialize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_laplacian_step(positions,
                                              orig_data.positions,
                                              average_positions,
                                              alpha,
                                              laplacian_disp,
                                              translations);
        scale_translations(translations, factors);

        scatter_data_mesh(laplacian_disp.as_span(), verts, all_laplacian_disp);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        const MutableSpan<float3> laplacian_disp = gather_data_mesh(
            all_laplacian_disp.as_span(), verts, tls.laplacian_disp);

        const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                               corner_verts,
                                                               vert_to_face_map,
                                                               attribute_data.hide_poly,
                                                               verts,
                                                               tls.neighbor_offsets,
                                                               tls.neighbor_data);

        tls.average_positions.resize(verts.size());
        const MutableSpan<float3> average_laplacian_disps = tls.average_positions;
        smooth::neighbor_data_average_mesh_check_loose(
            all_laplacian_disp.as_span(), verts, neighbors, average_laplacian_disps);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_displace_step(
            laplacian_disp, average_laplacian_disps, beta, translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_grids(object, nodes[i]);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        tls.average_positions.resize(positions.size());
        const MutableSpan<float3> average_positions = tls.average_positions;
        smooth::average_data_grids(
            subdiv_ccg, subdiv_ccg.positions.as_span(), grids, average_positions);

        tls.laplacian_disp.resize(positions.size());
        const MutableSpan<float3> laplacian_disp = tls.laplacian_disp;
        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_laplacian_step(positions,
                                              orig_data.positions,
                                              average_positions,
                                              alpha,
                                              laplacian_disp,
                                              translations);
        scale_translations(translations, factors);

        scatter_data_grids(subdiv_ccg, laplacian_disp.as_span(), grids, all_laplacian_disp);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        const MutableSpan<float3> laplacian_disp = gather_data_grids(
            subdiv_ccg, all_laplacian_disp.as_span(), grids, tls.laplacian_disp);

        tls.average_positions.resize(positions.size());
        const MutableSpan<float3> average_laplacian_disps = tls.average_positions;
        smooth::average_data_grids(
            subdiv_ccg, all_laplacian_disp.as_span(), grids, average_laplacian_disps);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_displace_step(
            laplacian_disp, average_laplacian_disps, beta, translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);
        Array<float3> orig_positions(verts.size());
        Array<float3> orig_normals(verts.size());
        orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        tls.average_positions.resize(verts.size());
        const MutableSpan<float3> average_positions = tls.average_positions;
        smooth::neighbor_position_average_bmesh(verts, average_positions);

        tls.laplacian_disp.resize(verts.size());
        const MutableSpan<float3> laplacian_disp = tls.laplacian_disp;
        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_laplacian_step(
            positions, orig_positions, average_positions, alpha, laplacian_disp, translations);
        scale_translations(translations, factors);

        scatter_data_bmesh(laplacian_disp.as_span(), verts, all_laplacian_disp);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, strength);
        clamp_factors(factors, 0.0f, 1.0f);

        const MutableSpan<float3> laplacian_disp = gather_data_bmesh(
            all_laplacian_disp.as_span(), verts, tls.laplacian_disp);

        tls.average_positions.resize(verts.size());
        const MutableSpan<float3> average_laplacian_disps = tls.average_positions;
        smooth::average_data_bmesh(all_laplacian_disp.as_span(), verts, average_laplacian_disps);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_displace_step(
            laplacian_disp, average_laplacian_disps, beta, translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

BLI_NOINLINE static void calc_sharpen_detail_translations(const filter::Cache &filter_cache,
                                                          const Span<float3> positions,
                                                          const Span<float3> smooth_positions,
                                                          const Span<float> sharpen_factors,
                                                          const Span<float3> detail_directions,
                                                          const MutableSpan<float3> translations)
{
  for (const int i : positions.index_range()) {
    float3 disp_avg = smooth_positions[i] - positions[i];
    disp_avg = disp_avg * filter_cache.sharpen_smooth_ratio * pow2f(sharpen_factors[i]);
    translations[i] += disp_avg;
    /* Intensify details. */
    if (filter_cache.sharpen_intensify_detail_strength > 0.0f) {
      float3 detail_strength = detail_directions[i];
      translations[i] += detail_strength * -filter_cache.sharpen_intensify_detail_strength *
                         sharpen_factors[i];
    }
  }
}

static void calc_sharpen_filter(const Depsgraph &depsgraph,
                                const Sculpt &sd,
                                const float strength,
                                Object &object,
                                const IndexMask &node_mask)
{
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<int> neighbor_offsets;
    Vector<int> neighbor_data;
    Vector<float3> smooth_positions;
    Vector<float> sharpen_factors;
    Vector<float3> detail_directions;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);

      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int node_index) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[node_index].verts();
        const Span<float3> positions = gather_data_mesh(position_data.eval, verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(depsgraph,
                                     object,
                                     ss.filter_cache->automasking.get(),
                                     nodes[node_index],
                                     verts,
                                     factors);
        scale_factors(factors, strength);

        /* This filter can't work at full strength as it needs multiple iterations to reach a
         * stable state. */
        clamp_factors(factors, 0.0f, 0.5f);

        const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                               corner_verts,
                                                               vert_to_face_map,
                                                               attribute_data.hide_poly,
                                                               verts,
                                                               tls.neighbor_offsets,
                                                               tls.neighbor_data);

        tls.smooth_positions.resize(verts.size());
        const MutableSpan<float3> smooth_positions = tls.smooth_positions;
        smooth::neighbor_data_average_mesh_check_loose(
            position_data.eval, verts, neighbors, smooth_positions);

        const Span<float> sharpen_factors = gather_data_mesh(
            ss.filter_cache->sharpen_factor.as_span(), verts, tls.sharpen_factors);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        for (const int i : verts.index_range()) {
          const int vert = verts[i];
          const float3 &position = position_data.eval[vert];

          float3 disp_sharpen(0.0f);
          for (const int neighbor : neighbors[i]) {
            float3 disp_n = position_data.eval[neighbor] - position;
            disp_n *= ss.filter_cache->sharpen_factor[neighbor];
            disp_sharpen += disp_n;
          }

          disp_sharpen *= (1.0f - sharpen_factors[i]);
          translations[i] = disp_sharpen;
        }

        const Span<float3> detail_directions = gather_data_mesh(
            ss.filter_cache->detail_directions.as_span(), verts, tls.detail_directions);

        calc_sharpen_detail_translations(*ss.filter_cache,
                                         positions,
                                         smooth_positions,
                                         sharpen_factors,
                                         detail_directions,
                                         translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      MutableSpan<float3> vert_positions = subdiv_ccg.positions;

      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int node_index) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[node_index].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(depsgraph,
                                      object,
                                      ss.filter_cache->automasking.get(),
                                      nodes[node_index],
                                      grids,
                                      factors);
        scale_factors(factors, strength);

        /* This filter can't work at full strength as it needs multiple iterations to reach a
         * stable state. */
        clamp_factors(factors, 0.0f, 0.5f);

        tls.smooth_positions.resize(positions.size());
        const MutableSpan<float3> smooth_positions = tls.smooth_positions;
        smooth::average_data_grids(
            subdiv_ccg, subdiv_ccg.positions.as_span(), grids, smooth_positions);

        const Span<float> sharpen_factors = gather_data_grids(
            subdiv_ccg, ss.filter_cache->sharpen_factor.as_span(), grids, tls.sharpen_factors);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        for (const int i : grids.index_range()) {
          const int node_verts_start = i * key.grid_area;
          const int grid = grids[i];
          for (const short y : IndexRange(key.grid_size)) {
            for (const short x : IndexRange(key.grid_size)) {
              const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
              const int node_vert = node_verts_start + offset;

              const float3 &position = positions[node_vert];

              float3 disp_sharpen(0.0f);
              SubdivCCGNeighbors neighbors;
              BKE_subdiv_ccg_neighbor_coords_get(
                  subdiv_ccg, SubdivCCGCoord{grid, x, y}, false, neighbors);
              for (const SubdivCCGCoord neighbor : neighbors.coords) {
                float3 disp_n = vert_positions[neighbor.to_index(key)] - position;
                disp_n *= ss.filter_cache->sharpen_factor[neighbor.to_index(key)];
                disp_sharpen += disp_n;
              }

              disp_sharpen *= (1.0f - sharpen_factors[node_vert]);
              translations[node_vert] = disp_sharpen;
            }
          }
        }

        const Span<float3> detail_directions = gather_data_grids(
            subdiv_ccg,
            ss.filter_cache->detail_directions.as_span(),
            grids,
            tls.detail_directions);

        calc_sharpen_detail_translations(*ss.filter_cache,
                                         positions,
                                         smooth_positions,
                                         sharpen_factors,
                                         detail_directions,
                                         translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      BM_mesh_elem_index_ensure(&bm, BM_VERT);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int node_index) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[node_index]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(depsgraph,
                                     object,
                                     ss.filter_cache->automasking.get(),
                                     nodes[node_index],
                                     verts,
                                     factors);
        scale_factors(factors, strength);

        /* This filter can't work at full strength as it needs multiple iterations to reach a
         * stable state. */
        clamp_factors(factors, 0.0f, 0.5f);

        tls.smooth_positions.resize(verts.size());
        const MutableSpan<float3> smooth_positions = tls.smooth_positions;
        smooth::neighbor_position_average_bmesh(verts, smooth_positions);

        const Span<float> sharpen_factors = gather_data_bmesh(
            ss.filter_cache->sharpen_factor.as_span(), verts, tls.sharpen_factors);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;

        Vector<BMVert *, 64> neighbors;

        int i = 0;
        for (BMVert *vert : verts) {
          const float3 position = vert->co;

          float3 disp_sharpen(0.0f);
          for (const BMVert *neighbor : vert_neighbors_get_bmesh(*vert, neighbors)) {
            float3 disp_n = float3(neighbor->co) - position;
            disp_n *= ss.filter_cache->sharpen_factor[BM_elem_index_get(neighbor)];
            disp_sharpen += disp_n;
          }

          disp_sharpen *= (1.0f - sharpen_factors[i]);
          translations[i] = disp_sharpen;
          i++;
        }

        const Span<float3> detail_directions = gather_data_bmesh(
            ss.filter_cache->detail_directions.as_span(), verts, tls.detail_directions);

        calc_sharpen_detail_translations(*ss.filter_cache,
                                         positions,
                                         smooth_positions,
                                         sharpen_factors,
                                         detail_directions,
                                         translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

static void calc_enhance_details_filter(const Depsgraph &depsgraph,
                                        const Sculpt &sd,
                                        const float strength,
                                        Object &object,
                                        const IndexMask &node_mask)
{
  const float final_strength = -std::abs(strength);
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, final_strength);

        const MutableSpan translations = gather_data_mesh(
            ss.filter_cache->detail_directions.as_span(), verts, tls.translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

        tls.factors.resize(positions.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
        scale_factors(factors, final_strength);

        const MutableSpan translations = gather_data_grids(
            subdiv_ccg, ss.filter_cache->detail_directions.as_span(), grids, tls.translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(
            depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], verts, factors);
        scale_factors(factors, final_strength);

        const MutableSpan<float3> translations = gather_data_bmesh(
            ss.filter_cache->detail_directions.as_span(), verts, tls.translations);
        scale_translations(translations, factors);

        zero_disabled_axis_components(*ss.filter_cache, translations);
        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      });
      break;
    }
  }
}

static void calc_erase_displacement_filter(const Depsgraph &depsgraph,
                                           const Sculpt &sd,
                                           const float strength,
                                           Object &object,
                                           const IndexMask &node_mask)
{
  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
    Vector<float3> new_positions;
    Vector<float3> translations;
  };
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    const Span<int> grids = nodes[i].grids();
    const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

    tls.factors.resize(positions.size());
    const MutableSpan<float> factors = tls.factors;
    fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
    auto_mask::calc_grids_factors(
        depsgraph, object, ss.filter_cache->automasking.get(), nodes[i], grids, factors);
    scale_factors(factors, strength);
    clamp_factors(factors, -1.0f, 1.0f);

    const MutableSpan<float3> new_positions = gather_data_grids(
        subdiv_ccg, ss.filter_cache->limit_surface_co.as_span(), grids, tls.new_positions);
    tls.translations.resize(positions.size());
    const MutableSpan<float3> translations = tls.translations;
    translations_from_new_positions(new_positions, positions, translations);
    scale_translations(translations, factors);

    zero_disabled_axis_components(*ss.filter_cache, translations);
    clip_and_lock_translations(sd, ss, positions, translations);
    apply_translations(translations, grids, subdiv_ccg);
  });
}

static void mesh_filter_surface_smooth_init(Object &object,
                                            const float shape_preservation,
                                            const float current_vertex_displacement)
{
  SculptSession &ss = *object.sculpt;
  const int totvert = SCULPT_vertex_count_get(object);
  filter::Cache *filter_cache = ss.filter_cache;

  filter_cache->surface_smooth_laplacian_disp.reinitialize(totvert);
  filter_cache->surface_smooth_shape_preservation = shape_preservation;
  filter_cache->surface_smooth_current_vertex = current_vertex_displacement;
}

static void calc_limit_surface_positions(const Object &object, MutableSpan<float3> limit_positions)
{
  const SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  threading::parallel_for(IndexRange(subdiv_ccg.grids_num), 512, [&](const IndexRange range) {
    for (const int grid : range) {
      MutableSpan grid_dst = limit_positions.slice(bke::ccg::grid_range(key, grid));
      BKE_subdiv_ccg_eval_limit_positions(subdiv_ccg, key, grid, grid_dst);
    }
  });
}

static void mesh_filter_sharpen_init(const Depsgraph &depsgraph,
                                     const Object &object,
                                     const float smooth_ratio,
                                     const float intensify_detail_strength,
                                     const int curvature_smooth_iterations,
                                     filter::Cache &filter_cache)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const IndexMask &node_mask = filter_cache.node_mask;
  const int totvert = SCULPT_vertex_count_get(object);

  filter_cache.sharpen_smooth_ratio = smooth_ratio;
  filter_cache.sharpen_intensify_detail_strength = intensify_detail_strength;
  filter_cache.sharpen_curvature_smooth_iterations = curvature_smooth_iterations;
  filter_cache.sharpen_factor.reinitialize(totvert);
  filter_cache.detail_directions.reinitialize(totvert);
  MutableSpan<float3> detail_directions = filter_cache.detail_directions;
  MutableSpan<float> sharpen_factors = filter_cache.sharpen_factor;

  calc_smooth_translations(depsgraph, object, node_mask, filter_cache.detail_directions);

  for (int i = 0; i < totvert; i++) {
    sharpen_factors[i] = math::length(detail_directions[i]);
  }

  float max_factor = 0.0f;
  for (int i = 0; i < totvert; i++) {
    max_factor = std::max(sharpen_factors[i], max_factor);
  }

  max_factor = 1.0f / max_factor;
  for (int i = 0; i < totvert; i++) {
    sharpen_factors[i] *= max_factor;
    sharpen_factors[i] = 1.0f - pow2f(1.0f - sharpen_factors[i]);
  }

  /* Smooth the calculated factors and directions to remove high frequency detail. */
  struct LocalData {
    Vector<int> neighbor_offsets;
    Vector<int> neighbor_data;
    Vector<float3> smooth_directions;
    Vector<float> smooth_factors;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  for ([[maybe_unused]] const int _ : IndexRange(filter_cache.sharpen_curvature_smooth_iterations))
  {
    switch (pbvh.type()) {
      case bke::pbvh::Type::Mesh: {
        Mesh &mesh = *static_cast<Mesh *>(object.data);
        const OffsetIndices faces = mesh.faces();
        const Span<int> corner_verts = mesh.corner_verts();
        const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
        const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
        const MeshAttributeData attribute_data(mesh);
        node_mask.foreach_index(GrainSize(1), [&](const int i) {
          LocalData &tls = all_tls.local();
          const Span<int> verts = nodes[i].verts();

          const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                                 corner_verts,
                                                                 vert_to_face_map,
                                                                 attribute_data.hide_poly,
                                                                 verts,
                                                                 tls.neighbor_offsets,
                                                                 tls.neighbor_data);

          tls.smooth_directions.resize(verts.size());
          smooth::neighbor_data_average_mesh_check_loose(detail_directions.as_span(),
                                                         verts,
                                                         neighbors,
                                                         tls.smooth_directions.as_mutable_span());
          scatter_data_mesh(tls.smooth_directions.as_span(), verts, detail_directions);

          tls.smooth_factors.resize(verts.size());
          smooth::neighbor_data_average_mesh_check_loose(
              sharpen_factors.as_span(), verts, neighbors, tls.smooth_factors.as_mutable_span());
          scatter_data_mesh(tls.smooth_factors.as_span(), verts, sharpen_factors);
        });
        break;
      }
      case bke::pbvh::Type::Grids: {
        SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
        const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
        node_mask.foreach_index(GrainSize(1), [&](const int i) {
          LocalData &tls = all_tls.local();
          const Span<int> grids = nodes[i].grids();
          const int grid_verts_num = grids.size() * key.grid_area;

          tls.smooth_directions.resize(grid_verts_num);
          smooth::average_data_grids(subdiv_ccg,
                                     detail_directions.as_span(),
                                     grids,
                                     tls.smooth_directions.as_mutable_span());
          scatter_data_grids(
              subdiv_ccg, tls.smooth_directions.as_span(), grids, detail_directions);

          tls.smooth_factors.resize(grid_verts_num);
          smooth::average_data_grids(
              subdiv_ccg, sharpen_factors.as_span(), grids, tls.smooth_factors.as_mutable_span());
          scatter_data_grids(subdiv_ccg, tls.smooth_factors.as_span(), grids, sharpen_factors);
        });
        break;
      }
      case bke::pbvh::Type::BMesh:
        const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
        node_mask.foreach_index(GrainSize(1), [&](const int i) {
          LocalData &tls = all_tls.local();
          const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(
              &const_cast<bke::pbvh::BMeshNode &>(nodes[i]));

          tls.smooth_directions.resize(verts.size());
          smooth::average_data_bmesh(
              detail_directions.as_span(), verts, tls.smooth_directions.as_mutable_span());
          scatter_data_bmesh(tls.smooth_directions.as_span(), verts, detail_directions);

          tls.smooth_factors.resize(verts.size());
          smooth::average_data_bmesh(
              sharpen_factors.as_span(), verts, tls.smooth_factors.as_mutable_span());
          scatter_data_bmesh(tls.smooth_factors.as_span(), verts, sharpen_factors);
        });
        break;
    }
  }
}

enum {
  FILTER_MESH_MODAL_CANCEL = 1,
  FILTER_MESH_MODAL_CONFIRM,
};

wmKeyMap *modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {FILTER_MESH_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {FILTER_MESH_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Mesh Filter Modal Map");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return nullptr;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Mesh Filter Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "SCULPT_OT_mesh_filter");

  return keymap;
}

static void sculpt_mesh_update_status_bar(bContext *C, wmOperator * /*op*/)
{
  WorkspaceStatus status(C);
  status.item(IFACE_("Confirm"), ICON_EVENT_RETURN);
  status.item(IFACE_("Cancel"), ICON_EVENT_ESC, ICON_MOUSE_RMB);
}

static void sculpt_mesh_filter_apply(bContext *C, wmOperator *op, bool is_replay = false)
{
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  const MeshFilterType filter_type = MeshFilterType(RNA_enum_get(op->ptr, "type"));
  const float strength = RNA_float_get(op->ptr, "strength");

  SCULPT_vertex_random_access_ensure(ob);

  const IndexMask &node_mask = ss.filter_cache->node_mask;
  if (auto_mask::is_enabled(sd, ob, nullptr) && ss.filter_cache->automasking &&
      ss.filter_cache->automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL)
  {
    ss.filter_cache->automasking->calc_cavity_factor(depsgraph, ob, node_mask);
  }
  switch (filter_type) {
    case MeshFilterType::Smooth:
      calc_smooth_filter(depsgraph,
                         sd,
                         strength,
                         ob,
                         node_mask,
                         is_replay && ss.filter_cache->iteration_count == 0);
      break;
    case MeshFilterType::Scale:
      calc_scale_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::Inflate:
      calc_inflate_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::Sphere:
      calc_sphere_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::Random:
      calc_random_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::Relax:
      calc_relax_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::RelaxFaceSets:
      calc_relax_face_sets_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::SurfaceSmooth:
      calc_surface_smooth_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::Sharpen:
      calc_sharpen_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::EnhanceDetails:
      calc_enhance_details_filter(depsgraph, sd, strength, ob, node_mask);
      break;
    case MeshFilterType::EraseDisplacement:
      calc_erase_displacement_filter(depsgraph, sd, strength, ob, node_mask);
      break;
  }

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  pbvh.tag_positions_changed(node_mask);
  pbvh.update_bounds(depsgraph, ob);

  ss.filter_cache->iteration_count++;

  flush_update_step(C, UpdateType::Position);
}

static void sculpt_mesh_update_strength(wmOperator *op,
                                        SculptSession &ss,
                                        float2 prev_press_mouse,
                                        float2 mouse)
{
  const float len = prev_press_mouse[0] - mouse[0];

  float filter_strength = ss.filter_cache->start_filter_strength * -len * 0.001f * UI_SCALE_FAC;
  RNA_float_set(op->ptr, "strength", filter_strength);
}
static void sculpt_mesh_filter_apply_with_history(bContext *C, wmOperator *op)
{
  /* Event history is only stored for smooth and relax filters. */
  if (!RNA_collection_length(op->ptr, "event_history")) {
    sculpt_mesh_filter_apply(C, op);
    return;
  }

  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  float2 start_mouse;
  bool first = true;
  float initial_strength = ss.filter_cache->start_filter_strength;

  RNA_BEGIN (op->ptr, item, "event_history") {
    float2 mouse;
    RNA_float_get_array(&item, "mouse_event", mouse);

    if (first) {
      first = false;
      start_mouse = mouse;
      continue;
    }

    sculpt_mesh_update_strength(op, ss, start_mouse, mouse);
    sculpt_mesh_filter_apply(C, op, true);
  }
  RNA_END;

  RNA_float_set(op->ptr, "strength", initial_strength);
}

static void sculpt_mesh_filter_end(bContext *C)
{
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;

  MEM_delete(ss.filter_cache);
  ss.filter_cache = nullptr;
  flush_update_done(C, ob, UpdateType::Position);
}

static int sculpt_mesh_filter_confirm(SculptSession &ss,
                                      wmOperator *op,
                                      const MeshFilterType filter_type)
{
  float initial_strength = ss.filter_cache->start_filter_strength;
  /* Don't update strength property if we're storing an event history. */
  if (sculpt_mesh_filter_is_continuous(filter_type)) {
    RNA_float_set(op->ptr, "strength", initial_strength);
  }

  return OPERATOR_FINISHED;
}

static void sculpt_mesh_filter_cancel(bContext *C, wmOperator * /*op*/)
{
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession *ss = ob.sculpt;
  bke::pbvh::Tree *pbvh = bke::object::pbvh_get(ob);

  if (!ss || !pbvh) {
    return;
  }

  undo::restore_position_from_undo_step(depsgraph, ob);
  bke::pbvh::update_normals(depsgraph, ob, *pbvh);
  pbvh->update_bounds(depsgraph, ob);
}

static int sculpt_mesh_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object &ob = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SculptSession &ss = *ob.sculpt;
  const MeshFilterType filter_type = MeshFilterType(RNA_enum_get(op->ptr, "type"));

  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EW_SCROLL);
  sculpt_mesh_update_status_bar(C, op);

  if (event->type == EVT_MODAL_MAP) {
    int ret = OPERATOR_FINISHED;
    switch (event->val) {
      case FILTER_MESH_MODAL_CANCEL:
        sculpt_mesh_filter_cancel(C, op);
        undo::push_end_ex(ob, true);
        ret = OPERATOR_CANCELLED;
        break;

      case FILTER_MESH_MODAL_CONFIRM:
        ret = sculpt_mesh_filter_confirm(ss, op, filter_type);
        undo::push_end_ex(ob, false);
        break;
    }

    sculpt_mesh_filter_end(C);
    ED_workspace_status_text(C, nullptr); /* Clear status bar */
    WM_cursor_modal_restore(CTX_wm_window(C));

    return ret;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* NOTE: some filter types are continuous, for these we store an
   * event history in RNA for continuous.
   * This way the user can tweak the last operator properties
   * or repeat the op and get expected results. */
  if (sculpt_mesh_filter_is_continuous(filter_type)) {
    if (RNA_collection_length(op->ptr, "event_history") == 0) {
      /* First entry is the start mouse position, event->prev_press_xy. */
      PointerRNA startptr;
      RNA_collection_add(op->ptr, "event_history", &startptr);

      float2 mouse_start(float(event->prev_press_xy[0]), float(event->prev_press_xy[1]));
      RNA_float_set_array(&startptr, "mouse_event", mouse_start);
    }

    PointerRNA itemptr;
    RNA_collection_add(op->ptr, "event_history", &itemptr);

    float2 mouse(float(event->xy[0]), float(event->xy[1]));
    RNA_float_set_array(&itemptr, "mouse_event", mouse);
    RNA_float_set(&itemptr, "pressure", WM_event_tablet_data(event, nullptr, nullptr));
  }
  else {
    undo::restore_position_from_undo_step(*depsgraph, ob);
  }

  float2 prev_mval(float(event->prev_press_xy[0]), float(event->prev_press_xy[1]));
  float2 mval(float(event->xy[0]), float(event->xy[1]));

  sculpt_mesh_update_strength(op, ss, prev_mval, mval);

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  sculpt_mesh_filter_apply(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_filter_specific_init(const Depsgraph &depsgraph,
                                        const MeshFilterType filter_type,
                                        wmOperator *op,
                                        Object &object)
{
  SculptSession &ss = *object.sculpt;
  switch (filter_type) {
    case MeshFilterType::SurfaceSmooth: {
      mesh_filter_surface_smooth_init(object,
                                      RNA_float_get(op->ptr, "surface_smooth_shape_preservation"),
                                      RNA_float_get(op->ptr, "surface_smooth_current_vertex"));
      break;
    }
    case MeshFilterType::Sharpen: {
      mesh_filter_sharpen_init(depsgraph,
                               object,
                               RNA_float_get(op->ptr, "sharpen_smooth_ratio"),
                               RNA_float_get(op->ptr, "sharpen_intensify_detail_strength"),
                               RNA_int_get(op->ptr, "sharpen_curvature_smooth_iterations"),
                               *ss.filter_cache);
      break;
    }
    case MeshFilterType::EnhanceDetails: {
      ss.filter_cache->detail_directions.reinitialize(SCULPT_vertex_count_get(object));
      calc_smooth_translations(
          depsgraph, object, ss.filter_cache->node_mask, ss.filter_cache->detail_directions);
      break;
    }
    case MeshFilterType::EraseDisplacement: {
      ss.filter_cache->limit_surface_co.reinitialize(SCULPT_vertex_count_get(object));
      calc_limit_surface_positions(object, ss.filter_cache->limit_surface_co);
      break;
    }
    default:
      break;
  }
}

/* Returns OPERATOR_PASS_THROUGH on success. */
static int sculpt_mesh_filter_start(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  int mval[2];
  RNA_int_get_array(op->ptr, "start_mouse", mval);

  const MeshFilterType filter_type = MeshFilterType(RNA_enum_get(op->ptr, "type"));
  const bool use_automasking = auto_mask::is_enabled(sd, ob, nullptr);
  const bool needs_topology_info = sculpt_mesh_filter_needs_pmap(filter_type) || use_automasking;

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  if (report_if_shape_key_is_locked(ob, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  SculptSession &ss = *ob.sculpt;

  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  if (filter_type == MeshFilterType::EraseDisplacement && pbvh.type() != bke::pbvh::Type::Grids) {
    return OPERATOR_CANCELLED;
  }

  const eMeshFilterDeformAxis deform_axis = eMeshFilterDeformAxis(
      RNA_enum_get(op->ptr, "deform_axis"));

  if (deform_axis == 0) {
    /* All axis are disabled, so the filter is not going to produce any deformation. */
    return OPERATOR_CANCELLED;
  }

  float2 mval_fl{float(mval[0]), float(mval[1])};
  if (use_automasking) {
    /* Update the active face set manually as the paint cursor is not enabled when using the
     * Mesh Filter Tool. */
    SculptCursorGeometryInfo sgi;
    SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);
  }

  SCULPT_vertex_random_access_ensure(ob);
  if (needs_topology_info) {
    boundary::ensure_boundary_info(ob);
  }

  undo::push_begin(scene, ob, op);

  cache_init(C,
             ob,
             sd,
             undo::Type::Position,
             mval_fl,
             RNA_float_get(op->ptr, "area_normal_radius"),
             RNA_float_get(op->ptr, "strength"));

  filter::Cache *filter_cache = ss.filter_cache;
  filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  filter_cache->automasking = auto_mask::cache_init(*depsgraph, sd, ob);

  sculpt_filter_specific_init(*depsgraph, filter_type, op, ob);

  ss.filter_cache->enabled_axis[0] = deform_axis & MESH_FILTER_DEFORM_X;
  ss.filter_cache->enabled_axis[1] = deform_axis & MESH_FILTER_DEFORM_Y;
  ss.filter_cache->enabled_axis[2] = deform_axis & MESH_FILTER_DEFORM_Z;

  ss.filter_cache->orientation = FilterOrientation(RNA_enum_get(op->ptr, "orientation"));

  return OPERATOR_PASS_THROUGH;
}

static int sculpt_mesh_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set_array(op->ptr, "start_mouse", event->mval);
  int ret = sculpt_mesh_filter_start(C, op);

  if (ret == OPERATOR_PASS_THROUGH) {
    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return ret;
}

static int sculpt_mesh_filter_exec(bContext *C, wmOperator *op)
{
  int ret = sculpt_mesh_filter_start(C, op);

  if (ret == OPERATOR_PASS_THROUGH) {
    int iterations = RNA_int_get(op->ptr, "iteration_count");

    for (int i = 0; i < iterations; i++) {
      sculpt_mesh_filter_apply_with_history(C, op);
    }

    sculpt_mesh_filter_end(C);
    undo::push_end(*CTX_data_active_object(C));

    return OPERATOR_FINISHED;
  }

  return ret;
}

void register_operator_props(wmOperatorType *ot)
{
  RNA_def_int_array(
      ot->srna, "start_mouse", 2, nullptr, 0, 1 << 14, "Starting Mouse", "", 0, 1 << 14);

  RNA_def_float(ot->srna,
                "area_normal_radius",
                0.25,
                0.001,
                5.0,
                "Normal Radius",
                "Radius used for calculating area normal on initial click,\nin percentage "
                "of brush radius",
                0.01,
                1.0);
  PropertyRNA *prop = RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter strength", -10.0f, 10.0f);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_int(ot->srna,
              "iteration_count",
              1,
              1,
              10000,
              "Repeat",
              "How many times to repeat the filter",
              1,
              100);

  /* Smooth filter requires entire event history. */
  prop = RNA_def_collection_runtime(ot->srna, "event_history", &RNA_OperatorStrokeElement, "", "");
  RNA_def_property_flag(prop, PropertyFlag(int(PROP_HIDDEN) | int(PROP_SKIP_SAVE)));
}

static void sculpt_mesh_ui_exec(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  uiItemR(layout, op->ptr, "strength", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  uiItemR(layout, op->ptr, "iteration_count", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  uiItemR(layout, op->ptr, "orientation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout = uiLayoutRow(layout, true);
  uiItemR(layout, op->ptr, "deform_axis", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

void SCULPT_OT_mesh_filter(wmOperatorType *ot)
{
  ot->name = "Filter Mesh";
  ot->idname = "SCULPT_OT_mesh_filter";
  ot->description = "Applies a filter to modify the current mesh";

  ot->invoke = sculpt_mesh_filter_invoke;
  ot->modal = sculpt_mesh_filter_modal;
  ot->poll = SCULPT_mode_poll;
  ot->exec = sculpt_mesh_filter_exec;
  ot->ui = sculpt_mesh_ui_exec;

  /* Doesn't seem to actually be called?
   * Check `sculpt_mesh_filter_modal` to see where it's really called. */
  ot->cancel = sculpt_mesh_filter_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR_X | OPTYPE_BLOCKING |
             OPTYPE_DEPENDS_ON_CURSOR;

  register_operator_props(ot);

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          prop_mesh_filter_types,
                          int(MeshFilterType::Inflate),
                          "Filter Type",
                          "Operation that is going to be applied to the mesh");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_enum_flag(ot->srna,
                    "deform_axis",
                    prop_mesh_filter_deform_axis_items,
                    MESH_FILTER_DEFORM_X | MESH_FILTER_DEFORM_Y | MESH_FILTER_DEFORM_Z,
                    "Deform Axis",
                    "Apply the deformation in the selected axis");
  RNA_def_enum(ot->srna,
               "orientation",
               prop_mesh_filter_orientation_items,
               int(FilterOrientation::Local),
               "Orientation",
               "Orientation of the axis to limit the filter displacement");

  /* Surface Smooth Mesh Filter properties. */
  RNA_def_float(ot->srna,
                "surface_smooth_shape_preservation",
                0.5f,
                0.0f,
                1.0f,
                "Shape Preservation",
                "How much of the original shape is preserved when smoothing",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "surface_smooth_current_vertex",
                0.5f,
                0.0f,
                1.0f,
                "Per Vertex Displacement",
                "How much the position of each individual vertex influences the final result",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "sharpen_smooth_ratio",
                0.35f,
                0.0f,
                1.0f,
                "Smooth Ratio",
                "How much smoothing is applied to polished surfaces",
                0.0f,
                1.0f);

  RNA_def_float(ot->srna,
                "sharpen_intensify_detail_strength",
                0.0f,
                0.0f,
                10.0f,
                "Intensify Details",
                "How much creases and valleys are intensified",
                0.0f,
                1.0f);

  RNA_def_int(ot->srna,
              "sharpen_curvature_smooth_iterations",
              0,
              0,
              10,
              "Curvature Smooth Iterations",
              "How much smooth the resulting shape is, ignoring high frequency details",
              0,
              10);
}

}  // namespace blender::ed::sculpt_paint::filter
