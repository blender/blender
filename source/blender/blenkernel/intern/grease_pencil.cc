/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <iostream>
#include <optional>

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_asset_edit.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_attribute_storage_blend_write.hh"
#include "BKE_bake_data_block_id.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_fcurve.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_color_types.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_euler_types.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memarena.h"
#include "BLI_memory_utils.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_resource_scope.hh"
#include "BLI_span.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"
#include "BLI_virtual_array.hh"

#include "BLO_read_write.hh"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_ID_enums.h"
#include "DNA_brush_types.h"
#include "DNA_defaults.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "MEM_guardedalloc.h"

#include "attribute_storage_access.hh"

using blender::float3;
using blender::int3;
using blender::Span;
using blender::VectorSet;

static const char *ATTR_POSITION = "position";

/* Forward declarations. */
static void read_drawing_array(GreasePencil &grease_pencil, BlendDataReader *reader);
static void write_drawing_array(GreasePencil &grease_pencil,
                                blender::ResourceScope &scope,
                                BlendWriter *writer);
static void free_drawing_array(GreasePencil &grease_pencil);

static void read_layer_tree(GreasePencil &grease_pencil, BlendDataReader *reader);
static void write_layer_tree(GreasePencil &grease_pencil, BlendWriter *writer);

static void grease_pencil_init_data(ID *id)
{
  using namespace blender::bke;

  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(grease_pencil, id));

  MEMCPY_STRUCT_AFTER(grease_pencil, DNA_struct_default_get(GreasePencil), id);

  grease_pencil->root_group_ptr = MEM_new<greasepencil::LayerGroup>(__func__);
  grease_pencil->set_active_node(nullptr);

  CustomData_reset(&grease_pencil->layers_data_legacy);
  new (&grease_pencil->attribute_storage.wrap()) blender::bke::AttributeStorage();

  grease_pencil->runtime = MEM_new<GreasePencilRuntime>(__func__);
}

/* See if the layer visibility is animated. This is determined whenever a copy is made, so that
 * this happens in the "create evaluation copy" node of the depsgraph. */
static void grease_pencil_set_runtime_visibilities(ID &id_dst, GreasePencil &grease_pencil)
{
  using namespace blender::bke;

  if (!DEG_is_evaluated(&id_dst) || !grease_pencil.adt) {
    return;
  }

  PropertyRNA *layer_hide_prop = RNA_struct_type_find_property(&RNA_GreasePencilLayer, "hide");
  BLI_assert_msg(layer_hide_prop,
                 "RNA struct GreasePencilLayer is expected to have a 'hide' property.");
  PropertyRNA *group_hide_prop = RNA_struct_type_find_property(&RNA_GreasePencilLayerGroup,
                                                               "hide");
  BLI_assert_msg(group_hide_prop,
                 "RNA struct GreasePencilLayerGroup is expected to have a 'hide' property.");

  for (greasepencil::LayerGroup *layer_group : grease_pencil.layer_groups_for_write()) {
    PointerRNA layer_ptr = RNA_pointer_create_discrete(
        &id_dst, &RNA_GreasePencilLayerGroup, layer_group);
    std::optional<std::string> rna_path = RNA_path_from_ID_to_property(&layer_ptr,
                                                                       group_hide_prop);
    BLI_assert_msg(
        rna_path,
        "It should be possible to construct the RNA path of a grease pencil layer group.");

    layer_group->runtime->is_visibility_animated_ = animdata::prop_is_animated(
        grease_pencil.adt, rna_path->c_str(), 0);
  }

  std::function<bool(greasepencil::LayerGroup &)> parent_group_visibility_animated =
      [&](greasepencil::LayerGroup &parent) {
        if (parent.runtime->is_visibility_animated_) {
          return true;
        }
        greasepencil::LayerGroup *parent_group = parent.as_node().parent_group();
        if (parent_group) {
          return parent_group_visibility_animated(*parent_group);
        }
        return false;
      };

  for (greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
    if (parent_group_visibility_animated(layer->parent_group())) {
      layer->runtime->is_visibility_animated_ = true;
      continue;
    }
    PointerRNA layer_ptr = RNA_pointer_create_discrete(&id_dst, &RNA_GreasePencilLayer, layer);
    std::optional<std::string> rna_path = RNA_path_from_ID_to_property(&layer_ptr,
                                                                       layer_hide_prop);
    BLI_assert_msg(rna_path,
                   "It should be possible to construct the RNA path of a grease pencil layer.");
    layer->runtime->is_visibility_animated_ = animdata::prop_is_animated(
        grease_pencil.adt, rna_path.value(), 0);
  }
}

static void grease_pencil_initialize_drawing_user_counts_after_read(GreasePencil &grease_pencil)
{
  using namespace blender;
  using namespace blender::bke::greasepencil;
  const Array<int> user_counts = grease_pencil.count_frame_users_for_drawings();
  BLI_assert(user_counts.size() == grease_pencil.drawings().size());
  for (const int drawing_i : grease_pencil.drawings().index_range()) {
    GreasePencilDrawingBase *drawing_base = grease_pencil.drawing(drawing_i);
    if (drawing_base->type != GP_DRAWING_REFERENCE) {
      Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap();
      drawing.runtime->user_count.store(user_counts[drawing_i]);
    }
  }
}

static void grease_pencil_copy_data(Main * /*bmain*/,
                                    std::optional<Library *> /*owner_library*/,
                                    ID *id_dst,
                                    const ID *id_src,
                                    const int /*flag*/)
{
  using namespace blender;

  GreasePencil *grease_pencil_dst = reinterpret_cast<GreasePencil *>(id_dst);
  const GreasePencil *grease_pencil_src = reinterpret_cast<const GreasePencil *>(id_src);

  /* Duplicate material array. */
  grease_pencil_dst->material_array = static_cast<Material **>(
      MEM_dupallocN(grease_pencil_src->material_array));

  BKE_grease_pencil_duplicate_drawing_array(grease_pencil_src, grease_pencil_dst);

  /* Duplicate layer tree. */
  grease_pencil_dst->root_group_ptr = MEM_new<bke::greasepencil::LayerGroup>(
      __func__, grease_pencil_src->root_group());

  /* Set active node. */
  if (grease_pencil_src->get_active_node()) {
    bke::greasepencil::TreeNode *active_node = grease_pencil_dst->find_node_by_name(
        grease_pencil_src->get_active_node()->name());
    BLI_assert(active_node);
    grease_pencil_dst->set_active_node(active_node);
  }

  new (&grease_pencil_dst->attribute_storage.wrap())
      blender::bke::AttributeStorage(grease_pencil_src->attribute_storage.wrap());

  BKE_defgroup_copy_list(&grease_pencil_dst->vertex_group_names,
                         &grease_pencil_src->vertex_group_names);

  /* Make sure the runtime pointer exists. */
  grease_pencil_dst->runtime = MEM_new<bke::GreasePencilRuntime>(__func__);

  if (grease_pencil_src->runtime->bake_materials) {
    grease_pencil_dst->runtime->bake_materials = std::make_unique<bke::bake::BakeMaterialsList>(
        *grease_pencil_src->runtime->bake_materials);
  }

  grease_pencil_set_runtime_visibilities(*id_dst, *grease_pencil_dst);
}

static void grease_pencil_free_data(ID *id)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
  BKE_animdata_free(&grease_pencil->id, false);

  MEM_SAFE_FREE(grease_pencil->material_array);

  grease_pencil->attribute_storage.wrap().~AttributeStorage();

  free_drawing_array(*grease_pencil);
  MEM_delete(&grease_pencil->root_group());

  BLI_freelistN(&grease_pencil->vertex_group_names);

  BKE_grease_pencil_batch_cache_free(grease_pencil);

  MEM_delete(grease_pencil->runtime);
  grease_pencil->runtime = nullptr;
}

static void grease_pencil_foreach_id(ID *id, LibraryForeachIDData *data)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
  for (int i = 0; i < grease_pencil->material_array_num; i++) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, grease_pencil->material_array[i], IDWALK_CB_USER);
  }
  for (GreasePencilDrawingBase *drawing_base : grease_pencil->drawings()) {
    if (drawing_base->type == GP_DRAWING_REFERENCE) {
      GreasePencilDrawingReference *drawing_reference =
          reinterpret_cast<GreasePencilDrawingReference *>(drawing_base);
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, drawing_reference->id_reference, IDWALK_CB_USER);
    }
  }
  for (const blender::bke::greasepencil::Layer *layer : grease_pencil->layers()) {
    if (layer->parent) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, layer->parent, IDWALK_CB_USER);
    }
  }
}

static void grease_pencil_foreach_working_space_color(ID *id,
                                                      const IDTypeForeachColorFunctionCallback &fn)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);

  fn.single(grease_pencil->onion_skinning_settings.color_after);
  fn.single(grease_pencil->onion_skinning_settings.color_before);

  for (blender::bke::greasepencil::TreeNode *node : grease_pencil->nodes_for_write()) {
    fn.single(node->color);
  }

  grease_pencil->attribute_storage.wrap().foreach_working_space_color(fn);
}

static void grease_pencil_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  using namespace blender;
  using namespace blender::bke;
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);

  blender::ResourceScope scope;

  blender::Vector<CustomDataLayer, 16> layers_data_layers;
  blender::bke::AttributeStorage::BlendWriteData attribute_data{scope};
  attribute_storage_blend_write_prepare(grease_pencil->attribute_storage.wrap(), attribute_data);
  grease_pencil->attribute_storage.dna_attributes = attribute_data.attributes.data();
  grease_pencil->attribute_storage.dna_attributes_num = attribute_data.attributes.size();

  CustomData_reset(&grease_pencil->layers_data_legacy);

  /* Write LibData */
  BLO_write_id_struct(writer, GreasePencil, id_address, &grease_pencil->id);
  BKE_id_blend_write(writer, &grease_pencil->id);

  grease_pencil->attribute_storage.wrap().blend_write(*writer, attribute_data);

  /* Write drawings. */
  write_drawing_array(*grease_pencil, scope, writer);
  /* Write layer tree. */
  write_layer_tree(*grease_pencil, writer);

  /* Write materials. */
  BLO_write_pointer_array(
      writer, grease_pencil->material_array_num, grease_pencil->material_array);
  /* Write vertex group names. */
  BKE_defbase_blend_write(writer, &grease_pencil->vertex_group_names);
}

static void grease_pencil_blend_read_data(BlendDataReader *reader, ID *id)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);

  /* Read drawings. */
  read_drawing_array(*grease_pencil, reader);
  /* Read layer tree. */
  read_layer_tree(*grease_pencil, reader);
  /* Initialize drawing user counts */
  grease_pencil_initialize_drawing_user_counts_after_read(*grease_pencil);

  CustomData_blend_read(
      reader, &grease_pencil->layers_data_legacy, grease_pencil->layers().size());
  grease_pencil->attribute_storage.wrap().blend_read(*reader);

  /* Read materials. */
  BLO_read_pointer_array(reader,
                         grease_pencil->material_array_num,
                         reinterpret_cast<void **>(&grease_pencil->material_array));
  /* Read vertex group names. */
  BLO_read_struct_list(reader, bDeformGroup, &grease_pencil->vertex_group_names);

  grease_pencil->runtime = MEM_new<blender::bke::GreasePencilRuntime>(__func__);
}

IDTypeInfo IDType_ID_GP = {
    /*id_code*/ GreasePencil::id_type,
    /*id_filter*/ FILTER_ID_GP,
    /*dependencies_id_types*/ FILTER_ID_GP | FILTER_ID_MA | FILTER_ID_OB,
    /*main_listbase_index*/ INDEX_ID_GP,
    /*struct_size*/ sizeof(GreasePencil),
    /*name*/ "GreasePencil",
    /*name_plural*/ N_("grease_pencils"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_GPENCIL,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ grease_pencil_init_data,
    /*copy_data*/ grease_pencil_copy_data,
    /*free_data*/ grease_pencil_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ grease_pencil_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ grease_pencil_foreach_working_space_color,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ grease_pencil_blend_write,
    /*blend_read_data*/ grease_pencil_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

namespace blender::bke::greasepencil {
constexpr StringRef ATTR_RADIUS = "radius";
constexpr StringRef ATTR_OPACITY = "opacity";
constexpr StringRef ATTR_VERTEX_COLOR = "vertex_color";
constexpr StringRef ATTR_FILL_COLOR = "fill_color";

Drawing::Drawing()
{
  this->base.type = GP_DRAWING;
  this->base.flag = 0;

  new (&this->geometry) bke::CurvesGeometry();
  /* Initialize runtime data. */
  this->runtime = MEM_new<bke::greasepencil::DrawingRuntime>(__func__);
}

Drawing::Drawing(const Drawing &other)
{
  this->base.type = GP_DRAWING;
  this->base.flag = other.base.flag;

  new (&this->geometry) bke::CurvesGeometry(other.strokes());
  /* Initialize runtime data. */
  this->runtime = MEM_new<bke::greasepencil::DrawingRuntime>(__func__);

  this->runtime->triangle_offsets_cache = other.runtime->triangle_offsets_cache;
  this->runtime->triangles_cache = other.runtime->triangles_cache;
  this->runtime->curve_plane_normals_cache = other.runtime->curve_plane_normals_cache;
  this->runtime->curve_texture_matrices = other.runtime->curve_texture_matrices;
}

Drawing::Drawing(Drawing &&other)
{
  this->base.type = GP_DRAWING;
  other.base.type = GP_DRAWING;
  this->base.flag = other.base.flag;
  other.base.flag = 0;

  new (&this->geometry) bke::CurvesGeometry(std::move(other.geometry.wrap()));

  this->runtime = other.runtime;
  other.runtime = nullptr;
}

Drawing &Drawing::operator=(const Drawing &other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) Drawing(other);
  return *this;
}

Drawing &Drawing::operator=(Drawing &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) Drawing(std::move(other));
  return *this;
}

Drawing::~Drawing()
{
  this->strokes().~CurvesGeometry();
  MEM_delete(this->runtime);
  this->runtime = nullptr;
}

OffsetIndices<int> Drawing::triangle_offsets() const
{
  this->runtime->triangle_offsets_cache.ensure([&](Vector<int> &r_offsets) {
    const CurvesGeometry &curves = this->strokes();
    const OffsetIndices<int> points_by_curve = curves.evaluated_points_by_curve();

    int offset = 0;
    r_offsets.reinitialize(curves.curves_num() + 1);
    for (const int curve_i : points_by_curve.index_range()) {
      const IndexRange points = points_by_curve[curve_i];
      r_offsets[curve_i] = offset;
      offset += std::max(int(points.size() - 2), 0);
    }
    r_offsets.last() = offset;
  });
  return this->runtime->triangle_offsets_cache.data().as_span();
}

static void update_triangle_cache(const Span<float3> positions,
                                  const Span<float3> normals,
                                  const OffsetIndices<int> points_by_curve,
                                  const OffsetIndices<int> triangle_offsets,
                                  const IndexMask &curve_mask,
                                  MutableSpan<int3> triangles)
{
  struct LocalMemArena {
    MemArena *pf_arena = nullptr;
    LocalMemArena() : pf_arena(BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "Drawing::triangles")) {}

    ~LocalMemArena()
    {
      if (pf_arena != nullptr) {
        BLI_memarena_free(pf_arena);
      }
    }
  };
  threading::EnumerableThreadSpecific<LocalMemArena> all_local_mem_arenas;
  curve_mask.foreach_segment(GrainSize(32), [&](const IndexMaskSegment mask_segment) {
    MemArena *pf_arena = all_local_mem_arenas.local().pf_arena;
    for (const int curve_i : mask_segment) {
      const IndexRange points = points_by_curve[curve_i];
      if (points.size() < 3) {
        continue;
      }
      MutableSpan<int3> r_tris = triangles.slice(triangle_offsets[curve_i]);

      float (*projverts)[2] = static_cast<float (*)[2]>(
          BLI_memarena_alloc(pf_arena, sizeof(*projverts) * size_t(points.size())));

      float3x3 axis_mat;
      axis_dominant_v3_to_m3(axis_mat.ptr(), normals[curve_i]);

      for (const int i : IndexRange(points.size())) {
        mul_v2_m3v3(projverts[i], axis_mat.ptr(), positions[points[i]]);
      }

      BLI_polyfill_calc_arena(
          projverts, points.size(), 0, reinterpret_cast<uint32_t (*)[3]>(r_tris.data()), pf_arena);
      BLI_memarena_clear(pf_arena);
    }
  });
}

Span<int3> Drawing::triangles() const
{
  const CurvesGeometry &curves = this->strokes();
  const OffsetIndices<int> triangle_offsets = this->triangle_offsets();
  this->runtime->triangles_cache.ensure([&](Vector<int3> &r_data) {
    const int total_triangles = triangle_offsets.total_size();
    r_data.resize(total_triangles);

    update_triangle_cache(curves.evaluated_positions(),
                          this->curve_plane_normals(),
                          curves.evaluated_points_by_curve(),
                          triangle_offsets,
                          curves.curves_range(),
                          r_data.as_mutable_span());
  });

  return this->runtime->triangles_cache.data().as_span();
}

static void update_curve_plane_normal_cache(const Span<float3> positions,
                                            const OffsetIndices<int> points_by_curve,
                                            const IndexMask &curve_mask,
                                            MutableSpan<float3> normals)
{
  curve_mask.foreach_index(GrainSize(512), [&](const int curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    if (points.size() < 2) {
      normals[curve_i] = float3(1.0f, 0.0f, 0.0f);
      return;
    }

    /* Calculate normal using Newell's method. */
    float3 normal(0.0f);
    float3 prev_point = positions[points.last()];
    for (const int point_i : points) {
      const float3 curr_point = positions[point_i];
      add_newell_cross_v3_v3v3(normal, prev_point, curr_point);
      prev_point = curr_point;
    }

    float length;
    normal = math::normalize_and_get_length(normal, length);
    /* Check for degenerate case where the points are on a line. */
    if (math::is_zero(length)) {
      for (const int point_i : points.drop_back(1)) {
        float3 segment_vec = positions[point_i] - positions[point_i + 1];
        if (math::length_squared(segment_vec) != 0.0f) {
          normal = math::normalize(float3(segment_vec.y, -segment_vec.x, 0.0f));
          break;
        }
      }
    }

    normals[curve_i] = normal;
  });
}

Span<float3> Drawing::curve_plane_normals() const
{
  this->runtime->curve_plane_normals_cache.ensure([&](Vector<float3> &r_data) {
    const CurvesGeometry &curves = this->strokes();
    r_data.reinitialize(curves.curves_num());
    update_curve_plane_normal_cache(curves.positions(),
                                    curves.points_by_curve(),
                                    curves.curves_range(),
                                    r_data.as_mutable_span());
  });
  return this->runtime->curve_plane_normals_cache.data().as_span();
}

/*
 * Returns the matrix that transforms from a 3D point in layer-space to a 2D point in
 * stroke-space for the stroke `curve_i`
 */
static float4x2 get_local_to_stroke_matrix(const Span<float3> positions, const float3 normal)
{
  using namespace blender::math;

  if (positions.size() <= 2) {
    return float4x2::identity();
  }

  const float3 point_0 = positions[0];
  const float3 point_1 = positions[1];

  /* Local X axis (p0 -> p1) */
  const float3 local_x = normalize(point_1 - point_0);
  /* Local Y axis (cross to normal/x axis). */
  const float3 local_y = normalize(cross(normal, local_x));

  if (length_squared(local_x) == 0.0f || length_squared(local_y) == 0.0f) {
    return float4x2::identity();
  }

  /* Get local space using first point as origin. */
  const float4x2 mat = transpose(
      float2x4(float4(local_x, -dot(point_0, local_x)), float4(local_y, -dot(point_0, local_y))));

  return mat;
}

