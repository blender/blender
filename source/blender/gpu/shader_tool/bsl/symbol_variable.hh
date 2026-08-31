/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "resource.hh"
#include "source_location.hh"

#include <cstdint>
#include <variant>

namespace bsl {

struct SymbolScope;
struct SymbolClass;
struct SymbolTable;

/**
 * Base struct for all named code entities.
 */
struct Symbol {
  /* Mangled identifier (eg. for 'i' in 'struct A { static int i; }' it would be 'A_i'). */
  string identifier;
  /* Unresolved identifier */
  string original;
  SourceLocation loc;
  SymbolScope *parent = nullptr;
};

/**
 * Note that we do not support smaller types (e.g. char, ushort, ...) because they might not exist
 * in the target language. Supporting them would mean having potentially 2 different result for the
 * same expression depending on whether or not it is constexpr.
 */
using ConstexprValue = std::variant<bool, int32_t, uint32_t, float>;

template<typename T> T value_as(const ConstexprValue &v)
{
  return std::visit([](auto &&arg) -> T { return static_cast<T>(arg); }, v);
}

/**
 * Represent variable declarations, parameters, or enum values.
 */
struct SymbolVariable : Symbol {
  SymbolClass *type = nullptr;

  /* Valid if the variable is a local reference.
   * The variable will be substituted to this statement. */
  ast::Expr reference_value = {};

  /* Value if constexpr. */
  ConstexprValue value = 0;

  ResourceType res_type = ResourceType::NONE;

  int8_t array_dimensions = 0;
  /* Number of array elements in the declaration.
   * 0 if not array.
   * -1 if not known at compile time. */
  int array_elements = 0;
  /* In bytes. From scope start, taking alignment into account. */
  int offset = 0;

  bool is_static = false;
  bool is_error = false;
  bool is_constexpr = false;
  bool is_const = false;
  bool is_compilation_const = false;

  SymbolVariable(SymbolScope *parent,
                 SymbolClass *type,
                 ast::Declarator decl,
                 const SymbolTable &table);

  SymbolVariable(SymbolScope *parent, SymbolClass *type, ast::EnumValue decl)
      : Symbol(string(decl.identifier().str()),
               string(decl.identifier().str()),
               decl.front(),
               parent),
        type(type),
        is_static(true)
  {
  }

  SymbolVariable(SymbolScope *parent, SymbolClass *type, Token id, const string &str)
      : Symbol(str, str, id, parent), type(type)
  {
  }

  void set_offset(bool is_union, int &offset);

  /* Can be null if in global space. */
  SymbolClass *parent_class();

  string value_str() const;
};

}  // namespace bsl
