/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * The velocity pass outputs motion vectors to use for either
 * temporal re-projection or motion blur.
 *
 * It is the module that tracks the objects data between frames updates.
 */

#pragma once

#include <variant>

#include "BLI_map.hh"

#include "DRW_gpu_wrapper.hh"

#include "GPU_batch.hh"

#include "eevee_camera_shared.hh"
#include "eevee_sync.hh"
#include "eevee_velocity_shared.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name VelocityModule
 *
 * \{ */

using CameraDataBuf = draw::UniformBuffer<CameraData>;
using VelocityGeometryBuf = draw::StorageArrayBuffer<float4, 16, true>;
using VelocityIndexBuf = draw::StorageArrayBuffer<VelocityIndex, 16>;
using VelocityObjectBuf = draw::StorageArrayBuffer<float4x4, 16>;

/** Container for scene velocity data. */
class VelocityModule {
 public:
  struct VelocityObjectData : public VelocityIndex {
    /** ID key to retrieve the corresponding #VelocityGeometryData after copy. */
    uint64_t id;
  };
  struct VelocityGeometryData {
    /** VertBuf not yet ready to be copied to the #VelocityGeometryBuf. */
    std::variant<std::monostate, gpu::Batch *, gpu::VertBuf *> pos_buf;
    /* Offset in the #VelocityGeometryBuf to the start of the data. In vertex. */
    int ofs = 0;
    /* Length of the vertex buffer. In vertex. */
    int len = 0;

    gpu::VertBuf *pos_buf_get() const
    {
      if (std::holds_alternative<gpu::VertBuf *>(this->pos_buf)) {
        return std::get<gpu::VertBuf *>(this->pos_buf);
      }
      if (std::holds_alternative<gpu::Batch *>(this->pos_buf)) {
        gpu::VertBuf *buf = std::get<gpu::Batch *>(this->pos_buf)->verts_(1);
        if (!buf) {
          return nullptr;
        }
        BLI_assert(STREQ(buf->format.names, "pos"));
        return buf;
      }
      return nullptr;
    }

    /* Returns true if the data is or **will** be available after the end of sync. */
    bool has_data() const
    {
      if (std::holds_alternative<gpu::VertBuf *>(this->pos_buf)) {
        return true;
      }
      if (std::holds_alternative<gpu::Batch *>(this->pos_buf)) {
        return true;
      }
      return false;
    }
  };
  /**
   * The map contains indirection indices to the obmat and geometry in each step buffer.
   * Note that each object component gets its own resource id so one component correspond to one
   * geometry offset.
   */
  Map<ObjectKey, VelocityObjectData> velocity_map;
  /** Geometry to be copied to VelocityGeometryBuf. Indexed by evaluated ID hash. Empty after */
  Map<uint64_t, VelocityGeometryData> geometry_map;
  /** Contains all objects matrices for each time step. */
  std::array<VelocityObjectBuf *, 3> object_steps;
  /** Contains all Geometry steps from deforming objects for each time step. */
  std::array<VelocityGeometryBuf *, 3> geometry_steps;
  /** Number of occupied slot in each `object_steps`. */
  int3 object_steps_usage = int3(0);
  /** Buffer of all #VelocityIndex used in this frame. Indexed by draw manager resource id. */
  VelocityIndexBuf indirection_buf;
  /** Frame time at which each steps were evaluated. */
  float3 step_time;

  /**
   * Copies of camera data. One for previous and one for next time step.
   */
  std::array<CameraDataBuf *, 3> camera_steps;

 private:
  Instance &inst_;

  /** Step being synced. */
  eVelocityStep step_ = STEP_CURRENT;
  /** Step referenced as next step. */
  eVelocityStep next_step_ = STEP_NEXT;

 public:
  VelocityModule(Instance &inst) : inst_(inst)
  {
    for (VelocityObjectBuf *&step_buf : object_steps) {
      step_buf = new VelocityObjectBuf();
    }
    for (VelocityGeometryBuf *&step_buf : geometry_steps) {
      step_buf = new VelocityGeometryBuf();
    }
    for (CameraDataBuf *&step_buf : camera_steps) {
      step_buf = new CameraDataBuf();
    }
  };

  ~VelocityModule()
  {
    for (VelocityObjectBuf *step_buf : object_steps) {
      delete step_buf;
    }
    for (VelocityGeometryBuf *step_buf : geometry_steps) {
      delete step_buf;
    }
    for (CameraDataBuf *step_buf : camera_steps) {
      delete step_buf;
    }
  }

  void init();

  void step_camera_sync();
  void step_sync(eVelocityStep step, float time);

  /* Gather motion data. Returns true if the object **can** have motion. */
  bool step_object_sync(ObjectKey &object_key,
                        const ObjectRef &object_ref,
                        int recalc,
                        ResourceHandleRange resource_handle,
                        ModifierData *modifier_data = nullptr,
                        ParticleSystem *particle_sys = nullptr);

  /**
   * In Render, moves the next frame data to previous frame data. Nullify next frame data.
   * In Viewport, the current frame data will be used as previous frame data in the next frame.
   */
  void step_swap();

  void begin_sync();
  /** This is the end of the current frame sync. Not the step_sync. */
  void end_sync();

  template<typename PassType> void bind_resources(PassType &pass)
  {
    /* Storage Buffer. */
    pass.bind_ssbo(VELOCITY_OBJ_PREV_BUF_SLOT, &(*object_steps[STEP_PREVIOUS]));
    pass.bind_ssbo(VELOCITY_OBJ_NEXT_BUF_SLOT, &(*object_steps[next_step_]));
    pass.bind_ssbo(VELOCITY_GEO_PREV_BUF_SLOT, &(*geometry_steps[STEP_PREVIOUS]));
    pass.bind_ssbo(VELOCITY_GEO_NEXT_BUF_SLOT, &(*geometry_steps[next_step_]));
    pass.bind_ssbo(VELOCITY_INDIRECTION_BUF_SLOT, &indirection_buf);
    /* Uniform Buffer. */
    pass.bind_ubo(VELOCITY_CAMERA_PREV_BUF, &(*camera_steps[STEP_PREVIOUS]));
    pass.bind_ubo(VELOCITY_CAMERA_CURR_BUF, &(*camera_steps[STEP_CURRENT]));
    pass.bind_ubo(VELOCITY_CAMERA_NEXT_BUF, &(*camera_steps[next_step_]));
  }

  bool camera_has_motion() const;
  bool camera_changed_projection() const;

  /* Returns frame time difference between two steps. */
  float step_time_delta_get(eVelocityStep start, eVelocityStep end) const;

  /* Perform VelocityGeometryData offset computation and copy into the geometry step buffer.
   * Should be called after all the vertex buffers have been updated by batch cache extraction. */
  void geometry_steps_fill();

 private:
  bool object_has_velocity(const Object *ob);
  bool object_is_deform(const Object *ob);
};

/** \} */

}  // namespace blender::eevee