/*
 * Returns the matrix that transforms from a 2D point in stroke-space to a 2D point in
 * texture-space for a stroke `curve_i`
 */
static float3x2 get_stroke_to_texture_matrix(const float uv_rotation,
                                             const float2 uv_translation,
                                             const float2 uv_scale)
{
  using namespace blender::math;

  const float2 uv_scale_inv = safe_rcp(uv_scale);
  const float s = sin(uv_rotation);
  const float c = cos(uv_rotation);
  const float2x2 rot = float2x2(float2(c, s), float2(-s, c));

  float3x2 texture_matrix = float3x2::identity();
  /*
   * The order in which the three transforms are applied has been carefully chosen to be easy to
   * invert.
   *
   * The translation is applied last so that the origin goes to `uv_translation`
   * The rotation is applied after the scale so that the `u` direction's angle is `uv_rotation`
   * Scale is the only transform that changes the length of the basis vectors and if it is applied
   * first it's independent of the other transforms.
   *
   * These properties are not true with a different order.
   */

  /* Apply scale. */
  texture_matrix = from_scale<float2x2>(uv_scale_inv) * texture_matrix;

  /* Apply rotation. */
  texture_matrix = rot * texture_matrix;

  /* Apply translation. */
  texture_matrix[2] += uv_translation;

  return texture_matrix;
}

static float4x3 expand_4x2_mat(const float4x2 &strokemat)
{
  float4x3 strokemat4x3 = float4x3(strokemat);

  /*
   * We need the diagonal of ones to start from the bottom right instead top left to properly
   * apply the two matrices.
   *
   * i.e.
   *          # # # #              # # # #
   * We need  # # # #  Instead of  # # # #
   *          0 0 0 1              0 0 1 0
   *
   */
  strokemat4x3[2][2] = 0.0f;
  strokemat4x3[3][2] = 1.0f;

  return strokemat4x3;
}

Span<float4x2> Drawing::texture_matrices() const
{
  this->runtime->curve_texture_matrices.ensure([&](Vector<float4x2> &r_data) {
    const CurvesGeometry &curves = this->strokes();
    const AttributeAccessor attributes = curves.attributes();

    const VArray<float> uv_rotations = *attributes.lookup_or_default<float>(
        "uv_rotation", AttrDomain::Curve, 0.0f);
    const VArray<float2> uv_translations = *attributes.lookup_or_default<float2>(
        "uv_translation", AttrDomain::Curve, float2(0.0f, 0.0f));
    const VArray<float2> uv_scales = *attributes.lookup_or_default<float2>(
        "uv_scale", AttrDomain::Curve, float2(1.0f, 1.0f));

    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const Span<float3> positions = curves.positions();
    const Span<float3> normals = this->curve_plane_normals();

    r_data.reinitialize(curves.curves_num());
    threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = points_by_curve[curve_i];
        const float3 normal = normals[curve_i];
        const float4x2 strokemat = get_local_to_stroke_matrix(positions.slice(points), normal);
        const float3x2 texture_matrix = get_stroke_to_texture_matrix(
            uv_rotations[curve_i], uv_translations[curve_i], uv_scales[curve_i]);

        const float4x2 texspace = texture_matrix * expand_4x2_mat(strokemat);

        r_data[curve_i] = texspace;
      }
    });
  });
  return this->runtime->curve_texture_matrices.data().as_span();
}

void Drawing::set_texture_matrices(Span<float4x2> matrices, const IndexMask &selection)
{
  CurvesGeometry &curves = this->strokes_for_write();
  MutableAttributeAccessor attributes = curves.attributes_for_write();
  SpanAttributeWriter<float> uv_rotations = attributes.lookup_or_add_for_write_span<float>(
      "uv_rotation", AttrDomain::Curve);
  SpanAttributeWriter<float2> uv_translations = attributes.lookup_or_add_for_write_span<float2>(
      "uv_translation", AttrDomain::Curve);
  SpanAttributeWriter<float2> uv_scales = attributes.lookup_or_add_for_write_span<float2>(
      "uv_scale",
      AttrDomain::Curve,
      AttributeInitVArray(VArray<float2>::from_single(float2(1.0f, 1.0f), curves.curves_num())));

  if (!uv_rotations || !uv_translations || !uv_scales) {
    /* FIXME: It might be better to ensure the attributes exist and are on the right domain. */
    return;
  }

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const Span<float3> positions = curves.positions();
  const Span<float3> normals = this->curve_plane_normals();

  selection.foreach_index(GrainSize(256), [&](const int64_t curve_i, const int64_t pos) {
    const IndexRange points = points_by_curve[curve_i];
    const float3 normal = normals[curve_i];
    const float4x2 strokemat = get_local_to_stroke_matrix(positions.slice(points), normal);
    const float4x2 texspace = matrices[pos];

    /* We do the computation using doubles to avoid numerical precision errors. */
    const double4x3 strokemat4x3 = double4x3(expand_4x2_mat(strokemat));

    /*
     * We want to solve for `texture_matrix` in the equation:
     * `texspace = texture_matrix * strokemat4x3`
     * Because these matrices are not square we can not use a standard inverse.
     *
     * Our problem has the form of: `X = A * Y`
     * We can solve for `A` using: `A = X * B`
     *
     * Where `B` is the Right-sided inverse or Moore-Penrose pseudo inverse.
     * Calculated as:
     *
     *  |--------------------------|
     *  | B = T(Y) * (Y * T(Y))^-1 |
     *  |--------------------------|
     *
     * And `T()` is transpose and `()^-1` is the inverse.
     */

    const double3x4 transpose_strokemat = math::transpose(strokemat4x3);
    const double3x4 right_inverse = transpose_strokemat *
                                    math::invert(strokemat4x3 * transpose_strokemat);

    const float3x2 texture_matrix = float3x2(double4x2(texspace) * right_inverse);

    /* Solve for translation, the translation is simply the origin. */
    const float2 uv_translation = texture_matrix[2];

    /* Solve rotation, the angle of the `u` basis is the rotation. */
    const float uv_rotation = math::atan2(texture_matrix[0][1], texture_matrix[0][0]);

    /* Calculate the determinant to check if the `v` scale is negative. */
    const float det = math::determinant(float2x2(texture_matrix));

    /* Solve scale, scaling is the only transformation that changes the length, so scale factor
     * is simply the length. And flip the sign of `v` if the determinant is negative. */
    const float2 uv_scale = math::safe_rcp(float2(
        math::length(texture_matrix[0]), math::sign(det) * math::length(texture_matrix[1])));

    uv_rotations.span[curve_i] = uv_rotation;
    uv_translations.span[curve_i] = uv_translation;
    uv_scales.span[curve_i] = uv_scale;
  });
  uv_rotations.finish();
  uv_translations.finish();
  uv_scales.finish();

  this->tag_texture_matrices_changed();
}

const bke::CurvesGeometry &Drawing::strokes() const
{
  return this->geometry.wrap();
}

bke::CurvesGeometry &Drawing::strokes_for_write()
{
  return this->geometry.wrap();
}

VArray<float> Drawing::radii() const
{
  return *this->strokes().attributes().lookup_or_default<float>(
      ATTR_RADIUS, AttrDomain::Point, 0.01f);
}

MutableSpan<float> Drawing::radii_for_write()
{
  return blender::bke::get_mutable_attribute<float>(
      this->strokes_for_write().attribute_storage.wrap(),
      AttrDomain::Point,
      ATTR_RADIUS,
      this->strokes().points_num(),
      0.01f);
}

VArray<float> Drawing::opacities() const
{
  return *this->strokes().attributes().lookup_or_default<float>(
      ATTR_OPACITY, AttrDomain::Point, 1.0f);
}

MutableSpan<float> Drawing::opacities_for_write()
{
  return blender::bke::get_mutable_attribute<float>(
      this->strokes_for_write().attribute_storage.wrap(),
      AttrDomain::Point,
      ATTR_OPACITY,
      this->strokes().points_num(),
      1.0f);
}

