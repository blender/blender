/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "symbol_variable.hh"
#include "symbol_table.hh"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace bsl {

using namespace blender::gpu::shader::parser::ast;

SymbolVariable::SymbolVariable(SymbolScope *parent,
                               SymbolClass *type,
                               Declarator decl,
                               const SymbolTable &table)
    : Symbol(
          string(decl.identifier().str()), string(decl.identifier().str()), decl.front(), parent),
      type(type),
      array_dimensions(decl.array().dimensions()),
      is_bitfield(decl.bitfield().is_valid()),
      is_static(decl.type().is_static()),
      is_compilation_const(AttrList(decl.type().prev()).contains_attr("compilation_constant"))
{
  if (ast::Const constant = decl.type().constant(); constant.is_valid()) {
    is_constexpr = constant.front() == Constexpr;
    is_const = true;
  }
  if (ArrayDecl array = decl.array(); array.is_valid()) {
    Subscript sub = array.sub();
    array_elements = 1;
    for (int i = 0; i < array_dimensions; ++i, sub = sub.next()) {
      Node node = sub.expr().child_first();
      if (!node.is_valid()) {
        array_elements = -1;
        break;
      }
      auto [result, _] = table.expr_type_analysis(*parent, node);
      if (!result.is_constexpr()) {
        array_elements = -1;
        break;
      }
      array_elements *= result.value.comp_as<int>(0);
    }
  }
}

void SymbolVariable::set_offset(bool is_union, int &offset)
{
  if (is_union) {
    this->offset = 0;
  }
  else {
    this->offset = pad(offset, this->type->align);
    offset = this->offset;
    int elements = (this->array_dimensions == 0) ? 1 : this->array_elements;
    if (elements != -1) {
      offset += this->type->size * elements;
    }
    else {
      /* TODO(fclem): Maybe assert here. There is no reason to allow non-static array size. */
      offset += this->type->size;
    }
  }
}

/* Can be null if in global space. */
SymbolClass *SymbolVariable::parent_class()
{
  return this->parent->as_class();
}

static std::string to_string_exact(float val)
{
  std::stringstream ss;
  ss << std::setprecision(std::numeric_limits<float>::max_digits10) << val;
  string str = ss.str();
  /* Add trailing zero if there is no decimal. */
  if (str.find_first_of(".eE") == std::string::npos) {
    str += ".0";
  }
  return str + "f";
}

std::string constexpr_to_string(const ConstexprValue &v)
{
  const int size = v.values.size();
  const ConstexprValue::CompType comp_type = v.comp_type();
  std::string s;

  if (size > 1) {
    switch (comp_type) {
      case ConstexprValue::BOOL:
        s += "bool";
        break;
      case ConstexprValue::UINT:
        s += "uint";
        break;
      case ConstexprValue::INT:
        s += "int";
        break;
      case ConstexprValue::FLOAT:
        s += "float";
        break;
    }
    s += std::to_string(size) + "(";
  }

  for (int i = 0; i < size; ++i) {
    switch (comp_type) {
      case ConstexprValue::BOOL:
        s += v.values[i] ? "true" : "false";
        break;
      case ConstexprValue::UINT:
        s += std::to_string(v.values[i]) + "u";
        break;
      case ConstexprValue::INT:
        s += std::to_string(int32_t(v.values[i]));
        break;
      case ConstexprValue::FLOAT:
        if (std::isnan(std::bit_cast<float>(v.values[i]))) {
          s += "uintBitsToFloat(0x7FC00000u)";
        }
        else {
          s += to_string_exact(std::bit_cast<float>(v.values[i]));
        }
        break;
    }
    if (size > 1 && i < size - 1) {
      s += ", ";
    }
  }

  if (size > 1) {
    s += ")";
  }

  return s;
}

string SymbolVariable::value_str() const
{
  assert(is_constexpr);
  return constexpr_to_string(value);
}

}  // namespace bsl
