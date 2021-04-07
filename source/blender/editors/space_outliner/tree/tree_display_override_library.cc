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

#include "DNA_collection_types.h"

#include "BLT_translation.h"

#include "../outliner_intern.h"
#include "tree_display.hh"

namespace blender::ed::outliner {

/* Convenience/readability. */
template<typename T> using List = ListBaseWrapper<T>;

TreeDisplayOverrideLibrary::TreeDisplayOverrideLibrary(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplayOverrideLibrary::buildTree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};

  {
    /* current file first - mainvar provides tselem with unique pointer - not used */
    TreeElement *ten = add_library_contents(*source_data.bmain, tree, nullptr);
    TreeStoreElem *tselem;

    if (ten) {
      tselem = TREESTORE(ten);
      if (!tselem->used) {
        tselem->flag &= ~TSE_CLOSED;
      }
    }
  }

  for (ID *id : List<ID>(source_data.bmain->libraries)) {
    Library *lib = reinterpret_cast<Library *>(id);
    TreeElement *ten = add_library_contents(*source_data.bmain, tree, lib);
    /* NULL-check matters, due to filtering there may not be a new element. */
    if (ten) {
      lib->id.newid = (ID *)ten;
    }
  }

  /* make hierarchy */
  for (TreeElement *ten : List<TreeElement>(tree)) {
    if (ten == tree.first) {
      /* First item is main, skip. */
      continue;
    }

    TreeStoreElem *tselem = TREESTORE(ten);
    Library *lib = (Library *)tselem->id;
    BLI_assert(!lib || (GS(lib->id.name) == ID_LI));
    if (!lib || !lib->parent) {
      continue;
    }

    TreeElement *parent = (TreeElement *)lib->parent->id.newid;

    if (tselem->id->tag & LIB_TAG_INDIRECT) {
      /* Only remove from 'first level' if lib is not also directly used. */
      BLI_remlink(&tree, ten);
      BLI_addtail(&parent->subtree, ten);
      ten->parent = parent;
    }
    else {
      /* Else, make a new copy of the libtree for our parent. */
      TreeElement *dupten = add_library_contents(*source_data.bmain, parent->subtree, lib);
      if (dupten) {
        dupten->parent = parent;
      }
    }
  }
  /* restore newid pointers */
  for (ID *library_id : List<ID>(source_data.bmain->libraries)) {
    library_id->newid = nullptr;
  }

  return tree;
}

TreeElement *TreeDisplayOverrideLibrary::add_library_contents(Main &mainvar,
                                                              ListBase &lb,
                                                              Library *lib) const
{
  const short filter_id_type = id_filter_get();

  ListBase *lbarray[INDEX_ID_MAX];
  int tot;
  if (filter_id_type) {
    lbarray[0] = which_libbase(&mainvar, space_outliner_.filter_id_type);
    tot = 1;
  }
  else {
    tot = set_listbasepointers(&mainvar, lbarray);
  }

  TreeElement *tenlib = nullptr;
  for (int a = 0; a < tot; a++) {
    if (!lbarray[a] || !lbarray[a]->first) {
      continue;
    }

    ID *id = nullptr;

    /* check if there's data in current lib */
    for (ID *id_iter : List<ID>(lbarray[a])) {
      if (id_iter->lib == lib && ID_IS_OVERRIDE_LIBRARY_REAL(id_iter)) {
        id = id_iter;
        break;
      }
    }

    if (id != nullptr) {
      if (!tenlib) {
        /* Create library tree element on demand, depending if there are any data-blocks. */
        if (lib) {
          tenlib = outliner_add_element(&space_outliner_, &lb, lib, nullptr, TSE_SOME_ID, 0);
        }
        else {
          tenlib = outliner_add_element(&space_outliner_, &lb, &mainvar, nullptr, TSE_ID_BASE, 0);
          tenlib->name = IFACE_("Current File");
        }
      }

      /* Create data-block list parent element on demand. */
      if (id != nullptr) {
        TreeElement *ten;

        if (filter_id_type) {
          ten = tenlib;
        }
        else {
          ten = outliner_add_element(
              &space_outliner_, &tenlib->subtree, lbarray[a], nullptr, TSE_ID_BASE, 0);
          ten->directdata = lbarray[a];
          ten->name = outliner_idcode_to_plural(GS(id->name));
        }

        for (ID *id : List<ID>(lbarray[a])) {
          if (override_library_id_filter_poll(lib, id)) {
            TreeElement *override_tree_element = outliner_add_element(
                &space_outliner_, &ten->subtree, id, ten, TSE_LIBRARY_OVERRIDE_BASE, 0);

            if (BLI_listbase_is_empty(&override_tree_element->subtree)) {
              outliner_free_tree_element(override_tree_element, &ten->subtree);
            }
          }
        }
      }
    }
  }

  return tenlib;
}

short TreeDisplayOverrideLibrary::id_filter_get() const
{
  if (space_outliner_.filter & SO_FILTER_ID_TYPE) {
    return space_outliner_.filter_id_type;
  }
  return 0;
}

bool TreeDisplayOverrideLibrary::override_library_id_filter_poll(Library *lib, ID *id) const
{
  if (id->lib != lib) {
    return false;
  }

  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    return false;
  }

  return true;
}

}  // namespace blender::ed::outliner
