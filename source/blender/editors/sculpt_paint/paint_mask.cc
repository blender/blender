/* SPDX-FileCopyrightText: 2012 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_vec_types.h"

#include "BLI_array_utils.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_lasso_2d.h"
#include "BLI_math_base.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_polyfill_2d.h"
#include "BLI_rect.h"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_ccg.h"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_sculpt.hh"
#include "ED_view3d.hh"

#include "bmesh.hh"
#include "tools/bmesh_boolean.hh"

#include "paint_intern.hh"

/* For undo push. */
#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint::mask {

Array<float> duplicate_mask(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  switch (BKE_pbvh_type(ss.pbvh)) {
    case PBVH_FACES: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArray mask = *attributes.lookup_or_default<float>(
          ".sculpt_mask", bke::AttrDomain::Point, 0.0f);
      Array<float> result(mask.size());
      mask.materialize(result);
      return result;
    }
    case PBVH_GRIDS: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const Span<CCGElem *> grids = subdiv_ccg.grids;

      Array<float> result(grids.size() * key.grid_area);
      int index = 0;
      for (const int grid : grids.index_range()) {
        CCGElem *elem = grids[grid];
        for (const int i : IndexRange(key.grid_area)) {
          result[index] = *CCG_elem_offset_mask(&key, elem, i);
          index++;
        }
      }
      return result;
    }
    case PBVH_BMESH: {
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

/* The gesture API doesn't write to this enum type,
 * it writes to eSelectOp from ED_select_utils.hh.
 * We must thus map the modes here to the desired
 * eSelectOp modes.
 *
 * Fixes #102349.
 */
enum PaintMaskFloodMode {
  PAINT_MASK_FLOOD_VALUE = SEL_OP_SUB,
  PAINT_MASK_FLOOD_VALUE_INVERSE = SEL_OP_ADD,
  PAINT_MASK_INVERT = SEL_OP_XOR,
};

static const EnumPropertyItem mode_items[] = {
    {PAINT_MASK_FLOOD_VALUE,
     "VALUE",
     0,
     "Value",
     "Set mask to the level specified by the 'value' property"},
    {PAINT_MASK_FLOOD_VALUE_INVERSE,
     "VALUE_INVERSE",
     0,
     "Value Inverted",
     "Set mask to the level specified by the inverted 'value' property"},
    {PAINT_MASK_INVERT, "INVERT", 0, "Invert", "Invert the mask"},
    {0}};

static float mask_flood_fill_get_new_value_for_elem(const float elem,
                                                    PaintMaskFloodMode mode,
                                                    float value)
{
  switch (mode) {
    case PAINT_MASK_FLOOD_VALUE:
      return value;
    case PAINT_MASK_FLOOD_VALUE_INVERSE:
      return 1.0f - value;
    case PAINT_MASK_INVERT:
      return 1.0f - elem;
  }
  BLI_assert_unreachable();
  return 0.0f;
}

static Span<int> get_visible_verts(const PBVHNode &node,
                                   const Span<bool> hide_vert,
                                   Vector<int> &indices)
{
  if (BKE_pbvh_node_fully_hidden_get(&node)) {
    return {};
  }
  const Span<int> verts = BKE_pbvh_node_get_unique_vert_indices(&node);
  if (hide_vert.is_empty()) {
    return verts;
  }
  indices.resize(verts.size());
  const int *end = std::copy_if(verts.begin(), verts.end(), indices.begin(), [&](const int vert) {
    return !hide_vert[vert];
  });
  indices.resize(end - indices.begin());
  return indices;
}

static Span<int> get_hidden_verts(const PBVHNode &node,
                                  const Span<bool> hide_vert,
                                  Vector<int> &indices)
{
  if (hide_vert.is_empty()) {
    return {};
  }
  const Span<int> verts = BKE_pbvh_node_get_unique_vert_indices(&node);
  if (BKE_pbvh_node_fully_hidden_get(&node)) {
    return verts;
  }
  indices.resize(verts.size());
  const int *end = std::copy_if(verts.begin(), verts.end(), indices.begin(), [&](const int vert) {
    return hide_vert[vert];
  });
  indices.resize(end - indices.begin());
  return indices;
}

static bool try_remove_mask_mesh(Object &object, const Span<PBVHNode *> nodes)
{
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
      nodes.index_range(),
      1,
      false,
      [&](const IndexRange range, bool init) {
        if (init) {
          return init;
        }
        Vector<int> &index_data = all_index_data.local();
        for (const PBVHNode *node : nodes.slice(range)) {
          const Span<int> verts = get_hidden_verts(*node, hide_vert, index_data);
          if (std::any_of(verts.begin(), verts.end(), [&](int i) { return mask[i] > 0.0f; })) {
            return true;
          }
        }
        return false;
      },
      std::logical_or());
  if (hidden_masked_verts) {
    return false;
  }

  /* Store undo data for nodes with changed mask. */
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> verts = BKE_pbvh_node_get_unique_vert_indices(node);
      if (std::all_of(verts.begin(), verts.end(), [&](const int i) { return mask[i] == 0.0f; })) {
        continue;
      }
      undo::push_node(&object, node, undo::Type::Mask);
      BKE_pbvh_node_mark_redraw(node);
    }
  });

  attributes.remove(".sculpt_mask");
  return true;
}

static void fill_mask_mesh(Object &object, const float value, const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  if (value == 0.0f) {
    if (try_remove_mask_mesh(object, nodes)) {
      return;
    }
  }

  bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_only_span<float>(
      ".sculpt_mask", bke::AttrDomain::Point);

  threading::EnumerableThreadSpecific<Vector<int>> all_index_data;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    Vector<int> &index_data = all_index_data.local();
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> verts = get_visible_verts(*node, hide_vert, index_data);
      if (std::all_of(verts.begin(), verts.end(), [&](int i) { return mask.span[i] == value; })) {
        continue;
      }
      undo::push_node(&object, node, undo::Type::Mask);
      mask.span.fill_indices(verts, value);
      BKE_pbvh_node_mark_redraw(node);
    }
  });

  mask.finish();
}

static void fill_mask_grids(Main &bmain,
                            const Scene &scene,
                            Depsgraph &depsgraph,
                            Object &object,
                            const float value,
                            const Span<PBVHNode *> nodes)
{
  SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  if (value == 0.0f && !key.has_mask) {
    /* Unlike meshes, don't dynamically remove masks since it is interleaved with other data. */
    return;
  }

  MultiresModifierData &mmd = *BKE_sculpt_multires_active(&scene, &object);
  BKE_sculpt_mask_layers_ensure(&depsgraph, &bmain, &object, &mmd);

  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;

  const Span<CCGElem *> grids = subdiv_ccg.grids;
  bool any_changed = false;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> grid_indices = BKE_pbvh_node_get_grid_indices(*node);
      if (std::all_of(grid_indices.begin(), grid_indices.end(), [&](const int grid) {
            CCGElem *elem = grids[grid];
            for (const int i : IndexRange(key.grid_area)) {
              if (*CCG_elem_offset_mask(&key, elem, i) != value) {
                return false;
              }
            }
            return true;
          }))
      {
        continue;
      }
      undo::push_node(&object, node, undo::Type::Mask);

      if (grid_hidden.is_empty()) {
        for (const int grid : grid_indices) {
          CCGElem *elem = grids[grid];
          for (const int i : IndexRange(key.grid_area)) {
            *CCG_elem_offset_mask(&key, elem, i) = value;
          }
        }
      }
      else {
        for (const int grid : grid_indices) {
          CCGElem *elem = grids[grid];
          bits::foreach_0_index(grid_hidden[grid], [&](const int i) {
            *CCG_elem_offset_mask(&key, elem, i) = value;
          });
        }
      }
      BKE_pbvh_node_mark_redraw(node);
      any_changed = true;
    }
  });

  if (any_changed) {
    multires_mark_as_modified(&depsgraph, &object, MULTIRES_COORDS_MODIFIED);
  }
}

static void fill_mask_bmesh(Object &object, const float value, const Span<PBVHNode *> nodes)
{
  BMesh &bm = *object.sculpt->bm;
  const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  if (value == 0.0f && offset == -1) {
    return;
  }
  if (offset == -1) {
    /* Mask is not dynamically added or removed for dynamic topology sculpting. */
    BLI_assert_unreachable();
    return;
  }

  undo::push_node(&object, nodes.first(), undo::Type::Mask);
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      bool redraw = false;
      for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node)) {
        if (!BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
          if (BM_ELEM_CD_GET_FLOAT(vert, offset) != value) {
            BM_ELEM_CD_SET_FLOAT(vert, offset, value);
            redraw = true;
          }
        }
      }
      if (redraw) {
        BKE_pbvh_node_mark_redraw(node);
      }
    }
  });
}

