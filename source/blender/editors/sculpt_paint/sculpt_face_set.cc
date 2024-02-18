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
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.h"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_layer.hh"
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

#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "bmesh.hh"
#include "bmesh_idmap.hh"

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::Span;
using blender::Vector;

namespace blender::ed::sculpt_paint::face_set {
/* Utils. */

static int mesh_find_next_available_id(Mesh *mesh)
{
  using namespace blender;
  const VArray<int> attribute = *mesh->attributes().lookup<int>(".sculpt_face_set",
                                                                bke::AttrDomain::Face);
  if (!attribute) {
    return SCULPT_FACE_SET_NONE;
  }
  const VArraySpan<int> face_sets(attribute);

  int next_face_set_id = 0;
  for (const int i : face_sets.index_range()) {
    next_face_set_id = max_ii(next_face_set_id, face_sets[i]);
  }
  next_face_set_id++;

  return next_face_set_id;
}

void initialize_none_to_id(Mesh *mesh, int new_id)
{
  using namespace blender;
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
  object.sculpt->face_sets = static_cast<int *>(CustomData_get_layer_named_for_write(
      &mesh.face_data, CD_PROP_INT32, ".sculpt_face_set", mesh.faces_num));
  return attributes.lookup_or_add_for_write_span<int>(".sculpt_face_set", bke::AttrDomain::Face);
}

/* Draw Face Sets Brush. */
constexpr float FACE_SET_BRUSH_MIN_FADE = 0.05f;
static void do_draw_face_sets_brush_task(Object *ob,
                                         const Brush *brush,
                                         bool have_fset_automasking,
                                         bool set_active_faceset,
                                         PBVHNode *node)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  const float bstrength = ss->cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  const MutableSpan<float3> positions = SCULPT_mesh_deformed_positions_get(ss);
  auto_mask::NodeData automask_data = auto_mask::node_begin(*ob, ss->cache->automasking.get(), *node);

  /* Ensure automasking data is up to date. */
  if (ss->cache->automasking) {
    PBVHVertexIter vd;

    BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_ALL) {
      auto_mask::node_update(automask_data, vd);
    }
    BKE_pbvh_vertex_iter_end;
  }

  bool changed = false;

  PBVHFaceIter fd;
  BKE_pbvh_face_iter_begin (ss->pbvh, node, fd) {
    if (SCULPT_face_is_hidden(ss, fd.face)) {
      continue;
    }

    float3 poly_center = {};
    float mask = 0.0;

    for (int i = 0; i < fd.verts_num; i++) {
      poly_center += SCULPT_vertex_co_get(ss, fd.verts[i]);
      mask += SCULPT_vertex_mask_get(ss, fd.verts[i]);
    }

    poly_center /= (float)fd.verts_num;
    mask /= (float)fd.verts_num;

    if (!sculpt_brush_test_sq_fn(&test, poly_center)) {
      continue;
    }

    /* Face set automasking in inverted draw mode is tricky, we have
     * to sample the automasking face set after the stroke has started.
     */
    if (set_active_faceset &&
        *fd.face_set != abs(ss->cache->automasking->settings.initial_face_set))
    {

      float radius = ss->cache->radius;
      float pixels = 8; /* TODO: multiply with DPI? */
      radius = pixels * (radius / (float)ss->cache->dyntopo_pixel_radius);

      if (sqrtf(test.dist) < radius) {
        ss->cache->automasking->settings.initial_face_set = *fd.face_set;
        set_active_faceset = false;
        ss->cache->automasking->settings.flags |= BRUSH_AUTOMASKING_FACE_SETS;
      }
      else {
        continue;
      }
    }

    if (have_fset_automasking) {
      if (*fd.face_set != ss->cache->automasking->settings.initial_face_set) {
        continue;
      }
    }

    float fno[3];
    face_normal_get(ob, fd.face, fno);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                poly_center,
                                                                sqrtf(test.dist),
                                                                fno,
                                                                fno,
                                                                mask,
                                                                fd.verts[0],
                                                                thread_id,
                                                                &automask_data);

    if (fade > 0.05f) {
      for (int i = 0; i < fd.verts_num; i++) {
        BKE_sculpt_boundary_flag_update(ss, fd.verts[i], true);
      }

      *fd.face_set = ss->cache->paint_face_set;
      changed = true;
    }
  }
  BKE_pbvh_face_iter_end(fd);

  if (changed) {
    BKE_pbvh_vert_tag_update_normal_triangulation(node);
    BKE_pbvh_node_mark_rebuild_draw(node);
  }
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
  auto_mask::NodeData automask_data = auto_mask::node_begin(*ob, ss->cache->automasking.get(), *node);

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

    smooth::relax_vertex(ss, &vd, fade * bstrength, SCULPT_BOUNDARY_FACE_SET, vd.co);
  }
  BKE_pbvh_vertex_iter_end;
}

