/* SPDX-FileCopyrightText: 2012 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "paint_mask.hh"

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_array_utils.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_context.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_select_utils.hh"

#include "bmesh.hh"

#include "mesh_brush_common.hh"
#include "paint_intern.hh"
#include "sculpt_automask.hh"
#include "sculpt_gesture.hh"
#include "sculpt_hide.hh"
#include "sculpt_intern.hh"
#include "sculpt_undo.hh"

namespace blender::ed::sculpt_paint::mask {

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

Array<float> duplicate_mask(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArray mask = *attributes.lookup_or_default<float>(
          ".sculpt_mask", bke::AttrDomain::Point, 0.0f);
      Array<float> result(mask.size());
      mask.materialize(result);
      return result;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      if (subdiv_ccg.masks.is_empty()) {
        return Array<float>(subdiv_ccg.positions.size(), 0.0f);
      }
      return subdiv_ccg.masks;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
      Array<float> result(bm.totvert);
      if (offset == -1) {
        result.fill(0.0f);
      }
      else {
        BM_mesh_elem_table_ensure(&bm, BM_VERT);
        for (const int i : result.index_range()) {
          result[i] = BM_ELEM_CD_GET_FLOAT(BM_vert_at_index(&bm, i), offset);
        }
      }
      return result;
    }
  }
  BLI_assert_unreachable();
  return {};
}

void mix_new_masks(const Span<float> new_masks,
                   const Span<float> factors,
                   const MutableSpan<float> masks)
{
  BLI_assert(new_masks.size() == factors.size());
  BLI_assert(new_masks.size() == masks.size());

  for (const int i : masks.index_range()) {
    masks[i] += (new_masks[i] - masks[i]) * factors[i];
  }
}

void clamp_mask(const MutableSpan<float> masks)
{
  for (float &mask : masks) {
    mask = std::clamp(mask, 0.0f, 1.0f);
  }
}

void invert_mask(const MutableSpan<float> masks)
{
  for (float &mask : masks) {
    mask = 1.0f - mask;
  }
}

void gather_mask_bmesh(const BMesh &bm,
                       const Set<BMVert *, 0> &verts,
                       const MutableSpan<float> r_mask)
{
  BLI_assert(verts.size() == r_mask.size());

  /* TODO: Avoid overhead of accessing attributes for every bke::pbvh::Tree node. */
  const int mask_offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  int i = 0;
  for (const BMVert *vert : verts) {
    r_mask[i] = (mask_offset == -1) ? 0.0f : BM_ELEM_CD_GET_FLOAT(vert, mask_offset);
    i++;
  }
}

void gather_mask_grids(const SubdivCCG &subdiv_ccg,
                       const Span<int> grids,
                       const MutableSpan<float> r_mask)
{
  if (!subdiv_ccg.masks.is_empty()) {
    gather_data_grids(subdiv_ccg, subdiv_ccg.masks.as_span(), grids, r_mask);
  }
  else {
    r_mask.fill(0.0f);
  }
}

void scatter_mask_grids(const Span<float> mask, SubdivCCG &subdiv_ccg, const Span<int> grids)
{
  scatter_data_grids(subdiv_ccg, mask, grids, subdiv_ccg.masks.as_mutable_span());
}

void scatter_mask_bmesh(const Span<float> mask, const BMesh &bm, const Set<BMVert *, 0> &verts)
{
  BLI_assert(verts.size() == mask.size());

  const int mask_offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  BLI_assert(mask_offset != -1);
  int i = 0;
  for (BMVert *vert : verts) {
    BM_ELEM_CD_SET_FLOAT(vert, mask_offset, mask[i]);
    i++;
  }
}

static float average_masks(const int mask_offset, const Span<const BMVert *> verts)
{
  float sum = 0;
  for (const BMVert *vert : verts) {
    sum += BM_ELEM_CD_GET_FLOAT(vert, mask_offset);
  }
  return sum / float(verts.size());
}

