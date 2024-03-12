/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <iostream>
#include <optional>

#include "BKE_action.h"
#include "BKE_anim_data.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.h"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "BLI_bounds.hh"
#include "BLI_map.hh"
#include "BLI_math_euler_types.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memarena.h"
#include "BLI_memory_utils.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_span.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utils.hh"
#include "BLI_vector_set.hh"
#include "BLI_virtual_array.hh"

#include "BLO_read_write.hh"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_ID_enums.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"

using blender::float3;
using blender::Span;
using blender::uint3;
using blender::VectorSet;

/* Forward declarations. */
static void read_drawing_array(GreasePencil &grease_pencil, BlendDataReader *reader);
static void write_drawing_array(GreasePencil &grease_pencil, BlendWriter *writer);
static void free_drawing_array(GreasePencil &grease_pencil);

static void read_layer_tree(GreasePencil &grease_pencil, BlendDataReader *reader);
static void write_layer_tree(GreasePencil &grease_pencil, BlendWriter *writer);

static void grease_pencil_init_data(ID *id)
{
  using namespace blender::bke;

  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);

  grease_pencil->root_group_ptr = MEM_new<greasepencil::LayerGroup>(__func__);
  grease_pencil->active_layer = nullptr;
  grease_pencil->flag |= GREASE_PENCIL_ANIM_CHANNEL_EXPANDED;

  CustomData_reset(&grease_pencil->layers_data);

  grease_pencil->runtime = MEM_new<GreasePencilRuntime>(__func__);
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

  /* Set active layer. */
  if (grease_pencil_src->has_active_layer()) {
    bke::greasepencil::TreeNode *active_node = grease_pencil_dst->find_node_by_name(
        grease_pencil_src->active_layer->wrap().name());
    BLI_assert(active_node && active_node->is_layer());
    grease_pencil_dst->set_active_layer(&active_node->as_layer());
  }

  CustomData_copy(&grease_pencil_src->layers_data,
                  &grease_pencil_dst->layers_data,
                  CD_MASK_ALL,
                  grease_pencil_dst->layers().size());

  BKE_defgroup_copy_list(&grease_pencil_dst->vertex_group_names,
                         &grease_pencil_src->vertex_group_names);

  /* Make sure the runtime pointer exists. */
  grease_pencil_dst->runtime = MEM_new<bke::GreasePencilRuntime>(__func__);
}

static void grease_pencil_free_data(ID *id)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
  BKE_animdata_free(&grease_pencil->id, false);

  MEM_SAFE_FREE(grease_pencil->material_array);

  CustomData_free(&grease_pencil->layers_data, grease_pencil->layers().size());

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
}

static void grease_pencil_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);

  blender::Vector<CustomDataLayer, 16> layers_data_layers;
  CustomData_blend_write_prepare(grease_pencil->layers_data, layers_data_layers);

  /* Write LibData */
  BLO_write_id_struct(writer, GreasePencil, id_address, &grease_pencil->id);
  BKE_id_blend_write(writer, &grease_pencil->id);

  CustomData_blend_write(writer,
                         &grease_pencil->layers_data,
                         layers_data_layers,
                         grease_pencil->layers().size(),
                         CD_MASK_ALL,
                         id);

  /* Write drawings. */
  write_drawing_array(*grease_pencil, writer);
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

  CustomData_blend_read(reader, &grease_pencil->layers_data, grease_pencil->layers().size());

  /* Read materials. */
  BLO_read_pointer_array(reader, reinterpret_cast<void **>(&grease_pencil->material_array));
  /* Read vertex group names. */
  BLO_read_list(reader, &grease_pencil->vertex_group_names);

  grease_pencil->runtime = MEM_new<blender::bke::GreasePencilRuntime>(__func__);
}

IDTypeInfo IDType_ID_GP = {
    /*id_code*/ ID_GP,
    /*id_filter*/ FILTER_ID_GP,
    /*dependencies_id_types*/ FILTER_ID_GP | FILTER_ID_MA,
    /*main_listbase_index*/ INDEX_ID_GP,
    /*struct_size*/ sizeof(GreasePencil),
    /*name*/ "GreasePencil",
    /*name_plural*/ N_("grease_pencils_v3"),
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
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ grease_pencil_blend_write,
    /*blend_read_data*/ grease_pencil_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

namespace blender::bke::greasepencil {

static const std::string ATTR_RADIUS = "radius";
static const std::string ATTR_OPACITY = "opacity";
static const std::string ATTR_VERTEX_COLOR = "vertex_color";

/* Curves attributes getters */
static int domain_num(const CurvesGeometry &curves, const AttrDomain domain)
{
  return domain == AttrDomain::Point ? curves.points_num() : curves.curves_num();
}
static CustomData &domain_custom_data(CurvesGeometry &curves, const AttrDomain domain)
{
  return domain == AttrDomain::Point ? curves.point_data : curves.curve_data;
}
template<typename T>
static MutableSpan<T> get_mutable_attribute(CurvesGeometry &curves,
                                            const AttrDomain domain,
                                            const StringRef name,
                                            const T default_value = T())
{
  const int num = domain_num(curves, domain);
  if (num == 0) {
    return {};
  }
  const eCustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());
  CustomData &custom_data = domain_custom_data(curves, domain);

  T *data = (T *)CustomData_get_layer_named_for_write(&custom_data, type, name, num);
  if (data != nullptr) {
    return {data, num};
  }
  data = (T *)CustomData_add_layer_named(&custom_data, type, CD_SET_DEFAULT, num, name);
  MutableSpan<T> span = {data, num};
  if (num > 0 && span.first() != default_value) {
    span.fill(default_value);
  }
  return span;
}

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

  this->runtime->triangles_cache = other.runtime->triangles_cache;
  this->runtime->curve_plane_normals_cache = other.runtime->curve_plane_normals_cache;
}

Drawing::~Drawing()
{
  this->strokes().~CurvesGeometry();
  MEM_delete(this->runtime);
  this->runtime = nullptr;
}

