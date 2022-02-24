/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_listBase.h"

#include "../outliner_intern.hh"

#include "tree_element_id_library.hh"

namespace blender::ed::outliner {

TreeElementIDLibrary::TreeElementIDLibrary(TreeElement &legacy_te, Library &library)
    : TreeElementID(legacy_te, library.id)
{
  legacy_te.name = library.filepath;
}

bool TreeElementIDLibrary::isExpandValid() const
{
  return true;
}

}  // namespace blender::ed::outliner
