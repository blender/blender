/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_anim_data.h"
#include "BKE_curves.hh"
#include "BKE_customdata.h"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_object.h"

#include "BLI_bounds.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memarena.h"
#include "BLI_memory_utils.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_span.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utils.h"
#include "BLI_vector_set.hh"

#include "BLO_read_write.h"

#include "BLT_translation.h"

#include "DNA_ID.h"
#include "DNA_ID_enums.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_material_types.h"

#include "MEM_guardedalloc.h"

using blender::float3;
using blender::Span;
using blender::uint3;
using blender::VectorSet;

static void grease_pencil_init_data(ID *id)
{
  using namespace blender::bke;

  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
  grease_pencil->runtime = MEM_new<GreasePencilRuntime>(__func__);

  new (&grease_pencil->root_group) greasepencil::LayerGroup();
  grease_pencil->active_layer = nullptr;
}

static void grease_pencil_copy_data(Main * /*bmain*/,
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

  /* Duplicate drawing array. */
  grease_pencil_dst->drawing_array_num = grease_pencil_src->drawing_array_num;
  grease_pencil_dst->drawing_array = MEM_cnew_array<GreasePencilDrawingBase *>(
      grease_pencil_src->drawing_array_num, __func__);
  for (int i = 0; i < grease_pencil_src->drawing_array_num; i++) {
    const GreasePencilDrawingBase *src_drawing_base = grease_pencil_src->drawing_array[i];
    switch (src_drawing_base->type) {
      case GP_DRAWING: {
        const GreasePencilDrawing *src_drawing = reinterpret_cast<const GreasePencilDrawing *>(
            src_drawing_base);
        grease_pencil_dst->drawing_array[i] = reinterpret_cast<GreasePencilDrawingBase *>(
            MEM_new<bke::greasepencil::Drawing>(__func__, src_drawing->wrap()));
        break;
      }
      case GP_DRAWING_REFERENCE: {
        const GreasePencilDrawingReference *src_drawing_reference =
            reinterpret_cast<const GreasePencilDrawingReference *>(src_drawing_base);
        grease_pencil_dst->drawing_array[i] = reinterpret_cast<GreasePencilDrawingBase *>(
            MEM_dupallocN(src_drawing_reference));
        break;
      }
    }
    /* TODO: Update drawing user counts. */
  }

  /* Duplicate layer tree. */
  new (&grease_pencil_dst->root_group)
      bke::greasepencil::LayerGroup(grease_pencil_src->root_group.wrap());

  /* Set active layer. */
  if (grease_pencil_src->has_active_layer()) {
    grease_pencil_dst->set_active_layer(
        grease_pencil_dst->find_layer_by_name(grease_pencil_src->active_layer->wrap().name()));
  }

  /* Make sure the runtime pointer exists. */
  grease_pencil_dst->runtime = MEM_new<bke::GreasePencilRuntime>(__func__);
}

static void grease_pencil_free_data(ID *id)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
  BKE_animdata_free(&grease_pencil->id, false);

  MEM_SAFE_FREE(grease_pencil->material_array);

  grease_pencil->free_drawing_array();
  grease_pencil->root_group.wrap().~LayerGroup();

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

  /* Write LibData */
  BLO_write_id_struct(writer, GreasePencil, id_address, &grease_pencil->id);
  BKE_id_blend_write(writer, &grease_pencil->id);

  /* Write animation data. */
  if (grease_pencil->adt) {
    BKE_animdata_blend_write(writer, grease_pencil->adt);
  }

  /* Write drawings. */
  grease_pencil->write_drawing_array(writer);
  /* Write layer tree. */
  grease_pencil->write_layer_tree(writer);

  /* Write materials. */
  BLO_write_pointer_array(
      writer, grease_pencil->material_array_num, grease_pencil->material_array);
}

static void grease_pencil_blend_read_data(BlendDataReader *reader, ID *id)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);

  /* Read animation data. */
  BLO_read_data_address(reader, &grease_pencil->adt);
  BKE_animdata_blend_read_data(reader, grease_pencil->adt);

  /* Read drawings. */
  grease_pencil->read_drawing_array(reader);
  /* Read layer tree. */
  grease_pencil->read_layer_tree(reader);
  /* Read active layer. */
  BLO_read_data_address(reader, reinterpret_cast<void **>(&grease_pencil->active_layer));

  /* Read materials. */
  BLO_read_pointer_array(reader, reinterpret_cast<void **>(&grease_pencil->material_array));

  grease_pencil->runtime = MEM_new<blender::bke::GreasePencilRuntime>(__func__);
}

static void grease_pencil_blend_read_lib(BlendLibReader *reader, ID *id)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
  for (int i = 0; i < grease_pencil->material_array_num; i++) {
    BLO_read_id_address(reader, id, &grease_pencil->material_array[i]);
  }
  for (int i = 0; i < grease_pencil->drawing_array_num; i++) {
    GreasePencilDrawingBase *drawing_base = grease_pencil->drawing_array[i];
    if (drawing_base->type == GP_DRAWING_REFERENCE) {
      GreasePencilDrawingReference *drawing_reference =
          reinterpret_cast<GreasePencilDrawingReference *>(drawing_base);
      BLO_read_id_address(reader, id, &drawing_reference->id_reference);
    }
  }
}

