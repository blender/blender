/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

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

#include "BLI_sys_types.h"

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
  ObjectMatricesBuf matrix_buf;
  ObjectBoundsBuf bounds_buf;
  ObjectInfosBuf infos_buf;

  /** List of textures coming from Image data-blocks. They need to be refcounted in order to avoid
   * beeing freed in another thread. */
  Vector<GPUTexture *> acquired_textures;

 private:
  uint resource_len_ = 0;
  Object *object = nullptr;

  Object *object_active = nullptr;

 public:
  Manager(){};
  ~Manager();

  /**
   * Create a new resource handle for the given object. Can be called multiple time with the
   * same object **successively** without duplicating the data.
   */
  ResourceHandle resource_handle(const ObjectRef ref);
  /**
   * Get resource id for a loose matrix. The draw-calls for this resource handle won't be culled
   * and there won't be any associated object info / bounds. Assumes correct handedness / winding.
   */
  ResourceHandle resource_handle(const float4x4 &model_matrix);
  /**
   * Get resource id for a loose matrix with bounds. The draw-calls for this resource handle will
   * be culled bute there won't be any associated object info / bounds. Assumes correct handedness
   * / winding.
   */
  ResourceHandle resource_handle(const float4x4 &model_matrix,
                                 const float3 &bounds_center,
                                 const float3 &bounds_half_extent);

  /**
   * Populate additional per resource data on demand.
   */
  void extract_object_attributes(ResourceHandle handle,
                                 Object &object,
                                 Span<GPUMaterial *> materials);

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

  /** TODO(fclem): The following should become private at some point. */
  void begin_sync();
  void end_sync();

  void debug_bind();
};

inline ResourceHandle Manager::resource_handle(const ObjectRef ref)
{
  bool is_active_object = (ref.dupli_object ? ref.dupli_parent : ref.object) == object_active;
  matrix_buf.get_or_resize(resource_len_).sync(*ref.object);
  bounds_buf.get_or_resize(resource_len_).sync(*ref.object);
  infos_buf.get_or_resize(resource_len_).sync(ref, is_active_object);
  return ResourceHandle(resource_len_++, (ref.object->transflag & OB_NEG_SCALE) != 0);
}

inline ResourceHandle Manager::resource_handle(const float4x4 &model_matrix)
{
  matrix_buf.get_or_resize(resource_len_).sync(model_matrix);
  bounds_buf.get_or_resize(resource_len_).sync();
  infos_buf.get_or_resize(resource_len_).sync();
  return ResourceHandle(resource_len_++, false);
}

inline ResourceHandle Manager::resource_handle(const float4x4 &model_matrix,
                                               const float3 &bounds_center,
                                               const float3 &bounds_half_extent)
{
  matrix_buf.get_or_resize(resource_len_).sync(model_matrix);
  bounds_buf.get_or_resize(resource_len_).sync(bounds_center, bounds_half_extent);
  infos_buf.get_or_resize(resource_len_).sync();
  return ResourceHandle(resource_len_++, false);
}

inline void Manager::extract_object_attributes(ResourceHandle handle,
                                               Object &object,
                                               Span<GPUMaterial *> materials)
{
  /* TODO */
  (void)handle;
  (void)object;
  (void)materials;
}

}  // namespace blender::draw

/* TODO(@fclem): This is for testing. The manager should be passed to the engine through the
 * callbacks. */
blender::draw::Manager *DRW_manager_get();
blender::draw::ObjectRef DRW_object_ref_get(Object *object);
