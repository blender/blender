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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/SceneExporter.cpp
 *  \ingroup collada
 */

#include "SceneExporter.h"

SceneExporter::SceneExporter(COLLADASW::StreamWriter *sw, ArmatureExporter *arm, const ExportSettings *export_settings)
	: COLLADASW::LibraryVisualScenes(sw), arm_exporter(arm), export_settings(export_settings)
{}
	
void SceneExporter::exportScene(Scene *sce)
{
	// <library_visual_scenes> <visual_scene>
	std::string id_naming = id_name(sce);
	openVisualScene(translate_id(id_naming), id_naming);
	exportHierarchy(sce);
	closeVisualScene();
	closeLibrary();
}

void SceneExporter::exportHierarchy(Scene *sce)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;

		if (!ob->parent) {
			if (sce->lay & ob->lay) {
				switch(ob->type) {
					case OB_MESH:
					case OB_CAMERA:
					case OB_LAMP:
					case OB_ARMATURE:
					case OB_EMPTY:
						if (this->export_settings->selected && !(ob->flag & SELECT)) {
							break;
						}
						// write nodes....
						writeNodes(ob, sce);
						break;
				}
			}
		}

		base= base->next;
	}
}

void SceneExporter::writeNodes(Object *ob, Scene *sce)
{
	COLLADASW::Node node(mSW);
	node.setNodeId(translate_id(id_name(ob)));
	node.setType(COLLADASW::Node::NODE);

	node.start();

	bool is_skinned_mesh = arm_exporter->is_skinned_mesh(ob);
	std::list<Object*> child_objects;

	// list child objects
	Base *b = (Base*) sce->base.first;
	while(b) {
		// cob - child object
		Object *cob = b->object;

		if (cob->parent == ob) {
			switch(cob->type) {
				case OB_MESH:
				case OB_CAMERA:
				case OB_LAMP:
				case OB_EMPTY:
				case OB_ARMATURE:
					child_objects.push_back(cob);
					break;
			}
		}

		b = b->next;
	}


	if (ob->type == OB_MESH && is_skinned_mesh)
		// for skinned mesh we write obmat in <bind_shape_matrix>
		TransformWriter::add_node_transform_identity(node);
	else
		TransformWriter::add_node_transform_ob(node, ob);

	// <instance_geometry>
	if (ob->type == OB_MESH) {
		if (is_skinned_mesh) {
			arm_exporter->add_instance_controller(ob);
		}
		else {
			COLLADASW::InstanceGeometry instGeom(mSW);
			instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_geometry_id(ob)));

			InstanceWriter::add_material_bindings(instGeom.getBindMaterial(), ob);

			instGeom.add();
		}
	}

	// <instance_controller>
	else if (ob->type == OB_ARMATURE) {
		arm_exporter->add_armature_bones(ob, sce, this, child_objects);

		// XXX this looks unstable...
		node.end();
	}

	// <instance_camera>
	else if (ob->type == OB_CAMERA) {
		COLLADASW::InstanceCamera instCam(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_camera_id(ob)));
		instCam.add();
	}

	// <instance_light>
	else if (ob->type == OB_LAMP) {
		COLLADASW::InstanceLight instLa(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_light_id(ob)));
		instLa.add();
	}

	// empty object
	else if (ob->type == OB_EMPTY) { // TODO: handle groups (OB_DUPLIGROUP
		if ((ob->transflag & OB_DUPLIGROUP) == OB_DUPLIGROUP && ob->dup_group) {
			GroupObject *go = NULL;
			Group *gr = ob->dup_group;
			/* printf("group detected '%s'\n", gr->id.name+2); */
			for (go = (GroupObject*)(gr->gobject.first); go; go=go->next) {
				printf("\t%s\n", go->ob->id.name);
			}
		}
	}

	for (std::list<Object*>::iterator i= child_objects.begin(); i != child_objects.end(); ++i)
	{
		writeNodes(*i, sce);
	}


	if (ob->type != OB_ARMATURE)
		node.end();
}

