/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "sculpt_face_set.hh"

#include <cmath>
#include <cstdlib>
#include <queue>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_bit_vector.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_function_ref.hh"
#include "BLI_hash.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_fair.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_sculpt.hh"

#include "mesh_brush_common.hh"
#include "paint_hide.hh"
#include "sculpt_automask.hh"
#include "sculpt_boundary.hh"
#include "sculpt_gesture.hh"
#include "sculpt_intern.hh"
#include "sculpt_islands.hh"
#include "sculpt_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::face_set {

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

int find_next_available_id(Object &object)
{
  SculptSession &ss = *object.sculpt;
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
    case bke::pbvh::Type::Grids: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan<int> face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                                bke::AttrDomain::Face);
      const int max = threading::parallel_reduce(
          face_sets.index_range(),
          4096,
          1,
          [&](const IndexRange range, int max) {
            for (const int id : face_sets.slice(range)) {
              max = std::max(max, id);
            }
            return max;
          },
          [](const int a, const int b) { return std::max(a, b); });
      return max + 1;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      const int cd_offset = CustomData_get_offset_named(
          &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
      if (cd_offset == -1) {
        return 1;
      }
      int next_face_set = 1;
      BMIter iter;
      BMFace *f;
      BM_ITER_MESH (f, &iter, &bm, BM_FACES_OF_MESH) {
        const int fset = *static_cast<const int *>(POINTER_OFFSET(f->head.data, cd_offset));
        next_face_set = std::max(next_face_set, fset);
      }

      return next_face_set + 1;
    }
  }
  BLI_assert_unreachable();
  return 0;
}

void initialize_none_to_id(Mesh *mesh, const int new_id)
{
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<int> face_sets = attributes.lookup_for_write_span<int>(
      ".sculpt_face_set");
  if (!face_sets) {
    return;
  }

  for (const int i : face_sets.span.index_range()) {
    if (face_sets.span[i] == SCULPT_FACE_SET_NONE) {
      face_sets.span[i] = new_id;
    }
  }
  face_sets.finish();
}

int active_update_and_get(bContext *C, Object &ob, const float mval[2])
{
  if (!ob.sculpt) {
    return SCULPT_FACE_SET_NONE;
  }

  SculptCursorGeometryInfo gi;
  if (!SCULPT_cursor_geometry_info_update(C, &gi, mval, false)) {
    return SCULPT_FACE_SET_NONE;
  }

  return active_face_set_get(ob);
}

bool create_face_sets_mesh(Object &object)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (attributes.contains(".sculpt_face_set")) {
    return false;
  }
  attributes.add<int>(".sculpt_face_set",
                      bke::AttrDomain::Face,
                      bke::AttributeInitVArray(VArray<int>::ForSingle(1, mesh.faces_num)));
  mesh.face_sets_color_default = 1;
  return true;
}

bke::SpanAttributeWriter<int> ensure_face_sets_mesh(Mesh &mesh)
{
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (!attributes.contains(".sculpt_face_set")) {
    attributes.add<int>(".sculpt_face_set",
                        bke::AttrDomain::Face,
                        bke::AttributeInitVArray(VArray<int>::ForSingle(1, mesh.faces_num)));
    mesh.face_sets_color_default = 1;
  }
  return attributes.lookup_or_add_for_write_span<int>(".sculpt_face_set", bke::AttrDomain::Face);
}

int ensure_face_sets_bmesh(Object &object)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  SculptSession &ss = *object.sculpt;
  BMesh &bm = *ss.bm;
  if (!CustomData_has_layer_named(&bm.pdata, CD_PROP_INT32, ".sculpt_face_set")) {
    BM_data_layer_add_named(&bm, &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
    const int offset = CustomData_get_offset_named(&bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
    if (offset == -1) {
      return -1;
    }
    BMIter iter;
    BMFace *face;
    BM_ITER_MESH (face, &iter, &bm, BM_FACES_OF_MESH) {
      BM_ELEM_CD_SET_INT(face, offset, 1);
    }
    mesh.face_sets_color_default = 1;
    return offset;
  }
  return CustomData_get_offset_named(&bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
}

Array<int> duplicate_face_sets(const Mesh &mesh)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArray<int> attribute = *attributes.lookup_or_default(
      ".sculpt_face_set", bke::AttrDomain::Face, 0);
  Array<int> face_sets(attribute.size());
  array_utils::copy(attribute, face_sets.as_mutable_span());
  return face_sets;
}

void filter_verts_with_unique_face_sets_mesh(const GroupedSpan<int> vert_to_face_map,
                                             const Span<int> face_sets,
                                             const bool unique,
                                             const Span<int> verts,
                                             const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  for (const int i : verts.index_range()) {
    if (unique == face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, verts[i])) {
      factors[i] = 0.0f;
    }
  }
}

void filter_verts_with_unique_face_sets_grids(const OffsetIndices<int> faces,
                                              const Span<int> corner_verts,
                                              const GroupedSpan<int> vert_to_face_map,
                                              const Span<int> face_sets,
                                              const SubdivCCG &subdiv_ccg,
                                              const bool unique,
                                              const Span<int> grids,
                                              const MutableSpan<float> factors)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  BLI_assert(grids.size() * key.grid_area == factors.size());

  for (const int i : grids.index_range()) {
    const int node_start = i * key.grid_area;
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert = node_start + offset;
        if (factors[node_vert] == 0.0f) {
          continue;
        }

        SubdivCCGCoord coord{};
        coord.grid_index = grids[i];
        coord.x = x;
        coord.y = y;
        if (unique == face_set::vert_has_unique_face_set(
                          faces, corner_verts, vert_to_face_map, face_sets, subdiv_ccg, coord))
        {
          factors[node_vert] = 0.0f;
        }
      }
    }
  }
}

