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

#include "../outliner_intern.h"

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