Span<uint3> Drawing::triangles() const
{
  const char *func = __func__;
  this->runtime->triangles_cache.ensure([&](Vector<uint3> &r_data) {
    MemArena *pf_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, func);

    const CurvesGeometry &curves = this->strokes();
    const Span<float3> positions = curves.positions();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();

    int total_triangles = 0;
    Array<int> tris_offests(curves.curves_num());
    for (int curve_i : curves.curves_range()) {
      IndexRange points = points_by_curve[curve_i];
      if (points.size() > 2) {
        tris_offests[curve_i] = total_triangles;
        total_triangles += points.size() - 2;
      }
    }

    r_data.resize(total_triangles);

    /* TODO: use threading. */
    for (const int curve_i : curves.curves_range()) {
      const IndexRange points = points_by_curve[curve_i];

      if (points.size() < 3) {
        continue;
      }

      const int num_triangles = points.size() - 2;
      MutableSpan<uint3> r_tris = r_data.as_mutable_span().slice(tris_offests[curve_i],
                                                                 num_triangles);

      float(*projverts)[2] = static_cast<float(*)[2]>(
          BLI_memarena_alloc(pf_arena, sizeof(*projverts) * size_t(points.size())));

      /* TODO: calculate axis_mat properly. */
      float3x3 axis_mat;
      axis_dominant_v3_to_m3(axis_mat.ptr(), float3(0.0f, -1.0f, 0.0f));

      for (const int i : IndexRange(points.size())) {
        mul_v2_m3v3(projverts[i], axis_mat.ptr(), positions[points[i]]);
      }

      BLI_polyfill_calc_arena(
          projverts, points.size(), 0, reinterpret_cast<uint32_t(*)[3]>(r_tris.data()), pf_arena);
      BLI_memarena_clear(pf_arena);
    }

    BLI_memarena_free(pf_arena);
  });

  return this->runtime->triangles_cache.data().as_span();
}

Span<float3> Drawing::curve_plane_normals() const
{
  this->runtime->curve_plane_normals_cache.ensure([&](Vector<float3> &r_data) {
    const CurvesGeometry &curves = this->strokes();
    const Span<float3> positions = curves.positions();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();

    r_data.reinitialize(curves.curves_num());
    threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() < 2) {
          r_data[curve_i] = float3(1.0f, 0.0f, 0.0f);
          continue;
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
            float3 segment_vec = math::normalize(positions[point_i] - positions[point_i + 1]);
            if (math::length_squared(segment_vec) != 0.0f) {
              normal = float3(segment_vec.y, -segment_vec.x, 0.0f);
              break;
            }
          }
        }

        r_data[curve_i] = normal;
      }
    });
  });
  return this->runtime->curve_plane_normals_cache.data().as_span();
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
  return get_mutable_attribute<float>(
      this->strokes_for_write(), AttrDomain::Point, ATTR_RADIUS, 0.01f);
}

VArray<float> Drawing::opacities() const
{
  return *this->strokes().attributes().lookup_or_default<float>(
      ATTR_OPACITY, AttrDomain::Point, 1.0f);
}

MutableSpan<float> Drawing::opacities_for_write()
{
  return get_mutable_attribute<float>(
      this->strokes_for_write(), AttrDomain::Point, ATTR_OPACITY, 1.0f);
}