void filter_verts_with_unique_face_sets_bmesh(int face_set_offset,
                                              const bool unique,
                                              const Set<BMVert *, 0> &verts,
                                              const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  int i = 0;
  for (const BMVert *vert : verts) {
    if (unique == face_set::vert_has_unique_face_set(face_set_offset, *vert)) {
      factors[i] = 0.0f;
    }
    i++;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Global Mesh Operators
 * Operators that work on the mesh as a whole.
 * \{ */

static void face_sets_update(const Depsgraph &depsgraph,
                             Object &object,
                             const IndexMask &node_mask,
                             const FunctionRef<void(Span<int>, MutableSpan<int>)> calc_face_sets)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  bke::SpanAttributeWriter<int> face_sets = ensure_face_sets_mesh(
      *static_cast<Mesh *>(object.data));

  struct TLS {
    Vector<int> face_indices;
    Vector<int> new_face_sets;
  };

  Array<bool> node_changed(pbvh.nodes_num(), false);

  threading::EnumerableThreadSpecific<TLS> all_tls;
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      TLS &tls = all_tls.local();
      const Span<int> faces = nodes[i].faces();

      tls.new_face_sets.resize(faces.size());
      MutableSpan<int> new_face_sets = tls.new_face_sets;
      gather_data_mesh(face_sets.span.as_span(), faces, new_face_sets);
      calc_face_sets(faces, new_face_sets);
      if (array_utils::indexed_data_equal<int>(face_sets.span, faces, new_face_sets)) {
        return;
      }

      undo::push_node(depsgraph, object, &nodes[i], undo::Type::FaceSet);
      scatter_data_mesh(new_face_sets.as_span(), faces, face_sets.span);
      node_changed[i] = true;
    });
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      TLS &tls = all_tls.local();
      const Span<int> faces = bke::pbvh::node_face_indices_calc_grids(
          *ss.subdiv_ccg, nodes[i], tls.face_indices);

      tls.new_face_sets.resize(faces.size());
      MutableSpan<int> new_face_sets = tls.new_face_sets;
      gather_data_mesh(face_sets.span.as_span(), faces, new_face_sets);
      calc_face_sets(faces, new_face_sets);
      if (array_utils::indexed_data_equal<int>(face_sets.span, faces, new_face_sets)) {
        return;
      }

      undo::push_node(depsgraph, object, &nodes[i], undo::Type::FaceSet);
      scatter_data_mesh(new_face_sets.as_span(), faces, face_sets.span);
      node_changed[i] = true;
    });
  }

  IndexMaskMemory memory;
  pbvh.tag_face_sets_changed(IndexMask::from_bools(node_changed, memory));
  face_sets.finish();
}

enum class CreateMode {
  Masked = 0,
  Visible = 1,
  All = 2,
  Selection = 3,
};

static void clear_face_sets(const Depsgraph &depsgraph, Object &object, const IndexMask &node_mask)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (!attributes.contains(".sculpt_face_set")) {
    return;
  }
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  Array<bool> node_changed(pbvh.nodes_num(), false);

  const int default_face_set = mesh.face_sets_color_default;
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      const Span<int> faces = nodes[i].faces();
      if (std::any_of(faces.begin(), faces.end(), [&](const int face) {
            return face_sets[face] != default_face_set;
          }))
      {
        undo::push_node(depsgraph, object, &nodes[i], undo::Type::FaceSet);
        node_changed[i] = true;
      }
    });
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    threading::EnumerableThreadSpecific<Vector<int>> all_face_indices;
    MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      Vector<int> &face_indices = all_face_indices.local();
      const Span<int> faces = bke::pbvh::node_face_indices_calc_grids(
          *ss.subdiv_ccg, nodes[i], face_indices);
      if (std::any_of(faces.begin(), faces.end(), [&](const int face) {
            return face_sets[face] != default_face_set;
          }))
      {
        undo::push_node(depsgraph, object, &nodes[i], undo::Type::FaceSet);
        node_changed[i] = true;
      }
    });
  }
  IndexMaskMemory memory;
  pbvh.tag_face_sets_changed(IndexMask::from_bools(node_changed, memory));
  attributes.remove(".sculpt_face_set");
}

