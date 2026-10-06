/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_map.hh"

#include "BLI_implicit_sharing.hh"
#include "BLI_implicit_sharing_ptr.hh"

namespace blender {
class ComputeContext;
namespace bke {
class bNodeTreeZone;
}
}  // namespace blender

namespace blender::compositor {

class Result;
class Closure;

using ClosurePtr = ImplicitSharingPtr<Closure>;

/* The representation of closures in the compositor. */
class Closure : public ImplicitSharingMixin {
 public:
  /* The zone that this operation represents. */
  const bke::bNodeTreeZone &zone;
  /* The compute context of where the zone is defined. */
  const ComputeContext &compute_context;
  /* A map of the values that the closure captures from outside of the zone at the point it was
   * defined, which corresponds to the external values of the zone tree operation, identified by
   * the identifier returned by ZoneTreeOperation::get_external_input_identifier. */
  Map<std::string, Result *> captured_values;

  Closure(const bke::bNodeTreeZone &zone,
          Map<std::string, Result *> captured_values,
          const ComputeContext &compute_context);

  /* Construct a new ClosurePtr from the given arguments using MEM_new. */
  static ClosurePtr create(const bke::bNodeTreeZone &zone,
                           Map<std::string, Result *> captured_values,
                           const ComputeContext &compute_context);

 private:
  /* Frees captured values and delete self. */
  void delete_self() override;
};

}  // namespace blender::compositor
