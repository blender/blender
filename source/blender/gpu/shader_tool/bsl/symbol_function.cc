/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "symbol_function.hh"
#include "symbol_table.hh"

namespace bsl {

using namespace blender::gpu::shader::parser::ast;

Result<vector<SymbolClass *>> SymbolFunction::to_arg_types(const SymbolTable &table,
                                                           const SymbolScope &scope,
                                                           FuncParamList list)
{
  std::optional<AstNodeException> error;
  vector<SymbolClass *> arg_types;
  for (Expr expr : list.children_of_type<Expr>()) {
    arg_types.emplace_back(table.expr_type_analysis(scope, expr.child_first()).unwrap(error).type);
  }
  return {arg_types, error};
}

Result<vector<SymbolClass *>> SymbolFunction::to_arg_types(const SymbolTable &table,
                                                           const SymbolScope &scope,
                                                           FuncArgList list)
{
  vector<SymbolClass *> arg_types;
  for (FuncArg arg : list.children_of_type<FuncArg>()) {
    auto [cls, _] = scope.lookup_class(table, arg.type().identifier());
    arg_types.emplace_back(cls);
  }
  return {arg_types, std::nullopt};
}

static string to_str(const vector<SymbolClass *> &types)
{
  string parameters = types.empty() ? ", " : "";
  for (SymbolClass *cls : types) {
    parameters += ", " + cls->identifier + "";
  }
  return "(" + parameters.substr(2) + ")";
}

optional<vector<MatchRank>> SymbolFunction::get_match_ranks(
    const SymbolTable &table, const vector<SymbolClass *> &param_types) const
{
  /* Early out if there are more arguments than parameters. */
  if (param_types.size() > arg_types.size()) {
    return nullopt; /* Not a viable candidate. */
  }

  vector<MatchRank> ranks;
  for (size_t i = 0; i < param_types.size(); ++i) {
    MatchRank rank = table.get_conversion_rank(param_types[i], arg_types[i], is_builtin);
    if (rank == MatchRank::None) {
      return {}; /* Not a viable candidate. */
    }
    ranks.push_back(rank);
  }

  /* Check if remaining parameters are optional. */
  for (size_t i = param_types.size(); i < arg_types.size(); ++i) {
    if (!arg_defaults[i].is_valid()) {
      return {}; /* Missing required argument. */
    }
    /* Default arguments are generally treated as an exact match for scoring purposes. */
    ranks.push_back(MatchRank::Exact);
  }

  return ranks;
}

static bool is_strictly_better(const std::vector<MatchRank> &candidate,
                               const std::vector<MatchRank> &best)
{
  bool has_strictly_better_arg = false;

  for (size_t i = 0; i < candidate.size(); ++i) {
    if (candidate[i] < best[i]) {
      return false; /* Worse in at least one argument, so it's not strictly better overall. */
    }
    if (candidate[i] > best[i]) {
      has_strictly_better_arg = true;
    }
  }

  return has_strictly_better_arg;
}

Result<SymbolFunction *> SymbolFunction::lookup_overload(IdQualified id,
                                                         const SymbolTable &table,
                                                         const vector<SymbolClass *> &arg_types)
{
  SymbolFunction *best_fn = nullptr;
  vector<MatchRank> best_ranks;
  bool is_ambiguous = false;

  for (SymbolFunction *fn = this; fn; fn = fn->overload_next) {
    if (!fn->is_complete) {
      continue;
    }

    auto current_ranks_opt = fn->get_match_ranks(table, arg_types);
    if (!current_ranks_opt) {
      continue; /* Skip non-viable candidates. */
    }

    const vector<MatchRank> &current_ranks = *current_ranks_opt;

    if (!best_fn) {
      best_fn = fn;
      best_ranks = std::move(current_ranks);
    }
    else {
      bool current_is_better = is_strictly_better(current_ranks, best_ranks);
      bool best_is_better = is_strictly_better(best_ranks, current_ranks);

      if (current_is_better) {
        best_fn = fn;
        best_ranks = std::move(current_ranks);
        is_ambiguous = false;
      }
      else if (!best_is_better) {
        is_ambiguous = true;
      }
    }
  }

  if (best_fn && !is_ambiguous) {
    return {best_fn, {}};
  }
  /* List all candidates inside the error message. */
  string candidates;
  for (const SymbolFunction *fn = this; fn; fn = fn->overload_next) {
    if (fn->is_complete) {
      candidates += "\ncandidate: " + fn->return_type->original + " " + fn->original +
                    to_str(fn->arg_types);
    }
  }

  Diag error_type = is_ambiguous ? Diag::OverloadAmbiguous : Diag::OverloadNotFound;
  return {root_scope()->lookup_function(SymbolTable::err_symbol),
          AstNodeException(id, error_type, original, to_str(arg_types), candidates)};
}

void SymbolFunction::add_overload(SymbolFunction *fn)
{
  fn->overload_next = this->overload_next;
  this->overload_next = fn;
}

void SymbolFunction::reserve_arguments(int n)
{
  this->arg_types.reserve(n);
  this->arg_defaults.reserve(n);
}

void SymbolFunction::add_argument(SymbolClass *type, Expr default_value)
{
  this->arg_types.emplace_back(type);
  this->arg_defaults.emplace_back(default_value);
}

}  // namespace bsl