void average_neighbor_mask_bmesh(const int mask_offset,
                                 const Set<BMVert *, 0> &verts,
                                 const MutableSpan<float> new_masks)
{
  Vector<BMVert *, 64> neighbors;
  int i = 0;
  for (BMVert *vert : verts) {
    new_masks[i] = average_masks(mask_offset, vert_neighbors_get_bmesh(*vert, neighbors));
    i++;
  }
}

void update_mask_mesh(const Depsgraph &depsgraph,
                      Object &object,
                      const IndexMask &node_mask,
                      FunctionRef<void(MutableSpan<float>, Span<int>)> update_fn)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
      ".sculpt_mask", bke::AttrDomain::Point);
  if (!mask) {
    return;
  }

  struct LocalData {
    Vector<int> visible_verts;
    Vector<float> mask;
  };

  Array<bool> node_changed(node_mask.min_array_size(), false);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    const Span<int> verts = hide::node_visible_verts(nodes[i], hide_vert, tls.visible_verts);
    tls.mask.resize(verts.size());
    gather_data_mesh(mask.span.as_span(), verts, tls.mask.as_mutable_span());
    update_fn(tls.mask, verts);
    if (array_utils::indexed_data_equal<float>(mask.span, verts, tls.mask)) {
      return;
    }
    undo::push_node(depsgraph, object, &nodes[i], undo::Type::Mask);
    scatter_data_mesh(tls.mask.as_span(), verts, mask.span);
    bke::pbvh::node_update_mask_mesh(mask.span, nodes[i]);
    node_changed[i] = true;
  });

  IndexMaskMemory memory;
  pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));

  mask.finish();
}

bool mask_equals_array_grids(const Span<float> masks,
                             const CCGKey &key,
                             const Span<int> grids,
                             const Span<float> values)
{
  BLI_assert(grids.size() * key.grid_area == values.size());

  const IndexRange range = grids.index_range();
  return std::all_of(range.begin(), range.end(), [&](const int i) {
    return masks.slice(bke::ccg::grid_range(key, grids[i])) ==
           values.slice(bke::ccg::grid_range(key, i));
    return true;
  });
}

