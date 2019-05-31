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

extern "C" {
#include "BLI_utildefines.h"
#include "BKE_collection.h"
#include "BKE_object.h"
#include "BLI_listbase.h"
#include "BKE_library.h"
}

#include "SceneExporter.h"
#include "collada_utils.h"
#include "BCSampleData.h"

void SceneExporter::exportScene()
{
  Scene *scene = blender_context.get_scene();

  /* <library_visual_scenes> <visual_scene> */
  std::string name = id_name(scene);
  openVisualScene(translate_id(name), encode_xml(name));
  exportHierarchy();
  closeVisualScene();
  closeLibrary();
}

void SceneExporter::exportHierarchy()
{
  LinkNode *node;
  ColladaBaseNodes base_objects;

  /* Ensure all objects in the export_set are marked */
  for (node = this->export_settings.get_export_set(); node; node = node->next) {
    Object *ob = (Object *)node->link;
    ob->id.tag |= LIB_TAG_DOIT;
  }

  /* Now find all exportable base objects (highest in export hierarchy) */
  for (node = this->export_settings.get_export_set(); node; node = node->next) {
    Object *ob = (Object *)node->link;
    if (this->export_settings.is_export_root(ob)) {
      switch (ob->type) {
        case OB_MESH:
        case OB_CAMERA:
        case OB_LAMP:
        case OB_EMPTY:
        case OB_GPENCIL:
        case OB_ARMATURE:
          base_objects.add(ob);
          break;
      }
    }
  }

  /* And now export the base objects: */
  for (int index = 0; index < base_objects.size(); index++) {
    Object *ob = base_objects.get(index);
    writeNode(ob);
    if (bc_is_marked(ob)) {
      bc_remove_mark(ob);
    }
  }
}

void SceneExporter::writeNodeList(std::vector<Object *> &child_objects, Object *parent)
{
  /* TODO: Handle the case where a parent is not exported
   * Actually i am not even sure if this can be done at all
   * in a good way.
   * I really prefer to enforce the export of hidden
   * elements in an object hierarchy. When the children of
   * the hidden elements are exported as well. */
  for (int i = 0; i < child_objects.size(); ++i) {
    Object *child = child_objects[i];
    writeNode(child);
    if (bc_is_marked(child)) {
      bc_remove_mark(child);
    }
  }
}

