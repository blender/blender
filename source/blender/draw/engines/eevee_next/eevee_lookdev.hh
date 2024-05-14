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

#include "DNA_world_types.h"

namespace blender::eevee {

class Instance;
class LookdevView;

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
  ::Image image = {};
  ::World world = {};

  LookdevParameters parameters_;

 public:
  LookdevWorld();
  ~LookdevWorld();

  /* Returns true if an update was detected. */
  bool sync(const LookdevParameters &new_parameters);

  ::World *world_get()
  {
    return &world;
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

class LookdevModule {
 private:
  Instance &inst_;

  bool enabled_;

  static constexpr int num_spheres = 2;
  /**
   * The scale of the lookdev spheres.
   *
   * The lookdev spheres are resized to a small scale. This would reduce shadow artifacts as they
   * would most likely be inside or outside shadow.
   */
  static constexpr float sphere_scale = 0.01f;

  rcti visible_rect_;

  /* Dummy textures: required to reuse forward mesh shader and avoid another shader variation. */
  Texture dummy_cryptomatte_tx_;
  Texture dummy_aov_color_tx_;
  Texture dummy_aov_value_tx_;

  Texture depth_tx_ = {"Lookdev.Depth"};

  struct Sphere {
    Framebuffer framebuffer = {"Lookdev.Framebuffer"};
    Texture color_tx_ = {"Lookdev.Color"};
    PassSimple pass = {"Lookdev.Sphere"};
  };

  Sphere spheres_[num_spheres];
  PassSimple display_ps_ = {"Lookdev.Display"};

 public:
  LookdevModule(Instance &inst);
  ~LookdevModule();

  void init(const rcti *visible_rect);
  void sync();

  void draw(View &view);

  void display();

 private:
  void sync_pass(PassSimple &pass, gpu::Batch *geom, ::Material *mat, ResourceHandle res_handle);
  void sync_display();

  float calc_viewport_scale();

  friend class LookdevView;
};

/** \} */

}  // namespace blender::eevee