void do_draw_face_sets_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  BKE_curvemapping_init(brush->curve);

  /* Note: face set automasking is fairly involved in this brush. */
  bool have_fset_automasking = ss->cache->automasking && ss->cache->automasking->settings.flags &
                                                             BRUSH_AUTOMASKING_FACE_SETS;
  /* In invert mode we have to set the automasking face set ourselves. */
  bool set_active_faceset = have_fset_automasking && ss->cache->invert &&
                            ss->cache->automasking->settings.initial_face_set ==
                                ss->cache->paint_face_set;

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
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_draw_face_sets_brush_task(
            ob, brush, have_fset_automasking, set_active_faceset, nodes[i]);
      }
    });
  }
}

/* Face Sets Operators */

enum eSculptFaceGroupsCreateModes {
  SCULPT_FACE_SET_MASKED = 0,
  SCULPT_FACE_SET_VISIBLE = 1,
  SCULPT_FACE_SET_ALL = 2,
  SCULPT_FACE_SET_SELECTION = 3,
};

static EnumPropertyItem prop_sculpt_face_set_create_types[] = {
    {
        SCULPT_FACE_SET_MASKED,
        "MASKED",
        0,
        "Face Set from Masked",
        "Create a new Face Set from the masked faces",
    },
    {
        SCULPT_FACE_SET_VISIBLE,
        "VISIBLE",
        0,
        "Face Set from Visible",
        "Create a new Face Set from the visible vertices",
    },
    {
        SCULPT_FACE_SET_ALL,
        "ALL",
        0,
        "Face Set Full Mesh",
        "Create an unique Face Set with all faces in the sculpt",
    },
    {
        SCULPT_FACE_SET_SELECTION,
        "SELECTION",
        0,
        "Face Set from Edit Mode Selection",
        "Create an Face Set corresponding to the Edit Mode face selection",
    },
    {0, nullptr, 0, nullptr, nullptr},
};

int find_next_available_id(Object &ob)
{
  SculptSession *ss = ob.sculpt;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BKE_sculpt_face_sets_ensure(&ob);

      int fset = 1;
      BMFace *f;
      BMIter iter;
      int cd_fset = ss->attrs.face_set->bmesh_cd_offset;

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        fset = max_ii(fset, BM_ELEM_CD_GET_INT(f, cd_fset) + 1);
      }

      return fset;
    }
    case PBVH_FACES:
    case PBVH_GRIDS:
      return mesh_find_next_available_id(static_cast<Mesh *>(ob.data));
  }

  return 1;
}

static void face_sets_update(Object &object,
                             const Span<PBVHNode *> nodes,
                             const FunctionRef<void(Span<int>, MutableSpan<int>)> calc_face_sets)
{
  PBVH &pbvh = *object.sculpt->pbvh;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::SpanAttributeWriter<int> face_sets = ensure_face_sets_mesh(object);

  Array<int> indices(mesh.faces_num);
  for (int i = 0; i < mesh.faces_num; i++) {
    indices[i] = i;
  }

  for (int i = 0; i < mesh.faces_num; i++) {
    calc_face_sets(indices, face_sets.span);
  }

  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update_face_sets(node);
  }

  face_sets.finish();
}

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
  BKE_sculptsession_sync_attributes(&object, &mesh, false);
}

