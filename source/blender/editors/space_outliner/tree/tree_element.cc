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

#include "DNA_listBase.h"

#include "tree_element_anim_data.hh"
#include "tree_element_collection.hh"
#include "tree_element_driver.hh"
#include "tree_element_gpencil_layer.hh"
#include "tree_element_id.hh"
#include "tree_element_nla.hh"
#include "tree_element_scene_objects.hh"
#include "tree_element_view_layer.hh"

#include "tree_element.h"
#include "tree_element.hh"

namespace blender::ed::outliner {

static AbstractTreeElement *tree_element_create(int type, TreeElement &legacy_te, void *idv)
{
  /* Would be nice to get rid of void * here, can we somehow expect the right type right away?
   * Perfect forwarding maybe, once the API is C++ only? */
  ID &id = *static_cast<ID *>(idv);

  switch (type) {
    case TSE_SOME_ID:
      return TreeElementID::createFromID(legacy_te, id);
    case TSE_ANIM_DATA:
      return new TreeElementAnimData(legacy_te, id);
    case TSE_DRIVER_BASE:
      return new TreeElementDriverBase(legacy_te, *static_cast<AnimData *>(idv));
    case TSE_NLA:
      return new TreeElementNLA(legacy_te, *static_cast<AnimData *>(idv));
    case TSE_NLA_TRACK:
      return new TreeElementNLATrack(legacy_te, *static_cast<NlaTrack *>(idv));
    case TSE_NLA_ACTION:
      return new TreeElementNLAAction(legacy_te);
    case TSE_GP_LAYER:
      return new TreeElementGPencilLayer(legacy_te, *static_cast<bGPDlayer *>(idv));
    case TSE_R_LAYER_BASE:
      return new TreeElementViewLayerBase(legacy_te, *static_cast<Scene *>(idv));
    case TSE_SCENE_COLLECTION_BASE:
      return new TreeElementCollectionBase(legacy_te, *static_cast<Scene *>(idv));
    case TSE_SCENE_OBJECTS_BASE:
      return new TreeElementSceneObjectsBase(legacy_te, *static_cast<Scene *>(idv));
    default:
      break;
  }

  return nullptr;
}

static void tree_element_free(AbstractTreeElement **tree_element)
{
  delete *tree_element;
  *tree_element = nullptr;
}

static void tree_element_expand(AbstractTreeElement &tree_element, SpaceOutliner &space_outliner)
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

/**
 * Needed for types that still expand in C, but need to execute the same post-expand logic. Can be
 * removed once all ID types expand entirely using the new design.
 */
static void tree_element_post_expand_only(AbstractTreeElement &tree_element,
                                          SpaceOutliner &space_outliner)
{
  tree_element.postExpand(space_outliner);
}
/**
 * Needed for types that still expand in C, to poll if they should expand in current context. Can
 * be removed once all ID types expand entirely using the new design.
 */
static bool tree_element_expand_poll(AbstractTreeElement &tree_element,
                                     SpaceOutliner &space_outliner)
{
  return tree_element.expandPoll(space_outliner);
}

}  // namespace blender::ed::outliner

namespace outliner = blender::ed::outliner;

TreeElementType *outliner_tree_element_type_create(int type, TreeElement *legacy_te, void *idv)
{
  outliner::AbstractTreeElement *element = outliner::tree_element_create(type, *legacy_te, idv);
  return reinterpret_cast<TreeElementType *>(element);
}

void outliner_tree_element_type_expand(TreeElementType *type, SpaceOutliner *space_outliner)
{
  outliner::tree_element_expand(reinterpret_cast<outliner::AbstractTreeElement &>(*type),
                                *space_outliner);
}
bool outliner_tree_element_type_is_expand_valid(TreeElementType *type)
{
  outliner::AbstractTreeElement &element = reinterpret_cast<outliner::AbstractTreeElement &>(
      *type);
  return element.isExpandValid();
}
bool outliner_tree_element_type_expand_poll(TreeElementType *type, SpaceOutliner *space_outliner)
{
  return outliner::tree_element_expand_poll(
      reinterpret_cast<outliner::AbstractTreeElement &>(*type), *space_outliner);
}
void outliner_tree_element_type_post_expand(TreeElementType *type, SpaceOutliner *space_outliner)
{
  outliner::tree_element_post_expand_only(reinterpret_cast<outliner::AbstractTreeElement &>(*type),
                                          *space_outliner);
}

void outliner_tree_element_type_free(TreeElementType **type)
{
  outliner::tree_element_free(reinterpret_cast<outliner::AbstractTreeElement **>(type));
}