static void fill_mask(
    Main &bmain, const Scene &scene, Depsgraph &depsgraph, Object &object, const float value)
{
  PBVH &pbvh = *object.sculpt->pbvh;
  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(&pbvh, {});
  switch (BKE_pbvh_type(&pbvh)) {
    case PBVH_FACES:
      fill_mask_mesh(object, value, nodes);
      break;
    case PBVH_GRIDS:
      fill_mask_grids(bmain, scene, depsgraph, object, value, nodes);
      break;
    case PBVH_BMESH:
      fill_mask_bmesh(object, value, nodes);
      break;
  }
  /* Avoid calling #BKE_pbvh_node_mark_update_mask by doing that update here. */
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_fully_masked_set(node, value == 1.0f);
    BKE_pbvh_node_fully_unmasked_set(node, value == 0.0f);
  }
}

static void invert_mask_mesh(Object &object, const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();

  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
      ".sculpt_mask", bke::AttrDomain::Point);
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      undo::push_node(&object, node, undo::Type::Mask);
      for (const int vert : BKE_pbvh_node_get_unique_vert_indices(node)) {
        if (!hide_vert.is_empty() && hide_vert[vert]) {
          continue;
        }
        mask.span[vert] = 1.0f - mask.span[vert];
      }
      BKE_pbvh_node_mark_redraw(node);
      bke::pbvh::node_update_mask_mesh(mask.span, *node);
    }
  });
  mask.finish();
}

static void invert_mask_grids(Main &bmain,
                              const Scene &scene,
                              Depsgraph &depsgraph,
                              Object &object,
                              const Span<PBVHNode *> nodes)
{
  SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;

  MultiresModifierData &mmd = *BKE_sculpt_multires_active(&scene, &object);
  BKE_sculpt_mask_layers_ensure(&depsgraph, &bmain, &object, &mmd);

  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      undo::push_node(&object, node, undo::Type::Mask);

      const Span<int> grid_indices = BKE_pbvh_node_get_grid_indices(*node);
      if (grid_hidden.is_empty()) {
        for (const int grid : grid_indices) {
          CCGElem *elem = grids[grid];
          for (const int i : IndexRange(key.grid_area)) {
            *CCG_elem_offset_mask(&key, elem, i) = 1.0f - *CCG_elem_offset_mask(&key, elem, i);
          }
        }
      }
      else {
        for (const int grid : grid_indices) {
          CCGElem *elem = grids[grid];
          bits::foreach_0_index(grid_hidden[grid], [&](const int i) {
            *CCG_elem_offset_mask(&key, elem, i) = 1.0f - *CCG_elem_offset_mask(&key, elem, i);
          });
        }
      }
      BKE_pbvh_node_mark_update_mask(node);
      bke::pbvh::node_update_mask_grids(key, grids, *node);
    }
  });

  multires_mark_as_modified(&depsgraph, &object, MULTIRES_COORDS_MODIFIED);
}

static void invert_mask_bmesh(Object &object, const Span<PBVHNode *> nodes)
{
  BMesh &bm = *object.sculpt->bm;
  const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  if (offset == -1) {
    BLI_assert_unreachable();
    return;
  }

  undo::push_node(&object, nodes.first(), undo::Type::Mask);
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node)) {
        if (!BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
          BM_ELEM_CD_SET_FLOAT(vert, offset, 1.0f - BM_ELEM_CD_GET_FLOAT(vert, offset));
        }
      }
      BKE_pbvh_node_mark_update_mask(node);
      bke::pbvh::node_update_mask_bmesh(offset, *node);
    }
  });
}

static void invert_mask(Main &bmain, const Scene &scene, Depsgraph &depsgraph, Object &object)
{
  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(object.sculpt->pbvh, {});
  switch (BKE_pbvh_type(object.sculpt->pbvh)) {
    case PBVH_FACES:
      invert_mask_mesh(object, nodes);
      break;
    case PBVH_GRIDS:
      invert_mask_grids(bmain, scene, depsgraph, object, nodes);
      break;
    case PBVH_BMESH:
      invert_mask_bmesh(object, nodes);
      break;
  }
}

static int mask_flood_fill_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  Depsgraph &depsgraph = *CTX_data_ensure_evaluated_depsgraph(C);

  const PaintMaskFloodMode mode = PaintMaskFloodMode(RNA_enum_get(op->ptr, "mode"));
  const float value = RNA_float_get(op->ptr, "value");

  BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);

  undo::push_begin(&object, op);
  switch (mode) {
    case PAINT_MASK_FLOOD_VALUE:
      fill_mask(bmain, scene, depsgraph, object, value);
      break;
    case PAINT_MASK_FLOOD_VALUE_INVERSE:
      fill_mask(bmain, scene, depsgraph, object, 1.0f - value);
      break;
    case PAINT_MASK_INVERT:
      invert_mask(bmain, scene, depsgraph, object);
      break;
  }

  undo::push_end(&object);

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
  RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", nullptr);
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

/* Sculpt Gesture Operators. */

enum eSculptGestureShapeType {
  SCULPT_GESTURE_SHAPE_BOX,
  SCULPT_GESTURE_SHAPE_LASSO,
  SCULPT_GESTURE_SHAPE_LINE,
};

struct LassoGestureData {
  float4x4 projviewobjmat;

  rcti boundbox;
  int width;

  /* 2D bitmap to test if a vertex is affected by the lasso shape. */
  blender::BitVector<> mask_px;
};

struct LineGestureData {
  /* Plane aligned to the gesture line. */
  float true_plane[4];
  float plane[4];

  /* Planes to limit the action to the length of the gesture segment at both sides of the affected
   * area. */
  float side_plane[2][4];
  float true_side_plane[2][4];
  bool use_side_planes;

  bool flip;
};

struct SculptGestureOperation;

struct SculptGestureContext {
  SculptSession *ss;
  ViewContext vc;

  /* Enabled and currently active symmetry. */
  ePaintSymmetryFlags symm;
  ePaintSymmetryFlags symmpass;

  /* Operation parameters. */
  eSculptGestureShapeType shape_type;
  bool front_faces_only;

  SculptGestureOperation *operation;

  /* Gesture data. */
  /* Screen space points that represent the gesture shape. */
  float (*gesture_points)[2];
  int tot_gesture_points;

  /* View parameters. */
  float3 true_view_normal;
  float3 view_normal;

  float3 true_view_origin;
  float3 view_origin;

  float true_clip_planes[4][4];
  float clip_planes[4][4];

  /* These store the view origin and normal in world space, which is used in some gestures to
   * generate geometry aligned from the view directly in world space. */
  /* World space view origin and normal are not affected by object symmetry when doing symmetry
   * passes, so there is no separate variables with the `true_` prefix to store their original
   * values without symmetry modifications. */
  float3 world_space_view_origin;
  float3 world_space_view_normal;

  /* Lasso Gesture. */
  LassoGestureData lasso;

  /* Line Gesture. */
  LineGestureData line;

  /* Task Callback Data. */
  Vector<PBVHNode *> nodes;
};

struct SculptGestureOperation {
  /* Initial setup (data updates, special undo push...). */
  void (*sculpt_gesture_begin)(bContext *, SculptGestureContext *);

  /* Apply the gesture action for each symmetry pass. */
  void (*sculpt_gesture_apply_for_symmetry_pass)(bContext *, SculptGestureContext *);

  /* Remaining actions after finishing the symmetry passes iterations
   * (updating data-layers, tagging PBVH updates...). */
  void (*sculpt_gesture_end)(bContext *, SculptGestureContext *);
};

static void sculpt_gesture_operator_properties(wmOperatorType *ot)
{
  RNA_def_boolean(ot->srna,
                  "use_front_faces_only",
                  false,
                  "Front Faces Only",
                  "Affect only faces facing towards the view");

  RNA_def_boolean(ot->srna,
                  "use_limit_to_segment",
                  false,
                  "Limit to Segment",
                  "Apply the gesture action only to the area that is contained within the "
                  "segment without extending its effect to the entire line");
}

static void sculpt_gesture_context_init_common(bContext *C,
                                               wmOperator *op,
                                               SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  sgcontext->vc = ED_view3d_viewcontext_init(C, depsgraph);
  Object *ob = sgcontext->vc.obact;

  /* Operator properties. */
  sgcontext->front_faces_only = RNA_boolean_get(op->ptr, "use_front_faces_only");
  sgcontext->line.use_side_planes = RNA_boolean_get(op->ptr, "use_limit_to_segment");

  /* SculptSession */
  sgcontext->ss = ob->sculpt;

  /* Symmetry. */
  sgcontext->symm = ePaintSymmetryFlags(SCULPT_mesh_symmetry_xyz_get(ob));

  /* View Normal. */
  float mat[3][3];
  float view_dir[3] = {0.0f, 0.0f, 1.0f};
  copy_m3_m4(mat, sgcontext->vc.rv3d->viewinv);
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(sgcontext->world_space_view_normal, view_dir);
  copy_m3_m4(mat, ob->world_to_object);
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(sgcontext->true_view_normal, view_dir);

  /* View Origin. */
  copy_v3_v3(sgcontext->world_space_view_origin, sgcontext->vc.rv3d->viewinv[3]);
  copy_v3_v3(sgcontext->true_view_origin, sgcontext->vc.rv3d->viewinv[3]);
}

static void sculpt_gesture_lasso_px_cb(int x, int x_end, int y, void *user_data)
{
  SculptGestureContext *mcontext = static_cast<SculptGestureContext *>(user_data);
  LassoGestureData *lasso = &mcontext->lasso;
  int index = (y * lasso->width) + x;
  int index_end = (y * lasso->width) + x_end;
  do {
    lasso->mask_px[index].set();
  } while (++index != index_end);
}

