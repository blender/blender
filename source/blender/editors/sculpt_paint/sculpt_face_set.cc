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
  auto_mask::NodeData automask_data = auto_mask::node_begin(*ob, ss->cache->automasking, *node);

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
        *fd.face_set != abs(ss->cache->automasking->settings.initial_face_set)) {

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
    face_normal_get(ss, fd.face, fno);

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
  auto_mask::NodeData automask_data = auto_mask::node_begin(*ob, ss->cache->automasking, *node);

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
    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
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

static int sculpt_face_set_create_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  const int mode = RNA_enum_get(op->ptr, "mode");

  /* Dyntopo not supported. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    return OPERATOR_CANCELLED;
  }

  Mesh *mesh = static_cast<Mesh *>(ob->data);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, mode == SCULPT_FACE_SET_MASKED);
  BKE_sculpt_face_sets_ensure(ob);

  const int tot_vert = SCULPT_vertex_count_get(ss);
  float threshold = 0.5f;

  PBVH *pbvh = ob->sculpt->pbvh;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, {});

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  undo::push_begin(ob, op);
  for (PBVHNode *node : nodes) {
    undo::push_node(ob, node, undo::Type::FaceSet);
  }

  const int next_face_set = find_next_available_id(*ob);

  if (mode == SCULPT_FACE_SET_MASKED) {
    for (int i = 0; i < tot_vert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      if (SCULPT_vertex_mask_get(ss, vertex) >= threshold && hide::vert_visible_get(ss, vertex)) {
        vert_face_set_set(ss, vertex, next_face_set);
      }
    }
  }

  if (mode == SCULPT_FACE_SET_VISIBLE) {

    /* If all vertices in the sculpt are visible, create the new face set and update the default
     * color. This way the new face set will be white, which is a quick way of disabling all face
     * sets and the performance hit of rendering the overlay. */
    bool all_visible = true;
    for (int i = 0; i < tot_vert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      if (!hide::vert_visible_get(ss, vertex)) {
        all_visible = false;
        break;
      }
    }

    if (all_visible) {
      mesh->face_sets_color_default = next_face_set;
    }

    for (int i = 0; i < tot_vert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      if (hide::vert_visible_get(ss, vertex)) {
        vert_face_set_set(ss, vertex, next_face_set);
      }
    }
  }

  if (mode == SCULPT_FACE_SET_ALL) {
    for (int i = 0; i < tot_vert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      vert_face_set_set(ss, vertex, next_face_set);
    }
  }

  if (mode == SCULPT_FACE_SET_SELECTION) {
    const bke::AttributeAccessor attributes = mesh->attributes();
    const VArraySpan<bool> select_poly = *attributes.lookup_or_default<bool>(
        ".select_poly", bke::AttrDomain::Face, false);
    threading::parallel_for(select_poly.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        if (select_poly[i]) {
          ss->face_sets[i] = next_face_set;
        }
      }
    });
  }

  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_redraw(node);
  }

  undo::push_end(ob);

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

enum eSculptFaceSetsInitMode {
  SCULPT_FACE_SETS_FROM_LOOSE_PARTS = 0,
  SCULPT_FACE_SETS_FROM_MATERIALS = 1,
  SCULPT_FACE_SETS_FROM_NORMALS = 2,
  SCULPT_FACE_SETS_FROM_UV_SEAMS = 3,
  SCULPT_FACE_SETS_FROM_CREASES = 4,
  SCULPT_FACE_SETS_FROM_SHARP_EDGES = 5,
  SCULPT_FACE_SETS_FROM_BEVEL_WEIGHT = 6,
  SCULPT_FACE_SETS_FROM_FACE_SET_BOUNDARIES = 8,
};

