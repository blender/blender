/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functionality to iterate an Action in various ways.
 */

#pragma once

#include <cstdint>

#include "BLI_vector.hh"
#include "DNA_action_types.h"

struct FCurve;
namespace blender::animrig {
class Action;
class Layer;
class Strip;
class ChannelBag;
}  // namespace blender::animrig

namespace blender::animrig {

using slot_handle_t = decltype(::ActionSlot::handle);

/**
 * Iterates over all FCurves of the given slot handle in the Action and executes the callback on
 * it. Only works on layered Actions.
 *
 * \note Use lambdas to have access to specific data in the callback.
 */
void action_foreach_fcurve(Action &action,
                           slot_handle_t handle,
                           FunctionRef<void(FCurve &fcurve)> callback);

}  // namespace blender::animrig