VArray<ColorGeometry4f> Drawing::vertex_colors() const
{
  return *this->strokes().attributes().lookup_or_default<ColorGeometry4f>(
      ATTR_VERTEX_COLOR, AttrDomain::Point, ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
}

MutableSpan<ColorGeometry4f> Drawing::vertex_colors_for_write()
{
  return blender::bke::get_mutable_attribute<ColorGeometry4f>(
      this->strokes_for_write().attribute_storage.wrap(),
      AttrDomain::Point,
      ATTR_VERTEX_COLOR,
      this->strokes().points_num(),
      ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
}

VArray<ColorGeometry4f> Drawing::fill_colors() const
{
  return *this->strokes().attributes().lookup_or_default<ColorGeometry4f>(
      ATTR_FILL_COLOR, AttrDomain::Curve, ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
}

MutableSpan<ColorGeometry4f> Drawing::fill_colors_for_write()
{
  return blender::bke::get_mutable_attribute<ColorGeometry4f>(
      this->strokes_for_write().attribute_storage.wrap(),
      AttrDomain::Curve,
      ATTR_FILL_COLOR,
      this->strokes().curves_num(),
      ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
}

void Drawing::tag_texture_matrices_changed()
{
  this->runtime->curve_texture_matrices.tag_dirty();
}

void Drawing::tag_positions_changed()
{
  this->strokes_for_write().tag_positions_changed();
  this->runtime->curve_plane_normals_cache.tag_dirty();
  this->runtime->triangles_cache.tag_dirty();
  this->tag_texture_matrices_changed();
}

void Drawing::tag_positions_changed(const IndexMask &changed_curves)
{
  if (changed_curves.is_empty()) {
    return;
  }
  /* If more than half of the curves have changed, update the entire cache instead.
   * The assumption here is that it's better to lazily compute the caches if more than half of the
   * curves need to be updated.
   * TODO: This could probably be a bit more rigorous once this function gets used in more places.
   */
  if (changed_curves.size() > this->strokes().curves_num() / 2) {
    this->tag_positions_changed();
    return;
  }
  if (!this->runtime->triangles_cache.is_cached() ||
      !this->runtime->curve_plane_normals_cache.is_cached())
  {
    this->tag_positions_changed();
    return;
  }
  /* Positions needs to be tagged first, because the triangle cache updates just after need the
   * positions to be up-to-date. */
  this->strokes_for_write().tag_positions_changed();
  this->runtime->curve_plane_normals_cache.update([&](Vector<float3> &normals) {
    const CurvesGeometry &curves = this->strokes();
    update_curve_plane_normal_cache(
        curves.positions(), curves.points_by_curve(), changed_curves, normals);
  });
  this->runtime->triangles_cache.update([&](Vector<int3> &triangles) {
    const CurvesGeometry &curves = this->strokes();
    update_triangle_cache(curves.evaluated_positions(),
                          this->curve_plane_normals(),
                          curves.evaluated_points_by_curve(),
                          this->triangle_offsets(),
                          curves.curves_range(),
                          triangles);
  });
  this->tag_texture_matrices_changed();
}

void Drawing::tag_topology_changed()
{
  this->runtime->triangle_offsets_cache.tag_dirty();
  this->tag_positions_changed();
  this->strokes_for_write().tag_topology_changed();
}

void Drawing::tag_topology_changed(const IndexMask &changed_curves)
{
  if (changed_curves.is_empty()) {
    return;
  }
  /* If more than half of the curves have changed, update the entire cache instead.
   * The assumption here is that it's better to lazily compute the caches if more than half of the
   * curves need to be updated.
   * TODO: This could probably be a bit more rigorous once this function gets used in more places.
   */
  if (changed_curves.size() > this->strokes().curves_num() / 2) {
    this->tag_topology_changed();
    return;
  }
  if (!this->runtime->triangles_cache.is_cached() ||
      !this->runtime->curve_plane_normals_cache.is_cached())
  {
    this->tag_topology_changed();
    return;
  }
  /* Positions needs to be tagged first, because the triangle cache updates just after need the
   * positions to be up-to-date. */
  this->strokes_for_write().tag_positions_changed();
  this->runtime->curve_plane_normals_cache.update([&](Vector<float3> &normals) {
    const CurvesGeometry &curves = this->strokes();
    update_curve_plane_normal_cache(
        curves.positions(), curves.points_by_curve(), changed_curves, normals);
  });
  /* Copy the current triangle offsets. These are used to copy over the triangle data for curves
   * that don't need to be updated. */
  const Array<int> src_triangle_offset_data(this->triangle_offsets().data());
  const OffsetIndices<int> src_triangle_offsets = src_triangle_offset_data.as_span();
  /* Tag the `triangle_offsets_cache` so that the `triangles_cache` update can use the up-to-date
   * triangle offsets. */
  this->runtime->triangle_offsets_cache.tag_dirty();

  this->runtime->triangles_cache.update([&](Vector<int3> &triangles) {
    const CurvesGeometry &curves = this->strokes();
    const OffsetIndices<int> dst_triangle_offsets = this->triangle_offsets();

    IndexMaskMemory memory;
    const IndexMask curves_to_copy = changed_curves.complement(curves.curves_range(), memory);

    const Vector<int3> src_triangles(triangles);
    triangles.reinitialize(dst_triangle_offsets.total_size());
    array_utils::copy_group_to_group(src_triangle_offsets,
                                     dst_triangle_offsets,
                                     curves_to_copy,
                                     src_triangles.as_span(),
                                     triangles.as_mutable_span());

    update_triangle_cache(curves.evaluated_positions(),
                          this->curve_plane_normals(),
                          curves.evaluated_points_by_curve(),
                          dst_triangle_offsets,
                          changed_curves,
                          triangles);
  });
  this->tag_texture_matrices_changed();
}

DrawingReference::DrawingReference()
{
  this->base.type = GP_DRAWING_REFERENCE;
  this->base.flag = 0;

  this->id_reference = nullptr;
}

DrawingReference::DrawingReference(const DrawingReference &other)
{
  this->base.type = GP_DRAWING_REFERENCE;
  this->base.flag = other.base.flag;

  this->id_reference = other.id_reference;
}

DrawingReference::~DrawingReference() = default;

void copy_drawing_array(Span<const GreasePencilDrawingBase *> src_drawings,
                        MutableSpan<GreasePencilDrawingBase *> dst_drawings)
{
  BLI_assert(src_drawings.size() == dst_drawings.size());
  for (const int i : src_drawings.index_range()) {
    const GreasePencilDrawingBase *src_drawing_base = src_drawings[i];
    switch (src_drawing_base->type) {
      case GP_DRAWING: {
        const GreasePencilDrawing *src_drawing = reinterpret_cast<const GreasePencilDrawing *>(
            src_drawing_base);
        dst_drawings[i] = reinterpret_cast<GreasePencilDrawingBase *>(
            MEM_new<bke::greasepencil::Drawing>(__func__, src_drawing->wrap()));
        break;
      }
      case GP_DRAWING_REFERENCE: {
        const GreasePencilDrawingReference *src_drawing_reference =
            reinterpret_cast<const GreasePencilDrawingReference *>(src_drawing_base);
        dst_drawings[i] = reinterpret_cast<GreasePencilDrawingBase *>(
            MEM_new<bke::greasepencil::DrawingReference>(__func__, src_drawing_reference->wrap()));
        break;
      }
    }
  }
}

TreeNode::TreeNode()
{
  this->next = this->prev = nullptr;
  this->parent = nullptr;

  this->GreasePencilLayerTreeNode::name = nullptr;
  this->flag = 0;
  this->color[0] = this->color[1] = this->color[2] = 0;
}

TreeNode::TreeNode(const GreasePencilLayerTreeNodeType type) : TreeNode()
{
  this->type = type;
}

TreeNode::TreeNode(const GreasePencilLayerTreeNodeType type, const StringRef name) : TreeNode()
{
  this->type = type;
  this->GreasePencilLayerTreeNode::name = BLI_strdupn(name.data(), name.size());
}

TreeNode::TreeNode(const TreeNode &other) : TreeNode(GreasePencilLayerTreeNodeType(other.type))
{
  this->GreasePencilLayerTreeNode::name = BLI_strdup_null(other.GreasePencilLayerTreeNode::name);
  this->flag = other.flag;
  copy_v3_v3(this->color, other.color);
}

TreeNode::~TreeNode()
{
  MEM_SAFE_FREE(this->GreasePencilLayerTreeNode::name);
}

void TreeNode::set_name(const StringRef name)
{
  MEM_SAFE_FREE(this->GreasePencilLayerTreeNode::name);
  this->GreasePencilLayerTreeNode::name = BLI_strdupn(name.data(), name.size());
}

const LayerGroup &TreeNode::as_group() const
{
  return *reinterpret_cast<const LayerGroup *>(this);
}

const Layer &TreeNode::as_layer() const
{
  return *reinterpret_cast<const Layer *>(this);
}

LayerGroup &TreeNode::as_group()
{
  return *reinterpret_cast<LayerGroup *>(this);
}

Layer &TreeNode::as_layer()
{
  return *reinterpret_cast<Layer *>(this);
}

const LayerGroup *TreeNode::parent_group() const
{
  return (this->parent) ? &this->parent->wrap() : nullptr;
}
LayerGroup *TreeNode::parent_group()
{
  return (this->parent) ? &this->parent->wrap() : nullptr;
}
const TreeNode *TreeNode::parent_node() const
{
  return this->parent_group() ? &this->parent->wrap().as_node() : nullptr;
}
TreeNode *TreeNode::parent_node()
{
  return this->parent_group() ? &this->parent->wrap().as_node() : nullptr;
}

int64_t TreeNode::depth() const
{
  const LayerGroup *parent = this->parent_group();
  if (parent == nullptr) {
    /* The root group has a depth of 0. */
    return 0;
  }
  return 1 + parent->as_node().depth();
}

LayerMask::LayerMask()
{
  this->layer_name = nullptr;
  this->flag = 0;
}

LayerMask::LayerMask(const StringRef name) : LayerMask()
{
  this->layer_name = BLI_strdupn(name.data(), name.size());
}

LayerMask::LayerMask(const LayerMask &other) : LayerMask()
{
  this->layer_name = BLI_strdup_null(other.layer_name);
  this->flag = other.flag;
}

LayerMask::~LayerMask()
{
  if (this->layer_name) {
    MEM_freeN(this->layer_name);
  }
}

void LayerRuntime::clear()
{
  frames_.clear();
  sorted_keys_cache_.tag_dirty();
  masks_.clear_and_shrink();
  trans_data_ = {};
}

Layer::Layer()
{
  new (&this->base) TreeNode(GP_LAYER_TREE_LEAF);

  this->frames_storage.num = 0;
  this->frames_storage.keys = nullptr;
  this->frames_storage.values = nullptr;
  this->frames_storage.flag = 0;

  this->blend_mode = GP_LAYER_BLEND_NONE;
  this->opacity = 1.0f;

  this->parent = nullptr;
  this->parsubstr = nullptr;
  unit_m4(this->parentinv);

  zero_v3(this->translation);
  zero_v3(this->rotation);
  copy_v3_fl(this->scale, 1.0f);

  this->viewlayername = nullptr;

  BLI_listbase_clear(&this->masks);
  this->active_mask_index = 0;

  this->runtime = MEM_new<LayerRuntime>(__func__);
}

Layer::Layer(const StringRef name) : Layer()
{
  new (&this->base) TreeNode(GP_LAYER_TREE_LEAF, name);
}

Layer::Layer(const Layer &other) : Layer()
{
  new (&this->base) TreeNode(other.base.wrap());

  LISTBASE_FOREACH (GreasePencilLayerMask *, other_mask, &other.masks) {
    LayerMask *new_mask = MEM_new<LayerMask>(__func__, *reinterpret_cast<LayerMask *>(other_mask));
    BLI_addtail(&this->masks, reinterpret_cast<GreasePencilLayerMask *>(new_mask));
  }
  this->active_mask_index = other.active_mask_index;

  this->blend_mode = other.blend_mode;
  this->opacity = other.opacity;

  this->parent = other.parent;
  this->set_parent_bone_name(other.parsubstr);
  copy_m4_m4(this->parentinv, other.parentinv);

  copy_v3_v3(this->translation, other.translation);
  copy_v3_v3(this->rotation, other.rotation);
  copy_v3_v3(this->scale, other.scale);

  this->set_view_layer_name(other.viewlayername);

  /* NOTE: We do not duplicate the frame storage since it is only needed for writing to file. */
  this->runtime->frames_ = other.runtime->frames_;
  this->runtime->sorted_keys_cache_ = other.runtime->sorted_keys_cache_;
  /* Tag the frames map, so the frame storage is recreated once the DNA is saved. */
  this->tag_frames_map_changed();

  /* TODO: what about masks cache? */
}

Layer::~Layer()
{
  this->base.wrap().~TreeNode();

  MEM_SAFE_FREE(this->frames_storage.keys);
  MEM_SAFE_FREE(this->frames_storage.values);

  LISTBASE_FOREACH_MUTABLE (GreasePencilLayerMask *, mask, &this->masks) {
    MEM_delete(reinterpret_cast<LayerMask *>(mask));
  }
  BLI_listbase_clear(&this->masks);

  MEM_SAFE_FREE(this->parsubstr);
  MEM_SAFE_FREE(this->viewlayername);

  MEM_delete(this->runtime);
  this->runtime = nullptr;
}

const Map<int, GreasePencilFrame> &Layer::frames() const
{
  return this->runtime->frames_;
}

Map<int, GreasePencilFrame> &Layer::frames_for_write()
{
  return this->runtime->frames_;
}

Layer::SortedKeysIterator Layer::remove_leading_end_frames_in_range(
    Layer::SortedKeysIterator begin, Layer::SortedKeysIterator end)
{
  Layer::SortedKeysIterator next_it = begin;
  while (next_it != end && this->frames().lookup(*next_it).is_end()) {
    this->frames_for_write().remove_contained(*next_it);
    this->tag_frames_map_keys_changed();
    next_it = std::next(next_it);
  }
  return next_it;
}

GreasePencilFrame *Layer::add_frame_internal(const FramesMapKeyT frame_number)
{
  if (!this->frames().contains(frame_number)) {
    GreasePencilFrame frame{};
    this->frames_for_write().add_new(frame_number, frame);
    this->tag_frames_map_keys_changed();
    return this->frames_for_write().lookup_ptr(frame_number);
  }
  /* Overwrite end-frames. */
  if (this->frames().lookup(frame_number).is_end()) {
    GreasePencilFrame frame{};
    this->frames_for_write().add_overwrite(frame_number, frame);
    this->tag_frames_map_changed();
    return this->frames_for_write().lookup_ptr(frame_number);
  }
  return nullptr;
}

GreasePencilFrame *Layer::add_frame(const FramesMapKeyT key, const int duration)
{
  BLI_assert(duration >= 0);
  GreasePencilFrame *frame = this->add_frame_internal(key);
  if (frame == nullptr) {
    return nullptr;
  }
  Span<FramesMapKeyT> sorted_keys = this->sorted_keys();
  const FramesMapKeyT end_key = key + duration;
  /* Finds the next greater key that is stored in the map. */
  SortedKeysIterator next_key_it = std::upper_bound(sorted_keys.begin(), sorted_keys.end(), key);
  /* If the next frame we found is at the end of the frame we're inserting, then we are done. */
  if (next_key_it != sorted_keys.end() && *next_key_it == end_key) {
    return frame;
  }
  next_key_it = this->remove_leading_end_frames_in_range(next_key_it, sorted_keys.end());
  /* If the duration is set to 0, the frame is marked as an implicit hold. */
  if (duration == 0) {
    frame->flag |= GP_FRAME_IMPLICIT_HOLD;
    return frame;
  }
  /* If the next frame comes after the end of the frame we're inserting (or if there are no more
   * frames), add an end-frame. */
  if (next_key_it == sorted_keys.end() || *next_key_it > end_key) {
    this->frames_for_write().add_new(end_key, GreasePencilFrame::end());
    this->tag_frames_map_keys_changed();
  }
  return frame;
}

bool Layer::remove_frame(const FramesMapKeyT key)
{
  /* If the frame number is not in the frames map, do nothing. */
  if (!this->frames().contains(key)) {
    return false;
  }
  if (this->frames().size() == 1) {
    this->frames_for_write().remove_contained(key);
    this->tag_frames_map_keys_changed();
    return true;
  }
  Span<FramesMapKeyT> sorted_keys = this->sorted_keys();
  /* Find the index of the frame to remove in the `sorted_keys` array. */
  SortedKeysIterator remove_key_it = std::lower_bound(sorted_keys.begin(), sorted_keys.end(), key);
  /* If there is a next frame: */
  if (std::next(remove_key_it) != sorted_keys.end()) {
    SortedKeysIterator next_key_it = std::next(remove_key_it);
    this->remove_leading_end_frames_in_range(next_key_it, sorted_keys.end());
  }
  /* If there is a previous frame: */
  if (remove_key_it != sorted_keys.begin()) {
    SortedKeysIterator prev_key_it = std::prev(remove_key_it);
    const GreasePencilFrame &prev_frame = this->frames().lookup(*prev_key_it);
    /* If the previous frame is not an implicit hold (e.g. it has a fixed duration) and it's not an
     * end frame, we cannot just delete the frame. We need to replace it with an end frame. */
    if (!prev_frame.is_implicit_hold() && !prev_frame.is_end()) {
      this->frames_for_write().lookup(key) = GreasePencilFrame::end();
      this->tag_frames_map_changed();
      /* Since the original frame was replaced with an end frame, we consider the frame to be
       * successfully removed here. */
      return true;
    }
  }
  /* Finally, remove the actual frame. */
  this->frames_for_write().remove_contained(key);
  this->tag_frames_map_keys_changed();
  return true;
}

Span<FramesMapKeyT> Layer::sorted_keys() const
{
  this->runtime->sorted_keys_cache_.ensure([&](Vector<FramesMapKeyT> &r_data) {
    r_data.reinitialize(this->frames().size());
    int i = 0;
    for (const FramesMapKeyT key : this->frames().keys()) {
      r_data[i++] = key;
    }
    std::sort(r_data.begin(), r_data.end());
  });
  return this->runtime->sorted_keys_cache_.data();
}

Layer::SortedKeysIterator Layer::sorted_keys_iterator_at(const int frame_number) const
{
  Span<int> sorted_keys = this->sorted_keys();
  /* No keyframes, return nullptr. */
  if (sorted_keys.is_empty()) {
    return nullptr;
  }
  /* Before the first frame, return nullptr. */
  if (frame_number < sorted_keys.first()) {
    return nullptr;
  }
  /* After or at the last frame, return iterator to last. */
  if (frame_number >= sorted_keys.last()) {
    return std::prev(sorted_keys.end());
  }
  /* Search for the frame. std::upper_bound will get the frame just after. */
  SortedKeysIterator it = std::upper_bound(sorted_keys.begin(), sorted_keys.end(), frame_number);
  if (it == sorted_keys.end()) {
    return nullptr;
  }
  return std::prev(it);
}

std::optional<FramesMapKeyT> Layer::frame_key_at(const int frame_number) const
{
  SortedKeysIterator it = this->sorted_keys_iterator_at(frame_number);
  if (it == nullptr) {
    return {};
  }
  return *it;
}

std::optional<int> Layer::start_frame_at(int frame_number) const
{
  const std::optional<FramesMapKeyT> frame_key = this->frame_key_at(frame_number);
  /* Return the frame number only if the frame key exists and if it's not an end frame. */
  if (frame_key && !this->frames().lookup_ptr(*frame_key)->is_end()) {
    return frame_key;
  }
  return {};
}

int Layer::sorted_keys_index_at(const int frame_number) const
{
  SortedKeysIterator it = this->sorted_keys_iterator_at(frame_number);
  if (it == nullptr) {
    return -1;
  }
  return std::distance(this->sorted_keys().begin(), it);
}

const GreasePencilFrame *Layer::frame_at(const int frame_number) const
{
  const GreasePencilFrame *frame_ptr = [&]() -> const GreasePencilFrame * {
    if (const GreasePencilFrame *frame = this->frames().lookup_ptr(frame_number)) {
      return frame;
    }
    /* Look for a keyframe that starts before `frame_number` and ends after `frame_number`. */
    const std::optional<FramesMapKeyT> frame_key = this->frame_key_at(frame_number);
    if (!frame_key) {
      return nullptr;
    }
    return this->frames().lookup_ptr(*frame_key);
  }();
  if (frame_ptr == nullptr || frame_ptr->is_end()) {
    /* Not a valid frame. */
    return nullptr;
  }
  return frame_ptr;
}

GreasePencilFrame *Layer::frame_at(const int frame_number)
{
  GreasePencilFrame *frame_ptr = [&]() -> GreasePencilFrame * {
    if (GreasePencilFrame *frame = this->frames_for_write().lookup_ptr(frame_number)) {
      return frame;
    }
    /* Look for a keyframe that starts before `frame_number`. */
    const std::optional<FramesMapKeyT> frame_key = this->frame_key_at(frame_number);
    if (!frame_key) {
      return nullptr;
    }
    return this->frames_for_write().lookup_ptr(*frame_key);
  }();
  if (frame_ptr == nullptr || frame_ptr->is_end()) {
    /* Not a valid frame. */
    return nullptr;
  }
  return frame_ptr;
}

int Layer::drawing_index_at(const int frame_number) const
{
  const GreasePencilFrame *frame = frame_at(frame_number);
  return (frame != nullptr) ? frame->drawing_index : -1;
}

bool Layer::has_drawing_at(const int frame_number) const
{
  return frame_at(frame_number) != nullptr;
}

int Layer::get_frame_duration_at(const int frame_number) const
{
  SortedKeysIterator it = this->sorted_keys_iterator_at(frame_number);
  if (it == nullptr) {
    return -1;
  }
  const FramesMapKeyT key = *it;
  const GreasePencilFrame *frame = this->frames().lookup_ptr(key);
  /* For frames that are implicitly held, we return a duration of 0. */
  if (frame->is_implicit_hold()) {
    return 0;
  }
  /* Frame is an end frame, so there is no keyframe at `frame_number`. */
  if (frame->is_end()) {
    return -1;
  }
  /* Compute the distance in frames between this key and the next key. */
  const int next_frame_number = *(std::next(it));
  return math::distance(key, next_frame_number);
}

void Layer::tag_frames_map_changed()
{
  this->frames_storage.flag |= GP_LAYER_FRAMES_STORAGE_DIRTY;
}

void Layer::tag_frames_map_keys_changed()
{
  this->tag_frames_map_changed();
  this->runtime->sorted_keys_cache_.tag_dirty();
}

void Layer::prepare_for_dna_write()
{
  /* Re-create the frames storage only if it was tagged dirty. */
  if ((frames_storage.flag & GP_LAYER_FRAMES_STORAGE_DIRTY) == 0) {
    return;
  }

  MEM_SAFE_FREE(frames_storage.keys);
  MEM_SAFE_FREE(frames_storage.values);

  const size_t frames_num = size_t(frames().size());
  frames_storage.num = int(frames_num);
  frames_storage.keys = MEM_calloc_arrayN<int>(frames_num, __func__);
  frames_storage.values = MEM_calloc_arrayN<GreasePencilFrame>(frames_num, __func__);
  const Span<int> sorted_keys_data = sorted_keys();
  for (const int64_t i : sorted_keys_data.index_range()) {
    frames_storage.keys[i] = sorted_keys_data[i];
    frames_storage.values[i] = frames().lookup(sorted_keys_data[i]);
  }

  /* Reset the flag. */
  frames_storage.flag &= ~GP_LAYER_FRAMES_STORAGE_DIRTY;
}

void Layer::update_from_dna_read()
{
  /* Re-create frames data in runtime map. */
  /* NOTE: Avoid re-allocating runtime data to reduce 'false positive' change detection from
   * MEMFILE undo. */
  if (runtime) {
    runtime->clear();
  }
  else {
    runtime = MEM_new<blender::bke::greasepencil::LayerRuntime>(__func__);
  }
  Map<int, GreasePencilFrame> &frames = frames_for_write();
  for (int i = 0; i < frames_storage.num; i++) {
    frames.add_new(frames_storage.keys[i], frames_storage.values[i]);
  }
}

float4x4 Layer::to_world_space(const Object &object) const
{
  if (this->parent == nullptr) {
    return object.object_to_world() * this->local_transform();
  }
  const Object &parent = *this->parent;
  return this->parent_to_world(parent) * this->parent_inverse() * this->local_transform();
}

float4x4 Layer::to_object_space(const Object &object) const
{
  if (this->parent == nullptr) {
    return this->local_transform();
  }
  const Object &parent = *this->parent;
  return object.world_to_object() * this->parent_to_world(parent) * this->parent_inverse() *
         this->local_transform();
}

StringRefNull Layer::parent_bone_name() const
{
  return (this->parsubstr != nullptr) ? StringRefNull(this->parsubstr) : StringRefNull();
}

void Layer::set_parent_bone_name(const StringRef new_name)
{
  if (this->parsubstr != nullptr) {
    MEM_freeN(this->parsubstr);
    this->parsubstr = nullptr;
  }
  if (!new_name.is_empty()) {
    this->parsubstr = BLI_strdupn(new_name.data(), new_name.size());
  }
}

float4x4 Layer::parent_to_world(const Object &parent) const
{
  const float4x4 &parent_object_to_world = parent.object_to_world();
  if (parent.type == OB_ARMATURE && !this->parent_bone_name().is_empty()) {
    if (bPoseChannel *channel = BKE_pose_channel_find_name(parent.pose,
                                                           this->parent_bone_name().c_str()))
    {
      return parent_object_to_world * float4x4_view(channel->pose_mat);
    }
  }
  return parent_object_to_world;
}

float4x4 Layer::parent_inverse() const
{
  return float4x4_view(this->parentinv);
}

float4x4 Layer::local_transform() const
{
  return math::from_loc_rot_scale<float4x4, math::EulerXYZ>(
      float3(this->translation), float3(this->rotation), float3(this->scale));
}

void Layer::set_local_transform(const float4x4 &transform)
{
  math::to_loc_rot_scale_safe<true>(transform,
                                    *reinterpret_cast<float3 *>(this->translation),
                                    *reinterpret_cast<math::EulerXYZ *>(this->rotation),
                                    *reinterpret_cast<float3 *>(this->scale));
}

StringRefNull Layer::view_layer_name() const
{
  return (this->viewlayername != nullptr) ? StringRefNull(this->viewlayername) : StringRefNull();
}

void Layer::set_view_layer_name(const StringRef new_name)
{
  if (this->viewlayername != nullptr) {
    MEM_freeN(this->viewlayername);
    this->viewlayername = nullptr;
  }
  if (!new_name.is_empty()) {
    this->viewlayername = BLI_strdupn(new_name.data(), new_name.size());
  }
}

LayerGroup::LayerGroup()
{
  new (&this->base) TreeNode(GP_LAYER_TREE_GROUP);

  BLI_listbase_clear(&this->children);
  this->color_tag = LAYERGROUP_COLOR_NONE;

  this->runtime = MEM_new<LayerGroupRuntime>(__func__);
}

LayerGroup::LayerGroup(StringRef name) : LayerGroup()
{
  new (&this->base) TreeNode(GP_LAYER_TREE_GROUP, name);
}

LayerGroup::LayerGroup(const LayerGroup &other) : LayerGroup()
{
  new (&this->base) TreeNode(other.base.wrap());

  LISTBASE_FOREACH (GreasePencilLayerTreeNode *, child, &other.children) {
    switch (child->type) {
      case GP_LAYER_TREE_LEAF: {
        GreasePencilLayer *layer = reinterpret_cast<GreasePencilLayer *>(child);
        Layer *dup_layer = MEM_new<Layer>(__func__, layer->wrap());
        this->add_node(dup_layer->as_node());
        break;
      }
      case GP_LAYER_TREE_GROUP: {
        GreasePencilLayerTreeGroup *group = reinterpret_cast<GreasePencilLayerTreeGroup *>(child);
        LayerGroup *dup_group = MEM_new<LayerGroup>(__func__, group->wrap());
        this->add_node(dup_group->as_node());
        break;
      }
    }
  }

  this->color_tag = other.color_tag;
}

LayerGroup::~LayerGroup()
{
  this->base.wrap().~TreeNode();

  LISTBASE_FOREACH_MUTABLE (GreasePencilLayerTreeNode *, child, &this->children) {
    switch (child->type) {
      case GP_LAYER_TREE_LEAF: {
        GreasePencilLayer *layer = reinterpret_cast<GreasePencilLayer *>(child);
        MEM_delete(&layer->wrap());
        break;
      }
      case GP_LAYER_TREE_GROUP: {
        GreasePencilLayerTreeGroup *group = reinterpret_cast<GreasePencilLayerTreeGroup *>(child);
        MEM_delete(&group->wrap());
        break;
      }
    }
  }

  MEM_delete(this->runtime);
  this->runtime = nullptr;
}

LayerGroup &LayerGroup::operator=(const LayerGroup &other)
{
  if (this == &other) {
    return *this;
  }

  this->~LayerGroup();
  new (this) LayerGroup(other);

  return *this;
}

bool LayerGroup::is_empty() const
{
  return BLI_listbase_is_empty(&this->children);
}

TreeNode &LayerGroup::add_node(TreeNode &node)
{
  BLI_addtail(&this->children, &node);
  node.parent = reinterpret_cast<GreasePencilLayerTreeGroup *>(this);
  this->tag_nodes_cache_dirty();
  return node;
}
void LayerGroup::add_node_before(TreeNode &node, TreeNode &link)
{
  BLI_assert(BLI_findindex(&this->children, &link) != -1);
  BLI_insertlinkbefore(&this->children, &link, &node);
  node.parent = reinterpret_cast<GreasePencilLayerTreeGroup *>(this);
  this->tag_nodes_cache_dirty();
}
void LayerGroup::add_node_after(TreeNode &node, TreeNode &link)
{
  BLI_assert(BLI_findindex(&this->children, &link) != -1);
  BLI_insertlinkafter(&this->children, &link, &node);
  node.parent = reinterpret_cast<GreasePencilLayerTreeGroup *>(this);
  this->tag_nodes_cache_dirty();
}

void LayerGroup::move_node_up(TreeNode &node, const int step)
{
  BLI_listbase_link_move(&this->children, &node, step);
  this->tag_nodes_cache_dirty();
}
void LayerGroup::move_node_down(TreeNode &node, const int step)
{
  BLI_listbase_link_move(&this->children, &node, -step);
  this->tag_nodes_cache_dirty();
}
void LayerGroup::move_node_top(TreeNode &node)
{
  BLI_remlink(&this->children, &node);
  BLI_insertlinkafter(&this->children, this->children.last, &node);
  this->tag_nodes_cache_dirty();
}
void LayerGroup::move_node_bottom(TreeNode &node)
{
  BLI_remlink(&this->children, &node);
  BLI_insertlinkbefore(&this->children, this->children.first, &node);
  this->tag_nodes_cache_dirty();
}

int64_t LayerGroup::num_direct_nodes() const
{
  return BLI_listbase_count(&this->children);
}

int64_t LayerGroup::num_nodes_total() const
{
  this->ensure_nodes_cache();
  return this->runtime->nodes_cache_.size();
}

bool LayerGroup::unlink_node(TreeNode &link, const bool keep_children)
{
  if (link.is_group() && !link.as_group().is_empty() && keep_children) {
    if (BLI_findindex(&this->children, &link) == -1) {
      return false;
    }

    /* Take ownership of the children of `link` by replacing the node with the listbase of its
     * children. */
    ListBase link_children = link.as_group().children;
    GreasePencilLayerTreeNode *first = static_cast<GreasePencilLayerTreeNode *>(
        link_children.first);
    GreasePencilLayerTreeNode *last = static_cast<GreasePencilLayerTreeNode *>(link_children.last);

    /* Rewrite the parent pointers. */
    LISTBASE_FOREACH (GreasePencilLayerTreeNode *, child, &link_children) {
      child->parent = this;
    }

    /* Update previous and/or next link(s). */
    if (link.next != nullptr) {
      link.next->prev = last;
      last->next = link.next;
    }
    if (link.prev != nullptr) {
      link.prev->next = first;
      first->prev = link.prev;
    }

    /* Update first and/or last link(s). */
    if (this->children.last == &link) {
      this->children.last = last;
    }
    if (this->children.first == &link) {
      this->children.first = first;
    }

    /* Listbase has been inserted in `this->children` we can clear the pointers in `link`. */
    BLI_listbase_clear(&link.as_group().children);
    link.parent = nullptr;

    this->tag_nodes_cache_dirty();
    return true;
  }
  if (BLI_remlink_safe(&this->children, &link)) {
    link.parent = nullptr;
    this->tag_nodes_cache_dirty();
    return true;
  }
  return false;
}

Span<const TreeNode *> LayerGroup::nodes() const
{
  this->ensure_nodes_cache();
  return this->runtime->nodes_cache_.as_span();
}

Span<TreeNode *> LayerGroup::nodes_for_write()
{
  this->ensure_nodes_cache();
  return this->runtime->nodes_cache_.as_span();
}

Span<const Layer *> LayerGroup::layers() const
{
  this->ensure_nodes_cache();
  return this->runtime->layer_cache_.as_span();
}

Span<Layer *> LayerGroup::layers_for_write()
{
  this->ensure_nodes_cache();
  return this->runtime->layer_cache_.as_span();
}

Span<const LayerGroup *> LayerGroup::groups() const
{
  this->ensure_nodes_cache();
  return this->runtime->layer_group_cache_.as_span();
}

Span<LayerGroup *> LayerGroup::groups_for_write()
{
  this->ensure_nodes_cache();
  return this->runtime->layer_group_cache_.as_span();
}

const TreeNode *LayerGroup::find_node_by_name(const StringRef name) const
{
  for (const TreeNode *node : this->nodes()) {
    if (node->name() == name) {
      return node;
    }
  }
  return nullptr;
}

TreeNode *LayerGroup::find_node_by_name(const StringRef name)
{
  for (TreeNode *node : this->nodes_for_write()) {
    if (node->name() == name) {
      return node;
    }
  }
  return nullptr;
}

bool LayerGroup::is_expanded() const
{
  return (this->base.flag & GP_LAYER_TREE_NODE_EXPANDED) != 0;
}

void LayerGroup::set_expanded(const bool expanded)
{
  SET_FLAG_FROM_TEST(this->base.flag, expanded, GP_LAYER_TREE_NODE_EXPANDED);
}

void LayerGroup::print_nodes(const StringRef header) const
{
  std::cout << header << std::endl;
  Stack<std::pair<int, TreeNode *>> next_node;
  LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, child_, &this->children) {
    TreeNode *child = reinterpret_cast<TreeNode *>(child_);
    next_node.push(std::make_pair(1, child));
  }
  while (!next_node.is_empty()) {
    auto [indent, node] = next_node.pop();
    for (int i = 0; i < indent; i++) {
      std::cout << "  ";
    }
    if (node->is_layer()) {
      std::cout << node->name();
    }
    else if (node->is_group()) {
      std::cout << node->name() << ": ";
      LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, child_, &node->as_group().children) {
        TreeNode *child = reinterpret_cast<TreeNode *>(child_);
        next_node.push(std::make_pair(indent + 1, child));
      }
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;
}

void LayerGroup::ensure_nodes_cache() const
{
  this->runtime->nodes_cache_mutex_.ensure([&]() {
    this->runtime->nodes_cache_.clear_and_shrink();
    this->runtime->layer_cache_.clear_and_shrink();
    this->runtime->layer_group_cache_.clear_and_shrink();

    LISTBASE_FOREACH (GreasePencilLayerTreeNode *, child_, &this->children) {
      TreeNode *node = reinterpret_cast<TreeNode *>(child_);
      this->runtime->nodes_cache_.append(node);
      switch (node->type) {
        case GP_LAYER_TREE_LEAF: {
          this->runtime->layer_cache_.append(&node->as_layer());
          break;
        }
        case GP_LAYER_TREE_GROUP: {
          this->runtime->layer_group_cache_.append(&node->as_group());
          for (TreeNode *child : node->as_group().nodes_for_write()) {
            this->runtime->nodes_cache_.append(child);
            if (child->is_layer()) {
              this->runtime->layer_cache_.append(&child->as_layer());
            }
            else if (child->is_group()) {
              this->runtime->layer_group_cache_.append(&child->as_group());
            }
          }
          break;
        }
      }
    }
  });
}

void LayerGroup::tag_nodes_cache_dirty() const
{
  this->runtime->nodes_cache_mutex_.tag_dirty();
  if (this->base.parent) {
    this->base.parent->wrap().tag_nodes_cache_dirty();
  }
}

void LayerGroup::prepare_for_dna_write()
{
  LISTBASE_FOREACH (TreeNode *, child, &children) {
    switch (child->type) {
      case GP_LAYER_TREE_LEAF: {
        child->as_layer().prepare_for_dna_write();
        break;
      }
      case GP_LAYER_TREE_GROUP: {
        child->as_group().prepare_for_dna_write();
        break;
      }
    }
  }
}

void LayerGroup::update_from_dna_read()
{
  LISTBASE_FOREACH (TreeNode *, child, &children) {
    switch (child->type) {
      case GP_LAYER_TREE_LEAF: {
        child->as_layer().update_from_dna_read();
        break;
      }
      case GP_LAYER_TREE_GROUP: {
        child->as_group().update_from_dna_read();
        break;
      }
    }
  }
}

void ensure_non_empty_layer_names(Main &bmain, GreasePencil &grease_pencil)
{
  for (bke::greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
    if (layer->name().is_empty()) {
      grease_pencil.rename_node(bmain, layer->as_node(), DATA_("Layer"));
    }
  }
}

}  // namespace blender::bke::greasepencil

namespace blender::bke {

GreasePencilRuntime::GreasePencilRuntime() = default;
GreasePencilRuntime::~GreasePencilRuntime() = default;

std::optional<Span<float3>> GreasePencilDrawingEditHints::positions() const
{
  if (!this->positions_data.has_value()) {
    return std::nullopt;
  }
  const int points_num = this->drawing_orig->geometry.wrap().points_num();
  return Span(static_cast<const float3 *>(this->positions_data.data), points_num);
}

std::optional<MutableSpan<float3>> GreasePencilDrawingEditHints::positions_for_write()
{
  if (!this->positions_data.has_value()) {
    return std::nullopt;
  }

  const int points_num = this->drawing_orig->geometry.wrap().points_num();
  ImplicitSharingPtrAndData &data = this->positions_data;
  if (data.sharing_info->is_mutable()) {
    /* If the referenced component is already mutable, return it directly. */
    data.sharing_info->tag_ensured_mutable();
  }
  else {
    auto *new_sharing_info = new ImplicitSharedValue<Array<float3>>(*this->positions());
    data.sharing_info = ImplicitSharingPtr<>(new_sharing_info);
    data.data = new_sharing_info->data.data();
  }

  return MutableSpan(const_cast<float3 *>(static_cast<const float3 *>(data.data)), points_num);
}

}  // namespace blender::bke

/* ------------------------------------------------------------------- */
/** \name Grease Pencil kernel functions
 * \{ */

bool BKE_grease_pencil_drawing_attribute_required(const GreasePencilDrawing * /*drawing*/,
                                                  const blender::StringRef name)
{
  return name == ATTR_POSITION;
}

GreasePencil *BKE_grease_pencil_add(Main *bmain, const char *name)
{
  GreasePencil *grease_pencil = BKE_id_new<GreasePencil>(bmain, name);

  return grease_pencil;
}

GreasePencil *BKE_grease_pencil_new_nomain()
{
  GreasePencil *grease_pencil = BKE_id_new_nomain<GreasePencil>(nullptr);
  return grease_pencil;
}

GreasePencil *BKE_grease_pencil_copy_for_eval(const GreasePencil *grease_pencil_src)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(
      BKE_id_copy_ex(nullptr, &grease_pencil_src->id, nullptr, LIB_ID_COPY_LOCALIZE));
  grease_pencil->runtime->eval_frame = grease_pencil_src->runtime->eval_frame;
  return grease_pencil;
}

void BKE_grease_pencil_copy_parameters(const GreasePencil &src, GreasePencil &dst)
{
  dst.material_array_num = src.material_array_num;
  dst.material_array = static_cast<Material **>(MEM_dupallocN(src.material_array));
  dst.attributes_active_index = src.attributes_active_index;
  dst.flag = src.flag;
  BLI_duplicatelist(&dst.vertex_group_names, &src.vertex_group_names);
  dst.vertex_group_active_index = src.vertex_group_active_index;
  dst.onion_skinning_settings = src.onion_skinning_settings;
}

void BKE_grease_pencil_copy_layer_parameters(const blender::bke::greasepencil::Layer &src,
                                             blender::bke::greasepencil::Layer &dst)
{
  using namespace blender::bke::greasepencil;
  dst.as_node().flag = src.as_node().flag;
  copy_v3_v3(dst.as_node().color, src.as_node().color);

  dst.blend_mode = src.blend_mode;
  dst.opacity = src.opacity;

  LISTBASE_FOREACH (GreasePencilLayerMask *, src_mask, &src.masks) {
    LayerMask *new_mask = MEM_new<LayerMask>(__func__, *reinterpret_cast<LayerMask *>(src_mask));
    BLI_addtail(&dst.masks, reinterpret_cast<GreasePencilLayerMask *>(new_mask));
  }
  dst.active_mask_index = src.active_mask_index;

  dst.parent = src.parent;
  dst.set_parent_bone_name(src.parsubstr);
  copy_m4_m4(dst.parentinv, src.parentinv);

  copy_v3_v3(dst.translation, src.translation);
  copy_v3_v3(dst.rotation, src.rotation);
  copy_v3_v3(dst.scale, src.scale);

  dst.set_view_layer_name(src.viewlayername);
}

void BKE_grease_pencil_copy_layer_group_parameters(
    const blender::bke::greasepencil::LayerGroup &src, blender::bke::greasepencil::LayerGroup &dst)
{
  using namespace blender::bke::greasepencil;
  dst.as_node().flag = src.as_node().flag;
  copy_v3_v3(dst.as_node().color, src.as_node().color);
  dst.color_tag = src.color_tag;
}

void BKE_grease_pencil_nomain_to_grease_pencil(GreasePencil *grease_pencil_src,
                                               GreasePencil *grease_pencil_dst)
{
  using namespace blender;
  using bke::greasepencil::Drawing;
  using bke::greasepencil::DrawingReference;

  /* Drawings. */
  free_drawing_array(*grease_pencil_dst);
  grease_pencil_dst->resize_drawings(grease_pencil_src->drawing_array_num);
  for (const int i : IndexRange(grease_pencil_dst->drawing_array_num)) {
    switch (grease_pencil_src->drawing_array[i]->type) {
      case GP_DRAWING: {
        const Drawing &src_drawing =
            reinterpret_cast<GreasePencilDrawing *>(grease_pencil_src->drawing_array[i])->wrap();
        grease_pencil_dst->drawing_array[i] = &MEM_new<Drawing>(__func__, src_drawing)->base;
        break;
      }
      case GP_DRAWING_REFERENCE:
        const DrawingReference &src_drawing_ref = reinterpret_cast<GreasePencilDrawingReference *>(
                                                      grease_pencil_src->drawing_array[i])
                                                      ->wrap();
        grease_pencil_dst->drawing_array[i] =
            &MEM_new<DrawingReference>(__func__, src_drawing_ref)->base;
        break;
    }
  }

  /* Layers. */
  if (grease_pencil_dst->root_group_ptr) {
    MEM_delete(&grease_pencil_dst->root_group());
  }

  grease_pencil_dst->root_group_ptr = MEM_new<bke::greasepencil::LayerGroup>(
      __func__, grease_pencil_src->root_group_ptr->wrap());
  BLI_assert(grease_pencil_src->layers().size() == grease_pencil_dst->layers().size());

  /* Reset the active node. */
  grease_pencil_dst->active_node = nullptr;

  grease_pencil_dst->attribute_storage.wrap() = std::move(
      grease_pencil_src->attribute_storage.wrap());

  DEG_id_tag_update(&grease_pencil_dst->id, ID_RECALC_GEOMETRY);

  BKE_id_free(nullptr, grease_pencil_src);
}

void BKE_grease_pencil_vgroup_name_update(Object *ob, const char *old_name, const char *new_name)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
    Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    CurvesGeometry &curves = drawing.strokes_for_write();
    LISTBASE_FOREACH (bDeformGroup *, vgroup, &curves.vertex_group_names) {
      if (STREQ(vgroup->name, old_name)) {
        STRNCPY_UTF8(vgroup->name, new_name);
      }
    }
  }
}

