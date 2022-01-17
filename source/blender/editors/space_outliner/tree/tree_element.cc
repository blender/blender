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
 * \ingroup spoutliner
 */

#include "DNA_anim_types.h"
#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "UI_resources.h"

#include "BLT_translation.h"

#include "tree_element_anim_data.hh"
#include "tree_element_collection.hh"
#include "tree_element_driver.hh"
#include "tree_element_gpencil_layer.hh"
#include "tree_element_id.hh"
#include "tree_element_nla.hh"
#include "tree_element_overrides.hh"
#include "tree_element_scene_objects.hh"
#include "tree_element_view_layer.hh"

#include "../outliner_intern.hh"
#include "tree_element.hh"

namespace blender::ed::outliner {

std::unique_ptr<AbstractTreeElement> AbstractTreeElement::createFromType(const int type,
                                                                         TreeElement &legacy_te,
                                                                         void *idv)
{
  ID &id = *static_cast<ID *>(idv);

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
      return TreeElementID::createFromID(legacy_te, id);
    case TSE_ANIM_DATA:
      return std::make_unique<TreeElementAnimData>(legacy_te,
                                                   *reinterpret_cast<IdAdtTemplate &>(id).adt);
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
      return std::make_unique<TreeElementOverridesBase>(legacy_te, id);
    case TSE_LIBRARY_OVERRIDE:
      return std::make_unique<TreeElementOverridesProperty>(
          legacy_te, *static_cast<TreeElementOverridesData *>(idv));
    default:
      break;
  }

  return nullptr;
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
  tree_element.postExpand(space_outliner);
}

bool tree_element_warnings_get(TreeElement *te, int *r_icon, const char **r_message)
{
  TreeStoreElem *tselem = te->store_elem;

  if (tselem->type != TSE_SOME_ID) {
    return false;
  }
  if (te->idcode != ID_LI) {
    return false;
  }

  Library *library = (Library *)tselem->id;
  if (library->tag & LIBRARY_TAG_RESYNC_REQUIRED) {
    if (r_icon) {
      *r_icon = ICON_ERROR;
    }
    if (r_message) {
      *r_message = TIP_(
          "Contains linked library overrides that need to be resynced, updating the library is "
          "recommended");
    }
    return true;
  }
  if (library->id.tag & LIB_TAG_MISSING) {
    if (r_icon) {
      *r_icon = ICON_ERROR;
    }
    if (r_message) {
      *r_message = TIP_("Missing library");
    }
    return true;
  }
  return false;
}

}  // namespace blender::ed::outliner