static EnumPropertyItem prop_sculpt_face_sets_init_types[] = {
    {
        SCULPT_FACE_SETS_FROM_LOOSE_PARTS,
        "LOOSE_PARTS",
        0,
        "Face Sets from Loose Parts",
        "Create a Face Set per loose part in the mesh",
    },
    {
        SCULPT_FACE_SETS_FROM_MATERIALS,
        "MATERIALS",
        0,
        "Face Sets from Material Slots",
        "Create a Face Set per Material Slot",
    },
    {
        SCULPT_FACE_SETS_FROM_NORMALS,
        "NORMALS",
        0,
        "Face Sets from Mesh Normals",
        "Create Face Sets for Faces that have similar normal",
    },
    {
        SCULPT_FACE_SETS_FROM_UV_SEAMS,
        "UV_SEAMS",
        0,
        "Face Sets from UV Seams",
        "Create Face Sets using UV Seams as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_CREASES,
        "CREASES",
        0,
        "Face Sets from Edge Creases",
        "Create Face Sets using Edge Creases as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_BEVEL_WEIGHT,
        "BEVEL_WEIGHT",
        0,
        "Face Sets from Bevel Weight",
        "Create Face Sets using Bevel Weights as boundaries",
    },
    {
        SCULPT_FACE_SETS_FROM_SHARP_EDGES,
        "SHARP_EDGES",
        0,
        "Face Sets from Sharp Edges",
        "Create Face Sets using Sharp Edges as boundaries",
    },

    {
        SCULPT_FACE_SETS_FROM_FACE_SET_BOUNDARIES,
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

  if (ss->epmap.is_empty()) {
    ss->epmap = bke::mesh::build_edge_to_face_map(
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
        for (const int neighbor_i : ss->epmap[edge_i]) {
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

static void sculpt_face_sets_init_loop(Object *ob, const int mode)
{
  using namespace blender;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  SculptSession *ss = ob->sculpt;

  if (mode == SCULPT_FACE_SETS_FROM_MATERIALS) {
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

  const int mode = RNA_enum_get(op->ptr, "mode");

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

    if (!ss->epmap.is_empty()) {
      ss->epmap = {};
      ss->edge_to_face_indices = {};
      ss->edge_to_face_offsets = {};
    }

    if (!ss->pmap.is_empty()) {
      ss->pmap = {};
    }
  }

  const bke::AttributeAccessor attributes = mesh->attributes();
  switch (mode) {
    case SCULPT_FACE_SETS_FROM_LOOSE_PARTS: {
      const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
          ".hide_poly", bke::AttrDomain::Face, false);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int from_face, const int /*edge*/, const int to_face) {
            return hide_poly[from_face] == hide_poly[to_face];
          });
      break;
    }
    case SCULPT_FACE_SETS_FROM_MATERIALS: {
      sculpt_face_sets_init_loop(ob, SCULPT_FACE_SETS_FROM_MATERIALS);
      break;
    }
    case SCULPT_FACE_SETS_FROM_NORMALS: {
      const Span<float3> face_normals = mesh->face_normals();
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int from_face, const int /*edge*/, const int to_face) -> bool {
            return std::abs(math::dot(face_normals[from_face], face_normals[to_face])) > threshold;
          });
      break;
    }
    case SCULPT_FACE_SETS_FROM_UV_SEAMS: {
      const VArraySpan<bool> uv_seams = *mesh->attributes().lookup_or_default<bool>(
          ".uv_seam", bke::AttrDomain::Edge, false);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return !uv_seams[edge];
          });
      break;
    }
    case SCULPT_FACE_SETS_FROM_CREASES: {
      const float *creases = static_cast<const float *>(
          CustomData_get_layer_named(&mesh->edge_data, CD_PROP_FLOAT, "crease_edge"));
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return creases ? creases[edge] < threshold : true;
          });
      break;
    }
    case SCULPT_FACE_SETS_FROM_SHARP_EDGES: {
      const VArraySpan<bool> sharp_edges = *mesh->attributes().lookup_or_default<bool>(
          "sharp_edge", bke::AttrDomain::Edge, false);
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return !sharp_edges[edge];
          });
      break;
    }
    case SCULPT_FACE_SETS_FROM_BEVEL_WEIGHT: {
      const float *bevel_weights = static_cast<const float *>(
          CustomData_get_layer_named(&mesh->edge_data, CD_PROP_FLOAT, "bevel_weight_edge"));
      sculpt_face_sets_init_flood_fill(
          ob, [&](const int /*from_face*/, const int edge, const int /*to_face*/) -> bool {
            return bevel_weights ? bevel_weights[edge] < threshold : true;
          });
      break;
    }
    case SCULPT_FACE_SETS_FROM_FACE_SET_BOUNDARIES: {
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

enum eSculptFaceGroupVisibilityModes {
  SCULPT_FACE_SET_VISIBILITY_TOGGLE = 0,
  SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE = 1,
  SCULPT_FACE_SET_VISIBILITY_HIDE_ACTIVE = 2,
};

static EnumPropertyItem prop_sculpt_face_sets_change_visibility_types[] = {
    {
        SCULPT_FACE_SET_VISIBILITY_TOGGLE,
        "TOGGLE",
        0,
        "Toggle Visibility",
        "Hide all Face Sets except for the active one",
    },
    {
        SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE,
        "SHOW_ACTIVE",
        0,
        "Show Active Face Set",
        "Show Active Face Set",
    },
    {
        SCULPT_FACE_SET_VISIBILITY_HIDE_ACTIVE,
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

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  const int mode = RNA_enum_get(op->ptr, "mode");
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
    case SCULPT_FACE_SET_VISIBILITY_TOGGLE: {
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
    case SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE:
      BKE_sculpt_hide_poly_ensure(ob);

      if (ss->attrs.face_set) {
        face_set::visibility_all_set(ob, false);
        hide::face_set(ss, active_face_set, true);
      }
      else {
        hide::face_set(ss, active_face_set, true);
      }
      break;
    case SCULPT_FACE_SET_VISIBILITY_HIDE_ACTIVE:
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
  if (ELEM(mode, SCULPT_FACE_SET_VISIBILITY_TOGGLE, SCULPT_FACE_SET_VISIBILITY_SHOW_ACTIVE)) {
    UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
    float location[3];
    copy_v3_v3(location, SCULPT_active_vertex_co_get(ss));
    mul_m4_v3(ob->object_to_world, location);
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
               SCULPT_FACE_SET_VISIBILITY_TOGGLE,
               "Mode",
               "");
}

static int sculpt_face_sets_randomize_colors_exec(bContext *C, wmOperator * /*op*/)
{

  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

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

enum eSculptFaceSetEditMode {
  SCULPT_FACE_SET_EDIT_GROW = 0,
  SCULPT_FACE_SET_EDIT_SHRINK = 1,
  SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY = 2,
  SCULPT_FACE_SET_EDIT_FAIR_POSITIONS = 3,
  SCULPT_FACE_SET_EDIT_FAIR_TANGENCY = 4,
};

static EnumPropertyItem prop_sculpt_face_sets_edit_types[] = {
    {
        SCULPT_FACE_SET_EDIT_GROW,
        "GROW",
        0,
        "Grow Face Set",
        "Grows the Face Sets boundary by one face based on mesh topology",
    },
    {
        SCULPT_FACE_SET_EDIT_SHRINK,
        "SHRINK",
        0,
        "Shrink Face Set",
        "Shrinks the Face Sets boundary by one face based on mesh topology",
    },
    {
        SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY,
        "DELETE_GEOMETRY",
        0,
        "Delete Geometry",
        "Deletes the faces that are assigned to the Face Set",
    },
    {
        SCULPT_FACE_SET_EDIT_FAIR_POSITIONS,
        "FAIR_POSITIONS",
        0,
        "Fair Positions",
        "Creates a smooth as possible geometry patch from the Face Set minimizing changes in "
        "vertex positions",
    },
    {
        SCULPT_FACE_SET_EDIT_FAIR_TANGENCY,
        "FAIR_TANGENCY",
        0,
        "Fair Tangency",
        "Creates a smooth as possible geometry patch from the Face Set minimizing changes in "
        "vertex tangents",
    },
    {0, nullptr, 0, nullptr, nullptr},
};

static void sculpt_face_set_grow_shrink(Object *ob,
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

    if (ss->bm) {
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
    else {  //
      for (const int vert_i : corner_verts.slice(faces[face_i])) {
        const Span<int> vert_map = ss->pmap[vert_i];
        for (int i : vert_map.index_range()) {
          const int neighbor_face_index = vert_map[i];
          if (neighbor_face_index == face_i) {
            continue;
          }

          if (grow) {
            ss->face_sets[neighbor_face_index] = active_face_set_id;
            modified_faces.append(BKE_pbvh_index_to_face(ss->pbvh, neighbor_face_index));
          }
          else if (prev_face_sets[neighbor_face_index] != active_face_set_id) {
            ss->face_sets[face_i] = prev_face_sets[neighbor_face_index];
            modified_faces.append(face);
          }
        }
      }
    }
  }

  for (PBVHFaceRef face : modified_faces) {
    face_mark_boundary_update(ss, face);
  }
}

static bool check_single_face_set(SculptSession *ss, const bool check_visible_only)
{
  int first_face_set = SCULPT_FACE_SET_NONE;
  if (check_visible_only) {
    for (int f = 0; f < ss->totfaces; f++) {
      PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, f);
      if (SCULPT_face_is_hidden(ss, face)) {
        continue;
      }

      first_face_set = SCULPT_face_set_get(ss, face);
      break;
    }
  }
  else if (ss->totfaces > 0) {
    PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, 0);
    first_face_set = SCULPT_face_set_get(ss, face);
  }
  else {
    first_face_set = SCULPT_FACE_SET_NONE;
  }

  if (first_face_set == SCULPT_FACE_SET_NONE) {
    return true;
  }

  for (int f = 0; f < ss->totfaces; f++) {
    PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, f);

    if (check_visible_only && SCULPT_face_is_hidden(ss, face)) {
      continue;
    }
    if (SCULPT_face_set_get(ss, face) != first_face_set) {
      return false;
    }
  }
  return true;
}

/* Deletes geometry without destroying the underlying PBVH. */
static void sculpt_face_set_delete_geometry_bmesh(Object *ob, BMesh *bm)
{
  SculptSession *ss = ob->sculpt;
  BMIter iter;

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr);
  for (PBVHNode *node : nodes) {
    /* Only need to do this once. */
    undo::ensure_dyntopo_node_undo(ob, node, undo::Type::None);
    break;
  }

  /* Tag verts/edges for deletion. */
  BMFace *f;
  BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
      continue;
    }

    BMLoop *l = f->l_first;
    do {
      BM_elem_flag_enable(l->v, BM_ELEM_TAG);
      BM_elem_flag_enable(l->e, BM_ELEM_TAG);
    } while ((l = l->next) != f->l_first);
  }

  /* Untag any shared verts/edges we want to keep. */
  BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
      continue;
    }

    BMLoop *l = f->l_first;
    do {
      BM_elem_flag_disable(l->v, BM_ELEM_TAG);
      BM_elem_flag_disable(l->e, BM_ELEM_TAG);
    } while ((l = l->next) != f->l_first);
  }

  BMVert *v;
  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      BKE_pbvh_bmesh_remove_vertex(ss->pbvh, v, false);
    }
  }

  BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
      continue;
    }

    BKE_pbvh_bmesh_remove_face(ss->pbvh, f, true);

    BM_idmap_release(ss->bm_idmap, reinterpret_cast<BMElem *>(f), true);
    BM_face_kill(bm, f);
  }

  BMEdge *e;
  BM_ITER_MESH (e, &iter, ss->bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
      BM_log_edge_removed(ss->bm, ss->bm_log, e);
      BM_idmap_release(ss->bm_idmap, reinterpret_cast<BMElem *>(e), true);
      BM_edge_kill(bm, e);
    }
  }

  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      BM_log_vert_removed(ss->bm, ss->bm_log, v);
      BM_idmap_release(ss->bm_idmap, reinterpret_cast<BMElem *>(v), true);
      BM_vert_kill(bm, v);
    }
  }

  ss->totfaces = bm->totface;
  ss->totvert = bm->totvert;

  blender::bke::dyntopo::after_stroke(ss->pbvh, true);
  bke::pbvh::update_bounds(*ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB);
}

