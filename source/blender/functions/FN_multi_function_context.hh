/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#include "FN_user_data.hh"

namespace blender::fn::multi_function {

class Context;
class ContextBuilder;

class Context {
 public:
  /**
   * Custom user data that can be used in the function.
   */
  UserData *user_data = nullptr;

  friend ContextBuilder;

 private:
  Context() = default;

 public:
  Context(ContextBuilder & /*builder*/);
};

class ContextBuilder {
 private:
  Context context_;

  friend Context;

 public:
  void user_data(UserData *user_data)
  {
    context_.user_data = user_data;
  }
};

inline Context::Context(ContextBuilder &builder)
{
  *this = builder.context_;
}

}  // namespace blender::fn::multi_function