static int create_op_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  const CreateMode mode = CreateMode(RNA_enum_get(op->ptr, "mode"));

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  if (pbvh.type() == bke::pbvh::Type::BMesh) {
    /* Dyntopo not supported. */
    return OPERATOR_CANCELLED;
  }

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const bke::AttributeAccessor attributes = mesh.attributes();

  BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);

  undo::push_begin(scene, object, op);

  const int next_face_set = find_next_available_id(object);

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  switch (mode) {
    case CreateMode::Masked: {
      if (pbvh.type() == bke::pbvh::Type::Mesh) {
        const OffsetIndices faces = mesh.faces();
        const Span<int> corner_verts = mesh.corner_verts();
        const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly",
                                                                    bke::AttrDomain::Face);
        const VArraySpan<float> mask = *attributes.lookup<float>(".sculpt_mask",
                                                                 bke::AttrDomain::Point);
        if (!mask.is_empty()) {
          face_sets_update(depsgraph,
                           object,
                           node_mask,
                           [&](const Span<int> indices, MutableSpan<int> face_sets) {
                             for (const int i : indices.index_range()) {
                               if (!hide_poly.is_empty() && hide_poly[indices[i]]) {
                                 continue;
                               }
                               const Span<int> face_verts = corner_verts.slice(faces[indices[i]]);
                               if (!std::any_of(face_verts.begin(),
                                                face_verts.end(),
                                                [&](const int vert) { return mask[vert] > 0.5f; }))
                               {
                                 continue;
                               }
                               face_sets[i] = next_face_set;
                             }
                           });
        }
      }
      else if (pbvh.type() == bke::pbvh::Type::Grids) {
        const OffsetIndices<int> faces = mesh.faces();
        const SculptSession &ss = *object.sculpt;
        const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        const int grid_area = subdiv_ccg.grid_area;
        const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly",
                                                                    bke::AttrDomain::Face);
        const Span<float> masks = subdiv_ccg.masks;
        if (!masks.is_empty()) {
          face_sets_update(depsgraph,
                           object,
                           node_mask,
                           [&](const Span<int> indices, MutableSpan<int> face_sets) {
                             for (const int i : indices.index_range()) {
                               if (!hide_poly.is_empty() && hide_poly[indices[i]]) {
                                 continue;
                               }

                               const Span<float> face_masks = masks.slice(
                                   bke::ccg::face_range(faces, grid_area, indices[i]));
                               if (!std::any_of(face_masks.begin(),
                                                face_masks.end(),
                                                [&](const float mask) { return mask > 0.5f; }))
                               {
                                 continue;
                               }
                               face_sets[i] = next_face_set;
                             }
                           });
        }
      }
      break;
    }
    case CreateMode::Visible: {
      const VArray<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
      switch (array_utils::booleans_mix_calc(hide_poly)) {
        case array_utils::BooleanMix::None:
        case array_utils::BooleanMix::AllTrue:
        case array_utils::BooleanMix::AllFalse:
          /* If all vertices in the sculpt are visible, remove face sets and update the default
           * color. This way the new face set will be white, and it is a quick way of disabling all
           * face sets and the performance hit of rendering the overlay. */
          clear_face_sets(depsgraph, object, node_mask);
          break;
        case array_utils::BooleanMix::Mixed:
          const VArraySpan<bool> hide_poly_span(hide_poly);
          face_sets_update(depsgraph,
                           object,
                           node_mask,
                           [&](const Span<int> indices, MutableSpan<int> face_sets) {
                             for (const int i : indices.index_range()) {
                               if (!hide_poly_span[indices[i]]) {
                                 face_sets[i] = next_face_set;
                               }
                             }
                           });
          break;
      }
      break;
    }
    case CreateMode::All: {
      face_sets_update(depsgraph,
                       object,
                       node_mask,
                       [&](const Span<int> /*indices*/, MutableSpan<int> face_sets) {
                         face_sets.fill(next_face_set);
                       });
      break;
    }
    case CreateMode::Selection: {
      const VArraySpan<bool> select_poly = *attributes.lookup_or_default<bool>(
          ".select_poly", bke::AttrDomain::Face, false);
      const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly",
                                                                  bke::AttrDomain::Face);

      face_sets_update(
          depsgraph, object, node_mask, [&](const Span<int> indices, MutableSpan<int> face_sets) {
            for (const int i : indices.index_range()) {
              if (select_poly[indices[i]]) {
                if (!hide_poly.is_empty() && hide_poly[i]) {
                  continue;
                }
                face_sets[i] = next_face_set;
              }
            }
          });

      break;
    }
  }

  undo::push_end(object);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_create(wmOperatorType *ot)
{
  ot->name = "Create Face Set";
  ot->idname = "SCULPT_OT_face_sets_create";
  ot->description = "Create a new Face Set";

  ot->exec = create_op_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  static EnumPropertyItem modes[] = {
      {int(CreateMode::Masked),
       "MASKED",
       0,
       "Face Set from Masked",
       "Create a new Face Set from the masked faces"},
      {int(CreateMode::Visible),
       "VISIBLE",
       0,
       "Face Set from Visible",
       "Create a new Face Set from the visible vertices"},
      {int(CreateMode::All),
       "ALL",
       0,
       "Face Set Full Mesh",
       "Create an unique Face Set with all faces in the sculpt"},
      {int(CreateMode::Selection),
       "SELECTION",
       0,
       "Face Set from Edit Mode Selection",
       "Create an Face Set corresponding to the Edit Mode face selection"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_enum(ot->srna, "mode", modes, int(CreateMode::Masked), "Mode", "");
}

enum class InitMode {
  LooseParts = 0,
  Materials = 1,
  Normals = 2,
  UVSeams = 3,
  Creases = 4,
  SharpEdges = 5,
  BevelWeight = 6,
  FaceSetBoundaries = 8,
};

using FaceSetsFloodFillFn = FunctionRef<bool(int from_face, int edge, int to_face)>;

static void init_flood_fill(Object &ob, const FaceSetsFloodFillFn &test_fn)
{
  SculptSession &ss = *ob.sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob.data);

  BitVector<> visited_faces(mesh->faces_num, false);

  bke::SpanAttributeWriter<int> face_sets = ensure_face_sets_mesh(*mesh);

  const Span<int2> edges = mesh->edges();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_edges = mesh->corner_edges();

  if (ss.edge_to_face_map.is_empty()) {
    ss.edge_to_face_map = bke::mesh::build_edge_to_face_map(
        faces, corner_edges, edges.size(), ss.edge_to_face_offsets, ss.edge_to_face_indices);
  }

  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const Set<int> hidden_face_sets = gather_hidden_face_sets(hide_poly, face_sets.span);

  int next_face_set = 1;

  for (const int i : faces.index_range()) {
    if (!hide_poly.is_empty() && hide_poly[i]) {
      continue;
    }
    if (visited_faces[i]) {
      continue;
    }
    std::queue<int> queue;

    while (hidden_face_sets.contains(next_face_set)) {
      next_face_set += 1;
    }
    face_sets.span[i] = next_face_set;
    visited_faces[i].set(true);
    queue.push(i);

    while (!queue.empty()) {
      const int face_i = queue.front();
      queue.pop();

      for (const int edge_i : corner_edges.slice(faces[face_i])) {
        for (const int neighbor_i : ss.edge_to_face_map[edge_i]) {
          if (neighbor_i == face_i) {
            continue;
          }
          if (visited_faces[neighbor_i]) {
            continue;
          }
          if (!hide_poly.is_empty() && hide_poly[neighbor_i]) {
            continue;
          }
          if (!test_fn(face_i, edge_i, neighbor_i)) {
            continue;
          }

          face_sets.span[neighbor_i] = next_face_set;
          visited_faces[neighbor_i].set(true);
          queue.push(neighbor_i);
        }
      }
    }

    next_face_set += 1;
  }

  face_sets.finish();
}

Set<int> gather_hidden_face_sets(const Span<bool> hide_poly, const Span<int> face_sets)
{
  if (hide_poly.is_empty()) {
    return {};
  }

  Set<int> hidden_face_sets;
  for (const int i : hide_poly.index_range()) {
    if (hide_poly[i]) {
      hidden_face_sets.add(face_sets[i]);
    }
  }

  return hidden_face_sets;
}

static int init_op_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const InitMode mode = InitMode(RNA_enum_get(op->ptr, "mode"));

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  /* Dyntopo not supported. */
  if (pbvh.type() == bke::pbvh::Type::BMesh) {
    return OPERATOR_CANCELLED;
  }

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  if (node_mask.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  undo::push_begin(scene, ob, op);
  undo::push_nodes(*depsgraph, ob, node_mask, undo::Type::FaceSet);

  const float threshold = RNA_float_get(op->ptr, "threshold");

  Mesh *mesh = static_cast<Mesh *>(ob.data);
  const bke::AttributeAccessor attributes = mesh->attributes();

  switch (mode) {
    case InitMode::LooseParts: {
      const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
          ".hide_poly", bke::AttrDomain::Face, false);
      init_flood_fill(ob, [&](const int from_face, const int /*edge*/, const int to_face) {
        return hide_poly[from_face] == hide_poly[to_face];
      });
      break;
    }
    case InitMode::Materials: {
      bke::SpanAttributeWriter<int> face_sets = ensure_face_sets_mesh(*mesh);
      const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
          "material_index", bke::AttrDomain::Face, 0);
      const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly",
                                                                  bke::AttrDomain::Face);
      for (const int i : IndexRange(mesh->faces_num)) {
        if (!hide_poly.is_empty() && hide_poly[i]) {
          continue;
        }

        /* In some cases material face set index could be same as hidden face set index
         * A more robust implementation is needed to avoid this */
        face_sets.span[i] = material_indices[i] + 1;
      }

      face_sets.finish();
      break;
    }
    case InitMode::Normals: {
      const Span<float3> face_normals = mesh->face_normals();
      init_flood_fill(ob, [&](const int from_face, const int /*edge*/, const int to_face) -> bool {
        return std::abs(math::dot(face_normals[from_face], face_normals[to_face])) > threshold;
      });
      break;
    }
    case InitMode::UVSeams: {
      const VArraySpan<bool> uv_seams = *mesh->attributes().lookup_or_default<bool>(
          "uv_seam", bke::AttrDomain::Edge, false);
      init_flood_fill(ob,
                      [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
                        return !uv_seams[edge];
                      });
      break;
    }
    case InitMode::Creases: {
      const VArraySpan<float> creases = *attributes.lookup_or_default<float>(
          "crease_edge", bke::AttrDomain::Edge, 0.0f);
      init_flood_fill(ob,
                      [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
                        return creases[edge] < threshold;
                      });
      break;
    }
    case InitMode::SharpEdges: {
      const VArraySpan<bool> sharp_edges = *mesh->attributes().lookup_or_default<bool>(
          "sharp_edge", bke::AttrDomain::Edge, false);
      init_flood_fill(ob,
                      [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
                        return !sharp_edges[edge];
                      });
      break;
    }
    case InitMode::BevelWeight: {
      const VArraySpan<float> bevel_weights = *attributes.lookup_or_default<float>(
          "bevel_weight_edge", bke::AttrDomain::Edge, 0.0f);
      init_flood_fill(ob,
                      [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
                        return bevel_weights[edge] < threshold;
                      });
      break;
    }
    case InitMode::FaceSetBoundaries: {
      Array<int> face_sets_copy = duplicate_face_sets(*mesh);
      init_flood_fill(ob, [&](const int from_face, const int /*edge*/, const int to_face) -> bool {
        return face_sets_copy[from_face] == face_sets_copy[to_face];
      });
      break;
    }
  }

  undo::push_end(ob);

  pbvh.tag_face_sets_changed(node_mask);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_init(wmOperatorType *ot)
{
  ot->name = "Init Face Sets";
  ot->idname = "SCULPT_OT_face_sets_init";
  ot->description = "Initializes all Face Sets in the mesh";

  ot->exec = init_op_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  static EnumPropertyItem modes[] = {
      {int(InitMode::LooseParts),
       "LOOSE_PARTS",
       0,
       "Face Sets from Loose Parts",
       "Create a Face Set per loose part in the mesh"},
      {int(InitMode::Materials),
       "MATERIALS",
       0,
       "Face Sets from Material Slots",
       "Create a Face Set per Material Slot"},
      {int(InitMode::Normals),
       "NORMALS",
       0,
       "Face Sets from Mesh Normals",
       "Create Face Sets for Faces that have similar normal"},
      {int(InitMode::UVSeams),
       "UV_SEAMS",
       0,
       "Face Sets from UV Seams",
       "Create Face Sets using UV Seams as boundaries"},
      {int(InitMode::Creases),
       "CREASES",
       0,
       "Face Sets from Edge Creases",
       "Create Face Sets using Edge Creases as boundaries"},
      {int(InitMode::BevelWeight),
       "BEVEL_WEIGHT",
       0,
       "Face Sets from Bevel Weight",
       "Create Face Sets using Bevel Weights as boundaries"},
      {int(InitMode::SharpEdges),
       "SHARP_EDGES",
       0,
       "Face Sets from Sharp Edges",
       "Create Face Sets using Sharp Edges as boundaries"},
      {int(InitMode::FaceSetBoundaries),
       "FACE_SET_BOUNDARIES",
       0,
       "Face Sets from Face Set Boundaries",
       "Create a Face Set per isolated Face Set"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_enum(ot->srna, "mode", modes, int(InitMode::LooseParts), "Mode", "");
  RNA_def_float(
      ot->srna,
      "threshold",
      0.5f,
      0.0f,
      1.0f,
      "Threshold",
      "Minimum value to consider a certain attribute a boundary when creating the Face Sets",
      0.0f,
      1.0f);
}

enum class VisibilityMode {
  Toggle = 0,
  ShowActive = 1,
  HideActive = 2,
};

static void face_hide_update(const Depsgraph &depsgraph,
                             Object &object,
                             const IndexMask &node_mask,
                             const FunctionRef<void(Span<int>, MutableSpan<bool>)> calc_hide)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_poly", bke::AttrDomain::Face);

  struct TLS {
    Vector<int> face_indices;
    Vector<bool> new_hide;
  };

  Array<bool> node_changed(node_mask.min_array_size(), false);

  threading::EnumerableThreadSpecific<TLS> all_tls;
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      TLS &tls = all_tls.local();
      const Span<int> faces = nodes[i].faces();

      tls.new_hide.resize(faces.size());
      MutableSpan<bool> new_hide = tls.new_hide;
      gather_data_mesh(hide_poly.span.as_span(), faces, new_hide);
      calc_hide(faces, new_hide);
      if (array_utils::indexed_data_equal<bool>(hide_poly.span, faces, new_hide)) {
        return;
      }

      undo::push_node(depsgraph, object, &nodes[i], undo::Type::HideFace);
      scatter_data_mesh(new_hide.as_span(), faces, hide_poly.span);
      node_changed[i] = true;
    });
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      TLS &tls = all_tls.local();
      const Span<int> faces = bke::pbvh::node_face_indices_calc_grids(
          *ss.subdiv_ccg, nodes[i], tls.face_indices);

      tls.new_hide.resize(faces.size());
      MutableSpan<bool> new_hide = tls.new_hide;
      gather_data_mesh(hide_poly.span.as_span(), faces, new_hide);
      calc_hide(faces, new_hide);
      if (array_utils::indexed_data_equal<bool>(hide_poly.span, faces, new_hide)) {
        return;
      }

      undo::push_node(depsgraph, object, &nodes[i], undo::Type::HideFace);
      scatter_data_mesh(new_hide.as_span(), faces, hide_poly.span);
      node_changed[i] = true;
    });
  }

  hide_poly.finish();

  IndexMaskMemory memory;
  const IndexMask changed_nodes = IndexMask::from_bools(node_changed, memory);
  if (changed_nodes.is_empty()) {
    return;
  }
  hide::sync_all_from_faces(object);
  pbvh.tag_visibility_changed(node_mask);
}