static void grease_pencil_evaluate_modifiers(Depsgraph *depsgraph,
                                             Scene *scene,
                                             Object *object,
                                             blender::bke::GeometrySet &geometry_set)
{
  /* Modifier evaluation modes. */
  const bool use_render = DEG_get_mode(depsgraph) == DAG_EVAL_RENDER;
  ModifierMode required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;
  if (BKE_object_is_in_editmode(object)) {
    required_mode |= eModifierMode_Editmode;
  }
  ModifierApplyFlag apply_flag = use_render ? MOD_APPLY_RENDER : MOD_APPLY_USECACHE;
  const ModifierEvalContext mectx = {depsgraph, object, apply_flag};

  BKE_modifiers_clear_errors(object);

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(object, &virtualModifierData);

  /* Evaluate time modifiers.
   * The time offset modifier can change what drawings are shown on the current frame. But it
   * doesn't affect the drawings data. Modifiers that modify the drawings data are only evaluated
   * for the current frame, so we run the time offset modifiers before all the other ones. */
  ModifierData *tmd = md;
  for (; tmd; tmd = tmd->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(tmd->type));

    if (!BKE_modifier_is_enabled(scene, tmd, required_mode) ||
        ModifierType(tmd->type) != eModifierType_GreasePencilTime)
    {
      continue;
    }

    blender::bke::ScopedModifierTimer modifier_timer{*md};

    if (mti->modify_geometry_set != nullptr) {
      mti->modify_geometry_set(tmd, &mectx, &geometry_set);
    }
  }

  /* Evaluate drawing modifiers. */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

    if (!BKE_modifier_is_enabled(scene, md, required_mode) ||
        ModifierType(md->type) == eModifierType_GreasePencilTime)
    {
      continue;
    }

    blender::bke::ScopedModifierTimer modifier_timer{*md};

    if (mti->modify_geometry_set != nullptr) {
      mti->modify_geometry_set(md, &mectx, &geometry_set);
    }
  }
}

static void grease_pencil_do_layer_adjustments(GreasePencil &grease_pencil)
{
  using namespace blender;
  using namespace bke::greasepencil;

  const bke::AttributeAccessor layer_attributes = grease_pencil.attributes();

  struct LayerDrawingInfo {
    Drawing *drawing;
    const int layer_index;
  };

  Set<Drawing *> all_drawings;
  Vector<LayerDrawingInfo> drawing_infos;
  for (const int layer_i : grease_pencil.layers().index_range()) {
    const Layer &layer = grease_pencil.layer(layer_i);
    /* Set of owned drawings, ignore drawing references to other data blocks. */
    if (Drawing *drawing = grease_pencil.get_eval_drawing(layer)) {
      if (all_drawings.add(drawing)) {
        drawing_infos.append({drawing, layer_i});
      }
    }
  }

  if (layer_attributes.contains("radius_offset")) {
    const VArray<float> radius_offsets = *layer_attributes.lookup_or_default<float>(
        "radius_offset", bke::AttrDomain::Layer, 0.0f);
    threading::parallel_for_each(drawing_infos, [&](LayerDrawingInfo &info) {
      if (radius_offsets[info.layer_index] == 0.0f) {
        return;
      }
      MutableSpan<float> radii = info.drawing->radii_for_write();
      threading::parallel_for(radii.index_range(), 4096, [&](const IndexRange range) {
        for (const int i : range) {
          radii[i] += radius_offsets[info.layer_index];
        }
      });
    });
  }

  if (layer_attributes.contains("tint_color")) {
    auto mix_tint = [](const float4 base, const float4 tint) -> float4 {
      return base * (1.0 - tint.w) + tint * tint.w;
    };
    const VArray<ColorGeometry4f> tint_colors =
        *layer_attributes.lookup_or_default<ColorGeometry4f>(
            "tint_color", bke::AttrDomain::Layer, ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
    threading::parallel_for_each(drawing_infos, [&](LayerDrawingInfo &info) {
      if (tint_colors[info.layer_index].a == 0.0f) {
        return;
      }
      MutableSpan<ColorGeometry4f> vertex_colors = info.drawing->vertex_colors_for_write();
      threading::parallel_for(vertex_colors.index_range(), 4096, [&](const IndexRange range) {
        for (const int i : range) {
          vertex_colors[i] = ColorGeometry4f(
              mix_tint(float4(vertex_colors[i]), float4(tint_colors[info.layer_index])));
        }
      });
      MutableSpan<ColorGeometry4f> fill_colors = info.drawing->fill_colors_for_write();
      threading::parallel_for(fill_colors.index_range(), 4096, [&](const IndexRange range) {
        for (const int i : range) {
          fill_colors[i] = ColorGeometry4f(
              mix_tint(float4(fill_colors[i]), float4(tint_colors[info.layer_index])));
        }
      });
    });
  }
}

static void grease_pencil_evaluate_layers(GreasePencil &grease_pencil)
{
  using namespace blender;
  using namespace blender::bke::greasepencil;

  /* Copy the layer cache into an array here, because removing a layer will invalidate the layer
   * cache. This will only copy the pointers to the layers, not the layers themselves. */
  Array<Layer *> layers = grease_pencil.layers_for_write();

  for (const int layer_i : layers.index_range()) {
    Layer *layer = layers[layer_i];
    /* Store the original index of the layer. */
    layer->runtime->orig_layer_index_ = layer_i;
    /* When the visibility is animated, the layer should be retained even when it is invisible.
     * Changing the visibility through the animation system does NOT create another evaluated copy,
     * and thus the layer has to be kept for this future use. */
    if (layer->is_visible() || layer->runtime->is_visibility_animated_) {
      continue;
    }

    /* Remove layer from evaluated data. */
    grease_pencil.remove_layer(*layer);
  }
}

void BKE_grease_pencil_eval_geometry(Depsgraph *depsgraph, GreasePencil *grease_pencil)
{
  using namespace blender;
  /* Store the frame that this grease pencil is evaluated on. */
  grease_pencil->runtime->eval_frame = int(DEG_get_ctime(depsgraph));
  /* This will remove layers that aren't visible. */
  grease_pencil_evaluate_layers(*grease_pencil);
}

void BKE_object_eval_grease_pencil(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  using namespace blender;
  using namespace blender::bke;
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);
  GeometrySet geometry_set = GeometrySet::from_grease_pencil(grease_pencil,
                                                             GeometryOwnershipType::ReadOnly);
  /* The layer adjustments for tinting and radii offsets are applied before modifier evaluation.
   * This ensures that the evaluated geometry contains the modifications. In the future, it would
   * be better to move these into modifiers. For now, these are hardcoded. */
  const bke::AttributeAccessor layer_attributes = grease_pencil->attributes();
  if (layer_attributes.contains("tint_color") || layer_attributes.contains("radius_offset")) {
    grease_pencil_do_layer_adjustments(*geometry_set.get_grease_pencil_for_write());
  }
  /* Only add the edit hint component in modes where users can potentially interact with deformed
   * drawings. */
  if (ELEM(object->mode,
           OB_MODE_EDIT,
           OB_MODE_SCULPT_GREASE_PENCIL,
           OB_MODE_VERTEX_GREASE_PENCIL,
           OB_MODE_WEIGHT_GREASE_PENCIL))
  {
    GeometryComponentEditData &edit_component =
        geometry_set.get_component_for_write<GeometryComponentEditData>();
    edit_component.grease_pencil_edit_hints_ = std::make_unique<GreasePencilEditHints>(
        *static_cast<const GreasePencil *>(DEG_get_original(object)->data));
  }
  grease_pencil_evaluate_modifiers(depsgraph, scene, object, geometry_set);

  if (geometry_set.has_grease_pencil()) {
    /* Output geometry set may be different from the input,
     * set the frame again to ensure a correct value. */
    geometry_set.get_grease_pencil()->runtime->eval_frame = int(DEG_get_ctime(depsgraph));
  }
  else {
    GreasePencil *empty_grease_pencil = BKE_grease_pencil_new_nomain();
    empty_grease_pencil->runtime->eval_frame = int(DEG_get_ctime(depsgraph));
    geometry_set.replace_grease_pencil(empty_grease_pencil);
  }

  /* For now the evaluated data is not const. We could use #get_grease_pencil_for_write, but that
   * would result in a copy when it's shared. So for now, we use a const_cast here. */
  GreasePencil *grease_pencil_eval = const_cast<GreasePencil *>(geometry_set.get_grease_pencil());

  /* Assign evaluated object. */
  BKE_object_eval_assign_data(object, &grease_pencil_eval->id, false);
  object->runtime->geometry_set_eval = new GeometrySet(std::move(geometry_set));
}

