/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "symbol_scope.hh"
#include "symbol_template.hh"

namespace bsl {

struct MatchRank {
  enum Rank : uint8_t { None = 0, Conversion = 1, Promotion = 2, Exact = 3 } rank = None;
  /* For class conversion to one of their base class, distance to the base class.
   * 0 means no conversion. */
  uint8_t conversion_depth = 0;

  MatchRank() = default;
  MatchRank(Rank rank) : rank(rank) {}
  MatchRank(Rank rank, uint8_t conversion_depth) : rank(rank), conversion_depth(conversion_depth)
  {
  }

  bool operator==(Rank other_rank) const
  {
    return rank == other_rank;
  }
  bool operator>=(Rank other_rank) const
  {
    return rank >= other_rank;
  }
  bool operator<=(Rank other_rank) const
  {
    return rank <= other_rank;
  }

  bool operator==(const MatchRank &other) const
  {
    return rank == other.rank && conversion_depth == other.conversion_depth;
  }

  bool operator<(const MatchRank &other) const
  {
    if (rank == other.rank && rank == Conversion) {
      /* A greater depth is worse. Inverse the comparison. */
      return conversion_depth > other.conversion_depth;
    }
    return rank < other.rank;
  }

  bool operator>(const MatchRank &other) const
  {
    if (rank == other.rank && rank == Conversion) {
      /* A greater depth is worse. Inverse the comparison. */
      return conversion_depth < other.conversion_depth;
    }
    return rank > other.rank;
  }
};

/**
 * Container for a function symbol.
 *
 * All overloads of this function are contained inside the first declaration.
 */
struct SymbolFunction : SymbolScope {
  /* Single linked list of overloads. */
  SymbolFunction *overload_next = nullptr;

  SymbolFunctionTemplate *template_data = nullptr;
  SymbolClass *return_type = nullptr;

  ast::FuncForwardDecl fwd = {};
  ast::FuncDecl decl = {};

  vector<bool> arg_const;
  vector<SymbolClass *> arg_types;
  vector<ast::Expr> arg_defaults;

  bool is_error = false;
  /* False if this is a template root and is not a symbol to match on. */
  bool is_complete = true;
  /* If true, will allow vector type casting. */
  bool allow_vector_promotion = false;
  /* If true, is part of language builtin functions. */
  bool is_builtin = false;
  /* If true, this builtin function is implemented as constexpr. */
  bool is_constexpr = false;
  /* True for template specialization and instantiation. */
  bool is_specialization = false;
  /* True for force-inlined functions. */
  bool is_inline = false;
  /* False if this function was only forward declared. */
  bool is_defined = true;

  /* Identifier of the temporary return variable.
   * The real identifier is then specialized per instantiation. */
  static constexpr const char *inline_fn_ret_id = "_r";

  enum Type { STATIC, MEMBER, GLOBAL } fn_type;
  enum class EntryPointType { FRAG, VERT, COMP, NONE } entry_point_type;

  template<typename AstNodeT>
  SymbolFunction(SymbolScope *parent,
                 SymbolClass *return_type,
                 AstNodeT decl,
                 const string &suffix = "")
      : SymbolScope(
            parent, decl.front(), string(decl.identifier().name().str()) + suffix, FUNCTION),
        return_type(return_type),
        decl(decl),
        is_inline(decl.attributes().contains_attr("force_inline")),
        fn_type(decl.is_method() ? (decl.is_static() ? STATIC : MEMBER) : GLOBAL),
        entry_point_type(EntryPointType::NONE)
  {
    if (decl.is_method()) {
      loc = decl.parent(ast::NodeType::ClassDecl).front();
    }
    for (ast::Attr attr : decl.attributes().template children_of_type<ast::Attr>()) {
      string_view id = attr.identifier().str();
      if (id == "vertex") {
        entry_point_type = EntryPointType::VERT;
      }
      else if (id == "fragment") {
        entry_point_type = EntryPointType::FRAG;
      }
      else if (id == "compute") {
        entry_point_type = EntryPointType::COMP;
      }
    }
  }

  SymbolFunction(
      SymbolScope *parent, Token tok, SymbolClass *return_type, const string &id, Type fn_type)
      : SymbolScope(parent, tok, id, FUNCTION),
        return_type(return_type),
        fn_type(fn_type),
        entry_point_type(EntryPointType::NONE)
  {
  }

  /* Can be null if in global space. */
  SymbolClass *parent_class()
  {
    return this->parent->as_class();
  }
  const SymbolClass *parent_class() const
  {
    return this->parent->as_class();
  }

  bool is_entry_point() const
  {
    return entry_point_type != SymbolFunction::EntryPointType::NONE;
  }

  bool is_method() const
  {
    return fn_type != GLOBAL;
  }

  bool is_builtin_ctor() const
  {
    return is_builtin && allow_vector_promotion;
  }

  /* Only used for sorting purpose. Return a unique location. */
  SourceLocation decl_loc() const
  {
    if (poi.has_value() && this->template_data) {
      return poi.value();
    }
    return SourceLocation(fwd.is_valid() ? fwd.front() : decl.front());
  }

  /* To be called on the first registered overload. */
  Result<SymbolFunction *> lookup_overload(ast::IdQualified id,
                                           const SymbolTable &table,
                                           const vector<SymbolClass *> &arg_types);

  void add_overload(SymbolFunction *fn);
  void add_argument(bool is_const, SymbolClass *type, ast::Expr default_value = {});

  void reserve_arguments(int n);

  bool is_compatible(ResourceType res_type);

  static Result<vector<SymbolClass *>> to_arg_types(const SymbolTable &table,
                                                    const SymbolScope &scope,
                                                    ast::FuncParamList list);
  static Result<vector<SymbolClass *>> to_arg_types(const SymbolTable &table,
                                                    const SymbolScope &scope,
                                                    ast::FuncArgList list);

 private:
  /* Returning empty indicates this is not a viable candidate */
  optional<vector<MatchRank>> get_match_ranks(const SymbolTable &table,
                                              const vector<SymbolClass *> &param_types) const;
};

std::string to_str(SymbolFunction::EntryPointType type);

}  // namespace bsl
