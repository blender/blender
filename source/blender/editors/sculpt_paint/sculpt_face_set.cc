/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

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

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.h"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_fair.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_sculpt.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::face_set {

int find_next_available_id(Object &object)
{
  SculptSession &ss = *object.sculpt;
  switch (BKE_pbvh_type(ss.pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS: {
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
    case PBVH_BMESH: {
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

int active_update_and_get(bContext *C, Object *ob, const float mval[2])
{
  SculptSession *ss = ob->sculpt;
  if (!ss) {
    return SCULPT_FACE_SET_NONE;
  }

  SculptCursorGeometryInfo gi;
  if (!SCULPT_cursor_geometry_info_update(C, &gi, mval, false)) {
    return SCULPT_FACE_SET_NONE;
  }

  return active_face_set_get(ss);
}

bke::SpanAttributeWriter<int> ensure_face_sets_mesh(Object &object)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (!attributes.contains(".sculpt_face_set")) {
    attributes.add<int>(".sculpt_face_set",
                        bke::AttrDomain::Face,
                        bke::AttributeInitVArray(VArray<int>::ForSingle(1, mesh.faces_num)));
    mesh.face_sets_color_default = 1;
  }
  object.sculpt->face_sets = static_cast<const int *>(
      CustomData_get_layer_named(&mesh.face_data, CD_PROP_INT32, ".sculpt_face_set"));
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

/* Draw Face Sets Brush. */

constexpr float FACE_SET_BRUSH_MIN_FADE = 0.05f;

static void do_draw_face_sets_brush_faces(Object *ob,
                                          const Brush *brush,
                                          const Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  const Span<float3> positions = SCULPT_mesh_deformed_positions_get(ss);

  Mesh &mesh = *static_cast<Mesh *>(ob->data);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  bke::SpanAttributeWriter<int> attribute = ensure_face_sets_mesh(*ob);
  MutableSpan<int> face_sets = attribute.span;

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    const float bstrength = ss->cache->bstrength;
    const int thread_id = BLI_task_parallel_thread_id(nullptr);
    for (PBVHNode *node : nodes.slice(range)) {
      SculptBrushTest test;
      SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
          ss, &test, brush->falloff_shape);

      auto_mask::NodeData automask_data = auto_mask::node_begin(
          *ob, ss->cache->automasking.get(), *node);

      bool changed = false;

      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
        auto_mask::node_update(automask_data, vd);

        for (const int face_i : ss->vert_to_face_map[vd.index]) {
          const IndexRange face = ss->faces[face_i];

          const float3 poly_center = bke::mesh::face_center_calc(positions,
                                                                 ss->corner_verts.slice(face));

          if (!sculpt_brush_test_sq_fn(&test, poly_center)) {
            continue;
          }
          if (!hide_poly.is_empty() && hide_poly[face_i]) {
            continue;
          }
          const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                      brush,
                                                                      vd.co,
                                                                      sqrtf(test.dist),
                                                                      vd.no,
                                                                      vd.fno,
                                                                      vd.mask,
                                                                      vd.vertex,
                                                                      thread_id,
                                                                      &automask_data);

          if (fade > FACE_SET_BRUSH_MIN_FADE) {
            face_sets[face_i] = ss->cache->paint_face_set;
            changed = true;
          }
        }
      }
      BKE_pbvh_vertex_iter_end;

      if (changed) {
        undo::push_node(ob, node, undo::Type::FaceSet);
      }
    }
  });
  attribute.finish();
}

