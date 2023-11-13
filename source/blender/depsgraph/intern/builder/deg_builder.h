/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

struct Base;
struct ID;
struct Main;
struct ModifierData;
struct Object;
struct PointerRNA;
struct Scene;
struct bPoseChannel;

namespace blender::deg {

struct Depsgraph;
class DepsgraphBuilderCache;

class DepsgraphBuilder {
 public:
  virtual ~DepsgraphBuilder() = default;

  virtual bool need_pull_base_into_graph(const Base *base);

  virtual bool is_object_visibility_animated(const Object *object);
  virtual bool is_modifier_visibility_animated(const Object *object, const ModifierData *modifier);

  virtual bool check_pchan_has_bbone(const Object *object, const bPoseChannel *pchan);
  virtual bool check_pchan_has_bbone_segments(const Object *object, const bPoseChannel *pchan);
  virtual bool check_pchan_has_bbone_segments(const Object *object, const char *bone_name);

  /**
   * If `target_prop` + `rna_path` uses indirection via the `scene.camera` pointer, returns
   * the sub-string of `rna_path` relative to the camera; otherwise returns nullptr.
   */
  static const char *get_rna_path_relative_to_scene_camera(const Scene *scene,
                                                           const PointerRNA &target_prop,
                                                           const char *rna_path);

 protected:
  /* NOTE: The builder does NOT take ownership over any of those resources. */
  DepsgraphBuilder(Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache);

  /* State which never changes, same for the whole builder time. */
  Main *bmain_;
  Depsgraph *graph_;
  DepsgraphBuilderCache *cache_;
};

bool deg_check_id_in_depsgraph(const Depsgraph *graph, ID *id_orig);
bool deg_check_base_in_depsgraph(const Depsgraph *graph, Base *base);
void deg_graph_build_finalize(Main *bmain, Depsgraph *graph);

}  // namespace blender::deg
