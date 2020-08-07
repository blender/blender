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

#include "IO_dupli_persistent_id.hh"

#include "BKE_duplilist.h"

#include <map>
#include <set>

namespace blender::io {

/* Find relations between duplicated objects. This class should be instanced for a single real
 * object, and fed its dupli-objects. */
class DupliParentFinder final {
 private:
  /* To check whether an Object * is instanced by this duplicator. */
  std::set<const Object *> dupli_set_;

  /* To find the DupliObject given its Persistent ID. */
  typedef std::map<const PersistentID, const DupliObject *> PIDToDupliMap;
  PIDToDupliMap pid_to_dupli_;

  /* Mapping from instancer PID to duplis instanced by it. */
  typedef std::map<const PersistentID, std::set<const DupliObject *>> InstancerPIDToDuplisMap;
  InstancerPIDToDuplisMap instancer_pid_to_duplis_;

 public:
  DupliParentFinder();
  ~DupliParentFinder();

  void insert(const DupliObject *dupli_ob);

  bool is_duplicated(const Object *object) const;
  const DupliObject *find_suitable_export_parent(const DupliObject *dupli_ob) const;

 private:
  const DupliObject *find_duplicated_parent(const DupliObject *dupli_ob) const;
  const DupliObject *find_instancer(const DupliObject *dupli_ob) const;
};

}  // namespace blender::io
