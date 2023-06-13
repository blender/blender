/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * An #Context is passed along with every call to a multi-function. Right now it does nothing,
 * but it can be used for the following purposes:
 * - Pass debug information up and down the function call stack.
 * - Pass reusable memory buffers to sub-functions to increase performance.
 * - Pass cached data to called functions.
 */

#include "BLI_utildefines.h"

#include "BLI_map.hh"

namespace blender::fn::multi_function {

class Context;

class ContextBuilder {
};

class Context {
 public:
  Context(ContextBuilder & /*builder*/) {}
};

}  // namespace blender::fn::multi_function
