/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "ast.hh"
#include "diagnostic.hh"
#include "symbol_class.hh"
#include "symbol_function.hh"
#include "symbol_variable.hh"
#include "type_checker.hh"

#include <deque>
#include <unordered_map>

namespace bsl {

struct StringPair {
  string str;
  string str_debug;
};

struct SymbolVariable;
struct SymbolFunction;
struct SymbolClass;
struct SymbolScope;

struct SymbolTable {

  static constexpr const char *err_symbol = "ERROR_SYMBOL";

  template<typename T> struct Allocator {
   private:
    std::deque<T> arena;

   public:
    template<typename... Args> T *alloc(Args &&...args)
    {
      arena.emplace_back(std::forward<Args>(args)...);
      return &arena.back();
    }
  };

  Allocator<SymbolVariable> var_arena;
  Allocator<SymbolFunction> fun_arena;
  Allocator<SymbolClass> cls_arena;
  Allocator<SymbolScope> scp_arena;
  Allocator<SymbolClassTemplate> tmp_cls_arena;
  Allocator<SymbolFunctionTemplate> tmp_fun_arena;

  SymbolVariable *err_var = nullptr;
  SymbolFunction *err_func = nullptr;

  SymbolClass *err_cls = nullptr;
  SymbolClass *char_cls = nullptr;
  SymbolClass *short_cls = nullptr;
  SymbolClass *int_cls = nullptr;
  SymbolClass *uchar_cls = nullptr;
  SymbolClass *ushort_cls = nullptr;
  SymbolClass *uint_cls = nullptr;
  SymbolClass *half_cls = nullptr;
  SymbolClass *float_cls = nullptr;
  SymbolClass *void_cls = nullptr;
  SymbolClass *str_cls = nullptr;
  SymbolClass *char2_cls = nullptr;
  SymbolClass *short2_cls = nullptr;
  SymbolClass *int2_cls = nullptr;
  SymbolClass *uchar2_cls = nullptr;
  SymbolClass *ushort2_cls = nullptr;
  SymbolClass *uint2_cls = nullptr;
  SymbolClass *half2_cls = nullptr;
  SymbolClass *char3_cls = nullptr;
  SymbolClass *short3_cls = nullptr;
  SymbolClass *int3_cls = nullptr;
  SymbolClass *uchar3_cls = nullptr;
  SymbolClass *ushort3_cls = nullptr;
  SymbolClass *uint3_cls = nullptr;
  SymbolClass *half3_cls = nullptr;
  SymbolClass *char4_cls = nullptr;
  SymbolClass *short4_cls = nullptr;
  SymbolClass *int4_cls = nullptr;
  SymbolClass *uchar4_cls = nullptr;
  SymbolClass *ushort4_cls = nullptr;
  SymbolClass *uint4_cls = nullptr;
  SymbolClass *half4_cls = nullptr;
  SymbolClass *float2_cls = nullptr;
  SymbolClass *float3_cls = nullptr;
  SymbolClass *float4_cls = nullptr;
  SymbolClass *packed_float2_cls = nullptr;
  SymbolClass *packed_float3_cls = nullptr;
  SymbolClass *packed_float4_cls = nullptr;
  SymbolClass *bool_cls = nullptr;
  SymbolClass *bool2_cls = nullptr;
  SymbolClass *bool3_cls = nullptr;
  SymbolClass *bool4_cls = nullptr;
  SymbolClass *float2x2_cls = nullptr;
  SymbolClass *float3x3_cls = nullptr;
  SymbolClass *float4x4_cls = nullptr;
  SymbolClass *float2x3_cls = nullptr;
  SymbolClass *float2x4_cls = nullptr;
  SymbolClass *float3x2_cls = nullptr;
  SymbolClass *float3x4_cls = nullptr;
  SymbolClass *float4x2_cls = nullptr;
  SymbolClass *float4x3_cls = nullptr;
  SymbolClass *bool32_t_cls = nullptr;

  unordered_map<int, string> image_formats;

  static constexpr const char *subscript_operator_id = "arr_op_";

  template<typename SymbolT> SymbolT *alloc(SymbolT &&sym);

  SymbolScope *root = nullptr;

  SymbolTable(ParserBase &builtin_parser);

  void parse(ast::LocalScope node, ErrorHandler &err_handler);

  Result<StringPair> mangle_identifier(ast::TemplateArgList args,
                                       ast::TemplateParamList list,
                                       const SymbolScope &scope) const;

  Result<StringPair> mangle_identifier(const SymbolFunctionTemplate &tmp,
                                       ast::FuncParamList list,
                                       const SymbolScope &scope) const;

  Result<ExpressionResult> expr_type_analysis(const SymbolScope &scope,
                                              ast::Node start_node,
                                              bool is_reference = false) const;

  Result<SymbolClass *> resolve_auto_type(SymbolScope &scope, ast::Declarator decl) const;

  Result<string> expr_to_string(const SymbolScope &scope,
                                ast::Node start,
                                int &node_count,
                                bool allow_compilation_constant = false) const;

  SymbolClass *get_literal_type(string_view lit) const;

  MatchRank get_conversion_rank(const SymbolClass *from,
                                const SymbolClass *to,
                                const bool ctor_conversion = false) const;

  SymbolClass *to_class(builtin::ClassId id) const;

 private:
  struct BuiltinType {
    /* "int", "int2", ... */
    SymbolClass *type;
    /* "int", "uint", "float", "bool", ... */
    SymbolClass *base;
  };

  struct BuiltinOp {
    SymbolClass *left;
    TokenType op;
    SymbolClass *right;
    SymbolClass *result;
  };

  struct BuiltinFunc {
    SymbolClass *return_type;
    string id;
    vector<SymbolClass *> arg_types;
  };

  void register_builtins(ast::LocalScope node);
  vector<BuiltinType> generate_builtin_types();
  vector<BuiltinFunc> generate_all_constructors(const vector<BuiltinType> &types);
  vector<BuiltinFunc> generate_all_builtin_functions();

  SymbolClass *make_type(SymbolClass *base, int size);

  void add_operator(
      SourceLocation loc, SymbolClass *left, TokenType op, SymbolClass *right, SymbolClass *ret);
};

}  // namespace bsl
