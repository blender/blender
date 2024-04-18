/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "internal/topology/topology_refiner_impl.h"

namespace blender::opensubdiv {

TopologyRefinerImpl::TopologyRefinerImpl() : topology_refiner(nullptr) {}

TopologyRefinerImpl::~TopologyRefinerImpl()
{
  delete topology_refiner;
}

}  // namespace blender::opensubdiv