bool mask_equals_array_bmesh(const int mask_offset,
                             const Set<BMVert *, 0> &verts,
                             const Span<float> values)
{
  BLI_assert(verts.size() == values.size());

  int i = 0;
  for (const BMVert *vert : verts) {
    if (BM_ELEM_CD_GET_FLOAT(vert, mask_offset) != values[i]) {
      return false;
    }
    i++;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Global Mask Operators
 * Operators that act upon the entirety of a given object's mesh.
 * \{ */

/* The gesture API doesn't write to this enum type,
 * it writes to eSelectOp from ED_select_utils.hh.
 * We must thus map the modes here to the desired
 * eSelectOp modes.
 *
 * Fixes #102349.
 */
enum class FloodFillMode {
  Value = SEL_OP_SUB,
  InverseValue = SEL_OP_ADD,
  InverseMeshValue = SEL_OP_XOR,
};

static const EnumPropertyItem mode_items[] = {
    {int(FloodFillMode::Value),
     "VALUE",
     0,
     "Value",
     "Set mask to the level specified by the 'value' property"},
    {int(FloodFillMode::InverseValue),
     "VALUE_INVERSE",
     0,
     "Value Inverted",
     "Set mask to the level specified by the inverted 'value' property"},
    {int(FloodFillMode::InverseMeshValue), "INVERT", 0, "Invert", "Invert the mask"},
    {0}};

static Span<int> get_hidden_verts(const bke::pbvh::MeshNode &node,
                                  const Span<bool> hide_vert,
                                  Vector<int> &indices)
{
  if (hide_vert.is_empty()) {
    return {};
  }
  const Span<int> verts = node.verts();
  if (BKE_pbvh_node_fully_hidden_get(node)) {
    return verts;
  }
  indices.resize(verts.size());
  const int *end = std::copy_if(verts.begin(), verts.end(), indices.begin(), [&](const int vert) {
    return hide_vert[vert];
  });
  indices.resize(end - indices.begin());
  return indices;
}

static bool try_remove_mask_mesh(const Depsgraph &depsgraph,
                                 Object &object,
                                 const IndexMask &node_mask)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArraySpan mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point);
  if (mask.is_empty()) {
    return true;
  }

  /* If there are any hidden vertices that shouldn't be affected with a mask value set, the
   * attribute cannot be removed. This could also be done by building an IndexMask in the full
   * vertex domain. */
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  threading::EnumerableThreadSpecific<Vector<int>> all_index_data;
  const bool hidden_masked_verts = threading::parallel_reduce(
      node_mask.index_range(),
      1,
      false,
      [&](const IndexRange range, bool value) {
        if (value) {
          return value;
        }
        Vector<int> &index_data = all_index_data.local();
        node_mask.slice(range).foreach_index([&](const int i) {
          if (value) {
            return;
          }
          const Span<int> verts = get_hidden_verts(nodes[i], hide_vert, index_data);
          if (std::any_of(verts.begin(), verts.end(), [&](int i) { return mask[i] > 0.0f; })) {
            value = true;
            return;
          }
        });
        return value;
      },
      std::logical_or());
  if (hidden_masked_verts) {
    return false;
  }

  IndexMaskMemory memory;
  const IndexMask changed_nodes = IndexMask::from_predicate(
      node_mask, GrainSize(1), memory, [&](const int i) {
        const Span<int> verts = nodes[i].verts();
        return std::any_of(
            verts.begin(), verts.end(), [&](const int i) { return mask[i] != 0.0f; });
      });

  undo::push_nodes(depsgraph, object, changed_nodes, undo::Type::Mask);
  attributes.remove(".sculpt_mask");
  changed_nodes.foreach_index([&](const int i) {
    BKE_pbvh_node_fully_masked_set(nodes[i], false);
    BKE_pbvh_node_fully_unmasked_set(nodes[i], true);
  });
  pbvh.tag_masks_changed(changed_nodes);
  return true;
}

static void fill_mask_mesh(const Depsgraph &depsgraph,
                           Object &object,
                           const float value,
                           const IndexMask &node_mask)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  if (value == 0.0f) {
    if (try_remove_mask_mesh(depsgraph, object, node_mask)) {
      return;
    }
  }

  bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
      ".sculpt_mask", bke::AttrDomain::Point);

  Array<bool> node_changed(node_mask.min_array_size(), false);

  threading::EnumerableThreadSpecific<Vector<int>> all_index_data;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    Vector<int> &index_data = all_index_data.local();
    const Span<int> verts = hide::node_visible_verts(nodes[i], hide_vert, index_data);
    if (std::all_of(verts.begin(), verts.end(), [&](int i) { return mask.span[i] == value; })) {
      return;
    }
    undo::push_node(depsgraph, object, &nodes[i], undo::Type::Mask);
    mask.span.fill_indices(verts, value);
    node_changed[i] = true;
  });

  IndexMaskMemory memory;
  const IndexMask changed_nodes = IndexMask::from_bools(node_mask, node_changed, memory);
  pbvh.tag_masks_changed(changed_nodes);

  mask.finish();
  changed_nodes.foreach_index([&](const int i) {
    BKE_pbvh_node_fully_masked_set(nodes[i], value == 1.0f);
    BKE_pbvh_node_fully_unmasked_set(nodes[i], value == 0.0f);
  });
}

