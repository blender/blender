/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup draw
 *
 * `draw::Manager` is the interface between scene data and viewport engines.
 *
 * It holds per component data (`ObjectInfo`, `ObjectMatrices`, ...) indexed per `ResourceHandle`.
 *
 * \note It is currently work in progress and should replace the old global draw manager.
 */

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_sys_types.h"

#include "GPU_material.hh"

#include "draw_resource.hh"
#include "draw_view.hh"

#include <atomic>

namespace blender::draw {

/* Forward declarations. */

namespace detail {
template<typename T> class Pass;
}  // namespace detail

namespace command {
class DrawCommandBuf;
class DrawMultiBuf;
}  // namespace command

using PassSimple = detail::Pass<command::DrawCommandBuf>;
using PassMain = detail::Pass<command::DrawMultiBuf>;
class PassSortable;

class Manager {
  using ObjectMatricesBuf = StorageArrayBuffer<ObjectMatrices, 128>;
  using ObjectBoundsBuf = StorageArrayBuffer<ObjectBounds, 128>;
  using ObjectInfosBuf = StorageArrayBuffer<ObjectInfos, 128>;
  using ObjectAttributeBuf = StorageArrayBuffer<ObjectAttribute, 128>;
  using LayerAttributeBuf = UniformArrayBuffer<LayerAttribute, 512>;
  /**
   * TODO(@fclem): Remove once we get rid of old EEVEE code-base.
   * `DRW_RESOURCE_CHUNK_LEN = 512`.
   */
  using ObjectAttributeLegacyBuf = UniformArrayBuffer<float4, 8 * 512>;

 public:
  struct SubmitDebugOutput {
    /** Indexed by resource id. */
    Span<uint32_t> visibility;
    /** Indexed by drawn instance. */
    Span<uint32_t> resource_id;
  };

  struct DataDebugOutput {
    /** Indexed by resource id. */
    Span<ObjectMatrices> matrices;
    /** Indexed by resource id. */
    Span<ObjectBounds> bounds;
    /** Indexed by resource id. */
    Span<ObjectInfos> infos;
  };

  /**
   * Buffers containing all object data. Referenced by resource index.
   * Exposed as public members for shader access after sync.
   */
  SwapChain<ObjectMatricesBuf, 2> matrix_buf;
  SwapChain<ObjectBoundsBuf, 2> bounds_buf;
  SwapChain<ObjectInfosBuf, 2> infos_buf;

  /**
   * Object Attributes are reference by indirection data inside ObjectInfos.
   * This is because attribute list is arbitrary.
   */
  ObjectAttributeBuf attributes_buf;

  /**
   * Table of all View Layer attributes required by shaders, used to populate the buffer below.
   */
  Map<uint32_t, GPULayerAttr> layer_attributes;

  /**
   * Buffer of layer attribute values, indexed and sorted by the hash.
   */
  LayerAttributeBuf layer_attributes_buf;

  /**
   * List of textures coming from Image data-blocks.
   * They need to be reference-counted in order to avoid being freed in another thread.
   */
  Vector<gpu::Texture *> acquired_textures;

 private:
  /** Number of sync done by managers. Used for fingerprint. */
  static std::atomic<uint32_t> global_sync_counter_;

  /* Local sync counter. Used for fingerprint. Must never be null. */
  uint32_t sync_counter_ = 1;

  /** Number of resource handle recorded. */
  uint resource_len_ = 0;
  /** Number of object attribute recorded. */
  uint attribute_len_ = 0;

  Object *object_active = nullptr;

 public:
  Manager() {};
  ~Manager();

  /**
   * Create a unique resource handle for the given object.
   * Returns the existing handle if it exists.
   */
  /* WORKAROUND: Instead of breaking const correctness everywhere, we only break it for this. */
  ResourceHandleRange unique_handle(const ObjectRef &ref);

  ResourceHandleRange unique_handle_for_sculpt(const ObjectRef &ref);

