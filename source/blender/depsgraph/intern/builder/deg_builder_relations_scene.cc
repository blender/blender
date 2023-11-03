/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

#include "DNA_scene_types.h"

namespace blender::deg {

void DepsgraphRelationBuilder::build_scene_render(Scene *scene, ViewLayer *view_layer)
{
  scene_ = scene;
  const bool build_compositor = (scene->r.scemode & R_DOCOMP);
  const bool build_sequencer = (scene->r.scemode & R_DOSEQ);
  build_scene_parameters(scene);
  build_animdata(&scene->id);
  build_scene_audio(scene);
  if (build_compositor) {
    build_scene_compositor(scene);
  }
  if (build_sequencer) {
    build_scene_sequencer(scene);
    build_scene_speakers(scene, view_layer);
  }
  if (scene->camera != nullptr) {
    build_object(scene->camera);
  }
}

void DepsgraphRelationBuilder::build_scene_camera(Scene *scene)
{
  if (scene->camera != nullptr) {
    build_object(scene->camera);
  }
  LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
    if (!ELEM(marker->camera, nullptr, scene->camera)) {
      build_object(marker->camera);
    }
  }
}

void DepsgraphRelationBuilder::build_scene_parameters(Scene *scene)
{
  if (built_map_.checkIsBuiltAndTag(scene, BuilderMap::TAG_PARAMETERS)) {
    return;
  }

  /* TODO(sergey): Trace as a scene parameters. */

  build_idproperties(scene->id.properties);
  build_parameters(&scene->id);
  OperationKey parameters_eval_key(
      &scene->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EXIT);
  OperationKey scene_eval_key(&scene->id, NodeType::PARAMETERS, OperationCode::SCENE_EVAL);
  add_relation(parameters_eval_key, scene_eval_key, "Parameters -> Scene Eval");

  LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
    build_idproperties(marker->prop);
  }
}

void DepsgraphRelationBuilder::build_scene_compositor(Scene *scene)
{
  if (built_map_.checkIsBuiltAndTag(scene, BuilderMap::TAG_SCENE_COMPOSITOR)) {
    return;
  }
  if (scene->nodetree == nullptr) {
    return;
  }

  /* TODO(sergey): Trace as a scene compositor. */

  build_nodetree(scene->nodetree);
}

}  // namespace blender::deg
