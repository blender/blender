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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BLI_string_ref.hh"

#include "RNA_types.h"

#include "intern/builder/deg_builder_relations.h"

struct FCurve;

namespace blender {
namespace deg {

/* Helper class for determining which relations are needed between driver evaluation nodes. */
class DriverDescriptor {
 public:
  /* Drivers are grouped by their RNA prefix. The prefix is the part of the RNA
   * path up to the last dot, the suffix is the remainder of the RNA path:
   *
   * fcu->rna_path                     rna_prefix              rna_suffix
   * -------------------------------   ----------------------  ----------
   * 'color'                           ''                      'color'
   * 'rigidbody_world.time_scale'      'rigidbody_world'       'time_scale'
   * 'pose.bones["master"].location'   'pose.bones["master"]'  'location'
   */
  StringRef rna_prefix;
  StringRef rna_suffix;

 public:
  DriverDescriptor(PointerRNA *id_ptr, FCurve *fcu);

  bool driver_relations_needed() const;
  bool is_array() const;
  /* Assumes that 'other' comes from the same RNA group, that is, has the same RNA path prefix. */
  bool is_same_array_as(const DriverDescriptor &other) const;
  OperationKey depsgraph_key() const;

 private:
  PointerRNA *id_ptr_;
  FCurve *fcu_;
  bool driver_relations_needed_;

  PointerRNA pointer_rna_;
  PropertyRNA *property_rna_;
  bool is_array_;

  bool determine_relations_needed();
  void split_rna_path();
  bool resolve_rna();
};

}  // namespace deg
}  // namespace blender
