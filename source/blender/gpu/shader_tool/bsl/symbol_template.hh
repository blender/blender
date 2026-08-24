/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "ast.hh"
#include "diagnostic.hh"
#include "symbol_variable.hh"

#include <unordered_map>

namespace bsl {

struct SymbolTable;
struct SymbolScope;
struct SymbolClass;
struct SymbolFunction;

/**
 * Templated symbol additional storage.
 * Contains instances and reference to the declaration.
 */
template<typename T> struct SymbolTemplate {
  /* Definition. */
  ast::TemplateDecl decl;

  unordered_map<string, T *> instances;

  /* For argument dependent lookup. Maps a template argument to a function argument. */
  vector<int> temp_arg_index_in_fn_arg;

  SymbolTemplate(ast::TemplateDecl temp);

  bool is_adl_possible() const
  {
    return !temp_arg_index_in_fn_arg.empty();
  }

  ResultPair<T *, string> lookup_inst(ast::TemplateParamList list,
                                      const SymbolScope &scope,
                                      const SymbolTable &table) const;
  Result<T *> lookup_adl(const SymbolTable &symbols,
                         ast::FuncParamList list,
                         const SymbolScope &scope) const;

 private:
  ast::IdQualified id() const
  {
    if (decl.is_class()) {
      return ast::ClassDecl(decl.decl()).identifier();
    }
    return ast::FuncDecl(decl.decl()).identifier();
  }
};

using SymbolClassTemplate = SymbolTemplate<SymbolClass>;
using SymbolFunctionTemplate = SymbolTemplate<SymbolFunction>;

}  // namespace bsl