static void sculpt_face_set_delete_geometry(Object *ob,
                                            SculptSession *ss,
                                            const int active_face_set_id,
                                            const bool modify_hidden)
{

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  BMesh *bm;

  if (!ss->bm) {
    const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);
    BMeshCreateParams create_params{};

    bm = BM_mesh_create(&allocsize, &create_params);

    BMeshFromMeshParams convert_params{};
    convert_params.calc_vert_normal = true;
    convert_params.calc_face_normal = true;

    BM_mesh_bm_from_me(bm, mesh, &convert_params);
  }
  else {
    bm = ss->bm;
  }

  int cd_fset_offset = CustomData_get_offset_named(
      &bm->pdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(face_set));

  if (cd_fset_offset == -1) {
    return;
  }

  BM_mesh_elem_table_init(bm, BM_FACE);
  BM_mesh_elem_table_ensure(bm, BM_FACE);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (!modify_hidden && BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    int fset = BM_ELEM_CD_GET_INT(f, cd_fset_offset);
    BM_elem_flag_set(f, BM_ELEM_TAG, fset == active_face_set_id);
  }

  if (ss->bm) {
    sculpt_face_set_delete_geometry_bmesh(ob, bm);
  }
  else {
    BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_FACES);
  }

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  if (!ss->bm) {
    BMeshToMeshParams bmesh_to_mesh_params{};
    bmesh_to_mesh_params.calc_object_remap = false;
    BM_mesh_bm_to_me(nullptr, bm, mesh, &bmesh_to_mesh_params);

    BM_mesh_free(bm);
  }

  BKE_sculptsession_update_attr_refs(ob);
  SCULPT_update_all_valence_boundary(ob);
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
  SCULPT_vertex_random_access_ensure(ss);

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    orig_positions[i] = SCULPT_vertex_co_get(ss, vertex);
    fair_verts[i] = !SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_MESH) &&
                    vert_has_face_set(ss, vertex, active_face_set_id) &&
                    vert_has_unique_face_set(ss, vertex);
  }

  blender::MutableSpan<float3> positions;

  if (ss->bm) {
    BKE_bmesh_prefair_and_fair_verts(ss->bm, fair_verts.data(), fair_order);
  }
  else {
    positions = SCULPT_mesh_deformed_positions_get(ss);
    BKE_mesh_prefair_and_fair_verts(mesh, positions, fair_verts.data(), fair_order);
  }

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
    float *co = ss->bm ? reinterpret_cast<BMVert *>(vertex.i)->co : &positions[i][0];

    if (fair_verts[i]) {
      interp_v3_v3v3(co, orig_positions[i], co, strength);
    }
  }
}

