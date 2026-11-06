/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "symbol_class.hh"

namespace bsl {

using namespace blender::gpu::shader::parser;

enum ExprFlag : uint8_t {
  IsNone = 0u,
  IsConstant = 1u << 0,
  IsConstexpr = 1u << 1,
};

struct ExpressionResult {
  const SymbolClass *type;
  /* Value if constexpr. */
  ConstexprValue value = ConstexprError(0);
  /* Array dimension of the current temporary. */
  uint8_t array_dim = 0;

  ExprFlag flags = ExprFlag::IsNone;
  bool is_temporary = false;
  bool is_ptr = false;

  ExpressionResult(const SymbolVariable &var)
      : type(&var.type),
        value(var.value),
        array_dim(var.array_dimensions),
        flags(ExprFlag((var.is_constexpr ? ExprFlag::IsConstexpr : ExprFlag::IsNone) |
                       (var.is_const ? ExprFlag::IsConstant : ExprFlag::IsNone))),
        is_ptr(var.is_ptr)
  {
  }

  ExpressionResult(const SymbolClass &cls, ExprFlag flags = ExprFlag::IsNone)
      : type(&cls), flags(flags), is_temporary(true)
  {
  }

  ExpressionResult(const SymbolClass &cls, ConstexprValue value, bool is_temporary = true)
      : type(&cls),
        value(value),
        flags(ExprFlag(ExprFlag::IsConstexpr | ExprFlag::IsConstant)),
        is_temporary(is_temporary)
  {
  }

  bool is_constexpr() const
  {
    return flags & ExprFlag::IsConstexpr;
  }

  bool is_constant() const
  {
    return flags & ExprFlag::IsConstant;
  }
};

} /* namespace bsl */
