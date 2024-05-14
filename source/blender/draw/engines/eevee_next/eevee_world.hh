/* SPDX-FileCopyrightText: 2021 Blender Authors
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

#include "eevee_lookdev.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Default World Node-Tree
 *
 * In order to support worlds without node-tree we reuse and configure a standalone node-tree that
 * we pass for shader generation. The GPUMaterial is still stored inside the World even if
 * it does not use a node-tree.
 * \{ */

class DefaultWorldNodeTree {
 private:
  bNodeTree *ntree_;
  bNodeSocketValueRGBA *color_socket_;

 public:
  DefaultWorldNodeTree();
  ~DefaultWorldNodeTree();

  bNodeTree *nodetree_get(::World *world);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name World
 *
 * \{ */

class World {
 public:
  /**
   * Buffer containing the sun light for the world.
   * Filled by #LightProbeModule and read by #LightModule.  */
  UniformBuffer<LightData> sunlight = {"sunlight"};

 private:
  Instance &inst_;

  DefaultWorldNodeTree default_tree;

  /* Used to detect if world change. */
  ::World *prev_original_world = nullptr;

  /* Used when the scene doesn't have a world. */
  ::World *default_world_ = nullptr;

  /* Is true if world as a valid volume shader compiled. */
  bool has_volume_ = false;
  /* Is true if the volume shader has absorption. Disables distant lights. */
  bool has_volume_absorption_ = false;
  /* Is true if the volume shader has scattering. */
  bool has_volume_scatter_ = false;

  LookdevWorld lookdev_world_;

 public:
  World(Instance &inst) : inst_(inst){};
  ~World();

  void sync();

  bool has_volume() const
  {
    return has_volume_;
  }

  bool has_volume_absorption() const
  {
    return has_volume_absorption_;
  }

  bool has_volume_scatter() const
  {
    return has_volume_scatter_;
  }

  float sun_threshold();

  float sun_angle()
  {
    return scene_world_get()->sun_angle;
  }

  float sun_shadow_max_resolution()
  {
    return scene_world_get()->sun_shadow_maximum_resolution;
  }

  bool use_sun_shadow()
  {
    return scene_world_get()->flag & WO_USE_SUN_SHADOW;
  }

 private:
  void sync_volume();

  /* Returns a dummy black world for when a valid world isn't present or when we want to suppress
   * any light coming from the world. */
  ::World *default_world_get();

  /* Returns either the scene world or the default world if scene has no world. */
  ::World *scene_world_get();
};

/** \} */

}  // namespace blender::eevee
