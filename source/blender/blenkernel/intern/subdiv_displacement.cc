/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

void BKE_subdiv_displacement_detach(Subdiv *subdiv)
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