static void show_all(Depsgraph &depsgraph, Object &object, const IndexMask &node_mask)
{
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      hide::mesh_show_all(depsgraph, object, node_mask);
      break;
    case bke::pbvh::Type::Grids:
      hide::grids_show_all(depsgraph, object, node_mask);
      break;
    case bke::pbvh::Type::BMesh:
      BLI_assert_unreachable();
      break;
  }
}

static int change_visibility_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  SculptSession &ss = *object.sculpt;
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  Mesh *mesh = BKE_object_get_original_mesh(&object);
  BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  if (pbvh.type() == bke::pbvh::Type::BMesh) {
    /* Not supported for dyntopo. There is no active face. */
    return OPERATOR_CANCELLED;
  }

  const VisibilityMode mode = VisibilityMode(RNA_enum_get(op->ptr, "mode"));
  const int active_face_set = active_face_set_get(object);

  undo::push_begin(scene, object, op);

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan<int> face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                            bke::AttrDomain::Face);

  switch (mode) {
    case VisibilityMode::Toggle: {
      if (hide_poly.contains(true) || face_sets.is_empty()) {
        show_all(depsgraph, object, node_mask);
      }
      else {
        face_hide_update(
            depsgraph, object, node_mask, [&](const Span<int> faces, MutableSpan<bool> hide) {
              for (const int i : hide.index_range()) {
                hide[i] = face_sets[faces[i]] != active_face_set;
              }
            });
      }
      break;
    }
    case VisibilityMode::ShowActive:
      if (face_sets.is_empty()) {
        show_all(depsgraph, object, node_mask);
      }
      else {
        face_hide_update(
            depsgraph, object, node_mask, [&](const Span<int> faces, MutableSpan<bool> hide) {
              for (const int i : hide.index_range()) {
                if (face_sets[faces[i]] == active_face_set) {
                  hide[i] = false;
                }
              }
            });
      }
      break;
    case VisibilityMode::HideActive:
      if (face_sets.is_empty()) {
        face_hide_update(
            depsgraph, object, node_mask, [&](const Span<int> /*faces*/, MutableSpan<bool> hide) {
              hide.fill(true);
            });
      }
      else {
        face_hide_update(
            depsgraph, object, node_mask, [&](const Span<int> faces, MutableSpan<bool> hide) {
              for (const int i : hide.index_range()) {
                if (face_sets[faces[i]] == active_face_set) {
                  hide[i] = true;
                }
              }
            });
      }
      break;
  }

  /* For modes that use the cursor active vertex, update the rotation origin for viewport
   * navigation. */
  if (ELEM(mode, VisibilityMode::Toggle, VisibilityMode::ShowActive)) {
    UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
    if (std::holds_alternative<std::monostate>(ss.active_vert())) {
      ups->last_stroke_valid = false;
    }
    else {
      float location[3];
      copy_v3_v3(location, ss.active_vert_position(depsgraph, object));
      mul_m4_v3(object.object_to_world().ptr(), location);
      copy_v3_v3(ups->average_stroke_accum, location);
      ups->average_stroke_counter = 1;
      ups->last_stroke_valid = true;
    }
  }

  undo::push_end(object);

  bke::pbvh::update_visibility(object, pbvh);

  islands::invalidate(*object.sculpt);
  hide::tag_update_visibility(*C);

  return OPERATOR_FINISHED;
}

