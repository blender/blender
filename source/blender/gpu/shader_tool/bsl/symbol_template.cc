/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "symbol_template.hh"
#include "symbol_table.hh"

namespace bsl {

using namespace blender::gpu::shader::parser::ast;

template<typename T>
ResultPair<T *, string> SymbolTemplate<T>::lookup_inst(TemplateParamList list,
                                                       const SymbolScope &scope,
                                                       const SymbolTable &table) const
{
  auto [mangled, err] = table.mangle_identifier(decl.arguments(), list, scope);
  auto [args, args_debug] = mangled;
  auto it = instances.find(args);
  string full_id = string(id().str()) + "<" + args_debug.substr(2) + ">";

  if (it == instances.end() && !err) {
    err = {list, Diag::TemplateMissingInstantiation, full_id};
  }

  if (err) {
    if constexpr (is_same_v<T, SymbolFunction>) {
      return {scope.root_scope()->lookup_function(SymbolTable::err_symbol), full_id, err};
    }
    else {
      return {scope.root_scope()->lookup_class(SymbolTable::err_symbol), full_id, err};
    }
  }
  return {it->second, full_id, err};
}

template<typename T> SymbolTemplate<T>::SymbolTemplate(ast::TemplateDecl temp) : decl(temp)
{
  if (temp.is_class()) {
    return;
  }
  TemplateArgList tmp_args(temp.arguments());

  int tmp_arg_count = tmp_args.child_count();
  if (tmp_arg_count == 0) {
    return;
  }

  FuncDecl decl(temp.decl());
  for (TemplateArg tmp_arg : tmp_args.children_of_type<TemplateArg>()) {
    int arg_id = -1;
    int id = 0;
    for (FuncArg arg : decl.arguments().children_of_type<FuncArg>()) {
      if (tmp_arg.identifier().str() == arg.type().identifier().str()) {
        arg_id = id;
        break;
      }
      ++id;
    }
    if (arg_id != -1) {
      temp_arg_index_in_fn_arg.emplace_back(arg_id);
    }
  }

  if (temp_arg_index_in_fn_arg.size() != tmp_arg_count) {
    /* ADL not possible. */
    temp_arg_index_in_fn_arg.clear();
  }
}

Result<StringPair> SymbolTable::mangle_identifier(const SymbolFunctionTemplate &tmp,
                                                  FuncParamList list,
                                                  const SymbolScope &scope) const
{
  vector<SymbolClass *> arg_cls;
  for (Expr expr : list.children_of_type<Expr>()) {
    auto [result, _] = expr_type_analysis(scope, expr.child_first());
    arg_cls.emplace_back(result.type);
  }

  StringPair result;
  for (int i : tmp.temp_arg_index_in_fn_arg) {
    if (i < arg_cls.size()) {
      result.str += "T" + arg_cls[i]->identifier;
      result.str_debug += ", " + arg_cls[i]->identifier;
    }
    else {
      result.str += "T" + string(err_symbol);
      result.str_debug += ", " + string(err_symbol);
    }
  }
  return {result, {}};
}

template<typename T>
Result<T *> SymbolTemplate<T>::lookup_adl(const SymbolTable &symbols,
                                          FuncParamList list,
                                          const SymbolScope &scope) const
{
  if constexpr (is_same_v<T, SymbolClass>) {
    return {scope.root_scope()->lookup_class(SymbolTable::err_symbol),
            AstNodeException(list, Diag::CompilerErrorADLOnTypesNotAllowed)};
  }
  else {
    if (!is_adl_possible()) {
      return {scope.root_scope()->lookup_function(SymbolTable::err_symbol),
              AstNodeException(list.prev(), Diag::TemplateMissingExplicitArguments)};
    }
    auto [mangled, err] = symbols.mangle_identifier(*this, list, scope);
    auto [args, args_debug] = mangled;
    auto it = instances.find(args);
    if (it == instances.end() && !err) {
      err = {list,
             Diag::TemplateMissingInstantiation,
             string(id().str()) + "<" + args_debug.substr(2) + ">'"};
    }

    if (err) {
      return {scope.root_scope()->lookup_function(SymbolTable::err_symbol), err};
    }
    return {it->second, err};
  }
}

template struct SymbolTemplate<SymbolClass>;
template struct SymbolTemplate<SymbolFunction>;

}  // namespace bsl