static void fill_mask_grids(Main &bmain,
                            const Scene &scene,
                            Depsgraph &depsgraph,
                            Object &object,
                            const float value,
                            const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;

  if (value == 0.0f && ss.subdiv_ccg->masks.is_empty()) {
    /* NOTE: Deleting the mask array would be possible here. */
    return;
  }

  MultiresModifierData &mmd = *BKE_sculpt_multires_active(&scene, &object);
  BKE_sculpt_mask_layers_ensure(&depsgraph, &bmain, &object, &mmd);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;

  Array<bool> node_changed(node_mask.min_array_size(), false);

  MutableSpan<float> masks = subdiv_ccg.masks;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    const Span<int> grid_indices = nodes[i].grids();
    if (std::all_of(grid_indices.begin(), grid_indices.end(), [&](const int grid) {
          const Span<float> grid_masks = masks.slice(bke::ccg::grid_range(key, grid));
          return std::all_of(grid_masks.begin(), grid_masks.end(), [&](const float mask) {
            return mask == value;
          });
        }))
    {
      return;
    }
    undo::push_node(depsgraph, object, &nodes[i], undo::Type::Mask);

    if (grid_hidden.is_empty()) {
      for (const int grid : grid_indices) {
        masks.slice(bke::ccg::grid_range(key, grid)).fill(value);
      }
    }
    else {
      for (const int grid : grid_indices) {
        MutableSpan<float> grid_masks = masks.slice(bke::ccg::grid_range(key, grid));
        bits::foreach_0_index(grid_hidden[grid], [&](const int i) { grid_masks[i] = value; });
      }
    }
    node_changed[i] = true;
  });

  IndexMaskMemory memory;
  const IndexMask changed_nodes = IndexMask::from_bools(node_changed, memory);
  if (node_changed.is_empty()) {
    return;
  }
  pbvh.tag_masks_changed(changed_nodes);
  multires_mark_as_modified(&depsgraph, &object, MULTIRES_COORDS_MODIFIED);
  changed_nodes.foreach_index([&](const int i) {
    BKE_pbvh_node_fully_masked_set(nodes[i], value == 1.0f);
    BKE_pbvh_node_fully_unmasked_set(nodes[i], value == 0.0f);
  });
}

static void fill_mask_bmesh(const Depsgraph &depsgraph,
                            Object &object,
                            const float value,
                            const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();

  BMesh &bm = *ss.bm;
  const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  if (value == 0.0f && offset == -1) {
    return;
  }
  if (offset == -1) {
    /* Mask is not dynamically added or removed for dynamic topology sculpting. */
    BLI_assert_unreachable();
    return;
  }

  undo::push_nodes(depsgraph, object, node_mask, undo::Type::Mask);

  Array<bool> node_changed(node_mask.min_array_size(), false);

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    bool changed = false;
    for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(&nodes[i])) {
      if (!BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
        if (BM_ELEM_CD_GET_FLOAT(vert, offset) != value) {
          BM_ELEM_CD_SET_FLOAT(vert, offset, value);
          changed = true;
        }
      }
    }
    if (changed) {
      node_changed[i] = true;
    }
  });
  IndexMaskMemory memory;
  const IndexMask changed_nodes = IndexMask::from_bools(node_changed, memory);
  if (node_changed.is_empty()) {
    return;
  }
  pbvh.tag_masks_changed(changed_nodes);
  changed_nodes.foreach_index([&](const int i) {
    BKE_pbvh_node_fully_masked_set(nodes[i], value == 1.0f);
    BKE_pbvh_node_fully_unmasked_set(nodes[i], value == 0.0f);
  });
}

static void fill_mask(
    Main &bmain, const Scene &scene, Depsgraph &depsgraph, Object &object, const float value)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      fill_mask_mesh(depsgraph, object, value, node_mask);
      break;
    case bke::pbvh::Type::Grids:
      fill_mask_grids(bmain, scene, depsgraph, object, value, node_mask);
      break;
    case bke::pbvh::Type::BMesh:
      fill_mask_bmesh(depsgraph, object, value, node_mask);
      break;
  }
}

