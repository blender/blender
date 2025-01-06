/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/**
 * Gamma corrects the inputs in its straight alpha form and writes the result to the output. The
 * gamma factor is assumes to be 2. The output will be allocated internally and is thus expected
 * not to be previously allocated.
 */
void gamma_correct(Context &context, const Result &input, Result &output);

/**
 * Gamma un-corrects the inputs in its straight alpha form and writes the result to the output. The
 * gamma factor is assumes to be 2. The output will be allocated internally and is thus expected
 * not to be previously allocated.
 */
void gamma_uncorrect(Context &context, const Result &input, Result &output);

}  // namespace blender::compositor
