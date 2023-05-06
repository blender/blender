/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_topology.h"

#include "BKE_subdiv.h"

#include "opensubdiv_topology_refiner_capi.h"

int BKE_subdiv_topology_num_fvar_layers_get(const Subdiv *subdiv)
{
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  return topology_refiner->getNumFVarChannels(topology_refiner);
}
