/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * The velocity pass outputs motion vectors to use for either
 * temporal re-projection or motion blur.
 *
 * It is the module that tracks the objects data between frames updates.
 */

#pragma once

#include "BLI_map.hh"

#include "eevee_shader_shared.hh"
#include "eevee_sync.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name VelocityModule
 *
 * \{ */

/** Container for scene velocity data. */
class VelocityModule {
  friend class VelocityView;

 public:
  struct VelocityObjectData : public VelocityIndex {
    /** ID to retrieve the corresponding #VelocityGeometryData after copy. */
    ID *id;
  };
  struct VelocityGeometryData {
    /** VertBuf not yet ready to be copied to the #VelocityGeometryBuf. */
    GPUVertBuf *pos_buf = nullptr;
    /* Offset in the #VelocityGeometryBuf to the start of the data. In vertex. */
    int ofs;
    /* Length of the vertex buffer. In vertex. */
    int len;
  };
  /**
   * The map contains indirection indices to the obmat and geometry in each step buffer.
   * Note that each object component gets its own resource id so one component correspond to one
   * geometry offset.
   */
  Map<ObjectKey, VelocityObjectData> velocity_map;
  /** Geometry to be copied to VelocityGeometryBuf. Indexed by evaluated ID *. Empty after */
  Map<ID *, VelocityGeometryData> geometry_map;
  /** Contains all objects matrices for each time step. */
  std::array<VelocityObjectBuf *, 3> object_steps;
  /** Contains all Geometry steps from deforming objects for each time step. */
  std::array<VelocityGeometryBuf *, 3> geometry_steps;
  /** Number of occupied slot in each `object_steps`. */
  int3 object_steps_usage = int3(0);
  /** Buffer of all #VelocityIndex used in this frame. Indexed by draw manager resource id. */
  VelocityIndexBuf indirection_buf;

  /**
   * Copies of camera data. One for previous and one for next time step.
   */
  std::array<CameraDataBuf *, 3> camera_steps;

 private:
  Instance &inst_;

  eVelocityStep step_ = STEP_CURRENT;

  DRWPass *resolve_ps_ = nullptr;

  /** Reference only. Not owned. */
  GPUTexture *input_depth_tx_;
  GPUTexture *velocity_view_tx_;
  GPUTexture *velocity_camera_tx_;

  int3 resolve_dispatch_size_ = int3(1, 1, 1);

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
  bool step_object_sync(Object *ob, ObjectKey &ob_key, int recalc = 0);

  /* Moves next frame data to previous frame data. Nullify next frame data. */
  void step_swap();

  void begin_sync();
  void end_sync();

  void bind_resources(DRWShadingGroup *grp);

 private:
  bool object_has_velocity(const Object *ob);
  bool object_is_deform(const Object *ob);

  void resolve_camera_motion(GPUTexture *depth_tx,
                             GPUTexture *velocity_view_tx,
                             GPUTexture *velocity_camera_tx);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Velocity
 *
 * \{ */

/**
 * Per view module.
 */
class VelocityView {
 private:
  Instance &inst_;

  StringRefNull view_name_;

  TextureFromPool velocity_camera_tx_ = {"velocity_camera_tx_"};
  TextureFromPool velocity_view_tx_ = {"velocity_view_tx_"};

 public:
  VelocityView(Instance &inst, const char *name) : inst_(inst), view_name_(name){};
  ~VelocityView(){};

  void sync();

  void acquire(int2 extent);
  void release();

  void resolve(GPUTexture *depth_tx);

  /**
   * Getters
   **/
  GPUTexture *view_vectors_get() const
  {
    return velocity_view_tx_;
  }
  GPUTexture *camera_vectors_get() const
  {
    return (velocity_camera_tx_.is_valid()) ? velocity_camera_tx_ : velocity_view_tx_;
  }
};

/** \} */

}  // namespace blender::eevee
