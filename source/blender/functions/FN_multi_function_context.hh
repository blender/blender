/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * An #MFContext is passed along with every call to a multi-function. Right now it does nothing,
 * but it can be used for the following purposes:
 * - Pass debug information up and down the function call stack.
 * - Pass reusable memory buffers to sub-functions to increase performance.
 * - Pass cached data to called functions.
 */

#include "BLI_utildefines.h"

#include "BLI_map.hh"

namespace blender::fn {

class MFContext;

class MFContextBuilder {
 private:
  friend MFContext;

 public:
};

class MFContext {
 private:
  MFContextBuilder &builder_;

 public:
  MFContext(MFContextBuilder &builder) : builder_(builder)
  {
  }
};

}  // namespace blender::fn
