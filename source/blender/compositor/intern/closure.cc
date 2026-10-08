/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_closure.hh"
#include "COM_result.hh"

namespace blender::compositor {

Closure::Closure(const bke::bNodeTreeZone &zone,
                 Map<std::string, Result *> captured_values,
                 const ComputeContext &compute_context)
    : zone(zone), compute_context(compute_context), captured_values(captured_values)
{
}

ClosurePtr Closure::create(const bke::bNodeTreeZone &zone,
                           Map<std::string, Result *> captured_values,
                           const ComputeContext &compute_context)
{
  return ClosurePtr(MEM_new<Closure>(__func__, zone, captured_values, compute_context));
}

void Closure::delete_self()
{
  for (Result *result : captured_values.values()) {
    result->release();
    MEM_delete(result);
  }
  MEM_delete(this);
}

}  // namespace blender::compositor