static int sculpt_face_set_create_exec(bContext *C, wmOperator *op)
{
  Object &object = *CTX_data_active_object(C);
  SculptSession &ss = *object.sculpt;
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  const CreateMode mode = CreateMode(RNA_enum_get(op->ptr, "mode"));

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);

  bool is_bmesh = BKE_pbvh_type(ss.pbvh) == PBVH_BMESH;
  if (is_bmesh) {
    /* Run operator on original mesh and flush back to bmesh. */
    BKE_sculptsession_bm_to_me_for_render(&object);
  }

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const bke::AttributeAccessor attributes = mesh.attributes();

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

  if (is_bmesh) {
    /* Load face sets back into bmesh. */
    BMFace *f;
    BMIter iter;
    int i;
    int cd_fset = CustomData_get_offset_named(&ss.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
    const int *face_sets = static_cast<const int *>(
        CustomData_get_layer_named(&mesh.vert_data, CD_PROP_INT32, ".sculpt_face_set"));

    BM_ITER_MESH_INDEX (f, &iter, ss.bm, BM_FACES_OF_MESH, i) {
      BM_ELEM_CD_SET_INT(f, cd_fset, face_sets[i]);
    }
  }
  undo::push_end(&object);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_create(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Create Face Set";
  ot->idname = "SCULPT_OT_face_sets_create";
  ot->description = "Create a new Face Set";

  /* api callbacks */
  ot->exec = sculpt_face_set_create_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_set_create_types, SCULPT_FACE_SET_MASKED, "Mode", "");
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

static EnumPropertyItem prop_sculpt_face_sets_init_types[] = {
    {
        int(InitMode::LooseParts),
        "LOOSE_PARTS",
        0,
        "Face Sets from Loose Parts",
        "Create a Face Set per loose part in the mesh",
    },
    {
        int(InitMode::Materials),
        "MATERIALS",
        0,
        "Face Sets from Material Slots",
        "Create a Face Set per Material Slot",
    },
    {
        int(InitMode::Normals),
        "NORMALS",
        0,
        "Face Sets from Mesh Normals",
        "Create Face Sets for Faces that have similar normal",
    },
    {
        int(InitMode::UVSeams),
        "UV_SEAMS",
        0,
        "Face Sets from UV Seams",
        "Create Face Sets using UV Seams as boundaries",
    },
    {
        int(InitMode::Creases),
        "CREASES",
        0,
        "Face Sets from Edge Creases",
        "Create Face Sets using Edge Creases as boundaries",
    },
    {
        int(InitMode::BevelWeight),
        "BEVEL_WEIGHT",
        0,
        "Face Sets from Bevel Weight",
        "Create Face Sets using Bevel Weights as boundaries",
    },
    {
        int(InitMode::SharpEdges),
        "SHARP_EDGES",
        0,
        "Face Sets from Sharp Edges",
        "Create Face Sets using Sharp Edges as boundaries",
    },

    {
        int(InitMode::FaceSetBoundaries),
        "FACE_SET_BOUNDARIES",
        0,
        "Face Sets from Face Set Boundaries",
        "Create a Face Set per isolated Face Set",
    },

    {0, nullptr, 0, nullptr, nullptr},
};

using FaceSetsFloodFillFn = blender::FunctionRef<bool(int from_face, int edge, int to_face)>;

static void sculpt_face_sets_init_flood_fill(Object *ob, const FaceSetsFloodFillFn &test_fn)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  BitVector<> visited_faces(mesh->faces_num, false);

  int *face_sets = static_cast<int *>(CustomData_get_layer_named_for_write(
      &mesh->face_data, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(face_set), mesh->faces_num));

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

    face_sets[i] = next_face_set;
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

          face_sets[neighbor_i] = next_face_set;
          visited_faces[neighbor_i].set(true);
          queue.push(neighbor_i);
        }
      }
    }

    next_face_set += 1;
  }
}

