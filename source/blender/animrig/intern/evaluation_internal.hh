/* SPDX-FileCopyrightText: 2024 Blender Developers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::animrig::internal {

/**
 * Evaluate the animation data on the given layer, for the given slot. This
 * just returns the evaluation result, without taking any other layers,
 * blending, influence, etc. into account.
 */
EvaluationResult evaluate_layer(PointerRNA &animated_id_ptr,
                                Layer &layer,
                                slot_handle_t slot_handle,
                                const AnimationEvalContext &anim_eval_context);

}  // namespace blender::animrig::internal
