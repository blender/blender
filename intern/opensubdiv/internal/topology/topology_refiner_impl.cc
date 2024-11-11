/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "opensubdiv_topology_refiner.hh"

namespace blender::opensubdiv {

TopologyRefinerImpl::TopologyRefinerImpl() : topology_refiner(nullptr) {}

TopologyRefinerImpl::~TopologyRefinerImpl()
{
  delete topology_refiner;
}

}  // namespace blender::opensubdiv
