/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <iostream>
#include <string>
#include <string_view>

#include "DNA_anim_types.h"
#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "UI_resources.hh"

#include "BLT_translation.h"

#include "tree_element_anim_data.hh"
#include "tree_element_bone.hh"
#include "tree_element_collection.hh"
#include "tree_element_constraint.hh"
#include "tree_element_defgroup.hh"
#include "tree_element_driver.hh"
#include "tree_element_edit_bone.hh"
#include "tree_element_gpencil_effect.hh"
#include "tree_element_gpencil_layer.hh"
#include "tree_element_grease_pencil_node.hh"
#include "tree_element_id.hh"
#include "tree_element_label.hh"
#include "tree_element_layer_collection.hh"
#include "tree_element_linked_object.hh"
#include "tree_element_modifier.hh"
#include "tree_element_nla.hh"
#include "tree_element_overrides.hh"
#include "tree_element_particle_system.hh"
#include "tree_element_pose.hh"
#include "tree_element_pose_group.hh"
#include "tree_element_rna.hh"
#include "tree_element_scene_objects.hh"
#include "tree_element_seq.hh"
#include "tree_element_view_collection.hh"
#include "tree_element_view_layer.hh"

#include "../outliner_intern.hh"
#include "tree_element.hh"

namespace blender::ed::outliner {

std::unique_ptr<AbstractTreeElement> AbstractTreeElement::create_from_type(const int type,
                                                                           TreeElement &legacy_te,
                                                                           ID *owner_id,
                                                                           void *create_data)
{
  if (owner_id == nullptr && create_data == nullptr) {
    return nullptr;
  }

  /*
   * The following calls make an implicit assumption about what data was passed to the
   * `create_data` argument of #outliner_add_element(). The old code does this already, here we
   * just centralize it as much as possible for now. Would be nice to entirely get rid of that, no
   * more `void *`.
   *
   * Once #outliner_add_element() is sufficiently simplified, it should be replaced by a C++ call.
   * It could take the derived type as template parameter (e.g. #TreeElementAnimData) and use C++
   * perfect forwarding to pass any data to the type's constructor.
   * If general Outliner code wants to access the data, they can query that through the derived
   * element type then. There's no need for `void *` anymore then.
   */

  switch (type) {
    case TSE_SOME_ID:
      return TreeElementID::create_from_id(legacy_te, *owner_id);
    case TSE_GENERIC_LABEL:
      return std::make_unique<TreeElementLabel>(legacy_te, static_cast<const char *>(create_data));
    case TSE_ANIM_DATA:
      return std::make_unique<TreeElementAnimData>(legacy_te,
                                                   *static_cast<AnimData *>(create_data));
    case TSE_DRIVER_BASE:
      return std::make_unique<TreeElementDriverBase>(legacy_te,
                                                     *static_cast<AnimData *>(create_data));
    case TSE_NLA:
      return std::make_unique<TreeElementNLA>(legacy_te, *static_cast<AnimData *>(create_data));
    case TSE_NLA_TRACK:
      return std::make_unique<TreeElementNLATrack>(legacy_te,
                                                   *static_cast<NlaTrack *>(create_data));
    case TSE_NLA_ACTION:
      return std::make_unique<TreeElementNLAAction>(legacy_te,
                                                    *reinterpret_cast<bAction *>(owner_id));
    case TSE_GP_LAYER:
      return std::make_unique<TreeElementGPencilLayer>(legacy_te,
                                                       *static_cast<bGPDlayer *>(create_data));
    case TSE_GREASE_PENCIL_NODE:
      return std::make_unique<TreeElementGreasePencilNode>(
          legacy_te,
          *reinterpret_cast<GreasePencil *>(owner_id),
          *static_cast<bke::greasepencil::TreeNode *>(create_data));
    case TSE_R_LAYER_BASE:
      return std::make_unique<TreeElementViewLayerBase>(legacy_te,
                                                        *reinterpret_cast<Scene *>(owner_id));
    case TSE_R_LAYER:
      return std::make_unique<TreeElementViewLayer>(
          legacy_te, *reinterpret_cast<Scene *>(owner_id), *static_cast<ViewLayer *>(create_data));
    case TSE_SCENE_COLLECTION_BASE:
      return std::make_unique<TreeElementCollectionBase>(legacy_te,
                                                         *reinterpret_cast<Scene *>(owner_id));
    case TSE_SCENE_OBJECTS_BASE:
      return std::make_unique<TreeElementSceneObjectsBase>(legacy_te,
                                                           *reinterpret_cast<Scene *>(owner_id));
    case TSE_LIBRARY_OVERRIDE_BASE:
      return std::make_unique<TreeElementOverridesBase>(legacy_te, *owner_id);
    case TSE_LIBRARY_OVERRIDE:
      return std::make_unique<TreeElementOverridesProperty>(
          legacy_te, *static_cast<TreeElementOverridesData *>(create_data));
    case TSE_LIBRARY_OVERRIDE_OPERATION:
      return std::make_unique<TreeElementOverridesPropertyOperation>(
          legacy_te, *static_cast<TreeElementOverridesData *>(create_data));
    case TSE_RNA_STRUCT:
      return std::make_unique<TreeElementRNAStruct>(legacy_te,
                                                    *static_cast<PointerRNA *>(create_data));
    case TSE_RNA_PROPERTY:
      return std::make_unique<TreeElementRNAProperty>(
          legacy_te, *static_cast<PointerRNA *>(create_data), legacy_te.index);
    case TSE_RNA_ARRAY_ELEM:
      return std::make_unique<TreeElementRNAArrayElement>(
          legacy_te, *static_cast<PointerRNA *>(create_data), legacy_te.index);
    case TSE_SEQUENCE:
      return std::make_unique<TreeElementSequence>(legacy_te,
                                                   *static_cast<Sequence *>(create_data));
    case TSE_SEQ_STRIP:
      return std::make_unique<TreeElementSequenceStrip>(legacy_te,
                                                        *static_cast<Strip *>(create_data));
    case TSE_SEQUENCE_DUP:
      return std::make_unique<TreeElementSequenceStripDuplicate>(
          legacy_te, *static_cast<Sequence *>(create_data));
    case TSE_BONE:
      return std::make_unique<TreeElementBone>(
          legacy_te, *owner_id, *static_cast<Bone *>(create_data));
    case TSE_EBONE:
      return std::make_unique<TreeElementEditBone>(
          legacy_te, *owner_id, *static_cast<EditBone *>(create_data));
    case TSE_GPENCIL_EFFECT:
      return std::make_unique<TreeElementGPencilEffect>(legacy_te,
                                                        *reinterpret_cast<Object *>(owner_id),
                                                        *static_cast<ShaderFxData *>(create_data));
    case TSE_GPENCIL_EFFECT_BASE:
      return std::make_unique<TreeElementGPencilEffectBase>(legacy_te,
                                                            *reinterpret_cast<Object *>(owner_id));
    case TSE_DEFGROUP_BASE:
      return std::make_unique<TreeElementDeformGroupBase>(legacy_te,
                                                          *reinterpret_cast<Object *>(owner_id));
    case TSE_DEFGROUP:
      return std::make_unique<TreeElementDeformGroup>(legacy_te,
                                                      *reinterpret_cast<Object *>(owner_id),
                                                      *static_cast<bDeformGroup *>(create_data));
    case TSE_LINKED_PSYS:
      return std::make_unique<TreeElementParticleSystem>(
          legacy_te,
          *reinterpret_cast<Object *>(owner_id),
          *static_cast<ParticleSystem *>(create_data));
    case TSE_CONSTRAINT_BASE:
      return std::make_unique<TreeElementConstraintBase>(legacy_te,
                                                         *reinterpret_cast<Object *>(owner_id));
    case TSE_CONSTRAINT:
      return std::make_unique<TreeElementConstraint>(legacy_te,
                                                     *reinterpret_cast<Object *>(owner_id),
                                                     *static_cast<bConstraint *>(create_data));
    case TSE_POSE_BASE:
      return std::make_unique<TreeElementPoseBase>(legacy_te,
                                                   *reinterpret_cast<Object *>(owner_id));
    case TSE_POSE_CHANNEL:
      return std::make_unique<TreeElementPoseChannel>(legacy_te,
                                                      *reinterpret_cast<Object *>(owner_id),
                                                      *static_cast<bPoseChannel *>(create_data));
    case TSE_POSEGRP_BASE:
      return std::make_unique<TreeElementPoseGroupBase>(legacy_te,
                                                        *reinterpret_cast<Object *>(owner_id));
    case TSE_POSEGRP:
      return std::make_unique<TreeElementPoseGroup>(legacy_te,
                                                    *reinterpret_cast<Object *>(owner_id),
                                                    *static_cast<bActionGroup *>(create_data));
    case TSE_MODIFIER_BASE:
      return std::make_unique<TreeElementModifierBase>(legacy_te,
                                                       *reinterpret_cast<Object *>(owner_id));
    case TSE_MODIFIER:
      return std::make_unique<TreeElementModifier>(
          legacy_te,
          *reinterpret_cast<Object *>(owner_id),
          *static_cast<ModifierDataStoreElem *>(create_data));
    case TSE_LINKED_OB:
      return std::make_unique<TreeElementLinkedObject>(legacy_te, *owner_id);
    case TSE_VIEW_COLLECTION_BASE:
      return std::make_unique<TreeElementViewCollectionBase>(legacy_te,
                                                             *reinterpret_cast<Scene *>(owner_id));
    case TSE_LAYER_COLLECTION:
      return std::make_unique<TreeElementLayerCollection>(
          legacy_te, *static_cast<LayerCollection *>(create_data));
    default:
      break;
  }

  return nullptr;
}

StringRefNull AbstractTreeElement::get_warning() const
{
  return "";
}

std::optional<BIFIconID> AbstractTreeElement::get_icon() const
{
  return {};
}

void AbstractTreeElement::print_path()
{
  std::string path = legacy_te_.name;

  for (TreeElement *parent = legacy_te_.parent; parent; parent = parent->parent) {
    path = parent->name + std::string_view("/") + path;
  }

  std::cout << path << std::endl;
}

void AbstractTreeElement::uncollapse_by_default(TreeElement *legacy_te)
{
  if (!TREESTORE(legacy_te)->used) {
    TREESTORE(legacy_te)->flag &= ~TSE_CLOSED;
  }
}

void tree_element_expand(const AbstractTreeElement &tree_element, SpaceOutliner &space_outliner)
{
  /* Most types can just expand. IDs optionally expand (hence the poll) and do additional, common
   * expanding. Could be done nicer, we could request a small "expander" helper object from the
   * element type, that the IDs have a more advanced implementation for. */
  if (!tree_element.expand_poll(space_outliner)) {
    return;
  }
  tree_element.expand(space_outliner);
}

}  // namespace blender::ed::outliner
