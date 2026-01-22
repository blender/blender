/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#pragma once

#include "lexit/lexit.hh"

namespace blender::gpu::shader::parser {

using namespace lexit;

enum class ScopeType : char {
  Invalid = 0,
  /* Use ascii chars to store them in string, and for easy debugging / testing. */
  Global = 'G',
  Namespace = 'N',
  Struct = 'S',
  Function = 'F',
  LoopArgs = 'l',
  LoopBody = 'p',
  SwitchArg = 'w',
  SwitchBody = 'W',
  FunctionArgs = 'f',
  FunctionCall = 'c',
  Template = 'T',
  TemplateArg = 't',
  Subscript = 'A',
  Preprocessor = 'P',
  Assignment = 'a',
  Attributes = 'B',
  Attribute = 'b',
  /* Added scope inside function body. */
  Local = 'L',
  /* Added scope inside FunctionArgs. */
  FunctionArg = 'g',
  /* Added scope inside FunctionCall. */
  FunctionParam = 'm',
  /* Added scope inside LoopArgs. */
  LoopArg = 'r',
};

}  // namespace blender::gpu::shader::parser
