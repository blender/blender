/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* Convolves the given color input by the given float or color kernel and write the result to the
 * given output. If normalize_kernel is true, the kernel will be normalized such that it integrates
 * to 1. The output will be allocated internally and is thus expected not to be previously
 * allocated. */
void convolve(Context &context,
              const Result &input,
              const Result &kernel,
              Result &output,
              const bool normalize_kernel);

}  // namespace blender::compositor
