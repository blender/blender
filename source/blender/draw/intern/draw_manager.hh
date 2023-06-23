/* SPDX-FileCopyrightText: 2022 Blender Foundation
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

#include "BLI_listbase_wrapper.hh"
#include "BLI_sys_types.h"
#include "GPU_material.h"

#include "draw_resource.hh"
#include "draw_view.hh"

#include <string>

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
   * TODO(@fclem): Remove once we get rid of old EEVEE code-base.
   * Only here to satisfy bindings.
   */
  ObjectAttributeLegacyBuf attributes_buf_legacy;

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
  Vector<GPUTexture *> acquired_textures;

 private:
  /** Number of resource handle recorded. */
  uint resource_len_ = 0;
  /** Number of object attribute recorded. */
  uint attribute_len_ = 0;

  Object *object_active = nullptr;

 public:
  Manager(){};
  ~Manager();

  /**
   * Create a new resource handle for the given object.
   */
  ResourceHandle resource_handle(const ObjectRef ref);
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
   * Populate additional per resource data on demand.
   */
  void extract_object_attributes(ResourceHandle handle,
                                 const ObjectRef &ref,
                                 Span<GPUMaterial *> materials);

  /**
   * Collect necessary View Layer attributes.
   */
  void register_layer_attributes(GPUMaterial *material);

  /**
   * Submit a pass for drawing. All resource reference will be dereferenced and commands will be
   * sent to GPU.
   */
  void submit(PassSimple &pass, View &view);
  void submit(PassMain &pass, View &view);
  void submit(PassSortable &pass, View &view);
  /**
   * Variant without any view. Must not contain any shader using `draw_view` create info.
   */
  void submit(PassSimple &pass);

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
  void acquire_texture(GPUTexture *texture)
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
  void begin_sync();
  void end_sync();

  void debug_bind();
  void resource_bind();

 private:
  void sync_layer_attributes();
};

inline ResourceHandle Manager::resource_handle(const ObjectRef ref)
{
  bool is_active_object = (ref.dupli_object ? ref.dupli_parent : ref.object) == object_active;
  matrix_buf.current().get_or_resize(resource_len_).sync(*ref.object);
  bounds_buf.current().get_or_resize(resource_len_).sync(*ref.object);
  infos_buf.current().get_or_resize(resource_len_).sync(ref, is_active_object);
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
blender::draw::ObjectRef DRW_object_ref_get(Object *object);