static void invert_mask_grids(Main &bmain,
                              const Scene &scene,
                              Depsgraph &depsgraph,
                              Object &object,
                              const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;

  MultiresModifierData &mmd = *BKE_sculpt_multires_active(&scene, &object);
  BKE_sculpt_mask_layers_ensure(&depsgraph, &bmain, &object, &mmd);

  undo::push_nodes(depsgraph, object, node_mask, undo::Type::Mask);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  MutableSpan<float> masks = subdiv_ccg.masks;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    const Span<int> grid_indices = nodes[i].grids();
    if (grid_hidden.is_empty()) {
      for (const int grid : grid_indices) {
        for (float &value : masks.slice(bke::ccg::grid_range(key, grid))) {
          value = 1.0f - value;
        }
      }
    }
    else {
      for (const int grid : grid_indices) {
        MutableSpan<float> grid_masks = masks.slice(bke::ccg::grid_range(key, grid));
        bits::foreach_0_index(grid_hidden[grid],
                              [&](const int i) { grid_masks[i] = 1.0f - grid_masks[i]; });
      }
    }
    bke::pbvh::node_update_mask_grids(key, masks, nodes[i]);
  });
  pbvh.tag_masks_changed(node_mask);

  multires_mark_as_modified(&depsgraph, &object, MULTIRES_COORDS_MODIFIED);
}

static void invert_mask_bmesh(const Depsgraph &depsgraph,
                              Object &object,
                              const IndexMask &node_mask)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  BMesh &bm = *object.sculpt->bm;
  const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  if (offset == -1) {
    BLI_assert_unreachable();
    return;
  }

  undo::push_nodes(depsgraph, object, node_mask, undo::Type::Mask);
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(&nodes[i])) {
      if (!BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
        BM_ELEM_CD_SET_FLOAT(vert, offset, 1.0f - BM_ELEM_CD_GET_FLOAT(vert, offset));
      }
    }
    bke::pbvh::node_update_mask_bmesh(offset, nodes[i]);
  });
  pbvh.tag_masks_changed(node_mask);
}

static void invert_mask(Main &bmain, const Scene &scene, Depsgraph &depsgraph, Object &object)
{
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(*bke::object::pbvh_get(object), memory);
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      write_mask_mesh(
          depsgraph, object, node_mask, [&](MutableSpan<float> mask, const Span<int> verts) {
            for (const int vert : verts) {
              mask[vert] = 1.0f - mask[vert];
            }
          });
      break;
    case bke::pbvh::Type::Grids:
      invert_mask_grids(bmain, scene, depsgraph, object, node_mask);
      break;
    case bke::pbvh::Type::BMesh:
      invert_mask_bmesh(depsgraph, object, node_mask);
      break;
  }
}