static void do_draw_face_sets_brush_grids(Object *ob,
                                          const Brush *brush,
                                          const Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  const float bstrength = ss->cache->bstrength;
  SubdivCCG &subdiv_ccg = *ss->subdiv_ccg;

  bke::SpanAttributeWriter<int> attribute = ensure_face_sets_mesh(*ob);
  MutableSpan<int> face_sets = attribute.span;

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    const int thread_id = BLI_task_parallel_thread_id(nullptr);
    for (PBVHNode *node : nodes.slice(range)) {
      SculptBrushTest test;
      SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
          ss, &test, brush->falloff_shape);

      auto_mask::NodeData automask_data = auto_mask::node_begin(
          *ob, ss->cache->automasking.get(), *node);

      bool changed = false;

      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
        auto_mask::node_update(automask_data, vd);

        if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
          continue;
        }
        const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                    brush,
                                                                    vd.co,
                                                                    sqrtf(test.dist),
                                                                    vd.no,
                                                                    vd.fno,
                                                                    vd.mask,
                                                                    vd.vertex,
                                                                    thread_id,
                                                                    &automask_data);

        if (fade > FACE_SET_BRUSH_MIN_FADE) {
          const int face_index = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, vd.g);
          face_sets[face_index] = ss->cache->paint_face_set;
          changed = true;
        }
      }
      BKE_pbvh_vertex_iter_end;

      if (changed) {
        undo::push_node(ob, node, undo::Type::FaceSet);
      }
    }
  });
  attribute.finish();
}

static void do_draw_face_sets_brush_bmesh(Object *ob,
                                          const Brush *brush,
                                          const Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  const float bstrength = ss->cache->bstrength;
  const int cd_offset = ensure_face_sets_bmesh(*ob);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    const int thread_id = BLI_task_parallel_thread_id(nullptr);
    for (PBVHNode *node : nodes.slice(range)) {

      SculptBrushTest test;
      SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
          ss, &test, brush->falloff_shape);

      /* Disable auto-masking code path which rely on an undo step to access original data.
       *
       * This is because the dynamic topology uses BMesh Log based undo system, which creates a
       * single node for the undo step, and its type could be different for the needs of the brush
       * undo and the original data access.
       *
       * For the brushes like Draw the ss->cache->automasking is set to nullptr at the first step
       * of the brush, as there is an explicit check there for the brushes which support dynamic
       * topology. Do it locally here for the Draw Face Set brush here, to mimic the behavior of
       * the other brushes but without marking the brush as supporting dynamic topology. */
      auto_mask::NodeData automask_data = auto_mask::node_begin(*ob, nullptr, *node);

      bool changed = false;

      for (BMFace *f : BKE_pbvh_bmesh_node_faces(node)) {
        if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
          continue;
        }

        float3 face_center;
        BM_face_calc_center_median(f, face_center);

        const BMLoop *l_iter = f->l_first = BM_FACE_FIRST_LOOP(f);
        do {
          if (!sculpt_brush_test_sq_fn(&test, l_iter->v->co)) {
            continue;
          }

          BMVert *vert = l_iter->v;

          /* There is no need to update the automasking data as it is disabled above. Additionally,
           * there is no access to the PBVHVertexIter as iteration happens over faces.
           *
           * The full auto-masking support would be very good to be implemented here, so keeping
           * the typical code flow for it here for the reference, and ease of looking at what needs
           * to be done for such integration.
           *
           * auto_mask::node_update(automask_data, vd); */

          const float fade = bstrength *
                             SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          face_center,
                                                          sqrtf(test.dist),
                                                          f->no,
                                                          f->no,
                                                          0.0f,
                                                          BKE_pbvh_make_vref(intptr_t(vert)),
                                                          thread_id,
                                                          &automask_data);

          if (fade <= FACE_SET_BRUSH_MIN_FADE) {
            continue;
          }

          int &fset = *static_cast<int *>(POINTER_OFFSET(f->head.data, cd_offset));
          fset = ss->cache->paint_face_set;
          changed = true;
          break;

        } while ((l_iter = l_iter->next) != f->l_first);
      }

      if (changed) {
        undo::push_node(ob, node, undo::Type::FaceSet);
      }
    }
  });
}

