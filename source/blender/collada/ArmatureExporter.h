/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed,
 *                 Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ArmatureExporter.h
 *  \ingroup collada
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
class ArmatureExporter : public COLLADASW::LibraryControllers, protected TransformWriter, protected InstanceWriter
{
public:
	ArmatureExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings);

	// write bone nodes
	void add_armature_bones(Object *ob_arm, Scene *sce, SceneExporter *se,
	                        std::list<Object *>& child_objects);

	bool add_instance_controller(Object *ob);

	//void export_controllers(Scene *sce);*/

	//void operator()(Object *ob);

private:
	UnitConverter converter;
	const ExportSettings *export_settings;

#if 0
	std::vector<Object *> written_armatures;

	bool already_written(Object *ob_arm);

	void wrote(Object *ob_arm);

	void find_objects_using_armature(Object *ob_arm, std::vector<Object *>& objects, Scene *sce);
#endif

	std::string get_joint_sid(Bone *bone, Object *ob_arm);

	// Scene, SceneExporter and the list of child_objects
	// are required for writing bone parented objects
	void add_bone_node(Bone *bone, Object *ob_arm, Scene *sce, SceneExporter *se,
	                   std::list<Object *>& child_objects);

	void add_bone_transform(Object *ob_arm, Bone *bone, COLLADASW::Node& node);

	void add_blender_leaf_bone(Bone *bone, Object *ob_arm, COLLADASW::Node& node);

	std::string get_controller_id(Object *ob_arm, Object *ob);

	void write_bone_URLs(COLLADASW::InstanceController &ins, Object *ob_arm, Bone *bone);
};

#endif
