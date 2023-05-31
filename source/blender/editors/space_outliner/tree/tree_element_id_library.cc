/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLT_translation.h"

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

StringRefNull TreeElementIDLibrary::getWarning() const
{
  Library &library = reinterpret_cast<Library &>(id_);

  if (library.tag & LIBRARY_TAG_RESYNC_REQUIRED) {
    return TIP_(
        "Contains linked library overrides that need to be resynced, updating the library is "
        "recommended");
  }

  if (library.id.tag & LIB_TAG_MISSING) {
    return TIP_("Missing library");
  }

  return {};
}

}  // namespace blender::ed::outliner