static void do_relax_face_sets_brush_task(Object *ob,
                                          const Brush *brush,
                                          const int iteration,
                                          PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);

  const bool relax_face_sets = !(ss->cache->iteration_count % 3 == 0);
  /* This operations needs a strength tweak as the relax deformation is too weak by default. */
  if (relax_face_sets && iteration < 2) {
    bstrength *= 1.5f;
  }

  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    auto_mask::node_update(automask_data, vd);

    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    if (relax_face_sets == vert_has_unique_face_set(ss, vd.vertex)) {
      continue;
    }

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    smooth::relax_vertex(ss, &vd, fade * bstrength, relax_face_sets, vd.co);
  }
  BKE_pbvh_vertex_iter_end;
}

void do_draw_face_sets_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  BKE_curvemapping_init(brush->curve);

  if (ss->cache->alt_smooth) {
    SCULPT_boundary_info_ensure(ob);
    for (int i = 0; i < 4; i++) {
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          do_relax_face_sets_brush_task(ob, brush, i, nodes[i]);
        }
      });
    }
  }
  else {
    switch (BKE_pbvh_type(ss->pbvh)) {
      case PBVH_FACES:
        do_draw_face_sets_brush_faces(ob, brush, nodes);
        break;
      case PBVH_GRIDS:
        do_draw_face_sets_brush_grids(ob, brush, nodes);
        break;
      case PBVH_BMESH:
        do_draw_face_sets_brush_bmesh(ob, brush, nodes);
        break;
    }
  }
}

static void face_sets_update(Object &object,
                             const Span<PBVHNode *> nodes,
                             const FunctionRef<void(Span<int>, MutableSpan<int>)> calc_face_sets)
{
  PBVH &pbvh = *object.sculpt->pbvh;
  bke::SpanAttributeWriter<int> face_sets = ensure_face_sets_mesh(object);

  struct TLS {
    Vector<int> face_indices;
    Vector<int> new_face_sets;
  };

  threading::EnumerableThreadSpecific<TLS> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    TLS &tls = all_tls.local();
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> faces =
          BKE_pbvh_type(&pbvh) == PBVH_FACES ?
              bke::pbvh::node_face_indices_calc_mesh(pbvh, *node, tls.face_indices) :
              bke::pbvh::node_face_indices_calc_grids(pbvh, *node, tls.face_indices);

      tls.new_face_sets.reinitialize(faces.size());
      MutableSpan<int> new_face_sets = tls.new_face_sets;
      array_utils::gather(face_sets.span.as_span(), faces, new_face_sets);
      calc_face_sets(faces, new_face_sets);
      if (!array_utils::indexed_data_equal<int>(face_sets.span, faces, new_face_sets)) {
        continue;
      }

      undo::push_node(&object, node, undo::Type::FaceSet);
      array_utils::scatter(new_face_sets.as_span(), faces, face_sets.span);
      BKE_pbvh_node_mark_update_face_sets(node);
    }
  });

  face_sets.finish();
}

/* Face Sets Operators */

enum class CreateMode {
  Masked = 0,
  Visible = 1,
  All = 2,
  Selection = 3,
};

static void clear_face_sets(Object &object, const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (!attributes.contains(".sculpt_face_set")) {
    return;
  }
  const PBVH &pbvh = *object.sculpt->pbvh;
  const int default_face_set = mesh.face_sets_color_default;
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  threading::EnumerableThreadSpecific<Vector<int>> all_face_indices;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    Vector<int> &face_indices = all_face_indices.local();
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> faces = bke::pbvh::node_face_indices_calc_mesh(pbvh, *node, face_indices);
      if (std::any_of(faces.begin(), faces.end(), [&](const int face) {
            return face_sets[face] != default_face_set;
          }))
      {
        undo::push_node(&object, node, undo::Type::FaceSet);
        BKE_pbvh_node_mark_update_face_sets(node);
      }
    }
  });
  attributes.remove(".sculpt_face_set");
}

