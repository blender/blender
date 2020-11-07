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

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"

#include "BKE_collection.h"
#include "BKE_main.h"

#include "BLT_translation.h"

#include "../outliner_intern.h"
#include "tree_view.hh"

namespace blender {
namespace ed {
namespace outliner {

/* Convenience/readability. */
template<typename T> using List = ListBaseWrapper<T>;

TreeViewLibraries::TreeViewLibraries(SpaceOutliner &space_outliner)
    : AbstractTreeView(space_outliner)
{
}

static bool outliner_library_id_show(Library *lib, ID *id, short filter_id_type)
{
  if (id->lib != lib) {
    return false;
  }

  if (filter_id_type == ID_GR) {
    /* Don't show child collections of non-scene master collection,
     * they are already shown as children. */
    Collection *collection = (Collection *)id;
    bool has_non_scene_parent = false;

    for (CollectionParent *cparent : List<CollectionParent>(collection->parents)) {
      if (!(cparent->collection->flag & COLLECTION_IS_MASTER)) {
        has_non_scene_parent = true;
      }
    }

    if (has_non_scene_parent) {
      return false;
    }
  }

  return true;
}

static TreeElement *outliner_add_library_contents(Main *mainvar,
                                                  SpaceOutliner *space_outliner,
                                                  ListBase *lb,
                                                  Library *lib)
{
  TreeElement *ten, *tenlib = nullptr;
  ListBase *lbarray[MAX_LIBARRAY];
  int a, tot;
  short filter_id_type = (space_outliner->filter & SO_FILTER_ID_TYPE) ?
                             space_outliner->filter_id_type :
                             0;

  if (filter_id_type) {
    lbarray[0] = which_libbase(mainvar, space_outliner->filter_id_type);
    tot = 1;
  }
  else {
    tot = set_listbasepointers(mainvar, lbarray);
  }

  for (a = 0; a < tot; a++) {
    if (lbarray[a] && lbarray[a]->first) {
      ID *id = static_cast<ID *>(lbarray[a]->first);
      const bool is_library = (GS(id->name) == ID_LI) && (lib != nullptr);

      /* check if there's data in current lib */
      for (ID *id_iter : List<ID>(lbarray[a])) {
        if (id_iter->lib == lib) {
          id = id_iter;
          break;
        }
      }

      /* We always want to create an entry for libraries, even if/when we have no more IDs from
       * them. This invalid state is important to show to user as well.*/
      if (id != nullptr || is_library) {
        if (!tenlib) {
          /* Create library tree element on demand, depending if there are any data-blocks. */
          if (lib) {
            tenlib = outliner_add_element(space_outliner, lb, lib, nullptr, 0, 0);
          }
          else {
            tenlib = outliner_add_element(space_outliner, lb, mainvar, nullptr, TSE_ID_BASE, 0);
            tenlib->name = IFACE_("Current File");
          }
        }

        /* Create data-block list parent element on demand. */
        if (id != nullptr) {
          if (filter_id_type) {
            ten = tenlib;
          }
          else {
            ten = outliner_add_element(
                space_outliner, &tenlib->subtree, lbarray[a], nullptr, TSE_ID_BASE, 0);
            ten->directdata = lbarray[a];
            ten->name = outliner_idcode_to_plural(GS(id->name));
          }

          for (ID *id : List<ID>(lbarray[a])) {
            if (outliner_library_id_show(lib, id, filter_id_type)) {
              outliner_add_element(space_outliner, &ten->subtree, id, ten, 0, 0);
            }
          }
        }
      }
    }
  }

  return tenlib;
}

ListBase TreeViewLibraries::buildTree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};

  /* current file first - mainvar provides tselem with unique pointer - not used */
  TreeElement *ten = outliner_add_library_contents(
      source_data.bmain, &_space_outliner, &tree, nullptr);
  TreeStoreElem *tselem;

  if (ten) {
    tselem = TREESTORE(ten);
    if (!tselem->used) {
      tselem->flag &= ~TSE_CLOSED;
    }
  }

  for (ID *id : List<ID>(source_data.bmain->libraries)) {
    Library *lib = reinterpret_cast<Library *>(id);
    ten = outliner_add_library_contents(source_data.bmain, &_space_outliner, &tree, lib);
    /* NULL-check matters, due to filtering there may not be a new element. */
    if (ten) {
      lib->id.newid = (ID *)ten;
    }
  }
  /* make hierarchy */
  ten = static_cast<TreeElement *>(tree.first);
  if (ten != nullptr) {
    ten = ten->next; /* first one is main */
    while (ten) {
      TreeElement *nten = ten->next, *par;
      tselem = TREESTORE(ten);
      Library *lib = (Library *)tselem->id;
      if (lib && lib->parent) {
        par = (TreeElement *)lib->parent->id.newid;
        if (tselem->id->tag & LIB_TAG_INDIRECT) {
          /* Only remove from 'first level' if lib is not also directly used. */
          BLI_remlink(&tree, ten);
          BLI_addtail(&par->subtree, ten);
          ten->parent = par;
        }
        else {
          /* Else, make a new copy of the libtree for our parent. */
          TreeElement *dupten = outliner_add_library_contents(
              source_data.bmain, &_space_outliner, &par->subtree, lib);
          if (dupten) {
            dupten->parent = par;
          }
        }
      }
      ten = nten;
    }
  }
  /* restore newid pointers */
  for (ID *library_id : List<ID>(source_data.bmain->libraries)) {
    library_id->newid = nullptr;
  }

  return tree;
}

}  // namespace outliner
}  // namespace ed
}  // namespace blender
