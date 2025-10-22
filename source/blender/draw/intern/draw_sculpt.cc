/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "draw_sculpt.hh"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "draw_attributes.hh"
#include "draw_context_private.hh"
#include "draw_view.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_customdata.hh"
#include "BKE_material.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"

#include "BLI_math_matrix.hh"

#include "bmesh_class.hh"

#include "DRW_pbvh.hh"
#include "DRW_render.hh"

namespace blender::draw {

float3 SculptBatch::debug_color()
{
  static float3 colors[9] = {
      {1.0f, 0.2f, 0.2f},
      {0.2f, 1.0f, 0.2f},
      {0.2f, 0.2f, 1.0f},
      {1.0f, 1.0f, 0.2f},
      {0.2f, 1.0f, 1.0f},
      {1.0f, 0.2f, 1.0f},
      {1.0f, 0.7f, 0.2f},
      {0.2f, 1.0f, 0.7f},
      {0.7f, 0.2f, 1.0f},
  };

  return colors[debug_index % 9];
}

static Vector<SculptBatch> sculpt_batches_get_ex(const Object *ob,
                                                 const bool use_wire,
                                                 const Span<pbvh::AttributeRequest> attrs)
{
  /* pbvh::Tree should always exist for non-empty meshes, created by depsgraph eval. */
  bke::pbvh::Tree *pbvh = ob->sculpt ? const_cast<bke::pbvh::Tree *>(bke::object::pbvh_get(*ob)) :
                                       nullptr;
  if (!pbvh) {
    return {};
  }

  /* TODO(Miguel Pozo): Don't use global context. */
  const DRWContext *drwctx = DRW_context_get();
  RegionView3D *rv3d = drwctx->rv3d;
  const bool navigating = rv3d && (rv3d->rflag & RV3D_NAVIGATING);

  Paint *paint = nullptr;
  if (drwctx->evil_C != nullptr) {
    paint = BKE_paint_get_active_from_context(drwctx->evil_C);
  }

  /* TODO: take into account partial redraw for clipping planes. */
  /* Frustum planes to show only visible pbvh::Tree nodes. */
  std::array<float4, 6> draw_frustum_planes = View::default_get().frustum_planes_get();
  /* Transform clipping planes to object space. Transforming a plane with a
   * 4x4 matrix is done by multiplying with the transpose inverse.
   * The inverse cancels out here since we transform by inverse(obmat). */
  float4x4 tmat = math::transpose(ob->object_to_world());
  for (int i : IndexRange(draw_frustum_planes.size())) {
    draw_frustum_planes[i] = tmat * draw_frustum_planes[i];
  }

  /* Fast mode to show low poly multires while navigating. */
  bool fast_mode = false;
  if (paint && (paint->flags & PAINT_FAST_NAVIGATE)) {
    fast_mode = navigating;
  }

  /* Update draw buffers only for visible nodes while painting.
   * But do update them otherwise so navigating stays smooth. */
  bool update_only_visible = rv3d && !(rv3d->rflag & RV3D_PAINTING);
  if (paint && (paint->flags & PAINT_SCULPT_DELAY_UPDATES)) {
    update_only_visible = true;
  }

  bke::pbvh::update_normals_from_eval(*const_cast<Object *>(ob), *pbvh);

  pbvh::DrawCache &draw_data = pbvh::ensure_draw_data(pbvh->draw_data);

  IndexMaskMemory memory;
  const IndexMask visible_nodes = bke::pbvh::search_nodes(
      *pbvh, memory, [&](const bke::pbvh::Node &node) {
        return !BKE_pbvh_node_fully_hidden_get(node) &&
               bke::pbvh::node_frustum_contain_aabb(node, draw_frustum_planes);
      });

  const IndexMask nodes_to_update = update_only_visible ? visible_nodes :
                                                          bke::pbvh::all_leaf_nodes(*pbvh, memory);

  Span<gpu::Batch *> batches;
  if (use_wire) {
    batches = draw_data.ensure_lines_batches(*ob, {{}, fast_mode}, nodes_to_update);
  }
  else {
    batches = draw_data.ensure_tris_batches(*ob, {attrs, fast_mode}, nodes_to_update);
  }

  const Span<int> material_indices = draw_data.ensure_material_indices(*ob);

  const int max_material = std::max(0, BKE_object_material_count_eval(ob) - 1);
  Vector<SculptBatch> result_batches(visible_nodes.size());
  visible_nodes.foreach_index([&](const int i, const int pos) {
    result_batches[pos] = {};
    result_batches[pos].batch = batches[i];
    result_batches[pos].material_slot = material_indices.is_empty() ?
                                            0 :
                                            std::clamp(material_indices[i], 0, max_material);
    result_batches[pos].debug_index = pos;
  });

  return result_batches;
}

static const CustomData *get_cdata(const BMesh &bm, const bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return &bm.vdata;
    case bke::AttrDomain::Corner:
      return &bm.ldata;
    case bke::AttrDomain::Face:
      return &bm.pdata;
    default:
      return nullptr;
  }
}