static void grease_pencil_blend_read_expand(BlendExpander *expander, ID *id)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
  for (int i = 0; i < grease_pencil->material_array_num; i++) {
    BLO_expand(expander, grease_pencil->material_array[i]);
  }
  for (int i = 0; i < grease_pencil->drawing_array_num; i++) {
    GreasePencilDrawingBase *drawing_base = grease_pencil->drawing_array[i];
    if (drawing_base->type == GP_DRAWING_REFERENCE) {
      GreasePencilDrawingReference *drawing_reference =
          reinterpret_cast<GreasePencilDrawingReference *>(drawing_base);
      BLO_expand(expander, drawing_reference->id_reference);
    }
  }
}

IDTypeInfo IDType_ID_GP = {
    /*id_code*/ ID_GP,
    /*id_filter*/ FILTER_ID_GP,
    /*main_listbase_index*/ INDEX_ID_GP,
    /*struct_size*/ sizeof(GreasePencil),
    /*name*/ "GreasePencil",
    /*name_plural*/ "grease_pencils_v3",
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
    /*blend_read_lib*/ grease_pencil_blend_read_lib,
    /*blend_read_expand*/ grease_pencil_blend_read_expand,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

namespace blender::bke::greasepencil {

Drawing::Drawing()
{
  this->base.type = GP_DRAWING;
  this->base.flag = 0;

  new (&this->geometry) bke::CurvesGeometry();
  /* Initialize runtime data. */
  this->runtime = MEM_new<bke::greasepencil::DrawingRuntime>(__func__);
}

Drawing::Drawing(const Drawing &other) : Drawing()
{
  this->base.flag = other.base.flag;

  new (&this->geometry) bke::CurvesGeometry(other.geometry.wrap());
  this->runtime->triangles_cache = other.runtime->triangles_cache;
}

Drawing::~Drawing()
{
  this->geometry.wrap().~CurvesGeometry();
  MEM_delete(this->runtime);
  this->runtime = nullptr;
}

Span<uint3> Drawing::triangles() const
{
  this->runtime->triangles_cache.ensure([&](Vector<uint3> &r_data) {
    MemArena *pf_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

    const CurvesGeometry &curves = this->geometry.wrap();
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

const bke::CurvesGeometry &Drawing::strokes() const
{
  return this->geometry.wrap();
}

bke::CurvesGeometry &Drawing::strokes_for_write()
{
  return this->geometry.wrap();
}

void Drawing::tag_positions_changed()
{
  this->geometry.wrap().tag_positions_changed();
  this->runtime->triangles_cache.tag_dirty();
}

TreeNode::TreeNode()
{
  this->next = this->prev = nullptr;
  this->parent = nullptr;

  this->name = nullptr;
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
  this->name = BLI_strdup(name.c_str());
}

TreeNode::TreeNode(const TreeNode &other)
    : TreeNode::TreeNode(GreasePencilLayerTreeNodeType(other.type))
{
  if (other.name) {
    this->name = BLI_strdup(other.name);
  }
  this->flag = other.flag;
  copy_v3_v3_uchar(this->color, other.color);
}

const LayerGroup &TreeNode::as_group() const
{
  return *reinterpret_cast<const LayerGroup *>(this);
}

const Layer &TreeNode::as_layer() const
{
  return *reinterpret_cast<const Layer *>(this);
}

LayerGroup &TreeNode::as_group_for_write()
{
  return *reinterpret_cast<LayerGroup *>(this);
}

Layer &TreeNode::as_layer_for_write()
{
  return *reinterpret_cast<Layer *>(this);
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

Layer::Layer()
{
  this->base = TreeNode(GP_LAYER_TREE_LEAF);

  this->frames_storage.num = 0;
  this->frames_storage.keys = nullptr;
  this->frames_storage.values = nullptr;
  this->frames_storage.flag = 0;

  BLI_listbase_clear(&this->masks);

  this->runtime = MEM_new<LayerRuntime>(__func__);
}

Layer::Layer(StringRefNull name) : Layer()
{
  this->base.name = BLI_strdup(name.c_str());
}

Layer::Layer(const Layer &other) : Layer()
{
  this->base = TreeNode(other.base.wrap());

  /* TODO: duplicate masks. */

  /* Note: We do not duplicate the frame storage since it is only needed for writing. */

  this->blend_mode = other.blend_mode;
  this->opacity = other.opacity;

  this->runtime->frames_ = other.runtime->frames_;
  this->runtime->sorted_keys_cache_ = other.runtime->sorted_keys_cache_;
  /* TODO: what about masks cache? */
}

Layer::~Layer()
{
  MEM_SAFE_FREE(this->base.name);
  MEM_SAFE_FREE(this->frames_storage.keys);
  MEM_SAFE_FREE(this->frames_storage.values);

  LISTBASE_FOREACH_MUTABLE (GreasePencilLayerMask *, mask, &this->masks) {
    MEM_SAFE_FREE(mask->layer_name);
    MEM_freeN(mask);
  }

  MEM_delete(this->runtime);
  this->runtime = nullptr;
}

void Layer::set_name(StringRefNull new_name)
{
  this->base.name = BLI_strdup(new_name.c_str());
}

const Map<int, GreasePencilFrame> &Layer::frames() const
{
  return this->runtime->frames_;
}

Map<int, GreasePencilFrame> &Layer::frames_for_write()
{
  return this->runtime->frames_;
}

bool Layer::is_visible() const
{
  return this->parent_group().is_visible() && (this->base.flag & GP_LAYER_TREE_NODE_HIDE) == 0;
}

bool Layer::is_locked() const
{
  return this->parent_group().is_locked() || (this->base.flag & GP_LAYER_TREE_NODE_LOCKED) != 0;
}

bool Layer::is_editable() const
{
  return !this->is_locked() && this->is_visible();
}

bool Layer::insert_frame(const int frame_number, const GreasePencilFrame &frame)
{
  BLI_assert(!frame.is_null());
  if (!this->frames().contains(frame_number)) {
    this->frames_for_write().add(frame_number, frame);
    this->tag_frames_map_keys_changed();
    return true;
  }
  /* Overwrite null-frames. */
  if (this->frames().lookup(frame_number).is_null()) {
    this->frames_for_write().add_overwrite(frame_number, frame);
    this->tag_frames_map_changed();
    return true;
  }
  return false;
}

bool Layer::insert_frame(const int frame_number,
                         const int duration,
                         const GreasePencilFrame &frame)
{
  BLI_assert(duration > 0);
  if (!this->insert_frame(frame_number, frame)) {
    return false;
  }
  Span<int> sorted_keys = this->sorted_keys();
  const int end_frame_number = frame_number + duration;
  /* Finds the next greater frame_number that is stored in the map. */
  auto next_frame_number_it = std::upper_bound(
      sorted_keys.begin(), sorted_keys.end(), frame_number);
  /* If the next frame we found is at the end of the frame we're inserting, then we are done. */
  if (next_frame_number_it != sorted_keys.end() && *next_frame_number_it == end_frame_number) {
    return true;
  }
  /* While the next frame is a null frame, remove it. */
  while (next_frame_number_it != sorted_keys.end() &&
         this->frames().lookup(*next_frame_number_it).is_null())
  {
    this->frames_for_write().remove(*next_frame_number_it);
    this->tag_frames_map_keys_changed();
    next_frame_number_it = std::next(next_frame_number_it);
  }
  /* If the next frame comes after the end of the frame we're inserting (or if there are no more
   * frames), add a null-frame. */
  if (next_frame_number_it == sorted_keys.end() || *next_frame_number_it > end_frame_number) {
    this->frames_for_write().add(end_frame_number, GreasePencilFrame::null());
    this->tag_frames_map_keys_changed();
  }
  return true;
}

bool Layer::overwrite_frame(int frame_number, const GreasePencilFrame &frame)
{
  this->tag_frames_map_changed();
  return this->frames_for_write().add_overwrite(frame_number, frame);
}

Span<int> Layer::sorted_keys() const
{
  this->runtime->sorted_keys_cache_.ensure([&](Vector<int> &r_data) {
    r_data.reinitialize(this->frames().size());
    int i = 0;
    for (int64_t key : this->frames().keys()) {
      r_data[i++] = key;
    }
    std::sort(r_data.begin(), r_data.end());
  });
  return this->runtime->sorted_keys_cache_.data();
}

int Layer::drawing_index_at(const int frame) const
{
  Span<int> sorted_keys = this->sorted_keys();
  /* No keyframes, return no drawing. */
  if (sorted_keys.size() == 0) {
    return -1;
  }
  /* Before the first drawing, return no drawing. */
  if (frame < sorted_keys.first()) {
    return -1;
  }
  /* After or at the last drawing, return the last drawing. */
  if (frame >= sorted_keys.last()) {
    return this->frames().lookup(sorted_keys.last()).drawing_index;
  }
  /* Search for the drawing. upper_bound will get the drawing just after. */
  auto it = std::upper_bound(sorted_keys.begin(), sorted_keys.end(), frame);
  if (it == sorted_keys.end() || it == sorted_keys.begin()) {
    return -1;
  }
  return this->frames().lookup(*std::prev(it)).drawing_index;
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

LayerGroup::LayerGroup()
{
  this->base = TreeNode(GP_LAYER_TREE_GROUP);

  BLI_listbase_clear(&this->children);

  this->runtime = MEM_new<LayerGroupRuntime>(__func__);
}

LayerGroup::LayerGroup(StringRefNull name) : LayerGroup()
{
  this->base.name = BLI_strdup(name.c_str());
}

LayerGroup::LayerGroup(const LayerGroup &other) : LayerGroup()
{
  this->base = TreeNode(other.base.wrap());

  LISTBASE_FOREACH (GreasePencilLayerTreeNode *, child, &other.children) {
    switch (child->type) {
      case GP_LAYER_TREE_LEAF: {
        GreasePencilLayer *layer = reinterpret_cast<GreasePencilLayer *>(child);
        Layer *dup_layer = MEM_new<Layer>(__func__, layer->wrap());
        this->add_layer(dup_layer);
        break;
      }
      case GP_LAYER_TREE_GROUP: {
        GreasePencilLayerTreeGroup *group = reinterpret_cast<GreasePencilLayerTreeGroup *>(child);
        LayerGroup *dup_group = MEM_new<LayerGroup>(__func__, group->wrap());
        this->add_group(dup_group);
        break;
      }
    }
  }
}

LayerGroup::~LayerGroup()
{
  MEM_SAFE_FREE(this->base.name);

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

void LayerGroup::set_name(StringRefNull new_name)
{
  this->base.name = BLI_strdup(new_name.c_str());
}

bool LayerGroup::is_visible() const
{
  if (this->base.parent) {
    return this->base.parent->wrap().is_visible() &&
           (this->base.flag & GP_LAYER_TREE_NODE_HIDE) == 0;
  }
  return (this->base.flag & GP_LAYER_TREE_NODE_HIDE) == 0;
}

bool LayerGroup::is_locked() const
{
  if (this->base.parent) {
    return this->base.parent->wrap().is_locked() ||
           (this->base.flag & GP_LAYER_TREE_NODE_LOCKED) != 0;
  }
  return (this->base.flag & GP_LAYER_TREE_NODE_LOCKED) != 0;
}

LayerGroup &LayerGroup::add_group(LayerGroup *group)
{
  BLI_assert(group != nullptr);
  BLI_addtail(&this->children, reinterpret_cast<GreasePencilLayerTreeNode *>(group));
  group->base.parent = reinterpret_cast<GreasePencilLayerTreeGroup *>(this);
  this->tag_nodes_cache_dirty();
  return *group;
}

LayerGroup &LayerGroup::add_group(StringRefNull name)
{
  LayerGroup *new_group = MEM_new<LayerGroup>(__func__, name);
  return this->add_group(new_group);
}

LayerGroup &LayerGroup::add_group_after(LayerGroup *group, TreeNode *link)
{
  BLI_assert(group != nullptr && link != nullptr);
  BLI_insertlinkafter(&this->children,
                      reinterpret_cast<GreasePencilLayerTreeNode *>(link),
                      reinterpret_cast<GreasePencilLayerTreeNode *>(group));
  group->base.parent = reinterpret_cast<GreasePencilLayerTreeGroup *>(this);
  this->tag_nodes_cache_dirty();
  return *group;
}

LayerGroup &LayerGroup::add_group_after(StringRefNull name, TreeNode *link)
{
  LayerGroup *new_group = MEM_new<LayerGroup>(__func__, name);
  return this->add_group_after(new_group, link);
}

Layer &LayerGroup::add_layer(Layer *layer)
{
  BLI_assert(layer != nullptr);
  BLI_addtail(&this->children, reinterpret_cast<GreasePencilLayerTreeNode *>(layer));
  layer->base.parent = reinterpret_cast<GreasePencilLayerTreeGroup *>(this);
  this->tag_nodes_cache_dirty();
  return *layer;
}

Layer &LayerGroup::add_layer_before(Layer *layer, Layer *link)
{
  BLI_assert(layer != nullptr && link != nullptr);
  BLI_insertlinkbefore(&this->children,
                       reinterpret_cast<GreasePencilLayerTreeNode *>(link),
                       reinterpret_cast<GreasePencilLayerTreeNode *>(layer));
  layer->base.parent = reinterpret_cast<GreasePencilLayerTreeGroup *>(this);
  this->tag_nodes_cache_dirty();
  return *layer;
}

Layer &LayerGroup::add_layer_after(Layer *layer, Layer *link)
{
  BLI_assert(layer != nullptr && link != nullptr);
  BLI_insertlinkafter(&this->children,
                      reinterpret_cast<GreasePencilLayerTreeNode *>(link),
                      reinterpret_cast<GreasePencilLayerTreeNode *>(layer));
  layer->base.parent = reinterpret_cast<GreasePencilLayerTreeGroup *>(this);
  this->tag_nodes_cache_dirty();
  return *layer;
}

Layer &LayerGroup::add_layer(StringRefNull name)
{
  Layer *new_layer = MEM_new<Layer>(__func__, name);
  return this->add_layer(new_layer);
}

Layer &LayerGroup::add_layer_before(StringRefNull name, Layer *link)
{
  Layer *new_layer = MEM_new<Layer>(__func__, name);
  return this->add_layer_before(new_layer, link);
}

Layer &LayerGroup::add_layer_after(StringRefNull name, Layer *link)
{
  Layer *new_layer = MEM_new<Layer>(__func__, name);
  return this->add_layer_after(new_layer, link);
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

bool LayerGroup::unlink_layer(Layer *link)
{
  if (BLI_remlink_safe(&this->children, link)) {
    this->tag_nodes_cache_dirty();
    link->base.parent = nullptr;
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

const Layer *LayerGroup::find_layer_by_name(const StringRefNull name) const
{
  for (const Layer *layer : this->layers()) {
    if (StringRef(layer->name()) == StringRef(name)) {
      return layer;
    }
  }
  return nullptr;
}

Layer *LayerGroup::find_layer_by_name(const StringRefNull name)
{
  for (Layer *layer : this->layers_for_write()) {
    if (StringRef(layer->name()) == StringRef(name)) {
      return layer;
    }
  }
  return nullptr;
}

const LayerGroup *LayerGroup::find_group_by_name(StringRefNull name) const
{
  for (const LayerGroup *group : this->groups()) {
    if (StringRef(group->name()) == StringRef(name)) {
      return group;
    }
  }
  return nullptr;
}

LayerGroup *LayerGroup::find_group_by_name(StringRefNull name)
{
  for (LayerGroup *group : this->groups_for_write()) {
    if (StringRef(group->name()) == StringRef(name)) {
      return group;
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
      std::cout << StringRefNull(node->name);
    }
    else if (node->is_group()) {
      std::cout << StringRefNull(node->name) << ": ";
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
          this->runtime->layer_cache_.append(&node->as_layer_for_write());
          break;
        }
        case GP_LAYER_TREE_GROUP: {
          this->runtime->layer_group_cache_.append(&node->as_group_for_write());
          for (TreeNode *child : node->as_group_for_write().nodes_for_write()) {
            this->runtime->nodes_cache_.append(child);
            if (child->is_layer()) {
              this->runtime->layer_cache_.append(&child->as_layer_for_write());
            }
            else if (child->is_group()) {
              this->runtime->layer_group_cache_.append(&child->as_group_for_write());
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

BoundBox *BKE_grease_pencil_boundbox_get(Object *ob)
{
  using namespace blender;
  BLI_assert(ob->type == OB_GREASE_PENCIL);
  const GreasePencil *grease_pencil = static_cast<const GreasePencil *>(ob->data);
  if (ob->runtime.bb != nullptr && (ob->runtime.bb->flag & BOUNDBOX_DIRTY) == 0) {
    return ob->runtime.bb;
  }
  if (ob->runtime.bb == nullptr) {
    ob->runtime.bb = MEM_cnew<BoundBox>(__func__);
  }

  if (const std::optional<Bounds<float3>> bounds = grease_pencil->bounds_min_max()) {
    BKE_boundbox_init_from_minmax(ob->runtime.bb, bounds->min, bounds->max);
  }
  else {
    BKE_boundbox_init_from_minmax(ob->runtime.bb, float3(-1), float3(1));
  }

  return ob->runtime.bb;
}

void BKE_grease_pencil_data_update(Depsgraph * /*depsgraph*/, Scene * /*scene*/, Object *object)
{
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(object->data);
  /* Evaluate modifiers. */
  /* TODO: evaluate modifiers. */

  /* Assign evaluated object. */
  /* TODO: Get eval from modifiers geometry set. */
  GreasePencil *grease_pencil_eval = reinterpret_cast<GreasePencil *>(
      BKE_id_copy_ex(nullptr, &grease_pencil->id, nullptr, LIB_ID_COPY_LOCALIZE));
  BKE_object_eval_assign_data(object, &grease_pencil_eval->id, true);
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

/** \} */

/* ------------------------------------------------------------------- */
/** \name Grease Pencil reference functions
 * \{ */

static bool grease_pencil_references_cyclic_check_internal(const GreasePencil *id_reference,
                                                           const GreasePencil *grease_pencil)
{
  for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type == GP_DRAWING_REFERENCE) {
      GreasePencilDrawingReference *reference = reinterpret_cast<GreasePencilDrawingReference *>(
          base);
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
/** \name Grease Pencil runtime API
 * \{ */

bool blender::bke::GreasePencilRuntime::has_stroke_buffer() const
{
  return this->stroke_cache.points.size() > 0;
}

blender::Span<blender::bke::greasepencil::StrokePoint> blender::bke::GreasePencilRuntime::
    stroke_buffer() const
{
  return this->stroke_cache.points.as_span();
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

  *array = new_array;
  *num = new_array_num;
}
template<typename T> static void shrink_array(T **array, int *num, const int shrink_num)
{
  BLI_assert(shrink_num > 0);
  const int new_array_num = *num - shrink_num;
  T *new_array = reinterpret_cast<T *>(MEM_cnew_array<T *>(new_array_num, __func__));

  blender::uninitialized_move_n(*array, new_array_num, new_array);
  MEM_freeN(*array);

  *array = new_array;
  *num = new_array_num;
}

blender::Span<GreasePencilDrawingBase *> GreasePencil::drawings() const
{
  return blender::Span<GreasePencilDrawingBase *>{this->drawing_array, this->drawing_array_num};
}

blender::MutableSpan<GreasePencilDrawingBase *> GreasePencil::drawings_for_write()
{
  return blender::MutableSpan<GreasePencilDrawingBase *>{this->drawing_array,
                                                         this->drawing_array_num};
}

void GreasePencil::add_empty_drawings(const int add_num)
{
  using namespace blender;
  BLI_assert(add_num > 0);
  const int prev_num = this->drawings().size();
  grow_array<GreasePencilDrawingBase *>(&this->drawing_array, &this->drawing_array_num, add_num);
  MutableSpan<GreasePencilDrawingBase *> new_drawings = this->drawings_for_write().drop_front(
      prev_num);
  for (const int i : new_drawings.index_range()) {
    new_drawings[i] = reinterpret_cast<GreasePencilDrawingBase *>(
        MEM_new<blender::bke::greasepencil::Drawing>(__func__));
  }

  /* TODO: Update drawing user counts. */
}

bool GreasePencil::insert_blank_frame(blender::bke::greasepencil::Layer &layer,
                                      int frame_number,
                                      int duration,
                                      eBezTriple_KeyframeType keytype)
{
  using namespace blender;
  GreasePencilFrame frame{static_cast<int>(this->drawings().size()), 0, keytype};
  if (!layer.insert_frame(frame_number, duration, frame)) {
    return false;
  }
  this->add_empty_drawings(1);
  return true;
}

void GreasePencil::remove_drawing(const int index_to_remove)
{
  using namespace blender::bke::greasepencil;
  /* In order to not change the indices of the drawings, we do the following to the drawing to be
   * removed:
   *  - If the drawing (A) is not the last one:
   *     1.1) Find any frames in the layers that reference the last drawing (B) and point them to
   *          A's index.
   *     1.2) Swap drawing A with drawing B.
   *  2) Destroy A and shrink the array by one.
   *  3) Remove any frames in the layers that reference the A's index.
   */
  BLI_assert(this->drawing_array_num > 0);
  BLI_assert(index_to_remove >= 0 && index_to_remove < this->drawing_array_num);

  /* Move the drawing that should be removed to the last index. */
  const int last_drawing_index = this->drawing_array_num - 1;
  if (index_to_remove != last_drawing_index) {
    for (Layer *layer : this->layers_for_write()) {
      blender::Map<int, GreasePencilFrame> &frames = layer->frames_for_write();
      for (auto [key, value] : frames.items()) {
        if (value.drawing_index == last_drawing_index) {
          value.drawing_index = index_to_remove;
        }
        else if (value.drawing_index == index_to_remove) {
          value.drawing_index = last_drawing_index;
        }
      }
    }
    std::swap(this->drawings_for_write()[index_to_remove],
              this->drawings_for_write()[last_drawing_index]);
  }

  /* Delete the last drawing. */
  GreasePencilDrawingBase *drawing_base_to_remove = this->drawings_for_write()[last_drawing_index];
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
      MEM_freeN(drawing_reference_to_remove);
      break;
    }
  }

  /* Remove any frame that points to the last drawing. */
  for (Layer *layer : this->layers_for_write()) {
    blender::Map<int, GreasePencilFrame> &frames = layer->frames_for_write();
    int64_t frames_removed = frames.remove_if([last_drawing_index](auto item) {
      return item.value.drawing_index == last_drawing_index;
    });
    if (frames_removed > 0) {
      layer->tag_frames_map_keys_changed();
    }
  }

  /* Shrink drawing array. */
  shrink_array<GreasePencilDrawingBase *>(&this->drawing_array, &this->drawing_array_num, 1);
}

enum ForeachDrawingMode {
  VISIBLE,
  EDITABLE,
};

static void foreach_drawing_ex(
    GreasePencil &grease_pencil,
    int frame,
    ForeachDrawingMode mode,
    blender::FunctionRef<void(int, blender::bke::greasepencil::Drawing &)> function)
{
  using namespace blender::bke::greasepencil;

  blender::Span<GreasePencilDrawingBase *> drawings = grease_pencil.drawings();
  for (const Layer *layer : grease_pencil.layers()) {
    switch (mode) {
      case VISIBLE: {
        if (!layer->is_visible()) {
          continue;
        }
        break;
      }
      case EDITABLE: {
        if (!layer->is_editable()) {
          continue;
        }
        break;
      }
    }

    int index = layer->drawing_index_at(frame);
    if (index == -1) {
      continue;
    }
    GreasePencilDrawingBase *drawing_base = drawings[index];
    if (drawing_base->type == GP_DRAWING) {
      GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
      function(index, drawing->wrap());
    }
    else if (drawing_base->type == GP_DRAWING_REFERENCE) {
      /* TODO */
    }
  }
}

void GreasePencil::foreach_visible_drawing(
    int frame, blender::FunctionRef<void(int, blender::bke::greasepencil::Drawing &)> function)
{
  foreach_drawing_ex(*this, frame, VISIBLE, function);
}

void GreasePencil::foreach_editable_drawing(
    int frame, blender::FunctionRef<void(int, blender::bke::greasepencil::Drawing &)> function)
{
  foreach_drawing_ex(*this, frame, EDITABLE, function);
}

std::optional<blender::Bounds<blender::float3>> GreasePencil::bounds_min_max() const
{
  using namespace blender;
  /* FIXME: this should somehow go through the visible drawings. We don't have access to the
   * scene time here, so we probably need to cache the visible drawing for each layer somehow. */
  std::optional<Bounds<float3>> bounds;
  for (int i = 0; i < this->drawing_array_num; i++) {
    GreasePencilDrawingBase *drawing_base = this->drawing_array[i];
    switch (drawing_base->type) {
      case GP_DRAWING: {
        GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
        const bke::CurvesGeometry &curves = drawing->geometry.wrap();
        bounds = bounds::merge(bounds, curves.bounds_min_max());
        break;
      }
      case GP_DRAWING_REFERENCE: {
        /* TODO: Calculate the bounding box of the reference drawing. */
        break;
      }
    }
  }

  return bounds;
}

blender::Span<const blender::bke::greasepencil::TreeNode *> GreasePencil::nodes() const
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group.wrap().nodes();
}

blender::Span<const blender::bke::greasepencil::Layer *> GreasePencil::layers() const
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group.wrap().layers();
}

blender::Span<blender::bke::greasepencil::Layer *> GreasePencil::layers_for_write()
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group.wrap().layers_for_write();
}

blender::Span<const blender::bke::greasepencil::LayerGroup *> GreasePencil::groups() const
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group.wrap().groups();
}

blender::Span<blender::bke::greasepencil::LayerGroup *> GreasePencil::groups_for_write()
{
  BLI_assert(this->runtime != nullptr);
  return this->root_group.wrap().groups_for_write();
}

const blender::bke::greasepencil::Layer *GreasePencil::get_active_layer() const
{
  if (this->active_layer == nullptr) {
    return nullptr;
  }
  return &this->active_layer->wrap();
}

blender::bke::greasepencil::Layer *GreasePencil::get_active_layer_for_write()
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
}

static blender::VectorSet<blender::StringRefNull> get_node_names(GreasePencil &grease_pencil)
{
  using namespace blender;
  VectorSet<StringRefNull> names;
  for (const blender::bke::greasepencil::TreeNode *node : grease_pencil.nodes()) {
    names.add(node->name);
  }
  return names;
}

static bool check_unique_node_cb(void *arg, const char *name)
{
  using namespace blender;
  VectorSet<StringRefNull> &names = *reinterpret_cast<VectorSet<StringRefNull> *>(arg);
  return names.contains(name);
}

static bool unique_layer_name(VectorSet<blender::StringRefNull> &names, char *name)
{
  return BLI_uniquename_cb(check_unique_node_cb, &names, "GP_Layer", '.', name, MAX_NAME);
}

static bool unique_layer_group_name(VectorSet<blender::StringRefNull> &names, char *name)
{
  return BLI_uniquename_cb(check_unique_node_cb, &names, "GP_Group", '.', name, MAX_NAME);
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer(
    blender::bke::greasepencil::LayerGroup &group, const blender::StringRefNull name)
{
  using namespace blender;
  VectorSet<StringRefNull> names = get_node_names(*this);
  std::string unique_name(name.c_str());
  unique_layer_name(names, unique_name.data());
  return group.add_layer(unique_name);
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer_after(
    blender::bke::greasepencil::LayerGroup &group,
    blender::bke::greasepencil::Layer *layer,
    const blender::StringRefNull name)
{
  using namespace blender;
  VectorSet<StringRefNull> names = get_node_names(*this);
  std::string unique_name(name.c_str());
  unique_layer_name(names, unique_name.data());
  return group.add_layer_after(unique_name, layer);
}

blender::bke::greasepencil::Layer &GreasePencil::add_layer(const blender::StringRefNull name)
{
  return this->add_layer(this->root_group.wrap(), name);
}

blender::bke::greasepencil::LayerGroup &GreasePencil::add_layer_group(
    blender::bke::greasepencil::LayerGroup &group, const blender::StringRefNull name)
{
  using namespace blender;
  VectorSet<StringRefNull> names = get_node_names(*this);
  std::string unique_name(name.c_str());
  unique_layer_group_name(names, unique_name.data());
  return group.add_group(unique_name);
}

blender::bke::greasepencil::LayerGroup &GreasePencil::add_layer_group_after(
    blender::bke::greasepencil::LayerGroup &group,
    blender::bke::greasepencil::TreeNode *node,
    const blender::StringRefNull name)
{
  using namespace blender;
  VectorSet<StringRefNull> names = get_node_names(*this);
  std::string unique_name(name.c_str());
  unique_layer_group_name(names, unique_name.data());
  return group.add_group_after(unique_name, node);
}

blender::bke::greasepencil::LayerGroup &GreasePencil::add_layer_group(
    const blender::StringRefNull name)
{
  return this->add_layer_group(this->root_group.wrap(), name);
}

const blender::bke::greasepencil::Layer *GreasePencil::find_layer_by_name(
    const blender::StringRefNull name) const
{
  return this->root_group.wrap().find_layer_by_name(name);
}

blender::bke::greasepencil::Layer *GreasePencil::find_layer_by_name(
    const blender::StringRefNull name)
{
  return this->root_group.wrap().find_layer_by_name(name);
}

const blender::bke::greasepencil::LayerGroup *GreasePencil::find_group_by_name(
    blender::StringRefNull name) const
{
  return this->root_group.wrap().find_group_by_name(name);
}

blender::bke::greasepencil::LayerGroup *GreasePencil::find_group_by_name(
    blender::StringRefNull name)
{
  return this->root_group.wrap().find_group_by_name(name);
}

void GreasePencil::rename_layer(blender::bke::greasepencil::Layer &layer,
                                blender::StringRefNull new_name)
{
  using namespace blender;
  if (layer.name() == new_name) {
    return;
  }
  VectorSet<StringRefNull> names = get_node_names(*this);
  std::string unique_name(new_name.c_str());
  unique_layer_name(names, unique_name.data());
  layer.set_name(unique_name);
}

void GreasePencil::rename_group(blender::bke::greasepencil::LayerGroup &group,
                                blender::StringRefNull new_name)
{
  using namespace blender;
  if (group.name() == new_name) {
    return;
  }
  VectorSet<StringRefNull> names = get_node_names(*this);
  std::string unique_name(new_name.c_str());
  unique_layer_group_name(names, unique_name.data());
  group.set_name(unique_name);
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

  /* Unlink the layer from the parent group. */
  layer.parent_group().unlink_layer(&layer);

  /* Remove drawings. */
  /* TODO: In the future this should only remove drawings when the user count hits zero. */
  for (GreasePencilFrame frame : layer.frames_for_write().values()) {
    this->remove_drawing(frame.drawing_index);
  }

  /* Delete the layer. */
  MEM_delete(&layer);
}

void GreasePencil::print_layer_tree()
{
  using namespace blender::bke::greasepencil;
  this->root_group.wrap().print_nodes("Layer Tree:");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Drawing array read/write functions
 * \{ */

void GreasePencil::read_drawing_array(BlendDataReader *reader)
{
  BLO_read_pointer_array(reader, reinterpret_cast<void **>(&this->drawing_array));
  for (int i = 0; i < this->drawing_array_num; i++) {
    BLO_read_data_address(reader, &this->drawing_array[i]);
    GreasePencilDrawingBase *drawing_base = this->drawing_array[i];
    switch (drawing_base->type) {
      case GP_DRAWING: {
        GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
        drawing->geometry.wrap().blend_read(*reader);
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

void GreasePencil::write_drawing_array(BlendWriter *writer)
{
  BLO_write_pointer_array(writer, this->drawing_array_num, this->drawing_array);
  for (int i = 0; i < this->drawing_array_num; i++) {
    GreasePencilDrawingBase *drawing_base = this->drawing_array[i];
    switch (drawing_base->type) {
      case GP_DRAWING: {
        GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
        BLO_write_struct(writer, GreasePencilDrawing, drawing);
        drawing->geometry.wrap().blend_write(*writer, this->id);
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

void GreasePencil::free_drawing_array()
{
  if (this->drawing_array == nullptr || this->drawing_array_num == 0) {
    return;
  }
  for (int i = 0; i < this->drawing_array_num; i++) {
    GreasePencilDrawingBase *drawing_base = this->drawing_array[i];
    switch (drawing_base->type) {
      case GP_DRAWING: {
        GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
        MEM_delete(&drawing->wrap());
        break;
      }
      case GP_DRAWING_REFERENCE: {
        GreasePencilDrawingReference *drawing_reference =
            reinterpret_cast<GreasePencilDrawingReference *>(drawing_base);
        MEM_freeN(drawing_reference);
        break;
      }
    }
  }
  MEM_freeN(this->drawing_array);
  this->drawing_array = nullptr;
  this->drawing_array_num = 0;
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

  /* Read frames storage. */
  BLO_read_int32_array(reader, node->frames_storage.num, &node->frames_storage.keys);
  BLO_read_data_address(reader, &node->frames_storage.values);

  /* Re-create frames data in runtime map. */
  node->wrap().runtime = MEM_new<blender::bke::greasepencil::LayerRuntime>(__func__);
  for (int i = 0; i < node->frames_storage.num; i++) {
    node->wrap().frames_for_write().add(node->frames_storage.keys[i],
                                        node->frames_storage.values[i]);
  }

  /* Read layer masks. */
  BLO_read_list(reader, &node->masks);
  LISTBASE_FOREACH (GreasePencilLayerMask *, mask, &node->masks) {
    BLO_read_data_address(reader, &mask->layer_name);
  }
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

void GreasePencil::read_layer_tree(BlendDataReader *reader)
{
  read_layer_tree_group(reader, &this->root_group, nullptr);
}

static void write_layer(BlendWriter *writer, GreasePencilLayer *node)
{
  using namespace blender::bke::greasepencil;

  BLO_write_struct(writer, GreasePencilLayer, node);
  BLO_write_string(writer, node->base.name);

  /* Re-create the frames storage only if it was tagged dirty. */
  if ((node->frames_storage.flag & GP_LAYER_FRAMES_STORAGE_DIRTY) != 0) {
    MEM_SAFE_FREE(node->frames_storage.keys);
    MEM_SAFE_FREE(node->frames_storage.values);

    const Layer &layer = node->wrap();
    node->frames_storage.num = layer.frames().size();
    node->frames_storage.keys = MEM_cnew_array<int>(node->frames_storage.num, __func__);
    node->frames_storage.values = MEM_cnew_array<GreasePencilFrame>(node->frames_storage.num,
                                                                    __func__);
    const Span<int> sorted_keys = layer.sorted_keys();
    for (const int i : sorted_keys.index_range()) {
      node->frames_storage.keys[i] = sorted_keys[i];
      node->frames_storage.values[i] = layer.frames().lookup(sorted_keys[i]);
    }

    /* Reset the flag. */
    node->frames_storage.flag &= ~GP_LAYER_FRAMES_STORAGE_DIRTY;
  }

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

void GreasePencil::write_layer_tree(BlendWriter *writer)
{
  write_layer_tree_group(writer, &this->root_group);
}

/** \} */