static int mask_flood_fill_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  Depsgraph &depsgraph = *CTX_data_ensure_evaluated_depsgraph(C);

  const FloodFillMode mode = FloodFillMode(RNA_enum_get(op->ptr, "mode"));
  const float value = RNA_float_get(op->ptr, "value");

  BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);

  undo::push_begin(scene, object, op);
  switch (mode) {
    case FloodFillMode::Value:
      fill_mask(bmain, scene, depsgraph, object, value);
      break;
    case FloodFillMode::InverseValue:
      fill_mask(bmain, scene, depsgraph, object, 1.0f - value);
      break;
    case FloodFillMode::InverseMeshValue:
      invert_mask(bmain, scene, depsgraph, object);
      break;
  }

  undo::push_end(object);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void PAINT_OT_mask_flood_fill(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Mask Flood Fill";
  ot->idname = "PAINT_OT_mask_flood_fill";
  ot->description = "Fill the whole mask with a given value, or invert its values";

  /* API callbacks. */
  ot->exec = mask_flood_fill_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* RNA. */
  RNA_def_enum(ot->srna, "mode", mode_items, int(FloodFillMode::Value), "Mode", nullptr);
  RNA_def_float(
      ot->srna,
      "value",
      0.0f,
      0.0f,
      1.0f,
      "Value",
      "Mask level to use when mode is 'Value'; zero means no masking and one is fully masked",
      0.0f,
      1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gesture-based Mask Operators
 * Operators that act upon a user-selected area.
 * \{ */

struct MaskOperation {
  gesture::Operation op;

  FloodFillMode mode;
  float value;
};

static void gesture_begin(bContext &C, wmOperator &op, gesture::GestureData &gesture_data)
{
  const Scene &scene = *CTX_data_scene(&C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  BKE_sculpt_update_object_for_edit(depsgraph, gesture_data.vc.obact, false);
  undo::push_begin(scene, *gesture_data.vc.obact, &op);
}

static float mask_gesture_get_new_value(const float elem, FloodFillMode mode, float value)
{
  switch (mode) {
    case FloodFillMode::Value:
      return value;
    case FloodFillMode::InverseValue:
      return 1.0f - value;
    case FloodFillMode::InverseMeshValue:
      return 1.0f - elem;
  }
  BLI_assert_unreachable();
  return 0.0f;
}

static void gesture_apply_for_symmetry_pass(bContext & /*C*/, gesture::GestureData &gesture_data)
{
  const IndexMask &node_mask = gesture_data.node_mask;
  const MaskOperation &op = *reinterpret_cast<const MaskOperation *>(gesture_data.operation);
  Object &object = *gesture_data.vc.obact;
  const Depsgraph &depsgraph = *gesture_data.vc.depsgraph;
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);
      const Span<float3> normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      update_mask_mesh(
          depsgraph, object, node_mask, [&](MutableSpan<float> node_mask, const Span<int> verts) {
            for (const int i : verts.index_range()) {
              const int vert = verts[i];
              if (gesture::is_affected(gesture_data, positions[vert], normals[vert])) {
                node_mask[i] = mask_gesture_get_new_value(node_mask[i], op.mode, op.value);
              }
            }
          });
      break;
    }
    case bke::pbvh::Type::Grids: {
      bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      SubdivCCG &subdiv_ccg = *gesture_data.ss->subdiv_ccg;
      const Span<float3> positions = subdiv_ccg.positions;
      const Span<float3> normals = subdiv_ccg.normals;
      MutableSpan<float> masks = subdiv_ccg.masks;
      const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

      Array<bool> node_changed(node_mask.min_array_size(), false);

      node_mask.foreach_index(GrainSize(1), [&](const int node_index) {
        bke::pbvh::GridsNode &node = nodes[node_index];
        bool any_changed = false;
        for (const int grid : node.grids()) {
          const int vert_start = grid * key.grid_area;
          BKE_subdiv_ccg_foreach_visible_grid_vert(key, grid_hidden, grid, [&](const int i) {
            const int vert = vert_start + i;
            if (gesture::is_affected(gesture_data, positions[vert], normals[vert])) {
              float &mask = masks[vert];
              if (!any_changed) {
                any_changed = true;
                undo::push_node(depsgraph, object, &node, undo::Type::Mask);
              }
              mask = mask_gesture_get_new_value(mask, op.mode, op.value);
            }
          });
          if (any_changed) {
            bke::pbvh::node_update_mask_grids(key, masks, node);
            node_changed[node_index] = true;
          }
        }
      });

      IndexMaskMemory memory;
      pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
      break;
    }
    case bke::pbvh::Type::BMesh: {
      bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      BMesh &bm = *gesture_data.ss->bm;
      const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");

      Array<bool> node_changed(node_mask.min_array_size(), false);

      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        bool any_changed = false;
        for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(&nodes[i])) {
          if (gesture::is_affected(gesture_data, vert->co, vert->no)) {
            const float old_mask = BM_ELEM_CD_GET_FLOAT(vert, offset);
            if (!any_changed) {
              any_changed = true;
              undo::push_node(depsgraph, object, &nodes[i], undo::Type::Mask);
            }
            const float new_mask = mask_gesture_get_new_value(old_mask, op.mode, op.value);
            BM_ELEM_CD_SET_FLOAT(vert, offset, new_mask);
          }
        }
        if (any_changed) {
          bke::pbvh::node_update_mask_bmesh(offset, nodes[i]);
          node_changed[i] = true;
        }
      });

      IndexMaskMemory memory;
      pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
      break;
    }
  }
}