VArray<ColorGeometry4f> Drawing::vertex_colors() const
{
  return *this->strokes().attributes().lookup_or_default<ColorGeometry4f>(
      ATTR_VERTEX_COLOR, AttrDomain::Point, ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
}

MutableSpan<ColorGeometry4f> Drawing::vertex_colors_for_write()
{
  return get_mutable_attribute<ColorGeometry4f>(this->strokes_for_write(),
                                                AttrDomain::Point,
                                                ATTR_VERTEX_COLOR,
                                                ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
}

void Drawing::tag_positions_changed()
{
  this->strokes_for_write().tag_positions_changed();
  this->runtime->triangles_cache.tag_dirty();
  this->runtime->curve_plane_normals_cache.tag_dirty();
}

void Drawing::tag_topology_changed()
{
  this->tag_positions_changed();
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

const Drawing *get_eval_grease_pencil_layer_drawing(const GreasePencil &grease_pencil,
                                                    const int layer_index)
{
  BLI_assert(layer_index >= 0 && layer_index < grease_pencil.layers().size());
  const Layer &layer = *grease_pencil.layers()[layer_index];
  const int drawing_index = layer.drawing_index_at(grease_pencil.runtime->eval_frame);
  if (drawing_index == -1) {
    return nullptr;
  }
  const GreasePencilDrawingBase *drawing_base = grease_pencil.drawing(drawing_index);
  if (drawing_base->type != GP_DRAWING) {
    return nullptr;
  }
  const Drawing &drawing = reinterpret_cast<const GreasePencilDrawing *>(drawing_base)->wrap();
  return &drawing;
}

Drawing *get_eval_grease_pencil_layer_drawing_for_write(GreasePencil &grease_pencil,
                                                        const int layer)
{
  return const_cast<Drawing *>(get_eval_grease_pencil_layer_drawing(grease_pencil, layer));
}

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

TreeNode::TreeNode(GreasePencilLayerTreeNodeType type) : TreeNode()
{
  this->type = type;
}

TreeNode::TreeNode(GreasePencilLayerTreeNodeType type, StringRefNull name) : TreeNode()
{
  this->type = type;
  this->GreasePencilLayerTreeNode::name = BLI_strdup(name.c_str());
}

TreeNode::TreeNode(const TreeNode &other) : TreeNode(GreasePencilLayerTreeNodeType(other.type))
{
  this->GreasePencilLayerTreeNode::name = BLI_strdup_null(other.GreasePencilLayerTreeNode::name);
  this->flag = other.flag;
  copy_v3_v3_uchar(this->color, other.color);
}

TreeNode::~TreeNode()
{
  MEM_SAFE_FREE(this->GreasePencilLayerTreeNode::name);
}

void TreeNode::set_name(StringRefNull name)
{
  MEM_SAFE_FREE(this->GreasePencilLayerTreeNode::name);
  this->GreasePencilLayerTreeNode::name = BLI_strdup(name.c_str());
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

LayerGroup *TreeNode::parent_group() const
{
  return (this->parent) ? &this->parent->wrap() : nullptr;
}

TreeNode *TreeNode::parent_node() const
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

LayerMask::LayerMask(StringRefNull name) : LayerMask()
{
  this->layer_name = BLI_strdup(name.c_str());
}

LayerMask::LayerMask(const LayerMask &other) : LayerMask()
{
  if (other.layer_name) {
    this->layer_name = BLI_strdup(other.layer_name);
  }
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
  frames_.clear_and_shrink();
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

  this->opacity = 1.0f;

  this->parent = nullptr;
  this->parsubstr = nullptr;

  zero_v3(this->translation);
  zero_v3(this->rotation);
  copy_v3_fl(this->scale, 1.0f);

  BLI_listbase_clear(&this->masks);

  this->runtime = MEM_new<LayerRuntime>(__func__);
}

Layer::Layer(StringRefNull name) : Layer()
{
  new (&this->base) TreeNode(GP_LAYER_TREE_LEAF, name);
}

Layer::Layer(const Layer &other) : Layer()
{
  new (&this->base) TreeNode(other.base.wrap());

  /* TODO: duplicate masks. */

  this->blend_mode = other.blend_mode;
  this->opacity = other.opacity;

  this->parent = other.parent;
  this->set_parent_bone_name(other.parsubstr);

  copy_v3_v3(this->translation, other.translation);
  copy_v3_v3(this->rotation, other.rotation);
  copy_v3_v3(this->scale, other.scale);

  /* Note: We do not duplicate the frame storage since it is only needed for writing to file. */
  this->runtime->frames_ = other.runtime->frames_;
  this->runtime->sorted_keys_cache_ = other.runtime->sorted_keys_cache_;
  /* Tag the frames map, so the frame storage is recreated once the DNA is saved.*/
  this->tag_frames_map_changed();

  /* TODO: what about masks cache? */
}

Layer::~Layer()
{
  this->base.wrap().~TreeNode();

  MEM_SAFE_FREE(this->frames_storage.keys);
  MEM_SAFE_FREE(this->frames_storage.values);

  LISTBASE_FOREACH_MUTABLE (GreasePencilLayerMask *, mask, &this->masks) {
    MEM_SAFE_FREE(mask->layer_name);
    MEM_freeN(mask);
  }

  MEM_SAFE_FREE(this->parsubstr);

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

Layer::SortedKeysIterator Layer::remove_leading_null_frames_in_range(
    Layer::SortedKeysIterator begin, Layer::SortedKeysIterator end)
{
  Layer::SortedKeysIterator next_it = begin;
  while (next_it != end && this->frames().lookup(*next_it).is_null()) {
    this->frames_for_write().remove_contained(*next_it);
    this->tag_frames_map_keys_changed();
    next_it = std::next(next_it);
  }
  return next_it;
}

GreasePencilFrame *Layer::add_frame_internal(const FramesMapKey frame_number,
                                             const int drawing_index)
{
  BLI_assert(drawing_index != -1);
  if (!this->frames().contains(frame_number)) {
    GreasePencilFrame frame{};
    frame.drawing_index = drawing_index;
    this->frames_for_write().add_new(frame_number, frame);
    this->tag_frames_map_keys_changed();
    return this->frames_for_write().lookup_ptr(frame_number);
  }
  /* Overwrite null-frames. */
  if (this->frames().lookup(frame_number).is_null()) {
    GreasePencilFrame frame{};
    frame.drawing_index = drawing_index;
    this->frames_for_write().add_overwrite(frame_number, frame);
    this->tag_frames_map_changed();
    return this->frames_for_write().lookup_ptr(frame_number);
  }
  return nullptr;
}

GreasePencilFrame *Layer::add_frame(const FramesMapKey key,
                                    const int drawing_index,
                                    const int duration)
{
  BLI_assert(duration >= 0);
  GreasePencilFrame *frame = this->add_frame_internal(key, drawing_index);
  if (frame == nullptr) {
    return nullptr;
  }
  Span<FramesMapKey> sorted_keys = this->sorted_keys();
  const FramesMapKey end_key = key + duration;
  /* Finds the next greater key that is stored in the map. */
  SortedKeysIterator next_key_it = std::upper_bound(sorted_keys.begin(), sorted_keys.end(), key);
  /* If the next frame we found is at the end of the frame we're inserting, then we are done. */
  if (next_key_it != sorted_keys.end() && *next_key_it == end_key) {
    return frame;
  }
  next_key_it = this->remove_leading_null_frames_in_range(next_key_it, sorted_keys.end());
  /* If the duration is set to 0, the frame is marked as an implicit hold. */
  if (duration == 0) {
    frame->flag |= GP_FRAME_IMPLICIT_HOLD;
    return frame;
  }
  /* If the next frame comes after the end of the frame we're inserting (or if there are no more
   * frames), add a null-frame. */
  if (next_key_it == sorted_keys.end() || *next_key_it > end_key) {
    this->frames_for_write().add_new(end_key, GreasePencilFrame::null());
    this->tag_frames_map_keys_changed();
  }
  return frame;
}

bool Layer::remove_frame(const FramesMapKey key)
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
  Span<FramesMapKey> sorted_keys = this->sorted_keys();
  /* Find the index of the frame to remove in the `sorted_keys` array. */
  SortedKeysIterator remove_key_it = std::lower_bound(sorted_keys.begin(), sorted_keys.end(), key);
  /* If there is a next frame: */
  if (std::next(remove_key_it) != sorted_keys.end()) {
    SortedKeysIterator next_key_it = std::next(remove_key_it);
    this->remove_leading_null_frames_in_range(next_key_it, sorted_keys.end());
  }
  /* If there is a previous frame: */
  if (remove_key_it != sorted_keys.begin()) {
    SortedKeysIterator prev_key_it = std::prev(remove_key_it);
    const GreasePencilFrame &prev_frame = this->frames().lookup(*prev_key_it);
    /* If the previous frame is not an implicit hold (e.g. it has a fixed duration) and it's not a
     * null frame, we cannot just delete the frame. We need to replace it with a null frame. */
    if (!prev_frame.is_implicit_hold() && !prev_frame.is_null()) {
      this->frames_for_write().lookup(key) = GreasePencilFrame::null();
      this->tag_frames_map_changed();
      /* Since the original frame was replaced with a null frame, we consider the frame to be
       * successfully removed here. */
      return true;
    }
  }
  /* Finally, remove the actual frame. */
  this->frames_for_write().remove_contained(key);
  this->tag_frames_map_keys_changed();
  return true;
}

Span<FramesMapKey> Layer::sorted_keys() const
{
  this->runtime->sorted_keys_cache_.ensure([&](Vector<FramesMapKey> &r_data) {
    r_data.reinitialize(this->frames().size());
    int i = 0;
    for (FramesMapKey key : this->frames().keys()) {
      r_data[i++] = key;
    }
    std::sort(r_data.begin(), r_data.end());
  });
  return this->runtime->sorted_keys_cache_.data();
}

std::optional<FramesMapKey> Layer::frame_key_at(const int frame_number) const
{
  Span<int> sorted_keys = this->sorted_keys();
  /* No keyframes, return no drawing. */
  if (sorted_keys.is_empty()) {
    return {};
  }
  /* Before the first drawing, return no drawing. */
  if (frame_number < sorted_keys.first()) {
    return {};
  }
  /* After or at the last drawing, return the last drawing. */
  if (frame_number >= sorted_keys.last()) {
    return sorted_keys.last();
  }
  /* Search for the drawing. upper_bound will get the drawing just after. */
  SortedKeysIterator it = std::upper_bound(sorted_keys.begin(), sorted_keys.end(), frame_number);
  if (it == sorted_keys.end() || it == sorted_keys.begin()) {
    return {};
  }
  return *std::prev(it);
}

const GreasePencilFrame *Layer::frame_at(const int frame_number) const
{
  const std::optional<FramesMapKey> frame_key = this->frame_key_at(frame_number);
  return frame_key ? this->frames().lookup_ptr(*frame_key) : nullptr;
}

GreasePencilFrame *Layer::frame_at(const int frame_number)
{
  const std::optional<FramesMapKey> frame_key = this->frame_key_at(frame_number);
  return frame_key ? this->frames_for_write().lookup_ptr(*frame_key) : nullptr;
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
  const std::optional<FramesMapKey> frame_key = this->frame_key_at(frame_number);
  if (!frame_key) {
    return -1;
  }
  SortedKeysIterator frame_number_it = std::next(this->sorted_keys().begin(), *frame_key);
  if (*frame_number_it == this->sorted_keys().last()) {
    return -1;
  }
  const int next_frame_number = *(std::next(frame_number_it));
  return next_frame_number - frame_number;
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
  frames_storage.keys = MEM_cnew_array<int>(frames_num, __func__);
  frames_storage.values = MEM_cnew_array<GreasePencilFrame>(frames_num, __func__);
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
  /* NOTE: Avoid re-allocating runtime data to reduce 'false positive' change detections from
   * memfile undo. */
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
  return this->parent_to_world(parent) * this->local_transform();
}

float4x4 Layer::to_object_space(const Object &object) const
{
  if (this->parent == nullptr) {
    return this->local_transform();
  }
  const Object &parent = *this->parent;
  return object.world_to_object() * this->parent_to_world(parent) * this->local_transform();
}

StringRefNull Layer::parent_bone_name() const
{
  return (this->parsubstr != nullptr) ? StringRefNull(this->parsubstr) : StringRefNull();
}

void Layer::set_parent_bone_name(const char *new_name)
{
  if (this->parsubstr != nullptr) {
    MEM_freeN(this->parsubstr);
  }
  this->parsubstr = BLI_strdup_null(new_name);
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

float4x4 Layer::local_transform() const
{
  return math::from_loc_rot_scale<float4x4, math::EulerXYZ>(
      float3(this->translation), float3(this->rotation), float3(this->scale));
}

LayerGroup::LayerGroup()
{
  new (&this->base) TreeNode(GP_LAYER_TREE_GROUP);

  BLI_listbase_clear(&this->children);

  this->runtime = MEM_new<LayerGroupRuntime>(__func__);
}

LayerGroup::LayerGroup(StringRefNull name) : LayerGroup()
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

Layer &LayerGroup::add_layer(StringRefNull name)
{
  Layer *new_layer = MEM_new<Layer>(__func__, name);
  return this->add_node(new_layer->as_node()).as_layer();
}

Layer &LayerGroup::add_layer(const Layer &duplicate_layer)
{
  Layer *new_layer = MEM_new<Layer>(__func__, duplicate_layer);
  return this->add_node(new_layer->as_node()).as_layer();
}

LayerGroup &LayerGroup::add_group(StringRefNull name)
{
  LayerGroup *new_group = MEM_new<LayerGroup>(__func__, name);
  return this->add_node(new_group->as_node()).as_group();
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

bool LayerGroup::unlink_node(TreeNode &link)
{
  if (BLI_remlink_safe(&this->children, &link)) {
    this->tag_nodes_cache_dirty();
    link.parent = nullptr;
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

const TreeNode *LayerGroup::find_node_by_name(const StringRefNull name) const
{
  for (const TreeNode *node : this->nodes()) {
    if (StringRef(node->name()) == StringRef(name)) {
      return node;
    }
  }
  return nullptr;
}

TreeNode *LayerGroup::find_node_by_name(const StringRefNull name)
{
  for (TreeNode *node : this->nodes_for_write()) {
    if (StringRef(node->name()) == StringRef(name)) {
      return node;
    }
  }
  return nullptr;
}

void LayerGroup::print_nodes(StringRefNull header) const
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

}  // namespace blender::bke::greasepencil

/* ------------------------------------------------------------------- */
/** \name Grease Pencil kernel functions
 * \{ */

void *BKE_grease_pencil_add(Main *bmain, const char *name)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(BKE_id_new(bmain, ID_GP, name));

  return grease_pencil;
}

GreasePencil *BKE_grease_pencil_new_nomain()
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(
      BKE_id_new_nomain(ID_GP, nullptr));
  return grease_pencil;
}

GreasePencil *BKE_grease_pencil_copy_for_eval(const GreasePencil *grease_pencil_src)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(
      BKE_id_copy_ex(nullptr, &grease_pencil_src->id, nullptr, LIB_ID_COPY_LOCALIZE));
  grease_pencil->runtime->eval_frame = grease_pencil_src->runtime->eval_frame;
  return grease_pencil;
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

    if (mti->modify_geometry_set != nullptr) {
      mti->modify_geometry_set(md, &mectx, &geometry_set);
    }
  }
}

void BKE_grease_pencil_data_update(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  using namespace blender::bke;
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  /* Evaluate modifiers. */
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);
  /* Store the frame that this grease pencil is evaluated on. */
  grease_pencil->runtime->eval_frame = int(DEG_get_ctime(depsgraph));
  GeometrySet geometry_set = GeometrySet::from_grease_pencil(grease_pencil,
                                                             GeometryOwnershipType::ReadOnly);
  /* Only add the edit hint component in edit mode for now so users can properly select deformed
   * drawings. */
  if (object->mode == OB_MODE_EDIT) {
    GeometryComponentEditData &edit_component =
        geometry_set.get_component_for_write<GeometryComponentEditData>();
    edit_component.grease_pencil_edit_hints_ = std::make_unique<GreasePencilEditHints>(
        *static_cast<const GreasePencil *>(DEG_get_original_object(object)->data));
  }
  grease_pencil_evaluate_modifiers(depsgraph, scene, object, geometry_set);

  if (!geometry_set.has_grease_pencil()) {
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
  grease_pencil_dst->drawing_array = MEM_cnew_array<GreasePencilDrawingBase *>(
      grease_pencil_src->drawing_array_num, __func__);
  bke::greasepencil::copy_drawing_array(grease_pencil_src->drawings(),
                                        grease_pencil_dst->drawings());
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
  if ((brush) && (brush->gpencil_settings) &&
      (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED))
  {
    Material *ma = BKE_grease_pencil_brush_material_get(brush);
    return ma;
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

Material *BKE_grease_pencil_brush_material_get(Brush *brush)
{
  if (brush == nullptr) {
    return nullptr;
  }
  if (brush->gpencil_settings == nullptr) {
    return nullptr;
  }
  return brush->gpencil_settings->material;
}

Material *BKE_grease_pencil_object_material_ensure_from_brush(Main *bmain,
                                                              Object *ob,
                                                              Brush *brush)
{
  if (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED) {
    Material *ma = BKE_grease_pencil_brush_material_get(brush);

    /* check if the material is already on object material slots and add it if missing */
    if (ma && BKE_object_material_index_get(ob, ma) < 0) {
      BKE_object_material_slot_add(bmain, ob);
      BKE_object_material_assign(bmain, ob, ma, ob->totcol, BKE_MAT_ASSIGN_USERPREF);
    }

    return ma;
  }

  /* Use the active material instead. */
  return BKE_object_material_get(ob, ob->actcol);
}

Material *BKE_grease_pencil_object_material_ensure_from_active_input_brush(Main *bmain,
                                                                           Object *ob,
                                                                           Brush *brush)
{
  if (brush == nullptr) {
    return BKE_grease_pencil_object_material_ensure_from_active_input_material(ob);
  }
  if (Material *ma = BKE_grease_pencil_object_material_ensure_from_brush(bmain, ob, brush)) {
    return ma;
  }
  if (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED) {
    /* It is easier to just unpin a null material, instead of setting a new one. */
    brush->gpencil_settings->flag &= ~GP_BRUSH_MATERIAL_PINNED;
  }
  return BKE_grease_pencil_object_material_ensure_from_active_input_material(ob);
}

Material *BKE_grease_pencil_object_material_ensure_from_active_input_material(Object *ob)
{
  if (Material *ma = BKE_object_material_get(ob, ob->actcol)) {
    return ma;
  }
  return BKE_material_default_gpencil();
}

Material *BKE_grease_pencil_object_material_ensure_active(Object *ob)
{
  Material *ma = BKE_grease_pencil_object_material_ensure_from_active_input_material(ob);
  if (ma->gp_style == nullptr) {
    BKE_gpencil_material_attr_init(ma);
  }
  return ma;
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
    SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
        "material_index", AttrDomain::Curve);
    if (!material_indices) {
      return;
    }
    for (const int i : material_indices.span.index_range()) {
      BLI_assert(blender::IndexRange(totcol).contains(remap[material_indices.span[i]]));
      UNUSED_VARS_NDEBUG(totcol);
      material_indices.span[i] = remap[material_indices.span[i]];
    }
    material_indices.finish();
  }
}

void BKE_grease_pencil_material_index_remove(GreasePencil *grease_pencil, int index)
{
  using namespace blender;
  using namespace blender::bke;

  for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }

    greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    MutableAttributeAccessor attributes = drawing.strokes_for_write().attributes_for_write();
    AttributeWriter<int> material_indices = attributes.lookup_for_write<int>("material_index");
    if (!material_indices) {
      return;
    }

    MutableVArraySpan<int> indices_span(material_indices.varray);
    for (const int i : indices_span.index_range()) {
      if (indices_span[i] > 0 && indices_span[i] >= index) {
        indices_span[i]--;
      }
    }
    indices_span.save();
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
  T *new_array = reinterpret_cast<T *>(MEM_cnew_array<T *>(new_array_num, __func__));

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

  T *new_array = reinterpret_cast<T *>(MEM_cnew_array<T *>(new_array_num, __func__));

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

void GreasePencil::resize_drawings(const int new_num)
{
  using namespace blender;
  BLI_assert(new_num > 0);

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
      if (old_drawings[i]) {
        MEM_delete(old_drawings[i]);
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

bool GreasePencil::insert_blank_frame(blender::bke::greasepencil::Layer &layer,
                                      int frame_number,
                                      int duration,
                                      eBezTriple_KeyframeType keytype)
{
  using namespace blender;
  GreasePencilFrame *frame = layer.add_frame(frame_number, int(this->drawings().size()), duration);
  if (frame == nullptr) {
    return false;
  }
  frame->type = int8_t(keytype);
  this->add_empty_drawings(1);
  return true;
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
  const GreasePencilFrame &src_frame = layer.frames().lookup(src_frame_number);

  /* Create the new frame structure, with the same duration.
   * If we want to make an instance of the source frame, the drawing index gets copied from the
   * source frame. Otherwise, we set the drawing index to the size of the drawings array, since we
   * are going to add a new drawing copied from the source drawing. */
  const int duration = src_frame.is_implicit_hold() ?
                           0 :
                           layer.get_frame_duration_at(src_frame_number);
  const int drawing_index = do_instance ? src_frame.drawing_index : int(this->drawings().size());
  GreasePencilFrame *dst_frame = layer.add_frame(dst_frame_number, drawing_index, duration);

  if (dst_frame == nullptr) {
    return false;
  }

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
    if (frame_to_remove.is_null()) {
      /* Null frames don't reference a drawing, continue. */
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
  return false;
}

static void remove_drawings_unchecked(GreasePencil &grease_pencil,
                                      Span<int64_t> sorted_indices_to_remove)
{
  using namespace blender::bke::greasepencil;
  if (grease_pencil.drawing_array_num == 0 || sorted_indices_to_remove.is_empty()) {
    return;
  }
  const int64_t drawings_to_remove = sorted_indices_to_remove.size();
  const blender::IndexRange last_drawings_range(
      grease_pencil.drawings().size() - drawings_to_remove, drawings_to_remove);

  /* We keep track of the next available index (for swapping) by iterating from the end and
   * skipping over drawings that are already in the range to be removed. */
  auto next_available_index = last_drawings_range.last();
  auto greatest_index_to_remove_it = std::rbegin(sorted_indices_to_remove);
  auto get_next_available_index = [&]() {
    while (next_available_index == *greatest_index_to_remove_it) {
      greatest_index_to_remove_it = std::prev(greatest_index_to_remove_it);
      next_available_index--;
    }
    return next_available_index;
  };

  /* Move the drawings to be removed to the end of the array by swapping the pointers. Make sure to
   * remap any frames pointing to the drawings being swapped. */
  for (const int64_t index_to_remove : sorted_indices_to_remove) {
    if (index_to_remove >= last_drawings_range.first()) {
      /* This drawing and all the next drawings are already in the range to be removed. */
      break;
    }
    const int64_t swap_index = get_next_available_index();
    /* Remap the drawing_index for frames that point to the drawing to be swapped with. */
    for (Layer *layer : grease_pencil.layers_for_write()) {
      for (auto [key, value] : layer->frames_for_write().items()) {
        if (value.drawing_index == swap_index) {
          value.drawing_index = index_to_remove;
          layer->tag_frames_map_changed();
        }
      }
    }
    /* Swap the pointers to the drawings in the drawing array. */
    std::swap(grease_pencil.drawing_array[index_to_remove],
              grease_pencil.drawing_array[swap_index]);
    next_available_index--;
  }

  /* Free the last drawings. */
  for (const int64_t drawing_index : last_drawings_range) {
    GreasePencilDrawingBase *drawing_base_to_remove = grease_pencil.drawing(drawing_index);
    switch (drawing_base_to_remove->type) {
      case GP_DRAWING: {
        GreasePencilDrawing *drawing_to_remove = reinterpret_cast<GreasePencilDrawing *>(
            drawing_base_to_remove);
        MEM_delete(&drawing_to_remove->wrap());
        break;
      }
      case GP_DRAWING_REFERENCE: {
        GreasePencilDrawingReference *drawing_reference_to_remove =
            reinterpret_cast<GreasePencilDrawingReference *>(drawing_base_to_remove);
        MEM_delete(&drawing_reference_to_remove->wrap());
        break;
      }
    }
  }

  /* Shrink drawing array. */
  shrink_array<GreasePencilDrawingBase *>(
      &grease_pencil.drawing_array, &grease_pencil.drawing_array_num, drawings_to_remove);
}

void GreasePencil::remove_drawings_with_no_users()
{
  using namespace blender;
  Vector<int64_t> drawings_to_be_removed;
  for (const int64_t drawing_i : this->drawings().index_range()) {
    GreasePencilDrawingBase *drawing_base = this->drawing(drawing_i);
    if (drawing_base->type != GP_DRAWING) {
      continue;
    }
    GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
    if (!drawing->wrap().has_users()) {
      drawings_to_be_removed.append(drawing_i);
    }
  }
  remove_drawings_unchecked(*this, drawings_to_be_removed.as_span());
}

void GreasePencil::update_drawing_users_for_layer(const blender::bke::greasepencil::Layer &layer)
{
  using namespace blender;
  for (auto [key, value] : layer.frames().items()) {
    if (value.drawing_index > 0 && value.drawing_index < this->drawings().size()) {
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
  }
}

void GreasePencil::move_frames(blender::bke::greasepencil::Layer &layer,
                               const blender::Map<int, int> &frame_number_destinations)
{
  return this->move_duplicate_frames(
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
    if (!frame.is_implicit_hold()) {
      src_layer_frames_durations.add(frame_number, layer.get_frame_duration_at(frame_number));
    }
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
    const int drawing_index = src_frame->drawing_index;
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
    GreasePencilFrame *frame = layer.add_frame(dst_frame_number, drawing_index, duration);
    *frame = *src_frame;
  }

  /* Remove drawings if they no longer have users. */
  this->remove_drawings_with_no_users();
}

const blender::bke::greasepencil::Drawing *GreasePencil::get_drawing_at(
    const blender::bke::greasepencil::Layer &layer, const int frame_number) const
{
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

blender::bke::greasepencil::Drawing *GreasePencil::get_editable_drawing_at(
    const blender::bke::greasepencil::Layer &layer, const int frame_number)
{
  if (!layer.is_editable()) {
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

std::optional<blender::Bounds<blender::float3>> GreasePencil::bounds_min_max(const int frame) const
{
  using namespace blender;
  std::optional<Bounds<float3>> bounds;
  const Span<const bke::greasepencil::Layer *> layers = this->layers();
  for (const int layer_i : layers.index_range()) {
    const bke::greasepencil::Layer &layer = *layers[layer_i];
    if (!layer.is_visible()) {
      continue;
    }
    if (const bke::greasepencil::Drawing *drawing = this->get_drawing_at(layer, frame)) {
      const bke::CurvesGeometry &curves = drawing->strokes();
      bounds = bounds::merge(bounds, curves.bounds_min_max());
    }
  }
  return bounds;
}

std::optional<blender::Bounds<blender::float3>> GreasePencil::bounds_min_max_eval() const
{
  return this->bounds_min_max(this->runtime->eval_frame);
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
  const int index = this->layers().first_index_try(&layer);
  if (index == -1) {
    return {};
  }
  return index;
}

const blender::bke::greasepencil::Layer *GreasePencil::get_active_layer() const
{
  if (this->active_layer == nullptr) {
    return nullptr;
  }
  return &this->active_layer->wrap();
}

blender::bke::greasepencil::Layer *GreasePencil::get_active_layer()
{
  if (this->active_layer == nullptr) {
    return nullptr;
  }
  return &this->active_layer->wrap();
}

void GreasePencil::set_active_layer(const blender::bke::greasepencil::Layer *layer)
{
  this->active_layer = const_cast<GreasePencilLayer *>(
      reinterpret_cast<const GreasePencilLayer *>(layer));

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

static blender::VectorSet<blender::StringRefNull> get_node_names(const GreasePencil &grease_pencil)
{
  using namespace blender;
  VectorSet<StringRefNull> names;
  for (const blender::bke::greasepencil::TreeNode *node : grease_pencil.nodes()) {
    names.add(node->name());
  }
  return names;
}

static bool check_unique_node_cb(void *arg, const char *name)
{
  using namespace blender;
  VectorSet<StringRefNull> &names = *reinterpret_cast<VectorSet<StringRefNull> *>(arg);
  return names.contains(name);
}

static void unique_node_name_ex(VectorSet<blender::StringRefNull> &names,
                                const char *default_name,
                                char *name)
{
  BLI_uniquename_cb(check_unique_node_cb, &names, default_name, '.', name, MAX_NAME);
}

static std::string unique_node_name(const GreasePencil &grease_pencil,
                                    const char *default_name,
                                    blender::StringRefNull name)
{
  using namespace blender;
  char unique_name[MAX_NAME];
  STRNCPY(unique_name, name.c_str());
  VectorSet<StringRefNull> names = get_node_names(grease_pencil);
  unique_node_name_ex(names, default_name, unique_name);
  return unique_name;
}

static std::string unique_layer_name(const GreasePencil &grease_pencil,
                                     blender::StringRefNull name)
{
  return unique_node_name(grease_pencil, DATA_("Layer"), name);
}

static std::string unique_layer_group_name(const GreasePencil &grease_pencil,
                                           blender::StringRefNull name)
{
  return unique_node_name(grease_pencil, DATA_("Group"), name);
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer(const blender::StringRefNull name)
{
  using namespace blender;
  std::string unique_name = unique_layer_name(*this, name);
  const int numLayers = layers().size();
  CustomData_realloc(&layers_data, numLayers, numLayers + 1);
  return root_group().add_layer(unique_name);
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer(
    blender::bke::greasepencil::LayerGroup &parent_group, const blender::StringRefNull name)
{
  using namespace blender;
  blender::bke::greasepencil::Layer &new_layer = add_layer(name);
  move_node_into(new_layer.as_node(), parent_group);
  return new_layer;
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer(
    const blender::bke::greasepencil::Layer &duplicate_layer)
{
  using namespace blender;
  std::string unique_name = unique_layer_name(*this, duplicate_layer.name());
  const int numLayers = layers().size();
  CustomData_realloc(&layers_data, numLayers, numLayers + 1);
  bke::greasepencil::Layer &new_layer = root_group().add_layer(duplicate_layer);
  this->update_drawing_users_for_layer(new_layer);
  new_layer.set_name(unique_name);
  return new_layer;
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer(
    blender::bke::greasepencil::LayerGroup &parent_group,
    const blender::bke::greasepencil::Layer &duplicate_layer)
{
  using namespace blender;
  bke::greasepencil::Layer &new_layer = add_layer(duplicate_layer);
  move_node_into(new_layer.as_node(), parent_group);
  return new_layer;
}

blender::bke::greasepencil::LayerGroup &GreasePencil::add_layer_group(
    blender::bke::greasepencil::LayerGroup &parent_group, const blender::StringRefNull name)
{
  using namespace blender;
  std::string unique_name = unique_layer_group_name(*this, name);
  return parent_group.add_group(unique_name);
}

static void reorder_customdata(CustomData &data, const Span<int> new_by_old_map)
{
  CustomData new_data;
  CustomData_copy_layout(&data, &new_data, CD_MASK_ALL, CD_CONSTRUCT, new_by_old_map.size());

  for (const int old_i : new_by_old_map.index_range()) {
    const int new_i = new_by_old_map[old_i];
    CustomData_copy_data(&data, &new_data, old_i, new_i, 1);
  }
  CustomData_free(&data, new_by_old_map.size());
  data = new_data;
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
    new_by_old_map[layer_i_old] = layer_i_new;
  }
  BLI_assert(old_layer_index_by_layer.is_empty());

  /* Use the mapping to re-order the custom data */
  reorder_customdata(grease_pencil.layers_data, new_by_old_map);
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
    const blender::StringRefNull name) const
{
  return this->root_group().find_node_by_name(name);
}

blender::bke::greasepencil::TreeNode *GreasePencil::find_node_by_name(
    const blender::StringRefNull name)
{
  return this->root_group().find_node_by_name(name);
}

blender::IndexMask GreasePencil::layer_selection_by_name(const blender::StringRefNull name,
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

void GreasePencil::rename_node(blender::bke::greasepencil::TreeNode &node,
                               blender::StringRefNull new_name)
{
  using namespace blender;
  if (node.name() == new_name) {
    return;
  }
  node.set_name(node.is_layer() ? unique_layer_name(*this, new_name) :
                                  unique_layer_group_name(*this, new_name));
}

static void shrink_customdata(CustomData &data, const int index_to_remove, const int size)
{
  using namespace blender;
  CustomData new_data;
  CustomData_copy_layout(&data, &new_data, CD_MASK_ALL, CD_CONSTRUCT, size);
  CustomData_realloc(&new_data, size, size - 1);

  const IndexRange range_before(index_to_remove);
  const IndexRange range_after(index_to_remove + 1, size - index_to_remove - 1);

  if (!range_before.is_empty()) {
    CustomData_copy_data(
        &data, &new_data, range_before.start(), range_before.start(), range_before.size());
  }
  if (!range_after.is_empty()) {
    CustomData_copy_data(
        &data, &new_data, range_after.start(), range_after.start() - 1, range_after.size());
  }

  CustomData_free(&data, size);
  data = new_data;
}

void GreasePencil::remove_layer(blender::bke::greasepencil::Layer &layer)
{
  using namespace blender::bke::greasepencil;
  /* If the layer is active, update the active layer. */
  const Layer *active_layer = this->get_active_layer();
  if (active_layer == &layer) {
    Span<const Layer *> layers = this->layers();
    /* If there is no other layer available , unset the active layer. */
    if (layers.size() == 1) {
      this->set_active_layer(nullptr);
    }
    else {
      /* Make the layer below active (if possible). */
      if (active_layer == layers.first()) {
        this->set_active_layer(layers[1]);
      }
      else {
        int64_t active_index = layers.first_index(active_layer);
        this->set_active_layer(layers[active_index - 1]);
      }
    }
  }

  /* Remove all the layer attributes and shrink the `CustomData`. */
  const int64_t layer_index = this->layers().first_index(&layer);
  shrink_customdata(this->layers_data, layer_index, this->layers().size());

  /* Unlink the layer from the parent group. */
  layer.parent_group().unlink_node(layer.as_node());

  /* Remove drawings. */
  for (const GreasePencilFrame frame : layer.frames().values()) {
    GreasePencilDrawingBase *drawing_base = this->drawing(frame.drawing_index);
    if (drawing_base->type != GP_DRAWING) {
      continue;
    }
    GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
    drawing->wrap().remove_user();
  }
  this->remove_drawings_with_no_users();

  /* Delete the layer. */
  MEM_delete(&layer);
}

void GreasePencil::print_layer_tree()
{
  using namespace blender::bke::greasepencil;
  this->root_group().print_nodes("Layer Tree:");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Drawing array read/write functions
 * \{ */

static void read_drawing_array(GreasePencil &grease_pencil, BlendDataReader *reader)
{
  BLO_read_pointer_array(reader, reinterpret_cast<void **>(&grease_pencil.drawing_array));
  for (int i = 0; i < grease_pencil.drawing_array_num; i++) {
    BLO_read_data_address(reader, &grease_pencil.drawing_array[i]);
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
        GreasePencilDrawingReference *drawing_reference =
            reinterpret_cast<GreasePencilDrawingReference *>(drawing_base);
        BLO_read_data_address(reader, &drawing_reference->id_reference);
        break;
      }
    }
  }
}

static void write_drawing_array(GreasePencil &grease_pencil, BlendWriter *writer)
{
  using namespace blender;
  BLO_write_pointer_array(writer, grease_pencil.drawing_array_num, grease_pencil.drawing_array);
  for (int i = 0; i < grease_pencil.drawing_array_num; i++) {
    GreasePencilDrawingBase *drawing_base = grease_pencil.drawing_array[i];
    switch (GreasePencilDrawingType(drawing_base->type)) {
      case GP_DRAWING: {
        GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
        bke::CurvesGeometry::BlendWriteData write_data =
            drawing->wrap().strokes_for_write().blend_write_prepare();
        BLO_write_struct(writer, GreasePencilDrawing, drawing);
        drawing->wrap().strokes_for_write().blend_write(*writer, grease_pencil.id, write_data);
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
  if (grease_pencil.drawing_array == nullptr) {
    BLI_assert(grease_pencil.drawing_array_num == 0);
    return;
  }
  for (int i = 0; i < grease_pencil.drawing_array_num; i++) {
    GreasePencilDrawingBase *drawing_base = grease_pencil.drawing_array[i];
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
  MEM_freeN(grease_pencil.drawing_array);
  grease_pencil.drawing_array = nullptr;
  grease_pencil.drawing_array_num = 0;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Layer tree read/write functions
 * \{ */

static void read_layer(BlendDataReader *reader,
                       GreasePencilLayer *node,
                       GreasePencilLayerTreeGroup *parent)
{
  BLO_read_data_address(reader, &node->base.name);
  node->base.parent = parent;
  BLO_read_data_address(reader, &node->parsubstr);

  /* Read frames storage. */
  BLO_read_int32_array(reader, node->frames_storage.num, &node->frames_storage.keys);
  BLO_read_data_address(reader, &node->frames_storage.values);

  /* Read layer masks. */
  BLO_read_list(reader, &node->masks);
  LISTBASE_FOREACH (GreasePencilLayerMask *, mask, &node->masks) {
    BLO_read_data_address(reader, &mask->layer_name);
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
  BLO_read_data_address(reader, &node->base.name);
  node->base.parent = parent;
  /* Read list of children. */
  BLO_read_list(reader, &node->children);
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
  BLO_read_data_address(reader, &grease_pencil.root_group_ptr);
  /* This shouldn't normally happen, but for files that were created before the root group became a
   * pointer, this address will not exist. In this case, we clear the pointer to the active layer
   * and create an empty root group to avoid crashes. */
  if (grease_pencil.root_group_ptr == nullptr) {
    grease_pencil.root_group_ptr = MEM_new<blender::bke::greasepencil::LayerGroup>(__func__);
    grease_pencil.active_layer = nullptr;
    return;
  }
  /* Read active layer. */
  BLO_read_data_address(reader, &grease_pencil.active_layer);
  read_layer_tree_group(reader, grease_pencil.root_group_ptr, nullptr);

  grease_pencil.root_group_ptr->wrap().update_from_dna_read();
}

static void write_layer(BlendWriter *writer, GreasePencilLayer *node)
{
  BLO_write_struct(writer, GreasePencilLayer, node);
  BLO_write_string(writer, node->base.name);
  BLO_write_string(writer, node->parsubstr);

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
