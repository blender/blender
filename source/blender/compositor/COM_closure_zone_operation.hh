/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_map.hh"

#include "COM_result.hh"
#include "COM_zone_operation.hh"

namespace blender::bke {
class bNodeTreeZone;
}  // namespace blender::bke

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Closure Zone Operation
 *
 * An operation that returns a closure type representing a closure zone. */
class ClosureZoneOperation : public ZoneOperation {
 public:
  using ZoneOperation::ZoneOperation;

  void execute() override;

 private:
  /* Get a map of the values that the closure captures from outside of the zone, which corresponds
   * to the external values of the zone tree operation, identified by the identifier returned by
   * ZoneTreeOperation::get_external_input_identifier. */
  Map<std::string, Result *> get_captured_values();
};

}  // namespace blender::compositor