static SculptGestureContext *sculpt_gesture_init_from_lasso(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = MEM_new<SculptGestureContext>(__func__);
  sgcontext->shape_type = SCULPT_GESTURE_SHAPE_LASSO;

  sculpt_gesture_context_init_common(C, op, sgcontext);

  int mcoords_len;
  const int(*mcoords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcoords_len);

  if (!mcoords) {
    return nullptr;
  }

  sgcontext->lasso.projviewobjmat = ED_view3d_ob_project_mat_get(sgcontext->vc.rv3d,
                                                                 sgcontext->vc.obact);
  BLI_lasso_boundbox(&sgcontext->lasso.boundbox, mcoords, mcoords_len);
  const int lasso_width = 1 + sgcontext->lasso.boundbox.xmax - sgcontext->lasso.boundbox.xmin;
  const int lasso_height = 1 + sgcontext->lasso.boundbox.ymax - sgcontext->lasso.boundbox.ymin;
  sgcontext->lasso.width = lasso_width;
  sgcontext->lasso.mask_px.resize(lasso_width * lasso_height);

  BLI_bitmap_draw_2d_poly_v2i_n(sgcontext->lasso.boundbox.xmin,
                                sgcontext->lasso.boundbox.ymin,
                                sgcontext->lasso.boundbox.xmax,
                                sgcontext->lasso.boundbox.ymax,
                                mcoords,
                                mcoords_len,
                                sculpt_gesture_lasso_px_cb,
                                sgcontext);

  BoundBox bb;
  ED_view3d_clipping_calc(&bb,
                          sgcontext->true_clip_planes,
                          sgcontext->vc.region,
                          sgcontext->vc.obact,
                          &sgcontext->lasso.boundbox);

  sgcontext->gesture_points = static_cast<float(*)[2]>(
      MEM_malloc_arrayN(mcoords_len, sizeof(float[2]), "trim points"));
  sgcontext->tot_gesture_points = mcoords_len;
  for (int i = 0; i < mcoords_len; i++) {
    sgcontext->gesture_points[i][0] = mcoords[i][0];
    sgcontext->gesture_points[i][1] = mcoords[i][1];
  }

  MEM_freeN((void *)mcoords);

  return sgcontext;
}

static SculptGestureContext *sculpt_gesture_init_from_box(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = MEM_new<SculptGestureContext>(__func__);
  sgcontext->shape_type = SCULPT_GESTURE_SHAPE_BOX;

  sculpt_gesture_context_init_common(C, op, sgcontext);

  rcti rect;
  WM_operator_properties_border_to_rcti(op, &rect);

  BoundBox bb;
  ED_view3d_clipping_calc(
      &bb, sgcontext->true_clip_planes, sgcontext->vc.region, sgcontext->vc.obact, &rect);

  sgcontext->gesture_points = static_cast<float(*)[2]>(
      MEM_calloc_arrayN(4, sizeof(float[2]), "trim points"));
  sgcontext->tot_gesture_points = 4;

  sgcontext->gesture_points[0][0] = rect.xmax;
  sgcontext->gesture_points[0][1] = rect.ymax;

  sgcontext->gesture_points[1][0] = rect.xmax;
  sgcontext->gesture_points[1][1] = rect.ymin;

  sgcontext->gesture_points[2][0] = rect.xmin;
  sgcontext->gesture_points[2][1] = rect.ymin;

  sgcontext->gesture_points[3][0] = rect.xmin;
  sgcontext->gesture_points[3][1] = rect.ymax;
  return sgcontext;
}

static void sculpt_gesture_line_plane_from_tri(float *r_plane,
                                               SculptGestureContext *sgcontext,
                                               const bool flip,
                                               const float p1[3],
                                               const float p2[3],
                                               const float p3[3])
{
  float normal[3];
  normal_tri_v3(normal, p1, p2, p3);
  mul_v3_mat3_m4v3(normal, sgcontext->vc.obact->world_to_object, normal);
  if (flip) {
    mul_v3_fl(normal, -1.0f);
  }
  float plane_point_object_space[3];
  mul_v3_m4v3(plane_point_object_space, sgcontext->vc.obact->world_to_object, p1);
  plane_from_point_normal_v3(r_plane, plane_point_object_space, normal);
}

/* Creates 4 points in the plane defined by the line and 2 extra points with an offset relative to
 * this plane. */
static void sculpt_gesture_line_calculate_plane_points(SculptGestureContext *sgcontext,
                                                       float line_points[2][2],
                                                       float r_plane_points[4][3],
                                                       float r_offset_plane_points[2][3])
{
  float depth_point[3];
  add_v3_v3v3(depth_point, sgcontext->true_view_origin, sgcontext->true_view_normal);
  ED_view3d_win_to_3d(
      sgcontext->vc.v3d, sgcontext->vc.region, depth_point, line_points[0], r_plane_points[0]);
  ED_view3d_win_to_3d(
      sgcontext->vc.v3d, sgcontext->vc.region, depth_point, line_points[1], r_plane_points[3]);

  madd_v3_v3v3fl(depth_point, sgcontext->true_view_origin, sgcontext->true_view_normal, 10.0f);
  ED_view3d_win_to_3d(
      sgcontext->vc.v3d, sgcontext->vc.region, depth_point, line_points[0], r_plane_points[1]);
  ED_view3d_win_to_3d(
      sgcontext->vc.v3d, sgcontext->vc.region, depth_point, line_points[1], r_plane_points[2]);

  float normal[3];
  normal_tri_v3(normal, r_plane_points[0], r_plane_points[1], r_plane_points[2]);
  add_v3_v3v3(r_offset_plane_points[0], r_plane_points[0], normal);
  add_v3_v3v3(r_offset_plane_points[1], r_plane_points[3], normal);
}

static SculptGestureContext *sculpt_gesture_init_from_line(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = MEM_new<SculptGestureContext>(__func__);
  sgcontext->shape_type = SCULPT_GESTURE_SHAPE_LINE;

  sculpt_gesture_context_init_common(C, op, sgcontext);

  float line_points[2][2];
  line_points[0][0] = RNA_int_get(op->ptr, "xstart");
  line_points[0][1] = RNA_int_get(op->ptr, "ystart");
  line_points[1][0] = RNA_int_get(op->ptr, "xend");
  line_points[1][1] = RNA_int_get(op->ptr, "yend");

  sgcontext->line.flip = RNA_boolean_get(op->ptr, "flip");

  float plane_points[4][3];
  float offset_plane_points[2][3];
  sculpt_gesture_line_calculate_plane_points(
      sgcontext, line_points, plane_points, offset_plane_points);

  /* Calculate line plane and normal. */
  const bool flip = sgcontext->line.flip ^ (!sgcontext->vc.rv3d->is_persp);
  sculpt_gesture_line_plane_from_tri(sgcontext->line.true_plane,
                                     sgcontext,
                                     flip,
                                     plane_points[0],
                                     plane_points[1],
                                     plane_points[2]);

  /* Calculate the side planes. */
  sculpt_gesture_line_plane_from_tri(sgcontext->line.true_side_plane[0],
                                     sgcontext,
                                     false,
                                     plane_points[1],
                                     plane_points[0],
                                     offset_plane_points[0]);
  sculpt_gesture_line_plane_from_tri(sgcontext->line.true_side_plane[1],
                                     sgcontext,
                                     false,
                                     plane_points[3],
                                     plane_points[2],
                                     offset_plane_points[1]);

  return sgcontext;
}

static void sculpt_gesture_context_free(SculptGestureContext *sgcontext)
{
  MEM_SAFE_FREE(sgcontext->gesture_points);
  MEM_SAFE_FREE(sgcontext->operation);
  MEM_delete(sgcontext);
}

static void flip_plane(float out[4], const float in[4], const char symm)
{
  if (symm & PAINT_SYMM_X) {
    out[0] = -in[0];
  }
  else {
    out[0] = in[0];
  }
  if (symm & PAINT_SYMM_Y) {
    out[1] = -in[1];
  }
  else {
    out[1] = in[1];
  }
  if (symm & PAINT_SYMM_Z) {
    out[2] = -in[2];
  }
  else {
    out[2] = in[2];
  }

  out[3] = in[3];
}

static void sculpt_gesture_flip_for_symmetry_pass(SculptGestureContext *sgcontext,
                                                  const ePaintSymmetryFlags symmpass)
{
  sgcontext->symmpass = symmpass;
  for (int j = 0; j < 4; j++) {
    flip_plane(sgcontext->clip_planes[j], sgcontext->true_clip_planes[j], symmpass);
  }

  negate_m4(sgcontext->clip_planes);

  flip_v3_v3(sgcontext->view_normal, sgcontext->true_view_normal, symmpass);
  flip_v3_v3(sgcontext->view_origin, sgcontext->true_view_origin, symmpass);
  flip_plane(sgcontext->line.plane, sgcontext->line.true_plane, symmpass);
  flip_plane(sgcontext->line.side_plane[0], sgcontext->line.true_side_plane[0], symmpass);
  flip_plane(sgcontext->line.side_plane[1], sgcontext->line.true_side_plane[1], symmpass);
}