  /**
   * Create a new resource handle for the given object.
   */
  ResourceHandleRange resource_handle(const ObjectRef &ref, float inflate_bounds = 0.0f);
  /**
   * Create a new resource handle for the given object, but optionally override model matrix and
   * bounds.
   */
  ResourceHandleRange resource_handle(const ObjectRef &ref,
                                      const float4x4 *model_matrix,
                                      const float3 *bounds_center,
                                      const float3 *bounds_half_extent);
  /**
   * Get resource id for a loose matrix. The draw-calls for this resource handle won't be culled
   * and there won't be any associated object info / bounds. Assumes correct handedness / winding.
   */
  ResourceHandle resource_handle(const float4x4 &model_matrix);
  /**
   * Get resource id for a loose matrix with bounds. The draw-calls for this resource handle will
   * be culled but there won't be any associated object info / bounds.
   * Assumes correct handedness / winding.
   */
  ResourceHandle resource_handle(const float4x4 &model_matrix,
                                 const float3 &bounds_center,
                                 const float3 &bounds_half_extent);
  /**
   * Get resource id for particle system. The draw-calls for this resource won't be culled. The
   * associated object info will contain the info from its parent object.
   */
  ResourceHandle resource_handle_for_psys(const ObjectRef &ref, const float4x4 &model_matrix);

  /** Update the bounds of an already created handle. */
  void update_handle_bounds(ResourceHandle handle,
                            const ObjectRef &ref,
                            float inflate_bounds = 0.0f);

  /**
   * Populate additional per resource data on demand.
   * IMPORTANT: Should be called only **once** per object.
   */
  void extract_object_attributes(ResourceHandle handle,
                                 const ObjectRef &ref,
                                 const GPUMaterial *material);
  void extract_object_attributes(ResourceHandle handle,
                                 const ObjectRef &ref,
                                 Span<GPUMaterial *> materials);

  /**
   * Collect necessary View Layer attributes.
   */
  void register_layer_attributes(GPUMaterial *material);

  /**
   * Compute <-> Graphic queue transition is quite slow on some backend. To avoid unnecessary
   * switching, it is better to dispatch all visibility computation as soon as possible before any
   * graphic work.
   *
   * Grouping the calls to `compute_visibility()` together is also beneficial for PSO switching
   * overhead. Same thing applies to `generate_commands()`.
   *
   * IMPORTANT: Generated commands are stored inside #PassMain and overrides commands generated for
   * a previous view.
   *
   * Before:
   * \code{.cpp}
   * manager.submit(pass1, view1);
   * manager.submit(pass2, view1);
   * manager.submit(pass1, view2);
   * manager.submit(pass2, view2);
   * \endcode
   *
   * After:
   * \code{.cpp}
   * manager.compute_visibility(view1);
   * manager.compute_visibility(view2);
   *
   * manager.generate_commands(pass1, view1);
   * manager.generate_commands(pass2, view1);
   *
   * manager.submit(pass1, view1);
   * manager.submit(pass2, view1);
   *
   * manager.generate_commands(pass1, view2);
   * manager.generate_commands(pass2, view2);
   *
   * manager.submit(pass1, view2);
   * manager.submit(pass2, view2);
   * \endcode
   */

  /**
   * Compute visibility of #ResourceHandle for the given #View.
   * The commands needs to be regenerated for any change inside the #Manager or in the #View.
   * Avoids just in time computation of visibility.
   */
  void compute_visibility(View &view);
  /**
   * Same as compute_visibility but only do it if needed.
   */
  void ensure_visibility(View &view);
  /**
   * Generate commands for #ResourceHandle for the given #View and #PassMain.
   * The commands needs to be regenerated for any change inside the #Manager, the #PassMain or in
   * the #View. Avoids just in time command generation.
   *
   * IMPORTANT: Generated commands are stored inside #PassMain and overrides commands previously
   * generated for a previous view.
   */
  void generate_commands(PassMain &pass, View &view);
  void generate_commands(PassSortable &pass, View &view);
  /**
   * Generate commands on CPU. Doesn't have the GPU compute dispatch overhead.
   */
  void generate_commands(PassSimple &pass);

  /**
   * Make sure the shader specialization constants are already compiled.
   * This avoid stalling the real submission call because of specialization.
   */
  void warm_shader_specialization(PassMain &pass);
  void warm_shader_specialization(PassSimple &pass);

  /**
   * Submit a pass for drawing. All resource reference will be dereferenced and commands will be
   * sent to GPU. Visibility and command generation **must** have already been done explicitly
   * using `compute_visibility` and `generate_commands`.
   */
  void submit_only(PassMain &pass, View &view);
  /**
   * Submit a pass for drawing. All resource reference will be dereferenced and commands will be
   * sent to GPU. Visibility and command generation are run JIT if needed.
   */
  void submit(PassSimple &pass, View &view);
  void submit(PassMain &pass, View &view);
  void submit(PassSortable &pass, View &view);
  /**
   * Variant without any view. Must not contain any shader using `draw_view` create info.
   */
  void submit(PassSimple &pass, bool inverted_view = false);