static int sculpt_face_set_create_exec(bContext *C, wmOperator *op)
{
  Object &object = *CTX_data_active_object(C);
  SculptSession &ss = *object.sculpt;
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  const CreateMode mode = CreateMode(RNA_enum_get(op->ptr, "mode"));

  if (BKE_pbvh_type(ss.pbvh) == PBVH_BMESH) {
    /* Dyntopo not supported. */
    return OPERATOR_CANCELLED;
  }

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const bke::AttributeAccessor attributes = mesh.attributes();

  BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);

  undo::push_begin(&object, op);

  const int next_face_set = find_next_available_id(object);

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(ss.pbvh, {});
  switch (mode) {
    case CreateMode::Masked: {
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly",
                                                                  bke::AttrDomain::Face);
      const VArraySpan<float> mask = *attributes.lookup<float>(".sculpt_mask",
                                                               bke::AttrDomain::Point);
      if (!mask.is_empty()) {
        face_sets_update(object, nodes, [&](const Span<int> indices, MutableSpan<int> face_sets) {
          for (const int i : indices.index_range()) {
            if (!hide_poly.is_empty() && hide_poly[indices[i]]) {
              continue;
            }
            const Span<int> face_verts = corner_verts.slice(faces[indices[i]]);
            if (!std::any_of(face_verts.begin(), face_verts.end(), [&](const int vert) {
                  return mask[vert] > 0.5f;
                }))
            {
              continue;
            }
            face_sets[i] = next_face_set;
          }
        });
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
          clear_face_sets(object, nodes);
          break;
        case array_utils::BooleanMix::Mixed:
          const VArraySpan<bool> hide_poly_span(hide_poly);
          face_sets_update(
              object, nodes, [&](const Span<int> indices, MutableSpan<int> face_sets) {
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
      face_sets_update(
          object, nodes, [&](const Span<int> /*indices*/, MutableSpan<int> face_sets) {
            face_sets.fill(next_face_set);
          });
      break;
    }
    case CreateMode::Selection: {
      const VArraySpan<bool> select_poly = *attributes.lookup_or_default<bool>(
          ".select_poly", bke::AttrDomain::Face, false);
      face_sets_update(object, nodes, [&](const Span<int> indices, MutableSpan<int> face_sets) {
        for (const int i : indices.index_range()) {
          if (select_poly[indices[i]]) {
            face_sets[i] = next_face_set;
          }
        }
      });

      break;
    }
  }

  undo::push_end(&object);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_create(wmOperatorType *ot)
{
  ot->name = "Create Face Set";
  ot->idname = "SCULPT_OT_face_sets_create";
  ot->description = "Create a new Face Set";

  ot->exec = sculpt_face_set_create_exec;
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

static void sculpt_face_sets_init_flood_fill(Object *ob, const FaceSetsFloodFillFn &test_fn)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  BitVector<> visited_faces(mesh->faces_num, false);

  bke::SpanAttributeWriter<int> face_sets = ensure_face_sets_mesh(*ob);

  const Span<int2> edges = mesh->edges();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_edges = mesh->corner_edges();

  if (ss->edge_to_face_map.is_empty()) {
    ss->edge_to_face_map = bke::mesh::build_edge_to_face_map(
        faces, corner_edges, edges.size(), ss->edge_to_face_offsets, ss->edge_to_face_indices);
  }

  int next_face_set = 1;

  for (const int i : faces.index_range()) {
    if (visited_faces[i]) {
      continue;
    }
    std::queue<int> queue;

    face_sets.span[i] = next_face_set;
    visited_faces[i].set(true);
    queue.push(i);

    while (!queue.empty()) {
      const int face_i = queue.front();
      queue.pop();

      for (const int edge_i : corner_edges.slice(faces[face_i])) {
        for (const int neighbor_i : ss->edge_to_face_map[edge_i]) {
          if (neighbor_i == face_i) {
            continue;
          }
          if (visited_faces[neighbor_i]) {
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

Array<int> duplicate_face_sets(const Mesh &mesh)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArray<int> attribute = *attributes.lookup_or_default(
      ".sculpt_face_set", bke::AttrDomain::Face, 0);
  Array<int> face_sets(attribute.size());
  array_utils::copy(attribute, face_sets.as_mutable_span());
  return face_sets;
}

static int sculpt_face_set_init_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const InitMode mode = InitMode(RNA_enum_get(op->ptr, "mode"));

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);

  /* Dyntopo not supported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  PBVH *pbvh = ob->sculpt->pbvh;
  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  undo::push_begin(ob, op);
  for (PBVHNode *node : nodes) {
    undo::push_node(ob, node, undo::Type::FaceSet);
  }

  const float threshold = RNA_float_get(op->ptr, "threshold");

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const bke::AttributeAccessor attributes = mesh->attributes();

  switch (mode) {
    case InitMode::LooseParts: {
      const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
          ".hide_poly", bke::AttrDomain::Face, false);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int from_face, const int /*edge*/, const int to_face) {
            return hide_poly[from_face] == hide_poly[to_face];
          });
      break;
    }
    case InitMode::Materials: {
      bke::SpanAttributeWriter<int> face_sets = ensure_face_sets_mesh(*ob);
      const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
          "material_index", bke::AttrDomain::Face, 0);
      for (const int i : IndexRange(mesh->faces_num)) {
        face_sets.span[i] = material_indices[i] + 1;
      }
      face_sets.finish();
      break;
    }
    case InitMode::Normals: {
      const Span<float3> face_normals = mesh->face_normals();
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int from_face, const int /*edge*/, const int to_face) -> bool {
            return std::abs(math::dot(face_normals[from_face], face_normals[to_face])) > threshold;
          });
      break;
    }
    case InitMode::UVSeams: {
      const VArraySpan<bool> uv_seams = *mesh->attributes().lookup_or_default<bool>(
          ".uv_seam", bke::AttrDomain::Edge, false);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return !uv_seams[edge];
          });
      break;
    }
    case InitMode::Creases: {
      const VArraySpan<float> creases = *attributes.lookup_or_default<float>(
          "crease_edge", bke::AttrDomain::Edge, 0.0f);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return creases[edge] < threshold;
          });
      break;
    }
    case InitMode::SharpEdges: {
      const VArraySpan<bool> sharp_edges = *mesh->attributes().lookup_or_default<bool>(
          "sharp_edge", bke::AttrDomain::Edge, false);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return !sharp_edges[edge];
          });
      break;
    }
    case InitMode::BevelWeight: {
      const VArraySpan<float> bevel_weights = *attributes.lookup_or_default<float>(
          "bevel_weight_edge", bke::AttrDomain::Edge, 0.0f);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return bevel_weights[edge] < threshold;
          });
      break;
    }
    case InitMode::FaceSetBoundaries: {
      Array<int> face_sets_copy = duplicate_face_sets(*mesh);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int from_face, const int /*edge*/, const int to_face) -> bool {
            return face_sets_copy[from_face] == face_sets_copy[to_face];
          });
      break;
    }
  }

  undo::push_end(ob);

  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_redraw(node);
  }

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_init(wmOperatorType *ot)
{
  ot->name = "Init Face Sets";
  ot->idname = "SCULPT_OT_face_sets_init";
  ot->description = "Initializes all Face Sets in the mesh";

  ot->exec = sculpt_face_set_init_exec;
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

static void face_hide_update(Object &object,
                             const Span<PBVHNode *> nodes,
                             const FunctionRef<void(Span<int>, MutableSpan<bool>)> calc_hide)
{
  PBVH &pbvh = *object.sculpt->pbvh;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_poly", bke::AttrDomain::Face);

  struct TLS {
    Vector<int> face_indices;
    Vector<bool> new_hide;
  };

  bool any_changed = false;
  threading::EnumerableThreadSpecific<TLS> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    TLS &tls = all_tls.local();
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> faces =
          (BKE_pbvh_type(&pbvh) == PBVH_FACES) ?
              bke::pbvh::node_face_indices_calc_mesh(pbvh, *node, tls.face_indices) :
              bke::pbvh::node_face_indices_calc_grids(pbvh, *node, tls.face_indices);

      tls.new_hide.reinitialize(faces.size());
      MutableSpan<bool> new_hide = tls.new_hide;
      array_utils::gather(hide_poly.span.as_span(), faces, new_hide);
      calc_hide(faces, new_hide);
      if (!array_utils::indexed_data_equal<bool>(hide_poly.span, faces, new_hide)) {
        continue;
      }

      any_changed = true;
      undo::push_node(&object, node, undo::Type::HideFace);
      array_utils::scatter(new_hide.as_span(), faces, hide_poly.span);
      BKE_pbvh_node_mark_update_visibility(node);
    }
  });

  hide_poly.finish();
  if (any_changed) {
    hide::sync_all_from_faces(object);
  }
}

