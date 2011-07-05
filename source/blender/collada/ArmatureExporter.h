/*
 * $Id$
 *
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
#include "DNA_scene_types.h"

#include "TransformWriter.h"
#include "InstanceWriter.h"

// XXX exporter writes wrong data for shared armatures.  A separate
// controller should be written for each armature-mesh binding how do
// we make controller ids then?
class ArmatureExporter: public COLLADASW::LibraryControllers, protected TransformWriter, protected InstanceWriter
{
private:
	Scene *scene;

public:
	ArmatureExporter(COLLADASW::StreamWriter *sw);

	// write bone nodes
	void add_armature_bones(Object *ob_arm, Scene *sce);

	bool is_skinned_mesh(Object *ob);

	void add_instance_controller(Object *ob);

	void export_controllers(Scene *sce, bool export_selected);

	void operator()(Object *ob);

private:

	UnitConverter converter;

#if 0
	std::vector<Object*> written_armatures;

	bool already_written(Object *ob_arm);

	void wrote(Object *ob_arm);

	void find_objects_using_armature(Object *ob_arm, std::vector<Object *>& objects, Scene *sce);
#endif

	Object *get_assigned_armature(Object *ob);

	std::string get_joint_sid(Bone *bone, Object *ob_arm);

	// parent_mat is armature-space
	void add_bone_node(Bone *bone, Object *ob_arm);

	void add_bone_transform(Object *ob_arm, Bone *bone, COLLADASW::Node& node);

	std::string get_controller_id(Object *ob_arm, Object *ob);

	// ob should be of type OB_MESH
	// both args are required
	void export_controller(Object* ob, Object *ob_arm);

	void add_joints_element(ListBase *defbase,
							const std::string& joints_source_id, const std::string& inv_bind_mat_source_id);

	void add_bind_shape_mat(Object *ob);

	std::string add_joints_source(Object *ob_arm, ListBase *defbase, const std::string& controller_id);

	std::string add_inv_bind_mats_source(Object *ob_arm, ListBase *defbase, const std::string& controller_id);

	Bone *get_bone_from_defgroup(Object *ob_arm, bDeformGroup* def);

	bool is_bone_defgroup(Object *ob_arm, bDeformGroup* def);

	std::string add_weights_source(Mesh *me, const std::string& controller_id);

	void add_vertex_weights_element(const std::string& weights_source_id, const std::string& joints_source_id, Mesh *me,
									Object *ob_arm, ListBase *defbase);
};

/*
struct GeometryFunctor {
	// f should have
	// void operator()(Object* ob)
	template<class Functor>
	void forEachMeshObjectInScene(Scene *sce, Functor &f)
	{
		
		Base *base= (Base*) sce->base.first;
		while(base) {
			Object *ob = base->object;
			
			if (ob->type == OB_MESH && ob->data) {
				f(ob);
			}
			base= base->next;
			
		}
	}
};*/

#endif