static Vector<PBVHNode *> sculpt_gesture_update_effected_nodes_by_line_plane(
    SculptGestureContext *sgcontext)
{
  SculptSession *ss = sgcontext->ss;
  float clip_planes[3][4];
  copy_v4_v4(clip_planes[0], sgcontext->line.plane);
  copy_v4_v4(clip_planes[1], sgcontext->line.side_plane[0]);
  copy_v4_v4(clip_planes[2], sgcontext->line.side_plane[1]);

  PBVHFrustumPlanes frustum{};
  frustum.planes = clip_planes;
  frustum.num_planes = sgcontext->line.use_side_planes ? 3 : 1;

  return sgcontext->nodes = bke::pbvh::search_gather(ss->pbvh, [&](PBVHNode &node) {
           return BKE_pbvh_node_frustum_contain_AABB(&node, &frustum);
         });
}

static void sculpt_gesture_update_effected_nodes_by_clip_planes(SculptGestureContext *sgcontext)
{
  SculptSession *ss = sgcontext->ss;
  float clip_planes[4][4];
  copy_m4_m4(clip_planes, sgcontext->clip_planes);
  negate_m4(clip_planes);

  PBVHFrustumPlanes frustum{};
  frustum.planes = clip_planes;
  frustum.num_planes = 4;

  sgcontext->nodes = bke::pbvh::search_gather(ss->pbvh, [&](PBVHNode &node) {
    return BKE_pbvh_node_frustum_contain_AABB(&node, &frustum);
  });
}

static void sculpt_gesture_update_effected_nodes(SculptGestureContext *sgcontext)
{
  switch (sgcontext->shape_type) {
    case SCULPT_GESTURE_SHAPE_BOX:
    case SCULPT_GESTURE_SHAPE_LASSO:
      sculpt_gesture_update_effected_nodes_by_clip_planes(sgcontext);
      break;
    case SCULPT_GESTURE_SHAPE_LINE:
      sculpt_gesture_update_effected_nodes_by_line_plane(sgcontext);
      break;
  }
}

static bool sculpt_gesture_is_effected_lasso(SculptGestureContext *sgcontext, const float co[3])
{
  int scr_co_s[2];
  float co_final[3];

  flip_v3_v3(co_final, co, sgcontext->symmpass);

  /* First project point to 2d space. */
  const float2 scr_co_f = ED_view3d_project_float_v2_m4(
      sgcontext->vc.region, co_final, sgcontext->lasso.projviewobjmat);

  scr_co_s[0] = scr_co_f[0];
  scr_co_s[1] = scr_co_f[1];

  /* Clip against lasso boundbox. */
  LassoGestureData *lasso = &sgcontext->lasso;
  if (!BLI_rcti_isect_pt(&lasso->boundbox, scr_co_s[0], scr_co_s[1])) {
    return false;
  }

  scr_co_s[0] -= lasso->boundbox.xmin;
  scr_co_s[1] -= lasso->boundbox.ymin;

  return lasso->mask_px[scr_co_s[1] * lasso->width + scr_co_s[0]].test();
}

static bool sculpt_gesture_is_effected(SculptGestureContext *sgcontext,
                                       const float3 &co,
                                       const float3 &vertex_normal)
{
  float dot = dot_v3v3(sgcontext->view_normal, vertex_normal);
  const bool is_effected_front_face = !(sgcontext->front_faces_only && dot < 0.0f);

  if (!is_effected_front_face) {
    return false;
  }

  switch (sgcontext->shape_type) {
    case SCULPT_GESTURE_SHAPE_BOX:
      return isect_point_planes_v3(sgcontext->clip_planes, 4, co);
    case SCULPT_GESTURE_SHAPE_LASSO:
      return sculpt_gesture_is_effected_lasso(sgcontext, co);
    case SCULPT_GESTURE_SHAPE_LINE:
      if (sgcontext->line.use_side_planes) {
        return plane_point_side_v3(sgcontext->line.plane, co) > 0.0f &&
               plane_point_side_v3(sgcontext->line.side_plane[0], co) > 0.0f &&
               plane_point_side_v3(sgcontext->line.side_plane[1], co) > 0.0f;
      }
      return plane_point_side_v3(sgcontext->line.plane, co) > 0.0f;
  }
  return false;
}

static void sculpt_gesture_apply(bContext *C, SculptGestureContext *sgcontext, wmOperator *op)
{
  SculptGestureOperation *operation = sgcontext->operation;
  undo::push_begin(CTX_data_active_object(C), op);

  operation->sculpt_gesture_begin(C, sgcontext);

  for (int symmpass = 0; symmpass <= sgcontext->symm; symmpass++) {
    if (SCULPT_is_symmetry_iteration_valid(symmpass, sgcontext->symm)) {
      sculpt_gesture_flip_for_symmetry_pass(sgcontext, ePaintSymmetryFlags(symmpass));
      sculpt_gesture_update_effected_nodes(sgcontext);

      operation->sculpt_gesture_apply_for_symmetry_pass(C, sgcontext);
    }
  }

  operation->sculpt_gesture_end(C, sgcontext);

  Object *ob = CTX_data_active_object(C);
  undo::push_end(ob);

  SCULPT_tag_update_overlays(C);
}

/* Face Set Gesture Operation. */

struct SculptGestureFaceSetOperation {
  SculptGestureOperation op;

  int new_face_set_id;
};

static void sculpt_gesture_face_set_begin(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, sgcontext->vc.obact, false);
}

static void face_set_gesture_apply_mesh(SculptGestureContext *sgcontext,
                                        const Span<PBVHNode *> nodes)
{
  SculptGestureFaceSetOperation *face_set_operation = (SculptGestureFaceSetOperation *)
                                                          sgcontext->operation;
  const int new_face_set = face_set_operation->new_face_set_id;
  Object &object = *sgcontext->vc.obact;
  SculptSession &ss = *sgcontext->ss;
  const PBVH &pbvh = *sgcontext->ss->pbvh;

  const Span<float3> positions = ss.vert_positions;
  const OffsetIndices<int> faces = ss.faces;
  const Span<int> corner_verts = ss.corner_verts;
  const bool *hide_poly = ss.hide_poly;
  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(object);

  threading::parallel_for(sgcontext->nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      undo::push_node(sgcontext->vc.obact, node, undo::Type::FaceSet);

      bool any_updated = false;
      for (const int face : BKE_pbvh_node_calc_face_indices(pbvh, *node)) {
        if (hide_poly && hide_poly[face]) {
          continue;
        }
        const Span<int> face_verts = corner_verts.slice(faces[face]);
        const float3 face_center = bke::mesh::face_center_calc(positions, face_verts);
        const float3 face_normal = bke::mesh::face_normal_calc(positions, face_verts);
        if (!sculpt_gesture_is_effected(sgcontext, face_center, face_normal)) {
          continue;
        }
        face_sets.span[face] = new_face_set;
        any_updated = true;
      }
      if (any_updated) {
        BKE_pbvh_node_mark_update_face_sets(node);
      }
    }
  });

  face_sets.finish();
}

static void face_set_gesture_apply_bmesh(SculptGestureContext *sgcontext,
                                         const Span<PBVHNode *> nodes)
{
  SculptGestureFaceSetOperation *face_set_operation = (SculptGestureFaceSetOperation *)
                                                          sgcontext->operation;
  const int new_face_set = face_set_operation->new_face_set_id;
  SculptSession &ss = *sgcontext->ss;
  BMesh *bm = ss.bm;
  const int offset = CustomData_get_offset_named(&bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  threading::parallel_for(sgcontext->nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      undo::push_node(sgcontext->vc.obact, node, undo::Type::FaceSet);

      bool any_updated = false;
      for (BMFace *face : BKE_pbvh_bmesh_node_faces(node)) {
        if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
          continue;
        }
        float3 center;
        BM_face_calc_center_median(face, center);
        if (!sculpt_gesture_is_effected(sgcontext, center, face->no)) {
          continue;
        }
        BM_ELEM_CD_SET_INT(face, offset, new_face_set);
        any_updated = true;
      }

      if (any_updated) {
        BKE_pbvh_node_mark_update_visibility(node);
      }
    }
  });
}

static void sculpt_gesture_face_set_apply_for_symmetry_pass(bContext * /*C*/,
                                                            SculptGestureContext *sgcontext)
{
  switch (BKE_pbvh_type(sgcontext->ss->pbvh)) {
    case PBVH_GRIDS:
    case PBVH_FACES:
      face_set_gesture_apply_mesh(sgcontext, sgcontext->nodes);
      break;
    case PBVH_BMESH:
      face_set_gesture_apply_bmesh(sgcontext, sgcontext->nodes);
  }
}

static void sculpt_gesture_face_set_end(bContext * /*C*/, SculptGestureContext * /*sgcontext*/) {}

