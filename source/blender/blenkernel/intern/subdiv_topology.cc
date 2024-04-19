/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"

#include "BKE_subdiv_topology.hh"

#include "BKE_subdiv.hh"

#include "opensubdiv_topology_refiner_capi.hh"

namespace blender::bke::subdiv {

int topology_num_fvar_layers_get(const Subdiv *subdiv)
{
#ifdef WITH_OPENSUBDIV
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  return topology_refiner->getNumFVarChannels();
#else
  UNUSED_VARS(subdiv);
  return 0;
#endif
}

}  // namespace blender::bke::subdiv
