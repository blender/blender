/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * World rendering with material handling. Also take care of lookdev
 * HDRI and default material.
 */

#pragma once

#include <string>

#include "DNA_image_types.h"
#include "DNA_vec_types.h"
#include "DNA_world_types.h"

#include "BLI_math_vector_types.hh"

#include "DRW_gpu_wrapper.hh"

#include "eevee_light_shared.hh"
#include "eevee_lightprobe.hh"
#include "eevee_lightprobe_shared.hh"

#include "draw_pass.hh"

struct bNode;
struct bNodeSocketValueFloat;
struct bNodeSocketValueVector;
struct View3D;

namespace blender::eevee {

class Instance;
class LookdevView;

using blender::draw::Framebuffer;
using blender::draw::PassSimple;
using blender::draw::ResourceHandleRange;
using blender::draw::Texture;
using blender::draw::View;

/* -------------------------------------------------------------------- */
/** \name Parameters
 *
 * Parameters used to check changes and to configure the world shader node tree.
 *
 * \{ */
struct LookdevParameters {
  std::string hdri;
  float rot_z = 0.0f;
  float background_opacity = 0.0f;
  float intensity = 1.0f;
  float blur = 0.0f;
  bool show_scene_world = true;
  bool camera_space = true;

  LookdevParameters();
  LookdevParameters(const ::View3D *v3d);
  bool operator==(const LookdevParameters &other) const;
  bool operator!=(const LookdevParameters &other) const;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport Override World
 *
 * In a viewport the world can be overridden by a custom HDRI and some settings.
 * \{ */

class LookdevWorld {
 private:
  bNode *environment_node_ = nullptr;
  bNodeSocketValueFloat *intensity_socket_ = nullptr;
  bNodeSocketValueFloat *angle_socket_ = nullptr;
  /* Vector multiply socket for flipping Y axes when transforming to camera space. */
  bNodeSocketValueVector *flip_y_socket_ = nullptr;
  /* Vector transform socket `convert_to`. */
  int *xform_socket_ = nullptr;
  /* Set to M_PI/2 for rotating the HDRI horizon line in camera space mode. */
  float *rotation_x_socket_ = nullptr;
  ::Image *image = nullptr;
  ::World *world = nullptr;

  LookdevParameters parameters_;

 public:
  LookdevWorld();
  ~LookdevWorld();

  /* Returns true if an update was detected. */
  bool sync(const LookdevParameters &new_parameters);

  ::World *world_get()
  {
    return world;
  }

  float background_opacity_get() const
  {
    return parameters_.background_opacity;
  }

  float background_blur_get() const
  {
    return parameters_.blur;
  }

  float intensity_get() const
  {
    return parameters_.intensity;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lookdev
 *
 * \{ */

using namespace draw;

class LookdevModule {
 private:
  Instance &inst_;

  bool use_reference_spheres_ = false;

  bool use_viewspace_lighting_ = false;
  /* Used for update detection. */
  float4x4 last_rotation_matrix_ = float4x4::identity();
  float studio_light_rotation_z_ = 0.0f;

  static constexpr int num_spheres = 2;

  /**
   * Shape resolution level of detail.
   */
  enum SphereLOD {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,

    MAX, /* Max number of level of detail */
  };

  std::array<gpu::Batch *, MAX> sphere_lod_ = {};

  /* Size and position of the look-dev spheres in world space. */
  float sphere_radius_;
  float3 sphere_position_;

  rcti visible_rect_;

  /* Dummy textures: required to reuse forward mesh shader and avoid another shader variation. */
  Texture dummy_cryptomatte_tx_;
  Texture dummy_aov_color_tx_;
  Texture dummy_aov_value_tx_;

  struct Sphere {
    Framebuffer framebuffer = {"Lookdev.Framebuffer"};
    Texture color_tx_ = {"Lookdev.Color"};
    PassSimple pass = {"Lookdev.Sphere"};
  };

  Sphere spheres_[num_spheres];
  PassSimple display_ps_ = {"Lookdev.Display"};

  /**
   * Copy of non-rotated world probe values when using the "view space" light overlay option.
   * These probes are then rotated to match the view direction.
   * This is faster than trying to recompute all these values for each frame.
   */
  Texture world_sphere_probe_ = {"world_sphere_probe_"};
  StorageBuffer<SphereProbeHarmonic, true> world_volume_probe_ = {"world_volume_probe_"};
  UniformArrayBuffer<LightData, 2> world_sunlight_ = {"world_sunlight_"};

 public:
  LookdevModule(Instance &inst);
  ~LookdevModule();

  void init(const rcti *visible_rect);
  void sync();

  void draw(View &view);

  void display();

  void rotate_world();

  void store_world_probe_data(Texture &in_sphere_probe,
                              const SphereProbeAtlasCoord &atlas_coord,
                              StorageBuffer<SphereProbeHarmonic, true> &in_volume_probe,
                              UniformArrayBuffer<LightData, 2> &in_sunlight);

  void rotate_world_probe_data(Texture &dst_sphere_probe,
                               const SphereProbeAtlasCoord &atlas_coord,
                               StorageBuffer<SphereProbeHarmonic, true> &dst_volume_probe,
                               UniformArrayBuffer<LightData, 2> &dst_sunlight,
                               float4x4 &rotation);

 private:
  void sync_pass(PassSimple &pass,
                 gpu::Batch *geom,
                 ::Material *mat,
                 ResourceHandleRange res_handle);
  void sync_display();

  float calc_viewport_scale();
  SphereLOD calc_level_of_detail(const float viewport_scale);
  blender::gpu::Batch *sphere_get(const SphereLOD level_of_detail);

  friend class LookdevView;
};

/** \} */

}  // namespace blender::eevee
