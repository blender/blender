/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_stack.h"

#include <iomanip>
#include <ios>
#include <iostream>

#include "BKE_idtype.h"

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"

namespace blender::deg {

/* Spacing between adjacent columns, in number of spaces. */
constexpr int kColumnSpacing = 4;

/* Width of table columns including column padding.
 * The type column width is a guesstimate based on "Particle Settings" with some extra padding. */
constexpr int kPrintDepthWidth = 5 + kColumnSpacing;
constexpr int kPrintTypeWidth = 21 + kColumnSpacing;

namespace {

/* NOTE: Depth column printing is already taken care of. */

void print(std::ostream &stream, const ID *id)
{
  const IDTypeInfo *id_type_info = BKE_idtype_get_info_from_id(id);
  stream << std::setw(kPrintTypeWidth) << id_type_info->name << (id->name + 2) << "\n";
}

void print(std::ostream &stream, const bConstraint *constraint)
{
  stream << std::setw(kPrintTypeWidth) << ("Constraint") << constraint->name << "\n";
}

void print(std::ostream &stream, const ModifierData *modifier_data)
{
  stream << std::setw(kPrintTypeWidth) << ("Modifier") << modifier_data->name << "\n";
}

void print(std::ostream &stream, const bPoseChannel *pchan)
{
  stream << std::setw(kPrintTypeWidth) << ("Pose Channel") << pchan->name << "\n";
}

}  // namespace

void BuilderStack::print_backtrace(std::ostream &stream)
{
  const std::ios_base::fmtflags old_flags(stream.flags());

  stream << std::left;

  stream << std::setw(kPrintDepthWidth) << "Depth" << std::setw(kPrintTypeWidth) << "Type"
         << "Name"
         << "\n";

  stream << std::setw(kPrintDepthWidth) << "-----" << std::setw(kPrintTypeWidth) << "----"
         << "----"
         << "\n";

  int depth = 1;
  for (const Entry &entry : stack_) {
    stream << std::setw(kPrintDepthWidth) << depth;
    ++depth;

    if (entry.id_ != nullptr) {
      print(stream, entry.id_);
    }
    else if (entry.constraint_ != nullptr) {
      print(stream, entry.constraint_);
    }
    else if (entry.modifier_data_ != nullptr) {
      print(stream, entry.modifier_data_);
    }
    else if (entry.pchan_ != nullptr) {
      print(stream, entry.pchan_);
    }
  }

  stream.flags(old_flags);
}

}  // namespace blender::deg
