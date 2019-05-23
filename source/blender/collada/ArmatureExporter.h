/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup collada
 */

#ifndef __ARMATUREEXPORTER_H__
#define __ARMATUREEXPORTER_H__

#include <list>
#include <string>
//#include <vector>

#include "COLLADASWStreamWriter.h"
#include "COLLADASWLibraryControllers.h"
#include "COLLADASWInputList.h"
#include "COLLADASWNode.h"

#include "DNA_armature_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"

#include "TransformWriter.h"
#include "InstanceWriter.h"

#include "ExportSettings.h"

class SceneExporter;

// XXX exporter writes wrong data for shared armatures.  A separate
// controller should be written for each armature-mesh binding how do
// we make controller ids then?
class ArmatureExporter : public COLLADASW::LibraryControllers,
                         protected TransformWriter,
                         protected InstanceWriter {
 public:
  // XXX exporter writes wrong data for shared armatures.  A separate
  // controller should be written for each armature-mesh binding how do
  // we make controller ids then?
  ArmatureExporter(BlenderContext &blender_context,
                   COLLADASW::StreamWriter *sw,
                   BCExportSettings &export_settings)
      : COLLADASW::LibraryControllers(sw),
        blender_context(blender_context),
        export_settings(export_settings)
  {
  }

  void add_armature_bones(Object *ob_arm,
                          ViewLayer *view_layer,
                          SceneExporter *se,
                          std::vector<Object *> &child_objects);

  bool add_instance_controller(Object *ob);

 private:
  BlenderContext &blender_context;
  BCExportSettings &export_settings;

#if 0
  std::vector<Object *> written_armatures;

  bool already_written(Object *ob_arm);

  void wrote(Object *ob_arm);

  void find_objects_using_armature(Object *ob_arm, std::vector<Object *> &objects, Scene *sce);
#endif

  // Scene, SceneExporter and the list of child_objects
  // are required for writing bone parented objects
  void add_bone_node(Bone *bone,
                     Object *ob_arm,
                     SceneExporter *se,
                     std::vector<Object *> &child_objects);

  inline bool can_export(Bone *bone)
  {
    return !(export_settings.get_deform_bones_only() && bone->flag & BONE_NO_DEFORM);
  }

  bool is_export_root(Bone *bone);
  void add_bone_transform(Object *ob_arm, Bone *bone, COLLADASW::Node &node);

  std::string get_controller_id(Object *ob_arm, Object *ob);

  void write_bone_URLs(COLLADASW::InstanceController &ins, Object *ob_arm, Bone *bone);
};

#endif
