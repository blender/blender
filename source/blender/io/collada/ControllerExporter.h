/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <list>
#include <string>
// #include <vector>

#include "COLLADASWExtraTechnique.h"
#include "COLLADASWInputList.h"
#include "COLLADASWInstanceController.h"
#include "COLLADASWLibraryControllers.h"
#include "COLLADASWNode.h"
#include "COLLADASWStreamWriter.h"

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "InstanceWriter.h"
#include "TransformWriter.h"

#include "ExportSettings.h"

#include "BKE_key.hh"

class SceneExporter;

class ControllerExporter : public COLLADASW::LibraryControllers,
                           protected TransformWriter,
                           protected InstanceWriter {
 private:
  BlenderContext &blender_context;
  BCExportSettings export_settings;

 public:
  /* XXX exporter writes wrong data for shared armatures.  A separate
   * controller should be written for each armature-mesh binding how do
   * we make controller ids then? */
  ControllerExporter(BlenderContext &blender_context,
                     COLLADASW::StreamWriter *sw,
                     BCExportSettings &export_settings)
      : COLLADASW::LibraryControllers(sw),
        blender_context(blender_context),
        export_settings(export_settings)
  {
  }

  bool is_skinned_mesh(Object *ob);

  bool add_instance_controller(Object *ob);

  void export_controllers();

  void operator()(Object *ob);

 private:
#if 0
  std::vector<Object *> written_armatures;

  bool already_written(Object *ob_arm);

  void wrote(Object *ob_arm);

  void find_objects_using_armature(Object *ob_arm, std::vector<Object *> &objects, Scene *sce);
#endif

  std::string get_controller_id(Object *ob_arm, Object *ob);

  std::string get_controller_id(Key *key, Object *ob);

  /** `ob` should be of type OB_MESH, both arguments are required. */
  void export_skin_controller(Object *ob, Object *ob_arm);

  void export_morph_controller(Object *ob, Key *key);

  void add_joints_element(const ListBase *defbase,
                          const std::string &joints_source_id,
                          const std::string &inv_bind_mat_source_id);

  void add_bind_shape_mat(Object *ob);

  std::string add_morph_targets(Key *key, Object *ob);

  std::string add_morph_weights(Key *key, Object *ob);

  /**
   * Added to implement support for animations.
   */
  void add_weight_extras(Key *key);

  std::string add_joints_source(Object *ob_arm,
                                const ListBase *defbase,
                                const std::string &controller_id);

  std::string add_inv_bind_mats_source(Object *ob_arm,
                                       const ListBase *defbase,
                                       const std::string &controller_id);

  Bone *get_bone_from_defgroup(Object *ob_arm, const bDeformGroup *def);

  bool is_bone_defgroup(Object *ob_arm, const bDeformGroup *def);

  std::string add_weights_source(Mesh *mesh,
                                 const std::string &controller_id,
                                 const std::list<float> &weights);

  void add_vertex_weights_element(const std::string &weights_source_id,
                                  const std::string &joints_source_id,
                                  const std::list<int> &vcount,
                                  const std::list<int> &joints);

  void write_bone_URLs(COLLADASW::InstanceController &ins, Object *ob_arm, Bone *bone);
};