static void sculpt_gesture_init_face_set_properties(SculptGestureContext *sgcontext,
                                                    wmOperator * /*op*/)
{
  Object &object = *sgcontext->vc.obact;
  sgcontext->operation = reinterpret_cast<SculptGestureOperation *>(
      MEM_cnew<SculptGestureFaceSetOperation>(__func__));

  SculptGestureFaceSetOperation *face_set_operation = (SculptGestureFaceSetOperation *)
                                                          sgcontext->operation;

  face_set_operation->op.sculpt_gesture_begin = sculpt_gesture_face_set_begin;
  face_set_operation->op.sculpt_gesture_apply_for_symmetry_pass =
      sculpt_gesture_face_set_apply_for_symmetry_pass;
  face_set_operation->op.sculpt_gesture_end = sculpt_gesture_face_set_end;

  face_set_operation->new_face_set_id = face_set::find_next_available_id(object);
}

/* Mask Gesture Operation. */

struct SculptGestureMaskOperation {
  SculptGestureOperation op;

  PaintMaskFloodMode mode;
  float value;
};

static void sculpt_gesture_mask_begin(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, sgcontext->vc.obact, false);
}

static void mask_gesture_apply_task(SculptGestureContext *sgcontext,
                                    const SculptMaskWriteInfo mask_write,
                                    PBVHNode *node)
{
  SculptGestureMaskOperation *mask_operation = (SculptGestureMaskOperation *)sgcontext->operation;
  Object *ob = sgcontext->vc.obact;

  const bool is_multires = BKE_pbvh_type(sgcontext->ss->pbvh) == PBVH_GRIDS;

  PBVHVertexIter vd;
  bool any_masked = false;
  bool redraw = false;

  BKE_pbvh_vertex_iter_begin (sgcontext->ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    float vertex_normal[3];
    const float *co = SCULPT_vertex_co_get(sgcontext->ss, vd.vertex);
    SCULPT_vertex_normal_get(sgcontext->ss, vd.vertex, vertex_normal);

    if (sculpt_gesture_is_effected(sgcontext, co, vertex_normal)) {
      float prevmask = vd.mask;
      if (!any_masked) {
        any_masked = true;

        undo::push_node(ob, node, undo::Type::Mask);

        if (is_multires) {
          BKE_pbvh_node_mark_positions_update(node);
        }
      }
      const float new_mask = mask_flood_fill_get_new_value_for_elem(
          prevmask, mask_operation->mode, mask_operation->value);
      if (prevmask != new_mask) {
        SCULPT_mask_vert_set(BKE_pbvh_type(ob->sculpt->pbvh), mask_write, new_mask, vd);
        redraw = true;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (redraw) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

static void sculpt_gesture_mask_apply_for_symmetry_pass(bContext * /*C*/,
                                                        SculptGestureContext *sgcontext)
{
  const SculptMaskWriteInfo mask_write = SCULPT_mask_get_for_write(sgcontext->ss);
  threading::parallel_for(sgcontext->nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      mask_gesture_apply_task(sgcontext, mask_write, sgcontext->nodes[i]);
    }
  });
}

static void sculpt_gesture_mask_end(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  if (BKE_pbvh_type(sgcontext->ss->pbvh) == PBVH_GRIDS) {
    multires_mark_as_modified(depsgraph, sgcontext->vc.obact, MULTIRES_COORDS_MODIFIED);
  }
  blender::bke::pbvh::update_mask(*sgcontext->ss->pbvh);
}

static void sculpt_gesture_init_mask_properties(bContext *C,
                                                SculptGestureContext *sgcontext,
                                                wmOperator *op)
{
  sgcontext->operation = reinterpret_cast<SculptGestureOperation *>(
      MEM_cnew<SculptGestureMaskOperation>(__func__));

  SculptGestureMaskOperation *mask_operation = (SculptGestureMaskOperation *)sgcontext->operation;

  Object *object = sgcontext->vc.obact;
  MultiresModifierData *mmd = BKE_sculpt_multires_active(sgcontext->vc.scene, object);
  BKE_sculpt_mask_layers_ensure(
      CTX_data_depsgraph_pointer(C), CTX_data_main(C), sgcontext->vc.obact, mmd);

  mask_operation->op.sculpt_gesture_begin = sculpt_gesture_mask_begin;
  mask_operation->op.sculpt_gesture_apply_for_symmetry_pass =
      sculpt_gesture_mask_apply_for_symmetry_pass;
  mask_operation->op.sculpt_gesture_end = sculpt_gesture_mask_end;

  mask_operation->mode = PaintMaskFloodMode(RNA_enum_get(op->ptr, "mode"));
  mask_operation->value = RNA_float_get(op->ptr, "value");
}

static void paint_mask_gesture_operator_properties(wmOperatorType *ot)
{
  RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", nullptr);
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

/* Trim Gesture Operation. */

enum eSculptTrimOperationType {
  SCULPT_GESTURE_TRIM_INTERSECT,
  SCULPT_GESTURE_TRIM_DIFFERENCE,
  SCULPT_GESTURE_TRIM_UNION,
  SCULPT_GESTURE_TRIM_JOIN,
};

/* Intersect is not exposed in the UI because it does not work correctly with symmetry (it deletes
 * the symmetrical part of the mesh in the first symmetry pass). */
static EnumPropertyItem prop_trim_operation_types[] = {
    {SCULPT_GESTURE_TRIM_DIFFERENCE,
     "DIFFERENCE",
     0,
     "Difference",
     "Use a difference boolean operation"},
    {SCULPT_GESTURE_TRIM_UNION, "UNION", 0, "Union", "Use a union boolean operation"},
    {SCULPT_GESTURE_TRIM_JOIN,
     "JOIN",
     0,
     "Join",
     "Join the new mesh as separate geometry, without performing any boolean operation"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum eSculptTrimOrientationType {
  SCULPT_GESTURE_TRIM_ORIENTATION_VIEW,
  SCULPT_GESTURE_TRIM_ORIENTATION_SURFACE,
};
static EnumPropertyItem prop_trim_orientation_types[] = {
    {SCULPT_GESTURE_TRIM_ORIENTATION_VIEW,
     "VIEW",
     0,
     "View",
     "Use the view to orientate the trimming shape"},
    {SCULPT_GESTURE_TRIM_ORIENTATION_SURFACE,
     "SURFACE",
     0,
     "Surface",
     "Use the surface normal to orientate the trimming shape"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum eSculptTrimExtrudeMode {
  SCULPT_GESTURE_TRIM_EXTRUDE_PROJECT,
  SCULPT_GESTURE_TRIM_EXTRUDE_FIXED
};

static EnumPropertyItem prop_trim_extrude_modes[] = {
    {SCULPT_GESTURE_TRIM_EXTRUDE_PROJECT,
     "PROJECT",
     0,
     "Project",
     "Project back faces when extruding"},
    {SCULPT_GESTURE_TRIM_EXTRUDE_FIXED, "FIXED", 0, "Fixed", "Extrude back faces by fixed amount"},
    {0, nullptr, 0, nullptr, nullptr},
};

struct SculptGestureTrimOperation {
  SculptGestureOperation op;

  Mesh *mesh;
  float (*true_mesh_co)[3];

  float depth_front;
  float depth_back;

  bool use_cursor_depth;

  eSculptTrimOperationType mode;
  eSculptTrimOrientationType orientation;
  eSculptTrimExtrudeMode extrude_mode;
};

static void sculpt_gesture_trim_normals_update(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  Mesh *trim_mesh = trim_operation->mesh;

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(trim_mesh);

  BMeshCreateParams bm_create_params{};
  bm_create_params.use_toolflags = true;
  BMesh *bm = BM_mesh_create(&allocsize, &bm_create_params);

  BMeshFromMeshParams bm_from_me_params{};
  bm_from_me_params.calc_face_normal = true;
  bm_from_me_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, trim_mesh, &bm_from_me_params);

  BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);
  BMO_op_callf(bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "recalc_face_normals faces=%hf",
               BM_ELEM_TAG);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BMeshToMeshParams convert_params{};
  convert_params.calc_object_remap = false;
  Mesh *result = BKE_mesh_from_bmesh_nomain(bm, &convert_params, trim_mesh);

  BM_mesh_free(bm);
  BKE_id_free(nullptr, trim_mesh);
  trim_operation->mesh = result;
}

/* Get the origin and normal that are going to be used for calculating the depth and position the
 * trimming geometry. */
static void sculpt_gesture_trim_shape_origin_normal_get(SculptGestureContext *sgcontext,
                                                        float *r_origin,
                                                        float *r_normal)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  /* Use the view origin and normal in world space. The trimming mesh coordinates are
   * calculated in world space, aligned to the view, and then converted to object space to
   * store them in the final trimming mesh which is going to be used in the boolean operation.
   */
  switch (trim_operation->orientation) {
    case SCULPT_GESTURE_TRIM_ORIENTATION_VIEW:
      mul_v3_m4v3(
          r_origin, sgcontext->vc.obact->object_to_world, sgcontext->ss->gesture_initial_location);
      copy_v3_v3(r_normal, sgcontext->world_space_view_normal);
      negate_v3(r_normal);
      break;
    case SCULPT_GESTURE_TRIM_ORIENTATION_SURFACE:
      mul_v3_m4v3(
          r_origin, sgcontext->vc.obact->object_to_world, sgcontext->ss->gesture_initial_location);
      /* Transforming the normal does not take non uniform scaling into account. Sculpt mode is not
       * expected to work on object with non uniform scaling. */
      copy_v3_v3(r_normal, sgcontext->ss->gesture_initial_normal);
      mul_mat3_m4_v3(sgcontext->vc.obact->object_to_world, r_normal);
      break;
  }
}

static void sculpt_gesture_trim_calculate_depth(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;

  SculptSession *ss = sgcontext->ss;
  ViewContext *vc = &sgcontext->vc;

  const int totvert = SCULPT_vertex_count_get(ss);

  float shape_plane[4];
  float shape_origin[3];
  float shape_normal[3];
  sculpt_gesture_trim_shape_origin_normal_get(sgcontext, shape_origin, shape_normal);
  plane_from_point_normal_v3(shape_plane, shape_origin, shape_normal);

  trim_operation->depth_front = FLT_MAX;
  trim_operation->depth_back = -FLT_MAX;

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    const float *vco = SCULPT_vertex_co_get(ss, vertex);
    /* Convert the coordinates to world space to calculate the depth. When generating the trimming
     * mesh, coordinates are first calculated in world space, then converted to object space to
     * store them. */
    float world_space_vco[3];
    mul_v3_m4v3(world_space_vco, vc->obact->object_to_world, vco);
    const float dist = dist_signed_to_plane_v3(world_space_vco, shape_plane);
    trim_operation->depth_front = min_ff(dist, trim_operation->depth_front);
    trim_operation->depth_back = max_ff(dist, trim_operation->depth_back);
  }

  if (trim_operation->use_cursor_depth) {
    float world_space_gesture_initial_location[3];
    mul_v3_m4v3(world_space_gesture_initial_location,
                vc->obact->object_to_world,
                ss->gesture_initial_location);

    float mid_point_depth;
    if (trim_operation->orientation == SCULPT_GESTURE_TRIM_ORIENTATION_VIEW) {
      mid_point_depth = ss->gesture_initial_hit ?
                            dist_signed_to_plane_v3(world_space_gesture_initial_location,
                                                    shape_plane) :
                            (trim_operation->depth_back + trim_operation->depth_front) * 0.5f;
    }
    else {
      /* When using normal orientation, if the stroke started over the mesh, position the mid point
       * at 0 distance from the shape plane. This positions the trimming shape half inside of the
       * surface. */
      mid_point_depth = ss->gesture_initial_hit ?
                            0.0f :
                            (trim_operation->depth_back + trim_operation->depth_front) * 0.5f;
    }

    float depth_radius;

    if (ss->gesture_initial_hit) {
      depth_radius = ss->cursor_radius;
    }
    else {
      /* ss->cursor_radius is only valid if the stroke started
       * over the sculpt mesh.  If it's not we must
       * compute the radius ourselves.  See #81452.
       */

      Sculpt *sd = CTX_data_tool_settings(vc->C)->sculpt;
      Brush *brush = BKE_paint_brush(&sd->paint);
      Scene *scene = CTX_data_scene(vc->C);

      if (!BKE_brush_use_locked_size(scene, brush)) {
        depth_radius = paint_calc_object_space_radius(
            vc, ss->gesture_initial_location, BKE_brush_size_get(scene, brush));
      }
      else {
        depth_radius = BKE_brush_unprojected_radius_get(scene, brush);
      }
    }

    trim_operation->depth_front = mid_point_depth - depth_radius;
    trim_operation->depth_back = mid_point_depth + depth_radius;
  }
}

static void sculpt_gesture_trim_geometry_generate(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  ViewContext *vc = &sgcontext->vc;
  ARegion *region = vc->region;

  const int tot_screen_points = sgcontext->tot_gesture_points;
  float(*screen_points)[2] = sgcontext->gesture_points;

  const int trim_totverts = tot_screen_points * 2;
  const int trim_faces_nums = (2 * (tot_screen_points - 2)) + (2 * tot_screen_points);
  trim_operation->mesh = BKE_mesh_new_nomain(
      trim_totverts, 0, trim_faces_nums, trim_faces_nums * 3);
  trim_operation->true_mesh_co = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(trim_totverts, sizeof(float[3]), "mesh orco"));

  float depth_front = trim_operation->depth_front;
  float depth_back = trim_operation->depth_back;
  float pad_factor = 0.0f;

  if (!trim_operation->use_cursor_depth) {
    pad_factor = (depth_back - depth_front) * 0.01f + 0.001f;

    /* When using cursor depth, don't modify the depth set by the cursor radius. If full depth is
     * used, adding a little padding to the trimming shape can help avoiding booleans with coplanar
     * faces. */
    depth_front -= pad_factor;
    depth_back += pad_factor;
  }

  float shape_origin[3];
  float shape_normal[3];
  float shape_plane[4];
  sculpt_gesture_trim_shape_origin_normal_get(sgcontext, shape_origin, shape_normal);
  plane_from_point_normal_v3(shape_plane, shape_origin, shape_normal);

  const float(*ob_imat)[4] = vc->obact->world_to_object;

  /* Write vertices coordinatesSCULPT_GESTURE_TRIM_DIFFERENCE for the front face. */
  MutableSpan<float3> positions = trim_operation->mesh->vert_positions_for_write();

  float depth_point[3];

  /* Get origin point for SCULPT_GESTURE_TRIM_ORIENTATION_VIEW.
   * Note: for projection extrusion we add depth_front here
   * instead of in the loop.
   */
  if (trim_operation->extrude_mode == SCULPT_GESTURE_TRIM_EXTRUDE_FIXED) {
    copy_v3_v3(depth_point, shape_origin);
  }
  else {
    madd_v3_v3v3fl(depth_point, shape_origin, shape_normal, depth_front);
  }

  for (int i = 0; i < tot_screen_points; i++) {
    float new_point[3];
    if (trim_operation->orientation == SCULPT_GESTURE_TRIM_ORIENTATION_VIEW) {
      ED_view3d_win_to_3d(vc->v3d, region, depth_point, screen_points[i], new_point);

      /* For fixed mode we add the shape normal here to avoid projection errors. */
      if (trim_operation->extrude_mode == SCULPT_GESTURE_TRIM_EXTRUDE_FIXED) {
        madd_v3_v3fl(new_point, shape_normal, depth_front);
      }
    }
    else {
      ED_view3d_win_to_3d_on_plane(region, shape_plane, screen_points[i], false, new_point);
      madd_v3_v3fl(new_point, shape_normal, depth_front);
    }

    copy_v3_v3(positions[i], new_point);
  }

  /* Write vertices coordinates for the back face. */
  madd_v3_v3v3fl(depth_point, shape_origin, shape_normal, depth_back);
  for (int i = 0; i < tot_screen_points; i++) {
    float new_point[3];

    if (trim_operation->extrude_mode == SCULPT_GESTURE_TRIM_EXTRUDE_PROJECT) {
      if (trim_operation->orientation == SCULPT_GESTURE_TRIM_ORIENTATION_VIEW) {
        ED_view3d_win_to_3d(vc->v3d, region, depth_point, screen_points[i], new_point);
      }
      else {
        ED_view3d_win_to_3d_on_plane(region, shape_plane, screen_points[i], false, new_point);
        madd_v3_v3fl(new_point, shape_normal, depth_back);
      }
    }
    else {
      copy_v3_v3(new_point, positions[i]);
      float dist = dist_signed_to_plane_v3(new_point, shape_plane);

      madd_v3_v3fl(new_point, shape_normal, depth_back - dist);
    }

    copy_v3_v3(positions[i + tot_screen_points], new_point);
  }

  /* Project to object space. */
  for (int i = 0; i < tot_screen_points * 2; i++) {
    float new_point[3];

    copy_v3_v3(new_point, positions[i]);
    mul_v3_m4v3(positions[i], ob_imat, new_point);
    mul_v3_m4v3(trim_operation->true_mesh_co[i], ob_imat, new_point);
  }

  /* Get the triangulation for the front/back poly. */
  const int tot_tris_face = tot_screen_points - 2;
  uint(*r_tris)[3] = static_cast<uint(*)[3]>(
      MEM_malloc_arrayN(tot_tris_face, sizeof(uint[3]), "tris"));
  BLI_polyfill_calc(screen_points, tot_screen_points, 0, r_tris);

  /* Write the front face triangle indices. */
  MutableSpan<int> face_offsets = trim_operation->mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = trim_operation->mesh->corner_verts_for_write();
  int face_index = 0;
  int loop_index = 0;
  for (int i = 0; i < tot_tris_face; i++) {
    face_offsets[face_index] = loop_index;
    corner_verts[loop_index + 0] = r_tris[i][0];
    corner_verts[loop_index + 1] = r_tris[i][1];
    corner_verts[loop_index + 2] = r_tris[i][2];
    face_index++;
    loop_index += 3;
  }

  /* Write the back face triangle indices. */
  for (int i = 0; i < tot_tris_face; i++) {
    face_offsets[face_index] = loop_index;
    corner_verts[loop_index + 0] = r_tris[i][0] + tot_screen_points;
    corner_verts[loop_index + 1] = r_tris[i][1] + tot_screen_points;
    corner_verts[loop_index + 2] = r_tris[i][2] + tot_screen_points;
    face_index++;
    loop_index += 3;
  }

  MEM_freeN(r_tris);

  /* Write the indices for the lateral triangles. */
  for (int i = 0; i < tot_screen_points; i++) {
    face_offsets[face_index] = loop_index;
    int current_index = i;
    int next_index = current_index + 1;
    if (next_index >= tot_screen_points) {
      next_index = 0;
    }
    corner_verts[loop_index + 0] = next_index + tot_screen_points;
    corner_verts[loop_index + 1] = next_index;
    corner_verts[loop_index + 2] = current_index;
    face_index++;
    loop_index += 3;
  }

  for (int i = 0; i < tot_screen_points; i++) {
    face_offsets[face_index] = loop_index;
    int current_index = i;
    int next_index = current_index + 1;
    if (next_index >= tot_screen_points) {
      next_index = 0;
    }
    corner_verts[loop_index + 0] = current_index;
    corner_verts[loop_index + 1] = current_index + tot_screen_points;
    corner_verts[loop_index + 2] = next_index + tot_screen_points;
    face_index++;
    loop_index += 3;
  }

  bke::mesh_smooth_set(*trim_operation->mesh, false);
  bke::mesh_calc_edges(*trim_operation->mesh, false, false);
  sculpt_gesture_trim_normals_update(sgcontext);
}

static void sculpt_gesture_trim_geometry_free(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  BKE_id_free(nullptr, trim_operation->mesh);
  MEM_freeN(trim_operation->true_mesh_co);
}

static int bm_face_isect_pair(BMFace *f, void * /*user_data*/)
{
  return BM_elem_flag_test(f, BM_ELEM_DRAW) ? 1 : 0;
}

static void sculpt_gesture_apply_trim(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  Mesh *sculpt_mesh = BKE_mesh_from_object(sgcontext->vc.obact);
  Mesh *trim_mesh = trim_operation->mesh;

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(sculpt_mesh, trim_mesh);

  BMeshCreateParams bm_create_params{};
  bm_create_params.use_toolflags = false;
  BMesh *bm = BM_mesh_create(&allocsize, &bm_create_params);

  BMeshFromMeshParams bm_from_me_params{};
  bm_from_me_params.calc_face_normal = true;
  bm_from_me_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, trim_mesh, &bm_from_me_params);
  BM_mesh_bm_from_me(bm, sculpt_mesh, &bm_from_me_params);

  const int corner_tris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  BMLoop *(*corner_tris)[3] = static_cast<BMLoop *(*)[3]>(
      MEM_malloc_arrayN(corner_tris_tot, sizeof(*corner_tris), __func__));
  BM_mesh_calc_tessellation_beauty(bm, corner_tris);

  BMIter iter;
  int i;
  const int i_faces_end = trim_mesh->faces_num;

  /* We need face normals because of 'BM_face_split_edgenet'
   * we could calculate on the fly too (before calling split). */

  const short ob_src_totcol = trim_mesh->totcol;
  Array<short> material_remap(ob_src_totcol ? ob_src_totcol : 1);

  BMFace *efa;
  i = 0;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    normalize_v3(efa->no);

    /* Temp tag to test which side split faces are from. */
    BM_elem_flag_enable(efa, BM_ELEM_DRAW);

    /* Remap material. */
    if (efa->mat_nr < ob_src_totcol) {
      efa->mat_nr = material_remap[efa->mat_nr];
    }

    if (++i == i_faces_end) {
      break;
    }
  }

  /* Join does not do a boolean operation, it just adds the geometry. */
  if (trim_operation->mode != SCULPT_GESTURE_TRIM_JOIN) {
    int boolean_mode = 0;
    switch (trim_operation->mode) {
      case SCULPT_GESTURE_TRIM_INTERSECT:
        boolean_mode = eBooleanModifierOp_Intersect;
        break;
      case SCULPT_GESTURE_TRIM_DIFFERENCE:
        boolean_mode = eBooleanModifierOp_Difference;
        break;
      case SCULPT_GESTURE_TRIM_UNION:
        boolean_mode = eBooleanModifierOp_Union;
        break;
      case SCULPT_GESTURE_TRIM_JOIN:
        BLI_assert(false);
        break;
    }
    BM_mesh_boolean(bm,
                    corner_tris,
                    corner_tris_tot,
                    bm_face_isect_pair,
                    nullptr,
                    2,
                    true,
                    true,
                    false,
                    boolean_mode);
  }

  MEM_freeN(corner_tris);

  BMeshToMeshParams convert_params{};
  convert_params.calc_object_remap = false;
  Mesh *result = BKE_mesh_from_bmesh_nomain(bm, &convert_params, sculpt_mesh);

  BM_mesh_free(bm);
  BKE_mesh_nomain_to_mesh(
      result, static_cast<Mesh *>(sgcontext->vc.obact->data), sgcontext->vc.obact);
}