static void sculpt_face_sets_init_loop(Object *ob, const InitMode mode)
{
  using namespace blender;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  SculptSession *ss = ob->sculpt;

  if (mode == InitMode::Materials) {
    const bke::AttributeAccessor attributes = mesh->attributes();
    const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
        "material_index", bke::AttrDomain::Face, 0);
    for (const int i : IndexRange(mesh->faces_num)) {
      ss->face_sets[i] = material_indices[i] + 1;
    }
  }
}

static int sculpt_face_set_init_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const InitMode mode = InitMode(RNA_enum_get(op->ptr, "mode"));

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);

  PBVH *pbvh = ob->sculpt->pbvh;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, {});

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  const float threshold = RNA_float_get(op->ptr, "threshold");

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  BKE_sculpt_face_sets_ensure(ob);

  undo::push_begin(ob, op);
  for (PBVHNode *node : nodes) {
    undo::push_node(ob, node, undo::Type::FaceSet);
  }

  /* Flush bmesh to base mesh. */
  if (ss->bm) {
    BKE_sculptsession_bm_to_me_for_render(ob);

    if (!ss->edge_to_face_map.is_empty()) {
      ss->edge_to_face_map = {};
      ss->edge_to_face_indices = {};
      ss->edge_to_face_offsets = {};
    }

    if (!ss->vert_to_face_map.is_empty()) {
      ss->vert_to_face_map = {};
    }
  }

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
      sculpt_face_sets_init_loop(ob, InitMode::Materials);
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
      const float *creases = static_cast<const float *>(
          CustomData_get_layer_named(&mesh->edge_data, CD_PROP_FLOAT, "crease_edge"));
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return creases ? creases[edge] < threshold : true;
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
      const float *bevel_weights = static_cast<const float *>(
          CustomData_get_layer_named(&mesh->edge_data, CD_PROP_FLOAT, "bevel_weight_edge"));
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return bevel_weights ? bevel_weights[edge] < threshold : true;
          });
      break;
    }
    case InitMode::FaceSetBoundaries: {
      Array<int> face_sets_copy(Span<int>(ss->face_sets, mesh->faces_num));
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int from_face, const int /*edge*/, const int to_face) -> bool {
            return face_sets_copy[from_face] == face_sets_copy[to_face];
          });
      break;
    }
  }

  undo::push_end(ob);

  if (ss->bm) {
    SCULPT_vertex_random_access_ensure(ss);
    SCULPT_face_random_access_ensure(ss);
    BKE_sculpt_face_sets_ensure(ob);

    int cd_fset = ss->attrs.face_set->bmesh_cd_offset;
    const int *face_sets = static_cast<const int *>(CustomData_get_layer_named(
        &mesh->face_data, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(face_set)));

    for (int i = 0; i < mesh->faces_num; i++) {
      BMFace *f = ss->bm->ftable[i];
      BM_ELEM_CD_SET_INT(f, cd_fset, face_sets[i]);
    }
  }

  int verts_num = SCULPT_vertex_count_get(ob->sculpt);
  for (int i : IndexRange(verts_num)) {
    BKE_sculpt_boundary_flag_update(ob->sculpt, BKE_pbvh_index_to_vertex(ss->pbvh, i), true);
  }

  /* Sync face sets visibility and vertex visibility as now all Face Sets are visible. */
  hide::sync_all_from_faces(*ob);

  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update_visibility(node);
  }

  bke::pbvh::update_visibility(*ss->pbvh);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