static Array<int> save_face_sets(SculptSession *ss)
{
  Array<int> prev_face_sets(ss->totfaces);

  for (int i = 0; i < ss->totfaces; i++) {
    PBVHFaceRef face = BKE_pbvh_index_to_face(ss->pbvh, i);

    prev_face_sets[i] = ss->attrs.face_set ? SCULPT_face_set_get(ss, face) : 0;
  }

  return prev_face_sets;
}

static void sculpt_face_set_apply_edit(Object *ob,
                                       const int active_face_set_id,
                                       const int mode,
                                       const bool modify_hidden,
                                       const float strength = 0.0f)
{
  SculptSession *ss = ob->sculpt;

  switch (mode) {
    case SCULPT_FACE_SET_EDIT_GROW: {
      sculpt_face_set_grow_shrink(
          ob, ss, save_face_sets(ss), active_face_set_id, modify_hidden, true);
      break;
    }
    case SCULPT_FACE_SET_EDIT_SHRINK: {
      sculpt_face_set_grow_shrink(
          ob, ss, save_face_sets(ss), active_face_set_id, modify_hidden, false);
      break;
    }
    case SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY:
      sculpt_face_set_delete_geometry(ob, ss, active_face_set_id, modify_hidden);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_POSITIONS:
      sculpt_face_set_edit_fair_face_set(
          ob, active_face_set_id, MESH_FAIRING_DEPTH_POSITION, strength);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_TANGENCY:
      sculpt_face_set_edit_fair_face_set(
          ob, active_face_set_id, MESH_FAIRING_DEPTH_TANGENCY, strength);
      break;
  }
}