static void sculpt_gesture_trim_begin(bContext *C, SculptGestureContext *sgcontext)
{
  Object *object = sgcontext->vc.obact;
  SculptSession *ss = object->sculpt;

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  sculpt_gesture_trim_calculate_depth(sgcontext);
  sculpt_gesture_trim_geometry_generate(sgcontext);
  SCULPT_topology_islands_invalidate(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, sgcontext->vc.obact, false);
  undo::push_node(sgcontext->vc.obact, nullptr, undo::Type::Geometry);
}

static void sculpt_gesture_trim_apply_for_symmetry_pass(bContext * /*C*/,
                                                        SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  Mesh *trim_mesh = trim_operation->mesh;
  MutableSpan<float3> positions = trim_mesh->vert_positions_for_write();
  for (int i = 0; i < trim_mesh->verts_num; i++) {
    flip_v3_v3(positions[i], trim_operation->true_mesh_co[i], sgcontext->symmpass);
  }
  sculpt_gesture_trim_normals_update(sgcontext);
  sculpt_gesture_apply_trim(sgcontext);
}

static void sculpt_gesture_trim_end(bContext * /*C*/, SculptGestureContext *sgcontext)
{
  Object *object = sgcontext->vc.obact;
  Mesh *mesh = (Mesh *)object->data;
  const bke::AttributeAccessor attributes = mesh->attributes_for_write();
  if (attributes.contains(".sculpt_face_set")) {
    /* Assign a new Face Set ID to the new faces created by the trim operation. */
    const int next_face_set_id = face_set::find_next_available_id(*object);
    face_set::initialize_none_to_id(mesh, next_face_set_id);
  }

  sculpt_gesture_trim_geometry_free(sgcontext);

  undo::push_node(sgcontext->vc.obact, nullptr, undo::Type::Geometry);
  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&sgcontext->vc.obact->id, ID_RECALC_GEOMETRY);
}