void BKE_grease_pencil_duplicate_drawing_array(const GreasePencil *grease_pencil_src,
                                               GreasePencil *grease_pencil_dst)
{
  using namespace blender;
  grease_pencil_dst->drawing_array_num = grease_pencil_src->drawing_array_num;
  if (grease_pencil_dst->drawing_array_num > 0) {
    grease_pencil_dst->drawing_array = MEM_calloc_arrayN<GreasePencilDrawingBase *>(
        grease_pencil_src->drawing_array_num, __func__);
    bke::greasepencil::copy_drawing_array(grease_pencil_src->drawings(),
                                          grease_pencil_dst->drawings());
  }
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Grease Pencil origin functions
 *  \note Used for "move only origins" in object_data_transform.cc.
 * \{ */

bool BKE_grease_pencil_has_curve_with_type(const GreasePencil &grease_pencil, const CurveType type)
{
  using namespace blender;

  for (const GreasePencilDrawingBase *base : grease_pencil.drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    const bke::greasepencil::Drawing &drawing =
        reinterpret_cast<const GreasePencilDrawing *>(base)->wrap();
    const bke::CurvesGeometry &curves = drawing.strokes();
    if (curves.has_curve_with_type(type)) {
      return true;
    }
  }

  return false;
}

int BKE_grease_pencil_stroke_point_count(const GreasePencil &grease_pencil)
{
  using namespace blender;

  int total_points = 0;

  for (const int layer_i : grease_pencil.layers().index_range()) {
    const bke::greasepencil::Layer &layer = grease_pencil.layer(layer_i);
    const Map<bke::greasepencil::FramesMapKeyT, GreasePencilFrame> frames = layer.frames();
    frames.foreach_item(
        [&](const bke::greasepencil::FramesMapKeyT /*key*/, const GreasePencilFrame frame) {
          const GreasePencilDrawingBase *base = grease_pencil.drawing(frame.drawing_index);
          if (base->type != GP_DRAWING) {
            return;
          }
          const bke::greasepencil::Drawing &drawing =
              reinterpret_cast<const GreasePencilDrawing *>(base)->wrap();
          const bke::CurvesGeometry &curves = drawing.strokes();
          total_points += curves.points_num();
        });
  }

  return total_points;
}

void BKE_grease_pencil_point_coords_get(const GreasePencil &grease_pencil,
                                        blender::MutableSpan<blender::float3> all_positions,
                                        blender::MutableSpan<float> all_radii)
{
  using namespace blender;
  int64_t index = 0;
  for (const int layer_i : grease_pencil.layers().index_range()) {
    const bke::greasepencil::Layer &layer = grease_pencil.layer(layer_i);
    const float4x4 layer_to_object = layer.local_transform();
    const Map<bke::greasepencil::FramesMapKeyT, GreasePencilFrame> frames = layer.frames();
    frames.foreach_item([&](const bke::greasepencil::FramesMapKeyT /*key*/,
                            const GreasePencilFrame frame) {
      const GreasePencilDrawingBase *base = grease_pencil.drawing(frame.drawing_index);
      if (base->type != GP_DRAWING) {
        return;
      }
      const bke::greasepencil::Drawing &drawing =
          reinterpret_cast<const GreasePencilDrawing *>(base)->wrap();
      const bke::CurvesGeometry &curves = drawing.strokes();
      const Span<float3> positions = curves.positions();
      const VArray<float> radii = drawing.radii();

      if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
        for (const int i : curves.points_range()) {
          all_positions[index] = math::transform_point(layer_to_object, positions[i]);
          all_radii[index] = radii[i];
          index++;
        }
      }
      else {
        const std::optional<Span<float3>> handle_positions_left = curves.handle_positions_left();
        const std::optional<Span<float3>> handle_positions_right = curves.handle_positions_right();
        for (const int i : curves.points_range()) {
          const int index_pos = index * 3;
          all_positions[index_pos] = math::transform_point(layer_to_object,
                                                           (*handle_positions_left)[i]);
          all_positions[index_pos + 1] = math::transform_point(layer_to_object, positions[i]);
          all_positions[index_pos + 2] = math::transform_point(layer_to_object,
                                                               (*handle_positions_right)[i]);
          all_radii[index] = radii[i];
          index++;
        }
      }
    });
  }
}

void BKE_grease_pencil_point_coords_apply(GreasePencil &grease_pencil,
                                          blender::Span<blender::float3> all_positions,
                                          blender::Span<float> all_radii)
{
  using namespace blender;
  int64_t index = 0;
  for (const int layer_i : grease_pencil.layers().index_range()) {
    bke::greasepencil::Layer &layer = grease_pencil.layer(layer_i);
    const float4x4 layer_to_object = layer.local_transform();
    const float4x4 object_to_layer = math::invert(layer_to_object);
    const Map<bke::greasepencil::FramesMapKeyT, GreasePencilFrame> frames = layer.frames();
    frames.foreach_item([&](bke::greasepencil::FramesMapKeyT /*key*/, GreasePencilFrame frame) {
      GreasePencilDrawingBase *base = grease_pencil.drawing(frame.drawing_index);
      if (base->type != GP_DRAWING) {
        return;
      }
      bke::greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
      bke::CurvesGeometry &curves = drawing.strokes_for_write();

      MutableSpan<float3> positions = curves.positions_for_write();
      MutableSpan<float> radii = drawing.radii_for_write();

      if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
        for (const int i : curves.points_range()) {
          positions[i] = math::transform_point(object_to_layer, all_positions[index]);
          radii[i] = all_radii[index];
          index++;
        }
      }
      else {
        MutableSpan<float3> handle_positions_left = curves.handle_positions_left_for_write();
        MutableSpan<float3> handle_positions_right = curves.handle_positions_right_for_write();
        for (const int i : curves.points_range()) {
          const int index_pos = index * 3;
          handle_positions_left[i] = math::transform_point(object_to_layer,
                                                           all_positions[index_pos]);
          positions[i] = math::transform_point(object_to_layer, all_positions[index_pos + 1]);
          handle_positions_right[i] = math::transform_point(object_to_layer,
                                                            all_positions[index_pos + 2]);
          radii[i] = all_radii[index];
          index++;
        }
      }

      curves.tag_radii_changed();
      drawing.tag_positions_changed();
    });
  }
}

void BKE_grease_pencil_point_coords_apply_with_mat4(GreasePencil &grease_pencil,
                                                    blender::Span<blender::float3> all_positions,
                                                    blender::Span<float> all_radii,
                                                    const blender::float4x4 &mat)
{
  using namespace blender;
  const float scalef = mat4_to_scale(mat.ptr());
  int64_t index = 0;
  for (const int layer_i : grease_pencil.layers().index_range()) {
    bke::greasepencil::Layer &layer = grease_pencil.layer(layer_i);
    const float4x4 layer_to_object = layer.local_transform();
    const float4x4 object_to_layer = math::invert(layer_to_object);
    const Map<bke::greasepencil::FramesMapKeyT, GreasePencilFrame> frames = layer.frames();
    frames.foreach_item([&](bke::greasepencil::FramesMapKeyT /*key*/, GreasePencilFrame frame) {
      GreasePencilDrawingBase *base = grease_pencil.drawing(frame.drawing_index);
      if (base->type != GP_DRAWING) {
        return;
      }
      bke::greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
      bke::CurvesGeometry &curves = drawing.strokes_for_write();

      MutableSpan<float3> positions = curves.positions_for_write();
      MutableSpan<float> radii = drawing.radii_for_write();

      if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
        for (const int i : curves.points_range()) {
          positions[i] = math::transform_point(object_to_layer * mat, all_positions[index]);
          radii[i] = all_radii[index] * scalef;
          index++;
        }
      }
      else {
        MutableSpan<float3> handle_positions_left = curves.handle_positions_left_for_write();
        MutableSpan<float3> handle_positions_right = curves.handle_positions_right_for_write();
        for (const int i : curves.points_range()) {
          const int index_pos = index * 3;
          handle_positions_left[i] = math::transform_point(object_to_layer * mat,
                                                           all_positions[index_pos]);
          positions[i] = math::transform_point(object_to_layer * mat,
                                               all_positions[index_pos + 1]);
          handle_positions_right[i] = math::transform_point(object_to_layer * mat,
                                                            all_positions[index_pos + 2]);
          radii[i] = all_radii[index] * scalef;
          index++;
        }
      }

      curves.tag_radii_changed();
      drawing.tag_positions_changed();
    });
  }
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Grease Pencil material functions
 * \{ */

int BKE_grease_pencil_object_material_index_get_by_name(Object *ob, const char *name)
{
  short *totcol = BKE_object_material_len_p(ob);
  Material *read_ma = nullptr;
  for (short i = 0; i < *totcol; i++) {
    read_ma = BKE_object_material_get(ob, i + 1);
    if (STREQ(name, read_ma->id.name + 2)) {
      return i;
    }
  }

  return -1;
}

Material *BKE_grease_pencil_object_material_new(Main *bmain,
                                                Object *ob,
                                                const char *name,
                                                int *r_index)
{
  Material *ma = BKE_gpencil_material_add(bmain, name);
  id_us_min(&ma->id); /* no users yet */

  BKE_object_material_slot_add(bmain, ob);
  BKE_object_material_assign(bmain, ob, ma, ob->totcol, BKE_MAT_ASSIGN_USERPREF);

  if (r_index) {
    *r_index = ob->actcol - 1;
  }
  return ma;
}

Material *BKE_grease_pencil_object_material_from_brush_get(Object *ob, Brush *brush)
{
  if (brush && brush->gpencil_settings &&
      (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED))
  {
    return brush->gpencil_settings->material;
  }

  return BKE_object_material_get(ob, ob->actcol);
}

Material *BKE_grease_pencil_object_material_ensure_by_name(Main *bmain,
                                                           Object *ob,
                                                           const char *name,
                                                           int *r_index)
{
  int index = BKE_grease_pencil_object_material_index_get_by_name(ob, name);
  if (index != -1) {
    *r_index = index;
    return BKE_object_material_get(ob, index + 1);
  }
  return BKE_grease_pencil_object_material_new(bmain, ob, name, r_index);
}

static Material *grease_pencil_object_material_ensure_from_brush_pinned(Main *bmain,
                                                                        Object *ob,
                                                                        Brush *brush)
{
  Material *ma = (brush->gpencil_settings) ? brush->gpencil_settings->material : nullptr;

  if (ma) {
    /* Ensure we assign a local datablock if this is an editable asset. */
    ma = reinterpret_cast<Material *>(blender::bke::asset_edit_id_ensure_local(*bmain, ma->id));
  }

  /* check if the material is already on object material slots and add it if missing */
  if (ma && BKE_object_material_index_get(ob, ma) < 0) {
    /* The object's active material is what's used for the unpinned material. Do not touch it
     * while using a pinned material. */
    const bool change_active_material = false;

    BKE_object_material_slot_add(bmain, ob, change_active_material);
    BKE_object_material_assign(bmain, ob, ma, ob->totcol, BKE_MAT_ASSIGN_USERPREF);
  }

  return ma;
}

Material *BKE_grease_pencil_object_material_ensure_from_brush(Main *bmain,
                                                              Object *ob,
                                                              Brush *brush)
{
  /* Use pinned material. */
  if (brush && brush->gpencil_settings &&
      (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED))
  {
    if (Material *ma = grease_pencil_object_material_ensure_from_brush_pinned(bmain, ob, brush)) {
      return ma;
    }

    /* It is easier to just unpin a null material, instead of setting a new one. */
    brush->gpencil_settings->flag &= ~GP_BRUSH_MATERIAL_PINNED;
  }

  /* Use the active material. */
  if (Material *ma = BKE_object_material_get(ob, ob->actcol)) {
    return ma;
  }

  /* Fall back to default material. */
  /* XXX FIXME This is critical abuse of the 'default material' feature, these IDs should never be
   * used/returned as 'regular' data. */
  return BKE_material_default_gpencil();
}

Material *BKE_grease_pencil_object_material_alt_ensure_from_brush(Main *bmain,
                                                                  Object *ob,
                                                                  Brush *brush)
{
  Material *material_alt = (brush->gpencil_settings) ? brush->gpencil_settings->material_alt :
                                                       nullptr;
  if (material_alt) {
    material_alt = reinterpret_cast<Material *>(
        blender::bke::asset_edit_id_find_local(*bmain, material_alt->id));
    if (material_alt && BKE_object_material_slot_find_index(ob, material_alt) != -1) {
      return material_alt;
    }
  }

  return BKE_grease_pencil_object_material_ensure_from_brush(bmain, ob, brush);
}

void BKE_grease_pencil_material_remap(GreasePencil *grease_pencil, const uint *remap, int totcol)
{
  using namespace blender::bke;

  for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    MutableAttributeAccessor attributes = drawing.strokes_for_write().attributes_for_write();
    SpanAttributeWriter<int> material_indices = attributes.lookup_for_write_span<int>(
        "material_index");
    if (!material_indices) {
      continue;
    }
    BLI_assert(material_indices.domain == AttrDomain::Curve);
    for (const int i : material_indices.span.index_range()) {
      BLI_assert(blender::IndexRange(totcol).contains(remap[material_indices.span[i]]));
      UNUSED_VARS_NDEBUG(totcol);
      material_indices.span[i] = remap[material_indices.span[i]];
    }
    material_indices.finish();
  }
}

void BKE_grease_pencil_material_index_remove(GreasePencil *grease_pencil, const int index)
{
  using namespace blender;
  using namespace blender::bke;

  for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    MutableAttributeAccessor attributes = drawing.strokes_for_write().attributes_for_write();
    SpanAttributeWriter<int> material_indices = attributes.lookup_for_write_span<int>(
        "material_index");
    if (!material_indices) {
      continue;
    }
    BLI_assert(material_indices.domain == AttrDomain::Curve);
    for (const int i : material_indices.span.index_range()) {
      if (material_indices.span[i] > 0 && material_indices.span[i] >= index) {
        material_indices.span[i]--;
      }
    }
    material_indices.finish();
  }
}

bool BKE_grease_pencil_material_index_used(GreasePencil *grease_pencil, int index)
{
  using namespace blender;
  using namespace blender::bke;

  for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    AttributeAccessor attributes = drawing.strokes().attributes();
    const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
        "material_index", AttrDomain::Curve, 0);

    if (material_indices.contains(index)) {
      return true;
    }
  }
  return false;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Grease Pencil reference functions
 * \{ */

static bool grease_pencil_references_cyclic_check_internal(const GreasePencil *id_reference,
                                                           const GreasePencil *grease_pencil)
{
  for (const GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type == GP_DRAWING_REFERENCE) {
      const auto *reference = reinterpret_cast<const GreasePencilDrawingReference *>(base);
      if (id_reference == reference->id_reference) {
        return true;
      }

      if (grease_pencil_references_cyclic_check_internal(id_reference, reference->id_reference)) {
        return true;
      }
    }
  }
  return false;
}

