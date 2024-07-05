/* SPDX-FileCopyrightText: 2023 Blender Developers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Layered Action evaluation.
 */
#pragma once

#include "DNA_anim_types.h"

#include "ANIM_action.hh"

struct AnimationEvalContext;
struct PointerRNA;

namespace blender::animrig {

/**
 * Top level animation evaluation function.
 *
 * Animate the given ID, using the layered Action and the given slot.
 *
 * \param flush_to_original: when true, look up the original data-block (assuming
 * the given one is an evaluated copy) and update that too.
 */
void evaluate_and_apply_action(PointerRNA &animated_id_ptr,
                               Action &action,
                               slot_handle_t slot_handle,
                               const AnimationEvalContext &anim_eval_context,
                               bool flush_to_original);

}  // namespace blender::animrig