Array<int> duplicate_face_sets(Object &object)
{
  SculptSession *ss = object.sculpt;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS: {
      Mesh &mesh = *BKE_object_get_original_mesh(&object);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArray<int> attribute = *attributes.lookup_or_default(
          ".sculpt_face_set", bke::AttrDomain::Face, 0);
      Array<int> face_sets(attribute.size());
      array_utils::copy(attribute, face_sets.as_mutable_span());
      return face_sets;
    }
    case PBVH_BMESH: {
      BMesh *bm = ss->bm;
      Array<int> face_sets(bm->totface);
      int cd_fset = CustomData_get_offset_named(&bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

      if (cd_fset == -1) {
        return face_sets;
      }

      BMIter iter;
      BMFace *f;
      int i = 0;

      BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        face_sets[i++] = BM_ELEM_CD_GET_INT(f, cd_fset);
      }
    }
  }

  return Array<int>(ss->totfaces);
}

void SCULPT_OT_face_sets_init(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Init Face Sets";
  ot->idname = "SCULPT_OT_face_sets_init";
  ot->description = "Initializes all Face Sets in the mesh";

  /* api callbacks */
  ot->exec = sculpt_face_set_init_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_sets_init_types, SCULPT_FACE_SET_MASKED, "Mode", "");
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

static EnumPropertyItem prop_sculpt_face_sets_change_visibility_types[] = {
    {
        int(VisibilityMode::Toggle),
        "TOGGLE",
        0,
        "Toggle Visibility",
        "Hide all Face Sets except for the active one",
    },
    {
        int(VisibilityMode::ShowActive),
        "SHOW_ACTIVE",
        0,
        "Show Active Face Set",
        "Show Active Face Set",
    },
    {
        int(VisibilityMode::HideActive),
        "HIDE_ACTIVE",
        0,
        "Hide Active Face Sets",
        "Hide Active Face Sets",
    },
    {0, nullptr, 0, nullptr, nullptr},
};

static int sculpt_face_set_change_visibility_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  const VisibilityMode mode = VisibilityMode(RNA_enum_get(op->ptr, "mode"));
  const int tot_vert = SCULPT_vertex_count_get(ss);

  PBVH *pbvh = ob->sculpt->pbvh;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, {});

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  const int active_face_set = active_face_set_get(ss);

  undo::push_begin(ob, op);
  for (PBVHNode *node : nodes) {
    undo::push_node(ob, node, undo::Type::HideFace);
  }

  switch (mode) {
    case VisibilityMode::Toggle: {
      bool hidden_vertex = false;

      /* This can fail with regular meshes with non-manifold geometry as the visibility state can't
       * be synced from face sets to non-manifold vertices. */
      if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
        for (int i = 0; i < tot_vert; i++) {
          PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

          if (!hide::vert_visible_get(ss, vertex)) {
            hidden_vertex = true;
            break;
          }
        }
      }

      if (ss->attrs.hide_poly) {
        for (int i = 0; i < ss->totfaces; i++) {
          PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, i);
          if (SCULPT_face_is_hidden(ss, face)) {
            hidden_vertex = true;
            break;
          }
        }
      }

      BKE_sculpt_hide_poly_ensure(ob);

      if (hidden_vertex) {
        face_set::visibility_all_set(ob, true);
      }
      else {
        if (ss->attrs.face_set) {
          face_set::visibility_all_set(ob, false);
          hide::face_set(ss, active_face_set, true);
        }
        else {
          face_set::visibility_all_set(ob, true);
        }
      }
      break;
    }
    case VisibilityMode::ShowActive:
      BKE_sculpt_hide_poly_ensure(ob);

      if (ss->attrs.face_set) {
        face_set::visibility_all_set(ob, false);
        hide::face_set(ss, active_face_set, true);
      }
      else {
        hide::face_set(ss, active_face_set, true);
      }
      break;
    case VisibilityMode::HideActive:
      BKE_sculpt_hide_poly_ensure(ob);

      if (ss->attrs.face_set) {
        hide::face_set(ss, active_face_set, false);
      }
      else {
        face_set::visibility_all_set(ob, false);
      }

      break;
  }

  /* For modes that use the cursor active vertex, update the rotation origin for viewport
   * navigation.
   */
  if (ELEM(mode, VisibilityMode::Toggle, VisibilityMode::ShowActive)) {
    UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
    float location[3];
    copy_v3_v3(location, SCULPT_active_vertex_co_get(ss));
    mul_m4_v3(ob->object_to_world().ptr(), location);
    copy_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter = 1;
    ups->last_stroke_valid = true;
  }

  /* Sync face sets visibility and vertex visibility. */
  hide::sync_all_from_faces(*ob);

  undo::push_end(ob);
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update_visibility(node);
  }

  bke::pbvh::update_visibility(*ss->pbvh);

  SCULPT_tag_update_overlays(C);

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
  /* Identifiers. */
  ot->name = "Face Sets Visibility";
  ot->idname = "SCULPT_OT_face_set_change_visibility";
  ot->description = "Change the visibility of the Face Sets of the sculpt";

  /* Api callbacks. */
  ot->exec = sculpt_face_set_change_visibility_exec;
  ot->invoke = sculpt_face_set_change_visibility_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_face_sets_change_visibility_types,
               int(VisibilityMode::Toggle),
               "Mode",
               "");
}