bool BKE_grease_pencil_references_cyclic_check(const GreasePencil *id_reference,
                                               const GreasePencil *grease_pencil)
{
  return grease_pencil_references_cyclic_check_internal(id_reference, grease_pencil);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Draw Cache
 * \{ */

void (*BKE_grease_pencil_batch_cache_dirty_tag_cb)(GreasePencil *grease_pencil,
                                                   int mode) = nullptr;
void (*BKE_grease_pencil_batch_cache_free_cb)(GreasePencil *grease_pencil) = nullptr;

void BKE_grease_pencil_batch_cache_dirty_tag(GreasePencil *grease_pencil, int mode)
{
  if (grease_pencil->runtime && grease_pencil->runtime->batch_cache) {
    BKE_grease_pencil_batch_cache_dirty_tag_cb(grease_pencil, mode);
  }
}

void BKE_grease_pencil_batch_cache_free(GreasePencil *grease_pencil)
{
  if (grease_pencil->runtime && grease_pencil->runtime->batch_cache) {
    BKE_grease_pencil_batch_cache_free_cb(grease_pencil);
  }
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Grease Pencil data-block API
 * \{ */

template<typename T> static void grow_array(T **array, int *num, const int add_num)
{
  BLI_assert(add_num > 0);
  const int new_array_num = *num + add_num;
  T *new_array = MEM_calloc_arrayN<T>(new_array_num, __func__);

  blender::uninitialized_relocate_n(*array, *num, new_array);
  if (*array != nullptr) {
    MEM_freeN(*array);
  }

  *array = new_array;
  *num = new_array_num;
}
template<typename T> static void shrink_array(T **array, int *num, const int shrink_num)
{
  BLI_assert(shrink_num > 0);
  const int new_array_num = *num - shrink_num;
  if (new_array_num == 0) {
    MEM_freeN(*array);
    *array = nullptr;
    *num = 0;
    return;
  }

  T *new_array = MEM_calloc_arrayN<T>(new_array_num, __func__);

  blender::uninitialized_move_n(*array, new_array_num, new_array);
  MEM_freeN(*array);

  *array = new_array;
  *num = new_array_num;
}

blender::Span<const GreasePencilDrawingBase *> GreasePencil::drawings() const
{
  return blender::Span<GreasePencilDrawingBase *>{this->drawing_array, this->drawing_array_num};
}

blender::MutableSpan<GreasePencilDrawingBase *> GreasePencil::drawings()
{
  return blender::MutableSpan<GreasePencilDrawingBase *>{this->drawing_array,
                                                         this->drawing_array_num};
}

static void delete_drawing(GreasePencilDrawingBase *drawing_base)
{
  switch (GreasePencilDrawingType(drawing_base->type)) {
    case GP_DRAWING: {
      GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
      MEM_delete(&drawing->wrap());
      break;
    }
    case GP_DRAWING_REFERENCE: {
      GreasePencilDrawingReference *drawing_reference =
          reinterpret_cast<GreasePencilDrawingReference *>(drawing_base);
      MEM_delete(&drawing_reference->wrap());
      break;
    }
  }
}

void GreasePencil::resize_drawings(const int new_num)
{
  using namespace blender;
  BLI_assert(new_num >= 0);

  const int prev_num = int(this->drawings().size());
  if (new_num == prev_num) {
    return;
  }
  if (new_num > prev_num) {
    const int add_num = new_num - prev_num;
    grow_array<GreasePencilDrawingBase *>(&this->drawing_array, &this->drawing_array_num, add_num);
  }
  else { /* if (new_num < prev_num) */
    const int shrink_num = prev_num - new_num;
    MutableSpan<GreasePencilDrawingBase *> old_drawings = this->drawings().drop_front(new_num);
    for (const int64_t i : old_drawings.index_range()) {
      if (GreasePencilDrawingBase *drawing_base = old_drawings[i]) {
        delete_drawing(drawing_base);
      }
    }
    shrink_array<GreasePencilDrawingBase *>(
        &this->drawing_array, &this->drawing_array_num, shrink_num);
  }
}

void GreasePencil::add_empty_drawings(const int add_num)
{
  using namespace blender;
  BLI_assert(add_num > 0);
  const int prev_num = this->drawings().size();
  grow_array<GreasePencilDrawingBase *>(&this->drawing_array, &this->drawing_array_num, add_num);
  MutableSpan<GreasePencilDrawingBase *> new_drawings = this->drawings().drop_front(prev_num);
  for (const int i : new_drawings.index_range()) {
    new_drawings[i] = reinterpret_cast<GreasePencilDrawingBase *>(
        MEM_new<blender::bke::greasepencil::Drawing>(__func__));
  }
}

void GreasePencil::add_duplicate_drawings(const int duplicate_num,
                                          const blender::bke::greasepencil::Drawing &drawing)
{
  using namespace blender;
  BLI_assert(duplicate_num > 0);
  const int prev_num = this->drawings().size();
  grow_array<GreasePencilDrawingBase *>(
      &this->drawing_array, &this->drawing_array_num, duplicate_num);
  MutableSpan<GreasePencilDrawingBase *> new_drawings = this->drawings().drop_front(prev_num);
  for (const int i : new_drawings.index_range()) {
    new_drawings[i] = reinterpret_cast<GreasePencilDrawingBase *>(
        MEM_new<bke::greasepencil::Drawing>(__func__, drawing));
  }
}

blender::bke::greasepencil::Drawing *GreasePencil::insert_frame(
    blender::bke::greasepencil::Layer &layer,
    const int frame_number,
    const int duration,
    const eBezTriple_KeyframeType keytype)
{
  using namespace blender;
  GreasePencilFrame *frame = layer.add_frame(frame_number, duration);
  if (frame == nullptr) {
    return nullptr;
  }
  this->add_empty_drawings(1);
  frame->drawing_index = this->drawings().index_range().last();
  frame->type = int8_t(keytype);

  GreasePencilDrawingBase *drawing_base = this->drawings().last();
  BLI_assert(drawing_base->type == GP_DRAWING);
  GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
  return &drawing->wrap();
}

void GreasePencil::insert_frames(Span<blender::bke::greasepencil::Layer *> layers,
                                 const int frame_number,
                                 const int duration,
                                 const eBezTriple_KeyframeType keytype)
{
  using namespace blender;
  if (layers.is_empty()) {
    return;
  }
  Vector<GreasePencilFrame *> frames;
  frames.reserve(layers.size());
  for (bke::greasepencil::Layer *layer : layers) {
    BLI_assert(layer != nullptr);
    GreasePencilFrame *frame = layer->add_frame(frame_number, duration);
    if (frame != nullptr) {
      frames.append(frame);
    }
  }

  if (frames.is_empty()) {
    return;
  }

  this->add_empty_drawings(frames.size());
  const IndexRange new_drawings = this->drawings().index_range().take_back(frames.size());
  for (const int frame_i : frames.index_range()) {
    GreasePencilFrame *frame = frames[frame_i];
    frame->drawing_index = new_drawings[frame_i];
    frame->type = int8_t(keytype);
  }
}

bool GreasePencil::insert_duplicate_frame(blender::bke::greasepencil::Layer &layer,
                                          const int src_frame_number,
                                          const int dst_frame_number,
                                          const bool do_instance)
{
  using namespace blender::bke::greasepencil;

  if (!layer.frames().contains(src_frame_number)) {
    return false;
  }

  if (layer.is_locked()) {
    return false;
  }
  const GreasePencilFrame src_frame = layer.frames().lookup(src_frame_number);

  /* Create the new frame structure, with the same duration.
   * If we want to make an instance of the source frame, the drawing index gets copied from the
   * source frame. Otherwise, we set the drawing index to the size of the drawings array, since we
   * are going to add a new drawing copied from the source drawing. */
  const int duration = layer.get_frame_duration_at(src_frame_number);
  GreasePencilFrame *dst_frame = layer.add_frame(dst_frame_number, duration);
  if (dst_frame == nullptr) {
    return false;
  }
  dst_frame->drawing_index = do_instance ? src_frame.drawing_index : int(this->drawings().size());
  dst_frame->type = src_frame.type;

  const GreasePencilDrawingBase *src_drawing_base = this->drawing(src_frame.drawing_index);
  switch (src_drawing_base->type) {
    case GP_DRAWING: {
      const Drawing &src_drawing =
          reinterpret_cast<const GreasePencilDrawing *>(src_drawing_base)->wrap();
      if (do_instance) {
        /* Adds the duplicate frame as a new instance of the same drawing. We thus increase the
         * user count of the corresponding drawing. */
        src_drawing.add_user();
      }
      else {
        /* Create a copy of the drawing, and add it at the end of the drawings array.
         * Note that the frame already points to this new drawing, as the drawing index was set to
         * `int(this->drawings().size())`. */
        this->add_duplicate_drawings(1, src_drawing);
      }
      break;
    }
    case GP_DRAWING_REFERENCE:
      /* TODO: Duplicate drawing references is not yet implemented.
       * For now, just remove the frame that we inserted. */
      layer.remove_frame(dst_frame_number);
      return false;
  }
#ifndef NDEBUG
  this->validate_drawing_user_counts();
#endif
  return true;
}

bool GreasePencil::remove_frames(blender::bke::greasepencil::Layer &layer,
                                 blender::Span<int> frame_numbers)
{
  using namespace blender::bke::greasepencil;
  bool removed_any_drawing_user = false;
  for (const int frame_number : frame_numbers) {
    if (!layer.frames().contains(frame_number)) {
      continue;
    }
    const GreasePencilFrame frame_to_remove = layer.frames().lookup(frame_number);
    const int64_t drawing_index_to_remove = frame_to_remove.drawing_index;
    if (!layer.remove_frame(frame_number)) {
      /* If removing the frame was not successful, continue. */
      continue;
    }
    if (frame_to_remove.is_end()) {
      /* End frames don't reference a drawing, continue. */
      continue;
    }
    GreasePencilDrawingBase *drawing_base = this->drawing(drawing_index_to_remove);
    if (drawing_base->type != GP_DRAWING) {
      /* If the drawing is referenced from another object, we don't track it's users because we
       * cannot delete drawings from another object. */
      continue;
    }
    Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap();
    drawing.remove_user();
    removed_any_drawing_user = true;
  }
  if (removed_any_drawing_user) {
    this->remove_drawings_with_no_users();
    return true;
  }
#ifndef NDEBUG
  else {
    this->validate_drawing_user_counts();
  }
#endif
  return false;
}

void GreasePencil::copy_frames_from_layer(blender::bke::greasepencil::Layer &dst_layer,
                                          const GreasePencil &src_grease_pencil,
                                          const blender::bke::greasepencil::Layer &src_layer,
                                          const std::optional<int> frame_select)
{
  using namespace blender;

  const Span<const GreasePencilDrawingBase *> src_drawings = src_grease_pencil.drawings();
  Array<int> drawing_index_map(src_grease_pencil.drawing_array_num, -1);

  for (auto [frame_number, src_frame] : src_layer.frames().items()) {
    if (frame_select && *frame_select != frame_number) {
      continue;
    }

    const int src_drawing_index = src_frame.drawing_index;
    int dst_drawing_index = drawing_index_map[src_drawing_index];
    if (dst_drawing_index < 0) {
      switch (src_drawings[src_drawing_index]->type) {
        case GP_DRAWING: {
          const bke::greasepencil::Drawing &src_drawing =
              reinterpret_cast<const GreasePencilDrawing *>(src_drawings[src_drawing_index])
                  ->wrap();
          this->add_duplicate_drawings(1, src_drawing);
          break;
        }
        case GP_DRAWING_REFERENCE:
          /* Dummy drawing to keep frame reference valid. */
          this->add_empty_drawings(1);
          break;
      }
      dst_drawing_index = this->drawings().size() - 1;
      drawing_index_map[src_drawing_index] = dst_drawing_index;
    }
    BLI_assert(this->drawings().index_range().contains(dst_drawing_index));

    GreasePencilFrame *dst_frame = dst_layer.add_frame(frame_number);
    dst_frame->flag = src_frame.flag;
    dst_frame->drawing_index = dst_drawing_index;
  }
}

void GreasePencil::add_layers_with_empty_drawings_for_eval(const int num)
{
  using namespace blender;
  using namespace blender::bke::greasepencil;
  const int old_drawings_num = this->drawing_array_num;
  const int old_layers_num = this->layers().size();
  this->add_empty_drawings(num);
  this->add_layers_for_eval(num);
  threading::parallel_for(IndexRange(num), 256, [&](const IndexRange range) {
    for (const int i : range) {
      const int new_drawing_i = old_drawings_num + i;
      const int new_layer_i = old_layers_num + i;
      Layer &layer = this->layer(new_layer_i);
      GreasePencilFrame *frame = layer.add_frame(this->runtime->eval_frame);
      BLI_assert(frame);
      frame->drawing_index = new_drawing_i;
    }
  });
}

void GreasePencil::remove_drawings_with_no_users()
{
  using namespace blender;
  using namespace blender::bke::greasepencil;

  /* Compress the drawings array by finding unused drawings.
   * In every step two indices are found:
   *   - The next unused drawing from the start
   *   - The last used drawing from the end
   * These two drawings are then swapped. Rinse and repeat until both iterators meet somewhere in
   * the middle. At this point the drawings array is fully compressed.
   * Then the drawing indices in frame data are remapped. */

  const MutableSpan<GreasePencilDrawingBase *> drawings = this->drawings();
  if (drawings.is_empty()) {
    return;
  }

  auto is_drawing_used = [&](const int drawing_index) {
    GreasePencilDrawingBase *drawing_base = drawings[drawing_index];
    /* NOTE: GreasePencilDrawingReference does not have a user count currently, but should
     * eventually be counted like GreasePencilDrawing. */
    if (drawing_base->type != GP_DRAWING) {
      return false;
    }
    GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
    return drawing->wrap().has_users() || drawing->runtime->fake_user;
  };

  /* Index map to remap drawing indices in frame data.
   * Index -1 indicates that the drawing has not been moved. */
  constexpr const int unchanged_index = -1;
  Array<int> drawing_index_map(drawings.size(), unchanged_index);

  int first_unused_drawing = -1;
  int last_used_drawing = drawings.size() - 1;
  /* Advance head and tail iterators to the next unused/used drawing respectively.
   * Returns true if an index pair was found that needs to be swapped. */
  auto find_next_swap_index = [&]() -> bool {
    do {
      ++first_unused_drawing;
    } while (first_unused_drawing <= last_used_drawing && is_drawing_used(first_unused_drawing));
    while (last_used_drawing >= 0 && !is_drawing_used(last_used_drawing)) {
      --last_used_drawing;
    }

    return first_unused_drawing < last_used_drawing;
  };

  while (find_next_swap_index()) {
    /* Found two valid iterators, now swap drawings. */
    std::swap(drawings[first_unused_drawing], drawings[last_used_drawing]);
    drawing_index_map[last_used_drawing] = first_unused_drawing;
  }

  /* `last_used_drawing` is expected to be exactly the item before the first unused drawing, once
   * the loop above is fully done and all unused drawings are supposed to be at the end of the
   * array. */
  BLI_assert(last_used_drawing == first_unused_drawing - 1);
#ifndef NDEBUG
  for (const int i : drawings.index_range()) {
    if (i < first_unused_drawing) {
      BLI_assert(is_drawing_used(i));
    }
    else {
      BLI_assert(!is_drawing_used(i));
    }
  }
#endif

  /* Tail range of unused drawings that can be removed. */
  const IndexRange drawings_to_remove = (first_unused_drawing > 0) ?
                                            drawings.index_range().drop_front(
                                                first_unused_drawing) :
                                            drawings.index_range();
  if (drawings_to_remove.is_empty()) {
    return;
  }

  /* Free the unused drawings. */
  for (const int i : drawings_to_remove) {
    GreasePencilDrawingBase *unused_drawing_base = drawings[i];
    switch (unused_drawing_base->type) {
      case GP_DRAWING: {
        auto *unused_drawing = reinterpret_cast<GreasePencilDrawing *>(unused_drawing_base);
        MEM_delete(&unused_drawing->wrap());
        break;
      }
      case GP_DRAWING_REFERENCE: {
        auto *unused_drawing_ref = reinterpret_cast<GreasePencilDrawingReference *>(
            unused_drawing_base);
        MEM_delete(&unused_drawing_ref->wrap());
        break;
      }
    }
  }
  shrink_array<GreasePencilDrawingBase *>(
      &this->drawing_array, &this->drawing_array_num, drawings_to_remove.size());

  /* Remap drawing indices in frame data. */
  for (Layer *layer : this->layers_for_write()) {
    for (auto [key, value] : layer->frames_for_write().items()) {
      const int new_drawing_index = drawing_index_map[value.drawing_index];
      if (new_drawing_index != unchanged_index) {
        value.drawing_index = new_drawing_index;
        layer->tag_frames_map_changed();
      }
    }
  }

#ifndef NDEBUG
  this->validate_drawing_user_counts();
#endif
}

void GreasePencil::update_drawing_users_for_layer(const blender::bke::greasepencil::Layer &layer)
{
  using namespace blender;
  for (const auto &[key, value] : layer.frames().items()) {
    BLI_assert(this->drawings().index_range().contains(value.drawing_index));
    GreasePencilDrawingBase *drawing_base = this->drawing(value.drawing_index);
    if (drawing_base->type != GP_DRAWING) {
      continue;
    }
    bke::greasepencil::Drawing &drawing =
        reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap();
    if (!drawing.has_users()) {
      drawing.add_user();
    }
  }

#ifndef NDEBUG
  this->validate_drawing_user_counts();
#endif
}

void GreasePencil::move_frames(blender::bke::greasepencil::Layer &layer,
                               const blender::Map<int, int> &frame_number_destinations)
{
  this->move_duplicate_frames(
      layer, frame_number_destinations, blender::Map<int, GreasePencilFrame>());
}

void GreasePencil::move_duplicate_frames(
    blender::bke::greasepencil::Layer &layer,
    const blender::Map<int, int> &frame_number_destinations,
    const blender::Map<int, GreasePencilFrame> &duplicate_frames)
{
  using namespace blender;
  Map<int, GreasePencilFrame> layer_frames_copy = layer.frames();

  /* Copy frames durations. */
  Map<int, int> src_layer_frames_durations;
  for (const auto [frame_number, frame] : layer.frames().items()) {
    src_layer_frames_durations.add(frame_number, layer.get_frame_duration_at(frame_number));
  }

  /* Remove original frames for duplicates before inserting any frames.
   * This has to be done early to avoid removing frames that may be inserted
   * in place of the source frames. */
  for (const auto src_frame_number : frame_number_destinations.keys()) {
    if (!duplicate_frames.contains(src_frame_number)) {
      /* User count not decremented here, the same frame is inserted again later. */
      layer.remove_frame(src_frame_number);
    }
  }

  auto get_source_frame = [&](const int frame_number) -> const GreasePencilFrame * {
    if (const GreasePencilFrame *ptr = duplicate_frames.lookup_ptr(frame_number)) {
      return ptr;
    }
    return layer_frames_copy.lookup_ptr(frame_number);
  };

  for (const auto [src_frame_number, dst_frame_number] : frame_number_destinations.items()) {
    const GreasePencilFrame *src_frame = get_source_frame(src_frame_number);
    if (!src_frame) {
      continue;
    }
    const int duration = src_layer_frames_durations.lookup_default(src_frame_number, 0);

    /* Add and overwrite the frame at the destination number. */
    if (layer.frames().contains(dst_frame_number)) {
      GreasePencilFrame frame_to_overwrite = layer.frames().lookup(dst_frame_number);
      GreasePencilDrawingBase *drawing_base = this->drawing(frame_to_overwrite.drawing_index);
      if (drawing_base->type == GP_DRAWING) {
        reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap().remove_user();
      }
      layer.remove_frame(dst_frame_number);
    }
    GreasePencilFrame *frame = layer.add_frame(dst_frame_number, duration);
    *frame = *src_frame;
  }

  /* Remove drawings if they no longer have users. */
  this->remove_drawings_with_no_users();
}

const blender::bke::greasepencil::Drawing *GreasePencil::get_drawing_at(
    const blender::bke::greasepencil::Layer &layer, const int frame_number) const
{
  if (this->drawings().is_empty()) {
    return nullptr;
  }
  const int drawing_index = layer.drawing_index_at(frame_number);
  if (drawing_index == -1) {
    /* No drawing found. */
    return nullptr;
  }
  const GreasePencilDrawingBase *drawing_base = this->drawing(drawing_index);
  if (drawing_base->type != GP_DRAWING) {
    /* TODO: Get reference drawing. */
    return nullptr;
  }
  const GreasePencilDrawing *drawing = reinterpret_cast<const GreasePencilDrawing *>(drawing_base);
  return &drawing->wrap();
}

blender::bke::greasepencil::Drawing *GreasePencil::get_drawing_at(
    const blender::bke::greasepencil::Layer &layer, const int frame_number)
{
  if (this->drawings().is_empty()) {
    return nullptr;
  }
  const int drawing_index = layer.drawing_index_at(frame_number);
  if (drawing_index == -1) {
    /* No drawing found. */
    return nullptr;
  }
  GreasePencilDrawingBase *drawing_base = this->drawing(drawing_index);
  if (drawing_base->type != GP_DRAWING) {
    /* TODO: Get reference drawing. */
    return nullptr;
  }
  GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
  return &drawing->wrap();
}

blender::bke::greasepencil::Drawing *GreasePencil::get_editable_drawing_at(
    const blender::bke::greasepencil::Layer &layer, const int frame_number)
{
  if (!layer.is_editable()) {
    return nullptr;
  }
  if (this->drawings().is_empty()) {
    return nullptr;
  }
  const int drawing_index = layer.drawing_index_at(frame_number);
  if (drawing_index == -1) {
    /* No drawing found. */
    return nullptr;
  }
  GreasePencilDrawingBase *drawing_base = this->drawing(drawing_index);
  if (drawing_base->type != GP_DRAWING) {
    /* Drawing references are not editable. */
    return nullptr;
  }
  GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
  return &drawing->wrap();
}

const blender::bke::greasepencil::Drawing *GreasePencil::get_eval_drawing(
    const blender::bke::greasepencil::Layer &layer) const
{
  return this->get_drawing_at(layer, this->runtime->eval_frame);
}

blender::bke::greasepencil::Drawing *GreasePencil::get_eval_drawing(
    const blender::bke::greasepencil::Layer &layer)
{
  return this->get_drawing_at(layer, this->runtime->eval_frame);
}

std::optional<blender::Bounds<blender::float3>> GreasePencil::bounds_min_max(
    const int frame, const bool use_radius) const
{
  using namespace blender;
  std::optional<Bounds<float3>> bounds;
  const Span<const bke::greasepencil::Layer *> layers = this->layers();
  for (const int layer_i : layers.index_range()) {
    const bke::greasepencil::Layer &layer = *layers[layer_i];
    const float4x4 layer_to_object = layer.local_transform();
    if (!layer.is_visible()) {
      continue;
    }
    const bke::greasepencil::Drawing *drawing = this->get_drawing_at(layer, frame);
    if (!drawing) {
      continue;
    }
    const bke::CurvesGeometry &curves = drawing->strokes();
    if (curves.is_empty()) {
      continue;
    }
    if (layer_to_object == float4x4::identity()) {
      bounds = bounds::merge(bounds, curves.bounds_min_max(use_radius));
      continue;
    }
    const VArray<float> radius = curves.radius();
    Array<float3> positions_world(curves.evaluated_points_num());
    math::transform_points(curves.evaluated_positions(), layer_to_object, positions_world);
    if (!use_radius) {
      const Bounds<float3> drawing_bounds = *bounds::min_max(positions_world.as_span());
      bounds = bounds::merge(bounds, drawing_bounds);
      continue;
    }
    if (const std::optional radius_single = radius.get_if_single()) {
      Bounds<float3> drawing_bounds = *curves.bounds_min_max(false);
      drawing_bounds.pad(*radius_single);
      bounds = bounds::merge(bounds, drawing_bounds);
      continue;
    }
    const Span radius_span = radius.get_internal_span();
    if (curves.is_single_type(CURVE_TYPE_POLY)) {
      const Bounds<float3> drawing_bounds = *bounds::min_max_with_radii(positions_world.as_span(),
                                                                        radius_span);
      bounds = bounds::merge(bounds, drawing_bounds);
      continue;
    }
    curves.ensure_can_interpolate_to_evaluated();
    Array<float> radii_eval(curves.evaluated_points_num());
    curves.interpolate_to_evaluated(radius_span, radii_eval.as_mutable_span());
    const Bounds<float3> drawing_bounds = *bounds::min_max_with_radii(positions_world.as_span(),
                                                                      radii_eval.as_span());
    bounds = bounds::merge(bounds, drawing_bounds);
  }
  return bounds;
}

std::optional<blender::Bounds<blender::float3>> GreasePencil::bounds_min_max_eval(
    const bool use_radius) const
{
  return this->bounds_min_max(this->runtime->eval_frame, use_radius);
}

void GreasePencil::count_memory(blender::MemoryCounter &memory) const
{
  using namespace blender::bke;
  for (const GreasePencilDrawingBase *base : this->drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    const greasepencil::Drawing &drawing =
        reinterpret_cast<const GreasePencilDrawing *>(base)->wrap();
    drawing.strokes().count_memory(memory);
  }
}

std::optional<int> GreasePencil::material_index_max_eval() const
{
  using namespace blender;
  using namespace blender::bke;
  std::optional<int> max_index;
  for (const greasepencil::Layer *layer : this->layers()) {
    if (const greasepencil::Drawing *drawing = this->get_eval_drawing(*layer)) {
      const bke::CurvesGeometry &curves = drawing->strokes();
      const std::optional<int> max_index_on_layer = curves.material_index_max();
      if (max_index) {
        if (max_index_on_layer) {
          max_index = std::max(*max_index, *max_index_on_layer);
        }
      }
      else {
        max_index = max_index_on_layer;
      }
    }
  }
  return max_index;
}

blender::Span<const blender::bke::greasepencil::Layer *> GreasePencil::layers() const
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group().layers();
}

blender::Span<blender::bke::greasepencil::Layer *> GreasePencil::layers_for_write()
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group().layers_for_write();
}

blender::Span<const blender::bke::greasepencil::LayerGroup *> GreasePencil::layer_groups() const
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group().groups();
}

blender::Span<blender::bke::greasepencil::LayerGroup *> GreasePencil::layer_groups_for_write()
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group().groups_for_write();
}

