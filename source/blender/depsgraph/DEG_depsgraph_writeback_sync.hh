/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * This file provides an API that can be used to modify original (as opposed to evaluated)
 * data-blocks after depsgraph evaluation. For some data (e.g. animated properties), this is done
 * during depsgraph evaluation. However, this is not possible in all cases. For example, if the
 * change to the original data adds a new relation between data-blocks, a user-count (#ID.us) has
 * to be increased. This counter is not atomic and can therefore not be modified arbitrarily from
 * different threads.
 */

#include <functional>

struct Depsgraph;

namespace blender::deg::sync_writeback {

/**
 * Add a writeback task during depsgraph evaluation. The given function is called after depsgraph
 * evaluation is done if the depsgraph is active. It is allowed to change original data blocks and
 * even to add new relations.
 */
void add(Depsgraph &depsgraph, std::function<void()> fn);

}  // namespace blender::deg::sync_writeback