static int change_visibility_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object &ob = *CTX_data_active_object(C);

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  /* Update the active vertex and Face Set using the cursor position to avoid relying on the paint
   * cursor updates. */
  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  SCULPT_vertex_random_access_ensure(ob);
  SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);

  return change_visibility_exec(C, op);
}

void SCULPT_OT_face_set_change_visibility(wmOperatorType *ot)
{
  ot->name = "Face Sets Visibility";
  ot->idname = "SCULPT_OT_face_set_change_visibility";
  ot->description = "Change the visibility of the Face Sets of the sculpt";

  ot->exec = change_visibility_exec;
  ot->invoke = change_visibility_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  static EnumPropertyItem modes[] = {
      {int(VisibilityMode::Toggle),
       "TOGGLE",
       0,
       "Toggle Visibility",
       "Hide all Face Sets except for the active one"},
      {int(VisibilityMode::ShowActive),
       "SHOW_ACTIVE",
       0,
       "Show Active Face Set",
       "Show Active Face Set"},
      {int(VisibilityMode::HideActive),
       "HIDE_ACTIVE",
       0,
       "Hide Active Face Sets",
       "Hide Active Face Sets"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_enum(ot->srna, "mode", modes, int(VisibilityMode::Toggle), "Mode", "");
}

static int randomize_colors_exec(bContext *C, wmOperator * /*op*/)
{
  Object &ob = *CTX_data_active_object(C);

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  /* Dyntopo not supported. */
  if (pbvh.type() == bke::pbvh::Type::BMesh) {
    return OPERATOR_CANCELLED;
  }

  Mesh *mesh = static_cast<Mesh *>(ob.data);
  const bke::AttributeAccessor attributes = mesh->attributes();

  if (!attributes.contains(".sculpt_face_set")) {
    return OPERATOR_CANCELLED;
  }

  const VArray<int> face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  const int random_index = clamp_i(mesh->faces_num * BLI_hash_int_01(mesh->face_sets_color_seed),
                                   0,
                                   max_ii(0, mesh->faces_num - 1));
  mesh->face_sets_color_default = face_sets[random_index];

  mesh->face_sets_color_seed += 1;

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  pbvh.tag_face_sets_changed(node_mask);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_randomize_colors(wmOperatorType *ot)
{
  ot->name = "Randomize Face Sets Colors";
  ot->idname = "SCULPT_OT_face_sets_randomize_colors";
  ot->description = "Generates a new set of random colors to render the Face Sets in the viewport";

  ot->exec = randomize_colors_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

enum class EditMode {
  Grow = 0,
  Shrink = 1,
  DeleteGeometry = 2,
  FairPositions = 3,
  FairTangency = 4,
};

static void edit_grow_shrink(const Depsgraph &depsgraph,
                             const Scene &scene,
                             Object &object,
                             const EditMode mode,
                             const int active_face_set_id,
                             const bool modify_hidden,
                             wmOperator *op)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();

  BLI_assert(attributes.contains(".sculpt_face_set"));

  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  Array<int> prev_face_sets = duplicate_face_sets(mesh);

  undo::push_begin(scene, object, op);

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  face_sets_update(
      depsgraph, object, node_mask, [&](const Span<int> indices, MutableSpan<int> face_sets) {
        for (const int i : indices.index_range()) {
          const int face = indices[i];
          if (!modify_hidden && !hide_poly.is_empty() && hide_poly[face]) {
            continue;
          }
          if (mode == EditMode::Grow) {
            for (const int vert : corner_verts.slice(faces[face])) {
              for (const int neighbor_face_index : vert_to_face_map[vert]) {
                if (neighbor_face_index == face) {
                  continue;
                }
                if (prev_face_sets[neighbor_face_index] == active_face_set_id) {
                  face_sets[i] = active_face_set_id;
                }
              }
            }
          }
          else {
            if (prev_face_sets[face] == active_face_set_id) {
              for (const int vert_i : corner_verts.slice(faces[face])) {
                for (const int neighbor_face_index : vert_to_face_map[vert_i]) {
                  if (neighbor_face_index == face) {
                    continue;
                  }
                  if (prev_face_sets[neighbor_face_index] != active_face_set_id) {
                    face_sets[i] = prev_face_sets[neighbor_face_index];
                  }
                }
              }
            }
          }
        }
      });

  undo::push_end(object);
}

static bool check_single_face_set(const Object &object, const bool check_visible_only)
{
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan<int> face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                            bke::AttrDomain::Face);

  if (face_sets.is_empty()) {
    return true;
  }
  int first_face_set = SCULPT_FACE_SET_NONE;
  if (check_visible_only) {
    for (const int i : face_sets.index_range()) {
      if (!hide_poly.is_empty() && hide_poly[i]) {
        continue;
      }
      first_face_set = face_sets[i];
      break;
    }
  }
  else {
    first_face_set = face_sets[0];
  }

  if (first_face_set == SCULPT_FACE_SET_NONE) {
    return true;
  }

  for (const int i : face_sets.index_range()) {
    if (check_visible_only && !hide_poly.is_empty() && hide_poly[i]) {
      continue;
    }
    if (face_sets[i] != first_face_set) {
      return false;
    }
  }
  return true;
}

static void delete_geometry(Object &ob, const int active_face_set_id, const bool modify_hidden)
{
  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan<int> face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                            bke::AttrDomain::Face);

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(&mesh);
  BMeshCreateParams create_params{};
  create_params.use_toolflags = true;
  BMesh *bm = BM_mesh_create(&allocsize, &create_params);

  BMeshFromMeshParams convert_params{};
  convert_params.calc_vert_normal = true;
  convert_params.calc_face_normal = true;
  BM_mesh_bm_from_me(bm, &mesh, &convert_params);

  BM_mesh_elem_table_init(bm, BM_FACE);
  BM_mesh_elem_table_ensure(bm, BM_FACE);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const int face_index = BM_elem_index_get(f);
    if (!modify_hidden && !hide_poly.is_empty() && hide_poly[face_index]) {
      continue;
    }
    BM_elem_flag_set(f, BM_ELEM_TAG, face_sets[face_index] == active_face_set_id);
  }
  BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_FACES);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BMeshToMeshParams bmesh_to_mesh_params{};
  bmesh_to_mesh_params.calc_object_remap = false;
  BM_mesh_bm_to_me(nullptr, bm, &mesh, &bmesh_to_mesh_params);

  BM_mesh_free(bm);
}