blender::Span<const blender::bke::greasepencil::TreeNode *> GreasePencil::nodes() const
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group().nodes();
}

blender::Span<blender::bke::greasepencil::TreeNode *> GreasePencil::nodes_for_write()
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group().nodes_for_write();
}

std::optional<int> GreasePencil::get_layer_index(
    const blender::bke::greasepencil::Layer &layer) const
{
  const int index = int(this->layers().first_index_try(&layer));
  if (index == -1) {
    return {};
  }
  return index;
}

const blender::bke::greasepencil::Layer *GreasePencil::get_active_layer() const
{
  if (this->active_node == nullptr) {
    return nullptr;
  }
  const blender::bke::greasepencil::TreeNode &active_node = *this->get_active_node();
  if (!active_node.is_layer()) {
    return nullptr;
  }
  return &active_node.as_layer();
}

blender::bke::greasepencil::Layer *GreasePencil::get_active_layer()
{
  if (this->active_node == nullptr) {
    return nullptr;
  }
  blender::bke::greasepencil::TreeNode &active_node = *this->get_active_node();
  if (!active_node.is_layer()) {
    return nullptr;
  }
  return &active_node.as_layer();
}

void GreasePencil::set_active_layer(blender::bke::greasepencil::Layer *layer)
{
  this->active_node = reinterpret_cast<GreasePencilLayerTreeNode *>(&layer->as_node());

  if (this->flag & GREASE_PENCIL_AUTOLOCK_LAYERS) {
    this->autolock_inactive_layers();
  }
}

bool GreasePencil::is_layer_active(const blender::bke::greasepencil::Layer *layer) const
{
  if (layer == nullptr) {
    return false;
  }
  return this->get_active_layer() == layer;
}

void GreasePencil::autolock_inactive_layers()
{
  using namespace blender::bke::greasepencil;

  for (Layer *layer : this->layers_for_write()) {
    if (this->is_layer_active(layer)) {
      layer->set_locked(false);
      continue;
    }
    layer->set_locked(true);
  }
}

const blender::bke::greasepencil::LayerGroup *GreasePencil::get_active_group() const
{
  if (this->active_node == nullptr) {
    return nullptr;
  }
  const blender::bke::greasepencil::TreeNode &active_node = *this->get_active_node();
  if (!active_node.is_group()) {
    return nullptr;
  }
  return &active_node.as_group();
}

blender::bke::greasepencil::LayerGroup *GreasePencil::get_active_group()
{
  if (this->active_node == nullptr) {
    return nullptr;
  }
  blender::bke::greasepencil::TreeNode &active_node = *this->get_active_node();
  if (!active_node.is_group()) {
    return nullptr;
  }
  return &active_node.as_group();
}

const blender::bke::greasepencil::TreeNode *GreasePencil::get_active_node() const
{
  if (this->active_node == nullptr) {
    return nullptr;
  }
  return &this->active_node->wrap();
}

blender::bke::greasepencil::TreeNode *GreasePencil::get_active_node()
{
  if (this->active_node == nullptr) {
    return nullptr;
  }
  return &this->active_node->wrap();
}

void GreasePencil::set_active_node(blender::bke::greasepencil::TreeNode *node)
{
  this->active_node = reinterpret_cast<GreasePencilLayerTreeNode *>(node);
}

static blender::VectorSet<blender::StringRef> get_node_names(const GreasePencil &grease_pencil)
{
  using namespace blender;
  VectorSet<StringRef> names;
  for (const blender::bke::greasepencil::TreeNode *node : grease_pencil.nodes()) {
    names.add(node->name());
  }
  return names;
}

static std::string unique_node_name(const GreasePencil &grease_pencil,
                                    const blender::StringRef name)
{
  using namespace blender;
  BLI_assert(!name.is_empty());
  const VectorSet<StringRef> names = get_node_names(grease_pencil);
  return BLI_uniquename_cb(
      [&](const StringRef check_name) { return names.contains(check_name); }, '.', name);
}

std::string GreasePencil::unique_layer_name(blender::StringRef name)
{
  if (name.is_empty()) {
    /* Default name is "Layer". */
    name = DATA_("Layer");
  }
  return unique_node_name(*this, name);
}

static std::string unique_layer_group_name(const GreasePencil &grease_pencil,
                                           blender::StringRef name)
{
  if (name.is_empty()) {
    /* Default name is "Group". */
    name = DATA_("Group");
  }
  return unique_node_name(grease_pencil, name);
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer(const blender::StringRef name,
                                                           const bool check_name_is_unique)
{
  using namespace blender;
  std::string unique_name = check_name_is_unique ? unique_layer_name(name) : std::string(name);
  const int numLayers = layers().size();
  this->attribute_storage.wrap().resize(bke::AttrDomain::Layer, numLayers + 1);
  bke::greasepencil::Layer *new_layer = MEM_new<bke::greasepencil::Layer>(__func__, unique_name);
  /* Enable Lights by default. */
  new_layer->base.flag |= GP_LAYER_TREE_NODE_USE_LIGHTS;
  /* Hide masks by default. */
  new_layer->base.flag |= GP_LAYER_TREE_NODE_HIDE_MASKS;
  bke::greasepencil::Layer &layer = root_group().add_node(new_layer->as_node()).as_layer();

  /* Initialize the attributes with default values. */
  bke::MutableAttributeAccessor attributes = this->attributes_for_write();
  bke::fill_attribute_range_default(attributes,
                                    bke::AttrDomain::Layer,
                                    bke::attribute_filter_from_skip_ref({"name"}),
                                    IndexRange::from_single(numLayers));

  return layer;
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer(
    blender::bke::greasepencil::LayerGroup &parent_group,
    const blender::StringRef name,
    const bool check_name_is_unique)
{
  using namespace blender;
  blender::bke::greasepencil::Layer &new_layer = this->add_layer(name, check_name_is_unique);
  move_node_into(new_layer.as_node(), parent_group);
  return new_layer;
}

void GreasePencil::add_layers_for_eval(const int num_new_layers)
{
  using namespace blender;
  const int num_layers = this->layers().size();
  this->attribute_storage.wrap().resize(bke::AttrDomain::Layer, num_layers + num_new_layers);
  for ([[maybe_unused]] const int i : IndexRange(num_new_layers)) {
    bke::greasepencil::Layer *new_layer = MEM_new<bke::greasepencil::Layer>(__func__);
    /* Hide masks by default. */
    new_layer->base.flag |= GP_LAYER_TREE_NODE_HIDE_MASKS;
    this->root_group().add_node(new_layer->as_node());
  }
}

blender::bke::greasepencil::Layer &GreasePencil::duplicate_layer(
    const blender::bke::greasepencil::Layer &duplicate_layer,
    const bool duplicate_frames,
    const bool duplicate_drawings)
{
  using namespace blender;
  std::string unique_name = unique_layer_name(duplicate_layer.name());
  std::optional<int> duplicate_layer_idx = get_layer_index(duplicate_layer);
  BLI_assert(duplicate_layer_idx.has_value());
  const int numLayers = layers().size();
  bke::greasepencil::Layer *new_layer = MEM_new<bke::greasepencil::Layer>(__func__,
                                                                          duplicate_layer);
  root_group().add_node(new_layer->as_node());

  this->attribute_storage.wrap().resize(bke::AttrDomain::Layer, numLayers + 1);
  bke::MutableAttributeAccessor attributes = this->attributes_for_write();
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    bke::GSpanAttributeWriter attr = attributes.lookup_for_write_span(iter.name);
    GMutableSpan span = attr.span;
    span.type().copy_assign(span[*duplicate_layer_idx], span[numLayers]);
    attr.finish();
  });

  /* When a layer is duplicated, the frames are shared by default. Clear the frames, to ensure a
   * valid state. */
  new_layer->frames_for_write().clear();
  if (duplicate_frames) {
    for (auto [frame_number, frame] : duplicate_layer.frames().items()) {
      const int duration = duplicate_layer.get_frame_duration_at(frame_number);
      bke::greasepencil::Drawing *dst_drawing = this->insert_frame(
          *new_layer, frame_number, duration, eBezTriple_KeyframeType(frame.type));
      if (duplicate_drawings) {
        BLI_assert(dst_drawing != nullptr);
        /* TODO: This can fail (return `nullptr`) if the drawing is a drawing reference! */
        const bke::greasepencil::Drawing &src_drawing = *this->get_drawing_at(duplicate_layer,
                                                                              frame_number);
        /* Duplicate the drawing. */
        *dst_drawing = src_drawing;
      }
    }
  }

  this->update_drawing_users_for_layer(*new_layer);
  new_layer->set_name(unique_name);
  return *new_layer;
}

blender::bke::greasepencil::Layer &GreasePencil::duplicate_layer(
    blender::bke::greasepencil::LayerGroup &parent_group,
    const blender::bke::greasepencil::Layer &duplicate_layer,
    const bool duplicate_frames,
    const bool duplicate_drawings)
{
  using namespace blender;
  bke::greasepencil::Layer &new_layer = this->duplicate_layer(
      duplicate_layer, duplicate_frames, duplicate_drawings);
  move_node_into(new_layer.as_node(), parent_group);
  return new_layer;
}

blender::bke::greasepencil::LayerGroup &GreasePencil::add_layer_group(
    const blender::StringRef name, const bool check_name_is_unique)
{
  using namespace blender;
  std::string unique_name = check_name_is_unique ? unique_layer_group_name(*this, name) :
                                                   std::string(name);
  bke::greasepencil::LayerGroup *new_group = MEM_new<bke::greasepencil::LayerGroup>(__func__,
                                                                                    unique_name);
  return root_group().add_node(new_group->as_node()).as_group();
}

blender::bke::greasepencil::LayerGroup &GreasePencil::add_layer_group(
    blender::bke::greasepencil::LayerGroup &parent_group,
    const blender::StringRef name,
    const bool check_name_is_unique)
{
  using namespace blender;
  bke::greasepencil::LayerGroup &new_group = this->add_layer_group(name, check_name_is_unique);
  move_node_into(new_group.as_node(), parent_group);
  return new_group;
}

static void reorder_attribute_domain(blender::bke::AttributeStorage &data,
                                     const blender::bke::AttrDomain domain,
                                     const Span<int> new_by_old_map)
{
  using namespace blender;
  data.foreach([&](bke::Attribute &attr) {
    if (attr.domain() != domain) {
      return;
    }
    const CPPType &type = bke::attribute_type_to_cpp_type(attr.data_type());
    switch (attr.storage_type()) {
      case bke::AttrStorageType::Array: {
        const auto &data = std::get<bke::Attribute::ArrayData>(attr.data());
        auto new_data = bke::Attribute::ArrayData::from_constructed(type, new_by_old_map.size());
        bke::attribute_math::gather(GSpan(type, data.data, data.size),
                                    new_by_old_map,
                                    GMutableSpan(type, new_data.data, new_data.size));
        attr.assign_data(std::move(new_data));
      }
      case bke::AttrStorageType::Single: {
        return;
      }
    }
  });
}

static void reorder_layer_data(GreasePencil &grease_pencil,
                               const blender::FunctionRef<void()> do_layer_order_changes)
{
  using namespace blender;
  Span<const bke::greasepencil::Layer *> layers = grease_pencil.layers();

  /* Stash the initial layer order that we can refer back to later */
  Map<const bke::greasepencil::Layer *, int> old_layer_index_by_layer;
  old_layer_index_by_layer.reserve(layers.size());
  for (const int i : layers.index_range()) {
    old_layer_index_by_layer.add_new(layers[i], i);
  }

  /* Execute the callback that changes the order of the layers. */
  do_layer_order_changes();
  layers = grease_pencil.layers();
  BLI_assert(layers.size() == old_layer_index_by_layer.size());

  /* Compose the mapping from old layer indices to new layer indices */
  Array<int> new_by_old_map(layers.size());
  for (const int layer_i_new : layers.index_range()) {
    const bke::greasepencil::Layer *layer = layers[layer_i_new];
    BLI_assert(old_layer_index_by_layer.contains(layer));
    const int layer_i_old = old_layer_index_by_layer.pop(layer);
    new_by_old_map[layer_i_new] = layer_i_old;
  }
  BLI_assert(old_layer_index_by_layer.is_empty());

  /* Use the mapping to re-order the custom data */
  reorder_attribute_domain(
      grease_pencil.attribute_storage.wrap(), bke::AttrDomain::Layer, new_by_old_map);
}

void GreasePencil::move_node_up(blender::bke::greasepencil::TreeNode &node, const int step)
{
  using namespace blender;
  if (!node.parent_group()) {
    return;
  }
  reorder_layer_data(*this, [&]() { node.parent_group()->move_node_up(node, step); });
}
void GreasePencil::move_node_down(blender::bke::greasepencil::TreeNode &node, const int step)
{
  using namespace blender;
  if (!node.parent_group()) {
    return;
  }
  reorder_layer_data(*this, [&]() { node.parent_group()->move_node_down(node, step); });
}
void GreasePencil::move_node_top(blender::bke::greasepencil::TreeNode &node)
{
  using namespace blender;
  if (!node.parent_group()) {
    return;
  }
  reorder_layer_data(*this, [&]() { node.parent_group()->move_node_top(node); });
}
void GreasePencil::move_node_bottom(blender::bke::greasepencil::TreeNode &node)
{
  using namespace blender;
  if (!node.parent_group()) {
    return;
  }
  reorder_layer_data(*this, [&]() { node.parent_group()->move_node_bottom(node); });
}

void GreasePencil::move_node_after(blender::bke::greasepencil::TreeNode &node,
                                   blender::bke::greasepencil::TreeNode &target_node)
{
  using namespace blender;
  if (!target_node.parent_group() || !node.parent_group()) {
    return;
  }
  reorder_layer_data(*this, [&]() {
    node.parent_group()->unlink_node(node);
    target_node.parent_group()->add_node_after(node, target_node);
  });
}

void GreasePencil::move_node_before(blender::bke::greasepencil::TreeNode &node,
                                    blender::bke::greasepencil::TreeNode &target_node)
{
  using namespace blender;
  if (!target_node.parent_group() || !node.parent_group()) {
    return;
  }
  reorder_layer_data(*this, [&]() {
    node.parent_group()->unlink_node(node);
    target_node.parent_group()->add_node_before(node, target_node);
  });
}

void GreasePencil::move_node_into(blender::bke::greasepencil::TreeNode &node,
                                  blender::bke::greasepencil::LayerGroup &parent_group)
{
  using namespace blender;
  if (!node.parent_group()) {
    return;
  }
  reorder_layer_data(*this, [&]() {
    node.parent_group()->unlink_node(node);
    parent_group.add_node(node);
  });
}

const blender::bke::greasepencil::TreeNode *GreasePencil::find_node_by_name(
    const blender::StringRef name) const
{
  return this->root_group().find_node_by_name(name);
}

blender::bke::greasepencil::TreeNode *GreasePencil::find_node_by_name(
    const blender::StringRef name)
{
  return this->root_group().find_node_by_name(name);
}

blender::IndexMask GreasePencil::layer_selection_by_name(const blender::StringRef name,
                                                         blender::IndexMaskMemory &memory) const
{
  using namespace blender::bke::greasepencil;
  const TreeNode *node = this->find_node_by_name(name);
  if (!node) {
    return {};
  }

  if (node->is_layer()) {
    const int index = *this->get_layer_index(node->as_layer());
    return blender::IndexMask::from_indices(blender::Span<int>{index}, memory);
  }
  if (node->is_group()) {
    blender::Vector<int64_t> layer_indices;
    for (const int64_t layer_index : this->layers().index_range()) {
      const Layer &layer = *this->layers()[layer_index];
      if (layer.is_child_of(node->as_group())) {
        layer_indices.append(layer_index);
      }
    }
    return blender::IndexMask::from_indices(layer_indices.as_span(), memory);
  }
  return {};
}

