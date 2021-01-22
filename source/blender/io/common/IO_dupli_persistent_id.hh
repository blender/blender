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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */
#pragma once

#include "BKE_duplilist.h"

#include "DNA_object_types.h" /* For MAX_DUPLI_RECUR */

#include <array>
#include <optional>
#include <ostream>

namespace blender::io {

/* Wrapper for DupliObject::persistent_id that can act as a map key. */
class PersistentID {
 protected:
  constexpr static int array_length_ = MAX_DUPLI_RECUR;
  typedef std::array<int, array_length_> PIDArray;
  PIDArray persistent_id_;

  explicit PersistentID(const PIDArray &persistent_id_values);

 public:
  PersistentID();
  explicit PersistentID(const DupliObject *dupli_ob);

  /* Return true if the persistent IDs are the same, ignoring the first digit. */
  bool is_from_same_instancer_as(const PersistentID &other) const;

  /* Construct the persistent ID of this instance's instancer. */
  PersistentID instancer_pid() const;

  /* Construct a string representation by reversing the persistent ID.
   * In case of a duplicator that is duplicated itself as well, this
   * results in strings like:
   * "3" for the duplicated duplicator, and
   * "3-0", "3-1", etc. for its duplis. */
  std::string as_object_name_suffix() const;

  friend bool operator==(const PersistentID &persistent_id_a, const PersistentID &persistent_id_b);
  friend bool operator<(const PersistentID &persistent_id_a, const PersistentID &persistent_id_b);
  friend std::ostream &operator<<(std::ostream &os, const PersistentID &persistent_id);

 private:
  void copy_values_from(const PIDArray &persistent_id_values);
};

}  // namespace blender::io
