/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "symbol_table.hh"
#include "expression.hh"

#include <bit>
#include <cmath>

namespace bsl {

using namespace blender::gpu::shader::parser;
using namespace blender::gpu::shader::parser::ast;
using namespace std;
using Token = blender::gpu::shader::parser::Token;

SymbolTable::SymbolTable(ParserBase &parser)
{
  LocalScope node = parser.root();
  root = scp_arena.alloc(node);
  register_builtins(node);
}

Result<SymbolClass *> SymbolTable::resolve_auto_type(SymbolScope &scope, Declarator decl) const
{
  std::optional<AstNodeException> err;
  Node node = decl.child_last();
  AssignStmt assign(node);

  if (Expr expr = assign.expr(); expr.is_valid()) {
    return {expr_type_analysis(scope, expr.child_first()).unwrap(err).type, err};
  }

  InitializerList list(node);
  if (!list.is_valid()) {
    list = assign.initializer_list();
  }

  if (list.is_valid()) {
    Initializer init(list.child_first());
    if (init.child_first().type() == NodeType::InitializerList) {
      err = {init.child_first(), Diag::AutoTypeNestedInitializer, string(decl.identifier().str())};
    }
    else if (init.next().is_valid()) {
      err = {init.next(), Diag::AutoTypeMultipleExpressions, string(decl.identifier().str())};
    }
    else {
      Expr expr(init.child_first());
      return {expr_type_analysis(scope, expr.child_first()).unwrap(err).type, err};
    }
  }

  return {scope.root_scope()->lookup_class(SymbolTable::err_symbol), err};
}

Result<StringPair> SymbolTable::mangle_identifier(TemplateArgList args,
                                                  TemplateParamList list,
                                                  const SymbolScope &scope) const
{
  Expr param = list.child_first();

  std::optional<AstNodeException> err;

  StringPair result = {};
  for (TemplateArg arg : args.children_range()) {
    assert(arg.is_valid());
    if (!param.is_valid()) {
      err = AstNodeException(list, Diag::TemplateMissingArgument, string(arg.type().str()));
    }
    else if (arg.front() == Typename) {
      if (param.child_count() != 1 || param.child_first().type() != NodeType::LocalVar) {
        err = AstNodeException(param, Diag::TemplateInvalidArg, string(arg.type().str()));
      }
      else {
        IdQualified type_id(LocalVar(param.child_first()).identifier());
        auto *type = scope.lookup_class_base(*this, type_id);
        if (type->template_data) {
          TemplateParamList list_nested = type_id.template_params();
          if (!list_nested.is_valid()) {
            err = AstNodeException(
                type_id, Diag::TemplateMissingParametersForType, string(type_id.str()));
          }
          else {
            /* Recursive. */
            auto [type_, _, err_] = type->template_data->lookup_inst(list_nested, scope, *this);
            type = type_;
            if (err_) {
              err = err_;
            }
          }
        }
        if (type->is_error) {
          err = AstNodeException(param, Diag::UnknownIdentifier, string(type_id.str()));
        }
        else {
          result.str += "T" + type->identifier;
          result.str_debug += ", " + type->identifier;
        }
      }
    }
    else {
      auto expr_result = expr_type_analysis(scope, param.child_first()).unwrap(err);
      if (!expr_result.is_constexpr()) {
        err = AstNodeException(param, Diag::TemplateParameterNotConstexpr);
      }
      int val = value_as<int>(expr_result.value);
      /* Replace minus sign by underscore. */
      if (arg.type().str() == "bool") {
        result.str += string("T") + (val ? "true" : "false");
      }
      else {
        result.str += string("T") + (val < 0 ? "_" : "") + to_string(abs(val));
      }
      result.str_debug += ", " + to_string(val);
    }
    param = param.next();
  }
  return {result, err};
}

Result<string> SymbolTable::expr_to_string(const SymbolScope &scope,
                                           Node start,
                                           int &node_count,
                                           bool allow_compilation_constant) const
{
  const char *expr_type = allow_compilation_constant ? "static branch condition" :
                                                       "constant expression";
  std::optional<AstNodeException> error;
  string expr_str;
  for (Node child = start; child.is_valid(); child = child.next()) {
    switch (child.type()) {
      case NodeType::LocalVar: {
        SymbolVariable *var = scope.lookup_variable(*this, LocalVar(child).identifier());
        /* Resolve member access. */

        for (Node next = child.next(); next.type() == NodeType::Op && next.front() == '.';
             next = child.next())
        {
          child = next.next();

          if (child.type() == NodeType::LocalVar) {
            var = var->type->lookup_variable(*this, LocalVar(child).identifier());
          }
          else if (child.type() == NodeType::FuncCall) {
            return {
                "0",
                AstNodeException(child, Diag::ExprCannotContainFunctionCalls, string(expr_type))};
          }
          else {
            return {
                "0",
                AstNodeException(
                    child, Diag::UnsupportedSymbolInExpr, string(child.str()), string(expr_type))};
          }
        }

        if (var->is_error) {
          return {"0", AstNodeException(child, Diag::UnknownVariable, string(child.str()))};
        }

        if (allow_compilation_constant && var->is_compilation_const) {
          /* Compilation constants are in global namespace. */
          expr_str += "SRT_CONSTANT_" + var->identifier;
        }
        else if (!var->is_constexpr) {
          return {"0", AstNodeException(child, Diag::NonConstVariableInExpr, string(expr_type))};
        }
        else {
          expr_str += var->value_str();
        }
        node_count++;
        break;
      }
      case NodeType::FuncCall:
        return {"0",
                AstNodeException(child, Diag::ExprCannotContainFunctionCalls, string(expr_type))};
      case NodeType::ExprSub: {
        auto [str, err] = expr_to_string(scope, ExprSub(child).expr().child_first(), node_count);
        expr_str += '(';
        expr_str += str;
        expr_str += ')';
        node_count++;
        if (err) {
          return {"0", err};
        }
        break;
      }
      case NodeType::Op:
      case NodeType::NumConst:
        expr_str += child.str();
        node_count++;
        break;
      default:
        return {"0",
                AstNodeException(
                    child, Diag::UnsupportedSymbolInExpr, string(child.str()), string(expr_type))};
    }
  }
  return {expr_str, error};
}

void SymbolTable::register_builtins(LocalScope node)
{
  Token tok = node.back();

  struct BuiltinBasicType {
    string id;
    size_t size;
    size_t align;
    builtin::ClassId cls;
  };

  const vector<BuiltinBasicType> basic_types = {
      {err_symbol, 1, 1, builtin::Invalid},

      {"char", 1, 1, builtin::char_t},
      {"short", 2, 2, builtin::short_t},
      {"int", 4, 4, builtin::int_t},
      {"uchar", 1, 1, builtin::uchar_t},
      {"ushort", 2, 2, builtin::ushort_t},
      {"uint", 4, 4, builtin::uint_t},
      {"half", 2, 2, builtin::half_t},
      {"float", 4, 4, builtin::float_t},
      {"bool", 1, 1, builtin::bool_t},
      {"bool32_t", 4, 4, builtin::bool_t},
  };

  for (const auto &t : basic_types) {
    SymbolClass *cls = cls_arena.alloc(root, tok, t.id, t.size, t.align, t.cls);
    cls->is_std140_compatible = t.align == 4;
    cls->is_std430_compatible = t.align == 4;
    cls->comp_len = 1;
    cls->is_error = t.id == err_symbol;
    root->classes.emplace(t.id, cls);
  }

  this->err_cls = root->lookup_class(SymbolTable::err_symbol);

  this->char_cls = root->lookup_class("char");
  this->short_cls = root->lookup_class("short");
  this->int_cls = root->lookup_class("int");
  this->uchar_cls = root->lookup_class("uchar");
  this->ushort_cls = root->lookup_class("ushort");
  this->uint_cls = root->lookup_class("uint");
  this->half_cls = root->lookup_class("half");
  this->float_cls = root->lookup_class("float");
  this->bool_cls = root->lookup_class("bool");
  this->bool32_t_cls = root->lookup_class("bool32_t");

  const vector<BuiltinBasicType> special_types = {
      {err_symbol, 1, 1},

      {"void", 1, 1}, /* Only for function return type. */

      {"string_t", 4, 4},

      {"samplerBuffer", 0, 1},
      {"sampler1D", 0, 1},
      {"sampler2D", 0, 1},
      {"sampler3D", 0, 1},
      {"isamplerBuffer", 0, 1},
      {"isampler1D", 0, 1},
      {"isampler2D", 0, 1},
      {"isampler3D", 0, 1},
      {"usamplerBuffer", 0, 1},
      {"usampler1D", 0, 1},
      {"usampler2D", 0, 1},
      {"usampler3D", 0, 1},
      {"sampler1DArray", 0, 1},
      {"sampler2DArray", 0, 1},
      {"isampler1DArray", 0, 1},
      {"isampler2DArray", 0, 1},
      {"usampler1DArray", 0, 1},
      {"usampler2DArray", 0, 1},
      {"samplerCube", 0, 1},
      {"isamplerCube", 0, 1},
      {"usamplerCube", 0, 1},
      {"samplerCubeArray", 0, 1},
      {"isamplerCubeArray", 0, 1},
      {"usamplerCubeArray", 0, 1},
      {"usampler1DAtomic", 0, 1},
      {"usampler2DAtomic", 0, 1},
      {"usampler2DArrayAtomic", 0, 1},
      {"usampler3DAtomic", 0, 1},
      {"isampler1DAtomic", 0, 1},
      {"isampler2DAtomic", 0, 1},
      {"isampler2DArrayAtomic", 0, 1},
      {"isampler3DAtomic", 0, 1},
      {"sampler2DDepth", 0, 1},
      {"sampler2DArrayDepth", 0, 1},
      {"samplerCubeDepth", 0, 1},
      {"samplerCubeArrayDepth", 0, 1},
      {"image1D", 0, 1},
      {"image2D", 0, 1},
      {"image3D", 0, 1},
      {"iimage1D", 0, 1},
      {"iimage2D", 0, 1},
      {"iimage3D", 0, 1},
      {"uimage1D", 0, 1},
      {"uimage2D", 0, 1},
      {"uimage3D", 0, 1},
      {"image1DArray", 0, 1},
      {"image2DArray", 0, 1},
      {"iimage1DArray", 0, 1},
      {"iimage2DArray", 0, 1},
      {"uimage1DArray", 0, 1},
      {"uimage2DArray", 0, 1},
      {"iimage2DAtomic", 0, 1},
      {"iimage3DAtomic", 0, 1},
      {"uimage2DAtomic", 0, 1},
      {"uimage3DAtomic", 0, 1},
      {"iimage2DArrayAtomic", 0, 1},
      {"uimage2DArrayAtomic", 0, 1},

      {"ShaderCreateInfo", 1, 1}, /* Only for compatibility. To be removed. */
  };

  for (const auto &t : special_types) {
    SymbolClass *sym = cls_arena.alloc(root, tok, t.id, t.size, t.align);
    sym->is_error = t.id == err_symbol;
    sym->is_opaque = true;
    root->classes.emplace(t.id, sym);
  }

  this->str_cls = root->lookup_class("string_t");
  this->void_cls = root->lookup_class("void");

  struct BuiltinVector {
    string id;
    SymbolClass *base;
    int comp;
    int align;
    builtin::ClassId cls_id;
  };

  const char *swizzle_xyzw[4] = {"x", "y", "z", "w"};
  const char *swizzle_rgba[4] = {"r", "g", "b", "a"};

  auto generate_vector = [&](const BuiltinVector &t, const bool do_swizzle) {
    SymbolClass *base = t.base;
    SymbolClass *cls = cls_arena.alloc(root, tok, t.id, t.comp * base->size, t.align, t.cls_id);
    root->classes.emplace(t.id, cls);
    root->scopes.emplace(t.id, cls);

    cls->is_std140_compatible = t.align >= 8 && t.id != "float3" && base->is_std140_compatible;
    cls->is_std430_compatible = cls->is_std140_compatible;

    SymbolFunction *sub_op = fun_arena.alloc(
        cls, tok, base, subscript_operator_id, SymbolFunction::MEMBER);

    cls->operator_subscript = sub_op;
    cls->functions.emplace(subscript_operator_id, sub_op);
    cls->comp_len = t.comp * base->comp_len;

    for (int i = 0; i < t.comp && do_swizzle; i++) {
      string id_xyzw(swizzle_xyzw[i]);
      cls->variables.emplace(id_xyzw, var_arena.alloc(cls, base, tok, id_xyzw));
      string id_rgba(swizzle_rgba[i]);
      cls->variables.emplace(id_rgba, var_arena.alloc(cls, base, tok, id_rgba));
    }
  };

  const std::vector<BuiltinVector> vectors = {
      {"float2", float_cls, 2, 8, builtin::ClassId::float2_t},
      {"float3", float_cls, 3, 16, builtin::ClassId::float3_t},
      {"float4", float_cls, 4, 16, builtin::ClassId::float4_t},
      {"half2", half_cls, 2, 4, builtin::ClassId::half2_t},
      {"half3", half_cls, 3, 8, builtin::ClassId::half3_t},
      {"half4", half_cls, 4, 8, builtin::ClassId::half4_t},
      {"char2", char_cls, 2, 2, builtin::ClassId::char2_t},
      {"char3", char_cls, 3, 3, builtin::ClassId::char3_t},
      {"char4", char_cls, 4, 4, builtin::ClassId::char4_t},
      {"uchar2", uchar_cls, 2, 2, builtin::ClassId::uchar2_t},
      {"uchar3", uchar_cls, 3, 3, builtin::ClassId::uchar3_t},
      {"uchar4", uchar_cls, 4, 4, builtin::ClassId::uchar4_t},
      {"short2", short_cls, 2, 4, builtin::ClassId::short2_t},
      {"short3", short_cls, 3, 6, builtin::ClassId::short3_t},
      {"short4", short_cls, 4, 8, builtin::ClassId::short4_t},
      {"ushort2", ushort_cls, 2, 4, builtin::ClassId::ushort2_t},
      {"ushort3", ushort_cls, 3, 6, builtin::ClassId::ushort3_t},
      {"ushort4", ushort_cls, 4, 8, builtin::ClassId::ushort4_t},
      {"int2", int_cls, 2, 8, builtin::ClassId::int2_t},
      {"int3", int_cls, 3, 16, builtin::ClassId::int3_t},
      {"int4", int_cls, 4, 16, builtin::ClassId::int4_t},
      {"uint2", uint_cls, 2, 8, builtin::ClassId::uint2_t},
      {"uint3", uint_cls, 3, 16, builtin::ClassId::uint3_t},
      {"uint4", uint_cls, 4, 16, builtin::ClassId::uint4_t},
      {"bool2", bool_cls, 2, 2, builtin::ClassId::bool2_t},
      {"bool3", bool_cls, 3, 3, builtin::ClassId::bool3_t},
      {"bool4", bool_cls, 4, 4, builtin::ClassId::bool4_t},
      {"packed_float2", float_cls, 2, 8, builtin::ClassId::float2_t},
      {"packed_float3", float_cls, 3, 16, builtin::ClassId::float3_t},
      {"packed_float4", float_cls, 4, 16, builtin::ClassId::float4_t},
      {"packed_int2", int_cls, 2, 8, builtin::ClassId::int2_t},
      {"packed_int3", int_cls, 3, 16, builtin::ClassId::int3_t},
      {"packed_int4", int_cls, 4, 16, builtin::ClassId::int4_t},
  };

  for (const auto &t : vectors) {
    generate_vector(t, true);
  }

  /* Fast lookup cache. */
  this->char2_cls = root->lookup_class("char2");
  this->short2_cls = root->lookup_class("short2");
  this->int2_cls = root->lookup_class("int2");
  this->uchar2_cls = root->lookup_class("uchar2");
  this->ushort2_cls = root->lookup_class("ushort2");
  this->uint2_cls = root->lookup_class("uint2");
  this->half2_cls = root->lookup_class("half2");
  this->char3_cls = root->lookup_class("char3");
  this->short3_cls = root->lookup_class("short3");
  this->int3_cls = root->lookup_class("int3");
  this->uchar3_cls = root->lookup_class("uchar3");
  this->ushort3_cls = root->lookup_class("ushort3");
  this->uint3_cls = root->lookup_class("uint3");
  this->half3_cls = root->lookup_class("half3");
  this->char4_cls = root->lookup_class("char4");
  this->short4_cls = root->lookup_class("short4");
  this->int4_cls = root->lookup_class("int4");
  this->uchar4_cls = root->lookup_class("uchar4");
  this->ushort4_cls = root->lookup_class("ushort4");
  this->uint4_cls = root->lookup_class("uint4");
  this->half4_cls = root->lookup_class("half4");
  this->float2_cls = root->lookup_class("float2");
  this->float3_cls = root->lookup_class("float3");
  this->float4_cls = root->lookup_class("float4");
  this->packed_float2_cls = root->lookup_class("packed_float2");
  this->packed_float3_cls = root->lookup_class("packed_float3");
  this->packed_float4_cls = root->lookup_class("packed_float4");
  this->bool2_cls = root->lookup_class("bool2");
  this->bool3_cls = root->lookup_class("bool3");
  this->bool4_cls = root->lookup_class("bool4");

  const std::vector<BuiltinVector> matrices = {
      {"float2x2", float2_cls, 2, 16, builtin::ClassId::float2x2_t},
      {"float2x3", float3_cls, 2, 16, builtin::ClassId::float2x3_t},
      {"float2x4", float4_cls, 2, 16, builtin::ClassId::float2x4_t},
      {"float3x2", float2_cls, 3, 16, builtin::ClassId::float3x2_t},
      {"float3x3", float3_cls, 3, 16, builtin::ClassId::float3x3_t},
      {"float3x4", float4_cls, 3, 16, builtin::ClassId::float3x4_t},
      {"float4x2", float2_cls, 4, 16, builtin::ClassId::float4x2_t},
      {"float4x3", float3_cls, 4, 16, builtin::ClassId::float4x3_t},
      {"float4x4", float4_cls, 4, 16, builtin::ClassId::float4x4_t},
  };

  for (const auto &t : matrices) {
    generate_vector(t, false);
  }

  this->float2x2_cls = root->lookup_class("float2x2");
  this->float2x3_cls = root->lookup_class("float2x3");
  this->float2x4_cls = root->lookup_class("float2x4");
  this->float3x2_cls = root->lookup_class("float3x2");
  this->float3x3_cls = root->lookup_class("float3x3");
  this->float3x4_cls = root->lookup_class("float3x4");
  this->float4x2_cls = root->lookup_class("float4x2");
  this->float4x3_cls = root->lookup_class("float4x3");
  this->float4x4_cls = root->lookup_class("float4x4");

  /* Aliases. */
  root->classes.emplace(string("int32_t"), root->classes["int"]);
  root->classes.emplace(string("uint32_t"), root->classes["uint"]);

  /* Swizzle Generation */
  /* We do this in a separate loop because a swizzle on a float2 (like `.xxxx`).
   * might require float4 to already be registered in root->classes. */
  for (const auto &[name, base, comp, __, ___] : vectors) {
    SymbolClass *type = root->classes[name];
    auto generate_swizzles =
        [&](auto &self, string current, int target_length, auto swizzle) -> void {
      if (current.length() == target_length) {
        /* e.g., "float" + "3" = "float3" */
        string return_type_id = base->identifier + to_string(target_length);

        SymbolVariable var(type, root->classes[return_type_id], tok, current);
        SymbolFunction fun(
            type, tok, root->classes[return_type_id], current, SymbolFunction::MEMBER);

        type->variables.emplace(current, var_arena.alloc(var));
        type->functions.emplace(current, fun_arena.alloc(fun));
        return;
      }
      /* Only allow swizzling components that actually exist on this vector type. */
      for (int i = 0; i < comp; i++) {
        self(self, current + swizzle[i], target_length, swizzle);
      }
    };
    /* Generate combinations for vec2, vec3, and vec4 variants. */
    for (int len = 2; len <= 4; len++) {
      generate_swizzles(generate_swizzles, "", len, swizzle_xyzw);
      generate_swizzles(generate_swizzles, "", len, swizzle_rgba);
    }
  }

  auto builtin_function_decl = [&](const BuiltinFunc &fn, bool allow_vector_promotion) {
    SymbolFunction *sym = fun_arena.alloc(
        root, tok, fn.return_type, fn.id, SymbolFunction::GLOBAL);
    sym->is_error = fn.id == err_symbol;
    sym->is_builtin = allow_vector_promotion;
    sym->reserve_arguments(fn.arg_types.size());
    for (SymbolClass *arg : fn.arg_types) {
      sym->add_argument(arg);
    }
    root->function_emplace(sym, true);
  };

  const vector<BuiltinFunc> functions = generate_all_builtin_functions();
  /* Builtin functions. */
  for (const auto &f : functions) {
    builtin_function_decl(f, false);
  }

  root->lookup_function("floatBitsToUint")->is_builtin = true;
  root->lookup_function("floatBitsToInt")->is_builtin = true;
  root->lookup_function("uintBitsToFloat")->is_builtin = true;
  root->lookup_function("intBitsToFloat")->is_builtin = true;

  const vector<BuiltinType> builtin_types = generate_builtin_types();

  const vector<BuiltinFunc> builtin_constructors = generate_all_constructors(builtin_types);
  for (const auto &fn : builtin_constructors) {
    builtin_function_decl(fn, true);
  }

  image_formats = {
      {0, "SNORM_8"},           {1, "SNORM_8_8"},        {2, "SNORM_8_8_8_8"},
      {3, "SNORM_16"},          {4, "SNORM_16_16"},      {5, "SNORM_16_16_16_16"},
      {6, "UNORM_8"},           {7, "UNORM_8_8"},        {8, "UNORM_8_8_8_8"},
      {9, "UNORM_16"},          {10, "UNORM_16_16"},     {11, "UNORM_16_16_16_16"},
      {12, "SINT_8"},           {13, "SINT_8_8"},        {14, "SINT_8_8_8_8"},
      {15, "SINT_16"},          {16, "SINT_16_16"},      {17, "SINT_16_16_16_16"},
      {18, "SINT_32"},          {19, "SINT_32_32"},      {20, "SINT_32_32_32_32"},
      {21, "UINT_8"},           {22, "UINT_8_8"},        {23, "UINT_8_8_8_8"},
      {24, "UINT_16"},          {25, "UINT_16_16"},      {26, "UINT_16_16_16_16"},
      {27, "UINT_32"},          {28, "UINT_32_32"},      {29, "UINT_32_32_32_32"},
      {30, "SFLOAT_16"},        {31, "SFLOAT_16_16"},    {32, "SFLOAT_16_16_16_16"},
      {33, "SFLOAT_32"},        {34, "SFLOAT_32_32"},    {35, "SFLOAT_32_32_32_32"},
      {36, "UNORM_10_10_10_2"}, {37, "UINT_10_10_10_2"}, {38, "UFLOAT_11_11_10"},
  };

  struct BuiltinConst {
    string id;
    string type;
    bool is_constexpr;
    ConstexprValue value;
  };

  std::vector<BuiltinConst> consts = {
      /* Error variable symbol. */
      {err_symbol, err_symbol, false, 1},

      {"true", "bool", true, true},
      {"false", "bool", true, false},

      /* WORKAROUND: Should become an entry point argument. */
      {"gl_FragStencilRefARB", "uint", false, 0},
      {"gpu_BaryCoord", "float3", false, 0},

      {"FLT_MAX", "float", true, std::bit_cast<float>(0x7F7FFFFFu)},
      {"FLT_MIN", "float", true, std::bit_cast<float>(0x00800000u)},
      {"FLT_EPSILON", "float", true, 1.192092896e-07f},
      {"SHRT_MAX", "float", true, 0x00007FFF},
      {"INT_MAX", "float", true, 0x7FFFFFFF},
      {"USHRT_MAX", "float", true, 0x0000FFFFu},
      {"UINT_MAX", "float", true, 0xFFFFFFFFu},
      {"NAN_FLT", "float", true, NAN},
      {"FLT_11_MAX", "float", false, 0},
      {"FLT_10_MAX", "float", false, 0},
      {"FLT_11_11_10_MAX", "float3", false, 0},

      {"M_PI", "float", true, 3.14159265358979323846f},
      {"M_TAU", "float", true, 6.28318530717958647692f},
      {"M_PI_2", "float", true, 1.57079632679489661923f},
      {"M_PI_4", "float", true, 0.78539816339744830962f},
      {"M_SQRT2", "float", true, 1.41421356237309504880f},
      {"M_SQRT1_2", "float", true, 0.70710678118654752440f},
      {"M_SQRT3", "float", true, 1.73205080756887729352f},
      {"M_SQRT1_3", "float", true, 0.57735026918962576450f},
      {"M_1_PI", "float", true, 0.318309886183790671538f},
      {"M_E", "float", true, 2.7182818284590452354f},
      {"M_LOG2E", "float", true, 1.4426950408889634074f},
      {"M_LOG10E", "float", true, 0.43429448190325182765f},
      {"M_LN2", "float", true, 0.69314718055994530942f},
      {"M_LN10", "float", true, 2.30258509299404568402f},
  };

  for (auto [value, name] : image_formats) {
    consts.emplace_back(name, "int", true, value);
  }

  /* Boolean constants. */
  for (auto c : consts) {
    SymbolVariable *sym = var_arena.alloc(root, root->lookup_class(c.type), tok, string(c.id));
    sym->is_error = c.id == err_symbol;
    sym->is_static = c.is_constexpr;
    sym->is_constexpr = c.is_constexpr;
    sym->value = c.value;
    root->variables.emplace(sym->identifier, sym);
  }

  this->err_var = root->lookup_variable(err_symbol);
  this->err_func = root->lookup_function(err_symbol);
}

SymbolClass *SymbolTable::get_literal_type(string_view lit) const
{
  if (lit == "true" || lit == "false") {
    return bool_cls;
  }

  /* Convert to lowercase helper for easier suffix matching */
  string str;
  str.reserve(lit.size());
  for (char c : lit) {
    str.push_back(tolower(static_cast<unsigned char>(c)));
  }

  bool is_hex = str.starts_with("0x");

  bool has_decimal = (str.find('.') != string::npos);
  bool has_exponent = !is_hex && (str.find('e') != string::npos);
  bool has_float_suffix = !is_hex && (str.back() == 'f');

  if (has_decimal || has_exponent || has_float_suffix) {
    return float_cls;
  }
  if (str.back() == 'u') {
    return uint_cls;
  }
  return int_cls;
}

MatchRank SymbolTable::get_conversion_rank(const SymbolClass *from,
                                           const SymbolClass *to,
                                           const bool ctor_conversion) const
{
  if (from == to) {
    return MatchRank::Exact;
  }

  if (!from->is_builtin()) {
    return MatchRank::None;
  }

  /* Casting enum or packed types. */
  if (from->builtin_class == to->builtin_class) {
    return MatchRank::Conversion;
  }

  using namespace builtin;

  const builtin::ClassId from_cls = from->builtin_class;
  const builtin::ClassId to_cls = to->builtin_class;

  const uint16_t from_raw = uint16_t(from_cls);
  const uint16_t to_raw = uint16_t(to_cls);

  /* Masking BASE_MASK strips component length, leaving only base type flags and rank. */
  const uint16_t from_len = from_raw & LEN_MASK;
  const uint16_t to_len = to_raw & LEN_MASK;
  /* Check standard GLSL compatible convertible base types (int <-> uint <-> float). */
  const bool both_numeric = (from_raw & FLAG_NUMERIC) && (to_raw & FLAG_NUMERIC);
  const bool is_promotion = (from_raw & FLAG_INTEGER) == (to_raw & FLAG_INTEGER);

  if (from_len == to_len) {
    if (from_len == 1) {
      if (both_numeric || ctor_conversion) {
        /* Scalar conversion. */
        return is_promotion ? MatchRank::Promotion : MatchRank::Conversion;
      }
    }
    if (ctor_conversion) {
      /* Vector conversion. */
      return MatchRank::Conversion;
    }
  }

  return MatchRank::None;
}

/* Generates every single legal BSL builtin function dynamically. */
vector<SymbolTable::BuiltinFunc> SymbolTable::generate_all_builtin_functions()
{
  /* Start with the error symbol. */
  vector<BuiltinFunc> functions = {{err_cls, err_symbol, {}}};

  /* Special functions */
  functions.push_back({void_cls, "printf", {str_cls}});
  functions.push_back({void_cls, "static_assert", {bool_cls}});

  for (SymbolClass *type : {float_cls,
                            float2_cls,
                            float3_cls,
                            float4_cls,
                            int_cls,
                            int2_cls,
                            int3_cls,
                            int4_cls,
                            uint_cls,
                            uint2_cls,
                            uint3_cls,
                            uint4_cls})
  {
    functions.push_back({bool_cls, "in_range_inclusive", {type, type, type}});
    functions.push_back({bool_cls, "in_range_exclusive", {type, type, type}});
  }

  for (SymbolClass *sampler_t : {root->lookup_class("sampler2DDepth"),
                                 root->lookup_class("sampler2D"),
                                 root->lookup_class("isampler2D"),
                                 root->lookup_class("usampler2D")})
  {
    functions.push_back({bool_cls, "in_texture_range", {int2_cls, sampler_t}});
  }

  for (SymbolClass *sampler_t : {root->lookup_class("sampler2DArray"),
                                 root->lookup_class("isampler2DArray"),
                                 root->lookup_class("usampler2DArray"),
                                 root->lookup_class("sampler3D"),
                                 root->lookup_class("isampler3D"),
                                 root->lookup_class("usampler3D"),
                                 root->lookup_class("isampler3DAtomic"),
                                 root->lookup_class("usampler3DAtomic"),
                                 root->lookup_class("isampler2DArrayAtomic"),
                                 root->lookup_class("usampler2DArrayAtomic")})
  {
    functions.push_back({bool_cls, "in_texture_range", {int2_cls, sampler_t}});
  }

  for (SymbolClass *image_t : {root->lookup_class("iimage1DArray"),
                               root->lookup_class("uimage1DArray"),
                               root->lookup_class("image2D"),
                               root->lookup_class("iimage2D"),
                               root->lookup_class("uimage2D"),
                               root->lookup_class("iimage2DAtomic"),
                               root->lookup_class("uimage2DAtomic")})
  {
    functions.push_back({bool_cls, "in_image_range", {int2_cls, image_t}});
  }

  for (SymbolClass *image_t : {root->lookup_class("image2DArray"),
                               root->lookup_class("iimage2DArray"),
                               root->lookup_class("uimage2DArray"),
                               root->lookup_class("image3D"),
                               root->lookup_class("iimage3D"),
                               root->lookup_class("uimage3D"),
                               root->lookup_class("iimage3DAtomic"),
                               root->lookup_class("uimage3DAtomic"),
                               root->lookup_class("iimage2DArrayAtomic"),
                               root->lookup_class("uimage2DArrayAtomic")})
  {
    functions.push_back({bool_cls, "in_image_range", {int2_cls, image_t}});
  }

  /* There are many other overloads but we don't use them currently. */
  functions.push_back({uint_cls, "simd_broadcast_first", {uint_cls}});
  functions.push_back({uint_cls, "simd_min", {uint_cls}});
  functions.push_back({uint_cls, "simd_max", {uint_cls}});
  functions.push_back({uint_cls, "simd_or", {uint_cls}});

  /* Type definition vectors for vectorized generation */
  const vector<SymbolClass *> f_types = {float_cls, float2_cls, float3_cls, float4_cls};
  const vector<SymbolClass *> i_types = {int_cls, int2_cls, int3_cls, int4_cls};
  const vector<SymbolClass *> u_types = {uint_cls, uint2_cls, uint3_cls, uint4_cls};
  const vector<SymbolClass *> b_types = {bool_cls, bool2_cls, bool3_cls, bool4_cls};

  /* 1-argument component-wise functions (e.g. sin, cos, abs). */
  auto add_unary = [&](const string &name, const vector<SymbolClass *> &types) {
    for (const auto &t : types) {
      functions.push_back({t, name, {t}});
    }
  };

  /* 2-argument component-wise functions (e.g. pow, min, max). */
  auto add_binary = [&](const string &name, const vector<SymbolClass *> &types) {
    for (const auto &t : types) {
      functions.push_back({t, name, {t, t}});
    }
  };

  /* 3-argument component-wise functions (e.g. clamp, mix, fma). */
  auto add_ternary = [&](const string &name, const vector<SymbolClass *> &types) {
    for (const auto &t : types) {
      functions.push_back({t, name, {t, t, t}});
    }
  };

  /* Functions with vector-scalar variants (e.g. min(float3, float), clamp(float3, float, float)).
   */
  auto add_vec_scalar_binary = [&](const string &name, const vector<SymbolClass *> &types) {
    SymbolClass *scalar = types[0];
    for (size_t i = 1; i < types.size(); ++i) {
      functions.push_back({types[i], name, {types[i], scalar}});
    }
  };

  auto add_scalar_vec_binary = [&](const string &name, const vector<SymbolClass *> &types) {
    SymbolClass *scalar = types[0];
    for (size_t i = 1; i < types.size(); ++i) {
      functions.push_back({types[i], name, {scalar, types[i]}});
    }
  };

  auto add_vec_scalar_scalar_ternary = [&](const string &name,
                                           const vector<SymbolClass *> &types) {
    SymbolClass *scalar = types[0];
    for (size_t i = 1; i < types.size(); ++i) {
      functions.push_back({types[i], name, {types[i], scalar, scalar}});
    }
  };

  auto add_vec_vec_scalar_ternary = [&](const string &name, const vector<SymbolClass *> &types) {
    SymbolClass *scalar = types[0];
    for (size_t i = 1; i < types.size(); ++i) {
      functions.push_back({types[i], name, {types[i], types[i], scalar}});
    }
  };

  auto add_scalar_scalar_vec_ternary = [&](const string &name,
                                           const vector<SymbolClass *> &types) {
    SymbolClass *scalar = types[0];
    for (size_t i = 1; i < types.size(); ++i) {
      functions.push_back({types[i], name, {types[i], scalar, scalar}});
    }
  };

  const vector<string> trig_unary = {"radians",
                                     "degrees",
                                     "sin",
                                     "cos",
                                     "tan",
                                     "asin",
                                     "acos",
                                     "atan",
                                     "sinh",
                                     "cosh",
                                     "tanh",
                                     "asinh",
                                     "acosh",
                                     "atanh"};
  for (const auto &name : trig_unary) {
    add_unary(name, f_types);
  }
  /* 2-argument atan(y, x). */
  add_binary("atan", f_types);

  for (string fn : {"EXPECT_EQ", "EXPECT_NE", "EXPECT_LE", "EXPECT_LT", "EXPECT_GE", "EXPECT_GT"})
  {
    add_binary(fn, f_types);
    add_binary(fn, i_types);
    add_binary(fn, u_types);
    add_binary(fn, {char_cls, uchar_cls});
  }
  add_unary("EXPECT_TRUE", b_types);
  add_unary("EXPECT_FALSE", b_types);
  add_ternary("EXPECT_NEAR", f_types);
  add_vec_vec_scalar_ternary("EXPECT_NEAR", f_types);

  const vector<string> exp_unary = {"exp", "log", "exp2", "log2", "sqrt", "inversesqrt"};
  for (const auto &name : exp_unary) {
    add_unary(name, f_types);
  }
  add_binary("pow", f_types);

  const vector<string> common_float_unary = {
      "floor",
      "trunc",
      "round",
      "roundEven",
      "ceil",
      "fract",
      "saturate",
  };
  for (const auto &name : common_float_unary) {
    add_unary(name, f_types);
  }

  for (size_t i = 0; i < f_types.size(); ++i) {
    functions.push_back({b_types[i], "isnan", {f_types[i]}});
    functions.push_back({b_types[i], "isinf", {f_types[i]}});
  }

  add_unary("abs", f_types);
  add_unary("abs", i_types);

  add_unary("sign", f_types);
  add_unary("sign", i_types);

  add_binary("mod", f_types);
  add_binary("modf", f_types);
  add_vec_scalar_binary("mod", f_types);

  for (const auto &tlist : {f_types, i_types, u_types}) {
    add_binary("min", tlist);
    add_vec_scalar_binary("min", tlist);
    add_binary("max", tlist);
    add_vec_scalar_binary("max", tlist);
    add_ternary("clamp", tlist);
    add_vec_scalar_scalar_ternary("clamp", tlist);
  }

  add_ternary("mix", f_types);
  add_vec_vec_scalar_ternary("mix", f_types);
  for (size_t i = 0; i < f_types.size(); ++i) {
    functions.push_back({f_types[i], "mix", {f_types[i], f_types[i], b_types[i]}});
    functions.push_back({f_types[i], "select", {f_types[i], f_types[i], b_types[i]}});
  }

  add_binary("step", f_types);
  add_scalar_vec_binary("step", f_types);
  add_ternary("smoothstep", f_types);
  add_scalar_scalar_vec_ternary("smoothstep", f_types);

  add_ternary("fma", f_types);

  /* Bit-reinterpretation conversions. */
  for (size_t i = 0; i < f_types.size(); ++i) {
    functions.push_back({i_types[i], "floatBitsToInt", {f_types[i]}});
    functions.push_back({u_types[i], "floatBitsToUint", {f_types[i]}});
    functions.push_back({f_types[i], "intBitsToFloat", {i_types[i]}});
    functions.push_back({f_types[i], "uintBitsToFloat", {u_types[i]}});
  }

  /* Matrix conversion functions. */
  functions.push_back({float2x2_cls, "to_float2x2", {float3x3_cls}});
  functions.push_back({float2x2_cls, "to_float2x2", {float4x4_cls}});
  functions.push_back({float3x3_cls, "to_float3x3", {float4x4_cls}});
  functions.push_back({float3x3_cls, "to_float3x3", {float2x2_cls}});
  functions.push_back({float4x4_cls, "to_float4x4", {float2x2_cls}});
  functions.push_back({float4x4_cls, "to_float4x4", {float3x3_cls}});
  functions.push_back({float3x3_cls, "to_float3x3", {float3x4_cls}});

  functions.push_back({uint_cls, "packUnorm2x16", {float2_cls}});
  functions.push_back({uint_cls, "packSnorm2x16", {float2_cls}});
  functions.push_back({uint_cls, "packUnorm4x8", {float4_cls}});
  functions.push_back({uint_cls, "packSnorm4x8", {float4_cls}});
  functions.push_back({uint_cls, "packHalf2x16", {float2_cls}});

  functions.push_back({float2_cls, "unpackUnorm2x16", {uint_cls}});
  functions.push_back({float2_cls, "unpackSnorm2x16", {uint_cls}});
  functions.push_back({float4_cls, "unpackUnorm4x8", {uint_cls}});
  functions.push_back({float4_cls, "unpackSnorm4x8", {uint_cls}});
  functions.push_back({float2_cls, "unpackHalf2x16", {uint_cls}});

  for (size_t i = 0; i < f_types.size(); ++i) {
    functions.push_back({float_cls, "length", {f_types[i]}});
    functions.push_back({float_cls, "distance", {f_types[i], f_types[i]}});
    functions.push_back({float_cls, "dot", {f_types[i], f_types[i]}});
    functions.push_back({f_types[i], "normalize", {f_types[i]}});
    functions.push_back({f_types[i], "faceforward", {f_types[i], f_types[i], f_types[i]}});
    functions.push_back({f_types[i], "reflect", {f_types[i], f_types[i]}});
    functions.push_back({f_types[i], "refract", {f_types[i], f_types[i], float_cls}});
  }
  functions.push_back({float3_cls, "cross", {float3_cls, float3_cls}});

  const vector<SymbolClass *> square_mats = {float2x2_cls, float3x3_cls, float4x4_cls};
  const vector<SymbolClass *> all_mats = {
      float2x2_cls,
      float3x3_cls,
      float4x4_cls,
      float2x3_cls,
      float2x4_cls,
      float3x2_cls,
      float3x4_cls,
      float4x2_cls,
      float4x3_cls,
  };

  for (const auto &m : all_mats) {
    functions.push_back({m, "transpose", {m}});
  }

  for (const auto &m : square_mats) {
    functions.push_back({float_cls, "determinant", {m}});
    functions.push_back({m, "inverse", {m}});
  }

  const vector<string> rel_ops = {
      "lessThan", "lessThanEqual", "greaterThan", "greaterThanEqual", "equal", "notEqual"};

  for (const auto &op : rel_ops) {
    for (size_t i = 1; i < f_types.size(); ++i) {
      functions.push_back({b_types[i], op, {f_types[i], f_types[i]}});
      functions.push_back({b_types[i], op, {i_types[i], i_types[i]}});
      functions.push_back({b_types[i], op, {u_types[i], u_types[i]}});
    }
  }

  for (size_t i = 1; i < b_types.size(); ++i) {
    functions.push_back({bool_cls, "any", {b_types[i]}});
    functions.push_back({bool_cls, "all", {b_types[i]}});
    functions.push_back({b_types[i], "not", {b_types[i]}});
  }

  const vector<string> bit_unary = {"bitfieldReverse"};
  for (const auto &op : bit_unary) {
    add_unary(op, i_types);
    add_unary(op, u_types);
  }

  for (int i : {0, 1, 2, 3}) {
    SymbolClass *u_type = u_types[i];
    SymbolClass *i_type = i_types[i];

    for (SymbolClass *input : {u_type, i_type}) {
      functions.push_back({i_type, "bitCount", {input}});
      functions.push_back({i_type, "findLSB", {input}});
      functions.push_back({i_type, "findMSB", {input}});
    }
  }

  for (const auto &tlist : {i_types, u_types}) {
    for (const auto &t : tlist) {
      functions.push_back({t, "bitfieldExtract", {t, int_cls, int_cls}});
      functions.push_back({t, "bitfieldInsert", {t, t, int_cls, int_cls}});
    }
  }

  const vector<string> atomic_2arg = {"atomicAdd",
                                      "atomicMin",
                                      "atomicMax",
                                      "atomicAnd",
                                      "atomicOr",
                                      "atomicXor",
                                      "atomicExchange"};

  for (const auto &op : atomic_2arg) {
    functions.push_back({int_cls, op, {int_cls, int_cls}});
    functions.push_back({uint_cls, op, {uint_cls, uint_cls}});
  }
  functions.push_back({int_cls, "atomicCompSwap", {int_cls, int_cls, int_cls}});
  functions.push_back({uint_cls, "atomicCompSwap", {uint_cls, uint_cls, uint_cls}});

  const vector<string> deriv_funcs = {"gpu_dfdx", "gpu_dfdy", "gpu_fwidth"};

  for (const auto &op : deriv_funcs) {
    add_unary(op, f_types);
  }

  functions.push_back({void_cls, "gpu_discard_fragment", {}});
  functions.push_back({void_cls, "barrier", {}});
  functions.push_back({void_cls, "assert", {bool_cls}});

  const vector<pair<SymbolClass *, string>> image_types = {
      {uint4_cls, "u"},
      {int4_cls, "i"},
      {float4_cls, ""},
  };

  for (auto [type, prefix] : image_types) {
    vector<tuple<SymbolClass *, int, int>> samplers = {
        /* Name, Coordinate Dimensions. */
        {root->classes[prefix + "sampler1D"], 1, 1},
        {root->classes[prefix + "sampler2D"], 2, 2},
        {root->classes[prefix + "sampler3D"], 3, 3},
        {root->classes[prefix + "samplerCube"], 3, 3},
        {root->classes[prefix + "sampler1DArray"], 2, 1},
        {root->classes[prefix + "sampler2DArray"], 3, 2},
        {root->classes[prefix + "samplerCubeArray"], 4, 3},
    };

    if (prefix.empty()) {
      samplers.emplace_back(root->classes["sampler2DDepth"], 2, 2);
      samplers.emplace_back(root->classes["samplerCubeDepth"], 3, 3);
      samplers.emplace_back(root->classes["sampler2DArrayDepth"], 3, 2);
      samplers.emplace_back(root->classes["samplerCubeArrayDepth"], 4, 3);
    }
    else {
      samplers.emplace_back(root->classes[prefix + "sampler2DAtomic"], 2, 2);
      samplers.emplace_back(root->classes[prefix + "sampler3DAtomic"], 3, 3);
      samplers.emplace_back(root->classes[prefix + "sampler2DArrayAtomic"], 3, 2);
    }

    for (auto [sampler, coord_dim, deriv_dim] : samplers) {
      SymbolClass *coord_type = f_types[coord_dim - 1];
      SymbolClass *icoord_type = i_types[coord_dim - 1];
      SymbolClass *deriv_type = f_types[deriv_dim - 1];

      functions.push_back({type, "texture", {sampler, coord_type}});
      functions.push_back({type, "textureLod", {sampler, coord_type, float_cls}});
      functions.push_back({type, "textureGather", {sampler, coord_type}});
      functions.push_back({type, "textureGather", {sampler, coord_type, int_cls}});
      functions.push_back({type, "textureGrad", {sampler, coord_type, deriv_type, deriv_type}});
      functions.push_back({icoord_type, "textureSize", {sampler, int_cls}});
      functions.push_back({type, "texelFetch", {sampler, icoord_type, int_cls}});
    }

    vector<tuple<SymbolClass *, int>> images = {
        {root->classes[prefix + "image2D"], 2},
        {root->classes[prefix + "image2DArray"], 3},
        {root->classes[prefix + "image3D"], 3},
    };

    if (!prefix.empty()) {
      images.emplace_back(root->classes[prefix + "image2DAtomic"], 2);
      images.emplace_back(root->classes[prefix + "image3DAtomic"], 3);
      images.emplace_back(root->classes[prefix + "image2DArrayAtomic"], 3);
    }

    for (auto [image, coord_dim] : images) {
      SymbolClass *coord_type = i_types[coord_dim - 1];
      functions.push_back({type, "imageLoad", {image, coord_type}});
      functions.push_back({type, "imageLoadFast", {image, coord_type}});
      functions.push_back({void_cls, "imageStore", {image, coord_type, type}});
      functions.push_back({void_cls, "imageStoreFast", {image, coord_type, type}});
      functions.push_back({coord_type, "imageSize", {image}});
      functions.push_back({void_cls, "imageFence", {image}});

      if (image->identifier.ends_with("Atomic")) {
        SymbolClass *scalar = prefix == "u" ? i_types[0] : u_types[0];
        functions.push_back({scalar, "imageAtomicAdd", {image, coord_type, scalar}});
        functions.push_back({scalar, "imageAtomicMin", {image, coord_type, scalar}});
        functions.push_back({scalar, "imageAtomicMax", {image, coord_type, scalar}});
        functions.push_back({scalar, "imageAtomicAnd", {image, coord_type, scalar}});
        functions.push_back({scalar, "imageAtomicOr", {image, coord_type, scalar}});
        functions.push_back({scalar, "imageAtomicXor", {image, coord_type, scalar}});
        functions.push_back({scalar, "imageAtomicExchange", {image, coord_type, scalar}});
        functions.push_back({scalar, "imageAtomicCompSwap", {image, coord_type, scalar, scalar}});
      }
    }
  }

  return functions;
}

}  // namespace bsl
