/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_operation.hh"

namespace blender {
class ComputeContext;
namespace bke {
class bNodeTreeZone;
}
}  // namespace blender

namespace blender::compositor {

class Context;
struct Schedule;

/* ------------------------------------------------------------------------------------------------
 * Zone Operation
 *
 * The zone operation is the abstract base class of zone operations. Its outputs match the type and
 * identifier of the outputs of the zone output node. Its inputs match the type and identifier of
 * the inputs of the zone input node. Finally it also has inputs representing links that cross the
 * border of the zone and are connected to outputs outside of the zone, those inputs match those of
 * the ZoneTreeOperation, see its description for more information. */
class ZoneOperation : public Operation {
 protected:
  /* The zone that this operation represents. */
  const bke::bNodeTreeZone &zone_;
  /* The compute context of the operation that uses the zone. */
  const ComputeContext &compute_context_;

 public:
  ZoneOperation(Context &context,
                const bke::bNodeTreeZone &zone,
                const ComputeContext &compute_context);

  /* Compute and set the initial reference counts of all the results of the operation. The node
   * execution schedule is given as an input. */
  void compute_results_reference_counts(const Schedule &schedule);

  const bke::bNodeTreeZone &zone() const;
};

}  // namespace blender::compositor
