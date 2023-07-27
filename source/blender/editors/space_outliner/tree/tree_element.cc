/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <string>
#include <string_view>

#include "DNA_anim_types.h"
#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "UI_resources.h"

#include "BLT_translation.h"

#include "tree_element_anim_data.hh"
#include "tree_element_bone.hh"
#include "tree_element_collection.hh"
#include "tree_element_driver.hh"
#include "tree_element_edit_bone.hh"
#include "tree_element_gpencil_layer.hh"
#include "tree_element_id.hh"
#include "tree_element_label.hh"
#include "tree_element_nla.hh"
#include "tree_element_overrides.hh"
#include "tree_element_particle_system.hh"
#include "tree_element_rna.hh"
#include "tree_element_scene_objects.hh"
#include "tree_element_seq.hh"
#include "tree_element_view_layer.hh"

#include "../outliner_intern.hh"
#include "tree_element.hh"

namespace blender::ed::outliner {

std::unique_ptr<AbstractTreeElement> AbstractTreeElement::createFromType(const int type,
                                                                         TreeElement &legacy_te,
                                                                         void *idv)
{
  if (idv == nullptr) {
    return nullptr;
  }

  /*
   * The following calls make an implicit assumption about what data was passed to the `idv`
   * argument of #outliner_add_element(). The old code does this already, here we just centralize
   * it as much as possible for now. Would be nice to entirely get rid of that, no more `void *`.
   *
   * Once #outliner_add_element() is sufficiently simplified, it should be replaced by a C++ call.
   * It could take the derived type as template parameter (e.g. #TreeElementAnimData) and use C++
   * perfect forwarding to pass any data to the type's constructor.
   * If general Outliner code wants to access the data, they can query that through the derived
   * element type then. There's no need for `void *` anymore then.
   */

  switch (type) {
    case TSE_SOME_ID:
      return TreeElementID::createFromID(legacy_te, *static_cast<ID *>(idv));
    case TSE_GENERIC_LABEL:
      return std::make_unique<TreeElementLabel>(legacy_te, static_cast<const char *>(idv));
    case TSE_ANIM_DATA:
      return std::make_unique<TreeElementAnimData>(legacy_te,
                                                   *static_cast<IdAdtTemplate *>(idv)->adt);
    case TSE_DRIVER_BASE:
      return std::make_unique<TreeElementDriverBase>(legacy_te, *static_cast<AnimData *>(idv));
    case TSE_NLA:
      return std::make_unique<TreeElementNLA>(legacy_te, *static_cast<AnimData *>(idv));
    case TSE_NLA_TRACK:
      return std::make_unique<TreeElementNLATrack>(legacy_te, *static_cast<NlaTrack *>(idv));
    case TSE_NLA_ACTION:
      return std::make_unique<TreeElementNLAAction>(legacy_te, *static_cast<bAction *>(idv));
    case TSE_GP_LAYER:
      return std::make_unique<TreeElementGPencilLayer>(legacy_te, *static_cast<bGPDlayer *>(idv));
    case TSE_R_LAYER_BASE:
      return std::make_unique<TreeElementViewLayerBase>(legacy_te, *static_cast<Scene *>(idv));
    case TSE_SCENE_COLLECTION_BASE:
      return std::make_unique<TreeElementCollectionBase>(legacy_te, *static_cast<Scene *>(idv));
    case TSE_SCENE_OBJECTS_BASE:
      return std::make_unique<TreeElementSceneObjectsBase>(legacy_te, *static_cast<Scene *>(idv));
    case TSE_LIBRARY_OVERRIDE_BASE:
      return std::make_unique<TreeElementOverridesBase>(legacy_te, *static_cast<ID *>(idv));
    case TSE_LIBRARY_OVERRIDE:
      return std::make_unique<TreeElementOverridesProperty>(
          legacy_te, *static_cast<TreeElementOverridesData *>(idv));
    case TSE_LIBRARY_OVERRIDE_OPERATION:
      return std::make_unique<TreeElementOverridesPropertyOperation>(
          legacy_te, *static_cast<TreeElementOverridesData *>(idv));
    case TSE_RNA_STRUCT:
      return std::make_unique<TreeElementRNAStruct>(legacy_te, *static_cast<PointerRNA *>(idv));
    case TSE_RNA_PROPERTY:
      return std::make_unique<TreeElementRNAProperty>(
          legacy_te, *static_cast<PointerRNA *>(idv), legacy_te.index);
    case TSE_RNA_ARRAY_ELEM:
      return std::make_unique<TreeElementRNAArrayElement>(
          legacy_te, *static_cast<PointerRNA *>(idv), legacy_te.index);
    case TSE_SEQUENCE:
      return std::make_unique<TreeElementSequence>(legacy_te, *static_cast<Sequence *>(idv));
    case TSE_SEQ_STRIP:
      return std::make_unique<TreeElementSequenceStrip>(legacy_te, *static_cast<Strip *>(idv));
    case TSE_SEQUENCE_DUP:
      return std::make_unique<TreeElementSequenceStripDuplicate>(legacy_te,
                                                                 *static_cast<Sequence *>(idv));
    case TSE_BONE: {
      BoneElementCreateData *bone_data = static_cast<BoneElementCreateData *>(idv);
      return std::make_unique<TreeElementBone>(
          legacy_te, *bone_data->armature_id, *bone_data->bone);
    }
    case TSE_EBONE: {
      EditBoneElementCreateData *ebone_data = static_cast<EditBoneElementCreateData *>(idv);
      return std::make_unique<TreeElementEditBone>(
          legacy_te, *ebone_data->armature_id, *ebone_data->ebone);
    }
    case TSE_LINKED_PSYS: {
      ParticleSystemElementCreateData *psys_data = static_cast<ParticleSystemElementCreateData *>(
          idv);
      return std::make_unique<TreeElementParticleSystem>(
          legacy_te, *psys_data->object, *psys_data->psys);
    }
    default:
      break;
  }

  return nullptr;
}

StringRefNull AbstractTreeElement::getWarning() const
{
  return "";
}

std::optional<BIFIconID> AbstractTreeElement::getIcon() const
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
  if (!tree_element.expandPoll(space_outliner)) {
    return;
  }
  tree_element.expand(space_outliner);
}

}  // namespace blender::ed::outliner