static void sculpt_gesture_init_trim_properties(SculptGestureContext *sgcontext, wmOperator *op)
{
  sgcontext->operation = reinterpret_cast<SculptGestureOperation *>(
      MEM_cnew<SculptGestureTrimOperation>(__func__));

  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;

  trim_operation->op.sculpt_gesture_begin = sculpt_gesture_trim_begin;
  trim_operation->op.sculpt_gesture_apply_for_symmetry_pass =
      sculpt_gesture_trim_apply_for_symmetry_pass;
  trim_operation->op.sculpt_gesture_end = sculpt_gesture_trim_end;

  trim_operation->mode = eSculptTrimOperationType(RNA_enum_get(op->ptr, "trim_mode"));
  trim_operation->use_cursor_depth = RNA_boolean_get(op->ptr, "use_cursor_depth");
  trim_operation->orientation = eSculptTrimOrientationType(
      RNA_enum_get(op->ptr, "trim_orientation"));
  trim_operation->extrude_mode = eSculptTrimExtrudeMode(
      RNA_enum_get(op->ptr, "trim_extrude_mode"));

  /* If the cursor was not over the mesh, force the orientation to view. */
  if (!sgcontext->ss->gesture_initial_hit) {
    trim_operation->orientation = SCULPT_GESTURE_TRIM_ORIENTATION_VIEW;
  }
}

static void sculpt_trim_gesture_operator_properties(wmOperatorType *ot)
{
  RNA_def_enum(ot->srna,
               "trim_mode",
               prop_trim_operation_types,
               SCULPT_GESTURE_TRIM_DIFFERENCE,
               "Trim Mode",
               nullptr);
  RNA_def_boolean(
      ot->srna,
      "use_cursor_depth",
      false,
      "Use Cursor for Depth",
      "Use cursor location and radius for the dimensions and position of the trimming shape");
  RNA_def_enum(ot->srna,
               "trim_orientation",
               prop_trim_orientation_types,
               SCULPT_GESTURE_TRIM_ORIENTATION_VIEW,
               "Shape Orientation",
               nullptr);
  RNA_def_enum(ot->srna,
               "trim_extrude_mode",
               prop_trim_extrude_modes,
               SCULPT_GESTURE_TRIM_EXTRUDE_FIXED,
               "Extrude Mode",
               nullptr);
}

/* Project Gesture Operation. */

struct SculptGestureProjectOperation {
  SculptGestureOperation operation;
};

