/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_timeit.hh"

#include "DNA_node_types.h"

#include "COM_profiler.hh"

namespace blender::compositor {

Map<bNodeInstanceKey, timeit::Nanoseconds> &Profiler::get_nodes_evaluation_times()
{
  return nodes_evaluation_times_;
}

void Profiler::set_node_evaluation_time(bNodeInstanceKey node_instance_key,
                                        timeit::Nanoseconds time)
{
  nodes_evaluation_times_.lookup_or_add(node_instance_key, timeit::Nanoseconds::zero()) += time;
}

}  // namespace blender::compositor