void SceneExporter::writeNode(Object *ob)
{
  ViewLayer *view_layer = blender_context.get_view_layer();

  std::vector<Object *> child_objects;
  bc_get_children(child_objects, ob, view_layer);
  bool can_export = bc_is_in_Export_set(this->export_settings.get_export_set(), ob, view_layer);

  /* Add associated armature first if available */
  bool armature_exported = false;
  Object *ob_arm = bc_get_assigned_armature(ob);

  if (ob_arm != NULL) {
    armature_exported = bc_is_in_Export_set(
        this->export_settings.get_export_set(), ob_arm, view_layer);
    if (armature_exported && bc_is_marked(ob_arm)) {
      writeNode(ob_arm);
      bc_remove_mark(ob_arm);
      armature_exported = true;
    }
  }

  if (can_export) {
    COLLADASW::Node colladaNode(mSW);
    colladaNode.setNodeId(translate_id(id_name(ob)));
    colladaNode.setNodeName(encode_xml(id_name(ob)));
    colladaNode.setType(COLLADASW::Node::NODE);

    colladaNode.start();
    if (ob->type == OB_MESH && armature_exported) {
      /* for skinned mesh we write obmat in <bind_shape_matrix> */
      TransformWriter::add_node_transform_identity(colladaNode, this->export_settings);
    }
    else {
      TransformWriter::add_node_transform_ob(colladaNode, ob, this->export_settings);
    }

    /* <instance_geometry> */
    if (ob->type == OB_MESH) {
      bool instance_controller_created = false;
      if (armature_exported) {
        instance_controller_created = arm_exporter->add_instance_controller(ob);
      }
      if (!instance_controller_created) {
        COLLADASW::InstanceGeometry instGeom(mSW);
        instGeom.setUrl(COLLADASW::URI(
            COLLADABU::Utils::EMPTY_STRING,
            get_geometry_id(ob, this->export_settings.get_use_object_instantiation())));
        instGeom.setName(encode_xml(id_name(ob)));
        InstanceWriter::add_material_bindings(
            instGeom.getBindMaterial(), ob, this->export_settings.get_active_uv_only());
        instGeom.add();
      }
    }

    /* <instance_controller> */
    else if (ob->type == OB_ARMATURE) {
      arm_exporter->add_armature_bones(ob, view_layer, this, child_objects);
    }

    /* <instance_camera> */
    else if (ob->type == OB_CAMERA) {
      COLLADASW::InstanceCamera instCam(
          mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_camera_id(ob)));
      instCam.add();
    }

    /* <instance_light> */
    else if (ob->type == OB_LAMP) {
      COLLADASW::InstanceLight instLa(
          mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_light_id(ob)));
      instLa.add();
    }

    /* empty object */
    else if (ob->type == OB_EMPTY) { /* TODO: handle groups (OB_DUPLICOLLECTION */
      if ((ob->transflag & OB_DUPLICOLLECTION) == OB_DUPLICOLLECTION && ob->instance_collection) {
        Collection *collection = ob->instance_collection;
        /* printf("group detected '%s'\n", group->id.name + 2); */
        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, object) {
          printf("\t%s\n", object->id.name);
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      }

      if (BLI_listbase_is_empty(&ob->constraints) == false) {
        bConstraint *con = (bConstraint *)ob->constraints.first;
        while (con) {
          std::string con_name(encode_xml(con->name));
          std::string con_tag = con_name + "_constraint";
          printf("%s\n", con_name.c_str());
          printf("%s\n\n", con_tag.c_str());
          colladaNode.addExtraTechniqueChildParameter("blender", con_tag, "type", con->type);
          colladaNode.addExtraTechniqueChildParameter("blender", con_tag, "enforce", con->enforce);
          colladaNode.addExtraTechniqueChildParameter("blender", con_tag, "flag", con->flag);
          colladaNode.addExtraTechniqueChildParameter(
              "blender", con_tag, "headtail", con->headtail);
          colladaNode.addExtraTechniqueChildParameter(
              "blender", con_tag, "lin_error", con->lin_error);
          colladaNode.addExtraTechniqueChildParameter(
              "blender", con_tag, "own_space", con->ownspace);
          colladaNode.addExtraTechniqueChildParameter(
              "blender", con_tag, "rot_error", con->rot_error);
          colladaNode.addExtraTechniqueChildParameter(
              "blender", con_tag, "tar_space", con->tarspace);
          colladaNode.addExtraTechniqueChildParameter(
              "blender", con_tag, "lin_error", con->lin_error);

          /* not ideal: add the target object name as another parameter.
           * No real mapping in the .dae
           * Need support for multiple target objects also. */
          const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
          ListBase targets = {NULL, NULL};
          if (cti && cti->get_constraint_targets) {

            bConstraintTarget *ct;
            Object *obtar;

            cti->get_constraint_targets(con, &targets);

            for (ct = (bConstraintTarget *)targets.first; ct; ct = ct->next) {
              obtar = ct->tar;
              std::string tar_id((obtar) ? id_name(obtar) : "");
              colladaNode.addExtraTechniqueChildParameter("blender", con_tag, "target_id", tar_id);
            }

            if (cti->flush_constraint_targets) {
              cti->flush_constraint_targets(con, &targets, 1);
            }
          }

          con = con->next;
        }
      }
    }
    bc_remove_mark(ob);
    writeNodeList(child_objects, ob);
    colladaNode.end();
  }
  else {
    writeNodeList(child_objects, ob);
  }
}