static void show_all(Depsgraph &depsgraph, Object &object, const Span<PBVHNode *> nodes)
{
  switch (BKE_pbvh_type(object.sculpt->pbvh)) {
    case PBVH_FACES:
      hide::mesh_show_all(object, nodes);
      break;
    case PBVH_GRIDS:
      hide::grids_show_all(depsgraph, object, nodes);
      break;
    case PBVH_BMESH:
      BLI_assert_unreachable();
      break;
  }
}

static int sculpt_face_set_change_visibility_exec(bContext *C, wmOperator *op)
{
  Object &object = *CTX_data_active_object(C);
  SculptSession *ss = object.sculpt;
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  Mesh *mesh = BKE_object_get_original_mesh(&object);
  BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    /* Not supported for dyntopo. There is no active face. */
    return OPERATOR_CANCELLED;
  }

  const VisibilityMode mode = VisibilityMode(RNA_enum_get(op->ptr, "mode"));
  const int active_face_set = active_face_set_get(ss);

  undo::push_begin(&object, op);

  PBVH *pbvh = object.sculpt->pbvh;
  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});

  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan<int> face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                            bke::AttrDomain::Face);

  switch (mode) {
    case VisibilityMode::Toggle: {
      if (hide_poly.contains(true) || face_sets.is_empty()) {
        show_all(depsgraph, object, nodes);
      }
      else {
        face_hide_update(object, nodes, [&](const Span<int> faces, MutableSpan<bool> hide) {
          for (const int i : hide.index_range()) {
            hide[i] = face_sets[faces[i]] != active_face_set;
          }
        });
      }
      break;
    }
    case VisibilityMode::ShowActive:
      if (face_sets.is_empty()) {
        show_all(depsgraph, object, nodes);
      }
      else {
        face_hide_update(object, nodes, [&](const Span<int> faces, MutableSpan<bool> hide) {
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
        face_hide_update(object, nodes, [&](const Span<int> /*faces*/, MutableSpan<bool> hide) {
          hide.fill(true);
        });
      }
      else {
        face_hide_update(object, nodes, [&](const Span<int> faces, MutableSpan<bool> hide) {
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
    float location[3];
    copy_v3_v3(location, SCULPT_active_vertex_co_get(ss));
    mul_m4_v3(object.object_to_world, location);
    copy_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter = 1;
    ups->last_stroke_valid = true;
  }

  undo::push_end(&object);

  bke::pbvh::update_visibility(*ss->pbvh);
  BKE_sculpt_hide_poly_pointer_update(object);

  SCULPT_topology_islands_invalidate(object.sculpt);
  hide::tag_update_visibility(*C);

  return OPERATOR_FINISHED;
}

static int sculpt_face_set_change_visibility_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  /* Update the active vertex and Face Set using the cursor position to avoid relying on the paint
   * cursor updates. */
  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);

  return sculpt_face_set_change_visibility_exec(C, op);
}

void SCULPT_OT_face_set_change_visibility(wmOperatorType *ot)
{
  ot->name = "Face Sets Visibility";
  ot->idname = "SCULPT_OT_face_set_change_visibility";
  ot->description = "Change the visibility of the Face Sets of the sculpt";

  ot->exec = sculpt_face_set_change_visibility_exec;
  ot->invoke = sculpt_face_set_change_visibility_invoke;
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

static int sculpt_face_sets_randomize_colors_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  /* Dyntopo not supported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  PBVH *pbvh = ob->sculpt->pbvh;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const bke::AttributeAccessor attributes = mesh->attributes();

  if (!attributes.contains(".sculpt_face_set")) {
    return OPERATOR_CANCELLED;
  }

  const VArray<int> face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  const int random_index = clamp_i(
      ss->totfaces * BLI_hash_int_01(mesh->face_sets_color_seed), 0, max_ii(0, ss->totfaces - 1));
  mesh->face_sets_color_default = face_sets[random_index];

  mesh->face_sets_color_seed += 1;

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_redraw(node);
  }

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_randomize_colors(wmOperatorType *ot)
{
  ot->name = "Randomize Face Sets Colors";
  ot->idname = "SCULPT_OT_face_sets_randomize_colors";
  ot->description = "Generates a new set of random colors to render the Face Sets in the viewport";

  ot->exec = sculpt_face_sets_randomize_colors_exec;
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

static void sculpt_face_set_grow_shrink(Object &object,
                                        const EditMode mode,
                                        const int active_face_set_id,
                                        const bool modify_hidden,
                                        wmOperator *op)
{
  SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = ss.vert_to_face_map;
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  Array<int> prev_face_sets = duplicate_face_sets(mesh);

  undo::push_begin(&object, op);

  const Vector<PBVHNode *> nodes = bke::pbvh::search_gather(ss.pbvh, {});
  face_sets_update(object, nodes, [&](const Span<int> indices, MutableSpan<int> face_sets) {
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

  undo::push_end(&object);
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

static void sculpt_face_set_delete_geometry(Object *ob,
                                            const int active_face_set_id,
                                            const bool modify_hidden)
{
  Mesh &mesh = *static_cast<Mesh *>(ob->data);
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

static void sculpt_face_set_edit_fair_face_set(Object *ob,
                                               const int active_face_set_id,
                                               const eMeshFairingDepth fair_order,
                                               const float strength)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  Vector<float3> orig_positions;
  Vector<bool> fair_verts;

  orig_positions.resize(totvert);
  fair_verts.resize(totvert);

  SCULPT_boundary_info_ensure(ob);

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    orig_positions[i] = SCULPT_vertex_co_get(ss, vertex);
    fair_verts[i] = !SCULPT_vertex_is_boundary(ss, vertex) &&
                    vert_has_face_set(ss, vertex, active_face_set_id) &&
                    vert_has_unique_face_set(ss, vertex);
  }

  MutableSpan<float3> positions = SCULPT_mesh_deformed_positions_get(ss);
  BKE_mesh_prefair_and_fair_verts(mesh, positions, fair_verts.data(), fair_order);

  for (int i = 0; i < totvert; i++) {
    if (fair_verts[i]) {
      interp_v3_v3v3(positions[i], orig_positions[i], positions[i], strength);
    }
  }
}

static bool sculpt_face_set_edit_is_operation_valid(const Object &object,
                                                    const EditMode mode,
                                                    const bool modify_hidden)
{
  if (BKE_pbvh_type(object.sculpt->pbvh) == PBVH_BMESH) {
    /* Dyntopo is not supported. */
    return false;
  }

  if (mode == EditMode::DeleteGeometry) {
    if (BKE_pbvh_type(object.sculpt->pbvh) == PBVH_GRIDS) {
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
    if (BKE_pbvh_type(object.sculpt->pbvh) == PBVH_GRIDS) {
      /* TODO: Multi-resolution topology representation using grids and duplicates can't be used
       * directly by the fair algorithm. Multi-resolution topology needs to be exposed in a
       * different way or converted to a mesh for this operation. */
      return false;
    }
  }

  return true;
}

static void sculpt_face_set_edit_modify_geometry(bContext *C,
                                                 Object *ob,
                                                 const int active_face_set,
                                                 const EditMode mode,
                                                 const bool modify_hidden,
                                                 wmOperator *op)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  undo::geometry_begin(ob, op);
  switch (mode) {
    case EditMode::DeleteGeometry:
      sculpt_face_set_delete_geometry(ob, active_face_set, modify_hidden);
      break;
    default:
      BLI_assert_unreachable();
  }
  undo::geometry_end(ob);
  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);
}

