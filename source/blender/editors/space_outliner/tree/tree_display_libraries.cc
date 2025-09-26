/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"

#include "BKE_collection.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"

#include "DNA_collection_types.h"
#include "DNA_space_types.h"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"
#include "common.hh"
#include "tree_display.hh"

namespace blender::ed::outliner {

template<typename T> using List = ListBaseWrapper<T>;

TreeDisplayLibraries::TreeDisplayLibraries(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplayLibraries::build_tree(const TreeSourceData &source_data)
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
    /* Null-check matters, due to filtering there may not be a new element. */
    if (ten) {
      lib->id.newid = (ID *)ten;
    }
  }

  /* Make hierarchy.
   *
   * Note: `List<T>` template is similar to `LISTBASE_FOREACH`, _not_ `LISTBASE_FOREACH_MUTABLE`,
   * so we need to iterate over an actual copy of the original list here, to avoid missing some
   * items. */
  for (TreeElement *ten : listbase_to_vector<TreeElement>(tree)) {
    if (ten == tree.first) {
      /* First item is main, skip. */
      continue;
    }

    TreeStoreElem *tselem = TREESTORE(ten);
    Library *lib = (Library *)tselem->id;
    BLI_assert(!lib || (GS(lib->id.name) == ID_LI));
    if (!lib || !(lib->runtime->parent || lib->archive_parent_library)) {
      continue;
    }

    /* A library with a non-null `parent` is always strictly indirectly linked. */
    TreeElement *parent = reinterpret_cast<TreeElement *>(
        (lib->archive_parent_library ? lib->archive_parent_library : lib->runtime->parent)
            ->id.newid);
    BLI_remlink(&tree, ten);
    BLI_addtail(&parent->subtree, ten);
    ten->parent = parent;
  }
  /* restore newid pointers */
  for (ID *library_id : List<ID>(source_data.bmain->libraries)) {
    library_id->newid = nullptr;
  }

  return tree;
}

TreeElement *TreeDisplayLibraries::add_library_contents(Main &mainvar, ListBase &lb, Library *lib)
{
  const short filter_id_type = id_filter_get();

  Vector<ListBase *> lbarray;
  if (filter_id_type) {
    lbarray.append(which_libbase(&mainvar, space_outliner_.filter_id_type));
  }
  else {
    lbarray.extend(BKE_main_lists_get(mainvar));
  }

  TreeElement *tenlib = nullptr;
  for (int a = 0; a < lbarray.size(); a++) {
    if (!lbarray[a] || !lbarray[a]->first) {
      continue;
    }

    ID *id = static_cast<ID *>(lbarray[a]->first);
    const bool is_library = (GS(id->name) == ID_LI) && (lib != nullptr);

    /* Don't show deprecated types. */
    if (ID_TYPE_IS_DEPRECATED(GS(id->name))) {
      continue;
    }

    /* check if there's data in current lib */
    for (ID *id_iter : List<ID>(lbarray[a])) {
      if (id_iter->lib == lib) {
        id = id_iter;
        break;
      }
    }

    /* We always want to create an entry for libraries, even if/when we have no more IDs from them.
     * This invalid state is important to show to user as well. */
    if (id != nullptr || is_library) {
      if (!tenlib) {
        /* Create library tree element on demand, depending if there are any data-blocks. */
        if (lib) {
          tenlib = add_element(&lb, reinterpret_cast<ID *>(lib), nullptr, nullptr, TSE_SOME_ID, 0);
        }
        else {
          tenlib = add_element(&lb, nullptr, &mainvar, nullptr, TSE_ID_BASE, 0);
          tenlib->name = IFACE_("Current File");
        }
      }

      /* Create data-block list parent element on demand. */
      if (id != nullptr) {
        TreeElement *ten;

        if (filter_id_type) {
          ten = tenlib;
        }
        else if (id->lib == lib) {
          ten = add_element(
              &tenlib->subtree, reinterpret_cast<ID *>(lib), nullptr, nullptr, TSE_ID_BASE, a);
          ten->directdata = lbarray[a];
          ten->name = outliner_idcode_to_plural(GS(id->name));
        }

        for (ID *id : List<ID>(lbarray[a])) {
          if (library_id_filter_poll(lib, id)) {
            add_element(&ten->subtree, id, nullptr, ten, TSE_SOME_ID, 0);
          }
        }
      }
    }
  }

  return tenlib;
}

short TreeDisplayLibraries::id_filter_get() const
{
  if (space_outliner_.filter & SO_FILTER_ID_TYPE) {
    return space_outliner_.filter_id_type;
  }
  return 0;
}

bool TreeDisplayLibraries::library_id_filter_poll(const Library *lib, ID *id) const
{
  if (id->lib != lib) {
    return false;
  }

  if (id_filter_get() == ID_GR) {
    /* Don't show child collections of non-scene master collection,
     * they are already shown as children. */
    Collection *collection = (Collection *)id;
    bool has_non_scene_parent = false;

    for (CollectionParent *cparent : List<CollectionParent>(collection->runtime->parents)) {
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

}  // namespace blender::ed::outliner
