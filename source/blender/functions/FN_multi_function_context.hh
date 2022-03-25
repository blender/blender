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
  Map<std::string, const void *> global_contexts_;

  friend MFContext;

 public:
  template<typename T> void add_global_context(std::string name, const T *context)
  {
    global_contexts_.add_new(std::move(name), static_cast<const void *>(context));
  }
};

class MFContext {
 private:
  MFContextBuilder &builder_;

 public:
  MFContext(MFContextBuilder &builder) : builder_(builder)
  {
  }

  template<typename T> const T *get_global_context(StringRef name) const
  {
    const void *context = builder_.global_contexts_.lookup_default_as(name, nullptr);
    /* TODO: Implement type checking. */
    return static_cast<const T *>(context);
  }
};

}  // namespace blender::fn