  /**
   * Submit a pass for drawing but read back all data buffers for inspection.
   */
  SubmitDebugOutput submit_debug(PassSimple &pass, View &view);
  SubmitDebugOutput submit_debug(PassMain &pass, View &view);

  /**
   * Check data buffers of the draw manager. Only to be used after end_sync().
   */
  DataDebugOutput data_debug();

  /**
   * Will acquire the texture using ref counting and release it after drawing. To be used for
   * texture coming from blender Image.
   */
  void acquire_texture(gpu::Texture *texture)
  {
    GPU_texture_ref(texture);
    acquired_textures.append(texture);
  }

  /**
   * Return the number of resource handles allocated.
   */
  uint resource_handle_count() const
  {
    return resource_len_;
  }

  /** TODO(fclem): The following should become private at some point. */
  void begin_sync(Object *object_active = nullptr);
  void end_sync();

  void debug_bind();
  void resource_bind();

 private:
  void sync_layer_attributes();

  /* Fingerprint of the manager in a certain state. Assured to not be 0.
   * Not reliable enough for general update detection. Only to be used for debugging assertion. */
  uint64_t fingerprint_get();
};

inline ResourceHandleRange Manager::unique_handle(const ObjectRef &ref)
{
  if (!ref.handle_.is_valid()) {
    /* WORKAROUND: Instead of breaking const correctness everywhere, we only break it for this. */
    const_cast<ObjectRef &>(ref).handle_ = resource_handle(ref);
  }
  return ref.handle_;
}

inline ResourceHandleRange Manager::resource_handle(const ObjectRef &ref, float inflate_bounds)
{
  bool is_active_object = ref.is_active(object_active);
  bool is_active_edit_mode = object_active &&
                             (DRW_object_is_in_edit_mode(object_active) ||
                              ELEM(object_active->mode, OB_MODE_TEXTURE_PAINT, OB_MODE_SCULPT)) &&
                             ref.object->mode == object_active->mode;
  if (ref.duplis_) {
    uint start = resource_len_;

    ObjectBounds proto_bounds;
    proto_bounds.sync(*ref.object, inflate_bounds);

    ObjectInfos proto_info;
    proto_info.sync(ref, is_active_object, is_active_edit_mode);

    for (const DupliObject *dupli : *ref.duplis_) {
      matrix_buf.current().get_or_resize(resource_len_).sync(float4x4(dupli->mat));
      bounds_buf.current().get_or_resize(resource_len_) = proto_bounds;

      ObjectInfos &info = infos_buf.current().get_or_resize(resource_len_);
      info = proto_info;
      info.random = dupli->random_id * (1.0f / (float)0xFFFFFFFF);

      resource_len_++;
    }
    return ResourceHandleRange(ResourceHandle(start, (ref.object->transflag & OB_NEG_SCALE) != 0),
                               resource_len_ - start);
  }
  else {
    matrix_buf.current().get_or_resize(resource_len_).sync(*ref.object);
    bounds_buf.current().get_or_resize(resource_len_).sync(*ref.object, inflate_bounds);
    infos_buf.current()
        .get_or_resize(resource_len_)
        .sync(ref, is_active_object, is_active_edit_mode);
    return ResourceHandle(resource_len_++, (ref.object->transflag & OB_NEG_SCALE) != 0);
  }
}

inline ResourceHandleRange Manager::resource_handle(const ObjectRef &ref,
                                                    const float4x4 *model_matrix,
                                                    const float3 *bounds_center,
                                                    const float3 *bounds_half_extent)
{
  BLI_assert(!ref.duplis_);
  bool is_active_object = ref.is_active(object_active);
  bool is_active_edit_mode = object_active &&
                             (DRW_object_is_in_edit_mode(object_active) ||
                              ELEM(object_active->mode, OB_MODE_TEXTURE_PAINT, OB_MODE_SCULPT)) &&
                             ref.object->mode == object_active->mode;
  if (model_matrix) {
    matrix_buf.current().get_or_resize(resource_len_).sync(*model_matrix);
  }
  else {
    matrix_buf.current().get_or_resize(resource_len_).sync(*ref.object);
  }
  if (bounds_center && bounds_half_extent) {
    bounds_buf.current().get_or_resize(resource_len_).sync(*bounds_center, *bounds_half_extent);
  }
  else {
    bounds_buf.current().get_or_resize(resource_len_).sync(*ref.object);
  }
  infos_buf.current()
      .get_or_resize(resource_len_)
      .sync(ref, is_active_object, is_active_edit_mode);
  return ResourceHandle(resource_len_++, (ref.object->transflag & OB_NEG_SCALE) != 0);
}

inline ResourceHandle Manager::resource_handle(const float4x4 &model_matrix)
{
  matrix_buf.current().get_or_resize(resource_len_).sync(model_matrix);
  bounds_buf.current().get_or_resize(resource_len_).sync();
  infos_buf.current().get_or_resize(resource_len_).sync();
  return ResourceHandle(resource_len_++, false);
}

inline ResourceHandle Manager::resource_handle(const float4x4 &model_matrix,
                                               const float3 &bounds_center,
                                               const float3 &bounds_half_extent)
{
  matrix_buf.current().get_or_resize(resource_len_).sync(model_matrix);
  bounds_buf.current().get_or_resize(resource_len_).sync(bounds_center, bounds_half_extent);
  infos_buf.current().get_or_resize(resource_len_).sync();
  return ResourceHandle(resource_len_++, false);
}

inline ResourceHandle Manager::resource_handle_for_psys(const ObjectRef &ref,
                                                        const float4x4 &model_matrix)
{
  BLI_assert(!ref.duplis_);
  bool is_active_object = ref.is_active(object_active);
  bool is_active_edit_mode = object_active &&
                             (DRW_object_is_in_edit_mode(object_active) ||
                              ELEM(object_active->mode, OB_MODE_TEXTURE_PAINT, OB_MODE_SCULPT)) &&
                             ref.object->mode == object_active->mode;
  matrix_buf.current().get_or_resize(resource_len_).sync(model_matrix);
  bounds_buf.current().get_or_resize(resource_len_).sync();
  infos_buf.current()
      .get_or_resize(resource_len_)
      .sync(ref, is_active_object, is_active_edit_mode);
  return ResourceHandle(resource_len_++, (ref.object->transflag & OB_NEG_SCALE) != 0);
}

inline void Manager::update_handle_bounds(ResourceHandle handle,
                                          const ObjectRef &ref,
                                          float inflate_bounds)
{
  bounds_buf.current()[handle.resource_index()].sync(*ref.object, inflate_bounds);
}

inline void Manager::extract_object_attributes(ResourceHandle handle,
                                               const ObjectRef &ref,
                                               const GPUMaterial *material)
{
  ObjectInfos &infos = infos_buf.current().get_or_resize(handle.resource_index());
  infos.object_attrs_offset = attribute_len_;

  const GPUUniformAttrList *attr_list = GPU_material_uniform_attributes(material);
  if (attr_list == nullptr) {
    return;
  }

  LISTBASE_FOREACH (const GPUUniformAttr *, attr, &attr_list->list) {
    if (attributes_buf.get_or_resize(attribute_len_).sync(ref, *attr)) {
      infos.object_attrs_len++;
      attribute_len_++;
    }
  }
}

inline void Manager::extract_object_attributes(ResourceHandle handle,
                                               const ObjectRef &ref,
                                               Span<GPUMaterial *> materials)
{
  ObjectInfos &infos = infos_buf.current().get_or_resize(handle.resource_index());
  infos.object_attrs_offset = attribute_len_;

  /* Simple cache solution to avoid duplicates. */
  Vector<uint32_t, 4> hash_cache;

  for (const GPUMaterial *mat : materials) {
    const GPUUniformAttrList *attr_list = GPU_material_uniform_attributes(mat);
    if (attr_list == nullptr) {
      continue;
    }

    LISTBASE_FOREACH (const GPUUniformAttr *, attr, &attr_list->list) {
      /** WATCH: Linear Search. Avoid duplicate attributes across materials. */
      if ((mat != materials.first()) && (hash_cache.first_index_of_try(attr->hash_code) != -1)) {
        /* Attribute has already been added to the attribute buffer by another material. */
        continue;
      }
      hash_cache.append(attr->hash_code);
      if (attributes_buf.get_or_resize(attribute_len_).sync(ref, *attr)) {
        infos.object_attrs_len++;
        attribute_len_++;
      }
    }
  }
}

inline void Manager::register_layer_attributes(GPUMaterial *material)
{
  const ListBase *attr_list = GPU_material_layer_attributes(material);

  if (attr_list != nullptr) {
    LISTBASE_FOREACH (const GPULayerAttr *, attr, attr_list) {
      /** Since layer attributes are global to the whole render pass,
       *  this only collects a table of their names. */
      layer_attributes.add(attr->hash_code, *attr);
    }
  }
}

}  // namespace blender::draw

/* TODO(@fclem): This is for testing. The manager should be passed to the engine through the
 * callbacks. */
blender::draw::Manager *DRW_manager_get();