static int sculpt_face_sets_randomize_colors_exec(bContext *C, wmOperator * /*op*/)
{

  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  if (!ss->attrs.face_set) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_face_random_access_ensure(ss);

  PBVH *pbvh = ob->sculpt->pbvh;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  mesh->face_sets_color_seed += 1;
  if (ss->attrs.face_set) {
    const int random_index = clamp_i(ss->totfaces * BLI_hash_int_01(mesh->face_sets_color_seed),
                                     0,
                                     max_ii(0, ss->totfaces - 1));
    PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, random_index);

    mesh->face_sets_color_default = blender::bke::paint::face_attr_get<int>(face,
                                                                            ss->attrs.face_set);
  }

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, {});
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_redraw(node);
  }

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_randomize_colors(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Randomize Face Sets Colors";
  ot->idname = "SCULPT_OT_face_sets_randomize_colors";
  ot->description = "Generates a new set of random colors to render the Face Sets in the viewport";

  /* Api callbacks. */
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

static void sculpt_face_set_grow_shrink_bmesh(Object *ob,
                                              SculptSession *ss,
                                              const Array<int> prev_face_sets,
                                              const int active_face_set_id,
                                              const bool modify_hidden,
                                              bool grow)
{
  using namespace blender;

  Mesh *mesh = BKE_mesh_from_object(ob);
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  Vector<PBVHFaceRef> modified_faces;

  for (int face_i = 0; face_i < ss->totfaces; face_i++) {
    PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, face_i);

    if ((!modify_hidden && SCULPT_face_is_hidden(ss, face)) ||
        prev_face_sets[face_i] != active_face_set_id)
    {
      continue;
    }

    BMFace *f = reinterpret_cast<BMFace *>(face.i);
    BMLoop *l = f->l_first;
    BMIter iter;
    BMFace *f2;

    do {
      BM_ITER_ELEM (f2, &iter, l->v, BM_FACES_OF_VERT) {
        if (f2 == f || (!modify_hidden && BM_elem_flag_test(f2, BM_ELEM_HIDDEN))) {
          continue;
        }

        PBVHFaceRef face2 = {reinterpret_cast<intptr_t>(f2)};
        int face2_i = BKE_pbvh_face_to_index(ss->pbvh, face2);

        if (grow) {
          SCULPT_face_set_set(ss, face2, active_face_set_id);
          modified_faces.append(face2);
        }
        else if (prev_face_sets[face2_i] != active_face_set_id) {
          SCULPT_face_set_set(ss, face, prev_face_sets[face2_i]);
          modified_faces.append(face);
        }
      }
    } while ((l = l->next) != f->l_first);
  }

  for (PBVHFaceRef face : modified_faces) {
    face_mark_boundary_update(ss, face);
  }
}