static bool sculpt_face_set_edit_is_operation_valid(SculptSession *ss,
                                                    const eSculptFaceSetEditMode mode,
                                                    const bool modify_hidden)
{
  if (mode == SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY) {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
      /* Modification of base mesh geometry requires special remapping of multi-resolution
       * displacement, which does not happen here.
       * Disable delete operation. It can be supported in the future by doing similar
       * displacement data remapping as what happens in the mesh edit mode. */
      return false;
    }
    if (check_single_face_set(ss, !modify_hidden)) {
      /* Cancel the operator if the mesh only contains one Face Set to avoid deleting the
       * entire object. */
      return false;
    }
  }

  if (ELEM(mode, SCULPT_FACE_SET_EDIT_FAIR_POSITIONS, SCULPT_FACE_SET_EDIT_FAIR_TANGENCY)) {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
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
                                                 const eSculptFaceSetEditMode mode,
                                                 const bool modify_hidden,
                                                 wmOperator *op)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  if (!ss->bm) {
    undo::geometry_begin(ob, op);
  }
  else {
    undo::push_begin(ob, op);
  }

  sculpt_face_set_apply_edit(ob, active_face_set, mode, modify_hidden);

  if (!ss->bm) {
    undo::geometry_end(ob);
  }
  else {
    undo::push_end(ob);
  }

  if (BKE_pbvh_type(ob->sculpt->pbvh) != PBVH_BMESH) {
    BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);
  }
}