static void edit_fairing(const Depsgraph &depsgraph,
                         const Sculpt &sd,
                         Object &ob,
                         const int active_face_set_id,
                         const eMeshFairingDepth fair_order,
                         const float strength)
{
  SculptSession &ss = *ob.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  boundary::ensure_boundary_info(ob);

  const PositionDeformData position_data(depsgraph, ob);
  const Span<float3> positions = position_data.eval;
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const BitSpan boundary_verts = ss.vertex_info.boundary;
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);

  Array<bool> fair_verts(positions.size(), false);
  for (const int vert : positions.index_range()) {
    if (boundary::vert_is_boundary(vert_to_face_map, hide_poly, boundary_verts, vert)) {
      continue;
    }
    if (!vert_has_face_set(vert_to_face_map, face_sets, vert, active_face_set_id)) {
      continue;
    }
    if (!vert_has_unique_face_set(vert_to_face_map, face_sets, vert)) {
      continue;
    }
    fair_verts[vert] = true;
  }

  Array<float3> new_positions = positions;
  BKE_mesh_prefair_and_fair_verts(&mesh, new_positions, fair_verts.data(), fair_order);

  struct LocalData {
    Vector<float3> translations;
  };

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    const Span<int> verts = nodes[i].verts();
    tls.translations.resize(verts.size());
    const MutableSpan<float3> translations = tls.translations;
    for (const int i : verts.index_range()) {
      translations[i] = new_positions[verts[i]] - positions[verts[i]];
    }
    scale_translations(translations, strength);
    clip_and_lock_translations(sd, ss, positions, verts, translations);
    position_data.deform(translations, verts);
  });
}

static bool edit_is_operation_valid(const Object &object,
                                    const EditMode mode,
                                    const bool modify_hidden)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  if (pbvh.type() == bke::pbvh::Type::BMesh) {
    /* Dyntopo is not supported. */
    return false;
  }

  if (mode == EditMode::DeleteGeometry) {
    if (pbvh.type() == bke::pbvh::Type::Grids) {
      /* Modification of base mesh geometry requires special remapping of multi-resolution
       * displacement, which does not happen here.
       * Disable delete operation. It can be supported in the future by doing similar displacement
       * data remapping as what happens in the mesh edit mode. */
      return false;
    }
    if (check_single_face_set(object, !modify_hidden)) {
      /* Cancel the operator if the mesh only contains one Face Set to avoid deleting the
       * entire object. */
      return false;
    }
  }

  if (ELEM(mode, EditMode::FairPositions, EditMode::FairTangency)) {
    if (pbvh.type() == bke::pbvh::Type::Grids) {
      /* TODO: Multi-resolution topology representation using grids and duplicates can't be used
       * directly by the fair algorithm. Multi-resolution topology needs to be exposed in a
       * different way or converted to a mesh for this operation. */
      return false;
    }
  }

  if (ELEM(mode, EditMode::Grow, EditMode::Shrink)) {
    if (pbvh.type() == bke::pbvh::Type::Mesh) {
      const Mesh &mesh = *static_cast<Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      if (!attributes.contains(".sculpt_face_set")) {
        /* If a mesh does not have the face set attribute, growing or shrinking the face set will
         * have no effect, exit early in this case. */
        return false;
      }
    }
  }

  return true;
}

