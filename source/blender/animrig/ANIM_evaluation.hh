/* SPDX-FileCopyrightText: 2023 Blender Developers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Animation data-block evaluation.
 */
#pragma once

#include "DNA_anim_types.h"

#include "ANIM_animation.hh"

struct AnimationEvalContext;
struct PointerRNA;

namespace blender::animrig {

/**
 * Top level animation evaluation function.
 *
 * Animate the given ID, using the animation data-block and the given binding.
 *
 * \param flush_to_original when true, look up the original data-block (assuming
 * the given one is an evaluated copy) and update that too.
 */
void evaluate_and_apply_animation(PointerRNA &animated_id_ptr,
                                  Animation &animation,
                                  binding_handle_t binding_handle,
                                  const AnimationEvalContext &anim_eval_context,
                                  bool flush_to_original);

}  // namespace blender::animrig