static void sculpt_gesture_project_begin(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, sgcontext->vc.obact, false);
}

static void project_line_gesture_apply_task(SculptGestureContext *sgcontext, PBVHNode *node)
{
  PBVHVertexIter vd;
  bool any_updated = false;

  undo::push_node(sgcontext->vc.obact, node, undo::Type::Position);

  BKE_pbvh_vertex_iter_begin (sgcontext->ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    float vertex_normal[3];
    const float *co = SCULPT_vertex_co_get(sgcontext->ss, vd.vertex);
    SCULPT_vertex_normal_get(sgcontext->ss, vd.vertex, vertex_normal);

    if (!sculpt_gesture_is_effected(sgcontext, co, vertex_normal)) {
      continue;
    }

    float projected_pos[3];
    closest_to_plane_v3(projected_pos, sgcontext->line.plane, vd.co);

    float disp[3];
    sub_v3_v3v3(disp, projected_pos, vd.co);
    const float mask = vd.mask;
    mul_v3_fl(disp, 1.0f - mask);
    if (is_zero_v3(disp)) {
      continue;
    }
    add_v3_v3(vd.co, disp);
    any_updated = true;
  }
  BKE_pbvh_vertex_iter_end;

  if (any_updated) {
    BKE_pbvh_node_mark_update(node);
  }
}

static void sculpt_gesture_project_apply_for_symmetry_pass(bContext * /*C*/,
                                                           SculptGestureContext *sgcontext)
{
  switch (sgcontext->shape_type) {
    case SCULPT_GESTURE_SHAPE_LINE:
      threading::parallel_for(sgcontext->nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          project_line_gesture_apply_task(sgcontext, sgcontext->nodes[i]);
        }
      });
      break;
    case SCULPT_GESTURE_SHAPE_LASSO:
    case SCULPT_GESTURE_SHAPE_BOX:
      /* Gesture shape projection not implemented yet. */
      BLI_assert(false);
      break;
  }
}

static void sculpt_gesture_project_end(bContext *C, SculptGestureContext *sgcontext)
{
  SculptSession *ss = sgcontext->ss;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, sgcontext->vc.obact, true);
  }

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  SCULPT_flush_update_done(C, sgcontext->vc.obact, SCULPT_UPDATE_COORDS);
}

static void sculpt_gesture_init_project_properties(SculptGestureContext *sgcontext,
                                                   wmOperator * /*op*/)
{
  sgcontext->operation = reinterpret_cast<SculptGestureOperation *>(
      MEM_cnew<SculptGestureFaceSetOperation>(__func__));

  SculptGestureProjectOperation *project_operation = (SculptGestureProjectOperation *)
                                                         sgcontext->operation;

  project_operation->operation.sculpt_gesture_begin = sculpt_gesture_project_begin;
  project_operation->operation.sculpt_gesture_apply_for_symmetry_pass =
      sculpt_gesture_project_apply_for_symmetry_pass;
  project_operation->operation.sculpt_gesture_end = sculpt_gesture_project_end;
}

static int paint_mask_gesture_box_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_box(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_mask_properties(C, sgcontext, op);
  sculpt_gesture_apply(C, sgcontext, op);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int paint_mask_gesture_lasso_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_lasso(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_mask_properties(C, sgcontext, op);
  sculpt_gesture_apply(C, sgcontext, op);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int paint_mask_gesture_line_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_line(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_mask_properties(C, sgcontext, op);
  sculpt_gesture_apply(C, sgcontext, op);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int face_set_gesture_box_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_box(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_face_set_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext, op);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int face_set_gesture_lasso_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_lasso(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_face_set_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext, op);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int sculpt_trim_gesture_box_exec(bContext *C, wmOperator *op)
{
  Object *object = CTX_data_active_object(C);
  SculptSession *ss = object->sculpt;
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    /* Not supported in Multires and Dyntopo. */
    return OPERATOR_CANCELLED;
  }

  if (ss->totvert == 0) {
    /* No geometry to trim or to detect a valid position for the trimming shape. */
    return OPERATOR_CANCELLED;
  }

  SculptGestureContext *sgcontext = sculpt_gesture_init_from_box(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }

  sculpt_gesture_init_trim_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext, op);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int sculpt_trim_gesture_box_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  SCULPT_vertex_random_access_ensure(ss);
  ss->gesture_initial_hit = SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);
  if (ss->gesture_initial_hit) {
    copy_v3_v3(ss->gesture_initial_location, sgi.location);
    copy_v3_v3(ss->gesture_initial_normal, sgi.normal);
  }

  return WM_gesture_box_invoke(C, op, event);
}

static int sculpt_trim_gesture_lasso_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *object = CTX_data_active_object(C);

  BKE_sculpt_update_object_for_edit(depsgraph, object, false);

  SculptSession *ss = object->sculpt;
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    /* Not supported in Multires and Dyntopo. */
    return OPERATOR_CANCELLED;
  }

  if (ss->totvert == 0) {
    /* No geometry to trim or to detect a valid position for the trimming shape. */
    return OPERATOR_CANCELLED;
  }

  SculptGestureContext *sgcontext = sculpt_gesture_init_from_lasso(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_trim_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext, op);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int sculpt_trim_gesture_lasso_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  SCULPT_vertex_random_access_ensure(ss);
  ss->gesture_initial_hit = SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);
  if (ss->gesture_initial_hit) {
    copy_v3_v3(ss->gesture_initial_location, sgi.location);
    copy_v3_v3(ss->gesture_initial_normal, sgi.normal);
  }

  return WM_gesture_lasso_invoke(C, op, event);
}

static int project_gesture_line_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_line(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_project_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext, op);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

void PAINT_OT_mask_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Lasso Gesture";
  ot->idname = "PAINT_OT_mask_lasso_gesture";
  ot->description = "Add mask within the lasso as you move the brush";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = paint_mask_gesture_lasso_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  /* Properties. */
  WM_operator_properties_gesture_lasso(ot);
  sculpt_gesture_operator_properties(ot);

  paint_mask_gesture_operator_properties(ot);
}

void PAINT_OT_mask_box_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Box Gesture";
  ot->idname = "PAINT_OT_mask_box_gesture";
  ot->description = "Add mask within the box as you move the brush";

  ot->invoke = WM_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = paint_mask_gesture_box_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_border(ot);
  sculpt_gesture_operator_properties(ot);

  paint_mask_gesture_operator_properties(ot);
}

void PAINT_OT_mask_line_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Line Gesture";
  ot->idname = "PAINT_OT_mask_line_gesture";
  ot->description = "Add mask to the right of a line as you move the brush";

  ot->invoke = WM_gesture_straightline_active_side_invoke;
  ot->modal = WM_gesture_straightline_oneshot_modal;
  ot->exec = paint_mask_gesture_line_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
  sculpt_gesture_operator_properties(ot);

  paint_mask_gesture_operator_properties(ot);
}

void SCULPT_OT_face_set_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Face Set Lasso Gesture";
  ot->idname = "SCULPT_OT_face_set_lasso_gesture";
  ot->description = "Add face set within the lasso as you move the brush";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = face_set_gesture_lasso_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_DEPENDS_ON_CURSOR;

  /* Properties. */
  WM_operator_properties_gesture_lasso(ot);
  sculpt_gesture_operator_properties(ot);
}

void SCULPT_OT_face_set_box_gesture(wmOperatorType *ot)
{
  ot->name = "Face Set Box Gesture";
  ot->idname = "SCULPT_OT_face_set_box_gesture";
  ot->description = "Add face set within the box as you move the brush";

  ot->invoke = WM_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = face_set_gesture_box_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_border(ot);
  sculpt_gesture_operator_properties(ot);
}

void SCULPT_OT_trim_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Trim Lasso Gesture";
  ot->idname = "SCULPT_OT_trim_lasso_gesture";
  ot->description = "Trims the mesh within the lasso as you move the brush";

  ot->invoke = sculpt_trim_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = sculpt_trim_gesture_lasso_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  /* Properties. */
  WM_operator_properties_gesture_lasso(ot);
  sculpt_gesture_operator_properties(ot);

  sculpt_trim_gesture_operator_properties(ot);
}

void SCULPT_OT_trim_box_gesture(wmOperatorType *ot)
{
  ot->name = "Trim Box Gesture";
  ot->idname = "SCULPT_OT_trim_box_gesture";
  ot->description = "Trims the mesh within the box as you move the brush";

  ot->invoke = sculpt_trim_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = sculpt_trim_gesture_box_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_border(ot);
  sculpt_gesture_operator_properties(ot);

  sculpt_trim_gesture_operator_properties(ot);
}

void SCULPT_OT_project_line_gesture(wmOperatorType *ot)
{
  ot->name = "Project Line Gesture";
  ot->idname = "SCULPT_OT_project_line_gesture";
  ot->description = "Project the geometry onto a plane defined by a line";

  ot->invoke = WM_gesture_straightline_active_side_invoke;
  ot->modal = WM_gesture_straightline_oneshot_modal;
  ot->exec = project_gesture_line_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
  sculpt_gesture_operator_properties(ot);
}

}  // namespace blender::ed::sculpt_paint::mask