static void face_set_edit_do_post_visibility_updates(Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;

  /* Sync face sets visibility and vertex visibility as now all Face Sets are visible. */
  hide::sync_all_from_faces(*ob);

  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update_visibility(node);
  }

  bke::pbvh::update_visibility(*ss->pbvh);
}

static void sculpt_face_set_edit_modify_face_sets(Object *ob,
                                                  const int active_face_set,
                                                  const eSculptFaceSetEditMode mode,
                                                  const bool modify_hidden,
                                                  wmOperator *op)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, {});

  if (nodes.is_empty()) {
    return;
  }
  undo::push_begin(ob, op);
  for (PBVHNode *node : nodes) {
    undo::push_node(ob, node, undo::Type::FaceSet);
  }
  sculpt_face_set_apply_edit(ob, active_face_set, mode, modify_hidden);
  undo::push_end(ob);
  face_set_edit_do_post_visibility_updates(ob, nodes);
}

static void sculpt_face_set_edit_modify_coordinates(bContext *C,
                                                    Object *ob,
                                                    const int active_face_set,
                                                    const eSculptFaceSetEditMode mode,
                                                    wmOperator *op)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, {});

  const float strength = RNA_float_get(op->ptr, "strength");

  undo::push_begin(ob, op);
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_mark_update(node);
    undo::push_node(ob, node, undo::Type::Position);
  }
  sculpt_face_set_apply_edit(ob, active_face_set, mode, false, strength);

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
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const eSculptFaceSetEditMode mode = static_cast<eSculptFaceSetEditMode>(
      RNA_enum_get(op->ptr, "mode"));
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  if (!sculpt_face_set_edit_is_operation_valid(ss, mode, modify_hidden)) {
    return false;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
  BKE_sculpt_face_sets_ensure(ob);

  return true;
}

static int sculpt_face_set_edit_exec(bContext *C, wmOperator *op)
{
  if (!sculpt_face_set_edit_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  Object *ob = CTX_data_active_object(C);

  SCULPT_vertex_random_access_ensure(ob->sculpt);
  SCULPT_face_random_access_ensure(ob->sculpt);

  const int active_face_set = RNA_int_get(op->ptr, "active_face_set");
  const eSculptFaceSetEditMode mode = static_cast<eSculptFaceSetEditMode>(
      RNA_enum_get(op->ptr, "mode"));
  const bool modify_hidden = RNA_boolean_get(op->ptr, "modify_hidden");

  switch (mode) {
    case SCULPT_FACE_SET_EDIT_DELETE_GEOMETRY:
      sculpt_face_set_edit_modify_geometry(C, ob, active_face_set, mode, modify_hidden, op);
      break;
    case SCULPT_FACE_SET_EDIT_GROW:
    case SCULPT_FACE_SET_EDIT_SHRINK:
      sculpt_face_set_edit_modify_face_sets(ob, active_face_set, mode, modify_hidden, op);
      break;
    case SCULPT_FACE_SET_EDIT_FAIR_POSITIONS:
    case SCULPT_FACE_SET_EDIT_FAIR_TANGENCY:
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
  /* Identifiers. */
  ot->name = "Edit Face Set";
  ot->idname = "SCULPT_OT_face_set_edit";
  ot->description = "Edits the current active Face Set";

  /* Api callbacks. */
  ot->invoke = sculpt_face_set_edit_invoke;
  ot->exec = sculpt_face_set_edit_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  PropertyRNA *prop = RNA_def_int(
      ot->srna, "active_face_set", 1, 0, INT_MAX, "Active Face Set", "", 0, 64);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_sets_edit_types, SCULPT_FACE_SET_EDIT_GROW, "Mode", "");
  RNA_def_float(ot->srna, "strength", 1.0f, 0.0f, 1.0f, "Strength", "", 0.0f, 1.0f);

  ot->prop = RNA_def_boolean(ot->srna,
                             "modify_hidden",
                             true,
                             "Modify Hidden",
                             "Apply the edit operation to hidden Face Sets");
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
