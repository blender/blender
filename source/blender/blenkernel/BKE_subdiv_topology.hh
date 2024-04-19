/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

namespace blender::bke::subdiv {

struct Subdiv;

int topology_num_fvar_layers_get(const Subdiv *subdiv);

}  // namespace blender::bke::subdiv
