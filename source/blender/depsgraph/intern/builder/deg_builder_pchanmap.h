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
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/depsgraph_type.h"

namespace blender {
namespace deg {

struct RootPChanMap {
  /* Constructor and destructor - Create and free the internal map respectively. */
  RootPChanMap();
  ~RootPChanMap();

  /* Debug contents of map. */
  void print_debug();

  /* Add a mapping. */
  void add_bone(const char *bone, const char *root);

  /* Check if there's a common root bone between two bones. */
  bool has_common_root(const char *bone1, const char *bone2) const;

 protected:
  /**
   * The strings are only referenced by this map. Users of RootPChanMap have to make sure that the
   * life-time of the strings is long enough.
   */
  Map<StringRefNull, Set<StringRefNull>> map_;
};

}  // namespace deg
}  // namespace blender