static bool bmesh_attribute_exists(const BMesh &bm,
                                   const bke::AttributeMetaData &meta_data,
                                   const StringRef name)
{
  const CustomData *cdata = get_cdata(bm, meta_data.domain);
  return cdata && CustomData_get_offset_named(
                      cdata, *bke::attr_type_to_custom_data_type(meta_data.data_type), name) != -1;
}

Vector<SculptBatch> sculpt_batches_get(const Object *ob, SculptBatchFeature features)
{
  Vector<pbvh::AttributeRequest, 16> attrs;

  attrs.append(pbvh::CustomRequest::Position);
  attrs.append(pbvh::CustomRequest::Normal);
  if (features & SCULPT_BATCH_MASK) {
    attrs.append(pbvh::CustomRequest::Mask);
  }
  if (features & SCULPT_BATCH_FACE_SET) {
    attrs.append(pbvh::CustomRequest::FaceSet);
  }

  const Mesh *mesh = BKE_object_get_original_mesh(ob);
  const bke::AttributeAccessor attributes = mesh->attributes();
  const SculptSession &ss = *ob->sculpt;

  /* If Dyntopo is enabled, the source of truth for an attribute existing or not is the BMesh, not
   * the Mesh. */
  if (features & SCULPT_BATCH_VERTEX_COLOR) {
    if (const char *name = mesh->active_color_attribute) {
      if (const std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(
              name))
      {
        if (ss.bm) {
          if (bmesh_attribute_exists(*ss.bm, *meta_data, name)) {
            attrs.append(pbvh::GenericRequest(name));
          }
        }
        else {
          attrs.append(pbvh::GenericRequest(name));
        }
      }
    }
  }

  if (features & SCULPT_BATCH_UV) {
    const StringRef uv_name = mesh->active_uv_map_name();
    if (!uv_name.is_empty()) {
      attrs.append(pbvh::GenericRequest(uv_name));
    }
  }

  return sculpt_batches_get_ex(ob, features & SCULPT_BATCH_WIREFRAME, attrs);
}

Vector<SculptBatch> sculpt_batches_per_material_get(const Object *ob,
                                                    Span<const GPUMaterial *> materials)
{
  BLI_assert(ob->type == OB_MESH);
  const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(*ob);

  VectorSet<std::string> draw_attrs;
  DRW_MeshCDMask cd_needed;
  DRW_mesh_get_attributes(*ob, mesh, materials, &draw_attrs, &cd_needed);

  Vector<pbvh::AttributeRequest, 16> attrs;

  attrs.append(pbvh::CustomRequest::Position);
  attrs.append(pbvh::CustomRequest::Normal);

  for (const StringRef name : draw_attrs) {
    attrs.append(pbvh::GenericRequest(name));
  }

  for (const StringRef name : cd_needed.uv) {
    attrs.append(pbvh::GenericRequest(name));
  }

  return sculpt_batches_get_ex(ob, false, attrs);
}

}  // namespace blender::draw