static void edit_modify_geometry(
    bContext *C, Object &ob, const int active_face_set, const bool modify_hidden, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Mesh *mesh = static_cast<Mesh *>(ob.data);
  undo::geometry_begin(scene, ob, op);
  delete_geometry(ob, active_face_set, modify_hidden);
  undo::geometry_end(ob);
  BKE_sculptsession_free_pbvh(ob);
  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);
}

static void edit_modify_coordinates(
    bContext *C, Object &ob, const int active_face_set, const EditMode mode, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  const float strength = RNA_float_get(op->ptr, "strength");

  undo::push_begin(scene, ob, op);
  undo::push_nodes(depsgraph, ob, node_mask, undo::Type::Position);

  pbvh.tag_positions_changed(node_mask);

  switch (mode) {
    case EditMode::FairPositions:
      edit_fairing(depsgraph, sd, ob, active_face_set, MESH_FAIRING_DEPTH_POSITION, strength);
      break;
    case EditMode::FairTangency:
      edit_fairing(depsgraph, sd, ob, active_face_set, MESH_FAIRING_DEPTH_TANGENCY, strength);
      break;
    default:
      BLI_assert_unreachable();
  }

  bke::pbvh::update_bounds(depsgraph, ob, pbvh);
  flush_update_step(C, UpdateType::Position);
  flush_update_done(C, ob, UpdateType::Position);
  undo::push_end(ob);
}

static bool edit_op_init(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const EditMode mode = EditMode(RNA_enum_get(op->ptr, "mode"));
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  if (!edit_is_operation_valid(*ob, mode, modify_hidden)) {
    return false;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);

  return true;
}

static int edit_op_exec(bContext *C, wmOperator *op)
{
  if (!edit_op_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  const Scene &scene = *CTX_data_scene(C);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);

  const int active_face_set = RNA_int_get(op->ptr, "active_face_set");
  const EditMode mode = EditMode(RNA_enum_get(op->ptr, "mode"));
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  switch (mode) {
    case EditMode::DeleteGeometry:
      edit_modify_geometry(C, ob, active_face_set, modify_hidden, op);
      break;
    case EditMode::Grow:
    case EditMode::Shrink:
      edit_grow_shrink(depsgraph, scene, ob, mode, active_face_set, modify_hidden, op);
      break;
    case EditMode::FairPositions:
    case EditMode::FairTangency:
      edit_modify_coordinates(C, ob, active_face_set, mode, op);
      break;
  }

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

static int edit_op_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  /* Update the current active Face Set and Vertex as the operator can be used directly from the
   * tool without brush cursor. */
  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  if (!SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false)) {
    /* The cursor is not over the mesh. Cancel to avoid editing the last updated Face Set ID. */
    return OPERATOR_CANCELLED;
  }
  RNA_int_set(op->ptr, "active_face_set", active_face_set_get(ob));

  return edit_op_exec(C, op);
}

void SCULPT_OT_face_sets_edit(wmOperatorType *ot)
{
  ot->name = "Edit Face Set";
  ot->idname = "SCULPT_OT_face_set_edit";
  ot->description = "Edits the current active Face Set";

  ot->invoke = edit_op_invoke;
  ot->exec = edit_op_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  PropertyRNA *prop = RNA_def_int(
      ot->srna, "active_face_set", 1, 0, INT_MAX, "Active Face Set", "", 0, 64);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  static EnumPropertyItem modes[] = {
      {int(EditMode::Grow),
       "GROW",
       0,
       "Grow Face Set",
       "Grows the Face Sets boundary by one face based on mesh topology"},
      {int(EditMode::Shrink),
       "SHRINK",
       0,
       "Shrink Face Set",
       "Shrinks the Face Sets boundary by one face based on mesh topology"},
      {int(EditMode::DeleteGeometry),
       "DELETE_GEOMETRY",
       0,
       "Delete Geometry",
       "Deletes the faces that are assigned to the Face Set"},
      {int(EditMode::FairPositions),
       "FAIR_POSITIONS",
       0,
       "Fair Positions",
       "Creates a smooth as possible geometry patch from the Face Set minimizing changes in "
       "vertex positions"},
      {int(EditMode::FairTangency),
       "FAIR_TANGENCY",
       0,
       "Fair Tangency",
       "Creates a smooth as possible geometry patch from the Face Set minimizing changes in "
       "vertex tangents"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_enum(ot->srna, "mode", modes, int(EditMode::Grow), "Mode", "");
  RNA_def_float(ot->srna, "strength", 1.0f, 0.0f, 1.0f, "Strength", "", 0.0f, 1.0f);

  ot->prop = RNA_def_boolean(ot->srna,
                             "modify_hidden",
                             false,
                             "Modify Hidden",
                             "Apply the edit operation to hidden geometry");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gesture Operators
 * Operators that modify face sets based on a selected area.
 * \{ */

struct FaceSetOperation {
  gesture::Operation op;

  int new_face_set_id;
};

static void gesture_begin(bContext &C, wmOperator &op, gesture::GestureData &gesture_data)
{
  const Scene &scene = *CTX_data_scene(&C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  BKE_sculpt_update_object_for_edit(depsgraph, gesture_data.vc.obact, false);
  undo::push_begin(scene, *gesture_data.vc.obact, &op);
}

static void gesture_apply_mesh(gesture::GestureData &gesture_data, const IndexMask &node_mask)
{
  FaceSetOperation *face_set_operation = (FaceSetOperation *)gesture_data.operation;
  const int new_face_set = face_set_operation->new_face_set_id;
  const Depsgraph &depsgraph = *gesture_data.vc.depsgraph;
  Object &object = *gesture_data.vc.obact;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::AttributeAccessor attributes = mesh.attributes();
  SculptSession &ss = *gesture_data.ss;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(mesh);

  struct TLS {
    Vector<int> face_indices;
  };

  Array<bool> node_changed(pbvh.nodes_num(), false);

  threading::EnumerableThreadSpecific<TLS> all_tls;
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      undo::push_node(depsgraph, *gesture_data.vc.obact, &nodes[i], undo::Type::FaceSet);
      bool any_updated = false;
      for (const int face : nodes[i].faces()) {
        if (!hide_poly.is_empty() && hide_poly[face]) {
          continue;
        }
        const Span<int> face_verts = corner_verts.slice(faces[face]);
        const float3 face_center = bke::mesh::face_center_calc(positions, face_verts);
        const float3 face_normal = bke::mesh::face_normal_calc(positions, face_verts);
        if (!gesture::is_affected(gesture_data, face_center, face_normal)) {
          continue;
        }
        face_sets.span[face] = new_face_set;
        any_updated = true;
      }
      if (any_updated) {
        node_changed[i] = true;
      }
    });
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      TLS &tls = all_tls.local();
      undo::push_node(depsgraph, *gesture_data.vc.obact, &nodes[i], undo::Type::FaceSet);
      const Span<int> node_faces = bke::pbvh::node_face_indices_calc_grids(
          *ss.subdiv_ccg, nodes[i], tls.face_indices);

      bool any_updated = false;
      for (const int face : node_faces) {
        if (!hide_poly.is_empty() && hide_poly[face]) {
          continue;
        }
        const Span<int> face_verts = corner_verts.slice(faces[face]);
        const float3 face_center = bke::mesh::face_center_calc(positions, face_verts);
        const float3 face_normal = bke::mesh::face_normal_calc(positions, face_verts);
        if (!gesture::is_affected(gesture_data, face_center, face_normal)) {
          continue;
        }
        face_sets.span[face] = new_face_set;
        any_updated = true;
      }
      if (any_updated) {
        node_changed[i] = true;
      }
    });
  }

  IndexMaskMemory memory;
  pbvh.tag_face_sets_changed(IndexMask::from_bools(node_changed, memory));
  face_sets.finish();
}

