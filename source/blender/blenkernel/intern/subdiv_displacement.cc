/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv.hh"

#include "MEM_guardedalloc.h"

namespace blender::bke::subdiv {

void displacement_detach(Subdiv *subdiv)
{
  if (subdiv->displacement_evaluator == nullptr) {
    return;
  }
  if (subdiv->displacement_evaluator->free != nullptr) {
    subdiv->displacement_evaluator->free(subdiv->displacement_evaluator);
  }
  MEM_freeN(subdiv->displacement_evaluator);
  subdiv->displacement_evaluator = nullptr;
}

}  // namespace blender::bke::subdiv