static void sculpt_face_set_grow_shrink(Object &object,
                                        const EditMode mode,
                                        const int active_face_set_id,
                                        const bool modify_hidden,
                                        wmOperator *op)
{
  SculptSession &ss = *object.sculpt;
  Array<int> prev_face_sets = duplicate_face_sets(object);

  if (ss.bm) {
    sculpt_face_set_grow_shrink_bmesh(
        &object, &ss, prev_face_sets, active_face_set_id, modify_hidden, mode == EditMode::Grow);
    return;
  }

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = ss.vert_to_face_map;
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

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
  SculptSession *ss = ob->sculpt;
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan<int> face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                            bke::AttrDomain::Face);
  BMesh *bm = ss->bm;

  if (!bm) {
    const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(&mesh);
    BMeshCreateParams create_params{};
    create_params.use_toolflags = true;
    bm = BM_mesh_create(&allocsize, &create_params);

    BMeshFromMeshParams convert_params{};
    convert_params.calc_vert_normal = true;
    convert_params.calc_face_normal = true;
    BM_mesh_bm_from_me(bm, &mesh, &convert_params);
  }

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

  if (!ss->bm) {
    BMeshToMeshParams bmesh_to_mesh_params{};
    bmesh_to_mesh_params.calc_object_remap = false;
    BM_mesh_bm_to_me(nullptr, bm, &mesh, &bmesh_to_mesh_params);

    BM_mesh_free(bm);
  }
  else {
    SCULPT_pbvh_clear(ob);
  }
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
  Vector<float3> positions;
  Vector<bool> fair_verts;

  orig_positions.resize(totvert);
  fair_verts.resize(totvert);

  SCULPT_boundary_info_ensure(ob);

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    orig_positions[i] = SCULPT_vertex_co_get(ss, vertex);
    positions[i] = orig_positions[i];
    fair_verts[i] = !SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_FACE_SET) &&
                    vert_has_face_set(ss, vertex, active_face_set_id) &&
                    vert_has_unique_face_set(ss, vertex);
  }

  MutableSpan<float3> mesh_positions = SCULPT_mesh_deformed_positions_get(ss);
  BKE_mesh_prefair_and_fair_verts(mesh, positions, fair_verts.data(), fair_order);

  bool has_bmesh = ss->bm;

  for (int i = 0; i < totvert; i++) {
    if (fair_verts[i]) {
      interp_v3_v3v3(positions[i], orig_positions[i], positions[i], strength);

      if (has_bmesh) {
        BMVert *v = BM_vert_at_index(ss->bm, i);
        copy_v3_v3(v->co, positions[i]);
      }
      else {
        mesh_positions[i] = positions[i];
      }
    }
  }
}

static bool sculpt_face_set_edit_is_operation_valid(const Object &object,
                                                    const EditMode mode,
                                                    const bool modify_hidden)
{
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

  if (ob->sculpt->bm) {
    /* Load bmesh back into original mesh. */
    BKE_sculptsession_bm_to_me_for_render(ob);
  }

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

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

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

static int sculpt_face_sets_invert_visibility_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);

  /* Not supported for dyntopo. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  PBVH *pbvh = ob->sculpt->pbvh;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, {});

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  ss->hide_poly = BKE_sculpt_hide_poly_ensure(ob);

  undo::push_begin(ob, op);
  for (PBVHNode *node : nodes) {
    undo::push_node(ob, node, undo::Type::HideFace);
  }

  visibility_all_invert(ss);

  undo::push_end(ob);

  /* Sync face sets visibility and vertex visibility. */
  hide::sync_all_from_faces(*ob);

  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update_visibility(node);
  }

  bke::pbvh::update_visibility(*ss->pbvh);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_sets_invert_visibility(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Invert Face Set Visibility";
  ot->idname = "SCULPT_OT_face_set_invert_visibility";
  ot->description = "Invert the visibility of the Face Sets of the sculpt";

  /* Api callbacks. */
  ot->exec = sculpt_face_sets_invert_visibility_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
}  // namespace blender::ed::sculpt_paint::face_set