static void gesture_apply_bmesh(gesture::GestureData &gesture_data, const IndexMask &node_mask)
{
  FaceSetOperation *face_set_operation = (FaceSetOperation *)gesture_data.operation;
  const Depsgraph &depsgraph = *gesture_data.vc.depsgraph;
  const int new_face_set = face_set_operation->new_face_set_id;
  SculptSession &ss = *gesture_data.ss;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(*gesture_data.vc.obact);
  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  BMesh *bm = ss.bm;
  const int offset = CustomData_get_offset_named(&bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  Array<bool> node_changed(node_mask.min_array_size(), false);

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    undo::push_node(depsgraph, *gesture_data.vc.obact, &nodes[i], undo::Type::FaceSet);

    bool any_updated = false;
    for (BMFace *face : BKE_pbvh_bmesh_node_faces(&nodes[i])) {
      if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
        continue;
      }
      float3 center;
      BM_face_calc_center_median(face, center);
      if (!gesture::is_affected(gesture_data, center, face->no)) {
        continue;
      }
      BM_ELEM_CD_SET_INT(face, offset, new_face_set);
      any_updated = true;
    }

    if (any_updated) {
      node_changed[i] = true;
    }
  });

  IndexMaskMemory memory;
  const IndexMask changed_nodes = IndexMask::from_bools(node_changed, memory);
  if (changed_nodes.is_empty()) {
    return;
  }
  pbvh.tag_face_sets_changed(node_mask);
}

static void gesture_apply_for_symmetry_pass(bContext & /*C*/, gesture::GestureData &gesture_data)
{
  switch (bke::object::pbvh_get(*gesture_data.vc.obact)->type()) {
    case bke::pbvh::Type::Grids:
    case bke::pbvh::Type::Mesh:
      gesture_apply_mesh(gesture_data, gesture_data.node_mask);
      break;
    case bke::pbvh::Type::BMesh:
      gesture_apply_bmesh(gesture_data, gesture_data.node_mask);
      break;
  }
}

static void gesture_end(bContext & /*C*/, gesture::GestureData &gesture_data)
{
  undo::push_end(*gesture_data.vc.obact);
}

static void init_operation(gesture::GestureData &gesture_data, wmOperator & /*op*/)
{
  Object &object = *gesture_data.vc.obact;
  gesture_data.operation = reinterpret_cast<gesture::Operation *>(
      MEM_cnew<FaceSetOperation>(__func__));

  FaceSetOperation *face_set_operation = (FaceSetOperation *)gesture_data.operation;

  face_set_operation->op.begin = gesture_begin;
  face_set_operation->op.apply_for_symmetry_pass = gesture_apply_for_symmetry_pass;
  face_set_operation->op.end = gesture_end;

  face_set_operation->new_face_set_id = face_set::find_next_available_id(object);
}

static int gesture_box_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  return WM_gesture_box_invoke(C, op, event);
}

static int gesture_box_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_box(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_lasso_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  return WM_gesture_lasso_invoke(C, op, event);
}

static int gesture_lasso_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_lasso(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_line_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  return WM_gesture_straightline_active_side_invoke(C, op, event);
}

static int gesture_line_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_line(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_polyline_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  return WM_gesture_polyline_invoke(C, op, event);
}

static int gesture_polyline_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_polyline(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  init_operation(*gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_set_polyline_gesture(wmOperatorType *ot)
{
  ot->name = "Face Set Lasso Gesture";
  ot->idname = "SCULPT_OT_face_set_polyline_gesture";
  ot->description = "Add a face set in a shape defined by the cursor";

  ot->invoke = gesture_polyline_invoke;
  ot->modal = WM_gesture_polyline_modal;
  ot->exec = gesture_polyline_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_DEPENDS_ON_CURSOR;

  WM_operator_properties_gesture_polyline(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Lasso);
}

void SCULPT_OT_face_set_box_gesture(wmOperatorType *ot)
{
  ot->name = "Face Set Box Gesture";
  ot->idname = "SCULPT_OT_face_set_box_gesture";
  ot->description = "Add a face set in a rectangle defined by the cursor";

  ot->invoke = gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = gesture_box_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  WM_operator_properties_border(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Box);
}

void SCULPT_OT_face_set_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Face Set Lasso Gesture";
  ot->idname = "SCULPT_OT_face_set_lasso_gesture";
  ot->description = "Add a face set in a shape defined by the cursor";

  ot->invoke = gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = gesture_lasso_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_DEPENDS_ON_CURSOR;

  WM_operator_properties_gesture_lasso(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Lasso);
}

void SCULPT_OT_face_set_line_gesture(wmOperatorType *ot)
{
  ot->name = "Face Set Line Gesture";
  ot->idname = "SCULPT_OT_face_set_line_gesture";
  ot->description = "Add a face set to one side of a line defined by the cursor";

  ot->invoke = gesture_line_invoke;
  ot->modal = WM_gesture_straightline_oneshot_modal;
  ot->exec = gesture_line_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
  gesture::operator_properties(ot, gesture::ShapeType::Line);
}
/** \} */

}  // namespace blender::ed::sculpt_paint::face_set
