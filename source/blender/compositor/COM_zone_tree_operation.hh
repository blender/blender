/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "COM_operation.hh"

namespace blender {
struct bNodeSocket;
class ComputeContext;
namespace bke {
class bNodeTreeZone;
}
}  // namespace blender

namespace blender::compositor {

class Context;

/* ------------------------------------------------------------------------------------------------
 * Zone Tree Operation
 *
 * The zone tree operation represents and evaluates the internal node tree of a zone. Its outputs
 * match the type and identifier of the inputs of the zone output node. Its inputs match the type
 * and identifier of the outputs of the zone input node. Finally it also has inputs representing
 * links that cross the border of the zone and are connected to outputs outside of the zone, those
 * inputs have identifiers returned by get_external_input_identifier for outputs outside of the
 * zone that are connected to inputs inside the zone. */
class ZoneTreeOperation : public Operation {
 private:
  /* The zone that this operation represents. */
  const bke::bNodeTreeZone &zone_;
  /* The compute context of the zone using this zone tree operation. */
  const ComputeContext &compute_context_;

 public:
  ZoneTreeOperation(Context &context,
                    const bke::bNodeTreeZone &zone,
                    const ComputeContext &compute_context);

  void execute() override;

  /* Returns the identifier of the input declared for the given output that belongs to a node that
   * is outside of the zone. */
  static std::string get_external_input_identifier(const bNodeSocket &output);
};

}  // namespace blender::compositor