static void sculpt_face_set_edit_modify_coordinates(
    bContext *C, Object *ob, const int active_face_set, const EditMode mode, wmOperator *op)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});

  const float strength = RNA_float_get(op->ptr, "strength");

  undo::push_begin(ob, op);
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update(node);
    undo::push_node(ob, node, undo::Type::Position);
  }
  switch (mode) {
    case EditMode::FairPositions:
      sculpt_face_set_edit_fair_face_set(
          ob, active_face_set, MESH_FAIRING_DEPTH_POSITION, strength);
      break;
    case EditMode::FairTangency:
      sculpt_face_set_edit_fair_face_set(
          ob, active_face_set, MESH_FAIRING_DEPTH_TANGENCY, strength);
      break;
    default:
      BLI_assert_unreachable();
  }

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, ob, true);
  }
  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
  undo::push_end(ob);
}

static bool sculpt_face_set_edit_init(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const EditMode mode = EditMode(RNA_enum_get(op->ptr, "mode"));
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  if (!sculpt_face_set_edit_is_operation_valid(*ob, mode, modify_hidden)) {
    return false;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);

  return true;
}

static int sculpt_face_set_edit_exec(bContext *C, wmOperator *op)
{
  if (!sculpt_face_set_edit_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  Object *ob = CTX_data_active_object(C);

  const int active_face_set = RNA_int_get(op->ptr, "active_face_set");
  const EditMode mode = EditMode(RNA_enum_get(op->ptr, "mode"));
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  switch (mode) {
    case EditMode::DeleteGeometry:
      sculpt_face_set_edit_modify_geometry(C, ob, active_face_set, mode, modify_hidden, op);
      break;
    case EditMode::Grow:
    case EditMode::Shrink:
      sculpt_face_set_grow_shrink(*ob, mode, active_face_set, modify_hidden, op);
      break;
    case EditMode::FairPositions:
    case EditMode::FairTangency:
      sculpt_face_set_edit_modify_coordinates(C, ob, active_face_set, mode, op);
      break;
  }

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

static int sculpt_face_set_edit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);

  /* Update the current active Face Set and Vertex as the operator can be used directly from the
   * tool without brush cursor. */
  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  if (!SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false)) {
    /* The cursor is not over the mesh. Cancel to avoid editing the last updated Face Set ID. */
    return OPERATOR_CANCELLED;
  }
  RNA_int_set(op->ptr, "active_face_set", active_face_set_get(ss));

  return sculpt_face_set_edit_exec(C, op);
}

void SCULPT_OT_face_sets_edit(wmOperatorType *ot)
{
  ot->name = "Edit Face Set";
  ot->idname = "SCULPT_OT_face_set_edit";
  ot->description = "Edits the current active Face Set";

  ot->invoke = sculpt_face_set_edit_invoke;
  ot->exec = sculpt_face_set_edit_exec;
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
                             true,
                             "Modify Hidden",
                             "Apply the edit operation to hidden geometry");
}

}  // namespace blender::ed::sculpt_paint::face_set
