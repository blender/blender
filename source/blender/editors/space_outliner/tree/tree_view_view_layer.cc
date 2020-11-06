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

#include <iostream>

#include "DNA_scene_types.h"

#include "BKE_layer.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "MEM_guardedalloc.h"

#include "../outliner_intern.h"
#include "tree_view.hh"

namespace blender {
namespace outliner {

/**
 * For all objects in the tree, lookup the parent in this map,
 * and move or add tree elements as needed.
 */
static void outliner_make_object_parent_hierarchy_collections(SpaceOutliner *space_outliner,
                                                              GHash *object_tree_elements_hash)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, object_tree_elements_hash) {
    Object *child = static_cast<Object *>(BLI_ghashIterator_getKey(&gh_iter));

    if (child->parent == NULL) {
      continue;
    }

    ListBase *child_ob_tree_elements = static_cast<ListBase *>(
        BLI_ghashIterator_getValue(&gh_iter));
    ListBase *parent_ob_tree_elements = static_cast<ListBase *>(
        BLI_ghash_lookup(object_tree_elements_hash, child->parent));
    if (parent_ob_tree_elements == NULL) {
      continue;
    }

    LISTBASE_FOREACH (LinkData *, link, parent_ob_tree_elements) {
      TreeElement *parent_ob_tree_element = static_cast<TreeElement *>(link->data);
      TreeElement *parent_ob_collection_tree_element = NULL;
      bool found = false;

      /* We always want to remove the child from the direct collection its parent is nested under.
       * This is particularly important when dealing with multi-level nesting (grandchildren). */
      parent_ob_collection_tree_element = parent_ob_tree_element->parent;
      while (!ELEM(TREESTORE(parent_ob_collection_tree_element)->type,
                   TSE_VIEW_COLLECTION_BASE,
                   TSE_LAYER_COLLECTION)) {
        parent_ob_collection_tree_element = parent_ob_collection_tree_element->parent;
      }

      LISTBASE_FOREACH (LinkData *, link_iter, child_ob_tree_elements) {
        TreeElement *child_ob_tree_element = static_cast<TreeElement *>(link_iter->data);

        if (child_ob_tree_element->parent == parent_ob_collection_tree_element) {
          /* Move from the collection subtree into the parent object subtree. */
          BLI_remlink(&parent_ob_collection_tree_element->subtree, child_ob_tree_element);
          BLI_addtail(&parent_ob_tree_element->subtree, child_ob_tree_element);
          child_ob_tree_element->parent = parent_ob_tree_element;
          found = true;
          break;
        }
      }

      if (!found) {
        /* We add the child in the tree even if it is not in the collection.
         * We deliberately clear its sub-tree though, to make it less prominent. */
        TreeElement *child_ob_tree_element = outliner_add_element(
            space_outliner, &parent_ob_tree_element->subtree, child, parent_ob_tree_element, 0, 0);
        outliner_free_tree(&child_ob_tree_element->subtree);
        child_ob_tree_element->flag |= TE_CHILD_NOT_IN_COLLECTION;
        BLI_addtail(child_ob_tree_elements, BLI_genericNodeN(child_ob_tree_element));
      }
    }
  }
}

/**
 * Build a map from Object* to a list of TreeElement* matching the object.
 */
static void outliner_object_tree_elements_lookup_create_recursive(GHash *object_tree_elements_hash,
                                                                  TreeElement *te_parent)
{
  LISTBASE_FOREACH (TreeElement *, te, &te_parent->subtree) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (tselem->type == TSE_LAYER_COLLECTION) {
      outliner_object_tree_elements_lookup_create_recursive(object_tree_elements_hash, te);
    }
    else if (tselem->type == 0 && te->idcode == ID_OB) {
      Object *ob = (Object *)tselem->id;
      ListBase *tree_elements = static_cast<ListBase *>(
          BLI_ghash_lookup(object_tree_elements_hash, ob));

      if (tree_elements == NULL) {
        tree_elements = static_cast<ListBase *>(MEM_callocN(sizeof(ListBase), __func__));
        BLI_ghash_insert(object_tree_elements_hash, ob, tree_elements);
      }

      BLI_addtail(tree_elements, BLI_genericNodeN(te));
      outliner_object_tree_elements_lookup_create_recursive(object_tree_elements_hash, te);
    }
  }
}

static void outliner_object_tree_elements_lookup_free(GHash *object_tree_elements_hash)
{
  GHASH_FOREACH_BEGIN (ListBase *, tree_elements, object_tree_elements_hash) {
    BLI_freelistN(tree_elements);
    MEM_freeN(tree_elements);
  }
  GHASH_FOREACH_END();
}