static void gesture_end(bContext &C, gesture::GestureData &gesture_data)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  Object &object = *gesture_data.vc.obact;
  if (bke::object::pbvh_get(object)->type() == bke::pbvh::Type::Grids) {
    multires_mark_as_modified(depsgraph, &object, MULTIRES_COORDS_MODIFIED);
  }
  undo::push_end(object);
}

static void init_operation(bContext &C, gesture::GestureData &gesture_data, wmOperator &op)
{
  gesture_data.operation = reinterpret_cast<gesture::Operation *>(
      MEM_cnew<MaskOperation>(__func__));

  MaskOperation *mask_operation = (MaskOperation *)gesture_data.operation;

  Object *object = gesture_data.vc.obact;
  MultiresModifierData *mmd = BKE_sculpt_multires_active(gesture_data.vc.scene, object);
  BKE_sculpt_mask_layers_ensure(
      CTX_data_depsgraph_pointer(&C), CTX_data_main(&C), gesture_data.vc.obact, mmd);

  mask_operation->op.begin = gesture_begin;
  mask_operation->op.apply_for_symmetry_pass = gesture_apply_for_symmetry_pass;
  mask_operation->op.end = gesture_end;

  mask_operation->mode = FloodFillMode(RNA_enum_get(op.ptr, "mode"));
  mask_operation->value = RNA_float_get(op.ptr, "value");
}

static void gesture_operator_properties(wmOperatorType *ot)
{
  RNA_def_enum(ot->srna, "mode", mode_items, int(FloodFillMode::Value), "Mode", nullptr);
  RNA_def_float(
      ot->srna,
      "value",
      1.0f,
      0.0f,
      1.0f,
      "Value",
      "Mask level to use when mode is 'Value'; zero means no masking and one is fully masked",
      0.0f,
      1.0f);
}

static int gesture_box_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_box(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*C, *gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_lasso_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_lasso(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*C, *gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_line_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_line(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*C, *gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_polyline_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_polyline(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*C, *gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

void PAINT_OT_mask_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Lasso Gesture";
  ot->idname = "PAINT_OT_mask_lasso_gesture";
  ot->description = "Mask within a shape defined by the cursor";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = gesture_lasso_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  WM_operator_properties_gesture_lasso(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Lasso);

  gesture_operator_properties(ot);
}

void PAINT_OT_mask_box_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Box Gesture";
  ot->idname = "PAINT_OT_mask_box_gesture";
  ot->description = "Mask within a rectangle defined by the cursor";

  ot->invoke = WM_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = gesture_box_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  WM_operator_properties_border(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Box);

  gesture_operator_properties(ot);
}

void PAINT_OT_mask_line_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Line Gesture";
  ot->idname = "PAINT_OT_mask_line_gesture";
  ot->description = "Mask to one side of a line defined by the cursor";

  ot->invoke = WM_gesture_straightline_active_side_invoke;
  ot->modal = WM_gesture_straightline_oneshot_modal;
  ot->exec = gesture_line_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
  gesture::operator_properties(ot, gesture::ShapeType::Line);

  gesture_operator_properties(ot);
}

void PAINT_OT_mask_polyline_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Polyline Gesture";
  ot->idname = "PAINT_OT_mask_polyline_gesture";
  ot->description = "Mask within a shape defined by the cursor";

  ot->invoke = WM_gesture_polyline_invoke;
  ot->modal = WM_gesture_polyline_modal;
  ot->exec = gesture_polyline_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  WM_operator_properties_gesture_polyline(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Lasso);

  gesture_operator_properties(ot);
}

/** \} */

}  // namespace blender::ed::sculpt_paint::mask