static GreasePencilModifierInfluenceData *influence_data_from_modifier(ModifierData *md)
{
  switch (md->type) {
    case eModifierType_GreasePencilArmature: {
      auto *amd = reinterpret_cast<GreasePencilArmatureModifierData *>(md);
      return &amd->influence;
    }
    case eModifierType_GreasePencilArray: {
      auto *mmd = reinterpret_cast<GreasePencilArrayModifierData *>(md);
      return &mmd->influence;
    }
    case eModifierType_GreasePencilBuild: {
      auto *bmd = reinterpret_cast<GreasePencilBuildModifierData *>(md);
      return &bmd->influence;
    }
    case eModifierType_GreasePencilColor: {
      auto *cmd = reinterpret_cast<GreasePencilColorModifierData *>(md);
      return &cmd->influence;
    }
    case eModifierType_GreasePencilDash: {
      auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(md);
      return &dmd->influence;
    }
    case eModifierType_GreasePencilEnvelope: {
      auto *emd = reinterpret_cast<GreasePencilEnvelopeModifierData *>(md);
      return &emd->influence;
    }
    case eModifierType_GreasePencilHook: {
      auto *hmd = reinterpret_cast<GreasePencilHookModifierData *>(md);
      return &hmd->influence;
    }
    case eModifierType_GreasePencilLattice: {
      auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);
      return &lmd->influence;
    }
    case eModifierType_GreasePencilLength: {
      auto *lmd = reinterpret_cast<GreasePencilLengthModifierData *>(md);
      return &lmd->influence;
    }
    case eModifierType_GreasePencilMirror: {
      auto *mmd = reinterpret_cast<GreasePencilMirrorModifierData *>(md);
      return &mmd->influence;
    }
    case eModifierType_GreasePencilMultiply: {
      auto *mmd = reinterpret_cast<GreasePencilMultiModifierData *>(md);
      return &mmd->influence;
    }
    case eModifierType_GreasePencilNoise: {
      auto *nmd = reinterpret_cast<GreasePencilNoiseModifierData *>(md);
      return &nmd->influence;
    }
    case eModifierType_GreasePencilOffset: {
      auto *omd = reinterpret_cast<GreasePencilOffsetModifierData *>(md);
      return &omd->influence;
    }
    case eModifierType_GreasePencilOpacity: {
      auto *omd = reinterpret_cast<GreasePencilOpacityModifierData *>(md);
      return &omd->influence;
    }
    case eModifierType_GreasePencilOutline: {
      auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);
      return &omd->influence;
    }
    case eModifierType_GreasePencilShrinkwrap: {
      auto *smd = reinterpret_cast<GreasePencilShrinkwrapModifierData *>(md);
      return &smd->influence;
    }
    case eModifierType_GreasePencilSimplify: {
      auto *smd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);
      return &smd->influence;
    }
    case eModifierType_GreasePencilSmooth: {
      auto *smd = reinterpret_cast<GreasePencilSmoothModifierData *>(md);
      return &smd->influence;
    }
    case eModifierType_GreasePencilSubdiv: {
      auto *smd = reinterpret_cast<GreasePencilSubdivModifierData *>(md);
      return &smd->influence;
    }
    case eModifierType_GreasePencilTexture: {
      auto *tmd = reinterpret_cast<GreasePencilTextureModifierData *>(md);
      return &tmd->influence;
    }
    case eModifierType_GreasePencilThickness: {
      auto *tmd = reinterpret_cast<GreasePencilThickModifierData *>(md);
      return &tmd->influence;
    }
    case eModifierType_GreasePencilTime: {
      auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(md);
      return &tmd->influence;
    }
    case eModifierType_GreasePencilTint: {
      auto *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);
      return &tmd->influence;
    }
    case eModifierType_GreasePencilWeightAngle: {
      auto *wmd = reinterpret_cast<GreasePencilWeightAngleModifierData *>(md);
      return &wmd->influence;
    }
    case eModifierType_GreasePencilWeightProximity: {
      auto *wmd = reinterpret_cast<GreasePencilWeightProximityModifierData *>(md);
      return &wmd->influence;
    }
    case eModifierType_GreasePencilLineart:
      ATTR_FALLTHROUGH;
    default:
      return nullptr;
  }
  return nullptr;
}

void GreasePencil::rename_node(Main &bmain,
                               blender::bke::greasepencil::TreeNode &node,
                               const blender::StringRef new_name)
{
  using namespace blender;
  if (node.name() == new_name) {
    return;
  }

  /* Rename the node. */
  std::string old_name = node.name();
  if (node.is_layer()) {
    node.set_name(unique_layer_name(new_name));
  }
  else if (node.is_group()) {
    node.set_name(unique_layer_group_name(*this, new_name));
  }

  /* Update layer name dependencies. */
  if (node.is_layer()) {
    BKE_animdata_fix_paths_rename_all(&this->id, "layers", old_name.c_str(), node.name().c_str());
    /* Update names in layer masks. */
    for (bke::greasepencil::Layer *layer : this->layers_for_write()) {
      LISTBASE_FOREACH (GreasePencilLayerMask *, mask, &layer->masks) {
        if (STREQ(mask->layer_name, old_name.c_str())) {
          mask->layer_name = BLI_strdup(node.name().c_str());
        }
      }
    }
  }

  /* Update name dependencies outside of the ID. */
  LISTBASE_FOREACH (Object *, object, &bmain.objects) {
    if (object->data != this) {
      continue;
    }

    /* Update the layer name of the influence data of the modifiers. */
    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      char *dst_layer_name = nullptr;
      size_t dst_layer_name_maxncpy = 0;
      /* LineArt doesn't use the `GreasePencilModifierInfluenceData` struct. */
      if (md->type == eModifierType_GreasePencilLineart) {
        auto *lmd = reinterpret_cast<GreasePencilLineartModifierData *>(md);
        dst_layer_name = lmd->target_layer;
        dst_layer_name_maxncpy = sizeof(lmd->target_layer);
      }
      else if (GreasePencilModifierInfluenceData *influence_data = influence_data_from_modifier(
                   md))
      {
        dst_layer_name = influence_data->layer_name;
        dst_layer_name_maxncpy = sizeof(influence_data->layer_name);
      }
      if (dst_layer_name && STREQ(dst_layer_name, old_name.c_str())) {
        BLI_strncpy(dst_layer_name, node.name().c_str(), dst_layer_name_maxncpy);
      }
    }
  }
}

static void shrink_attribute_storage(blender::bke::AttributeStorage &storage,
                                     const int index_to_remove,
                                     const int size)
{
  using namespace blender;
  const IndexRange range_before(index_to_remove);
  const IndexRange range_after(index_to_remove + 1, size - index_to_remove - 1);

  storage.foreach([&](bke::Attribute &attr) {
    const CPPType &type = bke::attribute_type_to_cpp_type(attr.data_type());
    switch (attr.storage_type()) {
      case bke::AttrStorageType::Array: {
        const auto &data = std::get<bke::Attribute::ArrayData>(attr.data());

        auto new_data = bke::Attribute::ArrayData::from_uninitialized(type, size - 1);
        type.copy_construct_n(data.data, new_data.data, range_before.size());
        type.copy_construct_n(POINTER_OFFSET(data.data, type.size * range_after.start()),
                              POINTER_OFFSET(new_data.data, type.size * index_to_remove),
                              range_after.size());

        attr.assign_data(std::move(new_data));
      }
      case bke::AttrStorageType::Single: {
        return;
      }
    }
  });
}

static void update_active_node_from_node_to_remove(
    GreasePencil &grease_pencil, const blender::bke::greasepencil::TreeNode &node)
{
  using namespace blender::bke::greasepencil;
  /* 1. Try setting the node below (within the same group) to be active. */
  if (node.prev != nullptr) {
    grease_pencil.set_active_node(reinterpret_cast<TreeNode *>(node.prev));
  }
  /* 2. If there is no node below, try setting the node above (within the same group) to be the
   * active one. */
  else if (node.next != nullptr) {
    grease_pencil.set_active_node(reinterpret_cast<TreeNode *>(node.next));
  }
  /* 3. If this is the only node within its parent group and the parent group is not the root
   * group, try setting the parent to be active. */
  else if (node.parent != grease_pencil.root_group_ptr) {
    grease_pencil.set_active_node(&node.parent->wrap().as_node());
  }
  /* 4. Otherwise, clear the active node. */
  else {
    grease_pencil.set_active_node(nullptr);
  }
}

void GreasePencil::remove_layer(blender::bke::greasepencil::Layer &layer)
{
  using namespace blender::bke::greasepencil;
  /* If the layer is active, update the active layer. */
  if (&layer.as_node() == this->get_active_node()) {
    update_active_node_from_node_to_remove(*this, layer.as_node());
  }

  /* Remove all the layer attributes and shrink the `CustomData`. */
  const int layer_index = *this->get_layer_index(layer);
  shrink_attribute_storage(this->attribute_storage.wrap(), layer_index, this->layers().size());

  /* Unlink the layer from the parent group. */
  layer.parent_group().unlink_node(layer.as_node());

  /* Remove drawings. */
  for (const GreasePencilFrame frame : layer.frames().values()) {
    GreasePencilDrawingBase *drawing_base = this->drawing(frame.drawing_index);
    if (drawing_base->type != GP_DRAWING) {
      /* TODO: Remove drawing reference. */
      continue;
    }
    GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
    drawing->wrap().remove_user();
  }
  this->remove_drawings_with_no_users();

  /* Delete the layer. */
  MEM_delete(&layer);
}

void GreasePencil::remove_group(blender::bke::greasepencil::LayerGroup &group,
                                const bool keep_children)
{
  using namespace blender::bke::greasepencil;
  /* If the group is active, update the active layer. */
  if (&group.as_node() == this->get_active_node()) {
    /* If we keep the children and there is at least one child, make it the active node. */
    if (keep_children && !group.is_empty()) {
      this->set_active_node(reinterpret_cast<TreeNode *>(group.children.last));
    }
    else {
      update_active_node_from_node_to_remove(*this, group.as_node());
    }
  }

  if (!keep_children) {
    /* Recursively remove groups and layers. */
    LISTBASE_FOREACH_MUTABLE (GreasePencilLayerTreeNode *, child, &group.children) {
      switch (child->type) {
        case GP_LAYER_TREE_LEAF: {
          this->remove_layer(reinterpret_cast<GreasePencilLayer *>(child)->wrap());
          break;
        }
        case GP_LAYER_TREE_GROUP: {
          this->remove_group(reinterpret_cast<GreasePencilLayerTreeGroup *>(child)->wrap(), false);
          break;
        }
        default:
          BLI_assert_unreachable();
      }
    }
    BLI_assert(BLI_listbase_is_empty(&group.children));
  }

  /* Unlink then delete active group node. */
  group.as_node().parent_group()->unlink_node(group.as_node(), true);
  MEM_delete(&group);
}

void GreasePencil::print_layer_tree()
{
  using namespace blender::bke::greasepencil;
  this->root_group().print_nodes("Layer Tree:");
}

blender::Array<int> GreasePencil::count_frame_users_for_drawings() const
{
  using namespace blender;
  using namespace blender::bke::greasepencil;
  Array<int> user_counts(this->drawings().size(), 0);
  for (const Layer *layer : this->layers()) {
    for (const auto &[frame, value] : layer->frames().items()) {
      BLI_assert(this->drawings().index_range().contains(value.drawing_index));
      user_counts[value.drawing_index]++;
    }
  }
  return user_counts;
}

void GreasePencil::validate_drawing_user_counts()
{
#ifndef NDEBUG
  using namespace blender::bke::greasepencil;
  blender::Array<int> actual_user_counts = this->count_frame_users_for_drawings();
  for (const int drawing_i : this->drawings().index_range()) {
    const GreasePencilDrawingBase *drawing_base = this->drawing(drawing_i);
    if (drawing_base->type != GP_DRAWING_REFERENCE) {
      const Drawing &drawing = reinterpret_cast<const GreasePencilDrawing *>(drawing_base)->wrap();
      /* Ignore `fake_user` flag. */
      BLI_assert(drawing.user_count() == actual_user_counts[drawing_i]);
    }
  }
#endif
}

blender::bke::AttributeAccessor GreasePencil::attributes() const
{
  return blender::bke::AttributeAccessor(
      this, blender::bke::greasepencil::get_attribute_accessor_functions());
}

blender::bke::MutableAttributeAccessor GreasePencil::attributes_for_write()
{
  return blender::bke::MutableAttributeAccessor(
      this, blender::bke::greasepencil::get_attribute_accessor_functions());
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Drawing array read/write functions
 * \{ */

static void read_drawing_array(GreasePencil &grease_pencil, BlendDataReader *reader)
{
  BLO_read_pointer_array(reader,
                         grease_pencil.drawing_array_num,
                         reinterpret_cast<void **>(&grease_pencil.drawing_array));
  for (int i = 0; i < grease_pencil.drawing_array_num; i++) {
    BLO_read_struct(reader, GreasePencilDrawingBase, &grease_pencil.drawing_array[i]);
    GreasePencilDrawingBase *drawing_base = grease_pencil.drawing_array[i];
    switch (GreasePencilDrawingType(drawing_base->type)) {
      case GP_DRAWING: {
        GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
        drawing->wrap().strokes_for_write().blend_read(*reader);
        /* Initialize runtime data. */
        drawing->runtime = MEM_new<blender::bke::greasepencil::DrawingRuntime>(__func__);
        break;
      }
      case GP_DRAWING_REFERENCE: {
        break;
      }
    }
  }
}

static void write_drawing_array(GreasePencil &grease_pencil,
                                blender::ResourceScope &scope,
                                BlendWriter *writer)
{
  using namespace blender;
  BLO_write_pointer_array(writer, grease_pencil.drawing_array_num, grease_pencil.drawing_array);
  for (int i = 0; i < grease_pencil.drawing_array_num; i++) {
    GreasePencilDrawingBase *drawing_base = grease_pencil.drawing_array[i];
    switch (GreasePencilDrawingType(drawing_base->type)) {
      case GP_DRAWING: {
        GreasePencilDrawing &drawing_copy = scope.construct<GreasePencilDrawing>();
        drawing_copy = *reinterpret_cast<GreasePencilDrawing *>(drawing_base);
        bke::CurvesGeometry &curves = drawing_copy.geometry.wrap();

        bke::CurvesGeometry::BlendWriteData write_data(scope);
        curves.blend_write_prepare(write_data);
        drawing_copy.runtime = nullptr;

        BLO_write_shared_tag(writer, curves.curve_offsets);
        BLO_write_shared_tag(writer, curves.custom_knots);

        BLO_write_struct_at_address(writer, GreasePencilDrawing, drawing_base, &drawing_copy);
        curves.blend_write(*writer, grease_pencil.id, write_data);
        break;
      }
      case GP_DRAWING_REFERENCE: {
        GreasePencilDrawingReference *drawing_reference =
            reinterpret_cast<GreasePencilDrawingReference *>(drawing_base);
        BLO_write_struct(writer, GreasePencilDrawingReference, drawing_reference);
        break;
      }
    }
  }
}

static void free_drawing_array(GreasePencil &grease_pencil)
{
  grease_pencil.resize_drawings(0);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Layer tree read/write functions
 * \{ */

static void read_layer(BlendDataReader *reader,
                       GreasePencilLayer *node,
                       GreasePencilLayerTreeGroup *parent)
{
  BLO_read_string(reader, &node->base.name);
  node->base.parent = parent;
  BLO_read_string(reader, &node->parsubstr);
  BLO_read_string(reader, &node->viewlayername);

  /* Read frames storage. */
  BLO_read_int32_array(reader, node->frames_storage.num, &node->frames_storage.keys);
  BLO_read_struct_array(
      reader, GreasePencilFrame, node->frames_storage.num, &node->frames_storage.values);

  /* Read layer masks. */
  BLO_read_struct_list(reader, GreasePencilLayerMask, &node->masks);
  LISTBASE_FOREACH (GreasePencilLayerMask *, mask, &node->masks) {
    BLO_read_string(reader, &mask->layer_name);
  }

  /* NOTE: Ideally this should be cleared on write, to reduce false 'changes' detection in memfile
   * undo system. This is not easily doable currently though, since modifying to actual data during
   * write is not an option (a shallow copy of the #Layer data would be needed then). */
  node->runtime = nullptr;
  node->wrap().update_from_dna_read();
}

static void read_layer_tree_group(BlendDataReader *reader,
                                  GreasePencilLayerTreeGroup *node,
                                  GreasePencilLayerTreeGroup *parent)
{
  BLO_read_string(reader, &node->base.name);
  node->base.parent = parent;
  /* Read list of children. */
  BLO_read_struct_list(reader, GreasePencilLayerTreeNode, &node->children);
  LISTBASE_FOREACH (GreasePencilLayerTreeNode *, child, &node->children) {
    switch (child->type) {
      case GP_LAYER_TREE_LEAF: {
        GreasePencilLayer *layer = reinterpret_cast<GreasePencilLayer *>(child);
        read_layer(reader, layer, node);
        break;
      }
      case GP_LAYER_TREE_GROUP: {
        GreasePencilLayerTreeGroup *group = reinterpret_cast<GreasePencilLayerTreeGroup *>(child);
        read_layer_tree_group(reader, group, node);
        break;
      }
    }
  }

  node->wrap().runtime = MEM_new<blender::bke::greasepencil::LayerGroupRuntime>(__func__);
}

static void read_layer_tree(GreasePencil &grease_pencil, BlendDataReader *reader)
{
  /* Read root group. */
  BLO_read_struct(reader, GreasePencilLayerTreeGroup, &grease_pencil.root_group_ptr);
  /* This shouldn't normally happen, but for files that were created before the root group became a
   * pointer, this address will not exist. In this case, we clear the pointer to the active layer
   * and create an empty root group to avoid crashes. */
  if (grease_pencil.root_group_ptr == nullptr) {
    grease_pencil.root_group_ptr = MEM_new<blender::bke::greasepencil::LayerGroup>(__func__);
    grease_pencil.set_active_node(nullptr);
    return;
  }
  /* Read active layer. */
  BLO_read_struct(reader, GreasePencilLayerTreeNode, &grease_pencil.active_node);
  read_layer_tree_group(reader, grease_pencil.root_group_ptr, nullptr);

  grease_pencil.root_group_ptr->wrap().update_from_dna_read();
}

static void write_layer(BlendWriter *writer, GreasePencilLayer *node)
{
  BLO_write_struct(writer, GreasePencilLayer, node);
  BLO_write_string(writer, node->base.name);
  BLO_write_string(writer, node->parsubstr);
  BLO_write_string(writer, node->viewlayername);

  BLO_write_int32_array(writer, node->frames_storage.num, node->frames_storage.keys);
  BLO_write_struct_array(
      writer, GreasePencilFrame, node->frames_storage.num, node->frames_storage.values);

  BLO_write_struct_list(writer, GreasePencilLayerMask, &node->masks);
  LISTBASE_FOREACH (GreasePencilLayerMask *, mask, &node->masks) {
    BLO_write_string(writer, mask->layer_name);
  }
}

static void write_layer_tree_group(BlendWriter *writer, GreasePencilLayerTreeGroup *node)
{
  BLO_write_struct(writer, GreasePencilLayerTreeGroup, node);
  BLO_write_string(writer, node->base.name);
  LISTBASE_FOREACH (GreasePencilLayerTreeNode *, child, &node->children) {
    switch (child->type) {
      case GP_LAYER_TREE_LEAF: {
        GreasePencilLayer *layer = reinterpret_cast<GreasePencilLayer *>(child);
        write_layer(writer, layer);
        break;
      }
      case GP_LAYER_TREE_GROUP: {
        GreasePencilLayerTreeGroup *group = reinterpret_cast<GreasePencilLayerTreeGroup *>(child);
        write_layer_tree_group(writer, group);
        break;
      }
    }
  }
}

static void write_layer_tree(GreasePencil &grease_pencil, BlendWriter *writer)
{
  grease_pencil.root_group_ptr->wrap().prepare_for_dna_write();
  write_layer_tree_group(writer, grease_pencil.root_group_ptr);
}