static void outliner_add_layer_collection_objects(SpaceOutliner *space_outliner,
                                                  ListBase *tree,
                                                  ViewLayer *layer,
                                                  LayerCollection *lc,
                                                  TreeElement *ten)
{
  LISTBASE_FOREACH (CollectionObject *, cob, &lc->collection->gobject) {
    Base *base = BKE_view_layer_base_find(layer, cob->ob);
    TreeElement *te_object = outliner_add_element(space_outliner, tree, base->object, ten, 0, 0);
    te_object->directdata = base;

    if (!(base->flag & BASE_VISIBLE_VIEWLAYER)) {
      te_object->flag |= TE_DISABLED;
    }
  }
}

static void outliner_add_layer_collections_recursive(SpaceOutliner *space_outliner,
                                                     ListBase *tree,
                                                     ViewLayer *layer,
                                                     ListBase *layer_collections,
                                                     TreeElement *parent_ten,
                                                     const bool show_objects)
{
  LISTBASE_FOREACH (LayerCollection *, lc, layer_collections) {
    const bool exclude = (lc->flag & LAYER_COLLECTION_EXCLUDE) != 0;
    TreeElement *ten;

    if (exclude && ((space_outliner->show_restrict_flags & SO_RESTRICT_ENABLE) == 0)) {
      ten = parent_ten;
    }
    else {
      ID *id = &lc->collection->id;
      ten = outliner_add_element(space_outliner, tree, id, parent_ten, TSE_LAYER_COLLECTION, 0);

      ten->name = id->name + 2;
      ten->directdata = lc;

      /* Open by default, except linked collections, which may contain many elements. */
      TreeStoreElem *tselem = TREESTORE(ten);
      if (!(tselem->used || ID_IS_LINKED(id) || ID_IS_OVERRIDE_LIBRARY(id))) {
        tselem->flag &= ~TSE_CLOSED;
      }

      if (exclude || (lc->runtime_flag & LAYER_COLLECTION_VISIBLE_VIEW_LAYER) == 0) {
        ten->flag |= TE_DISABLED;
      }
    }

    outliner_add_layer_collections_recursive(
        space_outliner, &ten->subtree, layer, &lc->layer_collections, ten, show_objects);
    if (!exclude && show_objects) {
      outliner_add_layer_collection_objects(space_outliner, &ten->subtree, layer, lc, ten);
    }
  }
}

static void outliner_add_view_layer(SpaceOutliner *space_outliner,
                                    ListBase *tree,
                                    TreeElement *parent,
                                    ViewLayer *layer,
                                    const bool show_objects)
{
  /* First layer collection is for master collection, don't show it. */
  LayerCollection *lc = static_cast<LayerCollection *>(layer->layer_collections.first);
  if (lc == NULL) {
    return;
  }

  outliner_add_layer_collections_recursive(
      space_outliner, tree, layer, &lc->layer_collections, parent, show_objects);
  if (show_objects) {
    outliner_add_layer_collection_objects(space_outliner, tree, layer, lc, parent);
  }
}

Tree TreeViewViewLayer::buildTree(const TreeSourceData &source_data, SpaceOutliner &space_outliner)
{
  Tree tree = {nullptr};

  if (space_outliner.filter & SO_FILTER_NO_COLLECTION) {
    /* Show objects in the view layer. */
    LISTBASE_FOREACH (Base *, base, &source_data.view_layer->object_bases) {
      TreeElement *te_object = outliner_add_element(
          &space_outliner, &tree, base->object, nullptr, 0, 0);
      te_object->directdata = base;
    }

    if ((space_outliner.filter & SO_FILTER_NO_CHILDREN) == 0) {
      outliner_make_object_parent_hierarchy(&tree);
    }
  }
  else {
    /* Show collections in the view layer. */
    TreeElement *ten = outliner_add_element(
        &space_outliner, &tree, source_data.scene, nullptr, TSE_VIEW_COLLECTION_BASE, 0);
    ten->name = IFACE_("Scene Collection");
    TREESTORE(ten)->flag &= ~TSE_CLOSED;

    bool show_objects = !(space_outliner.filter & SO_FILTER_NO_OBJECT);
    outliner_add_view_layer(
        &space_outliner, &ten->subtree, ten, source_data.view_layer, show_objects);

    if ((space_outliner.filter & SO_FILTER_NO_CHILDREN) == 0) {
      GHash *object_tree_elements_hash = BLI_ghash_new(
          BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
      outliner_object_tree_elements_lookup_create_recursive(object_tree_elements_hash, ten);
      outliner_make_object_parent_hierarchy_collections(&space_outliner,
                                                        object_tree_elements_hash);
      outliner_object_tree_elements_lookup_free(object_tree_elements_hash);
      BLI_ghash_free(object_tree_elements_hash, nullptr, nullptr);
    }
  }

  return tree;
}

}  // namespace outliner
}  // namespace blender
