// Copyright 2018 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#include "opensubdiv_topology_refiner_capi.h"

#include <cstddef>

OpenSubdiv_TopologyRefiner* openSubdiv_createTopologyRefinerFromConverter(
    OpenSubdiv_Converter* /*converter*/,
    const OpenSubdiv_TopologyRefinerSettings* /*settings*/) {
  return NULL;     
}

void openSubdiv_deleteTopologyRefiner(
    OpenSubdiv_TopologyRefiner* /*topology_refiner*/) {
}

bool openSubdiv_topologyRefinerCompareWithConverter(
    const OpenSubdiv_TopologyRefiner* /*topology_refiner*/,
    const OpenSubdiv_Converter* /*converter*/) {
  return false;
}
