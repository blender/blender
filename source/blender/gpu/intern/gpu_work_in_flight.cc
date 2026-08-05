/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GPU_work_in_flight.hh"

#include "gpu_backend.hh"

namespace blender::gpu {

using namespace blender::gpu;

WorkInFlightPtr WorkInFlight::create(unsigned int max_in_flight)
{
  return WorkInFlightPtr(GPUBackend::get()->work_in_flight_alloc(max_in_flight));
}

void WorkInFlightDeleter::operator()(WorkInFlight *work_in_flight)
{
  delete work_in_flight;
}

}  // namespace blender::gpu
