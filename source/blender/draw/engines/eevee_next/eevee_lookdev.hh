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
  bool gpu_parameters_changed(const LookdevParameters &other) const;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport Override Node-Tree
 *
 * In a viewport the world can be overridden by a custom HDRI and some settings.
 * \{ */

class LookdevWorldNodeTree {
 private:
  bNodeTree *ntree_ = nullptr;
  bNode *environment_node_ = nullptr;
  bNodeSocketValueFloat *intensity_socket_ = nullptr;
  bNodeSocketValueFloat *angle_socket_ = nullptr;
  ::Image image = {};

 public:
  LookdevWorldNodeTree();
  ~LookdevWorldNodeTree();

  bNodeTree *nodetree_get(const LookdevParameters &parameters);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lookdev
 *
 * Look Development can override the world.
 *
 * \{ */

class LookdevModule {
 public:
 private:
  Instance &inst_;

  LookdevWorldNodeTree world_override_tree;

  LookdevParameters parameters_;
  ListBase gpu_materials_ = {nullptr, nullptr};
  GPUMaterial *gpu_material_ = nullptr;

 public:
  LookdevModule(Instance &inst) : inst_(inst){};
  ~LookdevModule();

  bool sync_world();

 private:
  ::World *get_world(::bNodeTree *node_tree);
};

/** \} */

}  // namespace blender::eevee
